/*
The MIT License

Copyright (c) 2011 Comfirm <http://www.comfirm.se/>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

#include "firm-dkim.h"
#include "base64.h"
	
stringpair **relaxed_header_canon(stringpair **headers, int headerc);
char *relaxed_body_canon(char *body);
char *relaxed_body_canon_line(char *line);	

char *rtrim_lines(char *str);
char *rtrim(char *str);
char *ltrim(char *str);
char *wrap(char *str, int len);
int rsa_read_private_pem(RSA **rsa, char *buff, int len);
int rsa_read_public_pem(RSA **rsa, char *buff, int len);
char *str_to_lower(char *str);

dkim_header *dkim_parse_header(char *sig);
int dkim_check_compability(dkim_header *dh);
int dkim_validate_body_hash(dkim_header *dh, char *body);


/* parse DKIM DNS record */
dkim_record *dkim_parse_dns(char *txt) {
	int i;
	
	/* find all parameters */
	dkim_record *dr = malloc(sizeof(dkim_record));
	int txt_len = strlen(txt);
	
	int in_val = 0;
	int val_len = 0;
	char *param_ptr = NULL;
	
	for (i = 0; i < txt_len; ++i) {
		if (in_val) {
			/* in value */
			if (txt[i] == ';') {
				if (param_ptr != NULL) {
					param_ptr[val_len] = '\0';
					param_ptr = NULL;
				}
			
				in_val = 0;
				val_len = 0;
			} else if (txt[i] == '\t' || txt[i] == ' ') {
				/* ignore whitespace */
			} else {
				if (param_ptr != NULL && val_len < DKIM_RECORD_MAX_LEN-1) {
					memcpy (param_ptr+val_len, txt+i, 1);
					++val_len;
				}
			}
		} else {
			if (strncmp(txt+i, "k=", 2) == 0) {
				param_ptr = &dr->k;
				in_val = 1;
				++i;
			} else if (strncmp(txt+i, "p=", 2) == 0) {
				param_ptr = &dr->p;
				in_val = 1;
				++i;
			}
		} 
	}
	
	if (param_ptr != NULL) {
		param_ptr[val_len] = '\0';
	}
	
	return dr;
}


/* get and parse the DKIM header from all headers */
dkim_header *dkim_get_header(stringpair **headers, int headerc) {
	int i = 0;
	
	/* find DKIM header */
	int dkim_index = -1;
	for (i = 0; i < headerc; ++i) {
		if (strcmp(headers[i]->key, "DKIM-Signature") == 0) {
			/* found it */
			dkim_index = i;
			break;
		}
	}
	
	if (dkim_index != -1) {
		/* parse the DKIM header */
		return dkim_parse_header(headers[dkim_index]->value);
	}
	
	return NULL;
}


/* validate DKIM header */
int dkim_validate(stringpair **headers, int headerc, char *body, dkim_header *dh, char *public_key) {
	int i = 0;
	int e = 0;
	int retval = 0;
	
	/* find DKIM header */
	int dkim_index = -1;
	for (i = 0; i < headerc; ++i) {
		if (strcmp(headers[i]->key, "DKIM-Signature") == 0) {
			/* found it */
			dkim_index = i;
			break;
		}
	}
	
	/* get all headers that is included in the signature */
	int h_len = strlen(dh->h);
	int offset = 0;

	stringpair **inc_headers = malloc(sizeof(stringpair*) * 50);
	int inc_headerc = 0;

	for (i = 0; i < h_len; ++i) {
		if (dh->h[i] == ':' || i == (h_len-1)) {
			/* new header declaration */
			int head_len = i - offset;
			if (i == h_len-1) head_len++;
		
			char *header = malloc(head_len + 1);
			memcpy (header, ((char*)&dh->h)+offset, head_len);
			header[head_len] = '\0';
		
			/* loop thru all mail headers to find it */
			for (e = 0; e < headerc; ++e) {
				char *low_hdr = str_to_lower(headers[e]->key);
				if (strncmp(low_hdr, header, head_len) == 0) {
					/* found the header */
					stringpair *new_header = NULL;
					inc_headers[inc_headerc++] = new_header = malloc(sizeof(stringpair));
				
					new_header->key = headers[e]->key;
					new_header->value = headers[e]->value;
				}
				free (low_hdr);
			}
			
			free (header);
			offset = i+1;
		}
	}

	/* check compability */
	if (dkim_check_compability(dh) == 0) {
		/* validate body hash */
		if (dkim_validate_body_hash(dh, body) == 0) {
			/* add the DKIM header to headers */
			inc_headers[inc_headerc] = malloc(sizeof(stringpair));
			inc_headers[inc_headerc]->key = "dkim-signature";
			inc_headers[inc_headerc]->value = headers[dkim_index]->value;
			inc_headerc++;
			
			/* relax canonicalization headers */
			stringpair **new_headers = relaxed_header_canon(inc_headers, inc_headerc);
			
			/* make a string of all headers */
			char *header_str = malloc(1500*inc_headerc);
			int header_str_len = 0;

			for (i = 0; i < inc_headerc; ++i) {
				int key_len = strlen(new_headers[i]->key);
				int value_len = strlen(new_headers[i]->value);

				memcpy (header_str+header_str_len, new_headers[i]->key, key_len);
				header_str_len += key_len;
	
				memcpy (header_str+header_str_len, ":", 1);
				header_str_len++;
	
				memcpy (header_str+header_str_len, new_headers[i]->value, value_len);
				header_str_len += value_len;
	
				if (i < inc_headerc-1) {
					memcpy (header_str+header_str_len, "\r\n", 3);
					header_str_len += 2;
				}
			}

			/* remove the 'old' signature */
			for (i = header_str_len - strlen(dh->b); i > 1; --i) {
				if (header_str[i] == '=' && header_str[i-1] == 'b') {
					header_str[i+1] = '\0';
					header_str_len = i+1;
					break;
				}
			}
			
			/* base64 decode the signature */
			unsigned char *signature = NULL;
			int signature_len = base64_decode(&dh->b, &signature, strlen(&dh->b));

			if (signature_len > 0) {
				/* load public key */
				RSA *rsa_public = NULL;
				rsa_read_public_pem (&rsa_public, public_key, strlen(public_key));

				if (rsa_public != NULL) {
					/* hash the data */
					unsigned char *hash = malloc(SHA256_DIGEST_LENGTH);
					SHA256 ((unsigned char*)header_str, header_str_len, hash);
			
					/* verify the signature */
					retval = RSA_verify(NID_sha256, hash, SHA256_DIGEST_LENGTH, signature, signature_len, rsa_public);
				
					/* free some more memory */
					for (i = 0; i < inc_headerc; ++i) {
						if (i < inc_headerc-1) free (new_headers[i]->key);
						if (i < inc_headerc-1) free (new_headers[i]->value);
						free (new_headers[i]);
					}
			
					free (hash);
					RSA_free (rsa_public);
				} else {
					printf (" * ERROR when loading public key\n");
				}
				
				free (signature);
			} else {
				printf (" * ERROR failed to base64 decode the signature\n");
			}
			
			free (new_headers);
			free (header_str);		
		} else {
			/* body hash mismatching */
			printf (" * ERROR body hash mismatching\n");
		}
	} else {
		/* missing DKIM header parameters */
		printf (" * ERROR missing critical DKIM header parameters\n");
	}

	/* free included headers */
	for (i = 0; i < inc_headerc; ++i) {
		free (inc_headers[i]);
	}
	free (inc_headers);
	
	return retval;
}


/* parse DKIM header */
dkim_header *dkim_parse_header(char *sig) {
	int i;
	
	/* find all parameters */
	dkim_header *dh = malloc(sizeof(dkim_header));
	int sig_len = strlen(sig);
	
	int in_val = 0;
	int val_len = 0;
	char *param_ptr = NULL;
	
	for (i = 0; i < sig_len; ++i) {
		if (in_val) {
			/* in value */
			if (sig[i] == ';') {
				if (param_ptr != NULL) {
					param_ptr[val_len] = '\0';
					param_ptr = NULL;
				}
			
				in_val = 0;
				val_len = 0;
			} else if (sig[i] == '\t' || sig[i] == ' ') {
				/* ignore whitespace */
			} else {
				if (param_ptr != NULL && val_len < DKIM_VALUE_MAX_LEN-1) {
					memcpy (param_ptr+val_len, sig+i, 1);
					++val_len;
				}
			}
		} else {
			if (strncmp(sig+i, "v=", 2) == 0) {
				param_ptr = &dh->v;
				in_val = 1;
				++i;
			} else if (strncmp(sig+i, "a=", 2) == 0) {
				param_ptr = &dh->a;
				in_val = 1;
				++i;
			} else if (strncmp(sig+i, "c=", 2) == 0) {
				param_ptr = &dh->c;
				in_val = 1;
				++i;
			} else if (strncmp(sig+i, "d=", 2) == 0) {
				param_ptr = &dh->d;
				in_val = 1;
				++i;
			} else if (strncmp(sig+i, "s=", 2) == 0) {
				param_ptr = &dh->s;
				in_val = 1;
				++i;
			} else if (strncmp(sig+i, "bh=", 3) == 0) {
				param_ptr = &dh->bh;
				in_val = 1;
				i += 2;
			} else if (strncmp(sig+i, "b=", 2) == 0) {
				param_ptr = &dh->b;
				in_val = 1;
				++i;
			} else if (strncmp(sig+i, "h=", 2) == 0) {
				param_ptr = &dh->h;
				in_val = 1;
				++i;
			}
		} 
	}
	
	if (param_ptr != NULL) {
		param_ptr[val_len] = '\0';
	}
	
	return dh;
}


/* check DKIM header compability */
int dkim_check_compability(dkim_header *dh) {
	int i = 0;

	/* check parameters */
	if (dh->a == NULL || dh->h == NULL || dh->v == NULL ||
	    dh->c == NULL || dh->b == NULL || dh->bh == NULL || 
	    dh->s == NULL) {
		/* missing critical dkim parameters */
		printf (" * ERROR missing DKIM parameters\n");
		return 1;
	}
	
	/* check algorithm */
	if (strncmp(dh->a, "rsa-sha256", 10) != 0) {
		/* the algorithm is not supported at this time */
		printf (" * ERROR algorithm not supported\n");
		return 1;
	}
	
	/* check canonicalization method */
	if (strncmp(dh->c, "relaxed/relaxed", 15) != 0) {
		/* the canonicalization method is not supported at this time */
		printf (" * ERROR canonicalization method must be 'relaxed'");
		return 1;
	}
	
	return 0;
}


/* validate body hash */
int dkim_validate_body_hash(dkim_header *dh, char *body) {	
	int retval = 1;
	
	/* relax canonicalization for body */
	char *new_body = relaxed_body_canon(body);
	int new_body_len = strlen(new_body);

	/* hash of the canonicalized body */
	unsigned char *uhash = malloc(SHA256_DIGEST_LENGTH);
	SHA256 ((unsigned char*)new_body, new_body_len, uhash);
	free (new_body);
	
	/* base64 encode the hash */
	char *bh = base64_encode(uhash, SHA256_DIGEST_LENGTH);
	free (uhash);
	
	/* compare the hashes */
	if (strncmp(bh, dh->bh, strlen(bh)) == 0) {
		retval = 0;
	}
	
	free (bh);
	return retval;
}


/* create a DKIM-signature header value
   http://tools.ietf.org/html/rfc4871 */
char *dkim_create(stringpair **headers, int headerc, char *body, char *pkey, char *domain, char *selector, int v) {
	int i = 0;
	RSA *rsa_private = NULL;

	/* check input params */
	if (body == NULL || pkey == NULL || domain == NULL || selector == NULL) {
		printf (" * ERROR input parameters cannot be NULL\n");
		return NULL;
	}

	/* load the private key */
	if (v) printf (" * Loading rsa key...\n");
	if (rsa_read_private_pem(&rsa_private, pkey, strlen(pkey)) == -1) {
		printf (" * ERROR loading rsa key\n");
		return NULL;
	}

	/* relax canonicalization for headers */
	if (v) printf (" * 'Relaxing' headers...\n");
	stringpair **new_headers = relaxed_header_canon(headers, headerc);
	
	/* relax canonicalization for body */
	if (v) printf (" * 'Relaxing' body...\n");
	char *new_body = relaxed_body_canon(body);
	int new_body_len = strlen(new_body);

	/* signature timestamp */
	time_t ts = time(NULL);

	/* hash of the canonicalized body */
	if (v) printf (" * Hashing 'relaxed' body...\n");
	unsigned char *uhash = malloc(SHA_DIGEST_LENGTH);
	SHA1 ((unsigned char*)new_body, new_body_len, uhash);
	
	/* base64 encode the hash */
	char *bh = base64_encode(uhash, SHA_DIGEST_LENGTH);
	free (uhash);
	
	/* create header list */
	if (v) printf (" * Creating header list...\n");
	char *header_list = malloc(headerc*200);
	int header_list_len = 0;
	for (i = 0; i < headerc; ++i) {
		if (header_list_len != 0) {
			header_list_len += sprintf(header_list + header_list_len, ":");
		}
	
		header_list_len += sprintf(header_list + header_list_len, "%s", new_headers[i]->key);
	}
	
	/* create DKIM header */
	if (v) printf (" * Creating a DKIM header...\n");
	char *dkim_string = "v=1; a=rsa-sha1; s=%s; d=%s; l=%d; t=%d; c=relaxed/relaxed; h=%s; bh=%s; b=";
	char *dkim = malloc(strlen(dkim_string)+strlen(selector)+strlen(domain)+16+16+strlen(header_list)+strlen(bh)+1);
	int dkim_len = sprintf(dkim, dkim_string,
		selector, domain, new_body_len, ts, header_list, bh
	);
	
	/* free some memory */
	if (v) printf (" * Freeing some memory...\n");
	free (new_body);
	free (header_list);
	free (bh);
	
	/* add dkim header to headers */
	if (v) printf (" * Adding the dkim header to headers...\n");
	new_headers[headerc] = malloc(sizeof(stringpair));
	new_headers[headerc]->key = "dkim-signature";
	new_headers[headerc]->value = dkim;
	headerc++;
	
	/* make a string of all headers */
	if (v) printf (" * Making a string of all headers...\n");
	char *header_str = malloc(1500*headerc);
	int header_str_len = 0;
	
	for (i = 0; i < headerc; ++i) {
		int key_len = strlen(new_headers[i]->key);
		int value_len = strlen(new_headers[i]->value);
	
		memcpy (header_str+header_str_len, new_headers[i]->key, key_len);
		header_str_len += key_len;
		
		memcpy (header_str+header_str_len, ":", 1);
		header_str_len++;
		
		memcpy (header_str+header_str_len, new_headers[i]->value, value_len);
		header_str_len += value_len;
		
		if (i < headerc-1) {
			memcpy (header_str+header_str_len, "\r\n", 3);
			header_str_len += 2;
		}
	}
	
	/* hash the headers */
	if (v) printf (" * Hashing headers...\n");
	unsigned char *hash = malloc(SHA_DIGEST_LENGTH);
	SHA1 ((unsigned char*)header_str, header_str_len, hash);
	
	/* sign canon headers with private key */
	if (v) printf (" * Signing the header hash with the rsa key...\n");
	unsigned char *sig = malloc(RSA_size(rsa_private));
	unsigned int sig_len;
	
	if (RSA_sign(NID_sha1, hash, SHA_DIGEST_LENGTH, sig, &sig_len, rsa_private) == 0) {
		printf (" * ERROR RSA_sign(): %s\n", ERR_error_string(ERR_get_error(), NULL));
		free (hash);
		free (new_headers);
		free (header_str);
		free (sig);
		RSA_free (rsa_private);
		return NULL;
	}
	
	free (hash);

	/* base64 encode the signature */
	if (v) printf (" * Base64 encoding the signature...\n");
	char *sig_b64 = base64_encode(sig, sig_len);
	int sig_b64_len = strlen(sig_b64);
	
	/* add it to the dkim header */
	if (v) printf (" * Adding the signature to the dkim header...\n");
	dkim = realloc(dkim, dkim_len + sig_b64_len + 1);
	memcpy (dkim + dkim_len, sig_b64, sig_b64_len + 1);
	dkim_len += sig_b64_len;
	
	/* wrap dkim header */
	if (v) printf (" * Wrapping the header...\n");
	dkim = wrap(dkim, dkim_len);
	
	/* free some more memory */
	if (v) printf (" * Freeing some more memory...\n");
	for (i = 0; i < headerc; ++i) {
		if (i < headerc-1) free (new_headers[i]->key);
		if (i < headerc-1) free (new_headers[i]->value);
		free (new_headers[i]);
	}
	free (new_headers);
	free (header_str);
	free (sig);
	free (sig_b64);
	
	/* free the private key */
	if (v) printf (" * Freeing the rsa key...\n");
	RSA_free (rsa_private);
	
	/* return the dkim header value */
	if (v) printf (" * Done, returning...\n");
	return dkim;
}


/* The "relaxed" Header Canonicalization Algorithm */
stringpair **relaxed_header_canon(stringpair **headers, int headerc) {
	int i = 0;
	int e = 0;
	
	/* Copy all headers */
	stringpair **new_headers = (stringpair**)malloc(sizeof(stringpair*) * (headerc + 1));   
	   
	for (i = 0; i < headerc; ++i) {
		int key_len = strlen(headers[i]->key);
		int val_len = strlen(headers[i]->value);
		
		new_headers[i] = (stringpair*)malloc(sizeof(stringpair));
		new_headers[i]->key = malloc(key_len+1);
		new_headers[i]->value = malloc(val_len+1);
		
		memcpy (new_headers[i]->key, headers[i]->key, key_len+1);
		memcpy (new_headers[i]->value, headers[i]->value, val_len+1);
	}

	/* Convert all header field names (not the header field values) to
	   lowercase.  For example, convert "SUBJect: AbC" to "subject: AbC". */
	for (i = 0; i < headerc; ++i) {
		int key_len = strlen(new_headers[i]->key);

		for (e = 0; e < key_len; ++e) {
			new_headers[i]->key[e] = tolower(new_headers[i]->key[e]);
		}
	}
	
	/* Unfold all header field continuation lines as described in
	   [RFC2822]; in particular, lines with terminators embedded in
	   continued header field values (that is, CRLF sequences followed by
	   WSP) MUST be interpreted without the CRLF.  Implementations MUST
	   NOT remove the CRLF at the end of the header field value. */
	for (i = 0; i < headerc; ++i) {
		int val_len = strlen(new_headers[i]->value);
		int new_len = 0;

		for (e = 0; e < val_len; ++e) {
			if (e < val_len + 1) {
				if (!(new_headers[i]->value[e] == '\r' && new_headers[i]->value[e+1] == '\n')) {
					new_headers[i]->value[new_len++] = new_headers[i]->value[e];
				} else {
					++e;
				}
			} else {
				new_headers[i]->value[new_len++] = new_headers[i]->value[e];
			}
		}
		
		new_headers[i]->value[new_len] = '\0';
	}
	
	/* Convert all sequences of one or more WSP characters to a single SP
	   character.  WSP characters here include those before and after a
	   line folding boundary. */
	for (i = 0; i < headerc; ++i) {
		int val_len = strlen(new_headers[i]->value);
		int new_len = 0;

		for (e = 0; e < val_len; ++e) {
			if (new_headers[i]->value[e] == '\t') {
				new_headers[i]->value[e] = ' ';
			}
		
			if (e > 0) {
				if (!(new_headers[i]->value[e-1] == ' ' && new_headers[i]->value[e] == ' ')) {
					new_headers[i]->value[new_len++] = new_headers[i]->value[e];
				} 
			} else {
				new_headers[i]->value[new_len++] = new_headers[i]->value[e];
			}
		}
		
		new_headers[i]->value[new_len] = '\0';
	}
	
	/* Delete all WSP characters at the end of each unfolded header field
           value. */   
        /* Delete any WSP characters remaining before and after the colon
	   separating the header field name from the header field value.  The
	   colon separator MUST be retained. */
	for (i = 0; i < headerc; ++i) {
		new_headers[i]->value = rtrim(new_headers[i]->value);
		new_headers[i]->value = ltrim(new_headers[i]->value);
	}
	
	return new_headers;
}


/* The "relaxed" Body Canonicalization Algorithm */
char *relaxed_body_canon(char *body) {
	int i = 0;
	int offset = 0;
	int body_len = strlen(body);
	
	char *new_body = malloc(body_len*2+3);
	int new_body_len = 0;

	for (i = 0; i < body_len; ++i) {
		int is_r = 0;
	
		if (body[i] == '\n') {
			if (i > 0) {
				if (body[i-1] == '\r') {
					i--;
					is_r = 1;
				}
			}
			
			char *line = malloc(i - offset + 1);	
			memcpy (line, body+offset, i-offset);
			line[i-offset] = '\0';
		
			relaxed_body_canon_line (line);

			int line_len = strlen(line);
			memcpy (new_body+new_body_len, line, line_len);
			memcpy (new_body+new_body_len+line_len, "\r\n", 2);
			new_body_len += line_len+2;
			
			if (is_r) {
				i++;
			}	
			
			offset = i+1;
			free (line);
		}
	}

	if (offset < body_len) {
		char *line = malloc(i - offset + 1);	
		memcpy (line, body+offset, i-offset);
		line[i-offset] = '\0';
	
		relaxed_body_canon_line (line);
				
		int line_len = strlen(line);
		memcpy (new_body+new_body_len, line, line_len);
		memcpy (new_body+new_body_len+line_len, "\r\n", 2);
		new_body_len += line_len+2;
		
		free (line);
	}

	memcpy (new_body+new_body_len, "\0", 1);
	
	/* Ignores all empty lines at the end of the message body.  "Empty
      	   line" is defined in Section 3.4.3. */
	new_body = rtrim_lines(new_body);
	
	/* Note that a completely empty or missing body is canonicalized as a
           single "CRLF"; that is, the canonicalized length will be 2 octets. */
	new_body_len = strlen(new_body);
	new_body[new_body_len++] = '\r';
	new_body[new_body_len++] = '\n';
	new_body[new_body_len] = '\0';
	
	return new_body;	
}


/* The "relaxed" Body Canonicalization Algorithm, per line */
char *relaxed_body_canon_line(char *line) {
	int line_len = 0;
	int i;
	
	/* Ignores all whitespace at the end of lines.  Implementations MUST
      	   NOT remove the CRLF at the end of the line. */
	rtrim (line);
	
	/* Reduces all sequences of WSP within a line to a single SP
	   character. */
	line_len = strlen(line);
	int new_len = 0;

	for (i = 0; i < line_len; ++i) {
		if (line[i] == '\t') {
			line[i] = ' ';
		}
	
		if (i > 0) {
			if (!(line[i-1] == ' ' && line[i] == ' ')) {
				line[new_len++] = line[i];
			} 
		} else {
			line[new_len++] = line[i];
		}
	}
	
	line[new_len] = '\0';
	
	return line;
}


/* rtrim function for lines */
char *rtrim_lines(char *str) {
	char *end;
	int len = strlen(str);

	while (*str && len) {
		end = str + len-1;
		
		if (*end == '\r' || *end == '\n') {
			*end = '\0';
		} else {
			break;
		}
		
		len = strlen(str);
	}
	
	return str;
}


/* rtrim function */
char *rtrim(char *str) {
	char *end;
	int len = strlen(str);

	while (*str && len) {
		end = str + len-1;
		
		if (*end == ' ' || *end == '\t') {
			*end = '\0';
		} else {
			break;
		}
		
		len = strlen(str);
	}
	
	return str;
}


/* ltrim function */
char *ltrim(char *str) {
	char *start = str;
	int len = strlen(str);

	while (*start && len) {		
		if (*start == ' ' || *start == '\t') {
			start++;
		} else {
			break;
		}
		
		len = strlen(start);
	}
	
	char *n = malloc(strlen(start)+1);
	memcpy (n, start, strlen(start)+1);
	free (str);
	
	return n;
}


/* very simple word wrap function for headers */
char *wrap(char *str, int len) {
	char *tmp = malloc(len*3+1);
	int tmp_len = 0;
	int i;
	int lcount = 0;
	
	for (i = 0; i < len; ++i) {
		if (str[i] == ' ' || lcount == 75) {
			tmp[tmp_len++] = str[i];
			tmp[tmp_len++] = '\r';
			tmp[tmp_len++] = '\n';
			tmp[tmp_len++] = '\t';
			lcount = 0;
		} else {
			tmp[tmp_len++] = str[i];
			++lcount;
		}
	}
	
	tmp[tmp_len] = '\0';
	free (str);
	
	return tmp;
}


/* load an RSA key in PEM format from buff */
int rsa_read_private_pem(RSA **rsa, char *buff, int len) { 
	BIO *mem; 

	if ((mem = BIO_new_mem_buf(buff, len)) == NULL) { 
		goto err; 
	} 

	*rsa = PEM_read_bio_RSAPrivateKey(mem, NULL, NULL, NULL);
	BIO_free (mem); 

	if (*rsa == NULL) { 
		goto err; 
	} 

	return 0; 
err: 
	return -1; 
} 


/* load an RSA key in PEM format from buff */
int rsa_read_public_pem(RSA **rsa, char *buff, int len) { 
	BIO *mem; 

	if ((mem = BIO_new_mem_buf(buff, len)) == NULL) { 
		goto err; 
	} 

	PEM_read_bio_RSA_PUBKEY (mem, rsa, NULL, NULL);
	BIO_free (mem);

	if (*rsa == NULL) { 
		goto err; 
	}

	return 0;
err: 
	return -1; 
} 


/* convert a char string to lower case */
char *str_to_lower(char *str) {
	int len = strlen(str);
	char *tmp = malloc(len+1);
	int i = 0;
	
	for (; i < len; ++i) {
		tmp[i] = tolower(str[i]);
	}
	
	tmp[i] = '\0';
	return tmp;
}

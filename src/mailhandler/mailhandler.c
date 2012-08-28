#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include <libxml/parser.h>
#include <libxml/tree.h>

#include <firm-dkim.h>
#include "zlib.h"

#include "../lib-pq/persisted_queue.h"
#include "dns.h"
#include "base64.h"


/* validate the DKIM signature in message */
int validate_dkim(mail *message) {
	int retval = 0;
	int i;

	/* convert headers */
	stringpair **headers = malloc(sizeof(stringpair*) * message->header_count);
	for (i = 0; i < message->header_count; ++i) {
		headers[i] = malloc(sizeof(stringpair));
		headers[i]->key = &message->headers[i].key;
		headers[i]->value = &message->headers[i].value;
	}
	
	/* get and parse DKIM header */
	dkim_header *dh = dkim_get_header(headers, message->header_count);
	
	if (dh != NULL) {
		/* get public key from DNS */
		int ttl = 0;
		char *domain = malloc(strlen(dh->s) + strlen(dh->d) + 16);
		sprintf(domain, "%s._domainkey.%s", dh->s, dh->d);
		char *resp = dns_gettxt(domain, &ttl);
		
		if (resp != NULL) {
			/* parse DKIM dns record */	
			dkim_record *dr = dkim_parse_dns(resp);
		
			if (strncmp(dr->k, "rsa", 3) == 0) {
				/* fix public key */
				int key_len = strlen(dr->p);
				char *wrapped_key = base64_wrap(dr->p, key_len);
				key_len = strlen(wrapped_key);
			
				char *public_key = malloc(key_len + 100);
				key_len = sprintf(public_key, "-----BEGIN PUBLIC KEY-----\n%s\n-----END PUBLIC KEY-----\n", wrapped_key);
	
				/* validate the DKIM signature */
				retval = dkim_validate(headers, message->header_count, message->body, dh, public_key);
				
				/* free keys */
				free (public_key);
				free (wrapped_key);
			} else {
				printf (" * ERROR key type is not supported (must be rsa)\n");
			}
		
			free (dr);
			free (resp);
		} else {
			printf (" * ERROR could not get DKIM record from DNS ('%s')\n", domain);
		}
		
		free (domain);
		free (dh);
	} else {
		/* missing DKIM header */
		printf (" * ERROR mail is missing a DKIM header\n");
	}
	
	/* free mem */
	for (i = 0; i < message->header_count; ++i) {
		free (headers[i]);
	}

	free (headers);
	
	return retval;
}

/* decode mail content */
unsigned char *decode_content(mail *message, int *len) {
	int i;
	unsigned char *result = NULL;
	*len = 0;

	/* check content encoding */
	char *content_encoding = NULL;
	for (i = 0; i < message->header_count; ++i) {
		if (strcmp(message->headers[i].key, "Content-Transfer-Encoding") == 0) {
			content_encoding = &message->headers[i].value;
			break;
		}
	}
	
	/* decode */
	if (strcmp(content_encoding, "base64") == 0) {
		/* base64 */
		*len = base64_decode(&message->body, &result, strlen(message->body));
	}
	
	return result;
}

/* unzip file */
char *unzip(unsigned char *zipdata, int zipdata_len) {
	typedef struct {
		unsigned char file_signature[4];
		unsigned char version[2];
		unsigned char general_purpose_flag[2];
		unsigned char compression_method[2];
		unsigned char last_modified_time[2];
		unsigned char last_modified_date[2];
		unsigned char crc[4];
		unsigned char compressed_size[4];
		unsigned char uncompressed_size[4];
		unsigned char filename_length[2];
		unsigned char extra_field_length[2];
		
		char *filename;
	} zipfile;
	
	/* copy the data to the zip struct */
	zipfile *zip = malloc(sizeof(zipfile));
	memcpy (zip, zipdata, 30);
	
	/* check compabiity of the zip file */
	unsigned char fsig[4] = {0x50, 0x4b, 0x03, 0x04};
	if (memcmp(zip->file_signature, fsig, 4) != 0) {
		printf (" * ERROR unsupported zip format (must be 0x504b0304)\n");
		free (zip);
		return NULL;
	}
	
	/* check compression method */
	unsigned char cmeth[2] = {0x08, 0x00};
	if (memcmp(zip->compression_method, cmeth, 2) != 0) {
		printf (" * ERROR unsupported compression method (must be 0x08)\n");
		free (zip);
		return NULL;
	}
	
	/* get filename */
	int filename_len = 0;
	memcpy (&filename_len, &zip->filename_length, 2);
	zip->filename = malloc(filename_len + 1);
	memcpy (zip->filename, zipdata+30, filename_len);
	zip->filename[filename_len] = '\0';
	
	/* get extra field length */
	int extra_field_len = 0;
	memcpy (&extra_field_len, &zip->extra_field_length, 2);
	
	/* get data sizes */
	int compr_len = 0;
	int uncompr_len = 0;
	memcpy (&compr_len, &zip->compressed_size, 4);
	memcpy (&uncompr_len, &zip->uncompressed_size, 4);
	
	/* create zlib stream with header bytes */
	int zlib_stream_len = compr_len+2;
	unsigned char zlib_stream[zlib_stream_len];
	zlib_stream[0] = (unsigned char)(0x07 << 4) | 0x08; 	/* CMF */
	zlib_stream[1] = (unsigned char)0x9C;	  		/* FLG */
	
	/* copy compressed data */
	memcpy (((unsigned char*)&zlib_stream)+2, zipdata + 30 + filename_len + extra_field_len, compr_len);
	
	unsigned char uncompr[uncompr_len];
	unsigned char *result = NULL;
	z_stream strm; /* decompression stream */

	strm.zalloc = Z_NULL;
	strm.zfree = Z_NULL;
	strm.opaque = Z_NULL;

	if (inflateInit(&strm) != Z_STREAM_ERROR) {
		strm.next_in  = &zlib_stream;
		strm.avail_in = zlib_stream_len;
		strm.next_out = uncompr;
		strm.avail_out = uncompr_len;
	
		inflate (&strm, Z_FINISH);
		//CHECK_ERR(err, d_stream.msg);
	
		unsigned int have = uncompr_len - strm.avail_out;
		result = malloc(have+1);
		memcpy (result, uncompr, have);
	
		inflateEnd (&strm);
	}
	
	/* free some memory */
	free (zip->filename);
	free (zip);
	
	return (char*)result;
}

/* escape json string */
char *json_escape(char *str) {
	int len = strlen(str);
	int i, c = 0;
	char *new = malloc((len * 2) + 1);

	for (i = 0; i < len; ++i) {	
		if (str[i] == '\t') {
			new[c++] = '\\';
			new[c++] = 't';
		} else if (str[i] == '\n') {
			new[c++] = '\\';
			new[c++] = 'n';
		} else if (str[i] == '\\') {
			new[c++] = '\\';
			new[c++] = '\\';
		} else if (str[i] == '\r') {
			new[c++] = '\\';
			new[c++] = 'r';
		} else {
			new[c++] = str[i];
		}
	}
	
	new[c] = '\0';
	return new;
}

/* print node as json */
int print_as_json(char *name, xmlNode *a_node) {
	xmlNode *cur_node = a_node;

	if (name != NULL) {
		printf ("\"%s\": {\n", name);
	} else {
		printf ("{\n");
	}

	for (; cur_node; cur_node = cur_node->next) {
		if (cur_node->type == XML_ELEMENT_NODE) {
			
			if (cur_node->children != NULL) {
				if (cur_node->children->next == NULL) {
					char *content = (char*)xmlNodeGetContent(cur_node->children);
					char *value = json_escape(content);
					printf ("\t\"%s\": \"%s\",\n", cur_node->name, value);
					free (value);
				} else {
					printf ("\t");
					print_as_json (cur_node->name, cur_node->children);
				}
			} else {
				printf ("\t\"%s\": \"\",\n", cur_node->name);
			}
		}
	}
	
	printf (" }\n");
	
	return 0;
}

/* print all records */
int print_records(xmlNode *a_node) {
	xmlNode *cur_node = a_node;
	
	printf ("\"records\": [\n");
	
	for (; cur_node; cur_node = cur_node->next) {
		if (cur_node->type == XML_ELEMENT_NODE) {
			if (strcmp(cur_node->name, "record") == 0) {
				print_as_json (NULL, cur_node->children);
			}
		}
	}
	
	printf ("]\n");
	
	return 0;
}

/* parse the xml document */
int parse_xml(char *document) {
	LIBXML_TEST_VERSION
	
	xmlDocPtr doc = xmlReadMemory(document, strlen(document), "noname.xml", NULL, 0);
	
	if (doc == NULL) {
		fprintf (stderr, "Failed to parse document\n");
		return 1;
	}
	
	/* get the root element node */
	xmlNode *root_element = xmlDocGetRootElement(doc);

	/* loop thru all top elements */
	xmlNode *cur_node = root_element->children;
	int record_count = 0;

	for (; cur_node; cur_node = cur_node->next) {
		if (cur_node->type == XML_ELEMENT_NODE) {
			if (strcmp(cur_node->name, "report_metadata") == 0) {
				print_as_json ("metadata", cur_node->children);
			} else if (strcmp(cur_node->name, "policy_published") == 0) {
				print_as_json ("policy", cur_node->children);
			}
		}
	}
	
	/* print all records */
	print_records (root_element->children);
	
	/* free memory */
	xmlFreeDoc (doc);
	xmlCleanupParser();
	
	return 0;
}

int main(int argc, const char* argv[]) {
	const char *filepath = argv[1];
	int message_count = atoi(argv[2]);
	int i = 0;

	printf (" * Starting dequeing service... (queue-file='%s', message-count=%d)\n", filepath, message_count);

	pq_ref *queue;
	mail *message = NULL;

	/* open the queue file */
	if (pq_init(&queue, (char*)filepath, message_count) < 0) {
		printf (" * ERROR could not initiate the queue.\n");
		return 1;
	}

	printf (" * The service is up and running...\n");

	/* dequeue messages */
	while (1) {
		message = malloc(sizeof(mail));

		if (pq_dequeue(queue, message) == PQ_MESSAGE_QUEUE_EMPTY_ERROR) {
			/* unable to dequeue message, none in queue */
			pq_wait (queue);
		} else {
			printf (" * Dequeuing new message...\n");
			
			/* validate DKIM */
			if (!validate_dkim(message)) {
				/* incorrect signature! */
				printf (" * ERROR DKIM signature is incorrect!\n");
			} else {
				/* correct signature */
				printf (" * DKIM signature is correct\n");
			
				/* decode message body */
				int zipdata_len = 0;
				unsigned char *zipdata = decode_content(message, &zipdata_len);
			
				if (zipdata == NULL) {
					/* content encoding is not supported */
					printf (" * ERROR content encoding is not supported (must be base64)\n");
				} else {
					/* check zipdata length and filename */
					if (zipdata_len < 35) {
						/* zipfile is too small */
						printf (" * ERROR zipfile is too small\n");
					}
			
					/* unzip content */
					char *xml = unzip(zipdata, zipdata_len);
			
					if (xml != NULL) {
						/* parse the xml document */
						parse_xml (xml);
					}
			
					free (xml);
					free (zipdata);
				}
			}
		}
	
		free (message);
	}

	/* close the queue, should never get here */
	if (pq_close(queue) < 0) {
		printf (" * ERROR could not close the queue.\n");
		return 1;
	}

	return 0;
}


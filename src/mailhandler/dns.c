#include "dns.h"


char *dns_gettxt(const char *name, int *ttl) {
	unsigned char response[PACKETSZ];
	ns_msg handle;
	ns_rr rr;
	int i, len;
	char dispbuf[4096];

	if ((len = res_search(name, C_IN, T_TXT, response, sizeof(response))) < 0) {
		printf (" * ERROR res_search failed\n");
		return NULL;
	}

	if (ns_initparse(response, len, &handle) < 0) {
		printf (" * ERROR ns_initparse failed\n");
		return NULL;
	}

	if ((len = ns_msg_count(handle, ns_s_an)) < 0) {
		printf (" * ERROR ns_msg_count failed\n");
		return NULL;
	}

	if (ns_parserr(&handle, ns_s_an, 0, &rr)) {
		printf (" * ERROR ns_parserr failed\n");
		return NULL;
	}

	ns_sprintrr (&handle, &rr, NULL, NULL, dispbuf, sizeof (dispbuf));

	if (ns_rr_class(rr) == ns_c_in && ns_rr_type(rr) == ns_t_txt) {
		unsigned char *rdata = ns_rr_rdata(rr);
		*ttl = ns_rr_ttl(rr);
		
		int data_len = ns_rr_rdlen(rr);
		char *new = malloc(data_len+1);
		int new_len = 0;
		
		for (i = 0; i < data_len; ++i) {
			if (rdata[i] != 183 && rdata[i] != 218) {
				new[new_len++] = rdata[i];
			}
		}
		
		new[new_len] = '\0';
		return new;
	}

	return NULL;
}


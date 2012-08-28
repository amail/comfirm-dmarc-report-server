#include "response.h"

response *response_init() {
	/* init a new response struct */
	response *resp = malloc(sizeof(response));
	
	resp->data_alloc = 0;
	resp->data_len = 0;
	resp->data = NULL;
	
	return resp;
}

int response_append(response *resp, char *data) {
	int data_len = strlen(data);

	if (resp->data_len + data_len+1 >= resp->data_alloc) {
		/* realloc data var */
		resp->data_alloc = resp->data_len + data_len + 512;
		char *tmp = malloc(resp->data_alloc);
		memcpy (tmp, resp->data, resp->data_len);
		
		free (resp->data);
		resp->data = tmp;
	}
	
	memcpy (resp->data+resp->data_len, data, data_len+1);
	resp->data_len += data_len;
	
	return 0;
}

void *get_param_from_storage(storage *st, char *id, char type) {	
	if (storage_exists(st, id, strlen(id)) == 0) {
		/* this var exists */
		snode *val = storage_get_node(st, id, strlen(id));
		
		if (val->type == type) {
			/* found it */
			return val->value;
		} else {
			/* wrong value type */
			return NULL;
		}
	}
	
	/* not found */
	return NULL;
}

char *response_build_uuid(int64_t lo, int64_t hi) {
	/* build an uuid string from two longs */
	unsigned char *uuid = (unsigned char*) malloc(16); /* as bytes */				
	char *suuid = (char*) malloc(37);
	int suuid_len = 0;
	int i;

	memcpy (uuid, &lo, 8);
	memcpy (uuid+8, &hi, 8);

	for (i = 15; i >= 0; --i) {
		suuid_len += sprintf(suuid+suuid_len, "%2.2x", uuid[i]);

		if ((16-i) == 4 || (16-i) == 6 || (16-i) == 8 || (16-i) == 10) {
			suuid[suuid_len++] = '-';
		}
	}

	free (uuid);
	suuid[suuid_len] = '\0';
	
	return suuid;
}

int response_free(response *resp) {
	if (resp->data != NULL) {
		free (resp->data);
		resp->data = NULL;
	}

	free (resp);
	
	return 0;
}

#include "request.h"

request *request_init() {
	/* init a new request struct */
	int i;
	
	request *req = malloc(sizeof(request));
	req->headers = malloc(sizeof(hdr*) * HEADER_MAX_LEN);
	
	for (i = 0; i < HEADER_MAX_LEN; ++i) {
		req->headers[i] = malloc(sizeof(hdr));
	}
	
	req->body = NULL;
	req->body_len = 0;
	
	req->in_data = 0;
	req->data_alloc = 0;
	req->data_len = 0;
	req->data = NULL;
	
	req->quit = 0;
	req->process = 0;
	
	return req;
}

int request_parse_commands(server *srv, connection *conn) {	
	/* parse the buffer for commands */
	request *req = conn->request;
	char *resp = conn->response;

	char *data = conn->read_buffer;
	char *data_len = conn->buffer_len;
	int i;
	
	for (i = 0; i < data_len; ++i) {
		unsigned char cur = data[i];
		
		/* look for DATA */
		if (req->in_data == 0) {
			if (strncmp(data+i, "DATA\r\n", 6) == 0) {
			    	/* found DATA */
			    	response_append (resp, "354 End data with <CR><LF>.<CR><LF>\r\n");
			    	req->in_data = 1;
			    	i += 5;
			}
			else if (strncmp(data+i, "EHLO", 4) == 0) {
			    	response_append (resp, "250 Hi, you have a report for me?\r\n");
			}
			else if (strncmp(data+i, "MAIL FROM:", 10) == 0) {
			    	response_append (resp, "250 Ok\r\n");
			}
			else if (strncmp(data+i, "RCPT TO:", 8) == 0) {
			    	response_append (resp, "250 Ok\r\n");
			}
			else if (strncmp(data+i, "QUIT", 4) == 0) {
			    	response_append (resp, "221 Bye, see you tomorrow!\r\n");
			    	req->quit = 1;
			}
		} else {
			if (req->data_len + 2 >= req->data_alloc) {
				/* realloc data var */
				req->data_alloc = req->data_len + 512;
				char *tmp = malloc(req->data_alloc);
				
				if (req->data != NULL) {
					memcpy (tmp, req->data, req->data_len);
					free (req->data);
				}
				
				req->data = tmp;
			}
			
			if (req->data_len < srv->config->data_buffer_size) {
				/* store data */
				req->data[req->data_len++] = cur;
			
				/* check for end of data */
				if (req->data_len > 4) {
					if (strncmp(req->data+req->data_len-5, "\r\n.\r\n", 5) == 0) {
						response_append (resp, "250 Ok: queued as nada\r\n");
						req->data[req->data_len-5] = NULL;
					    	req->in_data = 0;
					    	req->process = 1;
					    	req->quit = 1;
					}
				}
			} else {
				safe_warn (srv, "data size is too big.");
				conn->status = CONN_ERROR;
				break;
			}
		}
	}
	
	if (conn->status == CONN_ERROR) {
		return 1;
	}
	
	return 0;
}

int request_parse_data(server *srv, connection *conn) {	
	/* parse the mail data (DATA) */
	request *req = conn->request;
	
	char *data = req->data;
	int data_len = req->data_len;

	int i;
	int header_count = 0;
	int done = 0;
	int is_headers = 0;
	int is_key = 1;
	int cur_len = 0;
	
	int in_line_folding = 0;
	
	hdr **headers = req->headers;
	
	for (i = 0; i < data_len && !done; ++i) {
		unsigned char cur = data[i];
		/*TODO: fix correct null terminating */
		
		if (in_line_folding) {
			if (data[i] == ' ' || data[i] == '\t' || data[i] == '\r' || data[i] == '\n' ) {
				continue;
			} else {
				in_line_folding = 0;
			}
		}
		
		if (data_len - i > 2) {
			if (data[i] == '\r' && data[i+1] == '\n' &&
				data[i+2] == '\r' && data[i+3] == '\n') {
				i += 2;
			    	done = 1;
			}
		}
		
		if (cur == ':' && is_key) {
			headers[header_count]->key[cur_len] = '\0';
			is_key = 0;
			cur_len = 0;
			++i;
		} else if (strncmp(data+i, "\r\n\t", 3) == 0 || strncmp(data+i, "\r\n ", 3) == 0) {
			/* header line folding */
			/*in_line_folding = 1;*/
			i += 2;
		} else if (cur == '\r') {
			if (header_count < HEADER_MAX_LEN) {
				headers[header_count]->value[cur_len] = '\0';
			}
			
			cur_len = 0;
			is_key = 1;
			
			++header_count;
			++i;	
		} else if (cur) {
			if (is_key) {
				if (cur != ' ') {
					if (cur_len < KEY_MAX_LEN-1) { 
						headers[header_count]->key[cur_len++] = cur;
					}
				}
			} else {	
				if (cur_len < VALUE_MAX_LEN-1) {
					headers[header_count]->value[cur_len++] = cur; 
				}
			}
		}
	}

	req->header_count = header_count;

	/* mail body */
	if (i < data_len && done == 1) {
		req->body_len = data_len - i;
		req->body = malloc(req->body_len + 1);
		memcpy (req->body, data+i, req->body_len);
		req->body[req->body_len] = '\0';
	}

	return 0;
}

int request_free(request *req) {
	int i;
	
	/* free headers */
	for (i = 0; i < HEADER_MAX_LEN; ++i) {
		free (req->headers[i]);
		req->headers[i] = NULL;
	}
	
	free (req->headers);
	
	if (req->data != NULL) {
		free (req->data);
		req->data = NULL;
	}
	
	if (req->body != NULL) {
		free (req->body);
		req->body = NULL;
	}
	
	free (req);
	
	return 0;
}

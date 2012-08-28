#include "base64.h"


/* Base64 decode algorithm using openssl */
int base64_decode(char *input, unsigned char **output, int length) {
	BIO *b64, *bmem;

	char *wrapped = base64_wrap(input, length);
	length = strlen(wrapped);

	unsigned char *buffer = malloc(length);
	memset(buffer, 0, length);

	b64 = BIO_new(BIO_f_base64());
	bmem = BIO_new_mem_buf(wrapped, length);
	bmem = BIO_push(b64, bmem);

	int len = BIO_read(bmem, buffer, length);
	BIO_free_all(bmem);
	free (wrapped);

	output[0] = buffer;
	return len;
}

/* Base64 encode algorithm using openssl */
char *base64_encode(const unsigned char *input, int length) {
	BIO *bmem, *b64;
	BUF_MEM *bptr;

	b64 = BIO_new(BIO_f_base64());
	bmem = BIO_new(BIO_s_mem());
	b64 = BIO_push(b64, bmem);
	
	BIO_write (b64, input, length);
	BIO_flush (b64);
	BIO_get_mem_ptr (b64, &bptr);

	char *buff = malloc(bptr->length);
	memcpy (buff, bptr->data, bptr->length-1);
	buff[bptr->length-1] = '\0';

	BIO_free_all (b64);

	/* remove line breaks */
	int buff_len = strlen(buff);
	int i, cur = 0;
	for (i = 0; i < buff_len; ++i) {
		if (buff[i] != '\r' && buff[i] != '\n') {
			buff[cur++] = buff[i];
		}
	}
	buff[cur] = '\0';	

	return buff;
}

char *base64_wrap(char *str, int len) {
	char *tmp = malloc(len*3+1);
	int tmp_len = 0;
	int i;
	int lcount = 0;
	
	for (i = 0; i < len; ++i) {
		if (str[i] == ' ' || lcount == 75) {
			tmp[tmp_len++] = str[i];
			tmp[tmp_len++] = '\r';
			tmp[tmp_len++] = '\n';
			lcount = 0;
		} else {
			tmp[tmp_len++] = str[i];
			++lcount;
		}
	}
	
	tmp[tmp_len] = '\0';
	return tmp;
}

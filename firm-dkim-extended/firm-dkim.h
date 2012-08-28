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
#ifndef _FIRM_DKIM_H_
#define _FIRM_DKIM_H_

#include <openssl/rand.h>
#include <openssl/rsa.h>
#include <openssl/engine.h>
#include <openssl/sha.h>

#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <openssl/bio.h>
#include <openssl/pem.h>
#include <openssl/buffer.h>
#include <openssl/err.h>

#define DKIM_KEY_MAX_LEN  	 128
#define DKIM_VALUE_MAX_LEN 	 1024
#define DKIM_RECORD_MAX_LEN	 2046

typedef struct {
	char *key;
	char *value;
} stringpair;

typedef struct {
	char v[DKIM_VALUE_MAX_LEN];
	char a[DKIM_VALUE_MAX_LEN];
	char c[DKIM_VALUE_MAX_LEN];
	char d[DKIM_VALUE_MAX_LEN];
	char s[DKIM_VALUE_MAX_LEN];
	char h[DKIM_VALUE_MAX_LEN];
	char bh[DKIM_VALUE_MAX_LEN];
	char b[DKIM_VALUE_MAX_LEN];
} dkim_header;

typedef struct {
	char k[DKIM_RECORD_MAX_LEN];
	char p[DKIM_RECORD_MAX_LEN];
} dkim_record;

dkim_header *dkim_get_header(stringpair **headers, int headerc);
dkim_record *dkim_parse_dns(char *txt);
int dkim_validate(stringpair **headers, int headerc, char *body, dkim_header *dh, char *pkey);
char *dkim_create(stringpair **headers, int headerc, char *body, char *pkey, char *domain, char *selector, int v);

#endif

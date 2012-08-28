#ifndef _BASE64_H_
#define _BASE64_H_

#include <string.h>
#include <openssl/sha.h>
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>

int base64_decode(char *input, unsigned char **output, int length);
char *base64_encode(const unsigned char *input, int length);
char *base64_wrap(char *str, int len);

#endif

#ifndef _BASE64_H_
#define _BASE64_H_

#include <stdio.h>
#include <string.h>

static const char base64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static const int BASE64_INPUT_SIZE = 57;

int isbase64(char c);
inline char base64_value(char c);
int base64_decode(char *dest, char *src, int srclen);

#endif

#include "base64.h"


int isbase64(char c) {
	return c && strchr(base64_table, c) != NULL;
}

inline char base64_value(char c) {
	const char *p = strchr(base64_table, c);
	if (p) {
		return p-base64_table;
	} else {
		return 0;
	}
}

int base64_decode(char *dest, char *src, int srclen) {
	if (*src == 0 || srclen == 0) {
		return 0;
	}
	
	char *p = dest;

	do {
		char a = base64_value(src[0]);
		char b = base64_value(src[1]);
		char c = base64_value(src[2]);
		char d = base64_value(src[3]);
		
		*p++ = (a << 2) | (b >> 4);
		*p++ = (b << 4) | (c >> 2);
		*p++ = (c << 6) | d;
		
		if(!isbase64(src[1])) {
			p -= 2;
			break;
		} 
		else if (!isbase64(src[2])) {
			p -= 2;
			break;
		} 
		else if(!isbase64(src[3])) {
			p--;
			break;
		}
		
		src += 4;
		while (*src && (*src == 13 || *src == 10)) src++;
	} while (srclen-= 4);
	
	*p = 0;
		
	return p-dest;
}

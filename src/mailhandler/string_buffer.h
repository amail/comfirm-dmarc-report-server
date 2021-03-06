#ifndef _STRING_BUFFER_H_
#define _STRING_BUFFER_H_

#include <stdio.h>
#include <string.h>

#define STRING_BUFFER_CHUNK_SIZE 512

typedef struct {
	char *str;
	int alloc_size;
	int size;
} string_buffer;

string_buffer *string_buffer_init();
int string_buffer_append(string_buffer *sb, char *value);
int string_buffer_free(string_buffer *sb);

#endif

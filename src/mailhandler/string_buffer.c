#include "string_buffer.h"

string_buffer *string_buffer_init() {
	string_buffer *sb = malloc(sizeof(string_buffer));
	sb->str = NULL;
	sb->alloc_size = 0;
	sb->size = 0;
	return sb;
}

int string_buffer_append(string_buffer *sb, char *value) {
	int val_len = strlen(value);
	
	if (val_len > 0) {
		if (val_len >= (sb->alloc_size - sb->size)) {
			// realloc the string buffer
			char *new = malloc(sb->alloc_size + STRING_BUFFER_CHUNK_SIZE + 1);
			if (sb->str != NULL) {
				memcpy (new, sb->str, sb->size);
				free (sb->str);
			}
			sb->str = new;
			sb->alloc_size = sb->alloc_size + STRING_BUFFER_CHUNK_SIZE;
		}
		
		memcpy (sb->str+sb->size, value, val_len);
		sb->size += val_len;
		sb->str[sb->size] = '\0';
	}
	
	return 1;
}

int string_buffer_free(string_buffer *sb) {
	if (sb->str != NULL) {
		free (sb->str);
	}
	free (sb);
	return 1;
}

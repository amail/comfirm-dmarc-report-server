#ifndef _BASE_H_
#define _BASE_H_

int math_min(int a, int b);
int math_max(int a, int b);
char* string_malloc(char* source, const int max_size);
int file_exist(char* filename);
int file_size(char* filename);

#endif

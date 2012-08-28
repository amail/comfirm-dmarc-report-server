#include <errno.h>
#include <sys/stat.h>

int math_min(int a, int b)
{
	if(a < b)
		return a;
	else
		return b;
}

int math_max(int a, int b)
{
	if(a > b)
		return a;
	else
		return b;
}

char* string_malloc(char* source, const int max_size)
{
	const int destination_size = math_min(max_size, strlen(source));
	char* destination = (char*)malloc(destination_size+1);

	memcpy(destination, source, destination_size);
	destination[destination_size] = '\0';

	return destination;
}

int file_size(char* filename)
{
	struct stat filestat;

	if (stat(filename, &filestat) == -1 && errno == ENOENT) {
    		return -1;
	}
	
	if (S_ISDIR(filestat.st_mode)) {
		return -1;
	}
	
	return filestat.st_size;
}

int file_exist(char* filename)
{
	struct stat filestat;

	if (stat(filename, &filestat) == -1 && errno == ENOENT) {
    		return -1;
	}
	
	if (S_ISDIR(filestat.st_mode)) {
		return -1;
	}
	
	return 0;
}

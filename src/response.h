#ifndef _RESPONSE_H_
#define _RESPONSE_H_

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <time.h>

#include <libjson/storage.h>
#include <libjson/libjson.h>

#include "base.h"

response *response_init();
int response_append(response *resp, char *data);
int response_free(response *resp);

void *get_param_from_storage(storage *st, char *id, char type);
char *response_build_uuid(int64_t lo, int64_t hi);

#endif

#ifndef _DATABASE_H_
#define _DATABASE_H_

#include "base.h"

database *database_init();
int database_connect(database *db);

redisReply *database_execute_command(database *db, const char *cmd, ...);

int database_free_reply(redisReply *reply);
int database_close(database *db);
int database_free(database *db);

#endif

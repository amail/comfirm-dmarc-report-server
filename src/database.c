#include <stdio.h>
#include <stdarg.h>
#include "database.h"

database *database_init() {
	/* init database */
	database *db = malloc(sizeof(database));
	
	db->is_connected = 0;
	db->conn = NULL;
	
	db->host = NULL;
	db->port = 0;
	
	/* 1.5 seconds timeout */
	db->timeout.tv_sec = 1;
	db->timeout.tv_usec = 500000;
	
	return db;
}

int database_connect(database *db) {
	/* connect to a database */
	db->conn = redisConnectWithTimeout(db->host, db->port, db->timeout);

	if (db->conn->err) {
		printf ("ERROR redisConnectWithTimeout: %s\n", db->conn->errstr);
		db->is_connected = 0;
		return -1;
	}

	db->is_connected = 1;

	return 0;
}

redisReply *database_execute_command(database *db, const char *cmd, ...) {
	/* execute a command */
	if (db->conn->err) {
		/* disconnected, try to reconnect */
		if (database_connect(db) == -1) {
			db->is_connected = 0;
			return NULL;
		}
	}
	
	va_list args;
	va_start (args, cmd);
	
	/* execute the command */
	char *buffer = malloc(strlen(cmd)+1024);
	int size = vsnprintf(buffer, strlen(cmd)+1024, cmd, args);
	
	redisReply *reply = redisCommand(db->conn, buffer);
	
	va_end (args);
	free (buffer);
	
	return reply;
}

int database_free_reply(redisReply *reply) {
	if (reply != NULL) {
		freeReplyObject (reply);
	}
	
	return 0;
}

int database_close(database *db) {
	/* close the connection */
	redisFree (db->conn);

	return 0;
}

int database_free(database *db) {
	/* free database */
	database_close (db);
	free (db);
	
	return 0;
}

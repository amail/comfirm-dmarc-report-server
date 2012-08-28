#ifndef _REQUEST_H_
#define _REQUEST_H_

#include "base.h"

request *request_init();
int request_parse_commands(server *srv, connection *conn);
int request_parse_data(server *srv, connection *conn);
int request_free(request *req);

#endif

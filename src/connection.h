#ifndef _CONNECTION_H_
#define _CONNECTION_H_

#include <fcntl.h>
#include <netinet/tcp.h>
#include <time.h>
#include <sys/uio.h>
#include <sys/epoll.h>

#include <sys/sendfile.h>
#include <firm-dkim.h>

#include "base.h"
#include "safe.h"
#include "lib-pq/persisted_queue.h"

connection *connection_init();
connection *connection_setup(master_server *master_srv);
int connection_start(master_server *master_srv, connection *conn);
int connection_handle(worker *w, connection *conn, event_handler *ev_handler, int fd);
int connection_reset(master_server *master_srv, connection *conn);
int connection_free(master_server *master_srv, connection *conn);
char *connection_get_ip(connection *conn);
unsigned long connection_get_iplong(connection *conn);

#endif

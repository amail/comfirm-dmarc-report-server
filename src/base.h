#ifndef _BASE_H_
#define _BASE_H_

#include "config.h"

#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <stdio.h>
#include <semaphore.h>

#ifdef HAVE_SYS_EPOLL_H
	#include <sys/epoll.h>
#elif defined(HAVE_SYS_EVENT_H)
	#include <sys/types.h>
	#include <sys/event.h>
	#include <sys/time.h>
#endif


/* start key to attach shared memory */
/* TODO: use pid instead */
#define SHARED_MEM_KEY 8212

/* connection event types */
#define CONN_STARTED  1
#define CONN_READING  2
#define CONN_ERROR    3
#define CONN_WRITING  4
#define CONN_CLOSED   5
#define CONN_INACTIVE 6

#define KEY_MAX_LEN 	 128
#define VALUE_MAX_LEN 	 1024
#define HEADER_MAX_LEN	 20
#define BODY_MAX_LEN	 8000


typedef struct {
	sem_t mutex;
} lock;

typedef struct {
	/* used by headers */
	char key[KEY_MAX_LEN];
	char value[VALUE_MAX_LEN];
} hdr;

typedef struct {
	int fd;
	int listen_port;
	
	socklen_t clilen;
	struct sockaddr_in serv_addr;
	struct sockaddr_in cli_addr;
} sock;

typedef struct {
	hdr **headers;
	int header_count;
	
	char *body;
	int body_len;
	
	int in_data;
	
	char *data;
	int data_alloc;
	int data_len;
	
	int quit;
	int process;
} request;

typedef struct {
	int data_alloc;
	int data_len;
	char *data;
} response;

typedef struct {
	int fd; 		/* socket */
	int port;		/* connected on this port */
	void *server;		/* which server this connection is assigned to */

	request *request;
	response *response;
	
	time_t start_ts; 	/* when the connection was initialized */
	
	int status;		/* connection status */
	
	char *read_buffer;
	int buffer_len;
} connection;

typedef struct {
	int fd;
#ifdef HAVE_SYS_EPOLL_H
	struct epoll_event **events;
#elif defined(HAVE_SYS_EVENT_H)
	struct kevent changes;
	struct kevent *events;
	int nevents;
#endif
} event_handler;

typedef struct {
	/* config */
	int listen_port;
	
	int max_workers;
	int max_pending;
	int max_clients;
	
	int child_stack_size;
	
	int read_buffer_size;	
	int write_buffer_size;
	int data_buffer_size;
	
	int tcp_nodelay; 	/* Nagle (TCP No Delay) algorithm */
	
	char *server_name; 	/* server name, used in welcome message */
	char *hostname;
	int daemonize;
	char *chroot;
	
	char *queue_file;
	int queue_size;
	
	char *web_service_url;
} config;

typedef struct {
	int running;

	char *config_file;	/* file path to the config file */
	config *config;		/* holds master server settings */

	void *server;
	
	void **workers; 	/* keep information and pointers to all workers */
	void **worker_map;	/* map an fd to a worker, just pointers */
	lock lock_log; 		/* semaphore lock between processes when logging */
} master_server;

typedef struct {
	int running; 		/* on / off? */
	
	config *config;
	
	sock *server_socket;	/* listening socket */
	
	master_server *master;	/* pointer to the master server */
} server;

typedef struct {
	int num; 			/* which process number */
	master_server *master_srv;	/* master server (configs and servers), not shared */
	pid_t pid;			/* Process ID */
	
	void *msg_queue;		/* internal queueing of messages */
	
	event_handler ev_handler;	/* event handler - epoll, kqueue */
} worker;

typedef struct {
	hdr headers[HEADER_MAX_LEN];
	int header_count;
	char body[BODY_MAX_LEN];
} mail;



#ifndef _LOCK_H_
#include "lock.h"
#endif

#ifndef _SERVER_H_
#include "server.h"
#endif

#ifndef _WORKER_H_
#include "worker.h"
#endif

#ifndef _SAFE_H_
#include "safe.h"
#endif

#ifndef _SOCKET_H_
#include "socket.h"
#endif

#ifndef _REQUEST_H_
#include "request.h"
#endif

#ifndef _RESPONSE_H_
#include "response.h"
#endif

#ifndef _CONNECTION_H_
#include "connection.h"
#endif

#ifndef _CONFIG_FILE_H_
#include "config_file.h"
#endif

#ifndef _EVENTS_H_
#include "events.h"
#endif

#endif

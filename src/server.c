#include "server.h"

int server_init(server *srv) {
	int i, e;
	
	/* init server socket */
	srv->server_socket = socket_init();

	return 0;
}

int server_free(server *srv) {
	/* free everything allocated in server_init */
	int i;
	
	/* free server socket */
	socket_free (srv->server_socket);
	
	/* free server */
	free (srv);
	
	return 0;
}

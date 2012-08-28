#include "connection.h"

connection *connection_init() {
	/* init connection struct */
    	return malloc(sizeof(connection));
}

connection *connection_setup(master_server *master_srv) {
	/* pre-setup connections (for performance) */
	connection **conns = malloc(master_srv->config->max_clients * 2 * sizeof(connection*));
	
	int i;
	for (i = 0; i < master_srv->config->max_clients*2; ++i) {
		conns[i] = connection_init();
		conns[i]->status = CONN_INACTIVE;
		conns[i]->fd = i;
		conns[i]->server = master_srv->server;
	}
	
	return conns;
}

int connection_start(master_server *master_srv, connection *conn) {
	/* start a new connection */
	conn->status = CONN_STARTED;
	conn->start_ts = NULL; //time(NULL); not used anyway...
	conn->buffer_len = 0;
	conn->read_buffer = NULL;

	conn->response = response_init();	
	conn->request = request_init();
	
	return 0;
}

int connection_handle(worker *w, connection *conn, event_handler *ev_handler, int fd) {
	/* handle a connection event */
	server *srv = conn->server;
	int i = 0;

	/* new connection */
	if (conn->status == CONN_STARTED) {
		/* send welcome message */
		response_append (conn->response, "220 dmarc.comfirm.se SMTP Comfirm DMARC Report Server\r\n");
		conn->status = CONN_WRITING;
	}

	/* reading from client */
	if (conn->status == CONN_READING) {
		if (conn->buffer_len == 0) {
			/* allocate read buffer */
			conn->read_buffer = malloc(srv->master->config->read_buffer_size+1);
		}
		
		if (srv->config->read_buffer_size - conn->buffer_len > 0) {
			/* read from socket */
			int s;
			if ((s = socket_read(conn->fd, conn->read_buffer+conn->buffer_len,
			    srv->master->config->read_buffer_size - conn->buffer_len)) != -1) {
				conn->buffer_len += s;
			} else {
				perror ("ERROR reading from socket");
				conn->status = CONN_ERROR;
			}
		} else {
			/* too big request */
			safe_warn (srv, "request size is too big.");
			conn->status = CONN_ERROR;
		}
		
		if (conn->buffer_len > 0 && conn->status != CONN_ERROR) {
			/* parse the commands (SMTP) */
			if (request_parse_commands(srv, conn) == 0) {
				if (conn->request->process) {
					printf ("Process data\n");
					
					/* parse the data (DATA) */
					request_parse_data (srv, conn);
					
					printf ("Processed data\n");
					
					if (conn->request->header_count > 0) {
						printf ("In queueing process...\n");
					
						/* create a mail object */
						mail *m = malloc(sizeof(mail));
						
						printf ("Allocated mail...\n%s\n", conn->request->body);
						
						/* copy body */
						if (conn->request->body_len < BODY_MAX_LEN) {
							printf ("copying...\n");
							memcpy (&m->body, conn->request->body, conn->request->body_len+1);
							printf ("copyed...\n");
						} else {
							safe_warn (srv, "body to large for queue.");
							m->body[0] = NULL;
						}
					
						printf ("copy headers...\n");
					
						/* copy headers */
						m->header_count = conn->request->header_count;
						for (i = 0; i < conn->request->header_count && i < conn->request->header_count; ++i) {
							memcpy (&m->headers[i], conn->request->headers[i], sizeof(hdr)); 
						}
					
						printf ("queueing...\n");
					
						/* add this mail to the queue for further validation */
						if (pq_enqueue(w->msg_queue, m) == PQ_MESSAGE_SIZE_REACHED_ERROR) {
							printf (" * ERROR Unable to queue message. Too many in queue!\n");	
						} else {
							printf (" * Successfully queued message!\n");
						}
						
						free (m);
					} else {
						/* missing headers */
						safe_warn (srv, "missing headers");
						conn->status = CONN_ERROR;		
					}
				}
				
				/* respond to the commands */
				conn->status = CONN_WRITING;
			} else {
				/* some error */
				conn->status = CONN_ERROR;
			}
		}
	}

	/* writing to socket */
	if (conn->status == CONN_WRITING) {
		/* send to client */
		if (conn->response->data_len <= srv->master->config->write_buffer_size) {
			/* small buffer, send it with one call */
			if (send(conn->fd, conn->response->data, conn->response->data_len, 0) == -1) {
				perror ("ERROR writing to socket");
				conn->status = CONN_ERROR;
			}
		}
		else {
			/* large buffer, chunk it */
			unsigned long data_sent = 0;
			int flag, size, sent;
		
			/* TODO: TCP_CORK or MSG_MORE? */
			while (data_sent < conn->response->data_len) {
				if ((size = conn->response->data_len - data_sent) >= srv->master->config->write_buffer_size) {
					size = srv->master->config->write_buffer_size;
					flag = MSG_MORE;
				} else {
					flag = 0;
				}
				
				if ((sent = send(conn->fd, conn->response->data+data_sent, size, flag)) == -1) {
					perror ("ERROR writing to socket");
					conn->status = CONN_ERROR;
					break;
				}
				
				data_sent += size;
			}
		}

		if (conn->status != CONN_ERROR) {
			if (conn->request->quit) {
				/* close connection */
				conn->status = CONN_CLOSED;
			} else {
				/* read some more */
				conn->status = CONN_READING;
			}

			/* clear some data */
			if (!conn->request->in_data) {
				request_free (conn->request);
				conn->request = request_init();	
			}
		
			response_free (conn->response);
			conn->response = response_init();
		
			if (conn->read_buffer != NULL) {
				free (conn->read_buffer);
				conn->read_buffer = NULL;
			}
			
			conn->buffer_len = 0;
		}
	}
	
	/* connection error */
	if (conn->status == CONN_ERROR) {
		conn->status = CONN_CLOSED;
	}
	
	/* close connection */
	if (conn->status == CONN_CLOSED) {
		if (events_del_event(ev_handler, fd) == -1) {
			perror ("ERROR events_del_event");
		}
		
		close (fd);
		conn->status = CONN_INACTIVE;
	}
	
	/* reset the connection */
	if (conn->status == CONN_INACTIVE) {
		connection_reset (srv->master, conn);
	}

	return 0;
}

int connection_reset(master_server *master_srv, connection *conn) {
	/* reset a connection */
	conn->start_ts = NULL;
	conn->buffer_len = 0;
	conn->status = CONN_INACTIVE;

	if (conn->read_buffer != NULL) {
		free (conn->read_buffer);
		conn->read_buffer = NULL;
	}

	if (conn->request != NULL) {
		request_free (conn->request);
		conn->request = NULL;
	}

	if (conn->response != NULL) {
		response_free (conn->response);
		conn->response = NULL;
	}

	return 0;
}

int connection_free(master_server *master_srv, connection *conn) {
	/* when the server shuts down, all connections need to be freed */
	connection_reset (master_srv, conn);
	free (conn);

	return 0;
}

char *connection_get_ip(connection *conn) {
	/* get the ip as string from the connection */
	char *str = malloc(INET6_ADDRSTRLEN+1);

	struct sockaddr_storage sa;
	socklen_t sa_len = sizeof(sa);

	if (getpeername(conn->fd, (struct sockaddr*)&sa, &sa_len) == -1) {
		printf ("ERROR getpeername\n");
		free (str);
		return NULL;
	}

	if (sa.ss_family == AF_INET) {
		/* IPv4 */
		struct sockaddr_in *s = (struct sockaddr_in*) &sa;
		inet_ntop (AF_INET, &s->sin_addr, str, INET6_ADDRSTRLEN);
	}
	else {
		/* IPv6 */
		struct sockaddr_in6 *s = (struct sockaddr_in6*) &sa;
		inet_ntop (AF_INET6, &s->sin6_addr, str, INET6_ADDRSTRLEN);
	}

	return str;
}


unsigned long connection_get_iplong(connection *conn) {
	/* get the ip as long from the connection */
	struct sockaddr_storage sa;
	socklen_t sa_len = sizeof(sa);

	if (getpeername(conn->fd, (struct sockaddr*)&sa, &sa_len) == -1) {
		printf ("ERROR getpeername\n");
		return 0;
	}

	if (sa.ss_family == AF_INET) {
		/* IPv4 */
		struct sockaddr_in *s = (struct sockaddr_in*) &sa;
		return s->sin_addr.s_addr;
	}
	
	/* does not support IPv6 */	
	return 0;
}


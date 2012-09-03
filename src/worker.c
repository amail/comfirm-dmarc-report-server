#include "worker.h"

worker *worker_init(master_server *master_srv, int num) {
	/* init a new worker */
	int shmid;
	key_t key = SHARED_MEM_KEY+num;
	worker *w = NULL;

	/* create a shared memory */
	if ((shmid = shmget(key, sizeof(worker), IPC_CREAT | 0666)) < 0) {
		perror ("ERROR shmget");
		return NULL;
	}

	/* attach it */
	if ((w = shmat(shmid, NULL, 0)) == (char*) -1) {
		perror ("ERROR shmat");
		return NULL;
	}
	
	w->num = num;
	w->master_srv = master_srv;
	
	/* initiate the message queue */
	char *filepath = malloc(strlen(master_srv->config->queue_file)+10);
	sprintf (filepath, "%s-%d", master_srv->config->queue_file, num);
	
	if (pq_init(&w->msg_queue, filepath, master_srv->config->queue_size) < 0) {
		printf ("ERROR could not initiate the internal queue.\n");
		return NULL;
	}
	
	/* now, start the dequeueing service for this worker */
	if (fork() == 0) {
		/* child */	
		char *q_size = malloc(30);
		sprintf (q_size, "%d", master_srv->config->queue_size);
	
		if (execl("/usr/local/bin/dmarc-mailhandler", "/usr/local/bin/dmarc-mailhandler", 
				filepath, 
				q_size,
				master_srv->config->web_service_url,
				(char*)0) == -1) {
			printf (" * ERROR could not start dequeueing service.\n");
			exit (1);
		}
		
		free (filepath);
		free (q_size);
	}

	return w;
}

int worker_free(worker *w) {
	/* detach the shared memory */
	if (shmdt(w) == -1) {
		perror ("ERROR shmdt");
		return -1;
	}
	
	return 0;
}

int worker_spawn(int number, master_server *master_srv) {
	/* spawn a new worker process */
	void **child_stack = malloc(master_srv->config->child_stack_size);
	worker *w = master_srv->workers[number];

	/* TODO: use rfork() on FreeBSD & OpenBSD */ 
	if (clone(worker_server, child_stack+sizeof(child_stack) / sizeof(*child_stack), CLONE_FS | CLONE_FILES, w) == -1) {
		perror ("ERROR clone()");
		return -1;
	}
	
	return 0;
}

static int worker_server(worker *info) {
	/* server worker */
	int nfds, fd, i;
	worker *w = NULL;

	int num = info->num;
	master_server *master_srv = info->master_srv;

	/* attach the shared mem */	
	int shmid;
	key_t key = SHARED_MEM_KEY+num;

	if ((shmid = shmget(key, sizeof(worker), 0666)) < 0) {
		perror ("ERROR shmget");
		exit (1);
	}

	/* attach it */
	if ((w = shmat(shmid, NULL, 0)) == (char*) -1) {
		perror ("ERROR shmat");
		exit (1);
	}
	
	/* process id */
	w->pid = getpid();
	
	/* worker process started */
	printf (" * Worker process #%d started.\n", num+1);
	
	/* pre-setup worker connections */
	connection **conns = connection_setup(master_srv);  
	
	/* create a new event handler for this worker */
	event_handler *ev_handler = events_create(master_srv->config->max_clients);

	/* share the event fd */
	w->ev_handler.fd = ev_handler->fd;
	
	/* entering main loop... */
	while (master_srv->running) {
		/* check for new data */
		if ((nfds = events_wait(ev_handler, master_srv)) == -1) {
			perror ("ERROR epoll_pwait");
		}

		for (i = 0; i < nfds; ++i) {
			/* data received */
			fd = events_get_fd(ev_handler, i);
			connection *conn = conns[fd];
			
			if (events_closed(ev_handler, i)) {
				/* the other end closed the connection */
				conn->status = CONN_CLOSED;
			} 
			else if (conn->status == CONN_INACTIVE) {
				/* this connection is inactive, initiate a new connection */
				connection_start (master_srv, conn);
				conn->status = CONN_READING;
			}
			
			/* handle the connection */
			connection_handle (w, conn, ev_handler, fd);
		}
	}
	
	printf (" * Shutting down worker process #%d...\n", num+1);
	
	/* free event handler */
	events_free (ev_handler, master_srv->config->max_clients);

	/* free this workers memory */
	worker_free (w);

	exit (0);
}


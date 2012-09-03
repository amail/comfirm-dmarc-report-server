#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/mman.h>

#include "config.h"
#include "base.h"

master_server *master_serv; /* used by signal handler */


static void signal_handler(int sig) {
	if (sig == SIGCHLD) {
		/* child exited, should start it again */
		printf ("WARNING a child process was shutdown!\n");
	} else {
		/* shutdown the whole server */
		master_serv->running = 0;
	}
}

static void daemonize() {
	/* daemonize server */
	if (fork() != 0) {
		exit (0);
	}

	umask (0);

	if (setsid() == -1) {
		perror ("ERROR setsid");
		exit (1);
	}
	
	/* close standard file descriptors */
	/*close (STDIN_FILENO);
        close (STDOUT_FILENO);
        close (STDERR_FILENO);*/
}

int master_server_init(master_server *master_srv) {
	/* init the master server */
	int i;
	
	/* init locks */
	lock_init (&master_srv->lock_log);
	
	/* allocate worker pointers */
	master_srv->workers = malloc(master_srv->config->max_workers * sizeof(worker*));
	
	for (i = 0; i < master_srv->config->max_workers; ++i) {	
		if ((master_srv->workers[i] = worker_init(master_srv, i)) == NULL) {
			perror ("ERROR creating a new worker");
			return -1;
		}
	}
	
	/* map workers to fds */
	master_srv->worker_map = malloc(sizeof(worker) * master_srv->config->max_clients * 2);
	for (i = 0; i < master_srv->config->max_clients * 2; ++i) {	
		master_srv->worker_map[i] = master_srv->workers[ i % master_srv->config->max_workers ];
	}
	
	/* allocate server */
	master_srv->server = malloc(sizeof(server));
	
	return 0;
}

int master_server_free(master_server *master_srv) {
	/* free everything allocated in master_server_init */
	int i;
	
	/* free worker pointers */
	for (i = 0; i < master_srv->config->max_workers; ++i) {
		worker_free (master_srv->workers[i]);
	}
	
	free (master_srv->workers);
	free (master_srv->worker_map);
	
	/* free server */
	free (master_srv->server);
	
	/* free locks */
	lock_free (&master_srv->lock_log);
	
	/* free the master server */
	free (master_srv);
	
	return 0;
}


int main(int argc, char *argv[]) {
	int nfds, fd, i, c;
	
	/* lock all memory in physical RAM */
	if (mlockall(MCL_CURRENT | MCL_FUTURE) == -1) {
		perror ("ERROR mlockall");
	}
	
	/* create a new master server */
	master_server *master_srv = malloc(sizeof(master_server));

	/* setup signal handlers */
	signal (SIGHUP, signal_handler);
	signal (SIGTERM, signal_handler);
	signal (SIGINT, signal_handler);
	signal (SIGKILL, signal_handler);
	signal (SIGQUIT, signal_handler);
	signal (SIGCHLD, signal_handler); /* child exited */
	
	/* parse command args (to get to config path) */
	while ((c = getopt (argc, argv, "abc:")) != -1) {
		switch (c) {
			case 'c':
				master_srv->config_file = malloc(strlen(optarg)+1);
				memcpy (master_srv->config_file, optarg, strlen(optarg)+1);
			break;
			
			case '?':
				if (optopt == 'c') {
					fprintf (stderr, "Option -%c requires an argument.\n", optopt);
				}
				
				exit (1);
			break;
		}
	}

	/* load master server settings from file */
	printf (" * Loading configuration file '%s'...\n", master_srv->config_file);
	master_srv->config = config_init();
	
	if (config_load(master_srv->config_file, master_srv->config) == -1) {
		perror ("ERROR loading config file");
		exit (1);
	}
	
	/* initiate the master server */
	if (master_server_init(master_srv) == -1) {
		exit (1);
	}
	
	master_srv->running = 1;

	/* daemonize? */
	if (master_srv->config->daemonize) {
		printf (" * Daemonizing...\n");
		daemonize();
	}

	/* chroot? */
	if (strlen(master_srv->config->chroot) > 0) {
		if (chdir(master_srv->config->chroot) != 0) {
			perror ("ERROR chdir");
			exit (1);
		}
		if (chroot(master_srv->config->chroot) != 0) {
			perror ("ERROR chroot");
			exit (1);
		}
	} else {
		if (chdir("/") != 0) {
			perror ("ERROR chdir");
			exit (1);
		}
	}
	
	/* create a new event handler for listening */
	event_handler *ev_handler = events_create(master_srv->config->max_clients);
	
	/* initiate the new server */
	server *srv = master_srv->server; 
	server_init (srv);
	config_copy (srv->config, master_srv->config);
	srv->running = 1;
	srv->master = master_srv;
	
	/* start listen */
	printf (" * Listening on port %d.\n", srv->config->listen_port);
			
	if (socket_listen(srv) == -1) {
		perror ("ERROR socket_listen");
		exit (1);
	}

	/* add it to the event handler */
	if (events_add_event(ev_handler, srv->server_socket->fd) == -1) {
		error ("ERROR events_add_event");
		exit (1);
	}
	
	/* starting workers */
	for (i = 0; i < master_srv->config->max_workers; ++i) {	
		worker_spawn (i, master_srv);
		usleep (1);
	}

	usleep (500000);
	
	printf (" * The server is up and running!\n");

	/* the server is up and running, entering the main loop... */
	while (master_srv->running) {
		/* wait for a new connection */
		if ((nfds = events_wait(ev_handler, master_srv)) == -1) {
			perror ("ERROR epoll_pwait");
		}
		
		for (i = 0; i < nfds; ++i) {
			/* new connection */
			fd = events_get_fd(ev_handler, i);
			
			/* accept the connection */
			if ((fd = socket_accept(srv->server_socket)) == -1) {
				perror ("ERROR on accept");
			}
			else {
				/* get the worker for this fd */
				worker *w = master_srv->worker_map[fd];
				
				/* set socket options */
				socket_setup (master_srv, fd);

				/* start a temporary connection */
				connection *conn = connection_init();
				conn->fd = fd;
				conn->server = srv;
				connection_start (master_srv, conn);
				
				/* handle the new connection (send welcome message) */
				connection_handle (w, conn, &w->ev_handler, fd);
				connection_free (master_srv, conn);

				/* add it to the worker's event handler */
				if (events_add_event_2(&w->ev_handler, fd) == -1) {
					error ("ERROR events_add_event_2");
				}
			}
		}
	}
	
	/* wait for all child processes to exit */
	for (i = 0; i < master_srv->config->max_workers; ++i) {
		worker *w = master_srv->workers[i];	
		waitpid (w->pid, NULL, 0);
	}
	
	/* close server connections */
	//socket_close (srv->server_socket);
	
	/* free the master server */
	master_server_free (master_srv);
	free (master_srv->config_file);
	
	printf (" * The server is down, exiting...\n");
	
	return 0; 
}

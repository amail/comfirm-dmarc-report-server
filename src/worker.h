#ifndef _WORKER_H_
#define _WORKER_H_

#include <sys/epoll.h>
#include <sched.h>

#include <sys/ipc.h>
#include <sys/shm.h>

#include "base.h"
#include "lib-pq/persisted_queue.h"

worker *worker_init(master_server *master_srv, int num);
int worker_spawn(int number, master_server *master_srv);
int worker_free(worker *w);
static int worker_server(worker *w);

#endif

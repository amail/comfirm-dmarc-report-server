#ifndef _PERSISTEDQUEUE_H_
#define _PERSISTEDQUEUE_H_

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>

#include "../base.h"

/*Errors related to MMF*/
#define PQ_MEM_MAP_OPEN_FILE_ERROR -1000
#define PQ_MEM_MAP_LSEEK_STRECH_ERROR -1001
#define PQ_MEM_MAP_EOF_WRITE_BYTE_ERROR -1002
#define PQ_MEM_MAP_MAPPING_ERROR -1003
#define PQ_MEM_MAP_UNMAPPING_ERROR -1004
#define PQ_MEM_MAP_INACCURATE_MESSAGE_SIZE -1005

/*General queue errors*/
#define PQ_MESSAGE_SIZE_REACHED_ERROR -2000
#define PQ_MESSAGE_QUEUE_EMPTY_ERROR -2001

/*Misc errors*/
#define PQ_REFERENCE_NOT_SET_ERROR -3000
#define PQ_SEMAPHORE_ERROR -4000

/*Semaphore key*/
#define SEM_KEY 1989

union semun {
    int val; 
    struct semid_ds *buf; 
    unsigned short int *array; 
    struct seminfo *__buf; 
};

typedef struct {
	volatile int write_offset, read_offset;
	volatile int message_count, message_max_count;
	volatile long memory_size;
} pq_ref_shared;

typedef struct {
	int handle;
	pq_ref_shared* shared;
	char* memory;
	int semid;
} pq_ref;

int semaphore_init(int semid);
int semaphore_wait(int semid);
int semaphore_post(int semid);


/*Creates a new queue*/
int pq_init(pq_ref** ref, char* filepath, int message_size);

/*Adds a new message at the top of the queue*/
int pq_enqueue(pq_ref* reference, mail* source);

/*Dequeues a message from the bottom of the queue*/
int pq_dequeue(pq_ref* reference, mail* destination);

/*Retrieves the mapped memory for a specific handle*/
int pq_get_mapped_memory(int handle, long memory_size, char** memory, int map_write);

/*Synchronizes/persists/flushes header data to disk*/
int pq_header_sync(pq_ref* reference, int write);

/*Blocks until a new message is enqueued*/
int pq_wait(pq_ref* reference);

/*Closes all open queue references*/
int pq_close(pq_ref* reference);

#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/file.h>

#include "persisted_queue.h"
#include "base.h"
#include "base.c"


int semaphore_init(int semid) {
    union semun argument; 
    unsigned short values[1]; 
    values[0] = 1; 
    argument.array = values; 
    return semctl (semid, 0, SETALL, argument); 
}

int semaphore_wait(int semid) {
    struct sembuf operations[1]; 
    /* Use the first (and only) semaphore */ 
    operations[0].sem_num = 0; 
    /* Decrement by 1 */ 
    operations[0].sem_op = -1; 
    /* Permit undoing */ 
    operations[0].sem_flg = SEM_UNDO; 
 
    return semop (semid, operations, 1); 
}
 
int semaphore_post(int semid) {
    struct sembuf operations[1]; 
    /* Use the first (and only) semaphore */ 
    operations[0].sem_num = 0; 
    /* Increment by 1 */ 
    operations[0].sem_op = 1; 
    /* Permit undo'ing */ 
    operations[0].sem_flg = SEM_UNDO; 
 
    return semop (semid, operations, 1); 
} 

int pq_open(pq_ref** reference, char* filepath, int message_max_count)
{
	char* temp_memory;
	int map_result, temp_file_size;
	pq_ref* temp_ref = (pq_ref*)malloc(sizeof(pq_ref));

	if((temp_file_size = file_size(filepath)) != -1){
		/*Assert that the message size of the existing file is correct*/
		if(temp_file_size != sizeof(pq_ref_shared)+(sizeof(mail)*message_max_count)) {
			return PQ_MEM_MAP_INACCURATE_MESSAGE_SIZE;
		}
	}

	if((temp_ref->handle = open(filepath, O_RDWR, (mode_t)0600)) == -1){
		return PQ_MEM_MAP_OPEN_FILE_ERROR;
	}

	if((map_result = pq_get_mapped_memory(temp_ref->handle, message_max_count * sizeof(mail), &temp_memory, false)) < -1){
		return map_result;
	}

	temp_ref->shared = (pq_ref_shared*)temp_memory;
	temp_ref->memory = temp_memory;
	*reference = temp_ref;

	return 0;
}

int pq_init(pq_ref** reference, char* filepath, int message_max_count)
{
	if(file_exist(filepath) == -1){
		char* temp_memory;
		int map_result, memory_size = message_max_count * sizeof(mail);
		pq_ref* temp_ref = (pq_ref*)malloc(sizeof(pq_ref));

		if((temp_ref->handle = open(filepath, O_RDWR | O_CREAT | O_TRUNC, (mode_t)0600)) == -1){
			return PQ_MEM_MAP_OPEN_FILE_ERROR;
		}

		if((map_result = pq_get_mapped_memory(temp_ref->handle, sizeof(pq_ref_shared)+memory_size, &temp_memory, true)) < 0){
			return map_result;
		}

		temp_ref->shared = (pq_ref_shared*)temp_memory;
		temp_ref->shared->memory_size = memory_size;
		temp_ref->shared->message_max_count = message_max_count;
		temp_ref->memory = temp_memory;
		*reference = temp_ref;

		// Flush header to disk
		msync(temp_ref->memory, sizeof(pq_ref_shared), MS_SYNC);

		/* set up semaphore */
	        if ((reference[0]->semid = semget(SEM_KEY, 1,  IPC_CREAT | 0666)) == -1) {
        	        perror (" * ERROR semget: semget() failed");
               	 	return PQ_SEMAPHORE_ERROR;
        	} else {
                	semaphore_init (reference[0]->semid);	
        	}

		return 0;
	}else{
		int ret = pq_open(reference, filepath, message_max_count);

		if (ret == 0) {
			/* set up semaphore */
        	        if ((reference[0]->semid = semget(SEM_KEY, 1,  IPC_CREAT | 0666)) == -1) {
                	        perror (" * ERROR semget: semget() failed");
                        	return PQ_SEMAPHORE_ERROR;
	                } else {
        	                semaphore_init (reference[0]->semid);
                	}
		}	
	
		return ret;
	}
}

int pq_get_mapped_memory(int handle, long memory_size, char** memory, int map_write)
{
	if(map_write == true) {
		if(lseek(handle, memory_size-1, SEEK_SET) == -1){
			close(handle);
			return PQ_MEM_MAP_LSEEK_STRECH_ERROR;
		}
		if(write(handle, "", 1) == -1){
			close(handle);
			return PQ_MEM_MAP_EOF_WRITE_BYTE_ERROR;
		}
	}

	if((*memory = (char*)mmap(0, memory_size, PROT_READ | PROT_WRITE, MAP_SHARED, handle, 0)) == MAP_FAILED){
		close(handle);
		return PQ_MEM_MAP_MAPPING_ERROR;
	}

	return 0;
}

int pq_enqueue(pq_ref* reference, mail* source)
{
	semaphore_wait (reference->semid);

	if(reference == NULL){
		semaphore_post (reference->semid);
		return PQ_REFERENCE_NOT_SET_ERROR;
	}

	if(reference->shared->message_count >= reference->shared->message_max_count){
		semaphore_post (reference->semid);
		return PQ_MESSAGE_SIZE_REACHED_ERROR;
	}

	void* message_offset = sizeof(pq_ref_shared)+reference->memory+(reference->shared->write_offset*sizeof(mail));
	memcpy(message_offset, source, sizeof(mail));

	reference->shared->write_offset = ++reference->shared->write_offset % reference->shared->message_max_count;
	++reference->shared->message_count;

	// Persist (flush to disk)
	pq_header_sync(reference, true);
	msync(message_offset, sizeof(mail), MS_SYNC);

	semaphore_post (reference->semid);
	return 0;
}

int pq_dequeue(pq_ref* reference, mail* destination)
{
	semaphore_wait (reference->semid);

	if(reference == NULL){
		semaphore_post (reference->semid);
		return PQ_REFERENCE_NOT_SET_ERROR;
	}

	if(reference->shared->message_count <= 0){
		semaphore_post (reference->semid);
		return PQ_MESSAGE_QUEUE_EMPTY_ERROR;
	}

	memcpy(destination, sizeof(pq_ref_shared)+reference->memory+(reference->shared->read_offset*sizeof(mail)),
		sizeof(mail));

	reference->shared->read_offset = ++reference->shared->read_offset % reference->shared->message_max_count;
	--reference->shared->message_count;

	// Persist (flush to disk)
	pq_header_sync(reference, false);

	semaphore_post (reference->semid);
	return 0;
}

int pq_header_sync(pq_ref* reference, int write)
{
	if(write == true){
		msync((void*)&(reference->shared->write_offset), sizeof(int), MS_SYNC);
	}else{
		msync((void*)&(reference->shared->read_offset), sizeof(int), MS_SYNC);
	}

	msync((void*)&(reference->shared->message_count), sizeof(int), MS_SYNC);

	return 0;
}

int pq_wait(pq_ref* reference)
{
	// TODO: Change to signaling instead!
	usleep(10 * 1000); //10 microseconds
	return 0;
}

/*Closes any open files and unmaps virtual memory*/
int pq_close(pq_ref* reference)
{
	if(reference == NULL)
		return PQ_REFERENCE_NOT_SET_ERROR;

	int result = 0;

	if(munmap(reference->memory, sizeof(pq_ref_shared)+reference->shared->memory_size) == -1){
		result = PQ_MEM_MAP_UNMAPPING_ERROR;
	}

	close(reference->handle);
	free(reference);

	return result;
}

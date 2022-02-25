#ifndef IPC_COMMON_H
#define IPC_COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/neutrino.h>
#include <sys/dispatch.h>
#include <sys/iofunc.h>
#include <unistd.h>
#include <errno.h>
#include <mqueue.h>
#include <pthread.h>
#include <sys/mman.h>

//* message passing: buffer & message types
#define MSG_BUFFER_SIZE 51200
#define MSG_DATA_FSIZE_TYPE (_IO_MAX + 3)
#define MSG_DATA_TYPE (_IO_MAX + 2)

// message queue: message size & max number of message
#define MQ_MAXMSG 1024
#define MQ_MSGSIZE 4096

// shm: set limit size (for now only working with 4kB)
#define SHM_LIMIT_IN_KB 4

typedef enum
{
	NONE, MSG, QUEUE, PIPE, SHM
} protocol_t;

typedef enum
{
	SEND_USAGE, RECEIVE_USAGE
} usage_t;

typedef struct
{
	uint16_t msg_type;
	size_t data_size;
} msg_data_t;

typedef union
{
	uint16_t msg_type;
	struct _pulse pulse;
	msg_data_t data;
} msg_buf_t;

typedef struct
{
	volatile unsigned is_init;
	volatile unsigned is_read;
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	size_t data_version;
	size_t data_size;
	size_t shared_mem_size;
	char data_ap[0]; // data access point
} shm_data_t;

int get_int(char *str_ptr);
long int find_file_size(int fd);
shm_data_t* get_shared_memory_pointer(char *shm_name, unsigned num_retries, int shm_size);
void error_handler(void *source);
void lock_mutex(pthread_mutex_t *mutex, char *shm_name);
void receive_msg(char *server_ptr, int fd);
void receive_pipe(char *pipe_ptr, int fd);
void receive_queue(char *queue_ptr, int fd);
void receive_shm(char *shm_name, char *shm_size, int fd);
void send_cond_broadcast(pthread_cond_t *cond, char *shm_name);
void send_msg(char *server_ptr, int fd);
void send_pipe(char *pipe_ptr, int fd);
void send_queue(char *queue_ptr, int fd);
void send_shm(char *shm_name, char *shm_size, int fd);
void shm_unlink_exit(char *shm_name);
void unlock_mutex(pthread_mutex_t *mutex, char *shm_name);
void usage(usage_t usage, int help);
void write_to_file(char *data, size_t data_size, int fd);

#endif

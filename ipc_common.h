#ifndef IPC_COMMON_H
#define IPC_COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/neutrino.h>
#include <sys/dispatch.h>
#include <sys/iofunc.h>

#define MAX_BUFFER_SIZE 51200
#define MSG_DATA_FSIZE_TYPE (_IO_MAX + 3)
#define MSG_DATA_TYPE (_IO_MAX + 2)

typedef enum {
	NONE, MSG, QUEUE, PIPE, SHM
} protocol_t;
typedef enum {
	SEND_USAGE, RECEIVE_USAGE
} usage_t;

typedef struct {
	uint16_t msg_type;
	size_t data_size;
} msg_data_t;

typedef union {
	uint16_t msg_type;
	struct _pulse pulse;
	msg_data_t data;
} msg_buf_t;

long int find_file_size(FILE *fd);
void receive_msg(char *server_ptr, FILE *fd);
void receive_pipe();
void receive_queue();
void receive_shm();
void send_msg(char *server_ptr, FILE *fd);
void send_pipe();
void send_queue();
void send_shm();
void usage(usage_t usage);
void write_to_file(char *data, size_t data_size, FILE *fd);

#endif

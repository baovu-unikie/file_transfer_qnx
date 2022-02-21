#include "ipc_common.h"

long int find_file_size(FILE *fd) {
	long int size = 0;
	// remember current position
	long int offset = ftell(fd);
	// set fd to the end of the file
	if (fseek(fd, 0L, SEEK_END) != 0) {
		perror("fseek");
		exit(EXIT_FAILURE);
	}
	// calculate size
	size = ftell(fd);
	// set back to the old position
	if (fseek(fd, offset, SEEK_SET) != 0) {
		perror("fseek");
		exit(EXIT_FAILURE);
	}
	return size;
}

void receive_msg(char *server_ptr, FILE *fd) {
	int rcvid = 0, status = 0;
	name_attach_t *attach;
	char *data;
	long int file_size = 0;
	long int receive_size = 0;
	msg_buf_t msg;

	if (!(attach = name_attach(NULL, server_ptr, 0))) {
		perror("name_attach");
		exit(EXIT_FAILURE);
	}
	printf("Start [%s]...\n", server_ptr);
	while (1) {
		rcvid = MsgReceive(attach->chid, &msg, sizeof(msg), NULL);
		if (rcvid == -1) {
			perror("MsgReceive");
			exit(EXIT_FAILURE);
		} else if (rcvid > 0) {
			switch (msg.msg_type) {
			case MSG_DATA_TYPE: // receive file content
				data = malloc(msg.data.data_size);
				if (data == NULL) {
					if (MsgError(rcvid, ENOMEM) == -1) {
						perror("MsgError");
						exit(EXIT_FAILURE);
					}
				} else {
					status = MsgRead(rcvid, data, msg.data.data_size,
							sizeof(msg_data_t));
					if (status == -1) {
						perror("MsgRead");
						exit(EXIT_FAILURE);
					}
					write_to_file(data, msg.data.data_size, fd);
					free(data);
					status = MsgReply(rcvid, EOK, NULL, 0);
					if (status == -1) {
						perror("MsgRead");
						exit(EXIT_FAILURE);
					}

				}
				break;
			case MSG_DATA_FSIZE_TYPE: // receive file size
				status = MsgRead(rcvid, &file_size, msg.data.data_size,
						sizeof(msg_data_t));
				if (status == -1) {
					perror("MsgRead");
					exit(EXIT_FAILURE);
				}
				status = MsgReply(rcvid, EOK, NULL, 0);
				if (status == -1) {
					perror("MsgRead");
					exit(EXIT_FAILURE);
				}
				break;

			default:
				if (MsgError(rcvid, ENOSYS) == -1)
					perror("MsgError");
				break;
			}
		} else if (rcvid == 0) {
			switch (msg.pulse.code) {
			case _PULSE_CODE_DISCONNECT: // check file size after disconnected
				receive_size = find_file_size(fd);
				if (file_size == receive_size) {
					printf("File received | File size = %ld byte(s)\n",
							receive_size);
					fclose(fd);
					exit(EXIT_SUCCESS);
				} else {
					printf("Connection lost. Interrupted transfer.\n");
					exit(EXIT_FAILURE);
				}
				break;
			default:
				printf("Unknown pulse received, code = %d\n", msg.pulse.code);
				break;
			}
		} else {
			printf("Unexpected rcvid: %d\n", rcvid);
		}

	}
}

void receive_pipe(char *pipe_ptr, FILE *fd) {
	int pd = 0;
	int read_count = 1;
	printf("Waiting for [%s] pipe...\n", pipe_ptr);
	while (pd == 0 || pd == -1) {
		pd = open(pipe_ptr, O_RDONLY);
	}
	while (read_count != 0) {
		char *data = malloc(PIPE_BUF);
		read_count = read(pd, data, PIPE_BUF);
		write_to_file(data, read_count, fd);
		free(data);
	}
	printf("File received | File size = %ld byte(s)\n", find_file_size(fd));
	close(pd);
	remove(pipe_ptr);
	fclose(fd);
}
void receive_queue() {
}
void receive_shm() {
}
void send_msg(char *server_ptr, FILE *fd) {
	long int file_size = find_file_size(fd);
	long int sent = 0;
	int coid = -1;
	int status = 0;
	iov_t siov[2];
	msg_data_t msg;

	// wait for server
	while (coid == -1) {
		coid = name_open(server_ptr, 0);
		printf("Waiting for [%s]...\n", server_ptr);
		sleep(1);
	}

	printf("Sending file to [%s] | File size = %ld byte(s)\n", server_ptr,
			file_size);
	// send file size
	msg.msg_type = MSG_DATA_FSIZE_TYPE;
	msg.data_size = sizeof(file_size);
	SETIOV(&siov[0], &msg, sizeof(msg));
	SETIOV(&siov[1], &file_size, msg.data_size);
	status = MsgSendvs(coid, siov, 2, NULL, 0);
	if (status == -1) {
		perror("MsgSendvs");
		exit(EXIT_FAILURE);
	}

	// send file content
	while (sent < file_size) {
		char *buf = malloc(MAX_BUFFER_SIZE);
		msg.msg_type = MSG_DATA_TYPE;
		long int read = fread(buf, 1, MAX_BUFFER_SIZE, fd);
		msg.data_size = read;
		SETIOV(&siov[0], &msg, sizeof(msg));
		SETIOV(&siov[1], buf, msg.data_size);
		status = MsgSendvs(coid, siov, 2, NULL, 0);
		if (status == -1) {
			perror("MsgSendvs");
			exit(EXIT_FAILURE);
		}
		sent += read;
		free(buf);
	}
	printf("File sent.\n");
	fclose(fd);

}
void send_pipe(char *pipe_ptr, FILE *fd) {
	int pd = 0;
	long int sent = 0;
	long int file_size = find_file_size(fd);

	// make sure old pipe is removed
	remove(pipe_ptr);
	// create a pipe
	if (mkfifo(pipe_ptr, 0777) > 0) {
		perror("mkfifo");
		exit(EXIT_FAILURE);
	}
	printf("Created a pipe named [%s] and waiting for client...\n", pipe_ptr);
	// open the pipe
	pd = open(pipe_ptr, O_RDWR);
	if (pd == -1) {
		perror("open");
		exit(EXIT_FAILURE);
	}

	while (sent < file_size) {
		char *buf = malloc(PIPE_BUF);
		// read data from the file
		long int read = fread(buf, 1, PIPE_BUF, fd);
		// write it to the pipe
		long int written = write(pd, buf, read);
		if (read == written)
			sent += read;
		else {
			perror("write");
			fclose(fd);
			free(buf);
			remove(pipe_ptr);
			exit(EXIT_FAILURE);
		}
		free(buf);
	}

	if (file_size == sent)
		printf("File sent | File size = %ld byte(s).\n", file_size);
	else {
		printf("Connection lost. Interrupted transfer. Removed [%s]\n",
				pipe_ptr);
		remove(pipe_ptr);
		fclose(fd);
		exit(EXIT_FAILURE);
	}
	fclose(fd);
}

void send_queue() {
}
void send_shm() {
}
void usage(usage_t usage) {
	char cmd[20];
	char description[65];
	char example[50];
	if (usage == SEND_USAGE) {
		strcpy(cmd, "ipc_sendfile");
		strcpy(description, "ipc_sendfile sends file to the given server.");
		strcpy(example, "ipc_sendfile -f file_to_send -m server_name");
	}
	if (usage == RECEIVE_USAGE) {
		strcpy(cmd, "ipc_receivefile");
		strcpy(description,
				"ipc_receivefile (server) listens to clients and receives files");
		strcpy(example, "ipc_receivefile -f file_to_save -m server_name");
	}

	printf("\e[1mSYNOPSIS\e[0m\n");
	printf("  %s -h | --help\n", cmd);
	printf("  %s [PROTOCOL] --file [FILE_PATH] \n", cmd);
	printf("  %s [PROTOCOL] --f [FILE_PATH] \n", cmd);
	printf("\e[1mDESCRIPTION\e[0m\n");
	printf("  %s\n", description);
	printf("    Available [PROTOCOL]\n");
	printf("      -m | --message [SERVER_NAME]. Example: -p myserver\n");
	printf("      -q | --queue []\n");
	printf("      -p | --pipe [NAMED_PIPE]. Example: -p /tmp/myfifo \n");
	printf("      -s | --shm []\n");
	printf("\e[1mEXAMPLE\e[0m\n");
	printf("  %s\n", example);
}

void write_to_file(char *data, size_t data_size, FILE *fd) {
	size_t status;
	status = fwrite(data, 1, data_size, fd);
	if (status != data_size) {
		perror("fwrite");
		fclose(fd);
		exit(EXIT_FAILURE);
	}
}

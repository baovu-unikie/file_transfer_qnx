#include "ipc_common.h"

// check if the string only contains number
// convert it to an integer
int get_int(char *str_ptr)
{
	char *ptr = str_ptr;
	while (*ptr != '\0')
	{
		if (*ptr < 48 || *ptr > 57)
		{
			printf("Please give a valid size of shared memory. Integer only.\n");
			usage(RECEIVE_USAGE, 0);
		}
		ptr++;
	}
	int size = atoi(str_ptr);
	if (size <= 0)
	{
		printf("Please give a valid size of shared memory (> 0).\n");
		usage(RECEIVE_USAGE, 0);
	}
	return size;
}

long int find_file_size(int fd)
{
	struct stat64 stat;
	fstat64(fd, &stat);
	return (long int) stat.st_size;
}

shm_data_t* get_shared_memory_pointer(char *shm_name, unsigned num_retries, int shm_size)
{
	unsigned tries;
	shm_data_t *ptr;
	int fd;

	printf("Trying to open the shared memory named %s\n", shm_name);
	for (tries = 0;;)
	{
		fd = shm_open(shm_name, O_RDWR, 0660);
		if (fd != -1)
			break;
		++tries;
		printf("Tried %ld time(s)\n", tries);
		if (tries > num_retries)
		{
			perror("shmn_open");
			return MAP_FAILED;
		}
		sleep(1);
	}

	printf("Trying to map the shared memory object named [/dev/shmem%s].\n", shm_name);
	for (tries = 0;;)
	{
		ptr = (shm_data_t*)mmap64(NULL, shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
		if (ptr != MAP_FAILED)
			break;
		++tries;
		printf("Tried %ld time(s)\n", tries);
		if (tries > num_retries)
		{
			perror("mmap64");
			return MAP_FAILED;
		}
		sleep(1);
	}

	(void) close(fd);

	printf("Waiting for the shared object to be initialized.\n", shm_name);
	for (tries = 0;;)
	{
		if (ptr->is_init)
			break;
		++tries;
		printf("Check initialize status %ld time(s)\n", tries);
		if (tries > num_retries)
		{
			fprintf(stderr, "Shared memory is not initialized\n");
			(void) munmap(ptr, shm_size);
			return MAP_FAILED;
		}
		sleep(1);
	}

	return ptr;
}

void lock_mutex(pthread_mutex_t *mutex, char *shm_name)
{
	if (pthread_mutex_lock(mutex) != EOK)
	{
		perror("pthread_mutex_lock");
		shm_unlink_exit(shm_name);
	}
}

void receive_msg(char *server_ptr, int fd)
{
	int rcvid = 0, status = 0;
	name_attach_t *attach;
	char *data;
	long int file_size = 0;
	long int receive_size = 0;
	msg_buf_t msg;

	if (!(attach = name_attach(NULL, server_ptr, 0)))
	{
		perror("name_attach");
		exit(EXIT_FAILURE);
	}

	printf("Start [%s]...\n", server_ptr);
	while (1)
	{
		rcvid = MsgReceive(attach->chid, &msg, sizeof(msg), NULL);
		if (rcvid == -1)
		{
			perror("MsgReceive");
			exit(EXIT_FAILURE);
		}
		else if (rcvid > 0)
		{
			switch (msg.msg_type)
			{
			case MSG_DATA_TYPE: // receive file content
				data = malloc(msg.data.data_size);
				if (data == NULL)
				{
					if (MsgError(rcvid, ENOMEM) == -1)
					{
						perror("MsgError");
						exit(EXIT_FAILURE);
					}
				}
				else
				{
					status = MsgRead(rcvid, data, msg.data.data_size, sizeof(msg_data_t));
					if (status == -1)
					{
						perror("MsgRead");
						exit(EXIT_FAILURE);
					}
					write_to_file(data, msg.data.data_size, fd);
					free(data);
					status = MsgReply(rcvid, EOK, NULL, 0);
					if (status == -1)
					{
						perror("MsgRead");
						exit(EXIT_FAILURE);
					}

				}
				break;
			case MSG_DATA_FSIZE_TYPE: // receive file size
				status = MsgRead(rcvid, &file_size, msg.data.data_size, sizeof(msg_data_t));
				if (status == -1)
				{
					perror("MsgRead");
					exit(EXIT_FAILURE);
				}
				status = MsgReply(rcvid, EOK, NULL, 0);
				if (status == -1)
				{
					perror("MsgRead");
					exit(EXIT_FAILURE);
				}
				break;

			default:
				if (MsgError(rcvid, ENOSYS) == -1)
					perror("MsgError");
				break;
			}
		}
		else if (rcvid == 0)
		{
			switch (msg.pulse.code)
			{
			case _PULSE_CODE_DISCONNECT: // check file size after disconnected
				receive_size = find_file_size(fd);
				if (file_size == receive_size)
				{
					printf("File received | File size = %ld byte(s)\n", receive_size);
					close(fd);
					exit(EXIT_SUCCESS);
				}
				else
				{
					printf("Connection lost. Interrupted transfer.\n");
					exit(EXIT_FAILURE);
				}
				break;
			default:
				printf("Unknown pulse received, code = %d\n", msg.pulse.code);
				break;
			}
		}
		else
		{
			printf("Unexpected rcvid: %d\n", rcvid);
		}

	}
}

void receive_pipe(char *pipe_ptr, int fd)
{
	int pd = 0;
	int read_size = 1;
	char *data = malloc(PIPE_BUF);
	printf("Waiting for [%s] pipe...\n", pipe_ptr);
	while (pd == 0 || pd == -1)
	{
		pd = open(pipe_ptr, O_RDONLY);
	}
	while (read_size != 0)
	{
		if (data != NULL)
		{
			read_size = read(pd, data, PIPE_BUF);
			if (read_size > -1)
				write_to_file(data, read_size, fd);
			else
			{
				perror("read");
				free(data);
				exit(EXIT_FAILURE);
			}
		}
		else
		{
			perror("malloc");
			free(data);
			exit(EXIT_FAILURE);
		}
	}
	free(data);
	printf("File received | File size = %ld byte(s)\n", find_file_size(fd));
	close(pd);
	remove(pipe_ptr);
	close(fd);
}

void receive_queue(char *queue_ptr, int fd)
{
	if (*queue_ptr != '/')
	{
		printf("ERROR: the queue name should start with a leading '/' character\n");
		usage(RECEIVE_USAGE, 0);
	}
	mqd_t md = -1;
	int read_size = 0;
	int error_code = 0;
	struct mq_attr new_attr;

	printf("Waiting for [/dev/mqueue%s] queue...\n", queue_ptr);
	while (md == -1)
	{
		md = mq_open(queue_ptr, O_RDONLY, 0660, NULL);
	}

	sleep(1); // waiting for ipc_sendfile to send msg to the queue

	// add O_NONBLOCK attribute to allow mq_receive return immediately with
	// an error code (EAGAIN) instead of blocking forever
	mq_getattr(md, &new_attr);
	new_attr.mq_flags = O_RDONLY | O_NONBLOCK;
	mq_setattr(md, &new_attr, NULL);

	char *data = malloc(MQ_MSGSIZE);
	while (error_code != EAGAIN)
	{
		if (data != NULL)
		{
			read_size = mq_receive(md, data, MQ_MSGSIZE, NULL);
			error_code = errno;
			if (read_size > 0)
				write_to_file(data, read_size, fd);
		}
		else
		{
			perror("malloc");
			free(data);
			mq_close(md);
			mq_unlink(queue_ptr);
			exit(EXIT_FAILURE);
		}
	}
	printf("File received | File size = %ld byte(s)\n", find_file_size(fd));
	free(data);
	mq_close(md);
	mq_unlink(queue_ptr);
	close(fd);
}

void receive_shm(char *shm_name, char *shm_size, int fd)
{
	int ret = 0;
	int is_end = 0;
	int num_of_tries = 10;
	size_t last_version = 0;
	size_t shared_mem_size = get_int(shm_size) * 1024;
	shm_data_t *shm_ptr;

	if (shared_mem_size < 0 || get_int(shm_size) > SHM_LIMIT_IN_KB)
	{
		printf("ERROR: Invalid shared memory size\n");
		usage(RECEIVE_USAGE, 0);
	}

	if (*shm_name != '/')
	{
		printf("ERROR: the shared memory name should start with a leading '/' character\n");
		usage(RECEIVE_USAGE, 0);
	}

	shm_ptr = get_shared_memory_pointer(shm_name, num_of_tries, shared_mem_size);
	if (shm_ptr == MAP_FAILED)
	{
		fprintf(stderr, "Unable to access shared object /dev/shmem%s\n", shm_name);
		shm_unlink_exit(shm_name);
	}
	printf("Get the shared memory pointer [%p].\n", shm_ptr);
	printf("Waiting for data on [/dev/shmem%s].\n", shm_name);

	while (is_end != 1)
	{
		lock_mutex(&shm_ptr->mutex, shm_name);
		while (last_version == shm_ptr->data_version)
		{
			ret = pthread_cond_wait(&shm_ptr->cond, &shm_ptr->mutex);
			if (ret != EOK)
			{
				perror("pthread_cond_wait");
				shm_unlink_exit(shm_name);
			}
		}
		if(shm_ptr->shared_mem_size != shared_mem_size)
		{
			printf("Invalid shared memory size. Should be equal %ld byte(s)\n", shm_ptr->shared_mem_size );
			shm_unlink_exit(shm_name);
		}

		if (shm_ptr->data_size != 0)
			write_to_file(&shm_ptr->data_ap[0], shm_ptr->data_size, fd);
		else
			is_end = 1;
		shm_ptr->is_read = 1;
		last_version = shm_ptr->data_version;
		unlock_mutex(&shm_ptr->mutex, shm_name);
	}
	printf("File received | File size = %ld byte(s)\n", find_file_size(fd));
	close(fd);
	shm_unlink(shm_name);
}

void send_cond_broadcast(pthread_cond_t *cond, char *shm_name)
{
	if (pthread_cond_broadcast(cond) != EOK)
	{
		perror("pthread_cond_broadcast");
		shm_unlink_exit(shm_name);
	}
}

void send_msg(char *server_ptr, int fd)
{
	long int file_size = find_file_size(fd);
	long int sent = 0;
	int coid = -1;
	int status = 0;
	iov_t siov[2];
	msg_data_t msg;

	// wait for server
	printf("Waiting for [%s]...\n", server_ptr);
	while (coid == -1)
	{
		coid = name_open(server_ptr, 0);
		sleep(1);
	}

	printf("Sending file to [%s] | File size = %ld byte(s)\n", server_ptr, file_size);
	// send file size
	msg.msg_type = MSG_DATA_FSIZE_TYPE;
	msg.data_size = sizeof(file_size);
	SETIOV(&siov[0], &msg, sizeof(msg));
	SETIOV(&siov[1], &file_size, msg.data_size);
	status = MsgSendvs(coid, siov, 2, NULL, 0);
	if (status == -1)
	{
		perror("MsgSendvs");
		exit(EXIT_FAILURE);
	}

	// send file content
	char *buf = malloc(MSG_BUFFER_SIZE);
	while (sent < file_size)
	{
		if (buf != NULL)
		{
			msg.msg_type = MSG_DATA_TYPE;
			long int read_size = read(fd, buf, MSG_BUFFER_SIZE);
			msg.data_size = read_size;
			SETIOV(&siov[0], &msg, sizeof(msg));
			SETIOV(&siov[1], buf, msg.data_size);
			status = MsgSendvs(coid, siov, 2, NULL, 0);
			if (status == -1)
			{
				perror("MsgSendvs");
				exit(EXIT_FAILURE);
			}
			sent += read_size;
		}
		else
		{
			perror("malloc");
			free(buf);
			exit(EXIT_FAILURE);
		}
	}
	free(buf);
	printf("File sent.\n");
	close(fd);

}

void send_pipe(char *pipe_ptr, int fd)
{
	int pd = 0;
	long int sent = 0;
	long int file_size = find_file_size(fd);

	// make sure old pipe is removed
	remove(pipe_ptr);
	// create a pipe
	if (mkfifo(pipe_ptr, 0660) > 0)
	{
		perror("mkfifo");
		exit(EXIT_FAILURE);
	}
	printf("Created a pipe named [%s] and waiting for client...\n", pipe_ptr);
	// open the pipe
	pd = open(pipe_ptr, O_RDWR);
	if (pd == -1)
	{
		perror("open");
		exit(EXIT_FAILURE);
	}

	char *buf = malloc(PIPE_BUF);
	while (sent < file_size)
	{
		if (buf != NULL)
		{
			// read data from the file
			long int read_size = read(fd, buf, PIPE_BUF);
			// write it to the pipe
			long int written = write(pd, buf, read_size);
			if (read_size == written)
				sent += read_size;
			else
			{
				perror("write");
				close(fd);
				free(buf);
				remove(pipe_ptr);
				exit(EXIT_FAILURE);
			}
		}
		else
		{
			perror("malloc");
			free(buf);
			exit(EXIT_FAILURE);
		}
	}
	free(buf);
	if (file_size == sent)
		printf("File sent | File size = %ld byte(s).\n", file_size);
	else
	{
		printf("Connection lost. Interrupted transfer. Removed [%s]\n", pipe_ptr);
		remove(pipe_ptr);
		close(fd);
		exit(EXIT_FAILURE);
	}
	close(fd);
}

void send_queue(char *queue_ptr, int fd)
{
	if (*queue_ptr != '/')
	{
		printf("ERROR: the queue name should start with a leading '/' character\n");
		usage(SEND_USAGE, 0);
	}
	mqd_t md;
	long int file_size = find_file_size(fd);
	long int sent = 0;
	// remove old queue
	mq_unlink(queue_ptr);
	// open a message queue using default attributes
	md = mq_open(queue_ptr, O_CREAT | O_EXCL | O_WRONLY, 0660, NULL);
	if (md == -1)
	{
		perror("mq_open");
		exit(EXIT_FAILURE);
	}
	printf("Created a queue named [/dev/mqueue%s]\n", queue_ptr);

	char *buf = malloc(MQ_MSGSIZE);
	while (sent < file_size)
	{
		if (buf != NULL)
		{
			long int read_size = read(fd, buf, MQ_MSGSIZE);
			int error_check = mq_send(md, buf, read_size, MQ_PRIO_MAX - 1);
			if (error_check == -1)
			{
				perror("mq_send");
				free(buf);
				exit(EXIT_FAILURE);
			}
			sent += read_size;
		}
		else
		{
			perror("malloc");
			free(buf);
			exit(EXIT_FAILURE);
		}
	}
	free(buf);

	if (file_size == sent)
	{
		printf("File is sent to the queue.\n");
		printf("Waiting for client to pick up...\n");
		int check_empty = -1;
		struct mq_attr attr;
		// check if the queue is empty
		while (check_empty != 0)
		{
			mq_getattr(md, &attr);
			check_empty = attr.mq_curmsgs;
		}
		printf("Client picked it up | File size = %ld byte(s).\n", file_size);
	}
	else
	{
		printf("Connection lost. Interrupted transfer. Removed [%s]\n", queue_ptr);
		mq_close(md);
		mq_unlink(queue_ptr);
		close(fd);
		exit(EXIT_FAILURE);
	}
	mq_close(md);
	close(fd);
}

void send_shm(char *shm_name, char *shm_size, int fd)
{
	int shmd = 0;
	int ret = 0;
	int is_end = 0;
	long int sent = 0;
	shm_data_t *shm_ptr;
	pthread_mutexattr_t mutex_attr;
	pthread_condattr_t cond_attr;
	size_t file_size = find_file_size(fd);
	size_t shared_mem_size = get_int(shm_size) * 1024;
	size_t data_size = shared_mem_size - sizeof(shm_data_t);

	if (shared_mem_size < 0 || get_int(shm_size) > SHM_LIMIT_IN_KB)
	{
		printf("ERROR: Invalid shared memory size\n");
		usage(SEND_USAGE, 0);
	}

	if (*shm_name != '/')
	{
		printf("ERROR: the shared memory name should start with a leading '/' character\n");
		usage(SEND_USAGE, 0);
	}

	// remove old shm if any
	shm_unlink(shm_name);
	shmd = shm_open(shm_name, O_RDWR | O_CREAT | O_EXCL, 0660);
	if (shmd == -1)
	{
		perror("shm_open");
		shm_unlink_exit(shm_name);
	}
	printf("Created a shared memory object named [/dev/shmem%s].\n", shm_name);

	ret = ftruncate64(shmd, shared_mem_size);
	if (ret == -1)
	{
		perror("ftruncate");
		shm_unlink_exit(shm_name);
	}
	printf("Set size of the shared memory object to %d byte(s).\n", shared_mem_size);

	shm_ptr = (shm_data_t*)mmap64(NULL, shared_mem_size, PROT_READ | PROT_WRITE, MAP_SHARED, shmd, 0);
	if (shm_ptr == MAP_FAILED)
	{
		perror("mmap64");
		shm_unlink_exit(shm_name);
	}
	printf("Get the shared memory pointer [%p].\n", shm_ptr);

	close(shmd);

	//set up the mutex
	pthread_mutexattr_init(&mutex_attr);
	pthread_mutexattr_setpshared(&mutex_attr, PTHREAD_PROCESS_SHARED);
	ret = pthread_mutex_init(&shm_ptr->mutex, &mutex_attr);
	if (ret != EOK)
	{
		perror("pthread_mutex_init");
		shm_unlink_exit(shm_name);
	}

	// set up the condvar
	pthread_condattr_init(&cond_attr);
	pthread_condattr_setpshared(&cond_attr, PTHREAD_PROCESS_SHARED);
	ret = pthread_cond_init(&shm_ptr->cond, &cond_attr);
	if (ret != EOK)
	{
		perror("pthread_cond_init");
		shm_unlink_exit(shm_name);
	}

	// set flags
	shm_ptr->is_init = 1; // is initialized
	shm_ptr->is_read = 1; // to start the sending process
	shm_ptr->shared_mem_size = shared_mem_size;

	while (is_end != -1)
	{
		lock_mutex(&shm_ptr->mutex, shm_name);
		if (shm_ptr->is_read == 1)
		{
			if (sent < file_size)
			{
				long int read_size = read(fd, &shm_ptr->data_ap[0], data_size);
				if(read_size != -1)
				{
					shm_ptr->is_read = 0;
					shm_ptr->data_version++;
					shm_ptr->data_size = read_size;
					sent += read_size;
				}
				else
				{
					perror("read");
					close(fd);
					shm_unlink_exit(shm_name);
				}
			}
			else if (sent == file_size && shm_ptr->data_size != 0)
			{
				shm_ptr->is_read = 0;
				shm_ptr->data_size = 0;
				shm_ptr->data_version++;
				is_end = -1;
			}
		}
		unlock_mutex(&shm_ptr->mutex, shm_name);
		send_cond_broadcast(&shm_ptr->cond, shm_name);
	}
	if (file_size == sent)
	{
		printf("File sent | File size = %ld byte(s).\n", file_size);
	}
	else
	{
		printf("Connection lost. Interrupted transfer. Removed [%s]\n", shm_name);
		close(fd);
		shm_unlink_exit(shm_name);
	}

}

void shm_unlink_exit(char *shm_name)
{
	(void) shm_unlink(shm_name);
	exit(EXIT_FAILURE);
}

void unlock_mutex(pthread_mutex_t *mutex, char *shm_name)
{
	if (pthread_mutex_unlock(mutex) != EOK)
	{
		perror("pthread_mutex_unlock");
		shm_unlink_exit(shm_name);
	}
}

void usage(usage_t usage, int help)
{
	char cmd[20];
	char description[65];
	char example1[60], example2[60], example3[60], example4[60];
	if (usage == SEND_USAGE)
	{
		strcpy(cmd, "ipc_sendfile");
		strcpy(description, "ipc_sendfile sends file to ipc_receivefile using a specified protocol.");
		strcpy(example1, "ipc_sendfile -f file_to_send -m server_name");
		strcpy(example2, "ipc_sendfile -f file_to_send -q /queue_name");
		strcpy(example3, "ipc_sendfile -f file_to_send -p pipe_name");
		strcpy(example4, "ipc_sendfile -f file_to_send -s /shm_name 4");
	}
	if (usage == RECEIVE_USAGE)
	{
		strcpy(cmd, "ipc_receivefile");
		strcpy(description, "ipc_receivefile receives file from ipc_sendfile using a specified protocol");
		strcpy(example1, "ipc_receivefile -f saved_file_name -m server_name");
		strcpy(example2, "ipc_receivefile -f saved_file_name -q /queue_name");
		strcpy(example3, "ipc_receivefile -f saved_file_name -p pipe_name");
		strcpy(example4, "ipc_receivefile -f saved_file_name -s /shm_name 4");
	}

	printf("\e[1mSYNOPSIS\e[0m\n");
	printf("  %s -h | --help\n", cmd);
	printf("  %s [PROTOCOL] --file [FILE_PATH] \n", cmd);
	printf("  %s [PROTOCOL] -f [FILE_PATH] \n", cmd);
	printf("\e[1mDESCRIPTION\e[0m\n");
	printf("  %s\n", description);
	printf("  Available [PROTOCOL]\n");
	printf("    -m | --message [SERVER_NAME].\n");
	printf("    -q | --queue [QUEUE_NAME].\n");
	printf("                 [QUEUE_NAME] should be start with '/' character.\n");
	printf("    -p | --pipe [PIPE_NAME]\n");
	printf("    -s | --shm [SHARED_MEM_NAME] [SIZE_OF_SHM_IN_KB]\n");
	printf("               [SHARED_MEM_NAME] should be start with '/' character.\n");
	printf("               0 < [SIZE_OF_SHM_IN_KB] ≤ %d\n", SHM_LIMIT_IN_KB);
	printf("\e[1mEXAMPLES\e[0m\n");
	printf("  %s\n", example1);
	printf("  %s\n", example2);
	printf("  %s\n", example3);
	printf("  %s\n", example4);

	if (help != 1)
		exit(EXIT_FAILURE);
}

void write_to_file(char *data, size_t data_size, int fd)
{
	size_t status;
	status = write(fd, data, data_size);
	if (status != data_size)
	{
		perror("write");
		close(fd);
		exit(EXIT_FAILURE);
	}
}

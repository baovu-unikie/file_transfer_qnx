#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ipc_common.h"

int main(int argc, char *argv[])
{
	protocol_t protocol = NONE;
	unsigned int name_index = 0;
	unsigned int size_index = 0;
	int fd = 0;

	if (argc != 5 && argc != 2 && argc != 6)
		usage(RECEIVE_USAGE, 0);

	for (int i = 1; i < argc; i++)
	{
		// options without arguments
		if ((!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h"))
				&& protocol == NONE)
			usage(RECEIVE_USAGE, 1);
		else if (i + 1 == argc)
			usage(RECEIVE_USAGE, 0);
		// options with one argument
		else if ((!strcmp(argv[i], "--message") || !strcmp(argv[i], "-m"))
				&& protocol == NONE)
		{
			protocol = MSG;
			name_index = ++i;
		}
		else if ((!strcmp(argv[i], "--queue") || !strcmp(argv[i], "-q"))
				&& protocol == NONE)
		{
			protocol = QUEUE;
			name_index = ++i;
		}
		else if ((!strcmp(argv[i], "--pipe") || !strcmp(argv[i], "-p"))
				&& protocol == NONE)
		{
			protocol = PIPE;
			name_index = ++i;
		}
		else if (!strcmp(argv[i], "--file") || !strcmp(argv[i], "-f"))
		{
			remove(argv[i+1]);
			fd = open64(argv[++i], O_RDWR | O_CREAT | O_EXCL , 0660);
			if (fd == -1)
			{
				perror("open64");
				return EXIT_FAILURE;
			}
		}
		// options with two arguments
		else if (i + 2 == argc)
			usage(RECEIVE_USAGE, 0);
		else if ((!strcmp(argv[i], "--shm") || !strcmp(argv[i], "-s"))
				&& protocol == NONE)
		{
			protocol = SHM;
			name_index = ++i;
			size_index = ++i;
		}
		else
			usage(RECEIVE_USAGE, 0);
	}

	if (protocol == MSG)
		receive_msg(argv[name_index], fd);
	if (protocol == QUEUE)
		receive_queue(argv[name_index], fd);
	if (protocol == PIPE)
		receive_pipe(argv[name_index], fd);
	if (protocol == SHM)
		receive_shm(argv[name_index], argv[size_index], fd);

	return EXIT_SUCCESS;
}


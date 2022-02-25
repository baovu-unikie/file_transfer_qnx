#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ipc_common.h"

int main(int argc, char *argv[])
{
	protocol_t protocol = NONE;
	unsigned int name_index = 0;
	unsigned int size_index = 0;
	int fd;

	if (argc != 5 && argc != 2 && argc != 6)
		usage(SEND_USAGE, 0);

	for (int i = 1; i < argc; i++)
	{
		// options without arguments
		if ((!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h"))
				&& protocol == NONE)
			usage(SEND_USAGE, 1);
		else if (i + 1 == argc)
			usage(SEND_USAGE, 0);
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
			fd = open64(argv[++i], O_RDONLY, 0660);
			if (fd == -1)
			{
				perror("open");
				return EXIT_FAILURE;
			}
		}
		// options with two arguments
		else if (i + 2 == argc)
			usage(SEND_USAGE, 0);
		else if ((!strcmp(argv[i], "--shm") || !strcmp(argv[i], "-s"))
				&& protocol == NONE)
		{
			protocol = SHM;
			name_index = ++i;
			size_index = ++i;
		}
		else
			usage(SEND_USAGE, 0);
	}

	if (protocol == MSG)
		send_msg(argv[name_index], fd);
	if (protocol == QUEUE)
		send_queue(argv[name_index], fd);
	if (protocol == PIPE)
		send_pipe(argv[name_index], fd);
	if (protocol == SHM)
		send_shm(argv[name_index], argv[size_index], fd);

	return EXIT_SUCCESS;
}


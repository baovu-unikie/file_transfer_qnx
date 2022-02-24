#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ipc_common.h"

int main(int argc, char *argv[]) {
	protocol_t protocol = NONE;
	unsigned int protocol_arg_index = 0;
	FILE *fp;

	if (argc != 5 && argc != 2)
		usage(RECEIVE_USAGE, 0);

	for (int i = 1; i < argc; i++) {
		// options without arguments
		if ((!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h"))
				&& protocol == NONE)
			usage(RECEIVE_USAGE, 1);
		else if (i + 1 == argc)
			usage(RECEIVE_USAGE, 0);
		// options with arguments
		else if ((!strcmp(argv[i], "--message") || !strcmp(argv[i], "-m"))
				&& protocol == NONE) {
			protocol = MSG;
			protocol_arg_index = ++i;
		} else if ((!strcmp(argv[i], "--queue") || !strcmp(argv[i], "-q"))
				&& protocol == NONE) {
			protocol = QUEUE;
			protocol_arg_index = ++i;
		} else if ((!strcmp(argv[i], "--pipe") || !strcmp(argv[i], "-p"))
				&& protocol == NONE) {
			protocol = PIPE;
			protocol_arg_index = ++i;
		} else if ((!strcmp(argv[i], "--shm") || !strcmp(argv[i], "-s"))
				&& protocol == NONE) {
			protocol = SHM;
			protocol_arg_index = ++i;
		} else if (!strcmp(argv[i], "--file") || !strcmp(argv[i], "-f")) {
			fp = fopen64(argv[++i], "wb");
			if (fp == NULL) {
				perror("fopen");
				return EXIT_FAILURE;
			}
		} else
			usage(RECEIVE_USAGE, 0);
	}

	if (protocol == MSG)
		receive_msg(argv[protocol_arg_index], fp);
	if (protocol == QUEUE)
		receive_queue(argv[protocol_arg_index], fp);
	if (protocol == PIPE)
		receive_pipe(argv[protocol_arg_index], fp);
	if (protocol == SHM)
		receive_shm();

	return EXIT_SUCCESS;
}


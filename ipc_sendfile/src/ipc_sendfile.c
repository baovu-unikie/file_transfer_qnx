#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(void)
{
	printf("Usage: \n");
	printf("Specify the protocol: \n");
	printf("\tipc_sendfile --[protocol] --file [file-to-send]\n");
	printf("Using the default protocol (message) \n");
	printf("\tipc_sendfile --file [file-to-send]\n");
	printf("Make sure you gave the correct path to the file\n");
	exit(1);
}

static enum protocols{MSG, QUEUE, PIPE, SHM};

int main(int argc, char* argv[]) {
	
	enum protocols protocol = MSG;
	int set = 0;
	FILE *fp;
	if ((argc != 4 && argc != 3) || (argc == 2 && strcmp(argv[1],"--help") == 0))
	{
		usage();
	}
	
	for(int i = 1; i < argc; i++)
	{
			// options without args
		if(!strcmp(argv[i], "--message") && set == 0) 
		{
			protocol = MSG;
			set = 1;
		}
		else if(!strcmp(argv[i], "--queue") && set == 0) 
		{
			protocol = QUEUE;
			set = 1;
		}
		else if(!strcmp(argv[i], "--pipe") && set == 0) 
		{
			protocol = PIPE;
			set = 1;
		}
		else if(!strcmp(argv[i], "--shm") && set == 0) 
		{
			protocol = SHM;
			set = 1;
		}
		else if (i + 1 == argc)
			usage();
		// options with args
		else if(!strcmp(argv[i], "--file")) 
		{
			fp = fopen(argv[++i], "a");
			if (fp == NULL)
				usage();
		}
		else 
			usage();
}
	return EXIT_SUCCESS;
}



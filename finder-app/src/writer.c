#include <stdio.h>
#include <syslog.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

int main(int argc, char** argv) {
	openlog(NULL, 0, LOG_USER);	

	if (argc < 3) {
		printf("2 arguments required\n\r");
		
		syslog(LOG_ERR, "Call with less than 2 arguments");	
		closelog();
		
		return 1;
	}

	int fd = open(argv[1], O_WRONLY | O_CREAT | O_TRUNC | O_SYNC, S_IRWXU | S_IRGRP | S_IROTH);
	
	if (fd == -1) {	
		printf("Writer can't open the file\n\r");
		
		syslog(LOG_ERR, "Error on open file %s", argv[1]);
		closelog();

		return 1;
	}
	
	syslog(LOG_DEBUG, "Writing %s to %s", argv[2], argv[1]);	
	int n = write(fd, argv[2], strlen(argv[2]));

	if (n == -1) {
		printf("Some problem happen on write the file\n\r");
		
		syslog(LOG_ERR, "Error on write on file %s", argv[1]);
		closelog();
	
		close(fd);

		return 1;
	}

	close(fd);

	return 0;
}

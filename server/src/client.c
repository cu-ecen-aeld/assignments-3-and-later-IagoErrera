#include <syslog.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>

//#define ADDRESS "127.0.0.1"
#define ADDRESS "localhost"
#define PORT "9000"
#define CONNECTION_QUEUE_SIZE 10
#define BUFFER_SIZE 256

#define WRITE_FILE "/var/tmp/aesdsocketdata"

int sockfd = -1, connection_fd = -1, file_fd = -1;

static void signal_handler(int signal_number) {
	if (sockfd != -1) close(sockfd);
	if (connection_fd != -1) close(connection_fd);
	if (file_fd != -1) close(file_fd);
	
	syslog(LOG_INFO, "Caught sinal, exiting");
	closelog();
}

int main() {
	openlog(NULL, 0, LOG_USER);
	struct sigaction new_action;
	memset(&new_action, 0, sizeof(struct sigaction));
	new_action.sa_handler = signal_handler;

	if (sigaction(SIGINT, &new_action, NULL) != 0) {
		perror("Error on register signal handler to SIGINT");
		syslog(LOG_ERR, "Error on register signal handler SIGINT");
		closelog();
		exit(1);
	}
	
	if (sigaction(SIGTERM, &new_action, NULL) != 0) {
		perror("Error on register signal handler to SIGTERM");
		syslog(LOG_ERR, "Error on register signal handler to SIGTERM");
		closelog();
		exit(1);
	}

	struct addrinfo hints, *serverinfo, *p;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	getaddrinfo(ADDRESS, PORT, &hints, &serverinfo);	
	
	for (p = serverinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
				perror("Error on socket creation!");
				syslog(LOG_ERR, "Error on socket creation!");
				continue;
		}

		if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
				perror("Error on connect!");
				syslog(LOG_ERR, "Error on connect!");
				continue;
		}

		break;
	}

	char s[INET6_ADDRSTRLEN];
	inet_ntop(p->ai_family, &((struct sockaddr_in*)((struct sockaddr*)p))->sin_addr, s, sizeof s);
	
	printf("client: connecting to %s\n", s);
	
	printf("Sending\n\r");	
	char* s_buf = "abcdefg\n";
	send(sockfd, s_buf, 8, 0);
	
	printf("Receiving\n\r");
	char r_buf[BUFFER_SIZE];
	read(sockfd, &r_buf, BUFFER_SIZE);

	printf("Received: %s\n\r", r_buf);

	freeaddrinfo(serverinfo);
	close(sockfd);
	closelog();
	return 0;
}

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

#define PORT "9000"
#define CONNECTION_QUEUE_SIZE 10
#define BUFFER_SIZE 256

#define WRITE_FILE "/var/tmp/aesdsocketdata"

int sockfd = -1, connection_fd = -1, file_fd = -1;
struct addrinfo hints, *res = NULL;
int should_free_res = 0;

static void signal_handler(int signal_number) {
	if (sockfd != -1) close(sockfd);
	if (connection_fd != -1) close(connection_fd);
	if (file_fd != -1) close(file_fd);
	if (should_free_res == 1) freeaddrinfo(res);
	
	remove(WRITE_FILE);

	syslog(LOG_INFO, "Caught sinal, exiting");
	closelog();

	exit(0);
}

int main(int argc, char** argv) {
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

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	getaddrinfo(NULL, PORT, &hints, &res);
	should_free_res = 1;	

	if ((sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) == -1) {
		perror("Error on socket creation!");
		syslog(LOG_ERR, "Error on socket creation!");
		closelog();
		freeaddrinfo(res);
		exit(1);
	}

	int yes = 1;
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1) {
		perror("Error on set socket options!");
		syslog(LOG_ERR, "Error on set socket options!");
		close(sockfd);
		closelog();
		freeaddrinfo(res);
		exit(1);
	}
 
	if (bind(sockfd, res->ai_addr, res->ai_addrlen) == -1) {
		perror("Error on bind socket!");
		syslog(LOG_ERR, "Error on bind socket!");
		close(sockfd);
		closelog();
		freeaddrinfo(res);
		exit(1);
	}

	if (listen(sockfd, CONNECTION_QUEUE_SIZE) == -1) {
		perror("Error on set socket to listen!");
		syslog(LOG_ERR, "Error on set socket to listen!");
		close(sockfd);
		closelog();
		freeaddrinfo(res);
		exit(1);
	}
	
	freeaddrinfo(res);
	should_free_res = 0;	

	printf("Listening on port %s \n\r", PORT);
	
	// Daemonization
	if (argc > 1 && strcmp(argv[1], "-d") == 0) {
		printf("Setting up daemon!");			
	
		pid_t pid = fork();
		if (pid == -1) {
			perror("Error on set daemon!");
			syslog(LOG_ERR, "Error on set daemon!");
			close(sockfd);
			closelog();
			exit(1);
		}

		if (pid != 0) {
			close(sockfd);
			closelog();
			exit(0);
		}
			
		if (setsid() == -1) {
			perror("Error on set daemon session!");
			syslog(LOG_ERR, "Error on set daemon session!");
			close(sockfd);
			closelog();
			exit(1);
		}

		if (chdir("/") == -1) {
			perror("Error on set daemon dir!");
			syslog(LOG_ERR, "Error on set daemon dir!");
			close(sockfd);
			closelog();
			exit(1);
		}

		int null_fd = open("/dev/null", O_RDWR);
		dup2(null_fd, STDIN_FILENO);
		dup2(null_fd, STDOUT_FILENO);
		dup2(null_fd, STDERR_FILENO);
	}
	
	if ((file_fd = open(WRITE_FILE, O_RDWR | O_CREAT | O_APPEND | O_SYNC, S_IRWXU | S_IRGRP | S_IROTH)) == -1) {
		perror("Error on open file to write!");
		syslog(LOG_ERR, "Error on open file to write!");
		close(sockfd);
		closelog();
		freeaddrinfo(res);
		exit(1);
	} 

	struct sockaddr_storage connection_addr;
	socklen_t connection_addr_size = sizeof(connection_addr);
	
	while (1) {
		if ((connection_fd = accept(sockfd, (struct sockaddr *)&connection_addr, &connection_addr_size)) == -1) {
			perror("Error on accept connection!");
			syslog(LOG_ERR, "Error on accept connection!");
			close(sockfd);
			close(file_fd);
			closelog();
			freeaddrinfo(res);
			exit(1);
		} 
		
		char s[INET6_ADDRSTRLEN];
		inet_ntop(
			connection_addr.ss_family,
			&((struct sockaddr_in*)((struct sockaddr*)&connection_addr))->sin_addr,
			s, sizeof(s)
		);	

		syslog(LOG_INFO, "Accepted connection from %s", s);
		
		char buf[BUFFER_SIZE];
		int n, total = 0;
		while ((n = recv(connection_fd, &buf, BUFFER_SIZE, 0)) != 0) {
			total += n;
			write(file_fd, &buf, n);
			if (buf[n-1] == '\n') break;
		}

		lseek(file_fd, 0, SEEK_SET);	
		while ((n = read(file_fd, &buf, BUFFER_SIZE)) != 0) {
			send(connection_fd, &buf, n, 0);
		}
			
		close(connection_fd);
		syslog(LOG_INFO, "Closed connection from %s", s);
	}

	return 0;
}

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
#include <pthread.h>
#include <time.h>

#include "queue.h"

#define PORT "9000"
#define CONNECTION_QUEUE_SIZE 10
#define BUFFER_SIZE 256

#define WRITE_FILE "/var/tmp/aesdsocketdata"

int sockfd = -1, file_fd = -1;
struct addrinfo hints, *res = NULL;
int should_free_res = 0;

// Connection List
typedef struct slist_data_s slist_data_t;
struct slist_data_s {
    pthread_t value;
    SLIST_ENTRY(slist_data_s) entries;
};

slist_data_t *datap = NULL;
SLIST_HEAD(slisthead, slist_data_s) head;

// File Mutex
pthread_mutex_t lock;

// Timer
timer_t timer;

// Signal Handler
static void signal_handler(int signal_number) {
	if (signal_number != SIGINT && signal_number != SIGTERM) return;
	
	timer_running = 0;

	while(!SLIST_EMPTY(&head)) {	
		datap = SLIST_FIRST(&head);
		if (datap != NULL) pthread_join(datap->value, NULL);
		SLIST_REMOVE_HEAD(&head, entries);
		if (datap != NULL) free(datap);
	}

	if (sockfd != -1) close(sockfd);
	if (file_fd != -1) close(file_fd);
	if (should_free_res == 1) freeaddrinfo(res);
	
	pthread_mutex_destroy(&lock);

	remove(WRITE_FILE);
	
	struct itimerspec disarm = {0};
    timer_settime(timer, 0, &disarm, NULL);
			
	timer_delete(timer);
	
	syslog(LOG_INFO, "Caught sinal, exiting");
	closelog();
	
	exit(0);
}

// Connection Handler
void* handle_connection(void* arg) {
	int connection_fd = *(int*)arg;

	char buf[BUFFER_SIZE];
	int n, total = 0;
	while ((n = recv(connection_fd, buf, BUFFER_SIZE, 0)) != 0) {
		total += n;
		pthread_mutex_lock(&lock);
		write(file_fd, &buf, n);
		pthread_mutex_unlock(&lock);
		if (buf[n-1] == '\n') break;
	}

	pthread_mutex_lock(&lock);
	lseek(file_fd, 0, SEEK_SET);	
	while ((n = read(file_fd, buf, BUFFER_SIZE)) != 0) {
		send(connection_fd, &buf, n, 0);
	}
	pthread_mutex_unlock(&lock);	
	
	free(arg);
	close(connection_fd);

	return NULL;
}

// Insert timestamp on file
void handle_time() {
	printf("Time\n\r");
	fflush(stdout);	

	time_t now;
	struct tm *tmp;
	char outstr[200];
	int n;

	strcpy(outstr, "timestamp:");

	now = time(NULL);
	tmp = localtime(&now);
	if (tmp == NULL) {
    	syslog(LOG_ERR, "localtime");
        return;
	}

	if ((n = strftime(&outstr[10], sizeof(outstr), "%a, %d %b %Y %T %z", tmp)) == 0) {
        syslog(LOG_ERR, "strftime");
		return;
	}

	outstr[n+10] = '\n';

	pthread_mutex_lock(&lock);
	write(file_fd, &outstr, n+11);
	pthread_mutex_unlock(&lock);
}

void* timer_thread() {
	while (timer_running) {
		handle_time();
		sleep(3);
	}

	return NULL;
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
	
	// Open File
	if ((file_fd = open(WRITE_FILE, O_RDWR | O_CREAT | O_APPEND | O_SYNC, S_IRWXU | S_IRGRP | S_IROTH)) == -1) {
		perror("Error on open file to write!");
		syslog(LOG_ERR, "Error on open file to write!");
		close(sockfd);
		closelog();
		freeaddrinfo(res);
		exit(1);
	} 

	// Setting timer			
	struct sigevent sevp = {
        .sigev_notify = SIGEV_THREAD,
        .sigev_notify_function = handle_time,
        .sigev_notify_attributes = NULL
    };

	if (timer_create(CLOCK_MONOTONIC, &sevp, &timer) == -1) {
		perror("Error create timer");
		syslog(LOG_ERR, "Error on create timer");
		close(sockfd);
		close(file_fd);
		closelog();
		freeaddrinfo(res);
		exit(1);
	} 
		
	struct itimerspec its = {
        .it_value = {.tv_sec = 10, .tv_nsec = 0},
        .it_interval = {.tv_sec = 10, .tv_nsec = 0}
    };
	
	if (timer_settime(timer, 0, &its, NULL) == -1) { 
		perror("Error set timer time");
		syslog(LOG_ERR, "Error on set timer time");
		close(sockfd);
		close(file_fd);
		closelog();
		freeaddrinfo(res);
		exit(1);
	}

	struct sockaddr_storage connection_addr;
	socklen_t connection_addr_size = sizeof(connection_addr);
	int connection_fd;
	SLIST_INIT(&head);
	pthread_mutex_init(&lock, NULL);	

	while (1) {
		if ((connection_fd = accept(sockfd, (struct sockaddr *)&connection_addr, &connection_addr_size)) == -1) {
			perror("Error on accept connection!");
			syslog(LOG_ERR, "Error on accept connection!");
			closelog();
			exit(1);
		} 
		
		char s[INET6_ADDRSTRLEN];
		inet_ntop(
			connection_addr.ss_family,
			&((struct sockaddr_in*)((struct sockaddr*)&connection_addr))->sin_addr,
			s, sizeof(s)
		);	

		syslog(LOG_INFO, "Accepted connection from %s", s);

		datap = (slist_data_t*)malloc(sizeof(slist_data_t));
		int* connection_fd_p = (int*)malloc(sizeof(int));
		*connection_fd_p = connection_fd;  
		if (pthread_create(&datap->value, NULL, &handle_connection, (void*)connection_fd_p) != 0) {	
			free(datap);
			free(connection_fd_p);
			close(connection_fd);	
			syslog(LOG_ERR, "Fail on create socket");
			continue;
		}
		SLIST_INSERT_HEAD(&head, datap, entries);
	}

	return 0;
}

#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>

#define portMin 0
#define portMax 65535
#define CHUNK_MAX_SIZE 1024

struct connection {
	char ip[15];
	int port;
	int socket;
	int fd;
};

char* get_time(char*buffer) {
	time_t rawtime = time(0);
	struct tm *ptm = localtime(&rawtime);
	strftime(buffer, 1024, "[%Y-%m-%d %r]", ptm);
	return buffer;
}

void *socket_read(void *arg) {
	struct connection* conn = (struct connection*)arg;
	char buf[CHUNK_MAX_SIZE];
	int n;
	char buffer[20];
	printf("%s %s:%d,open\n", get_time(buffer), conn->ip, conn->port);
	while ((n = read(conn->fd, buf, CHUNK_MAX_SIZE)) > 0) {
		printf("%s %s:%d,data,%d,", get_time(buffer), conn->ip, conn->port, n);
		for (int i = 0; i < n; i++) {
			printf("%c", buf[i]);
		}
	}
	printf("%s %s:%d,close\n", get_time(buffer), conn->ip, conn->port);
	close(conn->fd);
}

void *async_listen(void *arg) {
	struct connection* conn = (struct connection*)arg;
	pthread_t tid;
	while (1) {
		struct sockaddr_in client_addr;
		int addrlen;
		conn->fd = accept(conn->socket, (struct sockaddr*)&client_addr, (socklen_t*)&addrlen);
		if (conn->fd < 0) {
			perror("accept");
			exit(1);
		}
		sprintf(conn->ip, "%d.%d.%d.%d", 
			 client_addr.sin_addr.s_addr&0xFF,
			(client_addr.sin_addr.s_addr&0xFF00)>>8,
			(client_addr.sin_addr.s_addr&0xFF0000)>>16,
			(client_addr.sin_addr.s_addr&0xFF000000)>>24
		);
		pthread_create(&tid, NULL, socket_read, (void *)conn);
	}
	// close(sock); socket never closes
}

int main() {
	printf("Binding %d ports", portMax-portMin);
	int opt = 1;
	struct sockaddr_in* addrs; // ew
	int* sockets; // all socket file descriptors
	int* ports;
	int* failedPorts;
	int* failedPortsCurPtr;
	int failedCount = 0;
	int successCount = 0;
	int fdFail = 0;
	addrs = malloc(sizeof(struct sockaddr_in) * (portMax - portMin));
	sockets = malloc(sizeof(int) * (portMax - portMin));
	ports = malloc(sizeof(int) * (portMax - portMin));
	failedPorts = malloc(sizeof(int) * (portMax - portMin));
	for (int i = portMin; i < portMax; i++) { // set all fails to non fail
		failedPorts[i] = -1;
	}
	failedPortsCurPtr = failedPorts;
	for (int i = portMin; i < portMax; i++) {
		if (!(i%1024)) {
			putchar('.');
			fflush(stdout);
		};

		ports[successCount] = i;

		if ((sockets[successCount] = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
			//perror("socket failed");
			fdFail = i;
			break;
		}

		addrs[i].sin_family = AF_INET;
		addrs[i].sin_port = htons(i);
		addrs[i].sin_addr.s_addr = INADDR_ANY;

		if (setsockopt(sockets[successCount], SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(int))) { // socket is in use
			*failedPortsCurPtr = i; // add port to failed ports list
			failedPortsCurPtr++; // increment pointer
			failedCount++; // increment failed count
			continue;
		};

		if (bind(sockets[successCount], (struct sockaddr *)&addrs[i], sizeof(addrs[i])) < 0) {
			*failedPortsCurPtr = i; // add port to failed ports list
			failedPortsCurPtr++; // increment pointer
			failedCount++; // increment failed count
			continue;
		};
		
		if (listen(sockets[successCount], 3) < 0) {
			printf("Failed to create thread for port %d\n", ports[successCount]);
			*failedPortsCurPtr = i; // add port to failed ports list
			failedPortsCurPtr++; // increment pointer
			failedCount++; // increment failed count
			continue;
		}
		successCount++;
	}
	printf("\n%d ports binded, %d ports failed to bind\n\n", successCount, failedCount + (fdFail > 0));

	if (fdFail) 
		printf("There was a file descriptor failure at port %d, is your file descriptor limit too small? (check current limit with ulimit -n and set with ulimit -n 99999)\n\n", fdFail);

	if (failedCount > 999) {
		puts(">1,000 ports failed to bind, if this is a problem make sure you're running as root");
	} else if (failedCount > 0) {
		printf("All these ports failed: ");
		for (int i = 0; failedPorts[i] != -1; i++) {
			printf("%d", failedPorts[i]);
			if (i != failedCount - 1) {
				printf(", ");
			} else {
				printf("\n");
			}
		}
	}

	printf("Creating %d threads", successCount);
	pthread_t tid;
	int launches = 0;
	int fails = 0;
	for (int i = 0; i < successCount; i++) {
		if (!(i%1024)) {
			putchar('.');
			fflush(stdout);
		};

		//printf("Creating thread for port %d and descriptor %d\n", ports[i], sockets[i]);
		//printf("please from %d", sockets[i]);

		struct connection* conn = malloc(sizeof(struct connection));

		conn->socket = sockets[i];
		conn->port = ports[i];

		if (pthread_create(&tid, NULL, async_listen, (void *)conn)==0) {
			launches++;
		} else {
			perror("hi");
			exit(0);
			fails++;
		}
	}

	printf("\nLaunched %d threads and failed %d times\nHanging...\n", launches, fails);
	pthread_join(tid, NULL); // last thread created will never return
}

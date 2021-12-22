#include <stdint.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <argp.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <time.h>

void delay(int number_of_seconds)
{
    // Converting time into milli_seconds
    int milli_seconds = 1000 * number_of_seconds;
  
    // Storing start time
    clock_t start_time = clock();
  
    // looping till required time is not achieved
    while (clock() < start_time + milli_seconds)
        ;
}

int main(int argc, char *argv[]) {
	if(argc < 2) {
		printf("Must invoke ./TCPtest x.x.x.x y\n");
		return 0;
	}

	int port = atoi(argv[2]);
	printf("Connecting to port %d\n", port);

	struct sockaddr_in server;
	const int socket_handle = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if(socket_handle < 0)
	{
		perror("Socket: ");
		exit(EXIT_FAILURE);
	}
	memset(&server, 0, sizeof(server));
	server.sin_family = AF_INET;
	server.sin_port = htons(port);
	server.sin_addr.s_addr = inet_addr(argv[1]); // Router's public IP
	unsigned int len = sizeof(server);
	const char msg[] = "Hello";
	char resp[100];
	if(connect(socket_handle, (struct sockaddr *)(&server), sizeof(server)) < 0)
	{
		printf("Connect failed\n");
	} 
	else {
		printf("Connect succeeded\n");
		char msg[] = "Hello";
		send(socket_handle, msg, sizeof(msg), 0);
		close(socket_handle);
	}
	/*

	while(1) {
		if(connect(socket_handle, (struct sockaddr *)(&server), sizeof(server)) < 0)
		{
			printf("Connect failed\n");
		} 
		else
			printf("Connect succeeded\n");
		sleep(2);
	}*/
	return 0;
}

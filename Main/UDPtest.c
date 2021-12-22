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

int main(void) {

	struct sockaddr_in server;
	const int socket_handle = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if(socket_handle < 0)
	{
		perror("Socket: ");
		exit(EXIT_FAILURE);
	}
	memset(&server, 0, sizeof(server));
	server.sin_family = AF_INET;
	server.sin_port = htons(41714);
	server.sin_addr.s_addr = inet_addr("96.255.198.153"); // Router's public IP
	unsigned int len = sizeof(server);
	const char msg[] = "Hello";
	char resp[100];
	printf("Ready.\n");

	while(1) {
		if(sendto(socket_handle, &msg, sizeof(msg), 0, (struct sockaddr *)(&server), len) < 0)
		{
			perror("sendto: ");
			exit(EXIT_FAILURE);
		}
		printf("Sent: %s\n", msg);
		int readval = recvfrom(socket_handle, &resp, sizeof(resp), 0, (struct sockaddr *)(&server), &len);
		if(readval > 0) // we got a packet
		{
			printf("Received: %s\n", resp);
		}
		else {
			perror("recvfrom: ");
			exit(EXIT_FAILURE);
		}
		sleep(2);
	}
	return 0;
}
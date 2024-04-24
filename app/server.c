#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

void send_ok_response(int fd);
void send_not_found_response(int fd);
char* extract_req_url(char* buffer);

#define BUFFER_LENGTH 1024

int main() {
	// Disable output buffering
	setbuf(stdout, NULL);

	// You can use print statements as follows for debugging, they'll be visible when running tests.
	printf("Logs from your program will appear here!\n");

	// Uncomment this block to pass the first stage
	
	int server_fd, client_addr_len;
	struct sockaddr_in client_addr;
	
	server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (server_fd == -1) {
		printf("Socket creation failed: %s...\n", strerror(errno));
		return 1;
	}
	
	// Since the tester restarts your program quite often, setting REUSE_PORT
	// ensures that we don't run into 'Address already in use' errors
	int reuse = 1;
	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse)) < 0) {
		printf("SO_REUSEPORT failed: %s \n", strerror(errno));
		return 1;
	}
	
	struct sockaddr_in serv_addr = { .sin_family = AF_INET ,
									 .sin_port = htons(4221),
									 .sin_addr = { htonl(INADDR_ANY) },
									};
	
	if (bind(server_fd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) != 0) {
		printf("Bind failed: %s \n", strerror(errno));
		return 1;
	}
	
	int connection_backlog = 5;
	if (listen(server_fd, connection_backlog) != 0) {
		printf("Listen failed: %s \n", strerror(errno));
		return 1;
	}
	
	printf("Waiting for a client to connect...\n");
	client_addr_len = sizeof(client_addr);

	int fd = accept(server_fd, (struct sockaddr *) &client_addr, &client_addr_len);

	if(fd == -1) {
		printf("Could not accept connection from client.\n");
		return 1;
	}
	

	printf("Client connected\n");

	char buffer[BUFFER_LENGTH];

	int bytes = recv(fd, buffer, BUFFER_LENGTH, 0);

	if (bytes == -1)
	{
		printf("Recieving bytes error.\n");
		return 1;
	}

    buffer[bytes] = '\0';
	char* url = extract_req_url(buffer);

	if (strcmp(url, "/") == 0)
	{
		send_ok_response(fd);
	} else {
		send_not_found_response(fd);
	}
	
	close(server_fd);

	return 0;
}

void send_ok_response(int fd) {
	const char* ok_response = "HTTP/1.1 200 OK\r\n\r\n";
	send(fd, ok_response, strlen(ok_response), 0);	
}

void send_not_found_response(int fd) {
	const char* not_found_response = "HTTP/1.1 404 NOT FOUND\r\n\r\n";
	send(fd, not_found_response, strlen(not_found_response), 0);	
}


char* extract_req_url(char* buffer) {
	const char* delimiter = " ";
	char* token;

	// First token = request type GET, POST etc
	token = strtok(buffer, delimiter);

	// 2nd Token = Url
	token = strtok(NULL, delimiter);

	return token;
}

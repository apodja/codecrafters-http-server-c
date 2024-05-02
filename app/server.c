#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>

void send_ok_response(int fd);
void send_not_found_response(int fd);
char* extract_req_url(char* buffer, size_t size);
char* extract_query_str(char* url);
void build_response(char* response_body, char* response_sts, char* res_cont_type, char* response_buffer);
char* extract_user_agent(char* request_buffer);
void *handle_request(void* fd);
char* handle_get_file(char* filepath);

#define BUFFER_LENGTH 1024

struct arg_s {
	int* fd;
	char* dir;
};


int main(int argc, char* argv[]) {
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

	while (1)
	{
		
		client_addr_len = sizeof(client_addr);
		int client_fd = accept(server_fd, (struct sockaddr *) &client_addr, &client_addr_len);

		if(client_fd == -1) {
			printf("Could not accept connection from client.\n");
			return 1;
		}

		printf("Client connected\n");

		int* p_cli_fd = malloc(sizeof(int));
		char* dir = malloc(strlen(argv[2]) + 1);
		strcpy(dir, argv[2]);
		*p_cli_fd = client_fd;
		pthread_t tid;
		struct arg_s *arg_struct = malloc(sizeof(struct arg_s));
		arg_struct->fd = p_cli_fd;
		arg_struct->dir = dir;
		
		pthread_create(&tid, NULL, &handle_request, (void*)arg_struct);
	}
	
	close(server_fd);

	return 0;
}

void *handle_request(void* args) {
	struct arg_s* arg_s = args;
	int client_fd = *(arg_s->fd);
	free(arg_s->fd);
	char* dir = arg_s->dir;
		
	char buffer[BUFFER_LENGTH];
	char response_buffer[BUFFER_LENGTH];

	size_t bytes = recv(client_fd, buffer, BUFFER_LENGTH, 0);

	if (bytes == -1)
	{
		printf("Recieving bytes error.\n");
		return NULL;
	}

    buffer[bytes] = '\0';
	char* url = extract_req_url(buffer, bytes);
	printf("%s\n", url);
	if (strcmp(url, "/") == 0) {
		send_ok_response(client_fd);
	} else if (strncmp(url, "/echo", 5) == 0) {
		char* res_body = extract_query_str(url);
		build_response(res_body,"200 OK","text/plain", response_buffer);
		send(client_fd, response_buffer, strlen(response_buffer),0);
		if (res_body != NULL)
		{
			free(res_body);
		}
	} else if(strcmp(url, "/user-agent") == 0){
		char* user_agent = extract_user_agent(buffer);
		if (user_agent != NULL)
		{
			build_response(user_agent, "200 OK", "text/plain", response_buffer);
			send(client_fd, response_buffer, strlen(response_buffer), 0);
			free(user_agent);
		}
		
	} else if (strcmp(url, "/files")){
		char* filename = extract_query_str(url);
		
		if(filename != NULL) {
			char* full_fpath = malloc(strlen(filename) + strlen(dir) + 1);
			
			strcpy(full_fpath, dir);
			free(dir);
			free(arg_s);
			strcat(full_fpath, filename);
			char* content = handle_get_file(full_fpath);
			if (content != NULL) {
				build_response(content, "200 OK", "application/octet-stream", response_buffer);
				send(client_fd, response_buffer, strlen(response_buffer), 0);
				free(content);
			} else {
				build_response("", "404 Not Found", "application/octet-stream", response_buffer);
				send(client_fd, response_buffer, strlen(response_buffer), 0);
			}
		}
	} 
	free(url);
	send_not_found_response(client_fd);
	
}
	

void send_ok_response(int fd) {
	const char* ok_response = "HTTP/1.1 200 OK\r\n\r\n";
	send(fd, ok_response, strlen(ok_response), 0);	
}

void send_not_found_response(int fd) {
	const char* not_found_response = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nContent-Type: text/plain\r\n\r\n";
	send(fd, not_found_response, strlen(not_found_response), 0);	
}


char* extract_req_url(char* buffer, size_t size) {
	const char* delimiter = " ";
	char* token;
	char* buffer_cpy = malloc(size);

	strncpy(buffer_cpy, buffer, size);

	// First token = request type GET, POST etc
	token = strtok(buffer_cpy, delimiter);
	
	// 2nd Token = Url
	token = strtok(NULL, delimiter);

	char* url = malloc(strlen(token) + 1);
	strcpy(url, token);
	free(buffer_cpy);

	return url;
}

char* extract_query_str(char* url) {
	char* query_str_first = strchr(url, '/');
	char* query_str_sec;

	if (query_str_first != NULL) {
		query_str_sec = strchr(query_str_first + 1, '/');

		if (query_str_sec != NULL)
		{
			char* query_str = malloc(strlen(query_str_sec + 1));
			if (query_str != NULL)
			{
				strcpy(query_str, query_str_sec + 1);
				return query_str;
			}
			
		}
		
	}
	return NULL;
}

void build_response(char* response_body, char* response_sts, char* res_cont_type, char* response_buffer) {
	int response = sprintf(response_buffer,
		"HTTP/1.1 %s\r\nContent-Type: %s\r\nContent-Length: %d\r\n\r\n%s",
		response_sts,
		res_cont_type,
		strlen(response_body),
		response_body
	);
}

char* extract_user_agent(char* request_buffer) {
	char* req_buffer_cpy = malloc(strlen(request_buffer));
	strcpy(req_buffer_cpy, request_buffer);
	char* user_agent;
	const char* needle = "User-Agent: ";
	user_agent = strstr(req_buffer_cpy, needle);
	if (user_agent == NULL)
	{
		free(req_buffer_cpy);
		return NULL;
	}

	user_agent += strlen(needle);
	user_agent = strtok(user_agent, "\r\n");
	
	if (user_agent == NULL)
	{
		free(req_buffer_cpy);
		return NULL;
	}

	char* ret_user_agent = malloc(strlen(user_agent) + 1);
	strcpy(ret_user_agent, user_agent);
	free(req_buffer_cpy);
	

	return ret_user_agent;
}

char* handle_get_file(char* filepath) {
	FILE* fp;
	fp = fopen(filepath, "r");
	if(fp == NULL) {
		printf("Could not open file: %s.\n", filepath);
		free(filepath);
		return NULL;
	}

	fseek(fp, 0, SEEK_END);
    long f_length = ftell(fp);
    fseek(fp, 0L, SEEK_SET);

	char* contents = malloc(f_length + 1);

	fread(contents, 1, f_length, fp);
	contents[f_length] = '\0';
	fclose(fp);

	free(filepath);

	return contents;
}


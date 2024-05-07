#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/stat.h>

#define BUFFER_LENGTH 1024

struct arg_s {
	int* fd;
	char* dir;
};

typedef enum  {
	GET,
	POST
}RequestType;

typedef struct {
	char* url;
	char* query_string;
} Url;

 struct Request{
	RequestType type;
	Url* url;
	char* user_agent;
	char* body;
};

typedef struct {
	char* status;
	char* content_type;
	char* body;
} Response;


void send_ok_response(int fd);
void send_not_found_response(int fd);
void send_created_response(int fd);
void build_response(char* response_body, char* response_sts, char* res_cont_type, char* response_buffer);
char* extract_user_agent(char* request_buffer);
void *handle_request(void* fd);
char* handle_get_file(char* filepath);
void free_request(struct Request* request);
struct Request* parse_request(char* buffer);
struct Request* init_request();
Url* parse_url(char* line, RequestType type);
int handle_post_file(char* filepath, char* content);

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

struct Request* init_request() {
	struct Request* request = malloc(sizeof(struct Request));
	request->body = NULL;
	request->type = GET; // Default
	request->user_agent = NULL;

	return request;
}

struct Request* parse_request(char* buffer) {
	struct Request* request = init_request();
	const char* delimiter = "\r\n";
	char* buff_cpy = malloc(strlen(buffer) + 1);
	strcpy(buff_cpy, buffer);
	char* line;
	line = strtok(buff_cpy, delimiter);
	if (strncmp(line, "GET", 3) == 0) {
		request->type = GET;
	} else if(strncmp(line, "POST", 4) == 0) {
		request->type = POST;
	}

	Url* url = parse_url(line, request->type);
	request->url = url;
		
	// Hostname line
	line = strtok(NULL, delimiter);
	if (line == NULL)
	{
		printf("No hostname provided in the request.\n");
		return request;
	}

	// User-Agent Line
	line = strtok(NULL, delimiter);
	if (line == NULL)
	{
		printf("No User-Agent provided in the request.\n");
		return request;
	}
	
	char* user_agent = extract_user_agent(line);
	request->user_agent = user_agent;

	char* last_line;
	while (line != NULL) {
		last_line = line;
		line = strtok(NULL, delimiter);
	}

	if (strcmp(last_line, "Accept-Encoding: gzip" ) != 0)
	{
		request->body = malloc(strlen(last_line) + 1);
		strcpy(request->body, last_line);
	}
	free(buff_cpy);
	
	return request;
}

Url* parse_url(char* line, RequestType type) {
	Url* url = malloc(sizeof(Url));

	char* line_cpy = malloc(strlen(line) + 1);
	strcpy(line_cpy, line);
	char* start;
	char* end = " HTTP/1.1";

	if(type == GET) {
		start = "GET "; 
	} else {
		start = "POST ";
	}
	
	char* left = strstr(line_cpy, start);
	
	if (left == NULL)
	{
		free(url);
		free(line_cpy);
		return NULL;
	}
	left += strlen(start);
	
	char* right =  strstr(left, end);

	size_t length = right - left;

	char* url_str = malloc(length + 1);

	strncpy(url_str, left, length);
	url_str[length] = '\0';
	free(line_cpy);
	char* lastSlash = strrchr(url_str, '/');
	if(strcmp(lastSlash, url_str) == 0) {
		// No query string
		url->url = malloc(strlen(url_str) + 1);
		strcpy(url->url, url_str);
		url->query_string = NULL;

	} else {
		int index_last_slash = lastSlash - url_str;
		url_str[index_last_slash] = '\0';
		url->url = malloc(strlen(url_str) + 1);
		url->query_string = malloc(strlen(lastSlash + 1) + 1);
		strcpy(url->url, url_str);
		strcpy(url->query_string, lastSlash + 1);
	}

	free(url_str);

	return url;
}

void free_request(struct Request* request) {
	free(request->url->url);
	if(request->url->query_string != NULL)
		free(request->url->query_string);
	free(request->url);
	free(request->user_agent);
	if(request->body != NULL)
		free(request->body);
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
	struct Request* request = parse_request(buffer);
	printf("%s\n",request->url->url);
	if (strcmp(request->url->url, "/") == 0) {
		send_ok_response(client_fd);
	} else if (strncmp(request->url->url, "/echo", 5) == 0) {
		build_response(request->url->query_string,"200 OK","text/plain", response_buffer);
		send(client_fd, response_buffer, strlen(response_buffer),0);
	} else if(strcmp(request->url->url, "/user-agent") == 0){
		build_response(request->user_agent, "200 OK", "text/plain", response_buffer);
		send(client_fd, response_buffer, strlen(response_buffer), 0);
	} else if (strncmp(request->url->url, "/files", 6) == 0){

		char* full_fpath = malloc(strlen(request->url->query_string) + strlen(dir) + 1);
		
		strcpy(full_fpath, dir);

		free(dir);
		free(arg_s);
		strcat(full_fpath, request->url->query_string);
		if (request->type == GET) {
			char* content = handle_get_file(full_fpath);
			if (content != NULL) {
				build_response(content, "200 OK", "application/octet-stream", response_buffer);
				send(client_fd, response_buffer, strlen(response_buffer), 0);
				free(content);
			} else {
				send_not_found_response(client_fd);
			}
		} else if (request->type == POST) {
			int post = handle_post_file(full_fpath, request->body);
			
			if (post)
			{
				send_created_response(client_fd);
			}
			
		} else {
			printf("Not implemented\n");
		}
		free(full_fpath);
		
	} else {
		send_not_found_response(client_fd);
	}
	free_request(request);
}
	

void send_ok_response(int fd) {
	const char* ok_response = "HTTP/1.1 200 OK\r\n\r\n";
	send(fd, ok_response, strlen(ok_response), 0);	
}

void send_not_found_response(int fd) {
	const char* not_found_response = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nContent-Type: text/plain\r\n\r\n";
	send(fd, not_found_response, strlen(not_found_response), 0);	
}

void send_created_response(int fd) {
	const char* created_response = "HTTP/1.1 201 Created\r\nContent-Length: 0\r\nContent-Type: application/octet-stream\r\n\r\n";
	send(fd, created_response, strlen(created_response), 0);	
}

void build_response(char* response_body, char* response_sts, char* res_cont_type, char* response_buffer) {
	int response = sprintf(response_buffer,
		"HTTP/1.1 %s\r\nContent-Type: %s\r\nContent-Length: %d\r\n\r\n%s",
		response_sts,
		res_cont_type,
		(int)strlen(response_body),
		response_body
	);
}

char* extract_user_agent(char* line) {
	char* line_cpy = malloc(strlen(line));
	strcpy(line_cpy, line);
	char* user_agent;
	const char* needle = "User-Agent: ";
	user_agent = strstr(line_cpy, needle);
	if (user_agent == NULL)
	{
		free(line_cpy);
		return NULL;
	}

	user_agent += strlen(needle);
	
	if (user_agent == NULL)
	{
		free(line_cpy);
		return NULL;
	}

	char* ret_user_agent = malloc(strlen(user_agent) + 1);
	strcpy(ret_user_agent, user_agent);
	free(line_cpy);

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

	return contents;
}

int handle_post_file(char* filepath, char* content) {
    FILE* fp = fopen(filepath, "w+");
	if (fp == NULL)
	{
		perror("fopen");
		fclose(fp);
		return 0;
	}

	fwrite(content, 1, strlen(content), fp);
	fclose(fp);

	return 1;
}


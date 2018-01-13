#include <stdio.h>
#include <stdlib.h>
#include <alloca.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>

typedef struct{
	char htmlVersion[9];
	char method[4];
	char filename[32];
} Request;

typedef struct {
	char htmlVersion[9];
	char statusCode[48];
	char* headers;
	char* body;
} Response;

int initializeServerSocket(char ip[]){
	int newSocket = socket(AF_INET, SOCK_STREAM, 0);
	struct sockaddr_in address;
	address.sin_family = AF_INET;
	address.sin_port = htons(80);
	if(inet_aton(ip, &(address.sin_addr)) < 0)
		perror("Failed to decode ip");
	if(bind(newSocket, (struct sockaddr*)&address, sizeof(address)) < 0)
		perror("Failed to bind socket!");
	if(listen(newSocket, 5) < 0)
		perror("Failed to create listening socket!");
	return newSocket;
}

void getResponseCodeString(int code, char* string){
	switch(code){
		case 200:
			strcpy(string, "200 OK");
			break;
		case 501:
			strcpy(string, "501 Not Implemented");
			break;
		case 505:
			strcpy(string, "505 HTTP Version Not Supported");
			break;
	}
}

void generateResponseString(char* string, Response response){
	sprintf(string, "%s %s\r\n%s\r\n\%s", response.htmlVersion, response.statusCode, response.headers, response.body);
}


void handleConnection(int socket){
	//Create buffer for recv data
	char buffer[1024];
	memset(buffer, 0, 1024);
	
	//Recive data
	int received = 0;
	do{
		received = recv(socket, buffer, sizeof(buffer), 0);
		if(received < 0)
			return;
	}while(received < 14);
	
	//Get request info
	Request request;
	strncpy(request.method, strtok(buffer, " "), 3);	
	strncpy(request.filename, strtok(NULL, " "), 31);	
	strncpy(request.htmlVersion, strtok(NULL, " "), 8);	

	//Set up response 
	Response response;

	char* body = malloc(sizeof(char)*200);
	char* headers = malloc(sizeof(char)*60);
	memset(body, 0, 200);
	memset(headers, 0, 60);
	response.headers = headers;
	response.body = body;

	strncpy(response.htmlVersion, "HTTP/1.1", 8);
	
	//Check for request validity and parameters
	if(strcmp(request.htmlVersion, "HTTP/1.1") != 0){
		getResponseCodeString(505, response.statusCode);
	}
	else if(strcmp(request.method, "GET") != 0){
		getResponseCodeString(501, response.statusCode);
	}
	else{
		getResponseCodeString(200, response.statusCode);
		//TODO: output specified file rather than hard coded html
		strcpy(response.body, "<html><body><h1>Hello, World!</h1></body></html>");
	}

	//Form a response
	char responseString[2048];
	memset(buffer, 0, 2048);

	generateResponseString(responseString, response);

	printf("%s", responseString);

	//Send the response
	if(send(socket, responseString, strlen(responseString), 0) < 0)
		perror("Failed to send response!");

	free(response.body);
	free(response.headers);
}


int main(){
	signal(SIGCHLD, SIG_IGN);
	int serverSocket = initializeServerSocket("0.0.0.0");
	while(1){
		int connectionSocket = accept(serverSocket, NULL, NULL);
		if(connectionSocket < 0)
			perror("Failed to accept connection!");
		int pid = fork();
		if(pid == 0){
			handleConnection(connectionSocket);
			shutdown(connectionSocket, SHUT_RDWR);
			close(connectionSocket);
			_exit(0);
		}
	}
	return 0;
}

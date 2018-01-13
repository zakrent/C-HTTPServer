#include <stdio.h>
#include <time.h>
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
#include <dirent.h>

typedef struct{
	char htmlVersion[9];
	char method[4];
	char filename[32];
} Request;

typedef struct {
	char htmlVersion[9];
	char statusCode[48];
	int bodyLength;
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

int readFile(char** returnedFileBuffer, char fileName[]){
	// Return value:
	// -1  = file not found
	// -2  = internal error
	// > 0 = buffer size
	int fileNameLength = strlen(fileName);
	char* systemFileName;

	//when filename is / open index.html
	if(fileNameLength < 2){
	        char* defaultFileName = "index.html";
		systemFileName = defaultFileName;
	}
	else{
		systemFileName = fileName+1;
	}	

	//check if file exists
	char* fileDirectoryName = "./static";
	DIR* fileDirectory = opendir(fileDirectoryName); 
	if(fileDirectory == NULL){
		perror("Failed to open file directory!");
		return -2;
	}

	struct dirent *entry;
	while((entry = readdir(fileDirectory)) != NULL){
		if(entry->d_type == DT_REG && strcmp(entry->d_name, systemFileName) == 0){
			//generate file path
			char *filePath = alloca(strlen(fileDirectoryName)+strlen(entry->d_name)+2);
			sprintf(filePath, "%s/%s", fileDirectoryName, entry->d_name);

			//open file
			FILE *file;
			file = fopen(filePath, "r");
			if(file == NULL){
				perror("Failed to open file!");
				return -2;
			}

			//get file size
			if(fseek(file, 0, SEEK_END) < 0){
				perror("Failed to seek!");
				return -2;
			}
			long fileSize = ftell(file);
			rewind(file);

			//read file
			*returnedFileBuffer = malloc(fileSize);
			if(fread(*returnedFileBuffer, 1, fileSize, file) != fileSize){
				perror("Failed to read file!");
			}		
			return fileSize;
		}
	}
	return -1;
}

void getResponseCodeString(int code, char* string){
	switch(code){
		case 200:
			strcpy(string, "200 OK");
			break;
		case 404:
			strcpy(string, "404 Not Found");
			break;
		case 500:
			strcpy(string, "500 Internal Server Error");
			break;
		case 501:
			strcpy(string, "501 Not Implemented");
			break;
		case 505:
			strcpy(string, "505 HTTP Version Not Supported");
			break;
	}
}

char* generateResponseHeader(Response response){
	char nullString[] = "";
	if(response.headers == NULL){
		response.headers = nullString;	
	}

	int responseLength = strlen(response.htmlVersion)+1+strlen(response.statusCode)+2+strlen(response.headers)+2;
	char* string = malloc(responseLength);
	memset(string, 0, responseLength);
	sprintf(string, "%s %s\r\n%s\r\n", response.htmlVersion, response.statusCode, response.headers);
	return string;
}

char* getCurrentTimeString(){
	time_t rawtime;
	struct tm * timeinfo;

	time ( &rawtime );
	timeinfo = gmtime( &rawtime );
	return asctime(timeinfo);
}

char* generateHeaderFields(){
	char* headers = malloc(512);
	memset(headers, 0, 512);

	strcat(headers, "Server: CSSer\r\n");
	strcat(headers, "Date: ");
	strcat(headers, getCurrentTimeString());
	strcat(headers, "Connection: close\r\n");
	return headers;
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
	memset(&request, 0, sizeof(request));
	strncpy(request.method, strtok(buffer, " "), 3);	
	strncpy(request.filename, strtok(NULL, " "), 31);	
	strncpy(request.htmlVersion, strtok(NULL, " \r\n"), 8);

	//Set up response 
	Response response;
	memset(&response, 0, sizeof(response));
	strncpy(response.htmlVersion, "HTTP/1.1", 8);
	response.bodyLength = 0;
	
	//Check for request validity and parameters
	if(strcmp(request.htmlVersion, "HTTP/1.1") != 0){
		getResponseCodeString(505, response.statusCode);
	}
	else if(strcmp(request.method, "GET") != 0){
		getResponseCodeString(501, response.statusCode);
	}
	else{
		//get requested file
		char* fileBuffer;
		int ret = readFile(&fileBuffer, request.filename);
		if(ret == -1){
			getResponseCodeString(404, response.statusCode);
		}
		else if(ret < 0){
			getResponseCodeString(500, response.statusCode);
		}
		else{
			getResponseCodeString(200, response.statusCode);
			response.bodyLength = ret;
			response.body = fileBuffer;
		}
	}

	printf("%s\n", request.filename);

	//Form a header

	response.headers = generateHeaderFields();
	char* headerString = generateResponseHeader(response);

	//Send the response
	if(send(socket, headerString, strlen(headerString), 0) < 0)
		perror("Failed to send response!");
	if(response.bodyLength > 0)
		if(send(socket, response.body, response.bodyLength, 0) < 0)
			perror("Failed to send response!");

	free(headerString);
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

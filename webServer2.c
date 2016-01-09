#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pthread.h>
#include <sys/wait.h>

#define BUFFER_SIZE 512
#define MAX_FILE_SIZE 5*1024
#define MAX_CONNECTIONS 3
#define TRUE 1
#define FALSE 0
#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1

int port;
int deamon = FALSE;
char *wwwroot;
char *conf_file;
char *log_file;
char *fileType_file;

FILE *filePointer = NULL;

struct sockaddr_in address;
struct sockaddr_storage connector;
int current_socket;
int connecting_socket;
socklen_t addr_size;


sendString(char *message, int socket)
{
	int length, bytes_sent;
	length = strlen(message);

	bytes_sent = send(socket, message, length, 0);

	return bytes_sent;
}

int sendBinary(int *byte, int length)
{
	int bytes_sent;

	bytes_sent = send(connecting_socket, byte, length, 0);

	return bytes_sent;


	return 0;
}
void sendHeader(char *Status_code, char *Content_Type, int TotalSize, int socket)
{
	char *head = "\r\nHTTP/1.1 ";
	char *content_head = "\r\nContent-Type: ";
	char *server_head = "\r\nServer: PT06";
	char *length_head = "\r\nContent-Length: ";
	char *date_head = "\r\nDate: ";
	char *newline = "\r\n";
	char contentLength[100];

	time_t rawtime;

	time ( &rawtime );

	// int contentLength = strlen(HTML);
	sprintf(contentLength, "%i", TotalSize);

	char *message = malloc((
		strlen(head) +
		strlen(content_head) +
		strlen(server_head) +
		strlen(length_head) +
		strlen(date_head) +
		strlen(newline) +
		strlen(Status_code) +
		strlen(Content_Type) +
		strlen(contentLength) +
		28 +
		sizeof(char)) * 2);

	if ( message != NULL )
	{

		strcpy(message, head);

		strcat(message, Status_code);

		strcat(message, content_head);
		strcat(message, Content_Type);
		strcat(message, server_head);
		strcat(message, length_head);
		strcat(message, contentLength);
		strcat(message, date_head);
		strcat(message, (char*)ctime(&rawtime));
		strcat(message, newline);

		sendString(message, socket);

		free(message);
	}    
}

void sendHTML(char *statusCode, char *contentType, char *content, int size, int socket)
{
	sendHeader(statusCode, contentType, size, socket);
	sendString(content, socket);
}

void sendFile(FILE *fp, int file_size)
{
	int current_char = 0;

	do{
		current_char = fgetc(fp);
		sendBinary(&current_char, sizeof(char));
	}
	while(current_char != EOF);
}

int scan(char *input, char *output, int start, int max)
{
	if ( start >= strlen(input) )
		return -1;

	int appending_char_count = 0;
	int i = start;
	int count = 0;

	for ( ; i < strlen(input); i ++ )
	{
		if ( *(input + i) != '\t' && *(input + i) != ' ' && *(input + i) != '\n' && *(input + i) != '\r')
		{
			if (count < (max-1))
			{
				*(output + appending_char_count) = *(input + i ) ;
				appending_char_count += 1;

				count++;
			}		
		}	
		else
			break;
	}
	*(output + appending_char_count) = '\0';	

	// Find next word start
	i += 1;

	for (; i < strlen(input); i ++ )
	{
		if ( *(input + i ) != '\t' && *(input + i) != ' ' && *(input + i) != '\n' && *(input + i) != '\r')
			break;
	}

	return i;
}




int checkFileType(char *extension, char *file_type)
{
	char *current_word = malloc(600);
	char *word_holder = malloc(600);
	char *line = malloc(200);
	int startline = 0;

	FILE *fileTypeFile = fopen(fileType_file, "r");

	free(file_type);

	file_type = (char*)malloc(200);

	memset (file_type,'\0',200);

	while(fgets(line, 200, fileTypeFile) != NULL) { 

		if ( line[0] != '#' )
		{
			startline = scan(line, current_word, 0, 600);
			while ( 1 )
			{
				startline = scan(line, word_holder, startline, 600);
				if ( startline != -1 )
				{
					if ( strcmp ( word_holder, extension ) == 0 )
					{
						memcpy(file_type, current_word, strlen(current_word));
						free(current_word);
						free(word_holder);
						free(line);
						return 1;	
					}
				}
				else
				{
					break;
				}
			}
		}

		memset (line,'\0',200);
	}

	free(current_word);
	free(word_holder);
	free(line);

	return 0;
}

int getHttpVersion(char *input, char *output)
{
	char *filename = malloc(100);
	int start = scan(input, filename, 4, 100);
	if ( start > 0 )
	{
		if ( scan(input, output, start, 20) )
		{

			output[strlen(output)+1] = '\0';

			if ( strcmp("HTTP/1.1" , output) == 0 )
				return 1;

			else if ( strcmp("HTTP/1.0", output) == 0 )

				return 0;
			else
				return -1;
		}
		else
			return -1;
	}

	return -1;
}

int GetExtension(char *input, char *output, int max)
{
	int in_position = 0;
	int appended_position = 0;
	int i = 0;
	int count = 0;

	for ( ; i < strlen(input); i ++ )
	{		
		if ( in_position == 1 )
		{
			if(count < max)
			{
				output[appended_position] = input[i];
				appended_position +=1;
				count++;
			}
		}

		if ( input[i] == '.' )
			in_position = 1;

	}

	output[appended_position+1] = '\0';

	if ( strlen(output) > 0 )
		return 1;
	//Default web page 
	if(strlen(input)==0)
		return -2;
	else
		return -1;
}

int Content_Lenght(FILE *fp)
{
	int filesize = 0;

	fseek(fp, 0, SEEK_END);
	filesize = ftell(fp);
	rewind(fp);

	return filesize;
}

int handleHttpGET(char *input)
{
	// IF NOT EXISTS
	// RETURN -1
	// IF EXISTS
	// RETURN 1

	char *filename = (char*)malloc(200 * sizeof(char));
	char *path = (char*)malloc(1000 * sizeof(char));
	char *extension = (char*)malloc(10 * sizeof(char));
	char *file_type = (char*)malloc(200 * sizeof(char));
	char *httpVersion = (char*)malloc(20 * sizeof(char));

	int contentLength = 0;
	int fileSupported = 0;
	int fileNameLenght = 0;


	memset(path, '\0', 1000);
	memset(filename, '\0', 200);
	memset(extension, '\0', 10);
	memset(file_type, '\0', 200);
	memset(httpVersion, '\0', 20);

	

	

	fileNameLenght = scan(input, filename, 5, 200);


	if ( fileNameLenght > 0 )
	{

		if ( getHttpVersion(input, httpVersion) != -1 )
		{
			
			FILE *fp;
			

			if ( GetExtension(filename, extension, 10) == -1 )
			{
				printf("File extension not existing");
				
				sendString("400 Check file extention\n", connecting_socket);

			
				
				free(filename);
				free(file_type);
				free(path);
				free(extension);
				

				return -1;
			}
			if ( GetExtension(filename, extension, 10) == -2 )
			{
				strcpy(path, "/home/vindya/Desktop/WebServer/");

				strcat(path, "index.html");			

				fp = fopen(path, "rb");
				contentLength = Content_Lenght(fp);
				sendHeader("200 OK","html",contentLength, connecting_socket);

				sendFile(fp, contentLength);
				fclose(fp);

				free(filename);
				free(file_type);
				free(path);
				free(extension);
				return -1;
			}
			fileSupported =  checkFileType(extension, file_type);


			if ( fileSupported != 1)
			{
				printf("File type not supported");

				sendString("400 Bad Request (Can not find the file (file type is not supported ))\n", connecting_socket);
				
				
				free(filename);
				free(file_type);
				free(path);
				free(extension);
				fclose(fp);

				return -1;
			}

			// Open the requesting file as binary //

			strcpy(path, "/home/vindya/Desktop/WebServer/");

			strcat(path, filename);			

			fp = fopen(path, "rb");

			if ( fp == NULL )
			{
				printf("Unable to open file");

				sendString("404 File Not Found\n", connecting_socket);

				free(filename);
				free(file_type);
				free(extension);
				free(path);

				return -1;
			}


			// Calculate Content Length //
			contentLength = Content_Lenght(fp);
			if (contentLength  < 0 )
			{
				printf("File size is zero");

				free(filename);
				free(file_type);
				free(extension);
				free(path);

				fclose(fp);

				return -1;
			}

			// Send File Content //
			sendHeader("200 OK", file_type,contentLength, connecting_socket);

			sendFile(fp, contentLength);

			free(filename);
			free(file_type);
			free(extension);
			free(path);

			fclose(fp);

			return 1;
		}
		else
		{
			sendString("501 Not Implemented\n", connecting_socket);
		}
	}
	
	return -1;
}

int getRequestType(char *input)
{
	// IF NOT VALID REQUEST 
	// RETURN -1
	// IF VALID REQUEST
	// RETURN 1 IF GET
	// RETURN 2 IF HEAD
	// RETURN 0 IF NOT YET IMPLEMENTED

	int type = -1;

	if ( strlen ( input ) > 0 )
	{
		type = 1;
	}

	char *requestType = malloc(5);

	scan(input, requestType, 0, 5);

	if ( type == 1 && strcmp("GET", requestType) == 0)
	{
		type = 1;
	}
	else if (type == 1 && strcmp("HEAD", requestType) == 0)
	{
		type = 2;
	}
	else if (strlen(input) > 4 && strcmp("POST", requestType) == 0 )
	{
		type = 0;
	}
	else
	{
		type = -1;
	}
	return type;
}

int receive(int socket)
{
	int msgLen = 0;
	char buffer[BUFFER_SIZE];

	memset (buffer,'\0', BUFFER_SIZE);

	if ((msgLen = recv(socket, buffer, BUFFER_SIZE, 0)) == -1)
	{
		printf("Error handling incoming request");
		return -1;
	}

	int request = getRequestType(buffer);

	if ( request == 1 )				// GET
	{
		handleHttpGET(buffer);
	}
	else if ( request == 2 )		// HEAD
	{
		// SendHeader();
	}
	else if ( request == 0 )		// POST
	{
		sendString("501 Not Implemented\n", connecting_socket);
	}
	else							// GARBAGE
	{
		sendString("400 Bad Request\n", connecting_socket);
	}

	return 1;
}

/**
	Create a socket and assign current_socket to the descriptor
**/
void createSocket()
{
	current_socket = socket(AF_INET, SOCK_STREAM, 0);

	if ( current_socket == -1 )
	{
		perror("Create socket");
		exit(-1);
	}
}

/**
	Bind to the current_socket descriptor and listen to the port in PORT
**/
void bindSocket()
{
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(13800);

	if ( bind(current_socket, (struct sockaddr *)&address, sizeof(address)) < 0 )
	{
		perror("Bind to port");
		exit(-1);
	}
}

/**
	Start listening for connections and accept no more than MAX_CONNECTIONS in the Quee
**/
void startListener()
{
	if ( listen(current_socket, MAX_CONNECTIONS) < 0 )
	{
		perror("Listen on port");
		exit(-1);
	}
}

/**
Handles the current connector
**/
void handle(int socket)
{
	// --- Workflow --- //
	// 1. Receive ( recv() ) the GET / HEAD
	// 2. Process the request and see if the file exists
	// 3. Read the file content
	// 4. Send out with correct mine and http 1.1

	if (receive((int)socket) < 0)
	{
		perror("Receive");
		exit(-1);
	}
}

void acceptConnection()
{
	char *path = (char*)malloc(1000 * sizeof(char));
	

	int contentLength = 0;
	int pid; 
	// signal(SIGCHLD, SIG_IGN);

	// int child_process = fork();

	addr_size = sizeof(connector);

	connecting_socket = accept(current_socket, (struct sockaddr *)&connector, &addr_size);


	if ( connecting_socket < 0 )
	{
		perror("Accepting sockets");
		exit(-1);
	}

	
	pid = fork();
  	if (pid < 0)
   		error("ERROR on fork");
  	if (pid == 0) {
   		close(current_socket);
   		handle(connecting_socket);
   		exit(0);
  	} else{
		close(connecting_socket);
	}
 
	close(connecting_socket);	
}

void init()
{
	char* currentLine = malloc(100);
	fileType_file = malloc(600);

	// Setting default values
	strcpy(fileType_file, "files.types");
		
	
}

int main(int argc, char* argv[])
{
	int parameterCount;
	char* fileExt = malloc(10);
	char* file_type = malloc(800);

	init();

	createSocket();

	bindSocket();

	startListener();

	while ( 1 )
	{
		acceptConnection();
	}
	
	
	return 0;
}

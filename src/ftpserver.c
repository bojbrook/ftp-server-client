#include<stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>


#define BUFFER_SIZE 			1024
#define FILE_COMMAND 			"\\filename"
#define OK_COMMAND 				"200 Command OK"
#define OK_GOODBYE 				"221 Goodbye."
#define NOT_VALID_COMMAND "500 Syntax error (unrecognized command)"
#define QUIT 							"221 Goodbye."
#define NO_FILE_COMMAND 	"450 No such file or directory"
#define FILE_ERROR				"452 Error writing file."
#define TAG								"ftpserver:"

enum commandType {
	LS_COMMAND = 0,
	GET_COMMAND,
	PUT_COMMAND,
	QUIT_COMMAND
};

typedef struct{
	int client_socket;
}server_struct;


typedef struct {
	int data_socket;
	char * filename;
	enum commandType command;
	FILE *fp;
	int fsize;
}data_layer;

int get_file_size(FILE *fp){
	fseek(fp, 0, SEEK_END);
	int file_size = ftell(fp);
	rewind(fp);
	return file_size;
}

/*writes the buffer to file*/
void client_write_to_file(FILE * fp,char *buffer,int file_size){
	int count = 0;
	rewind(fp);
	while(count < file_size){
		fputc(*buffer,fp);
		buffer++;
		count++;
	}
}

int write_put_to_file(int socket,FILE * fp,int fsize){
	char buffer[fsize];
	int value;
	if((value = read(socket, buffer, fsize)) > 0){
		buffer[value] = 0;
		//break;
	}
	client_write_to_file(fp,buffer,fsize);
	return 2;
}

/*CREATES SOCKET FOR THE data communication*/
int server_data_socket(int port, struct in_addr sin_addr){
		struct sockaddr_in serverAddr;
		int sockfd = socket(AF_INET,SOCK_STREAM,0);
		memset(&serverAddr, '0', sizeof(serverAddr));
		serverAddr.sin_family = AF_INET;
		serverAddr.sin_port = htons(port);
		serverAddr.sin_addr = sin_addr;

		int x = connect(sockfd, (struct sockaddr *) &serverAddr,sizeof(serverAddr));
		if(x < 0 && !(errno & EALREADY)){
			printf("%s\n", "Socket Error, skipping server");
			return -1;
		}
		return sockfd;
}

/*CREATES SOCKET FOR THE INTERFACE*/
int create_server_socket(struct sockaddr_in address, int port){
	int sock = -1;
	int addressLen = sizeof(address);
	sock = socket(AF_INET, SOCK_STREAM, 0);
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(port);
	if((bind(sock, (struct sockaddr *) &address,addressLen))){
		if(!(errno & EADDRINUSE))
			return -1;
	}
	if(listen(sock, 5) == 0){
		printf("%s at Port: %d\n", "Listening", port);
	}else{
		printf("%s\n", "error for Listening");
	}

	return sock;
}

/*Server thread that sends over the data content*/
void *thread_data_layer(void* DATA){
	data_layer *dl = DATA;
	char send_buffer[1024];
	int file_size,value;
	int socket = dl->data_socket;
	FILE *fp;
	//could change this so that put uses it also
	if(dl->command != PUT_COMMAND){
		fp = fopen(dl->filename,"r");
		if(fp == NULL && dl->command != QUIT_COMMAND){
			printf("%s %s\n", TAG, "Something is very wrong");
			return NULL;
		}
	}
	switch(dl->command){
		case LS_COMMAND:
			file_size = get_file_size(fp);
			sprintf(send_buffer, "\\ls %d",file_size);
			break;
		case GET_COMMAND:
			file_size = get_file_size(fp);
			sprintf(send_buffer, "\\get %d %s",file_size, dl->filename);
			break;
		case PUT_COMMAND:
			sprintf(send_buffer, "\\put %d %s",dl->fsize, dl->filename);
			break;
		case QUIT_COMMAND:
			write(socket,"\\quit",strlen("\\quit"));
			return NULL;
		default:
			printf("%s\n", "Something went wrong");
			return NULL;
	}

	printf("%s\n", send_buffer);
	write(socket,send_buffer,strlen(send_buffer));
	if(dl->command != PUT_COMMAND){
		char ch;
		char file_buffer[file_size];
		int count = 0;
		while((ch = fgetc(fp)) != EOF){
				file_buffer[count] = ch;
				count++;
		}
		write(socket,file_buffer,file_size);
		fclose(fp);
	}else{
		if(write_put_to_file(socket,dl->fp,dl->fsize) < 0)
				printf("%s\n", "Something wrong with file making");
		fclose(dl->fp);
	}
	return NULL;
}

/*CHILD THREAD FOR SERVER*/
void *thread_child_server(void* server){

	int value, file_size;
	char bufferin[BUFFER_SIZE];
	char command[20];
	char args[50];
	struct sockaddr_in address;
	int addressLen = sizeof(address);
	server_struct *Server = server;
	int client_socket = Server->client_socket;
	data_layer data;
	data.data_socket = -1;
	

	/*Main loop that handles request from the client*/
	while(1){
		if((value = read(client_socket, bufferin, BUFFER_SIZE)) > 0){
			bufferin[value] = 0;
		}else{
			continue;
		}
		sscanf(bufferin,"%s %s %d",command,args,&file_size);
		printf("Command: %s args: %s fsize: %d\n", command,args,file_size);

		pthread_t data_thread;
		data_layer dl;
		dl.data_socket = data.data_socket;
		dl.filename = args;

		//checks for the data port command
		if(strcmp(command,"\\Port") == 0){
			int data_port= atoi(args);
			data.data_socket = server_data_socket(data_port,address.sin_addr);
			if(data.data_socket < 0){
				printf("%s\n", "ERROR setting up data socket");
				return NULL;
			}
			else{
				printf("%s %s\n",TAG,"conected to data layer");
				memset(&command, '0', sizeof(command));
				memset(&args, '\0', sizeof(args));
				continue;
			}
		}
		//checks for the ls command
		else if(strcmp(command, "\\ls") == 0 && data.data_socket > 0){
			// FILE *fp = fopen(args,"r");
			// if(fp == NULL && args[0] != 0){
			// 	printf("%s %s\n",TAG, "file dosen't exist" );
			// 	write(client_socket, NO_FILE_COMMAND ,strlen(NO_FILE_COMMAND));
			// 	continue;
			// }
			sprintf(command,"ls > output.txt");
			system(command);
			dl.filename = "output.txt";
			dl.command = LS_COMMAND;
		} 
		//handles the get command from the client
		else if(strcmp(command, "\\get") == 0 && data.data_socket > 0){
			FILE *fp = fopen(args,"r");
			if(fp == NULL){
				printf("%s %s\n",TAG, "file dosen't exist" );
				write(client_socket, NO_FILE_COMMAND ,strlen(NO_FILE_COMMAND));
				continue;
			}
			dl.fp = fp;
			dl.command = GET_COMMAND;
		}
		//handles the put command from the client
		else if(strcmp(command, "\\put") == 0 && data.data_socket > 0){
			FILE *fp = fopen(args,"w");
			if(fp == NULL){
				printf("%s %s\n",TAG, "Problem creating file" );
				write(client_socket,FILE_ERROR ,strlen(FILE_ERROR));
			}
			dl.fp = fp;
			dl.fsize = file_size;
			dl.command = PUT_COMMAND;		
		}
		//handles the quit command from the client
		else if(strcmp(command, "\\quit") == 0 && data.data_socket > 0){	
			dl.command = QUIT_COMMAND;
			//starting thread to send data over
			if(pthread_create(&data_thread, NULL, thread_data_layer, &dl)!= 0) {
				printf("Error creating thread\n");
				return NULL;
			}		
			write(client_socket,QUIT,strlen(QUIT));	//writing the open command
			if(pthread_join(data_thread, NULL)!= 0) {
			fprintf(stderr, "Error joining thread\n");
			return NULL;
			}	
			break;
		}else{
			//unrecognized command
			write(client_socket,NOT_VALID_COMMAND,strlen(NOT_VALID_COMMAND));
			continue;
		}
		//starting thread to send data over
		if(pthread_create(&data_thread, NULL, thread_data_layer, &dl)!= 0) {
			printf("Error creating thread\n");
			return NULL;
		}		
		write(client_socket,OK_COMMAND,strlen(OK_COMMAND));	//writing the open command
		if(pthread_join(data_thread, NULL)!= 0) {
		fprintf(stderr, "Error joining thread\n");
		return NULL;
		}
		memset(&command, '0', sizeof(command));
		memset(&args, '\0', sizeof(args));
	}
	return NULL;
 }


int main(int argc, char const *argv[]){
	if(argc < 1){
		printf("%s\n", "ERROR: not enough arguments");
		return 1;		
	}
	int listen_socket,client_socket;
	struct sockaddr_in address;
	int addressLen = sizeof(address);
	
	const int Port = atoi(argv[1]);

	/*CREATING LISTENING SOCKET*/
	listen_socket = create_server_socket(address,Port);
	if(listen_socket < 0){
		printf("%s\n", "ERROR creating socket");
		return 0;
	}
	/*MAIN LOOP*/
	int value;
	char bufferin[BUFFER_SIZE];
	char command[20];
	char args[50];
	pthread_t t;
	
	/*MAIN LOOP of the program*/
	while(1){
		client_socket = accept(listen_socket, (struct sockaddr*) &address, (socklen_t*)&addressLen);
		server_struct *server = malloc(sizeof(server_struct));
		server->client_socket = client_socket;
		if(pthread_create(&t, NULL, thread_child_server, server)!= 0) {
				printf("Error creating thread\n");
				return 1;
		}		

	}

	
	return 0;
}
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <errno.h>
#include <pthread.h>

// Library for all the fpt commands
#include "ftpcmd.h"

#define PROMPT 				"ftp> "
#define DATAPORT 			3434
#define BUFFERIN_SIZE 1024
#define TAG						"ftpclient:"


typedef struct {
	const char * Address;
	int Port;
}interface_layer_info;

typedef struct{
	const char * IP;
	int Port;
	int Socket;
	struct sockaddr_in address;
	pthread_mutex_t *mutex;
}data_layer_info;

/*Get the size of a file. fp must be a non void file pointer*/
int get_file_size(FILE *fp){
	fseek(fp, 0, SEEK_END);
	int file_size = ftell(fp);
	rewind(fp);
	return file_size;
}

/*CREATES SOCKET FOR THE INTERFACE*/
int create_interface_socket(interface_layer_info *info){
		struct sockaddr_in serverAddr;
		int sockfd = socket(AF_INET,SOCK_STREAM,0);
		memset(&serverAddr, '0', sizeof(serverAddr));
		serverAddr.sin_family = AF_INET;
		serverAddr.sin_port = htons(info->Port);

		// Convert IPv4 and IPv6 addresses from text to binary form
		if(inet_pton(AF_INET, info->Address, &serverAddr.sin_addr)<=0) 
		{
	      printf("Invalid address or Address not supported \n");
	      return -1;
		}

		int x = connect(sockfd, (struct sockaddr *) &serverAddr,sizeof(serverAddr));
		if(x < 0 && !(errno & EALREADY)){
			printf("%s\n", "Socket Error, skipping server");
			return -1;
		}
		return sockfd;
}

/*CREATES SOCKET FOR THE DATA*/
int client_data_socket(data_layer_info *data){
	int sock = -1;
	struct sockaddr_in address = data->address;
	int addressLen = sizeof(address);
	sock = socket(AF_INET, SOCK_STREAM, 0);
	address.sin_family = AF_INET;
	// Setting the data to only listen to this IP address
	address.sin_addr.s_addr = INADDR_ANY;
	// if(inet_pton(AF_INET, data->IP, &address.sin_addr)<=0) 
	// {
 //      printf("Invalid address or Address not supported \n");
 //      return -1;
	// }
	address.sin_port = htons(data->Port);
	if((bind(sock, (struct sockaddr *) &address,addressLen))){
		if(!(errno & EADDRINUSE))
			return -1;
	}
	if(listen(sock, 1) == 0){
		printf("%s %s at Port: %d\n",TAG ,"Data layer listening", data->Port);
	}else{
		printf("%s\n", "error for Listening");
	}
	return sock;
}

/*writes the buffer to file*/
void client_write_to_file(char * file_name,char *buffer,int file_size){
	FILE *fp = fopen(file_name, "w");
	int count = 0;
	while(count < file_size){
		fputc(*buffer,fp);
		buffer++;
		count++;
	}
	fclose(fp);
}


/*DATA level thread
This is the thread that will handle all of the transfers*/
void *thread_child_data_layer(void* Info){
	data_layer_info *data = (data_layer_info*)Info;
	struct sockaddr_in address = data->address;
	int addressLen = sizeof(address);
	int value;
	int file_size = -1;
	char command[20];
	char file_name[20];
	int data_socket = accept(data->Socket, (struct sockaddr*) &address, (socklen_t*)&addressLen);
	printf("%s %s\n",TAG ,"Client connected to data channel");
	/*Accepting the connection from the server*/
	char recv_buffer[1024];


	for(;;){
		pthread_mutex_lock(data->mutex);
		if((value = read(data_socket, recv_buffer, 1024)) > 0){
			recv_buffer[value] = 0;
			sscanf(recv_buffer,"%s %d %s",command,&file_size,file_name);
			char printbuffer[file_size];
			if(strcmp(command,"\\ls") == 0){
				client_read_command(data_socket,printbuffer,file_size);
				printf("%s", printbuffer);
			}else if (strcmp(command,"\\get") == 0){
				client_read_command(data_socket,printbuffer,file_size);
				printf("%s\n", "Transfer Complete");
				client_write_to_file(file_name,printbuffer,file_size);		
			}else if(strcmp(command,"\\put") == 0){
				FILE *fp = fopen(file_name, "r");
				if(fp == NULL){
					printf("%s\n", "error with file");
					continue;
				}
				char ch;
				char file_buffer[file_size];
				int count = 0;
				while((ch = fgetc(fp)) != EOF){
						file_buffer[count] = ch;
						count++;
				}
				write(data_socket,file_buffer,file_size);
				fclose(fp);
			}else if(strcmp(command,"\\quit") == 0){
				pthread_mutex_unlock(data->mutex);
				return NULL;
			}
			memset(&recv_buffer, '0', sizeof(recv_buffer));
			memset(&command, '0', sizeof(command));	
			file_size = 0;
			pthread_mutex_unlock(data->mutex);
		}
		
	}
	return NULL;
}

int main(int argc, char const *argv[])
{
	interface_layer_info server;
	if(argc > 2){
		const char *IP = argv[1];
		const int Port = atoi(argv[2]);
		server.Address = IP;
		server.Port = Port;
	}else{
		printf("%s\n", "ERROR: not enough arguments");
		return 1;
	}


	
	int value;
	int retVal;
	int file_size;
	int server_socket;
	char command[20];
	char args[50];
	
	/*Creating  the interface socket*/
	server_socket = create_interface_socket(&server);
	if(server_socket < 0){
		printf("%s\n", "Error Connecting to server");
		return 1;
	}
	printf("%s %s\n",TAG,"CONNECTED TO SERVER...");
	data_layer_info data;
	data.IP = server.Address;
	data.Port = DATAPORT;
	
	//making sure the port is open
	while((data.Socket = client_data_socket(&data)) < 0){
		data.Port++;
	}

	//lock for the printing of the threads
	pthread_mutex_t m	=	PTHREAD_MUTEX_INITIALIZER;
	data.mutex = &m;

	//starting data thread
	pthread_t t;
	if(pthread_create(&t, NULL, thread_child_data_layer, &data)!= 0) {
		fprintf(stderr, "Error creating thread\n");
		return 1;
	}

	//writing over the data port
	client_send_data_port(server_socket,data.Port);
	char write_buffer[1024];
	char recv_buffer[100];

	/*MAIN LOOP*/
	while(1){		
		memset(&command, '0', sizeof(command));
		memset(&args, '\0', sizeof(args));
		file_size = 0;
		pthread_mutex_lock(&m);	
		char * line = readline (PROMPT);
		sscanf(line,"%s %s",command,args);
		
		if(strcmp(command,"quit") == 0){
			write(server_socket,"\\quit",strlen("\\quit"));
			if((value = read(server_socket, recv_buffer, 100)) > 0){
				recv_buffer[value] = 0;
				printf("%s\n", recv_buffer);
			}
			pthread_mutex_unlock(&m);	
			break;
		}		
		else if(strcmp(command,"put") == 0){	
			FILE *fp = fopen(args,"r");
			if(fp == NULL){
				printf("%s\n", "Error: client has so file");
				pthread_mutex_unlock(&m);	
				continue;
			}
			file_size = get_file_size(fp);
			fclose(fp);
		}
		//sending over the command
		sprintf(write_buffer,"\\%s %s %d",command,args,file_size);
		write(server_socket,write_buffer,strlen(write_buffer));

		//check the status code from the server
		if(check_status_code(server_socket,recv_buffer) < 0){
				printf("%s\n", recv_buffer);
				continue;
		}
		pthread_mutex_unlock(&m);	
		// memset(&command, '0', sizeof(command));
		// memset(&args, '\0', sizeof(args));
	}
 
 	//this is incase the data thread is still transfering
	if(pthread_join(t, NULL)!= 0) {
		fprintf(stderr, "Error joining thread\n");
		return 0;
	}
	return 0;
}
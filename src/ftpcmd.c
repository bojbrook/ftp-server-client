#include<stdio.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/types.h>

#include "ftpcmd.h"





void client_send_data_port(int socket, int port){
	char bufferout[50];
	sprintf(bufferout,"\\Port %d", port);
	write(socket,bufferout,strlen(bufferout));
}

/*reads in the ls command from the server*/
void client_read_command(int socket,char *print_buffer, int fsize){
		char readbuffer[fsize];
		int value;
		if((value = read(socket, readbuffer, fsize)) > 0){
			readbuffer[value] = 0;
		}
		strcpy(print_buffer,readbuffer);
}

/*QUITS CLIENT SIDE*/
void client_quit(int socket){
	char bufferin[45];
	int value;
	while(strcmp(bufferin, QUIT_COMMAND) != 0){
		write(socket,"QUIT",strlen("QUIT"));
		if((value = read(socket, bufferin, 45)) > 0){
			bufferin[value] = 0;
		}
	}	
	printf("%s\n", bufferin);
}

/*ASKS SERVER FOR FILE*/
int client_get_file(int socket, char* filename){
	char bufferin[1024];
	char bufferout[50];
	int value;
	sprintf(bufferout,"\\filename %s", filename);
	printf("%s Buffer: %s\n",CMDTAG ,bufferout);
	write(socket,bufferout,strlen(bufferout));
	if((value = read(socket, bufferin, 45)) > 0){
			bufferin[value] = 0;
	}
	printf("%s %s\n",CMDTAG ,bufferin);
	return 1;
}

int check_status_code(int socket,char*buffer){
	int value;
	if((value = read(socket, buffer, 100)) > 0){
		*(buffer+value) = 0;
		if(*buffer == '2'){
			return 1;
		}
	}
	return -1;
}






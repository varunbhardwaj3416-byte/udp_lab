#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <stdint.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/time.h>
#include "packet.h"
#define PORT_NUMBER 8888

int main(){
	// Create a client 
	struct sockaddr_in server_addr ; 
	// create a socket 
	int sockfd ; 
	sockfd = socket(AF_INET , SOCK_DGRAM , IPPROTO_UDP) ; 
	if(sockfd < 0 ) {
		printf("Socket creation failed\n") ; 
		return 1 ; 
	}
	server_addr.sin_family = AF_INET ; 
	server_addr.sin_port = htons(PORT_NUMBER) ; 
	server_addr.sin_addr.s_addr = inet_addr("127.0.0.1") ; 
	
	char fileName[100] ; 
	scanf("%s" , fileName) ; 
	sendto(sockfd , fileName , strlen(fileName) + 1 , 0 , (struct sockaddr *)&server_addr , sizeof(server_addr)) ; 
	// send file size 
	FILE * fp = fopen(fileName , "rb") ; 
	if(!fp){
	printf("File does not exists\n") ; 
	return 1 ; 
	}
	fseek(fp , 0 , SEEK_END) ; 
	long file_size = ftell(fp) ; 
	fseek(fp , 0 , SEEK_SET) ; 
	printf("Sending file data...\n") ; 
	sendto(sockfd , &file_size , sizeof(file_size) , 0 , (struct sockaddr *)&server_addr , sizeof(server_addr)) ; 
	// sending file content 
	char buffer[1024] ; 
	int bytes ; 
	while((bytes = fread(buffer , 1 , 1024 , fp)) > 0) {
		sendto(sockfd , buffer , bytes , 0 , (struct sockaddr *)&server_addr , sizeof(server_addr)) ;
		memset(buffer , 0 , sizeof(buffer)) ;  
	}

	sendto(sockfd , "__END__", 7 , 0 , (struct sockaddr *)&server_addr , sizeof(server_addr)) ;

	// ---------- ACK-BASED RELIABILITY ----------
	// TODO : add timeout using select()
	// TODO : wait for ack
	// TODO : resend packet on timeout 

	
	
	
	
	
	
	
	
	
	
	
	
	
	
	
	 
	close(sockfd) ; 
	fclose(fp) ; 
	return 0 ; 
 
}

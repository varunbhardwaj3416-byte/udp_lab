#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include "packet.h"
#define PORT 8888


int main(){
	int sockfd ; 
	sockfd = socket(AF_INET , SOCK_DGRAM , IPPROTO_UDP) ; 
	if (sockfd < 0) {
		printf("Socket not created \n") ;
		return 1;
	} 
	struct sockaddr_in server_in ; 
	memset(&server_in , 0 , sizeof(server_in)) ; 
	server_in.sin_family = AF_INET ; 
	server_in.sin_port = htons(PORT) ; 
	server_in.sin_addr.s_addr = htonl(INADDR_ANY) ; 
	if(bind(sockfd , (struct sockaddr *)&server_in , sizeof(server_in)) < 0) {
		close(sockfd) ; 
		perror("Binding failed\n") ; 
		return 1 ; 
	}
	printf("Binding successfull\nWaiting for file name...\n") ; 
	char buffer[1024] ; 
	struct sockaddr_in client_addr ; 
	socklen_t client_len = sizeof(client_addr) ; 
	// recv_from gives how many bytes were recieved 
	int recv_from = recvfrom(sockfd , buffer , sizeof(buffer) - 1  , 0 , (struct sockaddr *)&client_addr , &client_len) ;
	if(recv_from < 0) {
		perror("Not recieved") ; 
		return 0 ; 
	}
	buffer[recv_from] = '\0' ; 
	printf("Client wants to send file : %s\n" , buffer) ; 	
	long file_size ; 
	int recv_from_file_size = recvfrom(sockfd , &file_size , sizeof(file_size) , 0 , (struct sockaddr *)&client_addr , &client_len) ; 
	if(recv_from_file_size < 0) {
	perror("File size not recieved :")  ; 
	}
	else {
	printf("File size : %ld bytes\n" , file_size) ; 
	}
	FILE *fp = fopen("udp_transfered_file" , "wb") ; 
	if(!fp) {
	printf("File not created\n") ; 
	}
	else {
		memset(buffer , 0 , sizeof(buffer)) ; 
		int len ; 
		while(1) {
		len = recvfrom(sockfd , buffer , sizeof(buffer) , 0 , (struct sockaddr *)&client_addr , &client_len) ; 
		if (len == 7 && memcmp(buffer , "__END__" , 7) == 0) {
		break ; 
		}
		fwrite(buffer , 1 , len , fp) ; 
		}
	}

	// ---------------- PHASE 2: ACK-BASED RELIABILITY ----------------
	// TODO: send ACK after receiving each packet

	
	
	
	
	
	
	
	
	
	
	
	fclose(fp) ; 
	close(sockfd) ; 
	return 0 ;
}

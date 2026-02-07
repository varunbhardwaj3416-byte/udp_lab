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
#define BUFFER_SIZE 1024

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
	/*
	char buffer[1024] ; 
	int bytes ; 
	while((bytes = fread(buffer , 1 , 1024 , fp)) > 0) {
		sendto(sockfd , buffer , bytes , 0 , (struct sockaddr *)&server_addr , sizeof(server_addr)) ;
		memset(buffer , 0 , sizeof(buffer)) ;  
	}

	sendto(sockfd , "__END__", 7 , 0 , (struct sockaddr *)&server_addr , sizeof(server_addr)) ; */

	// ---------- ACK-BASED RELIABILITY ----------
	// TODO : add timeout using select()
	// TODO : wait for ack
	// TODO : resend packet on timeout 

	// send 1 packet ;
	DataPacket file_data_packet ; 
	file_data_packet.seq_no = 1 ; 
	long data_len = fread(file_data_packet.buffer , 1 , BUFFER_SIZE , fp) ; 
	file_data_packet.data_len = data_len; 
	// read the file upto BUFFER_SIZE 
	int send_to = sendto(sockfd , &file_data_packet , sizeof(file_data_packet) , 0 , (struct sockaddr *)&server_addr , sizeof(server_addr)) ; 
	if(send_to < 0) {
	perror("Data packet not sent") ; 
	return 1 ; 
	}
	uint32_t ack_no ; 
	socklen_t server_addr_len = sizeof(server_addr) ; 
	int recv_from = recvfrom(sockfd , &ack_no , sizeof(ack_no) , 0 , (struct sockaddr *)&server_addr , &server_addr_len) ; 
	if (recv_from < 0) {
	perror("Acknowledgement not recieved") ; 
	return 1 ; 
	}
	ack_no = ntohl(ack_no) ; 
	if(ack_no == 1){
	printf("Acknowledgement recieved , Packet-1 Data written to file\n") ; 
	}

	 
	close(sockfd) ; 
	fclose(fp) ; 
	return 0 ; 
 
}

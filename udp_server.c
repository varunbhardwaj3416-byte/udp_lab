#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/select.h>
#include "packet.h"
#define PORT 8888


int main(){
	char fileName[100] ; 
	struct sockaddr_in client_addr ; 
	socklen_t client_len = sizeof(client_addr) ; 
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
	printf("Waiting for file...\n") ; 
	handshake handshake_packet ; 
	int recv_handshake = recvfrom(sockfd , &handshake_packet , sizeof(handshake_packet) , 0, (struct sockaddr *)&client_addr , &client_len) ; 
	if(recv_handshake < 0) {
	perror("Error") ; 
	}
	else {
	if(ntohl(handshake_packet.decision) == 0){
		printf("Client doesn't want to share the file\n") ; 
		return 1 ; 
	}
	else {
		AckPacket ack_packet ; 
		printf("Client want's to send a file\n") ;  
		printf("Enter 1 for Yes or 2 for No : \n") ; 
		int choice ; 
		scanf("%d" , &choice) ; 
		if(choice == 2) {
			handshake_packet.decision = htonl(0) ; 
			while(1){
				if(sendto(sockfd , &handshake_packet , sizeof(handshake_packet) , 0 , (struct sockaddr *)&client_addr , sizeof(client_addr)) > 0) exit(0) ; // see the port for 4 seconds 
			}
		}
		printf("Enter the file name \n") ; 
		scanf("%s" , fileName) ;  
		handshake_packet.decision = htonl(1) ; 
		while(1){
		if(sendto(sockfd , &handshake_packet , sizeof(handshake_packet) , 0 , (struct sockaddr *)&client_addr , sizeof(client_addr)) > 0) break ;
		}
		while(1) {
		
			// sent the ack 
			struct timeval handshake_timeout ; 
			handshake_timeout.tv_sec = 4 ; 
			handshake_timeout.tv_usec = 0 ; 
			fd_set readfd ; 
			FD_ZERO(&readfd) ; 
			FD_SET(sockfd , &readfd) ; 
			int handshake_ack = select(sockfd + 1 , &readfd , NULL , NULL , &handshake_timeout) ; 
			if (handshake_ack > 0) {
    			handshake recv_hs;
    			handshake send_hs ; 
    			recvfrom(sockfd, &recv_hs, sizeof(recv_hs), 0, (struct sockaddr *)&client_addr, &client_len);
    if (ntohl(recv_hs.decision) == 1) {
        printf("Valid handshake retry received\n");

        send_hs.decision = htonl(1);
        sendto(sockfd, &send_hs, sizeof(send_hs), 0,
               (struct sockaddr *)&client_addr,
               sizeof(client_addr));
    } else {
        // Not a handshake retry → ignore
        break ; 
    }
}

			if (handshake_ack == 0) break ; 
		}
	}
	}
	//printf("Binding successfull\nWaiting for file name...\n") ; 
	char buffer[1024] ; 
	// recv_from gives how many bytes were recieved 
	int recv_from = recvfrom(sockfd , buffer , sizeof(buffer) - 1  , 0 , (struct sockaddr *)&client_addr , &client_len) ;
	if(recv_from < 0) {
		perror("Not recieved") ; 
		return 0 ; 
	}
	buffer[recv_from] = '\0' ; 
	printf("Recieving file %s\n" , buffer) ; 
	long file_size ; 
	int recv_from_file_size = recvfrom(sockfd , &file_size , sizeof(file_size) , 0 , (struct sockaddr *)&client_addr , &client_len) ; 
	if(recv_from_file_size < 0) {
	perror("File size not recieved :")  ; 
	}
	else {
	printf("File size : %ld bytes\n" , file_size) ; 
	}
	//printf("Enter name for the file -> ") ; 
	//char file_name[100] ; 
	//scanf("%s" , file_name) ; 
	FILE *fp = fopen(fileName , "wb") ; 
	if(fp == NULL) {
		printf("File not opened\n") ; 
		return 1 ; 
	}
	/*if(!fp) {
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
	}*/

	// ---------------- PHASE 2: ACK-BASED RELIABILITY ----------------
	// TODO: send ACK after receiving 1 packet
	
	/*DataPacket file_data_packet ; 
	recv_from = recvfrom(sockfd , &file_data_packet , sizeof(file_data_packet) , 0 , (struct sockaddr *)&client_addr , &client_len) ;
	if(recv_from < 0) {
	perror("Not recieved") ; 
	return 1 ; 
	}
	else {
	// send ack packet 
	AckPacket ack_packet ; 
	ack_packet.ack_no = htonl(1) ; 
	int send_to = sendto(sockfd , &ack_packet , sizeof(ack_packet) , 0 , (struct sockaddr*)&client_addr , sizeof(client_addr)) ; 
	if(send_to < 0) {
		perror("Acknowledgement not sent ") ; 
		return 1 ; 
	}
	else{
	printf("Acknowledgement sent\n") ; 
	fwrite(file_data_packet.buffer , 1 , file_data_packet.data_len , fp) ;  
	}
	}*/
	DataPacket data_packet ; 
	AckPacket ack_packet ; 
	uint32_t packet_number ; 
	uint32_t no_of_packet_recieved = 0 ;  
	int send_to ; 
	int endPacketRecieved = 0 ; 
	fd_set readfd ; 
	FD_ZERO(&readfd) ; 
	FD_SET(sockfd , &readfd) ;  
	int packet_on_socket = 1 ; 
	while(packet_on_socket > 0 && recvfrom(sockfd , &data_packet , sizeof(data_packet) , 0 , (struct sockaddr *)&client_addr , &client_len) > 0) {
		FD_ZERO(&readfd) ; 
		FD_SET(sockfd , &readfd) ; 
		struct timeval timeout ; 
		timeout.tv_sec = 2 ; 
		timeout.tv_usec = 0 ;
		if(endPacketRecieved && data_packet.seq_no == no_of_packet_recieved){
			// send again the ack ; 
		ack_packet.ack_no = htonl(no_of_packet_recieved) ; 
		send_to = sendto(sockfd , &ack_packet , sizeof(ack_packet) , 0 , (struct sockaddr *)&client_addr , sizeof(client_addr)) ; 
		if(send_to < 0) {
		perror("Acknowledgement not sent") ; 
		}
		}
		// see the packet no 
		packet_number = ntohl(data_packet.seq_no) ; 
		if(packet_number == no_of_packet_recieved + 1 ){
		// got the next packet ; 
		if (data_packet.data_len <= 0) continue ; 
		no_of_packet_recieved += 1 ; 
		fwrite(data_packet.buffer , 1 , data_packet.data_len , fp) ; 
		fflush(fp) ; 
		ack_packet.ack_no = htonl(no_of_packet_recieved) ; 
		send_to = sendto(sockfd , &ack_packet , sizeof(ack_packet) , 0 , (struct sockaddr *)&client_addr , sizeof(client_addr)) ; 
		if(send_to < 0) {
		perror("Acknowledgement not sent") ; 
		}
		if(ntohl(data_packet.is_end) == 1){
		endPacketRecieved = 1 ; 
		}
		}
		else {
		// sent ack for packet recieved 
		ack_packet.ack_no = htonl(no_of_packet_recieved) ; 
		send_to = sendto(sockfd , &ack_packet , sizeof(ack_packet) , 0 , (struct sockaddr *)&client_addr , sizeof(client_addr)) ; 
		
		if(send_to < 0) {
		perror("error : ") ; 
		}
		}	
		packet_on_socket = select(sockfd + 1 , &readfd , NULL , NULL , &timeout) ; 
		}
		// didn't recieve the packet 
	printf("File recieved\n") ; 
	printf("Program terminated\n") ; 
	fclose(fp) ; 
	close(sockfd) ; 
	return 0 ;
}

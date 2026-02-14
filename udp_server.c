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

			printf("\n=====================================\n");
			printf("   File Transfer Request Received\n");
			printf("=====================================\n");

			printf("Client wants to send a file.\n");
			printf("Do you want to accept it?\n");
			printf("Enter 1 for Yes\n");
			printf("Enter 2 for No\n");
			printf("Your choice: ");

			int choice ; 
			scanf("%d" , &choice) ; 

			if(choice == 2) {
				handshake handshake_packet ;
				handshake_packet.decision = htonl(0) ; 
				while(1){
					if(sendto(sockfd , &handshake_packet , sizeof(handshake_packet) , 0 , (struct sockaddr *)&client_addr , sizeof(client_addr)) > 0) 
						exit(0) ;  // after sending ack see if client still sends packet
						//  meaning our packet got lost , write the code
				}
			}

			printf("Enter the file name \n") ; 
			scanf("%s" , fileName) ;  

			handshake_packet.decision = htonl(1) ; 
		while(1){
			if(sendto(sockfd , &handshake_packet , sizeof(handshake_packet) , 0 , (struct sockaddr *)&client_addr , sizeof(client_addr)) > 0) break ;
				}
			int handshake_ack = 1 ; 
			while(handshake_ack > 0) {
				struct timeval handshake_timeout ; 
				handshake_timeout.tv_sec = 5 ; 
				handshake_timeout.tv_usec = 0 ; 
				fd_set readfd ; 
				FD_ZERO(&readfd) ; 
				FD_SET(sockfd , &readfd) ; 
				handshake_ack = select(sockfd + 1 , &readfd , NULL , NULL , &handshake_timeout) ; 

				if (handshake_ack > 0) {
					handshake recv_hs;
					recvfrom(sockfd, &recv_hs, sizeof(recv_hs), 0, (struct sockaddr *)&client_addr, &client_len);

					if (ntohl(recv_hs.decision) == 1) {
						printf("Valid handshake retry received\n");
						// send again 
						while(1){
			if(sendto(sockfd , &handshake_packet , sizeof(handshake_packet) , 0 , (struct sockaddr *)&client_addr , sizeof(client_addr)) > 0) break ;
				}
				continue ; 
					} 
					else {
						break ; 
					}
				}
				if (handshake_ack == 0) 
					break ; 
			}

			char buffer[1024] ;
			int ack = 1 ; 
			int recv_from ; 

			while(ack > 0){
				handshake handshake_packet ; 
				recv_from = recvfrom(sockfd , buffer , sizeof(buffer) - 1  , 0 , (struct sockaddr *)&client_addr , &client_len) ;
				if(recv_from < 0) {
					perror("Not recieved") ; 
					return 1 ; 
				}

				handshake_packet.decision = htonl(1) ; 

				while(1){
					if(sendto(sockfd , &handshake_packet , sizeof(handshake_packet) , 0 , (struct sockaddr *)&client_addr , sizeof(client_addr)) > 0) 
						break ;  
				}

				fd_set readfd ; 
				FD_ZERO(&readfd) ; 
				FD_SET(sockfd , &readfd) ; 
				struct timeval ackTime ; 
				ackTime.tv_sec = 5 ; 
				ackTime.tv_usec = 0 ; 
				ack = select(sockfd + 1 , &readfd , NULL , NULL , &ackTime) ;

				if(ack == 0) 
					break ; 
			}

			buffer[recv_from] = '\0' ; 
			printf("Recieving file %s\n" , buffer) ; 

			uint32_t file_size ; 
			ack = 1 ; 

			while(ack > 0){
				handshake handshake_packet ; 
				int recv_from = recvfrom(sockfd , &file_size , sizeof(file_size) , 0 , (struct sockaddr *)&client_addr , &client_len) ;

				if(recv_from < 0) {
					perror("Not recieved") ; 
					return 1 ; 
				}

				handshake_packet.decision = htonl(1) ; 

				while(1){
					if(sendto(sockfd , &handshake_packet , sizeof(handshake_packet) , 0 , (struct sockaddr *)&client_addr , sizeof(client_addr)) > 0) 
						break ;  
				}

				fd_set readfd ; 
				FD_ZERO(&readfd) ; 
				FD_SET(sockfd , &readfd) ; 
				struct timeval ackTime ; 
				ackTime.tv_sec = 5 ; 
				ackTime.tv_usec = 0 ; 
				ack = select(sockfd + 1 , &readfd , NULL , NULL , &ackTime) ;

				if(ack == 0) 
					break ; 
			}

			file_size = ntohl(file_size) ; 
			printf("File size : %u bytes\n" , file_size) ;  

			FILE *fp = fopen(fileName , "wb") ; 
			if(fp == NULL) {
				printf("File not opened\n") ; 
				return 1 ; 
			}

			DataPacket data_packet ; 
			uint32_t packet_number ; 
			uint32_t no_of_packet_recieved = 0 ;  
			int send_to ; 
			int endPacketRecieved = 0 ; 
			fd_set readfd ; 
			int packet_on_socket = 1 ; 

			while(packet_on_socket > 0 && 
			      recvfrom(sockfd , &data_packet , sizeof(data_packet) , 0 , (struct sockaddr *)&client_addr , &client_len) > 0) {

				struct timeval timeout ; 
				timeout.tv_sec = 14 ; 
				timeout.tv_usec = 0 ;

				packet_number = ntohl(data_packet.seq_no) ; 

				if(packet_number == no_of_packet_recieved + 1 ){

					if (data_packet.data_len <= 0) 
						continue ; 

					no_of_packet_recieved += 1 ; 

					fwrite(data_packet.buffer , 1 , data_packet.data_len , fp) ; 
					fflush(fp) ; 

					ack_packet.ack_no = htonl(no_of_packet_recieved) ; 
					send_to = sendto(sockfd , &ack_packet , sizeof(ack_packet) , 0 , (struct sockaddr *)&client_addr , sizeof(client_addr)) ; 

					if(send_to < 0) 
						perror("Acknowledgement not sent") ; 

					if(ntohl(data_packet.is_end) == 1)
						endPacketRecieved = 1 ; 
				}
				else {

					ack_packet.ack_no = htonl(no_of_packet_recieved) ; 
					send_to = sendto(sockfd , &ack_packet , sizeof(ack_packet) , 0 , (struct sockaddr *)&client_addr , sizeof(client_addr)) ; 

					if(send_to < 0) 
						perror("error : ") ; 
				}

				FD_ZERO(&readfd) ; 
				FD_SET(sockfd , &readfd) ; 
				packet_on_socket = select(sockfd + 1 , &readfd , NULL , NULL , &timeout) ; 
			}

			printf("File recieved\n") ; 
			printf("Program terminated\n") ; 
			fclose(fp) ; 
			close(sockfd) ; 
			return 0 ;
		}
	}
}


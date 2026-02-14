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
	int choice ; 
	printf("\n==============================\n");
	printf("        UDP FILE CLIENT       \n");
	printf("==============================\n");
	printf("1. Transfer a file\n");
	printf("2. Exit\n");
	printf("------------------------------\n");
	printf("Enter your choice: ");

	scanf("%d" , &choice) ; 
	if(choice == 2) return 1 ;  
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
	socklen_t server_size = sizeof(server_addr) ; 
	char fileName[100] ; 
	printf("Enter the file name you want to transfer \n") ; 
	scanf("%s" , fileName) ; 
	// send handshake 
	handshake handshake_packet ; 
	handshake_packet.decision = htonl(1) ; // 1 = want to transfer the file
	// handshake request sent 
	while(1){
	if(sendto(sockfd , &handshake_packet , sizeof(handshake_packet) , 0 , (struct sockaddr *)&server_addr , sizeof(server_addr)) > 0){
		fd_set readfd ; 
		FD_ZERO(&readfd) ; 
		FD_SET(sockfd , &readfd) ;  
		struct timeval ackTime ; 
		ackTime.tv_sec = 3 ; 
		ackTime.tv_usec = 0 ; 
		int handshake_ack = select(sockfd + 1 , &readfd , NULL , NULL , &ackTime) ; 
			if(handshake_ack > 0){
				int recv_ack_packet = recvfrom(sockfd , &handshake_packet , sizeof(handshake_packet) , 0 , (struct sockaddr *)&server_addr , &server_size) ; // recv packet 
				if(recv_ack_packet > 0) break ;  
			}
	} 
	}
	if(ntohl(handshake_packet.decision) == 0) {
	printf("Handshaking rejected\n") ; 
	return 1 ; 
	}
	sleep(6) ;
// same logic for sending filename 
	while(1){
	if(sendto(sockfd , fileName , strlen(fileName) + 1 , 0 , (struct sockaddr *)&server_addr , sizeof(server_addr)) > 0) {
		//wait for ack 
		fd_set readfd ; 
		struct timeval ackTime ; 
		FD_ZERO(&readfd) ; 
		FD_SET(sockfd , &readfd) ; 
		ackTime.tv_sec = 1 ; 
		ackTime.tv_usec = 0 ; 
		int ack = select(sockfd + 1 , &readfd , NULL , NULL , &ackTime) ;
		if(ack > 0){
			// server will send ack for receiveing file name
			handshake h_packet ; 
			int recv_from = recvfrom(sockfd , &h_packet , sizeof(h_packet) , 0 , (struct sockaddr *)&server_addr , &server_size) ;
			if(recv_from > 0 && ntohl(h_packet.decision) == 1) {
			// handshake done 
			break ; 
			} 
		}  
	}
	}
	sleep(6) ; 
	// send file size 
	FILE * fp = fopen(fileName , "rb") ; 
	if(!fp){
	printf("File does not exists\n") ; 
	return 1 ; 
	}
	fseek(fp , 0 , SEEK_END) ; 
	uint32_t file_size = htonl(ftell(fp)) ;
	fseek(fp , 0 , SEEK_SET) ; 
	int ack  ; 
	while(1) {
	if(sendto(sockfd , &file_size , sizeof(file_size) , 0 , (struct sockaddr *)&server_addr , sizeof(server_addr)) > 0) {
		//wait for ack 
		fd_set readfd ; 
		struct timeval ackTime ; 
		FD_ZERO(&readfd) ; 
		FD_SET(sockfd , &readfd) ; 
		ackTime.tv_sec = 1 ; 
		ackTime.tv_usec = 0 ; 
		int ack = select(sockfd + 1 , &readfd , NULL , NULL , &ackTime) ;
		if(ack > 0){
			// server will send ack for receiveing file name
			handshake h_packet ; 
			int recv_from = recvfrom(sockfd , &h_packet , sizeof(h_packet) , 0 , (struct sockaddr *)&server_addr , &server_size) ;
			if(recv_from > 0 && ntohl(h_packet.decision) == 1) {
			// handshake done 
			break ; 
			} 
		}  
	}
	}
	sleep(6) ; 
	printf("\nStarting file transfer...\n");
	printf("----------------------------------\n");

	// bind client address to socket 
	
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
	/*DataPacket file_data_packet ; 
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
	}*/
	
	// sending file in packets
	int packet_number = 0 ;
	int current_packet = 0 ; 
	AckPacket ack_packet ; 
	while(1){
	DataPacket file_data_packet ; 
	file_data_packet.data_len = fread(file_data_packet.buffer , 1 , BUFFER_SIZE , fp) ; 
	if(file_data_packet.data_len == 0){
		printf("ACK received for Final Packet\n") ; 
		break ; 
		// end of file 
	}
	if(file_data_packet.data_len < BUFFER_SIZE) {
	file_data_packet.is_end = htonl(1) ; // end of file  
	current_packet += 1;
	file_data_packet.seq_no = htonl(current_packet);

	}
	else{
	current_packet += 1;
	file_data_packet.seq_no = htonl(current_packet);
	file_data_packet.is_end = htonl(0) ; 
	}
	// send the packet 
	int send_to = sendto(sockfd , &file_data_packet , sizeof(file_data_packet) , 0, (struct sockaddr *)&server_addr , sizeof(server_addr)) ; 
	if(send_to < 0) {
	perror("Error : ") ; 
	break ; 
	
	}
	else{
	// data packet sent 
	// wait for acknowledge 
	fd_set readfds ; 
	FD_ZERO(&readfds) ; 
	FD_SET(sockfd , &readfds) ; 
	struct timeval timeout ; 
	timeout.tv_sec = 2 ; 
	timeout.tv_usec = 0 ; 
	
	int ack_came = select(sockfd + 1 , &readfds , NULL , NULL , &timeout) ; 
	if(ack_came > 0){
		// got the acknowledgement 
		int recv_from = recvfrom(sockfd , &ack_packet ,sizeof(ack_packet) , 0 , (struct sockaddr *)&server_addr , &server_size ) ; 
		if(recv_from < 0) {
			perror("error :") ; 
			// resend the packet 
			current_packet -= 1 ; 
			fseek(fp , current_packet * BUFFER_SIZE , SEEK_SET) ; 
			continue ; 
		}
		else {
		// see acknowledge number if ack < packet sent . Start from loss packet onwards
			uint32_t ack_no = ntohl(ack_packet.ack_no) ; 
			if(ack_no == current_packet){
			packet_number = current_packet ; 
			}
			printf("ACK received for Packet %u\n", ack_no);
			if(ack_no == current_packet && ntohl(file_data_packet.is_end) == 1){
			// final ack received 
			printf("ACK for Final Packet received \n") ; 
			break ; 
			}
		}
	
	}
	else if(ack_came == 0){ // ack did not come send the packet again 
		printf("Timeout. Sending packet again.\n") ; 
		current_packet -= 1 ;
		fseek(fp , 0 , SEEK_SET) ;
		fseek(fp , current_packet * BUFFER_SIZE , SEEK_SET) ;  
		continue ; 
	}
	else {
		perror("error :") ; 
		break ; 
	}
	}
	}

	close(sockfd) ; 
	fclose(fp) ; 
	printf("File transfered. \n") ; 
	return 0 ; 
 
}

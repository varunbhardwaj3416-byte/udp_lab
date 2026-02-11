#ifndef PACKET_H
#define PACKET_H
#define MAX_SIZE 1024 
#include <stdint.h>

typedef struct {
	uint32_t seq_no ; 
	uint16_t data_len ; 
	uint8_t is_end ; 
	char buffer[MAX_SIZE] ; 
} DataPacket ; 

typedef struct{
	uint32_t ack_no ; 
} AckPacket ; 

typedef struct{
	uint32_t decision ; 
} handshake ; 






#endif 

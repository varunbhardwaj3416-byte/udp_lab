#ifndef SOCKET_H
#define SOCKET_H

#include <stdint.h>
#include <stddef.h>
#include <netinet/in.h>

typedef struct {
    int                fd;
    struct sockaddr_in local;
    struct sockaddr_in remote;
} UdpSocket;

int  sock_create_sender(UdpSocket *s, const char *dest_ip, uint16_t dest_port);
int  sock_create_receiver(UdpSocket *s, uint16_t bind_port);
int  sock_send(UdpSocket *s, const uint8_t *buf, size_t len);
int  sock_recv(UdpSocket *s, uint8_t *buf, size_t buf_len, long timeout_ms);
void sock_close(UdpSocket *s);

#endif /* SOCKET_H */

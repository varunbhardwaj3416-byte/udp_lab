#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include "socket.h"

int sock_create_sender(UdpSocket *s, const char *dest_ip, uint16_t dest_port)
{
    s->fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (s->fd < 0) { perror("socket"); return -1; }

    memset(&s->remote, 0, sizeof(s->remote));
    s->remote.sin_family      = AF_INET;
    s->remote.sin_port        = htons(dest_port);
    if (inet_pton(AF_INET, dest_ip, &s->remote.sin_addr) != 1) {
        fprintf(stderr, "Invalid IP: %s\n", dest_ip);
        close(s->fd);
        return -1;
    }
    return 0;
}

int sock_create_receiver(UdpSocket *s, uint16_t bind_port)
{
    s->fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (s->fd < 0) { perror("socket"); return -1; }

    int opt = 1;
    setsockopt(s->fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&s->local, 0, sizeof(s->local));
    s->local.sin_family      = AF_INET;
    s->local.sin_port        = htons(bind_port);
    s->local.sin_addr.s_addr = INADDR_ANY;

    if (bind(s->fd, (struct sockaddr *)&s->local, sizeof(s->local)) < 0) {
        perror("bind");
        close(s->fd);
        return -1;
    }
    return 0;
}

int sock_send(UdpSocket *s, const uint8_t *buf, size_t len)
{
    ssize_t sent = sendto(s->fd, buf, len, 0,
                          (struct sockaddr *)&s->remote, sizeof(s->remote));
    return (sent == (ssize_t)len) ? 0 : -1;
}

/*
 * Blocking receive with timeout.
 * timeout_ms = 0 means block indefinitely.
 * Returns bytes received, 0 on timeout, -1 on error.
 */
int sock_recv(UdpSocket *s, uint8_t *buf, size_t buf_len, long timeout_ms)
{
    if (timeout_ms > 0) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(s->fd, &rfds);
        struct timeval tv = {
            .tv_sec  = timeout_ms / 1000,
            .tv_usec = (timeout_ms % 1000) * 1000
        };
        int r = select(s->fd + 1, &rfds, NULL, NULL, &tv);
        if (r == 0) return 0;   /* timeout */
        if (r < 0)  return -1;
    }

    socklen_t addr_len = sizeof(s->remote);
    ssize_t n = recvfrom(s->fd, buf, buf_len, 0,
                         (struct sockaddr *)&s->remote, &addr_len);
    return (n < 0) ? -1 : (int)n;
}

void sock_close(UdpSocket *s)
{
    if (s->fd >= 0) {
        close(s->fd);
        s->fd = -1;
    }
}

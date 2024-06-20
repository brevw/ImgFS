#include "socket_layer.h"
#include "error.h"
#include "util.h"
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>

static const int LISTEN_BACKLOG = 20;

int tcp_server_init(uint16_t port)
{
    // create socket
    int socket_fd;
    struct sockaddr_in addr;
    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd == -1) {
        perror("socket() in tcp_server_init()");
        return ERR_IO;
    }

    // init addr struct
    zero_init_var(addr);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    // bind socket
    if (bind(socket_fd, (struct sockaddr*) &addr, sizeof(addr)) == -1) {
        perror("bind() in tcp_server_init()");
        if (close(socket_fd) == -1) {
            perror("close() in tcp_server_init()");
        }
        return ERR_IO;
    }
    if (listen(socket_fd, LISTEN_BACKLOG) == -1) {
        perror("listen() in tcp_server_init()");
        if (close(socket_fd) == -1) {
            perror("close() in tcp_server_init()");
        }
        return ERR_IO;
    }
    return socket_fd;
}

int tcp_accept(int passive_socket)
{
    return accept(passive_socket, NULL, NULL);
}

ssize_t tcp_read(int active_socket, char *buf, size_t buflen)
{
    M_REQUIRE_NON_NULL(buf);
    if (active_socket < 0 || buflen == 0) {
        return ERR_INVALID_ARGUMENT;
    }
    return recv(active_socket, buf, buflen, 0);
}

ssize_t tcp_send(int active_socket, const char *response, size_t response_len)
{
    M_REQUIRE_NON_NULL(response);
    if (active_socket < 0 || response_len == 0) {
        return ERR_INVALID_ARGUMENT;
    }
    return send(active_socket, response, response_len, 0);
}

#ifndef SOCKET_H
#define SOCKET_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h> // write and close
#include <sys/socket.h> // socket API
#include <sys/un.h> // unix-domain data structure
#include <poll.h> // for polling
#include <signal.h>
#include <errno.h>

#include "../include/define.h"

#define SCKT_RET_SUCCESS        0
#define SCKT_RET_FAILED         -1
#define SCKT_MAX_CLIENT         6

#define SCKT_FILE               "/tmp/file.socket"

typedef int socket_t;
extern int errno;

#define SCKT_INFO(format, ...)  do { \
                                    printf("[Socket][INFO] " format, ##__VA_ARGS__); \
                                    printf("\n"); \
                                } while(0)

#define SCKT_ERR(format, ...)   do { \
                                    printf("[Socket][ERROR][%s][%d] " format, \
                                        __func__, __LINE__, ##__VA_ARGS__); \
                                    printf(" (err: %s)\n", strerror(errno)); \
                                } while(0)
#ifdef SCKT_DEBUG
#define SCKT_DBG(format, ...)   do { \
                                    printf("[Socket][DEBUG][%s][%d] " format, \
                                        __func__, __LINE__, ##__VA_ARGS__); \
                                    printf("\n"); \
                                } while (0)
#else
#define SCKT_DBG(format, ...)
#endif // SCKT_DEBUG

typedef struct server_socket {
    socket_t fd; // server's socket descriptor
    struct sockaddr_un addr;
    socket_t clnt_fd[SCKT_MAX_CLIENT]; // client's socket descriptor
    struct sockaddr_un clnt_addr[SCKT_MAX_CLIENT];
    socklen_t clnt_addr_len[SCKT_MAX_CLIENT];
    char socket_file[64];
} ServerSocket; 

typedef struct client_socket {
    socket_t fd;
    struct sockaddr_un addr;
    socklen_t addr_len;
    char socket_file[64];
} ClientSocket;

int socket_server_init(ServerSocket *srv);
int socket_server_close(ServerSocket *srv);
int socket_client_init(ClientSocket *clnt);
int socket_server_try_accept(ServerSocket *srv);
int socket_client_close(ClientSocket *clnt);
int socket_try_read(socket_t fd, char *buf, int size);
int socket_read(socket_t fd, char *buf, int size);
int socket_write(socket_t fd, char *buf);

#endif // SOCKET_H
#ifndef SOCKET_H
#define SOCKET_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h> // write and close
#include <sys/socket.h> // socket API
#include <sys/un.h> // unix-domain data structure
#include <poll.h> // polling
#include <fcntl.h> // fcntl 
#include <signal.h>
#include <errno.h>

#include "../include/define.h"

#define SCKT_RET_SUCCESS        0
#define SCKT_RET_FAILED         -1
#define SCKT_MAX_CLIENT         6

#define SCKT_FILE               "/tmp/gpsd.sock"

typedef int socket_t;
extern int errno;

#define SCKT_INFO_LVL 1
#define SCKT_INFO(lvl, format, ...)  do { \
                                    if (lvl >= SCKT_INFO_LVL) { \
                                        printf("[SCKT][INFO] " format, ##__VA_ARGS__); \
                                        printf("\n"); \
                                    } \
                                } while(0)

// TODO: use perror instead()
#define SCKT_ERR_LVL 2
#define SCKT_ERR(lvl, format, ...)   do { \
                                    if (lvl >= SCKT_ERR_LVL) { \
                                        printf("[SCKT][ERROR][%s][%d] " format, \
                                            __func__, __LINE__, ##__VA_ARGS__); \
                                        printf(" (err: %s)\n", strerror(errno)); \
                                    } \
                                } while(0)
#define SCKT_DBG_LVL 4
#define SCKT_DBG(lvl, format, ...)   do { \
                                    if (lvl >= SCKT_DBG_LVL) { \
                                        printf("[SCKT][DEBUG][%s][%d] " format, \
                                            __func__, __LINE__, ##__VA_ARGS__); \
                                        printf("\n"); \
                                    } \
                                } while (0)

typedef struct client_socket {
    socket_t fd;
    struct sockaddr_un addr;
    socklen_t addr_len;
    char socket_file[64];
} ClientSocket;

typedef struct server_socket {
    socket_t fd; // server's socket descriptor
    struct sockaddr_un addr;
    ClientSocket client[SCKT_MAX_CLIENT];
    char socket_file[64];
} ServerSocket;

int socket_server_init(ServerSocket *srv, int log_lvl);
int socket_server_close(ServerSocket *srv);
int socket_client_init(ClientSocket *clnt);
int socket_server_try_accept(ServerSocket *srv);
int socket_client_close(ClientSocket *clnt);
int socket_try_read(socket_t fd, char *buf, int size);
int socket_read(socket_t fd, char *buf, int size);
int socket_write(socket_t fd, char *buf);

#endif // SOCKET_H
#include "socket.h"

/**
 * @brief Initialize Unix-domain socket
 * 
 * @return int 
 */
int socket_server_init(ServerSocket *srv)
{
    int i;

    memset(srv, 0, sizeof(*srv));

    /* Set socket file name */
    strncpy(srv->socket_file, SCKT_FILE, sizeof(srv->socket_file));

    /* Create socket for local communication only */
    srv->srv_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (srv->srv_fd < 0)
    {
        SCKT_ERR("Failed to create socket");
        return SCKT_RET_FAILED;
    }

    srv->srv_addr.sun_family = AF_UNIX;
    strncpy(srv->srv_addr.sun_path, srv->socket_file, sizeof(srv->srv_addr.sun_path));
    unlink(srv->srv_addr.sun_path); // delete a name/file if it's previously connected to another socket

    /* Assign a name to the socket */
    if (bind(srv->srv_fd, (struct sockaddr*)&srv->srv_addr, sizeof(srv->srv_addr))) {
        SCKT_ERR("Failed to bind");
        return SCKT_RET_FAILED;
    }

    /* Mark the socket as a passive socket to accept incoming connection requests */
    if (listen(srv->srv_fd, SCKT_MAX_CLIENT)) {
        SCKT_ERR("Failed to listen");
        return SCKT_RET_FAILED;
    }

    SCKT_INFO("Unix-domain socket created w/ %s and max client of %d",
                srv->socket_file, SCKT_MAX_CLIENT);

#if 0 // decide whether to use SIGIO or not later
    int fd_flags = 0; // server file descriptor's flag

    /* Set srv_fd owner to the current PID */
    fcntl(srv->srv_fd, F_SETOWN, getpid());

    /* Set the srv_fd as O_ASYNC */
    fd_flags = fcntl(srv->srv_fd, F_GETFL);
    fd_flags |= O_ASYNC;
    fcntl(srv->srv_fd, F_SETFD, fd_flags);
#endif

    /* Initialize clients' FDs to -1 */
    for (i = 0; i < SCKT_MAX_CLIENT; i++) {
        srv->clnt_fd[i] = -1;
    }

    /* Accept incoming client connection
     * Note that this function will block until a request comes in
     * TODO: accept multiple clients */
    if ((srv->clnt_fd[0] = accept(srv->srv_fd, (struct sockaddr*)&srv->clnt_addr[0],
                                &srv->clnt_addr_len[0]))
        < 0) {
        SCKT_ERR("Failed to accept client connection!");
        return SCKT_RET_FAILED;
    }

    SCKT_INFO("Connection w/ cllient %d established", srv->clnt_fd[0]);

    return SCKT_RET_SUCCESS;
}

/**
 * @brief Close socket connection
 * 
 * @param gpsd contains server and client's file descriptor and other socket info
 * @return int 
 */
int socket_server_close(ServerSocket *srv)
{
    int i;
    
    /* Close clients first */
    for (i = 0; i < SCKT_MAX_CLIENT; i++) {
        if (srv->clnt_fd[i] == -1) {
            break;
        }

        if (close(srv->clnt_fd[i])) {
            SCKT_ERR("Failed to close client socket %d", srv->clnt_fd[i]);
        }

        srv->clnt_fd[i] = -1;
        memset(&srv->clnt_addr[i], 0, sizeof(srv->clnt_addr[i]));
    }

    if (close(srv->srv_fd)) {
        SCKT_ERR("Failed to close socket %d", srv->srv_fd);
        return SCKT_RET_FAILED;
    }

    /* Reset server */
    srv->srv_fd = -1;
    memset(&srv->srv_addr, 0, sizeof(srv->srv_addr));

    SCKT_INFO("Successfully closed server");

    return SCKT_RET_SUCCESS;
}

int socket_client_init(ClientSocket *clnt)
{
    memset(clnt, 0, sizeof(*clnt));

    /* Set socket file name and type */
    clnt->addr.sun_family = AF_UNIX;
    strncpy(clnt->addr.sun_path, SCKT_FILE, sizeof(clnt->socket_file));

    /* Create a socket for client */
    if ((clnt->fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
        SCKT_ERR("Failed to create client socket");
        return SCKT_RET_FAILED;
    }

    /* Connect w/ server */
    if (connect(clnt->fd, (struct sockaddr*)&clnt->addr, sizeof(clnt->addr))) {
        SCKT_ERR("Failed to connect to %s", clnt->addr.sun_path);
        return SCKT_RET_FAILED;
    }

    SCKT_INFO("Successfully connected to %s", clnt->addr.sun_path);

    return SCKT_RET_SUCCESS;
}

int socket_client_close(ClientSocket *clnt)
{
    if (close(clnt->fd)) {
            SCKT_ERR("Failed to close client socket %d", clnt->fd);
    }

    clnt->fd = -1;
    memset(&clnt->addr, 0, sizeof(clnt->addr));

    SCKT_INFO("Successfully closed client");

    return SCKT_RET_SUCCESS;
}

/**
 * @brief Read data from a client
 * 
 * @param gpsd contains server and client's file descriptor and other socket info
 * @return int
 */
int socket_read(socket_t fd, char *buf, int size)
{
    return recv(fd, buf, size, 0);
}

int socket_write(socket_t fd, char *buf)
{
    if (send(fd, buf, strlen(buf), 0) < 0) {
        SCKT_ERR("Socket %d failed to send %s", fd, buf);
        return SCKT_RET_FAILED;
    }

    return SCKT_RET_SUCCESS;
}
#include "socket.h"

static int srv_log_lvl;

/**
 * @brief Initialize Unix-domain socket
 * 
 * @return int 
 */
int socket_server_init(ServerSocket *srv, int log_lvl)
{
    int i;

    memset(srv, 0, sizeof(*srv));

    /* Set log level */
    srv_log_lvl = log_lvl;

    /* Set socket file name */
    strncpy(srv->socket_file, SCKT_FILE, sizeof(srv->socket_file));

    /* Create socket for local communication only */
    srv->fd = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (srv->fd < 0)
    {
        SCKT_ERR(srv_log_lvl, "Failed to create socket");
        return -1;
    }

    srv->addr.sun_family = AF_UNIX;
    strncpy(srv->addr.sun_path, srv->socket_file, sizeof(srv->addr.sun_path));
    unlink(srv->addr.sun_path); // delete a name/file if it's previously connected to another socket

    /* Assign a name to the socket */
    if (bind(srv->fd, (struct sockaddr*)&srv->addr, sizeof(srv->addr))) {
        SCKT_ERR(srv_log_lvl, "Failed to bind");
        return -1;
    }

    /* Mark the socket as a passive socket to accept incoming connection requests */
    if (listen(srv->fd, SCKT_MAX_CLIENT)) {
        SCKT_ERR(srv_log_lvl, "Failed to listen");
        return -1;
    }

    SCKT_INFO(srv_log_lvl, "Unix-domain socket created w/ %s and max client of %d",
                srv->socket_file, SCKT_MAX_CLIENT);

#if 0 // decide whether to use SIGIO or not later
    int fd_flags = 0; // server file descriptor's flag

    /* Set fd owner to the current PID */
    fcntl(srv->fd, F_SETOWN, getpid());

    /* Set the fd as O_ASYNC */
    fd_flags = fcntl(srv->fd, F_GETFL);
    fd_flags |= O_ASYNC;
    fcntl(srv->fd, F_SETFD, fd_flags);
#endif

    /* Initialize clients' FDs to -1 */
    for (i = 0; i < SCKT_MAX_CLIENT; i++) {
        srv->client[i].fd = -1;
    }

    if (socket_server_try_accept(srv) < 0) {
        SCKT_DBG(srv_log_lvl, "No client connection yet!");
    } else {
        SCKT_DBG(srv_log_lvl, "Connection w/ cllient %d established", srv->client[i].fd);
    }

    return 0;
}

/**
 * @brief Try to accept incoming client connection if any
 * 
 * @note The socket should be set as non-blocking before using this APi
 * @see socket_server_init()
 * @param srv 
 * @return int file descriptor
 */
int socket_server_try_accept(ServerSocket *srv)
{
    /* TODO: accept multiple clients */
    srv->client[0].fd = accept(srv->fd, (struct sockaddr*)&srv->client[0].addr,
                                &srv->client[0].addr_len);

    return srv->client[0].fd;
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
        if (srv->client[i].fd == -1) {
            break;
        }

        if (close(srv->client[i].fd)) {
            SCKT_ERR(srv_log_lvl, "Failed to close client socket %d", srv->client[i].fd);
        }

        srv->client[i].fd = -1;
        memset(&srv->client[i].addr, 0, sizeof(srv->client[i].addr));
    }

    if (close(srv->fd)) {
        SCKT_ERR(srv_log_lvl, "Failed to close socket %d", srv->fd);
        return -1;
    }

    /* Reset server */
    srv->fd = -1;
    memset(&srv->addr, 0, sizeof(srv->addr));

    SCKT_INFO(srv_log_lvl, "Successfully closed server");

    return 0;
}

int socket_client_init(ClientSocket *clnt)
{
    memset(clnt, 0, sizeof(*clnt));

    /* Set socket file name and type */
    clnt->addr.sun_family = AF_UNIX;
    strncpy(clnt->addr.sun_path, SCKT_FILE, sizeof(clnt->socket_file));

    /* Create a socket for client */
    if ((clnt->fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
        SCKT_ERR(srv_log_lvl, "Failed to create client socket");
        return -1;
    }

    /* Connect w/ server */
    if (connect(clnt->fd, (struct sockaddr*)&clnt->addr, sizeof(clnt->addr))) {
        SCKT_ERR(srv_log_lvl, "Failed to connect to %s", clnt->addr.sun_path);
        return -1;
    }

    SCKT_INFO(srv_log_lvl, "Successfully connected to %s", clnt->addr.sun_path);

    return 0;
}

int socket_client_close(ClientSocket *clnt)
{
    if (close(clnt->fd)) {
        SCKT_ERR(srv_log_lvl, "Failed to close client socket %d", clnt->fd);
    }

    clnt->fd = -1;
    memset(&clnt->addr, 0, sizeof(clnt->addr));

    SCKT_INFO(srv_log_lvl, "Successfully closed client");

    return 0;
}

/**
 * @brief Try read data if available (non-blocking call)
 * 
 * @param fd 
 * @param buf 
 * @param size 
 * @return int 
 */
int socket_try_read(socket_t fd, char *buf, int size)
{
    struct pollfd fds;
    int ret = 0;

    /* Validate fd */
    if (fd < 0) {
        SCKT_ERR(srv_log_lvl, "Invalid fd %d", fd);
        return -1;
    }

    /* Poll the fd */
    memset(&fds, 0, sizeof(fds));
    fds.fd = fd;
    fds.events = POLLIN;
    ret = poll(&fds, 1, 0);
    if (ret == -1) {
        SCKT_ERR(srv_log_lvl, "Failed to poll fd %d", fd);
        return -1;
    } else if (fds.revents & POLLHUP) {
        SCKT_INFO(srv_log_lvl, "Client already disconnected");
        return -1;
    } else if (ret > 0) {
        return recv(fd, buf, size, 0);
    }

    SCKT_DBG(srv_log_lvl, "Nothing to read (revents: %d)", fds.revents);

    return 0;
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
    int ret;

    ret = send(fd, buf, strlen(buf), 0);
    if (ret < 0) {
        SCKT_ERR(srv_log_lvl, "Socket %d failed to send %s", fd, buf);
        return -1;
    }

    SCKT_DBG(srv_log_lvl, "Successfully write");

    return ret;
}
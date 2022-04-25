#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <signal.h>

#include "../socket/socket.h"

int stop = 0;

void handler(int sig)
{
    stop = 1;
}

int main(int argc, char **argv)
{
    ServerSocket srv;
    char buf[512];
    int size = 0;
    int cnt = 0;

    signal(SIGINT, handler);

    socket_server_init(&srv);

    while (!stop) {
        size = socket_read(srv.clnt_fd[0], buf, 512);
        if (size == 0) {
            printf("No more to read\n");
            break;
        }

        buf[size] = '\0';
        printf("[%d][size: %d] buf: %s\n", cnt, size, buf);

        /* Write back to client */
        socket_write(srv.clnt_fd[0], "hello from server");

        cnt++;
    }

    socket_server_close(&srv);

    return 0;
}
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
    ClientSocket clnt;
    char msg[512];
    char buf[512];
    int size = 0;
    int cnt = 0;

    signal(SIGINT, handler);

    socket_client_init(&clnt);

    memset(msg, 0, sizeof(msg));

    while (!stop) {
        sprintf(msg, "hello from client %d", cnt);
        socket_write(clnt.fd, msg);
        sleep(3);
        cnt++;

        size = socket_read(clnt.fd, buf, 512);
        buf[size] = 0;
        printf("buf: %s\n", buf);
    }

    socket_client_close(&clnt);

    return 0;
}
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <signal.h>

#include "../include/client.h"
#include "../socket/socket.h"

int stop = 0;

void handler(int sig)
{
    stop = 1;
}

int main(int argc, char **argv)
{
    ClientSocket clnt;
    int state;
    char buf[512];
    int size = 0;

    signal(SIGINT, handler);

    socket_client_init(&clnt);

    while (!stop) {
        socket_write(clnt.fd, "GpsState?");

        size = socket_read(clnt.fd, buf, 512);
        buf[size] = 0;

        state = atoi(buf);
        switch (state) {
            case GPS_NOT_READY:
                printf("GPS is not ready yet.\n");
                break;
            case GPS_POS_FIXED:
                printf("Position is fixed.\n");
                break;
            case GPS_TIME_FIXED:
                printf("Time is fixed.\n");
                break;
            default:
                printf("Unkown state: %d\n", state);
                break;
        }
    }

    socket_client_close(&clnt);

    return 0;
}
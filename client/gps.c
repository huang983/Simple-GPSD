#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <signal.h>

#include "gps.h"
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
                GPS_INFO("GPS is not ready yet.");
                break;
            case GPS_POS_FIXED:
                GPS_INFO("Position is fixed.");
                break;
            case GPS_TIME_FIXED:
                GPS_INFO("Time is fixed.");
                break;
            default:
                GPS_INFO("Unkown state: %d", state);
                break;
        }
    }

    socket_client_close(&clnt);

    return 0;
}
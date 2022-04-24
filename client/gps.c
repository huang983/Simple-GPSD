#include "gps.h"
#include "../include/define.h"
#include "../include/client.h"

int main(int argc, char **argv)
{
    int clnt_fd = 0;
    struct sockaddr_un clnt_addr;

    if ((clnt_fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
        GPS_ERR("Failed to create a socket");
    }

    GPS_INFO("Socket %d created", clnt_fd);

    clnt_addr.sun_family = AF_UNIX;
    strncpy(clnt_addr.sun_path, GPSD_UDS_FILE, sizeof(clnt_addr.sun_path));
    if (connect(clnt_fd, (struct sockaddr*)&clnt_addr, sizeof(clnt_addr)) < 0) {
        GPS_ERR("Failed to connect to %s", GPSD_UDS_FILE);
        exit(0);
    }

    GPS_INFO("Client connected!");

    return 0;
}
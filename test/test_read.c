#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <signal.h>

#include "../srv/device.h"
#include "../srv/ubx.h"

DeviceInfo gps_dev;
int stop = 0;

void handler(int sig)
{
    stop = 1;
}

int set_msg_rate()
{
    if (ubx_set_msg_rate(gps_dev.fd, NMEA_CLASS_STD, NMEA_ID_GSV, UBX_CFG_MSG_OFF)) {
        printf("Failed to set GSV message rate\n");
        return -1;
    }

    if (ubx_set_msg_rate(gps_dev.fd, NMEA_CLASS_STD, NMEA_ID_GLL, UBX_CFG_MSG_OFF)) {
        printf("Failed to set GLL message rate\n");
        return -1;
    }

    if (ubx_set_msg_rate(gps_dev.fd, NMEA_CLASS_STD, NMEA_ID_ZDA, UBX_CFG_MSG_OFF)) {
        printf("Failed to set ZDA message rate\n");
        return -1;
    }

    if (ubx_set_msg_rate(gps_dev.fd, UBX_CLASS_NAV, UBX_ID_NAV_TIMEGPS, UBX_CFG_MSG_OFF)) {
        printf("Failed to set UBX-NAV-TIMETGPS message rate\n");
        return -1;
    }

    if (ubx_set_msg_rate(gps_dev.fd, NMEA_CLASS_STD, NMEA_ID_GSA, UBX_CFG_MSG_ON)) {
        printf("Failed to set GSA message rate\n");
        return -1;
    }

    if (ubx_set_msg_rate(gps_dev.fd, NMEA_CLASS_STD, NMEA_ID_GNS, UBX_CFG_MSG_ON)) {
        printf("Failed to set GNS message rate\n");
        return -1;
    }

    return 0;
}

int main(int argc, char **argv)
{
#define BUF_SZ 1024
    FILE *fptr = NULL;
    char buf[BUF_SZ];
    int offset = 0;
    int size = 0;

    if (argc < 2) {
        printf("Please provide device path!\n");
        exit(0);
    }

    // 0 initialization
    memset(&gps_dev, 0, sizeof(gps_dev));
    
    // register signal handler
    signal(SIGINT, handler);

    // open device
    strncpy(gps_dev.name, argv[1], sizeof(gps_dev.name));
    if (device_init(&gps_dev)) {
        printf("Failed to open device %s", argv[1]);
    }

    if (set_msg_rate()) {
        printf("Failed to set msg rate\n");
        goto close_device;
    }

    printf("Successfully set message rate!\n");
    
    while (!stop) {
        if ((offset + 8) >= BUF_SZ) {
            printf("offset: %d\n", offset);
            break;
        }

        size = read(gps_dev.fd, (char *)(buf + offset), 8);
        printf("%.8s", (char *)(buf + offset));
        offset += size;
    }

#if 0
    fptr = fopen("msg.txt", "w");
    if (fptr == NULL) {
        goto close_device;
    }
    size = fwrite(buf, 1, BUF_SZ, fptr) < BUF_SZ;
    if (size < BUF_SZ) {
        printf("Only wrote %dbytes\n", size);
    }
    fclose(fptr);
    fptr = NULL;
#endif

close_device:
    if (device_close(&gps_dev)) {
        printf("Failed to close device %s\n", argv[1]);
        exit(0);
    }

    return 0;
}
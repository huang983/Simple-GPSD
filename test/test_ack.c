#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <signal.h>

#include "../srv/device.h"
#include "../srv/ubx.h"

int stop = 0;

void handler(int sig)
{
    stop = 1;
}

int main(int argc, char **argv)
{
    DeviceInfo gps_dev;
    int cnt = 0; // count test time

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
    if (device_init(&gps_dev, 2)) {
        printf("Failed to open device %s", argv[1]);
    }

    while (!stop) {
        if (ubx_set_msg_rate(gps_dev.fd, NMEA_CLASS_STD, NMEA_ID_VTG, UBX_CFG_MSG_OFF)) {
            printf("Failed to set VTG message rate\n");
            return -1;
        }

        if (ubx_set_msg_rate(gps_dev.fd, NMEA_CLASS_STD, NMEA_ID_GSV, UBX_CFG_MSG_OFF)) {
            printf("Failed to set GSV message rate\n");
            goto close_device;
        }

        if (ubx_set_msg_rate(gps_dev.fd, NMEA_CLASS_STD, NMEA_ID_GLL, UBX_CFG_MSG_OFF)) {
            printf("Failed to set GLL message rate\n");
            goto close_device;
        }

        if (ubx_set_msg_rate(gps_dev.fd, NMEA_CLASS_STD, NMEA_ID_ZDA, UBX_CFG_MSG_OFF)) {
            printf("Failed to set ZDA message rate\n");
            goto close_device;
        }

        if (ubx_set_msg_rate(gps_dev.fd, UBX_CLASS_NAV, UBX_ID_NAV_TIMEGPS, UBX_CFG_MSG_ON)) {
            printf("Failed to set UBX-NAV-TIMETGPS message rate\n");
            goto close_device;
        }

        if (ubx_set_msg_rate(gps_dev.fd, NMEA_CLASS_STD, NMEA_ID_GSA, UBX_CFG_MSG_ON)) {
            printf("Failed to set GSA message rate\n");
            goto close_device;
        }

        if (ubx_set_msg_rate(gps_dev.fd, NMEA_CLASS_STD, NMEA_ID_GNS, UBX_CFG_MSG_ON)) {
            printf("Failed to set GNS message rate\n");
            goto close_device;
        }

        cnt++;
        printf("Successfully set message rate! (cnt: %d)\n", cnt);
        sleep(1);
    }

close_device:
    if (device_close(&gps_dev)) {
        printf("Failed to close device %s\n", argv[1]);
        exit(0);
    }

    return 0;
}
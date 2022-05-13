#ifndef GPSD_H
#define GPSD_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h> // getopt
#include <fcntl.h>
#include <unistd.h> // write and close
#include <termios.h>
#include <sys/types.h>
#include <signal.h>
#include <poll.h>
#include <errno.h>

#include "device.h" // device-related APIs
#include "../socket/socket.h" // socket-related APIs
#include "../include/define.h"
#include "../include/client.h"

extern int errno;

#define GPSD_BUFSIZE 1024
#define NMEA_BUFSIZE 512
#define UBX_BUFSIZE 512

#define GPSD_RET_SUCCESS        0
#define GPSD_RET_FAILED         -1

#define GPSD_INFO_LVL 1
#define GPSD_INFO(lvl, format, ...)  do { \
                                    if (lvl >= GPSD_INFO_LVL) { \
                                        printf("[GPSD][INFO] " format, ##__VA_ARGS__); \
                                        printf("\n"); \
                                    } \
                                } while(0)

// TODO: use perror instead()
#define GPSD_ERR_LVL 2
#define GPSD_ERR(lvl, format, ...)   do { \
                                    if (lvl >= GPSD_ERR_LVL) { \
                                        printf("[GPSD][ERROR][%s][%d] " format, \
                                            __func__, __LINE__, ##__VA_ARGS__); \
                                        if (errno != 0) \
                                            printf(" (err: %s)", strerror(errno)); \
                                        printf("\n"); \
                                    } \
                                } while(0)
#define GPSD_DBG_LVL 4
#define GPSD_DBG(lvl, format, ...)   do { \
                                    if (lvl >= GPSD_DBG_LVL) { \
                                        printf("[GPSD][DEBUG][%s][%d] " format, \
                                            __func__, __LINE__, ##__VA_ARGS__); \
                                        printf("\n"); \
                                    } \
                                } while (0)

typedef struct gpsd_data {
    DeviceInfo gps_dev;
    ServerSocket srv;
    char rd_buf[GPSD_BUFSIZE]; // buffer to read client's message
    char wr_buf[GPSD_BUFSIZE]; // buffer to send client message
    int show_result; // print parsing result
    int log_lvl;
    int wait_pps;
    int socket_enable;
    int stop; // set to 1 by CTRL-C
} GpsdData;

#endif // GPSD_H
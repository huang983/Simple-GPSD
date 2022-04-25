#ifndef GPSD_H
#define GPSD_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h> // write and close
#include <termios.h>
#include <sys/types.h>
#include <signal.h>
#include <poll.h>
#include <errno.h>

#include "../include/define.h"
#include "../include/client.h"

extern int errno;

#define RD_BUFSIZE 1024
#define NMEA_BUFSIZE 512
#define UBX_BUFSIZE 512

#define GPSD_RET_SUCCESS        0
#define GPSD_RET_FAILED         -1

#define GPSD_INFO(format, ...)  do { \
                                    printf("[GPSD][INFO] " format, ##__VA_ARGS__); \
                                    printf("\n"); \
                                } while(0)

// TODO: use perror instead()
#define GPSD_ERR(format, ...)   do { \
                                    printf("[GPSD][ERROR][%s][%d] " format, \
                                        __func__, __LINE__, ##__VA_ARGS__); \
                                    printf(" (err: %s)\n", strerror(errno)); \
                                } while(0)
#ifdef GPSD_DEBUG
#define GPSD_DBG(format, ...)   do { \
                                    printf("[GPSD][DEBUG][%s][%d] " format, \
                                        __func__, __LINE__, ##__VA_ARGS__); \
                                    printf("\n"); \
                                } while (0)
#else
#define GPSD_DBG(format, ...)
#endif // GPSD_DEBUG

typedef struct gpsd_data {
    int fd; // file descriptor
    char dev_name[64]; // device name
    ServerSocket srv;
    int stop; // set to 1 by CTRL-C
} GpsdData;

#endif // GPSD_H
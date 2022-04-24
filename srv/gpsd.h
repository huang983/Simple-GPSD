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
#include <sys/socket.h> // socket API
#include <sys/un.h> // unix-domain data structure
#include <poll.h>
#include <errno.h>

extern int errno;

#define RD_BUFSIZE 1024
#define NMEA_BUFSIZE 512
#define UBX_BUFSIZE 512

#define GPSD_RET_SUCESS         0
#define GPSD_RET_FAILED         -1

#define GPSD_UDS_MAX_CLIENT     5 // arbitrary client count

#define GPSD_INFO(format, ...)  do { \
                                    printf("[GPSD][INFO] " format, ##__VA_ARGS__); \
                                    printf("\n"); \
                                } while(0)

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

#endif // GPSD_H
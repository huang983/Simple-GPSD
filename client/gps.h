#ifndef GPS_H
#define GPS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/socket.h> // socket API
#include <sys/types.h>
#include <sys/un.h> // unix-domain data structure
#include <errno.h>

extern int errno;

#define GPS_RET_SUCESS          0
#define GPS_RET_FAILED          -1

#define GPS_INFO(format, ...)  do { \
                                    printf("[GPS][INFO] " format, ##__VA_ARGS__); \
                                    printf("\n"); \
                                } while(0)

#define GPS_ERR(format, ...)   do { \
                                    printf("[GPS][ERROR][%s][%d] " format, \
                                        __func__, __LINE__, ##__VA_ARGS__); \
                                    printf(" (err: %s)\n", strerror(errno)); \
                                } while(0)
#ifdef GPS_DEBUG
#define GPS_DBG(format, ...)   do { \
                                    printf("[GPS][DEBUG][%s][%d] " format, \
                                        __func__, __LINE__, ##__VA_ARGS__); \
                                    printf("\n"); \
                                } while (0)
#else
#define GPS_DBG(format, ...)
#endif // GPSD_DEBUG

#endif
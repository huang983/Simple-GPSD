#ifndef DEVICE_H
#define DEVICE_H

#define DEV_RD_BUF_SIZE 1024

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h> // write and close
#include <fcntl.h> // open
#include <sys/types.h> // open

extern int errno;

typedef enum {
    NO_POS_FIX = 0,
    POS_2D_GNSS_FIX,
    POS_3D_GNSS_FIX,
    POS_FIX_NUM,
} PosFixMode;

typedef struct device_info {
    int fd; // file descriptor
    char name[64]; // device name
    uint8_t buf[DEV_RD_BUF_SIZE]; // store read system call result here
    int size; // read size returned by read()
    /* Below are time-related info extracted from the GPS module */
    PosFixMode mode; 
    uint8_t locked_sat; // number of locked satellites
    uint32_t iTOW;
    uint32_t fTOW;
    uint32_t week;
    uint8_t leap_sec;
    uint8_t valid;
    uint8_t tAcc; // time accuracy estimate
} DeviceInfo;

#define DEV_INFO(format, ...)  do { \
                                    printf("[DEV][INFO] " format, ##__VA_ARGS__); \
                                    printf("\n"); \
                                } while(0)

#define DEV_ERR(format, ...)   do { \
                                    printf("[DEV][ERROR][%s][%d] " format, \
                                        __func__, __LINE__, ##__VA_ARGS__); \
                                    printf(" (err: %s)\n", strerror(errno)); \
                                } while(0)
#ifdef DEV_DEBUG
#define DEV_DBG(format, ...)   do { \
                                    printf("[DEV][DEBUG][%s][%d] " format, \
                                        __func__, __LINE__, ##__VA_ARGS__); \
                                    printf("\n"); \
                                } while (0)
#else
#define DEV_DBG(format, ...)
#endif // DEV_DEBUG

int device_init(DeviceInfo *gps_dev);
int device_close(DeviceInfo *gps_dev);

#endif // DEVICE_H
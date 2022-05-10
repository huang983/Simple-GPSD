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
#include <sys/fcntl.h> 

#include "../../ec-platform-sw/linux/drivers/gnss/gnss_ioctl.h" // for ioctl

extern int errno;

typedef enum {
    POS_FIX_PAD = 0,
    NO_POS_FIX,
    POS_2D_GNSS_FIX,
    POS_3D_GNSS_FIX,
    POS_FIX_NUM,
} PosFixMode;

typedef struct device_info {
    int fd; // file descriptor
    char name[64]; // device name
    uint8_t buf[DEV_RD_BUF_SIZE]; // store read system call result here
    int size; // read size returned by read()
    int offset; // index of UBX-NAV-TIMEGPS
    int log_lvl;
    /* Below are time-related info extracted from the GPS module */
    PosFixMode mode; 
    uint8_t locked_sat; // number of locked satellites
    uint32_t iTOW;
    uint32_t fTOW;
    uint32_t week;
    uint8_t leap_sec;
    uint8_t valid;
    uint32_t tAcc; // time accuracy estimate
} DeviceInfo;

#define DEV_INFO_LVL 1
#define DEV_INFO(lvl, format, ...)  do { \
                                    if (lvl >= DEV_INFO_LVL) { \
                                        printf("[DEV][INFO] " format, ##__VA_ARGS__); \
                                        printf("\n"); \
                                    } \
                                } while(0)

// TODO: use perror instead()
#define DEV_ERR_LVL 2
#define DEV_ERR(lvl, format, ...)   do { \
                                    if (lvl >= DEV_ERR_LVL) { \
                                        printf("[DEV][ERROR][%s][%d] " format, \
                                            __func__, __LINE__, ##__VA_ARGS__); \
                                        printf(" (err: %s)\n", strerror(errno)); \
                                    } \
                                } while(0)
#define DEV_DBG_LVL 4
#define DEV_DBG(lvl, format, ...)   do { \
                                    if (lvl >= DEV_DBG_LVL) { \
                                        printf("[DEV][DEBUG][%s][%d] " format, \
                                            __func__, __LINE__, ##__VA_ARGS__); \
                                        printf("\n"); \
                                    } \
                                } while (0)

int device_init(DeviceInfo *gps_dev, int log_lvl);
int device_read(DeviceInfo *gps_dev);
int device_parse(DeviceInfo *gps_dev);
int device_close(DeviceInfo *gps_dev);

#endif // DEVICE_H
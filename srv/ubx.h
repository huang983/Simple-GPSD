#ifndef UBX_H
#define UBX_H

#include "gpsd.h"
#include "nmea.h"

#define COMBINE_TWO_EIGHT_BIT(msb, lsb) (((msb << 8)) | (lsb))
#define COMBINE_FOUR_EIGHT_BIT(first, second, third, fourth) \
                                (((first << 24) | (second << 16) | (third << 8)) | (fourth))

/* Indices for UBX message */
#define UBX_IDX_SYNC1               0
#define UBX_IDX_SYNC2               1
#define UBX_IDX_ClASS               2
#define UBX_IDX_ID                  3
#define UBX_IDX_PL_LSB              4
#define UBX_IDX_PL_MSB              5
#define UBX_IDX_PL_START            6

/* Header */
#define UBX_SYNC1                   0xB5
#define UBX_SYNC2                   0x62

/* Message Classes */
#define UBX_CLASS_NAV               0x01
#define UBX_CLASS_INF               0x04
#define UBX_CLASS_ACK               0x05
#define UBX_CLASS_CFG               0x06
#define UBX_CLASS_MON               0x0A

/* Message IDs */
#define UBX_ID_CFG_PRT              0x00
#define UBX_ID_CFG_MSG              0x01
#define UBX_ID_NAV_TIMEGPS          0x20

/* UBX-CFG-MSG related */
#define UBX_CFG_MSG_ON              0x01
#define UBX_CFG_MSG_OFF             0x00

/* UBX-ACK related */
#define UBX_ACK_ACK                 0x01
#define UBX_ACK_NAK                 0x00
#define UBX_ACK_MSG_LEN             10

/* UBX-NAV-TIMEGPS related */
#define NAV_TIMEGPS_MSG_LEN         24
#define NAV_TIMEGPS_VALID           7
#define NAV_TIMEGPS_iTOW_OFFSET     6
#define NAV_TIMEGPS_fTOW_OFFSET     10
#define NAV_TIMEGPS_WEEK_OFFSET     14
#define NAV_TIMEGPS_LEAP_OFFSET     16
#define NAV_TIMEGPS_VALID_OFFSET    17
#define NAV_TIMEGPS_tAcc_OFFSET     18

typedef struct ubx_msg {
    uint8_t header[2];
    uint8_t class;
    uint8_t id;
    uint8_t payload_length[2];
    uint8_t payload_ck_sum[256]; // payload and checksum combined
    int size; // total message size from header to checksum
} UbxMsg;

int ubx_set_msg_rate(int fd, uint8_t class, uint8_t id, uint8_t rate);

#define UBX_INFO(format, ...)  do { \
                                    printf("[UBX][INFO] " format, ##__VA_ARGS__); \
                                    printf("\n"); \
                                } while(0)

// TODO: use perror instead()
#define UBX_ERR(format, ...)   do { \
                                    printf("[UBX][ERROR][%s][%d] " format, \
                                        __func__, __LINE__, ##__VA_ARGS__); \
                                    printf(" (err: %s)\n", strerror(errno)); \
                                } while(0)
#ifdef UBX_DEBUG
#define UBX_DBG(format, ...)   do { \
                                    printf("[UBX][DEBUG][%s][%d] " format, \
                                        __func__, __LINE__, ##__VA_ARGS__); \
                                    printf("\n"); \
                                } while (0)
#else
#define UBX_DBG(format, ...)
#endif // UBX_DEBUG

#endif // UBX_H
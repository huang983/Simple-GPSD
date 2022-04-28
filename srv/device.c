#include "device.h"
#include "ubx.h"

/**
 * @brief Initialize GPS device and configure message rate
 * 
 * @param gps_dev store file decsriptor
 * @param name path of the device
 * @return int 
 */
int device_init(DeviceInfo *gps_dev)
{
    /* Open the device */
    gps_dev->fd = open(gps_dev->name, O_RDWR);
    if (gps_dev->fd == -1) {
        DEV_ERR("[%s] Cannot open %s\n", __FILE__, gps_dev->name);
        return -1;
    }

    DEV_INFO("Successfully opened %s", gps_dev->name);

    /* Poll UBX-NAV-TIMETGPS once per second */
    if (ubx_set_msg_rate(gps_dev->fd, UBX_CLASS_NAV, UBX_ID_NAV_TIMEGPS, UBX_CFG_MSG_ON)) {
        DEV_ERR("Failed to set UBX-NAV-TIMETGPS message rate");
    }

    return 0;
}

/**
 * @brief Read messages from GPS device
 * 
 * @param gps_dev 
 * @return int 
 */
int device_read(DeviceInfo *gps_dev)
{
    /* TODO: wait for PPS interrupt first */

    /* Read from device */
    if ((gps_dev->size = read(gps_dev->fd, gps_dev->buf, DEV_RD_BUF_SIZE))
            <= 0) {
        DEV_ERR("Failed to read %s (size: %d)", gps_dev->name, gps_dev->size);
        return -1;
    }
    
    return 0;
}

/**
 * @brief Parse GPS message and fill in time-related info in DeviceInfo
 * 
 * @param gps_dev 
 * @return int 
 */
int device_parse(DeviceInfo *gps_dev)
{
    int i = 0;

    while (i < gps_dev->size) {
        if (gps_dev->buf[i] == 0xB5) {
            /* UBX message */
            if (gps_dev->buf[i + UBX_IDX_ClASS] == UBX_CLASS_NAV &&
                gps_dev->buf[i + UBX_IDX_ID] == UBX_ID_NAV_TIMEGPS) {
                /* Validate bits first
                 * bit 0: TOW
                 * bit 1: Week
                 * bit 2: Leap sec */
                if (gps_dev->buf[i + NAV_TIMEGPS_VALID_OFFSET] & 0x7) {
                    gps_dev->iTOW = gps_dev->buf[i + NAV_TIMEGPS_iTOW_OFFSET];
                    gps_dev->fTOW = gps_dev->buf[i + NAV_TIMEGPS_fTOW_OFFSET];
                    gps_dev->week = gps_dev->buf[i + NAV_TIMEGPS_WEEK_OFFSET];
                    gps_dev->leap_sec = gps_dev->buf[i + NAV_TIMEGPS_LEAP_OFFSET];
                    gps_dev->valid = gps_dev->buf[i + NAV_TIMEGPS_VALID_OFFSET];
                    gps_dev->tAcc = gps_dev->buf[i + NAV_TIMEGPS_tAcc_OFFSET];
                }
            }
        } else if (gps_dev->buf[i] == '$') {
            /* NMEA */
        }

        i++;
    }

    return 0;
}

/**
 * @brief Close GPS device
 * 
 * @param gps_dev
 * @return int 
 */
int device_close(DeviceInfo *gps_dev)
{
    if (close(gps_dev->fd) < 0) {
        DEV_ERR("Failed to close %s w/ file descriptor = %d", gps_dev->name, gps_dev->fd);
        return -1;
    }

    gps_dev->fd = -1;

    DEV_INFO("Closed device %s", gps_dev->name);

    return 0;
}
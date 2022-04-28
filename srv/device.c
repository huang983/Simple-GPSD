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
    sleep(1);

    /* Read from device */
    if ((gps_dev->size = read(gps_dev->fd, gps_dev->buf, DEV_RD_BUF_SIZE))
            <= 0) {
        DEV_ERR("Failed to read %s (size: %d)", gps_dev->name, gps_dev->size);
        return -1;
    }
    
    // DEV_DBG("size: %d", gps_dev->size);
    // DEV_DBG("Message: %s", gps_dev->buf);

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

    if (gps_dev->size <= 0) {
        DEV_ERR("Size is 0. Nothing to parse!");
    }

    while (i < gps_dev->size) {
        if (gps_dev->buf[i] == 0xB5) {
            /* UBX message */
            DEV_DBG("Parsing UBX message");
            if ((i + NAV_TIMEGPS_tAcc_OFFSET) >= gps_dev->size) {
                DEV_ERR("UBX-NAV-TIMEGPS message is incomplete!");
                return -1;
            }

            if (gps_dev->buf[i + UBX_IDX_ClASS] == UBX_CLASS_NAV &&
                gps_dev->buf[i + UBX_IDX_ID] == UBX_ID_NAV_TIMEGPS) {
                /* Validate bits first
                 * bit 0: TOW
                 * bit 1: Week
                 * bit 2: Leap sec */
                if (gps_dev->buf[i + NAV_TIMEGPS_VALID_OFFSET] & 0x7) {
                    gps_dev->iTOW = COMBINE_FOUR_EIGHT_BIT(gps_dev->buf[i + NAV_TIMEGPS_iTOW_OFFSET + 3],
                                                           gps_dev->buf[i + NAV_TIMEGPS_iTOW_OFFSET + 2],
                                                           gps_dev->buf[i + NAV_TIMEGPS_iTOW_OFFSET + 1],
                                                           gps_dev->buf[i + NAV_TIMEGPS_iTOW_OFFSET]);
                    gps_dev->iTOW /= 1000; // convert to second
                    gps_dev->fTOW = COMBINE_FOUR_EIGHT_BIT(gps_dev->buf[i + NAV_TIMEGPS_fTOW_OFFSET + 3],
                                                           gps_dev->buf[i + NAV_TIMEGPS_fTOW_OFFSET + 2],
                                                           gps_dev->buf[i + NAV_TIMEGPS_fTOW_OFFSET + 1],
                                                           gps_dev->buf[i + NAV_TIMEGPS_fTOW_OFFSET]);
                    gps_dev->week = COMBINE_TWO_EIGHT_BIT(gps_dev->buf[i + NAV_TIMEGPS_WEEK_OFFSET + 1],
                                                          gps_dev->buf[i + NAV_TIMEGPS_WEEK_OFFSET]);
                    gps_dev->leap_sec = gps_dev->buf[i + NAV_TIMEGPS_LEAP_OFFSET];
                    gps_dev->valid = gps_dev->buf[i + NAV_TIMEGPS_VALID_OFFSET];
                    gps_dev->tAcc = gps_dev->buf[i + NAV_TIMEGPS_tAcc_OFFSET];
                } else {
                    DEV_ERR("UBX-NAV-TIMEGPS not valid! (valid: 0x%08X)", gps_dev->buf[i + NAV_TIMEGPS_VALID_OFFSET]);
                }
            }
        } else if (gps_dev->buf[i] == '$') {
            /* NMEA */
            DEV_DBG("Parsing NMEA message");
            if (strncmp((char *)&gps_dev->buf[i + 3], "GSA", 3) == 0) {
                DEV_DBG("Talker ID: %.2s, Class: %.3s, Op mode: %c, Nav mode: %c", &gps_dev->buf[i + 1],
                        &gps_dev->buf[i + 3], gps_dev->buf[i + 7], gps_dev->buf[i + 9]);
                /* Get position fix mode */
                switch (gps_dev->buf[i + NMEA_GSA_NAV_OFFSET]) {
                    case '1':
                        gps_dev->mode = NO_POS_FIX;
                        break;
                    case '2':
                        gps_dev->mode = POS_2D_GNSS_FIX;
                        break;
                    case '3':
                        gps_dev->mode = POS_3D_GNSS_FIX;
                        break;
                    default:
                        gps_dev->mode = POS_FIX_NUM;
                }
            }
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
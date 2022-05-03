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
    /* Read from device */
    if ((gps_dev->size = read(gps_dev->fd, gps_dev->buf, DEV_RD_BUF_SIZE))
            <= 0) {
        DEV_ERR("Failed to read %s (size: %d)", gps_dev->name, gps_dev->size);
        return -1;
    }

    if (gps_dev->buf[gps_dev->size - 2] != 0x0D &&
            gps_dev->buf[gps_dev->size - 1] != 0x0A) {
        DEV_DBG("Incomplete message");
    }

    gps_dev->offset = 0;

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
    int j = 0;

    if (gps_dev->size <= 0) {
        DEV_ERR("Size is %d. Nothing to parse!", gps_dev->size);
    }

    /* Reset locked satellites */
    i = gps_dev->offset;
    gps_dev->locked_sat = 0;
    DEV_DBG("Start parsing (size: %d)", gps_dev->size);
    while (i < gps_dev->size) {
        if (gps_dev->buf[i] == 0xB5) {
            DEV_DBG("UBX index: %d", i);
            /* UBX message */
            if ((i + NAV_TIMEGPS_tAcc_OFFSET) >= gps_dev->size) {
                DEV_ERR("UBX-NAV-TIMEGPS message is incomplete! (size: %d)", gps_dev->size);
                for (j = i; j < gps_dev->size; j++) {
                    printf("0x%X ", gps_dev->buf[j]);
                }
                printf("\n");
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
                    DEV_DBG("iTOW: %u", gps_dev->iTOW);
                } else {
                    DEV_ERR("UBX-NAV-TIMEGPS not valid! (valid: 0x%08X)", gps_dev->buf[i + NAV_TIMEGPS_VALID_OFFSET]);
                }
            }
        } else if (gps_dev->buf[i] == '$') {
            /* NMEA */
            if (strncmp((char *)&gps_dev->buf[i + 3], "GSA", 3) == 0) {
                DEV_DBG("GSA index: %d", i);
                DEV_DBG("Talker ID: %.2s, Class: %.3s, Op mode: %c, Nav mode: %c",
                        &gps_dev->buf[i + NMEA_TID_OFFSET], &gps_dev->buf[i + NMEA_CLASS_OFFSET],
                        gps_dev->buf[i + NMEA_GSA_OP_OFFSET], gps_dev->buf[i + NMEA_GSA_NAV_OFFSET]);
                DEV_DBG("SVID: %.36s", &gps_dev->buf[i + NMEA_GSA_SVID_OFFSET]);
                
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

                /* Count number of locked satellites
                 * TODO: use delimeter instead */
                j = i + NMEA_GSA_SVID_OFFSET;
                while (j < gps_dev->size) {
                    if (gps_dev->locked_sat == NMEA_GSA_MAX_SATELITTES) {
                        break;
                    }

                    if (gps_dev->buf[j] == ',') {
                        if (((j + 1) < gps_dev->size) && gps_dev->buf[j + 1] == ',') {
                            break;
                        }

                        gps_dev->locked_sat++;
                    }

                    j++;
                }
            } else if (strncmp((char *)&gps_dev->buf[i + 3], "GNS", 3) == 0) {
                DEV_DBG("%s", &gps_dev->buf[i]);
            }
        }

        i++;
    }
    DEV_DBG("Parsing done");

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
#include "device.h"
#include "ubx.h"

void *device_read_thrd(void *addr);

/**
 * @brief Initialize GPS device and configure message rate
 * 
 * @param gps_dev store file decsriptor
 * @param name path of the device
 * @return int 
 */
int device_init(DeviceInfo *gps_dev, int log_lvl)
{
    /* Set log level */
    gps_dev->log_lvl = log_lvl;

    /* Open the device */
    gps_dev->fd = open(gps_dev->name, O_RDWR);
    if (gps_dev->fd == -1) {
        DEV_ERR(gps_dev->log_lvl, "[%s] Cannot open %s\n", __FILE__, gps_dev->name);
        return -1;
    }

    /* Poll UBX-NAV-TIMETGPS once per second */
    if (ubx_set_msg_rate(gps_dev->fd, NMEA_CLASS_STD, NMEA_ID_GSV, UBX_CFG_MSG_OFF)) {
        DEV_ERR(gps_dev->log_lvl, "Failed to set GSV message rate");
        return -1;
    }

    if (ubx_set_msg_rate(gps_dev->fd, NMEA_CLASS_STD, NMEA_ID_GLL, UBX_CFG_MSG_OFF)) {
        DEV_ERR(gps_dev->log_lvl, "Failed to set GLL message rate");
        return -1;
    }

    if (ubx_set_msg_rate(gps_dev->fd, NMEA_CLASS_STD, NMEA_ID_ZDA, UBX_CFG_MSG_OFF)) {
        DEV_ERR(gps_dev->log_lvl, "Failed to set ZDA message rate");
        return -1;
    }

    if (ubx_set_msg_rate(gps_dev->fd, UBX_CLASS_NAV, UBX_ID_NAV_TIMEGPS, UBX_CFG_MSG_ON)) {
        DEV_ERR(gps_dev->log_lvl, "Failed to set UBX-NAV-TIMETGPS message rate");
        return -1;
    }

    if (ubx_set_msg_rate(gps_dev->fd, NMEA_CLASS_STD, NMEA_ID_GSA, UBX_CFG_MSG_ON)) {
        DEV_ERR(gps_dev->log_lvl, "Failed to set GSA message rate");
        return -1;
    }

    if (ubx_set_msg_rate(gps_dev->fd, NMEA_CLASS_STD, NMEA_ID_GNS, UBX_CFG_MSG_ON)) {
        DEV_ERR(gps_dev->log_lvl, "Failed to set GNS message rate");
        return -1;
    }

    /* Start the thread to read messages from the U-blox */
    gps_dev->rd_buf.avail = DEV_RD_BUF_SIZE;
    if (pthread_create(&gps_dev->tid, NULL, device_read_thrd, gps_dev)) {
        DEV_ERR(gps_dev->log_lvl, "Failed to create the reading thread");
        return -1;
    }

    DEV_INFO(gps_dev->log_lvl, "Successfully opened %s", gps_dev->name);

    return 0;
}

void *device_read_thrd(void *addr)
{
#define DEV_MAX_POLL_RETRY 5
    DeviceInfo *gps_dev = (DeviceInfo *)addr;
    RdBufInfo *rd_buf = &gps_dev->rd_buf;
    struct pollfd fds;
    int retry_poll = 0;
    int ret = 0;

    DEV_INFO(gps_dev->log_lvl, "Starting reading thread %ld", gps_dev->tid);
    
    /* Init poll info */
    memset(&fds, 0, sizeof(fds));
    fds.fd = gps_dev->fd;
    fds.events = POLLIN;

    /* Set invalid_rd in the beginning since no data yet */
    rd_buf->invalid_rd = 1;

    while (!gps_dev->thrd_stop) {
        /* Poll first */
gps_poll:
        ret = poll(&fds, 1, 1000);
        if (ret == 0) {
            /* timeout occurred */
            DEV_ERR(gps_dev->log_lvl, "Poll timeout occurred!");
            if (retry_poll < DEV_MAX_POLL_RETRY) {
                retry_poll++;
                goto gps_poll;
            } else {
                DEV_ERR(gps_dev->log_lvl, "Poll max retry time %d reached. Terminating the thread!",
                        DEV_MAX_POLL_RETRY);
                pthread_exit(NULL);
            }
        } else if (ret < 0) {
            /* Error occurred */
            DEV_ERR(gps_dev->log_lvl, "Polling failed!");
            pthread_exit(NULL);
        } else {
            /* Normal case */
            retry_poll = 0;
        }

        /* Reading buffer */
        if ((rd_buf->size = read(gps_dev->fd, rd_buf->buf + rd_buf->wr_offset, rd_buf->avail))
            < 0) {
            DEV_ERR(gps_dev->log_lvl, "Failed to read %s (size: %d)", gps_dev->name, gps_dev->size);
            return NULL;
        }

        if (rd_buf->size == 0) {
            DEV_INFO(gps_dev->log_lvl, "Nothing was read!");
            continue;
        }

        rd_buf->wr_offset += rd_buf->size;
        if (rd_buf->wr_offset > rd_buf->rd_offset) {
            rd_buf->avail = DEV_RD_BUF_SIZE - rd_buf->wr_offset;
            rd_buf->invalid_rd = 0;
        } else if (rd_buf->wr_offset < rd_buf->rd_offset) {
            rd_buf->avail = rd_buf->rd_offset - rd_buf->wr_offset - 1;

            if (rd_buf->avail == 0) {
                /* Set read to invalid and reset read and write offset */
                rd_buf->invalid_rd = 1;
                rd_buf->wr_offset = rd_buf->rd_offset;
                rd_buf->avail = DEV_RD_BUF_SIZE - rd_buf->wr_offset;
                DEV_INFO(gps_dev->log_lvl, "Invalidate read");
            }
        }
        
        /* Check if buffer is full */
        if (rd_buf->avail == 0) {
            rd_buf->wr_offset = 0;
            rd_buf->avail = DEV_RD_BUF_SIZE;
        }
    }

    DEV_INFO(gps_dev->log_lvl, "Reading thread %ld was stopped!", gps_dev->tid);

    return NULL;
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
        DEV_ERR(gps_dev->log_lvl, "Failed to read %s (size: %d)", gps_dev->name, gps_dev->size);
        return -1;
    }

    if (gps_dev->buf[gps_dev->size - 2] != 0x0D &&
            gps_dev->buf[gps_dev->size - 1] != 0x0A) {
        DEV_DBG(gps_dev->log_lvl, "Incomplete message");
    }

    gps_dev->offset = 0;

    return 0;
}

int device_print(DeviceInfo *gps_dev)
{
    RdBufInfo *rd_buf = &gps_dev->rd_buf;

    /* wait till read is valid */
    while (rd_buf->invalid_rd && !gps_dev->thrd_stop) {
        DEV_ERR(gps_dev->log_lvl, "Read is currently not available. Sleep 1 sec");
        sleep(1);
    }

    /* Print 128 bytes at a time */
    printf("%.128s", rd_buf->buf + rd_buf->rd_offset);
    rd_buf->rd_offset += 128;
    if (rd_buf->rd_offset == DEV_RD_BUF_SIZE) {
        rd_buf->rd_offset = 0;
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
    static int prev_iTOW = -1;
    RdBufInfo *rd_buf = &gps_dev->rd_buf;
    int i = rd_buf->rd_offset;
    int rd_end = rd_buf->wr_offset;
    int j = 0;

    DEV_DBG(gps_dev->log_lvl, "read index: %d", i);

    while (rd_buf->invalid_rd) {
        /* wait till read is valid */
        usleep(500000);
    }

    /* Reset locked satellites */
    gps_dev->locked_sat = 0;

    /* Get UBX first */
find_ubx:
    while (i != rd_end) {
        if (rd_buf->buf[i] == 0xB5) {
            // start of UBX
           break;
        }

        i = (i + 1) % DEV_RD_BUF_SIZE;
    }

    int ubx_end = i + NAV_TIMEGPS_MSG_LEN;
    if ((i < rd_end && ubx_end >= rd_end)
        || (i > rd_end && ubx_end > DEV_RD_BUF_SIZE && (ubx_end % DEV_RD_BUF_SIZE) >= rd_end)) {
        DEV_ERR(gps_dev->log_lvl, "Wait 0.3s for UBX message");
        usleep(300000);
        rd_end = rd_buf->wr_offset;
        goto find_ubx;
    }

    if (rd_buf->buf[i + UBX_IDX_ClASS] == UBX_CLASS_NAV &&
                rd_buf->buf[i + UBX_IDX_ID] == UBX_ID_NAV_TIMEGPS) {
                /* Validate bits first
                 * bit 0: TOW
                 * bit 1: Week
                 * bit 2: Leap sec */
        gps_dev->iTOW = COMBINE_FOUR_EIGHT_BIT(rd_buf->buf[(i + NAV_TIMEGPS_iTOW_OFFSET + 3) % DEV_RD_BUF_SIZE],
                                                rd_buf->buf[(i + NAV_TIMEGPS_iTOW_OFFSET + 2) % DEV_RD_BUF_SIZE],
                                                rd_buf->buf[(i + NAV_TIMEGPS_iTOW_OFFSET + 1) % DEV_RD_BUF_SIZE],
                                                rd_buf->buf[(i + NAV_TIMEGPS_iTOW_OFFSET) % DEV_RD_BUF_SIZE]);
        gps_dev->iTOW /= 1000; // convert to second
        gps_dev->fTOW = COMBINE_FOUR_EIGHT_BIT(rd_buf->buf[(i + NAV_TIMEGPS_fTOW_OFFSET + 3) % DEV_RD_BUF_SIZE],
                                                rd_buf->buf[(i + NAV_TIMEGPS_fTOW_OFFSET + 2) % DEV_RD_BUF_SIZE],
                                                rd_buf->buf[(i + NAV_TIMEGPS_fTOW_OFFSET + 1) % DEV_RD_BUF_SIZE],
                                                rd_buf->buf[(i + NAV_TIMEGPS_fTOW_OFFSET) % DEV_RD_BUF_SIZE]);
        gps_dev->week = COMBINE_TWO_EIGHT_BIT(rd_buf->buf[(i + NAV_TIMEGPS_WEEK_OFFSET + 1) % DEV_RD_BUF_SIZE],
                                                rd_buf->buf[(i + NAV_TIMEGPS_WEEK_OFFSET) % DEV_RD_BUF_SIZE]);
        gps_dev->leap_sec = rd_buf->buf[(i + NAV_TIMEGPS_LEAP_OFFSET) % DEV_RD_BUF_SIZE];
        gps_dev->valid = rd_buf->buf[(i + NAV_TIMEGPS_VALID_OFFSET) % DEV_RD_BUF_SIZE];
        gps_dev->tAcc = rd_buf->buf[(i + NAV_TIMEGPS_tAcc_OFFSET) % DEV_RD_BUF_SIZE];
        DEV_DBG(gps_dev->log_lvl, "iTOW: %u", gps_dev->iTOW);

        if (gps_dev->valid != NAV_TIMEGPS_VALID) {
            gps_dev->invalid_timegps_cnt++;
            DEV_ERR(gps_dev->log_lvl, "UBX-NAV-TIMEGPS not valid! (valid: 0x%08X, iTOW: %u, week: %u, leap: %u)",
                    gps_dev->valid, gps_dev->iTOW, gps_dev->week, gps_dev->leap_sec);
        }
    }
    /* Parse the GSA msg */
    char nmea_class[3];
    DEV_DBG(gps_dev->log_lvl, "i: %d, rd_end: %d", i, rd_end);
    while (i != rd_end) {
        if (rd_buf->buf[i] == '$') {
            nmea_class[0] = (char)rd_buf->buf[(i + 3) % DEV_RD_BUF_SIZE];
            nmea_class[1] = (char)rd_buf->buf[(i + 4) % DEV_RD_BUF_SIZE];
            nmea_class[2] = (char)rd_buf->buf[(i + 5) % DEV_RD_BUF_SIZE];
            DEV_DBG(gps_dev->log_lvl, "NMEA class: %s", nmea_class);
        }

        if (strncmp(nmea_class, "GSA", 3) == 0) {
            int nmea_end = i + NMEA_GSA_MAX_LEN;
check_nmea_end:
            if ((i < rd_end && nmea_end >= rd_end)
                || (i > rd_end && nmea_end > DEV_RD_BUF_SIZE &&
                    (nmea_end % DEV_RD_BUF_SIZE) >= rd_end)) {
                DEV_ERR(gps_dev->log_lvl, "Wait 0.3s for NMEA GSA message (rd_end: %d, nmea_end: %d)",
                        rd_end, nmea_end);
                usleep(300000);
                rd_end = rd_buf->wr_offset;
                goto check_nmea_end;
            }

            /* Get operation mode */
            gps_dev->mode = atoi((char *)&rd_buf->buf[i + NMEA_GSA_NAV_OFFSET]);

            /* Count number of satellites */
            j = i + NMEA_GSA_SVID_OFFSET;
            while (gps_dev->locked_sat < NMEA_GSA_MAX_SATELITTES) {
                if (rd_buf->buf[j] == ',') {
                    if (rd_buf->buf[(j + 1) % DEV_RD_BUF_SIZE] == ',') {
                        /* No more satelittes to read */
                        break;
                    }

                    gps_dev->locked_sat++;
                }

                j = (j + 1) % DEV_RD_BUF_SIZE;
            }

            break;
        } else if (strncmp(nmea_class, "GNS", 3) == 0) {
            DEV_DBG(gps_dev->log_lvl, "%.64s", (char *)&rd_buf->buf[i]);
        }

        i = (i + 1) % DEV_RD_BUF_SIZE;
    }

    /* Update rd_offset */
    DEV_DBG(gps_dev->log_lvl, "i: %d, j: %d", i, j);
    rd_buf->rd_offset = i;

    if (prev_iTOW != -1 && (prev_iTOW + 1) != gps_dev->iTOW) {
        gps_dev->iTOW_err_cnt++;
        DEV_ERR(gps_dev->log_lvl, "Time inconsistent! (prev: %u, curr: %u)", prev_iTOW, gps_dev->iTOW);
    }

    prev_iTOW = gps_dev->iTOW;

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
    /* Join the thread first */
    gps_dev->thrd_stop = 1;
    if (pthread_join(gps_dev->tid, NULL)) {
        DEV_ERR(gps_dev->log_lvl, "Failed to join the reading thread");
    }

    if (close(gps_dev->fd) < 0) {
        DEV_ERR(gps_dev->log_lvl, "Failed to close %s w/ file descriptor = %d", gps_dev->name, gps_dev->fd);
        return -1;
    }

    gps_dev->fd = -1;

    DEV_INFO(gps_dev->log_lvl, "Closed device %s", gps_dev->name);

    return 0;
}
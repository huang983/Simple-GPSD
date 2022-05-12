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

    DEV_INFO(gps_dev->log_lvl, "Successfully opened %s", gps_dev->name);

    return 0;
}

int device_start_read_thrd(DeviceInfo *gps_dev)
{
    /* Start the thread to read messages from the U-blox */
    gps_dev->rd_buf.avail = DEV_RD_BUF_SIZE;
    if (pthread_create(&gps_dev->tid, NULL, device_read_thrd, gps_dev)) {
        DEV_ERR(gps_dev->log_lvl, "Failed to create the reading thread");
        return -1;
    }

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

static int wait_for_complete_msg(DeviceInfo *gps_dev, int rd_start, int rd_end)
{
    RdBufInfo *rd_buf = &gps_dev->rd_buf;

    while ((rd_start < rd_buf->wr_offset && rd_end >= rd_buf->wr_offset) ||
            (rd_start > rd_buf->wr_offset && rd_end > DEV_RD_BUF_SIZE &&
            (rd_end % DEV_RD_BUF_SIZE) >= rd_buf->wr_offset)) {
        if (gps_dev->thrd_stop) {
            break;
        }

        usleep(300000);
    }

    return rd_buf->wr_offset;
}

#define extract_field_from_string(str, field_no) ({ \
                                            char *field = NULL; \
                                            field = strtok(str, ","); \
                                            for (int fno = 0; fno < field_no; fno++) { \
                                                field = strtok(NULL, ","); \
                                            } \
                                            field; \
                                          })

int parse_ubx(DeviceInfo *gps_dev)
{
    RdBufInfo *rd_buf = &gps_dev->rd_buf;
    static int prev_iTOW = -1;
    static int prev_iTOW_idx = -1;
    int curr_iTOW_idx = -1;
    int i = rd_buf->rd_offset;
    int rd_end = rd_buf->wr_offset;

    /* Wait till read is valid */
    while (rd_buf->invalid_rd) {
        usleep(300000);
    }

    /* Find the beginning of UBX message */
    while (i != rd_end) {
        int sync1_idx = (i + DEV_RD_BUF_SIZE - 3) % DEV_RD_BUF_SIZE;
        int sync2_idx = (i + DEV_RD_BUF_SIZE - 2) % DEV_RD_BUF_SIZE;
        int class_idx = (i + DEV_RD_BUF_SIZE - 1) % DEV_RD_BUF_SIZE;
        int id_idx = i;
        if (rd_buf->buf[sync1_idx] == UBX_SYNC1 && rd_buf->buf[sync2_idx] == UBX_SYNC2
            && rd_buf->buf[class_idx] == UBX_CLASS_NAV && rd_buf->buf[id_idx] && UBX_ID_NAV_TIMEGPS) {
            // start of UBX
            i = sync1_idx;
            rd_end = wait_for_complete_msg(gps_dev, i, i + NAV_TIMEGPS_MSG_LEN); // make sure writer gets all the data
            break;
        }

        i = (i + 1) % DEV_RD_BUF_SIZE;
    }

    if (i == rd_end) {
        DEV_INFO(gps_dev->log_lvl, "Couldn't find UBX msg");
        return -1;
    }


    // sanity check
    if (rd_buf->buf[(i + UBX_IDX_ClASS) % DEV_RD_BUF_SIZE] == UBX_CLASS_NAV &&
                rd_buf->buf[(i + UBX_IDX_ID) % DEV_RD_BUF_SIZE] == UBX_ID_NAV_TIMEGPS) {
        curr_iTOW_idx = (i + NAV_TIMEGPS_iTOW_OFFSET) % DEV_RD_BUF_SIZE;
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

        /* Validate bits first
        * bit 0: TOW
        * bit 1: Week
        * bit 2: Leap sec */
        if (gps_dev->valid != NAV_TIMEGPS_VALID) {
            gps_dev->invalid_timegps_cnt++;
            DEV_ERR(gps_dev->log_lvl, "UBX-NAV-TIMEGPS not valid! (valid: 0x%08X, iTOW: %u, week: %u, leap: %u)",
                    gps_dev->valid, gps_dev->iTOW, gps_dev->week, gps_dev->leap_sec);
        }
    } else {
        DEV_INFO(gps_dev->log_lvl, "Unexpected UBX message - class @ %d: 0x%X, ID @ %d: 0x%X",
                    i + UBX_IDX_ClASS, rd_buf->buf[i + UBX_IDX_ClASS],
                    i + UBX_IDX_ID, rd_buf->buf[i + UBX_IDX_ID]);
    }

    if (prev_iTOW != -1 && (prev_iTOW + 1) != gps_dev->iTOW) {
        gps_dev->iTOW_err_cnt++;
        DEV_ERR(gps_dev->log_lvl, "Time inconsistent! (prev @ %d: %u, curr @ %d: %u)", prev_iTOW_idx, prev_iTOW,
                                                                                    curr_iTOW_idx, gps_dev->iTOW);
    }

    prev_iTOW = gps_dev->iTOW;
    prev_iTOW_idx = curr_iTOW_idx;
    rd_buf->rd_offset = (i + NAV_TIMEGPS_MSG_LEN) % DEV_RD_BUF_SIZE;

    return 0;
}

int parse_nmea(DeviceInfo *gps_dev)
{
    RdBufInfo *rd_buf = &gps_dev->rd_buf;
    char nmea_id[3];
    char nmea_class[4];
    int gsa_parsed = 0;
    int gsv_parsed = 0;
    int rd_end = rd_buf->wr_offset;
    int i = rd_buf->rd_offset;
    int j = 0;

    /* Wait till read is valid */
    while (rd_buf->invalid_rd) {
        usleep(300000);
    }

    /* Reset locked satellites */
    gps_dev->locked_sat = 0;
    
    /* Parse the GSA and GSV msg */
    DEV_DBG(gps_dev->log_lvl, "i: %d, rd_end: %d", i, rd_end);
    while (i != rd_end) {
        if (rd_buf->buf[i] == '$') {
            rd_end = wait_for_complete_msg(gps_dev, i, i + NMEA_MSG_LEN); // make sure writer gets all the data
            nmea_id[0] = (char)rd_buf->buf[(i + 1) % DEV_RD_BUF_SIZE];
            nmea_id[1] = (char)rd_buf->buf[(i + 2) % DEV_RD_BUF_SIZE];
            nmea_id[2] = 0;
            nmea_class[0] = (char)rd_buf->buf[(i + 3) % DEV_RD_BUF_SIZE];
            nmea_class[1] = (char)rd_buf->buf[(i + 4) % DEV_RD_BUF_SIZE];
            nmea_class[2] = (char)rd_buf->buf[(i + 5) % DEV_RD_BUF_SIZE];
            nmea_class[3] = 0;
            DEV_DBG(gps_dev->log_lvl, "NMEA class: %s, NMEA id: %s", nmea_class, nmea_id);
            
            if (strncmp(nmea_class, "GSA", 3) == 0) {                
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
                gsa_parsed = 1;
            } else if (strncmp(nmea_class, "GNS", 3) == 0) {
                DEV_DBG(gps_dev->log_lvl, "%.64s", (char *)&rd_buf->buf[i]);
            } else if (strncmp(nmea_class, "GSV", 3) == 0) {
                char gsv_buf[NMEA_GSV_MAX_LEN];
                int first_cp_sz = (i + NMEA_GSV_MAX_LEN) > DEV_RD_BUF_SIZE ?
                                (DEV_RD_BUF_SIZE - i) : NMEA_GSV_MAX_LEN;
                int second_cp_sz = first_cp_sz == NMEA_GSV_MAX_LEN ?
                                0 : (i + NMEA_GSV_MAX_LEN) % DEV_RD_BUF_SIZE;
                char *numSV_str = NULL;
                
                DEV_DBG(gps_dev->log_lvl, "Parsing GSV");

                memcpy(gsv_buf, (char *)&rd_buf->buf[i], first_cp_sz);
                if (second_cp_sz) {
                    memcpy(gsv_buf + first_cp_sz, (char *)&rd_buf->buf[0], second_cp_sz);
                }
                numSV_str = extract_field_from_string(gsv_buf, NMEA_GSV_numSV_FNO);
                
                if (numSV_str == NULL) {
                    DEV_ERR(gps_dev->log_lvl, "Cannot parse satellites in view");
                } else  {
                    if (strcmp(nmea_id, "GP") == 0) {
                        gps_dev->gps_sat_in_view = atoi(numSV_str);
                        DEV_DBG(gps_dev->log_lvl, "gps_sat_in_view: %d", gps_dev->gps_sat_in_view);
                    } else if (strcmp(nmea_id, "GL") == 0) {
                        gps_dev->gln_sat_in_view = atoi(numSV_str);
                        DEV_DBG(gps_dev->log_lvl, "gln_sat_in_view: %d", gps_dev->gln_sat_in_view);
                    } else if (strcmp(nmea_id, "GA") == 0) {
                        gps_dev->gal_sat_in_view = atoi(numSV_str);
                    } else if (strcmp(nmea_id, "GB") == 0) {
                        gps_dev->bd_sat_in_view = atoi(numSV_str);
                    }
                }
                
                i = (i + first_cp_sz + second_cp_sz) % DEV_RD_BUF_SIZE;
                gsv_parsed = 1;
            }

            if (gsa_parsed) {
                if ((gps_dev->sat_in_view_enable && gsv_parsed) || !gps_dev->sat_in_view_enable) {
                    break;
                }
            }
        }

        i = (i + 1) % DEV_RD_BUF_SIZE;
    }

    /* Update rd_offset */
    DEV_DBG(gps_dev->log_lvl, "i: %d, j: %d", i, j);
    rd_buf->rd_offset = i;

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
    /* Get UBX first */
    while (parse_ubx(gps_dev));

    /* Get NMEA */
    while (parse_nmea(gps_dev));

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
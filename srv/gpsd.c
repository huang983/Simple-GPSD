#include "gpsd.h" // libaries, print macros, and data structures
#include "ubx.h" // UBX-related APIs

GpsdData g_gpsd_data;
GpsdBuf gpsd_buf;

/* UBX protocols */
uint8_t ubx_nav_time_gps[] = { 0xB5,0x62, 0x01,0x20, 0x00,0x00, 0x2C,0x83 };
uint8_t ubx_cfg_msg[] = { 0xB5,0x62, 0x06,0x01, 0x03,0x00, 0x01,0x20,0x01, 0x2C,0x83 };
uint8_t ubx_cfg_inf[] = { 0xB5,0x62, 0x06,0x02, 0x00,0x00, 0x08,0x1E };
uint8_t ubx_mon_ver[] = { 0xB5,0x62,0x0A,0x04,0x00,0x00,0x0E,0x34 };
uint8_t ubx_cfg_prt[] = { 0xB5,0x62, 0x06,0x00, 0x00,0x00, 0x06,0x18 };

void sig_handler(int sig)
{
    GpsdData *gpsd = &g_gpsd_data;

    gpsd->stop = 1;
}

static void __attribute__((unused)) test_log()
{
    GPSD_INFO("Hello %d + %d = %d", 1, 1, (1 + 1));
    GPSD_ERR("Hello %d", 2);
    GPSD_DBG("Hello%s", ", I'm Tim :)");
}

int main(int argc, char **argv)
{
    GpsdData *gpsd = &g_gpsd_data;
    DeviceInfo *gps_dev = &gpsd->gps_dev;
    int size = 0;
    uint8_t *buf;
    char *buf_ptr = NULL;
    int i = 0;

    if (argc < 2) {
        GPSD_INFO("Please provide device path!");
        return 0;
    }
    
    /* Iniialize GPSD data */
    memset(gpsd, 0, sizeof(*gpsd));
    memset(&gpsd_buf, 0, sizeof(gpsd_buf));
    gpsd_buf.curr_type = GPSD_MSG_NUM;

    /* Initialize GPS device */
    strncpy(gpsd->gps_dev.name, argv[1], sizeof(gpsd->gps_dev.name));
    if (device_init(gps_dev)) {
        exit(0);
    }

#ifdef SCKT_ENABLE
    /* Set up Unix-domain socket connection */
    if (socket_server_init(&gpsd->srv)) {
        exit(0);
    }

    GPSD_INFO("Socket setup is successful");
#endif


    /* Stop the program by CTRL+C */
    signal(SIGINT, sig_handler);

    buf = calloc(RD_BUFSIZE, sizeof(*buf) * RD_BUFSIZE);
    if (buf == NULL) {
        GPSD_ERR("[%s] Failed to allocate (err: %s)\n", __FILE__, strerror(errno));
    }

    while (!gpsd->stop) {
        if (device_read(gps_dev)) {
            GPSD_ERR("Failed to read GPS device");
        }

        if (device_parse(gps_dev)) {
            GPSD_ERR("Failed to parse GPS message");
        } else {
            GPSD_INFO("TOW: %u, Week: %u, Leap sec: %u, Valid: 0x%X",
                        gps_dev->iTOW, gps_dev->week, gps_dev->leap_sec, gps_dev->valid);
        }

#if 0
        if (gpsd_buf.nmea_idx == 512 && gpsd_buf.ubx_idx == 512) {
            break;
        }

        size = read(gpsd->gps_dev.fd, buf, RD_BUFSIZE);
        buf[size] = '\0';

        if (size <= 0) {
            GPSD_ERR("[%s] Cannot read fd %d (err: %s)\n", __FILE__, gpsd->gps_dev.fd, strerror(errno));
        } else if (errno == EINTR) {
            GPSD_ERR("[%s] Read operation was interrupted (err: %s)\n", __FILE__, strerror(errno));
        }

        for (i = 0; i < size; i++) {
            if (buf[i] == 0xB5) {
                GPSD_DBG("UBX message:");
                gpsd_buf.curr_type = GPSD_MSG_UBX;
            } else if (buf[i] == '$') {
                GPSD_DBG("NMEA message:");
                gpsd_buf.curr_type = GPSD_MSG_NMEA;
            }

            if ((gpsd_buf.curr_type == GPSD_MSG_NMEA && gpsd_buf.nmea_idx == 512) ||
                    (gpsd_buf.curr_type == GPSD_MSG_UBX && gpsd_buf.ubx_idx == 512) ||
                        gpsd_buf.curr_type == GPSD_MSG_NUM) {
                continue;
            }
            GPSD_DBG("nmea: %d, ubx: %d", gpsd_buf.nmea_idx, gpsd_buf.ubx_idx);
            buf_ptr = (gpsd_buf.curr_type == GPSD_MSG_NMEA) ?
                        gpsd_buf.nmea + gpsd_buf.nmea_idx :
                        gpsd_buf.ubx + gpsd_buf.ubx_idx;
            memcpy(buf_ptr, &buf[i], 1);
            if (gpsd_buf.curr_type == GPSD_MSG_UBX) {
                gpsd_buf.ubx_idx++;
            } else {
                gpsd_buf.nmea_idx++;
            }
        }

        sleep(1); // sleep 1 second to simulate PPS interrupt
#endif
    }
    
    // GPSD_INFO("%s", gpsd_buf.nmea);
    
    free(buf);

#ifdef SCKT_ENABLE
    socket_server_close(&gpsd->srv);
#endif

    device_close(gps_dev);

    return 0;
}
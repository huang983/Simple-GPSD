#include "gpsd.h" // libaries, print macros, and data structures
#include "ubx.h" // UBX-related APIs
#include "nmea.h"

GpsdData g_gpsd_data;

void sig_handler(int sig)
{
    GpsdData *gpsd = &g_gpsd_data;

    gpsd->stop = 1;
}

static void usage(void)
{
    printf("usage: gpsd [OPTIONS] device\n\n\
  Options include: \n\
  -s       = Show time and position-fix status \n\
  -u       = Create Unix-domain socket (e.g. -u /tmp/gpsd.sock)\n\
  -d       = Set log level - 0: turn off all, 1: info, 2: error, 4: debug\n\
  -v       = Enable satelittes in view\n");
}

static void report(void)
{
    GpsdData *gpsd = &g_gpsd_data;
    DeviceInfo *gps_dev = &gpsd->gps_dev;

    GPSD_INFO(gpsd->log_lvl, "Inconsistent iTOW count: %u", gps_dev->iTOW_err_cnt);
    GPSD_INFO(gpsd->log_lvl, "Invalid NAV-TIMEGPS count: %u", gps_dev->invalid_timegps_cnt);
}

static int parse_args(int argc, char **argv)
{
    GpsdData *gpsd = &g_gpsd_data;
    DeviceInfo *gps_dev = &gpsd->gps_dev;
    // ServerSocket *srv = &gpsd->srv;
    const char *optstr = "su:d:v";
    int ch;

    /* Deault log level to 2 */
    gpsd->log_lvl = 2;

    /* Parse options */
    while ((ch = getopt(argc, argv, optstr)) != -1) {
        switch (ch) {
            case 's':
                /* Print parsing result */
                gpsd->show_result = 1;
                break;
            case 'u':
                /* Unix-domain socket */
                gpsd->socket_enable = 1;
                /* Hardcode socket file for now */
                // strncpy(srv->addr.sun_path, optarg, strlen(optarg));
                // GPSD_INFO(gpsd->log_lvl, "Socket file: %s", srv->addr.sun_path);
                break;
            case 'd':
                gpsd->log_lvl = atoi(optarg);
                break;
            case 'v':
                /* Enable satelittes in view */
                gps_dev->sat_in_view_enable = 1;
                break;
            default:
                usage();
                break;
        }
    }

    /* Get device name */
    if (optind >= argc) {
        GPSD_ERR(gpsd->log_lvl, "Please provide device path");
        GPSD_DBG(gpsd->log_lvl, "optind: %d, argc: %d", optind, argc);
        usage();
        return -1;
    }

    GPSD_DBG(gpsd->log_lvl, "Device: %s, size: %ld", argv[optind], sizeof(gps_dev->name));
    strncpy(gps_dev->name, argv[optind], sizeof(gps_dev->name));

    return 0;
}

int config_msg_rate(void)
{
    GpsdData *gpsd = &g_gpsd_data;
    DeviceInfo *gps_dev = &gpsd->gps_dev;

    /* Poll UBX-NAV-TIMETGPS once per second */
    if (ubx_set_msg_rate(gps_dev->fd, NMEA_CLASS_STD, NMEA_ID_VTG, UBX_CFG_MSG_OFF)) {
        DEV_ERR(gps_dev->log_lvl, "Failed to set VTG message rate");
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

    if (ubx_set_msg_rate(gps_dev->fd, NMEA_CLASS_STD, NMEA_ID_GSV, (gps_dev->sat_in_view_enable) ?
                                                                    UBX_CFG_MSG_ON : UBX_CFG_MSG_OFF)) {
        DEV_ERR(gps_dev->log_lvl, "Failed to set GSV message rate");
        return -1;
    }

    return 0;
}

int main(int argc, char **argv)
{
    GpsdData *gpsd = &g_gpsd_data;
    DeviceInfo *gps_dev = &gpsd->gps_dev;
    ServerSocket *srv = &gpsd->srv;
    int ret = 0;

    /* Iniialize GPSD data */
    memset(gpsd, 0, sizeof(*gpsd));

    /* Parse command-line options */
    if (parse_args(argc, argv)) {
        GPSD_ERR(gpsd->log_lvl, "Parsing failed. Exiting program!");
        exit(0);
    }

    /* Initialize GPS device */
    GPSD_DBG(gpsd->log_lvl, "Device: %s", gps_dev->name);
    if (device_init(gps_dev, gpsd->log_lvl)) {
        GPSD_ERR(gpsd->log_lvl, "Failed to open device");
        exit(0);
    }

    /* Turn on/off NMEA and UBX messages */
    if (config_msg_rate()) {
        goto close_device; // or retry? normally shouldn't fail
    }

    if (device_start_read_thrd(gps_dev)) {
        GPSD_ERR(gpsd->log_lvl, "Failed to start reading thread");
        goto close_device;  // or retry? normally shouldn't fail
    }

    if (gpsd->socket_enable) {
        /* Set up Unix-domain socket connection */
        if (socket_server_init(srv, gpsd->log_lvl)) {
            GPSD_ERR(gpsd->log_lvl, "Failed to start server");
            socket_server_close(srv);
            gpsd->socket_enable = 0;
        }

        GPSD_INFO(gpsd->log_lvl, "Socket setup is successful");
    }

    /* Stop the program by CTRL+C */
    signal(SIGINT, sig_handler);

    /* Start here */
    while (!gpsd->stop) {
        /* TODO: use ioctl to wait for PPS interrupt */
        sleep(1);

        if (device_parse(gps_dev)) {
            GPSD_ERR(gpsd->log_lvl, "Failed to parse GPS message");
        }

        if (gpsd->show_result) {
            GPSD_INFO(gpsd->log_lvl, "TOW: %u, Week: %u, Leap sec: %u, Valid: 0x%X, Pos fix mode: %d, Locked sat: %u",
                        gps_dev->iTOW, gps_dev->week, gps_dev->leap_sec, gps_dev->valid,
                        gps_dev->mode, gps_dev->locked_sat);
        }

        if (gps_dev->sat_in_view_enable) {
            GPSD_INFO(gpsd->log_lvl, "Satellites in view - GPS: %d, GLONASS: %d, Galileo: %d, Beidou: %d",
                        gps_dev->gps_sat_in_view, gps_dev->gln_sat_in_view,
                        gps_dev->gal_sat_in_view, gps_dev->bd_sat_in_view);
        }

        if (gpsd->socket_enable) {
            if (srv->client[0].fd == -1) {
                /* No client connection yet */
                socket_server_try_accept(srv);
            }

            if (srv->client[0].fd > 0) {
                ret = socket_try_read(srv->client[0].fd, gpsd->rd_buf, GPSD_BUFSIZE);
                if (ret > 0) {
                    /* TODO: parse client's query and send back the corresponding result */
                    char msg[5];

                    if (gps_dev->mode == POS_3D_GNSS_FIX &&
                                gps_dev->valid & NAV_TIMEGPS_VALID) {
                        sprintf(msg, "%d", GPS_TIME_FIXED);
                    } else if (gps_dev->mode == POS_2D_GNSS_FIX) {
                        sprintf(msg, "%d", GPS_POS_FIXED);
                    } else {
                        /* gps_dev->mode == NO_POS_FIX */
                        sprintf(msg, "%d", GPS_NOT_READY);
                    }

                    if (socket_write(srv->client[0].fd, msg) <= 0) {
                        GPSD_ERR(gpsd->log_lvl, "Failed to send to client %d", srv->client[0].fd);
                    }
                } else if (ret < 0) {
                    /* Client has closed the connection */
                    socket_client_close(&srv->client[0]);
                }
            }
        }
    }
        
    if (gpsd->socket_enable) {
        socket_server_close(srv);
    }

close_device:
    device_close(gps_dev);

    report();

    return 0;
}
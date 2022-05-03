#include "gpsd.h" // libaries, print macros, and data structures
#include "ubx.h" // UBX-related APIs
#include "nmea.h"

GpsdData g_gpsd_data;

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

static void usage(void)
{
    printf("usage: gpsd [OPTIONS] device\n\n\
  Options include: \n\
  -s       = Show time and position-fix status \n\
  -u       = Create Unix-domain socket\n");

}

static int parse_args(int argc, char **argv)
{
    GpsdData *gpsd = &g_gpsd_data;
    DeviceInfo *gps_dev = &gpsd->gps_dev;
    // ServerSocket *srv = &gpsd->srv;
    const char *optstr = "su:";
    int ch;

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
                // GPSD_INFO("Socket file: %s", srv->addr.sun_path);
                break;
            default:
                usage();
                break;
        }
    }

    /* Get device name */
    if (optind >= argc) {
        GPSD_ERR("Please provide device path (optind: %d, argc: %d)",
                    optind, argc);
        return -1;
    }

    GPSD_DBG("Device: %s, size: %ld", argv[optind], sizeof(gps_dev->name));
    strncpy(gps_dev->name, argv[optind], sizeof(gps_dev->name));

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
        GPSD_ERR("Parsing failed. Exiting program!");
        exit(0);
    }

    /* Initialize GPS device */
    GPSD_DBG("Device: %s", gps_dev->name);
    if (device_init(gps_dev)) {
        exit(0);
    }

    /* Poll UBX-NAV-TIMETGPS once per second */
    if (ubx_set_msg_rate(gps_dev->fd, NMEA_CLASS_STD, NMEA_ID_GSV, UBX_CFG_MSG_OFF)) {
        GPSD_ERR("Failed to set GSV message rate");
        goto close_device;
    }

    if (ubx_set_msg_rate(gps_dev->fd, NMEA_CLASS_STD, NMEA_ID_GLL, UBX_CFG_MSG_OFF)) {
        GPSD_ERR("Failed to set GLL message rate");
        goto close_device;
    }

    if (ubx_set_msg_rate(gps_dev->fd, NMEA_CLASS_STD, NMEA_ID_ZDA, UBX_CFG_MSG_OFF)) {
        GPSD_ERR("Failed to set ZDA message rate");
        goto close_device;
    }

    if (ubx_set_msg_rate(gps_dev->fd, UBX_CLASS_NAV, UBX_ID_NAV_TIMEGPS, UBX_CFG_MSG_ON)) {
        GPSD_ERR("Failed to set UBX-NAV-TIMETGPS message rate");
        goto close_device;
    }

    if (ubx_set_msg_rate(gps_dev->fd, NMEA_CLASS_STD, NMEA_ID_GSA, UBX_CFG_MSG_ON)) {
        GPSD_ERR("Failed to set GSA message rate");
        goto close_device;
    }

    if (ubx_set_msg_rate(gps_dev->fd, NMEA_CLASS_STD, NMEA_ID_GNS, UBX_CFG_MSG_ON)) {
        GPSD_ERR("Failed to set GNS message rate");
        goto close_device;
    }

    if (gpsd->socket_enable) {
        /* Set up Unix-domain socket connection */
        if (socket_server_init(srv)) {
            exit(0);
        }

        GPSD_INFO("Socket setup is successful");
    }

    /* Stop the program by CTRL+C */
    signal(SIGINT, sig_handler);

    /* Start here */
    while (!gpsd->stop) {
        if (device_read(gps_dev)) {
            GPSD_ERR("Failed to read GPS device");
        }

        if (device_parse(gps_dev)) {
            GPSD_ERR("Failed to parse GPS message");
        }

        if (gpsd->show_result) {
            GPSD_INFO("TOW: %u, Week: %u, Leap sec: %u, Valid: 0x%X, Pos fix mode: %d, Locked sat: %u",
                        gps_dev->iTOW, gps_dev->week, gps_dev->leap_sec, gps_dev->valid,
                        gps_dev->mode, gps_dev->locked_sat);
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
                    char res[5];

                    sprintf(res, "%d", gps_dev->mode);
                    if (socket_write(srv->client[0].fd, res) <= 0) {
                        GPSD_ERR("Failed to send to client %d", srv->client[0].fd);
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

    return 0;
}
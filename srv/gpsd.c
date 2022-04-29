#include "gpsd.h" // libaries, print macros, and data structures
#include "ubx.h" // UBX-related APIs

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
  -S       = Show time and position-fix status \n");

}

static int parse_args(int argc, char **argv)
{
    GpsdData *gpsd = &g_gpsd_data;
    DeviceInfo *gps_dev = &gpsd->gps_dev;
    const char *optstr = "S";
    int ch;

    /* Parse options */
    while ((ch = getopt(argc, argv, optstr)) != -1) {
        switch (ch) {
            case 'S':
                /* Print parsing result */
                gpsd->show_result = 1;
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

#ifdef SCKT_ENABLE
    /* Set up Unix-domain socket connection */
    if (socket_server_init(srv)) {
        exit(0);
    }

    GPSD_INFO("Socket setup is successful");
#endif


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

#ifdef SCKT_ENABLE
        if (socket_try_read(srv->fd, gpsd->rd_buf, GPSD_BUFSIZE) > 0) {
            /* TODO: parse client's query and send back the result */       
        }
#endif
    }
        
#ifdef SCKT_ENABLE
    socket_server_close(srv);
#endif

    device_close(gps_dev);

    return 0;
}
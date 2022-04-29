#include "gpsd.h" // libaries, print macros, and data structures
#include "ubx.h" // UBX-related APIs

GpsdData g_gpsd_data;
GpsdBuf gpsd_buf;

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

    /* Start here */
    while (!gpsd->stop) {
        if (device_read(gps_dev)) {
            GPSD_ERR("Failed to read GPS device");
        }

        if (device_parse(gps_dev)) {
            GPSD_ERR("Failed to parse GPS message");
        } else {
            GPSD_DBG("TOW: %u, Week: %u, Leap sec: %u, Valid: 0x%X, Pos hold mode: %d, Locked sat: %u",
                        gps_dev->iTOW, gps_dev->week, gps_dev->leap_sec, gps_dev->valid,
                        gps_dev->mode, gps_dev->locked_sat);
        }
    }
        
#ifdef SCKT_ENABLE
    socket_server_close(&gpsd->srv);
#endif

    device_close(gps_dev);

    return 0;
}
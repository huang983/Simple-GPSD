#include "gpsd.h" // libaries, print macros, and data structures
#include "ubx.h" // UBX-related APIs
#include "device.h" // device-related APIs
#include "socket.h" // socket-related APIs

GpsdData g_gpsd_data;

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
    int size = 0;
    uint8_t *buf;
    int i = 0;

    if (argc < 2) {
        GPSD_INFO("Please provide device path!");
        return 0;
    }
    
    /* Iniialize GPSD data */
    memset(gpsd, 0, sizeof(*gpsd));

    /* Initialize GPS device */
    strncpy(gpsd->dev_name, argv[1], sizeof(gpsd->dev_name));
    if (device_init(gpsd)) {
        exit(0);
    }

    /* Set up Unix-domain socket connection */
    if (socket_init(&gpsd->srv)) {
        exit(0);
    }

    GPSD_INFO("Socket set up is successful");

    signal(SIGINT, sig_handler);

    buf = calloc(RD_BUFSIZE, sizeof(*buf) * RD_BUFSIZE);
    if (buf == NULL) {
        printf("[%s] Failed to allocate (err: %s)\n", __FILE__, strerror(errno));
    }

    while (!gpsd->stop) {
        // printf("Reading...\n");
        size = read(gpsd->fd, buf, RD_BUFSIZE);
        // printf("Read success\n");
        buf[size] = '\0';

        if (size <= 0) {
            printf("[%s] Cannot read fd %d (err: %s)\n", __FILE__, gpsd->fd, strerror(errno));
        } else if (errno == EINTR) {
            printf("[%s] Read operation was interrupted (err: %s)\n", __FILE__, strerror(errno));
        }

        for (i = 0; i < size; i++) {
            if (buf[i] == 0xB5) {
                printf("\nUBX message:\n");
            } else if (buf[i] == '$') {
                printf("\nNMEA message:\n");
            }
            
            printf("0x%X ", buf[i]);
        }
    }
    
    free(buf);

    socket_server_close(&gpsd->srv);
    device_close(gpsd);

    return 0;
}
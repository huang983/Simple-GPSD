#include "gpsd.h"
#include "ubx.h"
#include "../include/client.h"
#include "../include/define.h"

typedef struct gpsd_data {
    int fd; // file descriptor
    socket_t srv_fd; // server's socket descriptor
    struct sockaddr_un srv_addr;
    socket_t clnt_fd[GPSD_UDS_MAX_CLIENT]; // client's socket descriptor
    struct sockaddr_un clnt_addr[GPSD_UDS_MAX_CLIENT];
    socklen_t clnt_addr_len[GPSD_UDS_MAX_CLIENT];
    int stop; // set to 1 by CTRL-C
} GpsdData;

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

/**
 * @brief Initialize Unix-domain socket
 * 
 * @return int 
 */
static int init_uds()
{
    GpsdData *gpsd = &g_gpsd_data;

    /* Create socket for local communication only */
    gpsd->srv_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (gpsd->srv_fd < 0)
    {
        GPSD_ERR("Failed to create socket");
        return GPSD_RET_FAILED;
    }


    gpsd->srv_addr.sun_family = AF_UNIX;
    strncpy(gpsd->srv_addr.sun_path, GPSD_UDS_FILE, sizeof(gpsd->srv_addr.sun_path));
    unlink(gpsd->srv_addr.sun_path); // delete a name/file if it's previously connected to another socket

    /* Assign a name to the socket */
    if (bind(gpsd->srv_fd, (struct sockaddr*)&gpsd->srv_addr, sizeof(gpsd->srv_addr))) {
        GPSD_ERR("Failed to bind");
        return GPSD_RET_FAILED;
    }


    /* Mark the socket as a passive socket to accept incoming connection requests */
    if (listen(gpsd->srv_fd, GPSD_UDS_MAX_CLIENT)) {
        GPSD_ERR("Failed to listen");
        return GPSD_RET_FAILED;
    }

    GPSD_INFO("Unix-domain socket created w/ %s and max client of %d",
                GPSD_UDS_FILE, GPSD_UDS_MAX_CLIENT);

#if 0 // decide whether to use SIGIO or not later
    int fd_flags = 0; // server file descriptor's flag

    /* Set srv_fd owner to the current PID */
    fcntl(gpsd->srv_fd, F_SETOWN, getpid());

    /* Set the srv_fd as O_ASYNC */
    fd_flags = fcntl(gpsd->srv_fd, F_GETFL);
    fd_flags |= O_ASYNC;
    fcntl(gpsd->srv_fd, F_SETFD, fd_flags);
#endif

    /* Accept incoming client connection
     * Note that this function will block until a request comes in */
    if ((gpsd->clnt_fd[0] = accept(gpsd->srv_fd, (struct sockaddr*)&gpsd->clnt_addr[0],
                                &gpsd->clnt_addr_len[0]))
        < 0) {
        GPSD_ERR("Failed to accept client connection!");
        return GPSD_RET_FAILED;
    }

    GPSD_INFO("Connection w/ cllient %d established", gpsd->clnt_fd[0]);

    return GPSD_RET_SUCESS;
}

static int init_dev(char *dev_name)
{
    GpsdData *gpsd = &g_gpsd_data;

    /* Iniialize GPSD data */
    memset(&g_gpsd_data, 0, sizeof(g_gpsd_data));

    /* Open the device */
    gpsd->fd = open(dev_name, O_RDWR);
    if (gpsd->fd == -1) {
        GPSD_ERR("[%s] Cannot open %s\n", __FILE__, dev_name);
        return GPSD_RET_FAILED;
    }

    GPSD_INFO("Successfully opened %s", dev_name);

    /* Poll UBX-NAV-TIMETGPS once per second */
    if (ubx_set_msg_rate(gpsd->fd, UBX_CLASS_NAV, UBX_ID_NAV_TIMEGPS, UBX_CFG_MSG_ON)) {
        GPSD_ERR("Failed to set UBX-NAV-TIMETGPS message rate\n");
    }

    return GPSD_RET_SUCESS;
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

    /* Initialize GPS device */
    if (init_dev(argv[1])) {
        exit(0);
    }

    /* Set up Unix-domain socket connection */
    if (init_uds()) {
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


    if (close(gpsd->fd) < 0) {
        printf("[%s] Failed to close %s w/ file descriptor = %d\n", __FILE__, argv[1], gpsd->fd);
    }

    return 0;
}

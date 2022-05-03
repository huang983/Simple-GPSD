#include "ubx.h"

static inline int init_ubx_header(UbxMsg *ubx_msg)
{
    ubx_msg->header[0] = UBX_SYNC1;
    ubx_msg->header[1] = UBX_SYNC2;

    return GPSD_RET_SUCCESS;
}

static inline int calc_ubx_msg_size(UbxMsg *ubx_msg)
{
    return sizeof(*ubx_msg) - sizeof(ubx_msg->size) - sizeof(ubx_msg->payload_ck_sum)
            + COMBINE_TWO_EIGHT_BIT(ubx_msg->payload_length[1],ubx_msg->payload_length[0]);
}

static void check_sum(UbxMsg *ubx_msg)
{
    uint8_t ck_a_offset, ck_b_offset;
    uint8_t ck_a = 0;
    uint8_t ck_b = 0;
    uint16_t payload_length = 0;
    uint8_t *ck_sum_ptr = &ubx_msg->class; // skip sync chars
    int ck_sum_cnt, i = 0;

    payload_length = COMBINE_TWO_EIGHT_BIT(ubx_msg->payload_length[1],
                                            ubx_msg->payload_length[0]);
    ck_sum_cnt = 4 + payload_length; // Class, ID, and length take up 4 bytes
    ck_a_offset = ck_sum_cnt;
    ck_b_offset = ck_sum_cnt + 1;
    for (i = 0; i < ck_sum_cnt; i++) {
        ck_a += (uint8_t)*(ck_sum_ptr + i);
        ck_b += ck_a;
    }

    *(ck_sum_ptr + ck_a_offset) = ck_a;
    *(ck_sum_ptr + ck_b_offset) = ck_b;
    ubx_msg->size = calc_ubx_msg_size(ubx_msg);

    return;
}

/**
 * @brief Wait for ACK message from the U-blox
 * 
 * @param id 
 * @return int 0 on success, -1 failed
 */
static int wait_for_ack(int fd, uint8_t class, uint8_t id)
{
#define WAIT_ACK_BUF_SZ 1024
#define ACK_MSG_LEN 10
    uint8_t buf[WAIT_ACK_BUF_SZ];
    uint8_t ack_msg[ACK_MSG_LEN] = { UBX_SYNC1, UBX_SYNC2, UBX_CLASS_ACK, UBX_ACK_ACK,
                                     0x02, 0x00, 0x00, 0x00, 0x00, 0x00 }; // skip checksum
    int size = 0;
    int cnt = 0; // return -1 if read more than 5 times
    int i = 0;
    int j = 0;

    /* Assign class, ID, and checksum */
    ack_msg[6] = class;
    ack_msg[7] = id;

    for (i = 2; i < (ACK_MSG_LEN - 2); i++) {
        ack_msg[8] += ack_msg[i];
        ack_msg[9] += ack_msg[8];
    }

    while (1) {
        if (cnt == 6) {
            UBX_ERR("Failed to get ACK message!");
            return -1;
        }
        
        usleep(500000);

        size = read(fd, buf, WAIT_ACK_BUF_SZ);
        if (size <= 0) {
            UBX_ERR("Read failed! (cnt: %d)", cnt);
            goto upd_cnt;
        }

        for (i = 0; i < size; i++) {
            if ((size - i) < UBX_ACK_MSG_LEN) {
                /* Didn't receive complete message */
                break;
            }

            if (buf[i] == ack_msg[0]) {
                for (j = i; j < (i + UBX_ACK_MSG_LEN); j++) {
                    if (buf[j] != ack_msg[j - i]) {
                        if (j == UBX_IDX_ID && buf[j] == UBX_ACK_NAK) {
                            UBX_ERR("NAK was received!");
                        }
                        break;
                    }
                }

                if (j == (i + UBX_ACK_MSG_LEN)) {
                    /* return 0 on scucess if all matched */
                    UBX_INFO("Successfuly get ACK message for class 0x%X and ID 0x%X!", class, id);
                    return 0;
                }
            }
        }
upd_cnt:
        cnt++;
    }

    return -1;
}

/** 
 * @brief set the message rate of the given message class and ID
 * @param fd file descriptor of the device
 * @param class message's class
 * @param id message's ID
 * @param rate desired message rate (0: disable the message)
 * @return 0 on sucess
 */
int ubx_set_msg_rate(int fd, uint8_t class, uint8_t id, uint8_t rate)
{
    UbxMsg ubx_msg;
    int size = 0;

    memset(&ubx_msg, 0, sizeof(ubx_msg));

    /* Set sync chars */
    init_ubx_header(&ubx_msg);

    /* Set class, ID, and payload length */
    ubx_msg.class = UBX_CLASS_CFG;
    ubx_msg.id = UBX_ID_CFG_MSG;
    ubx_msg.payload_length[0] = 0x03; // little-endian order
    ubx_msg.payload_length[1] = 0x00;

    /* Set payload */
    ubx_msg.payload_ck_sum[0] = class;
    ubx_msg.payload_ck_sum[1] = id;
    ubx_msg.payload_ck_sum[2] = rate;

    check_sum(&ubx_msg);
    // printf("ck_a: 0x%X, ck_b: 0x%X\n", ubx_msg.payload_ck_sum[ubx_msg.size - 6 - 2],
    //             ubx_msg.payload_ck_sum[ubx_msg.size - 6 - 1]);

    size = write(fd, &ubx_msg, ubx_msg.size);
    if (size < ubx_msg.size) {
        printf("[%s] bytes written: %d (%s)\n", __FILE__, size, strerror(errno));
    }

    return wait_for_ack(fd, UBX_CLASS_CFG, UBX_ID_CFG_MSG);
}
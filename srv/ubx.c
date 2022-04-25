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
    // printf("ck_sum_cnt: %d, ck_a: 0x%X, ck_b: 0x%X\n", ck_sum_cnt, ck_a, ck_b);
    for (i = 0; i < ck_sum_cnt; i++) {
        // printf("[%d] ubx_msg val: 0x%X\n", i, *(ck_sum_ptr + i));
        ck_a += (uint8_t)*(ck_sum_ptr + i);
        ck_b += ck_a;
    }

    *(ck_sum_ptr + ck_a_offset) = ck_a;
    *(ck_sum_ptr + ck_b_offset) = ck_b;
    ubx_msg->size = calc_ubx_msg_size(ubx_msg);
    // printf("[%s] ck_a: 0x%X, ck_b: 0x%X, size: %d\n", __func__, ck_a, ck_b, ubx_msg->size);

    return;
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
    printf("ck_a: 0x%X, ck_b: 0x%X\n", ubx_msg.payload_ck_sum[ubx_msg.size - 6 - 2],
                ubx_msg.payload_ck_sum[ubx_msg.size - 6 - 1]);

    size = write(fd, &ubx_msg, ubx_msg.size);
    if (size < ubx_msg.size) {
        printf("[%s] bytes written: %d (%s)\n", __FILE__, size, strerror(errno));
    }

    return GPSD_RET_SUCCESS;
}
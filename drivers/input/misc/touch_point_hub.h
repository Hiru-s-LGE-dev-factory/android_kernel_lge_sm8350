#ifndef _TOUCH_POINT_HUB_H
#define _TOUCH_POINT_HUB_H

typedef struct {
    /* it indicates which device to read/write(hub or sensor) */
    uint8_t target;
    /* high 8-bits of read/write start address */
    uint8_t addr_h;
	/* low 8-bits of read/write start address */
	uint8_t addr_l;
    /* read/write length from the addr */
    uint16_t len;
    /* read/write data buffer */
    uint8_t buf[];
} __attribute__((packed)) ussys_rw_msg;

typedef enum {
	USSYS_HUB_ENABLE_INPUT_KEY_EVENT = 1,
	USSYS_HUB_WAKEUP,
	USSYS_HUB_ENABLE_KEEP_AWAKE,
	USSYS_HUB_SET_CALI_FLAG,
	USSYS_HUB_GET_KEY_CNT,
	USSYS_HUB_GET_KEY_NAME,
	USSYS_HUB_GET_INIT_DONE,
	USSYS_HUB_GET_VERSION,
	USSYS_HUB_GET_LINUX_DRV_VERSION,
	USSYS_HUB_RECOVER_DEV,
	USSYS_SNS_TOGGLE_INT0,
	USSYS_SNS_ENABLE_HPM,
	USSYS_SNS_GET_OTP_MEM,
	USSYS_HUB_GET_ALGO_MODE_IN_DTS_SETTING,
	USSYS_HUB_GET_KNOWN_NUM_OF_KEYS,
} ussys_param_type;

#define USSYSIO				0xA5

/* IOCTL Cmds Definition */
#define USSYSIO_READ_DEV           	_IOWR(USSYSIO, 0x01, uint8_t *)
#define USSYSIO_WRITE_DEV       	_IOW(USSYSIO, 0x02, uint8_t *)
#define USSYSIO_SET_PARAM			_IOW(USSYSIO, 0x03, uint8_t *)
#define USSYSIO_GET_PARAM			_IOWR(USSYSIO, 0x04, uint8_t *)

#endif/*_TOUCH_POINT_HUB_H*/

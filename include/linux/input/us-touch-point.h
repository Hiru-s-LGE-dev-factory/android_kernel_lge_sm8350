#ifndef _US_TOUCH_POINT_H
#define _US_TOUCH_POINT_H

typedef struct {
    /* used for digital mux channel select */
    uint8_t channel;
    /* read/write start address */
    uint8_t addr;
    /* read/write lenght from the addr */
    uint16_t len;
    /* read/write data buffer */
    uint8_t buf[];
} __attribute__((packed)) ussys_rw_msg;

#define USSYSIO				0xA5

/* IOCTLs for USSYS DAEMON and Library */
#define USSYSIO_TOGGLE_INT_PIN      _IO(USSYSIO, 0x01)
#define USSYSIO_READ_DEV           	_IOWR(USSYSIO, 0x02, uint8_t *)
#define USSYSIO_WRITE_DEV       	_IOW(USSYSIO, 0x03, uint8_t *)
#define USSYSIO_ENABLE_ALWAYS_ON    _IOW(USSYSIO, 0x04, uint8_t)
#define USSYSIO_SET_CALI_FLAG		_IOW(USSYSIO, 0x05, uint8_t *)
#define USSYSIO_GET_KEY_NAME		_IOR(USSYSIO, 0x06, char *)
#define USSYSIO_DUMP_OTP_MEM		_IOR(USSYSIO, 0x07, uint8_t *)
#define USSYSIO_GET_VERSION			_IOR(USSYSIO, 0x08, char *)
#define USSYSIO_RECOVER_DEV			_IO(USSYSIO, 0x09)

#endif

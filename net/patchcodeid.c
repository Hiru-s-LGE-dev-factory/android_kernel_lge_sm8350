/* 2018-01-25 ickjun.kim@lge.com LGP_DATA_ENV_PATCHCODEID [START] */
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>

#include <net/patchcodeid.h>

void patch_code_id(char* buf) {
    // Do nothing. This API is only for checking patch code ID.
    if (buf != NULL && buf[0] == 'T') {
        buf[0] = 'T'; //without modification, compile optimization remove this code.
    } // dummy code.
}
/* 2018-01-25 ickjun.kim@lge.com LGP_DATA_ENV_PATCHCODEID [END] */

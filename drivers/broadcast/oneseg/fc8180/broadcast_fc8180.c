#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/spi/spi.h>
#include <linux/spi/spidev.h>
#include <linux/interrupt.h>

#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/pm_wakeup.h>         /* wakelock, unlock */
#include <linux/version.h>          /* check linux version */

#include <linux/kthread.h>
#include <linux/irq.h>
#include <linux/platform_device.h>

#include <linux/err.h>
#include <linux/of_gpio.h>

#include <linux/clk.h>
#include <linux/pinctrl/consumer.h>

#include <linux/power_supply.h>
#include <linux/regulator/consumer.h>
#include <linux/async.h>
#include <linux/types.h>
#include <linux/string.h>

typedef long intptr_t;

#include "../broadcast_dmb_typedef.h"
#include "../broadcast_dmb_drv_ifdef.h"
#include "broadcast_fc8180.h"
#include "drv/inc/fci_types.h"
#include "drv/inc/fci_oal.h"
#include "drv/inc/bbm.h"
#include "drv/inc/fc8180_drv_api.h"
#include "drv/inc/fc8180_isr.h"
#include "drv/inc/fci_ringbuffer.h"

#define LGE_FC8180_DRV_VER "2.00.00"

struct ISDBT_INIT_INFO_T *hInit;

#define FC8180_NAME        "broadcast1"

#define RING_BUFFER_SIZE    (188 * 320)

static int broadcast_spi_remove(struct spi_device *spi);
static int broadcast_spi_probe(struct spi_device *spi);
static int broadcast_check_chip_id(void);

static struct task_struct *isdbt_kthread;
struct fci_ringbuffer        RingBuffer;
static wait_queue_head_t isdbt_isr_wait;
u8 irq_error_cnt;
static u8 isdbt_isr_sig=0;
static u8 isdbt_isr_start=0;
u32 totalTS=0;
u32 totalErrTS=0;

enum ISDBT_MODE{
    ISDBT_POWERON       = 0,
    ISDBT_POWEROFF        = 1,
    ISDBT_DATAREAD        = 2
};
enum ISDBT_MODE driver_mode = ISDBT_POWEROFF;

u8 static_ringbuffer[RING_BUFFER_SIZE];
static DEFINE_MUTEX(ringbuffer_lock);

struct isdbt_platform_data
{
    int                    powstate;
    struct wakeup_source   *wakelock;
    struct spi_device*     spidev;
    struct i2c_client*     pclient;
    struct clk             *clk;
    struct platform_device *pdev;

    struct pinctrl         *pinctrl;
    struct pinctrl_state   *pins_active;
    struct pinctrl_state   *pins_sleep;

    int                    en_gpio;
    int                    reset_gpio;

    int                    antsw_gpio;
    int                    dmb_ant_sw_active_value;

    int                    lna_gc_gpio;
    int                    lna_en_gpio;

    int                    isdbt_en_lna_en_use_same_gpio;
    int                    ldo_en_gpio;

    int                    dtv_use_xtal;
    int                    dtv_xtal_freq;
    int                    dtv_interface_freq;

    int                    use_isdbt_ldo;
    struct regulator       *isdbt_ldo_reg;
    int                    use_lna_ldo;
    struct regulator       *lna_ldo_reg;
    int                    use_ant_sw_ldo;
    struct regulator       *ant_sw_ldo_reg;
};

static struct of_device_id isdbt_table[] = {
{
    .compatible = "fci,fc8180-spi",}, //Compatible node must match dts
    { },
};

static struct spi_driver broadcast_spi_driver = {
    .probe = broadcast_spi_probe,
    .remove    = __broadcast_dev_exit_p(broadcast_spi_remove),
    .driver = {
        .name = "fc8180-spi",
        .of_match_table = isdbt_table,
        .bus    = &spi_bus_type,
        .owner = THIS_MODULE,
    },
};
static struct isdbt_platform_data  fc8180_pdata;

struct spi_device* FCI_GET_SPI_DRIVER(void)
{
    return fc8180_pdata.spidev;
}

static Device_drv device_fc8180 = {
    &broadcast_fc8180_drv_if_power_on,
    &broadcast_fc8180_drv_if_power_off,
    &broadcast_fc8180_drv_if_open,
    &broadcast_fc8180_drv_if_close,
    &broadcast_fc8180_drv_if_set_channel,
    &broadcast_fc8180_drv_if_resync,
    &broadcast_fc8180_drv_if_detect_sync,
    &broadcast_fc8180_drv_if_get_sig_info,
    &broadcast_fc8180_drv_if_get_ch_info,
    &broadcast_fc8180_drv_if_get_dmb_data,
    &broadcast_fc8180_drv_if_reset_ch,
    &broadcast_fc8180_drv_if_user_stop,
    &broadcast_fc8180_drv_if_select_antenna,
    &broadcast_fc8180_drv_if_read_control,
    &broadcast_fc8180_drv_if_get_mode,
};

enum dt_entry_type {
    DT_U32,
    DT_GPIO,
    DT_BOOL,
};

enum dt_entry_status {
    DT_REQ,  /* Required:  fail if missing */
    DT_SGST, /* Suggested: warn if missing */
    DT_OPT,  /* Optional:  don't warn if missing */
};

struct dt_to_pdata_map {
    const char             *dt_name;
    void                   *ptr_data;
    enum dt_entry_status   status;
    enum dt_entry_type     type;
    int                    default_val;
};


static inline void* isdbt_get_platform_data(void)
{
    return &fc8180_pdata;
}

static int isdbt_dt_to_pdata_populate(struct platform_device *pdev,
                    struct dt_to_pdata_map  *itr)
{
    int  ret, err = 0;
    struct device_node *node = pdev->dev.of_node;

    for (; itr->dt_name; ++itr) {
        switch (itr->type) {
        case DT_GPIO:
            ret = of_get_named_gpio(node, itr->dt_name, 0);
            if (ret >= 0) {
                *((int *) itr->ptr_data) = ret;
                ret = 0;
            }
            break;
        case DT_U32:
            ret = of_property_read_u32(node, itr->dt_name,
                    (u32 *) itr->ptr_data);
            break;
        case DT_BOOL:
            *((bool *) itr->ptr_data) =
                of_property_read_bool(node, itr->dt_name);
            ret = 0;
            break;
        default:
            printk("[dtv] %d is an unknown DT entry type\n", itr->type);
            ret = -1;
        }

        /* If you like to see debug logs for node, de-block the blow printk */
        /*printk("[dtv] DeviceTree entry ret:%d name:%s val:%d\n",
            ret, itr->dt_name, *((int *)itr->ptr_data)); */

        if (ret) {
            *((int *)itr->ptr_data) = itr->default_val;
            if (itr->status < DT_OPT) {
                printk("[dtv] Missing '%s' DT entry\n", itr->dt_name);
                /* cont on err to dump all missing entries */
                if (itr->status == DT_REQ && !err)
                    err = ret;
            }
        }
    }

    return err;
}

static int isdbt_dt_to_platform_data(struct isdbt_platform_data* tpdata)
{
    int rc = OK;

    printk("[dtv] %s\n", __func__);

    if(tpdata == NULL) {
        printk("[dtv] tpdata is null error\n");
        return ERROR;
    }
    // of_node is valid, enter to get node information of dts
    if(tpdata->pdev && tpdata->pdev->dev.of_node) {
        struct dt_to_pdata_map dts_map[] = {
            {"en-gpio",
                &tpdata->en_gpio, DT_OPT, DT_GPIO, -1},
            {"reset-gpio",
                &tpdata->reset_gpio, DT_OPT, DT_GPIO, -1},
            {"ant-sw-gpio",
                &tpdata->antsw_gpio, DT_OPT, DT_GPIO, -1},
            {"lna-gc-gpio",
                &tpdata->lna_gc_gpio, DT_OPT, DT_GPIO, -1},
            {"lna-en-gpio",
                &tpdata->lna_en_gpio, DT_OPT, DT_GPIO, -1},
            {"ldo-en-gpio",
                &tpdata->ldo_en_gpio, DT_OPT, DT_GPIO, -1},
            {"ant-sw-active-value",
                &tpdata->dmb_ant_sw_active_value, DT_OPT, DT_U32, 0},
            {"isdbt-en-lna-en-same-gpio",
                &tpdata->isdbt_en_lna_en_use_same_gpio, DT_OPT, DT_U32, 0},
            {"use-xtal",
                &tpdata->dtv_use_xtal, DT_OPT, DT_U32, 0},
            {"xtal-freq",
                &tpdata->dtv_xtal_freq, DT_OPT, DT_U32, 0},
            {"ctrl-isdbt-ldo",
                &tpdata->use_isdbt_ldo, DT_OPT, DT_U32, 0},
            {"ctrl-lna-ldo",
                &tpdata->use_lna_ldo, DT_OPT, DT_U32, 0},
            {"ctrl-ant-sw-ldo",
                &tpdata->use_ant_sw_ldo, DT_OPT, DT_U32, 0},
            {NULL,  NULL,  0,  0,   0},
        };

        if (isdbt_dt_to_pdata_populate(tpdata->pdev, dts_map)) {
            printk("[dtv] dt_to_pdata error\n");
            return ERROR;
        }
    } else {
        printk("[dtv] pdev or dev.of_node is not valid\n");
        return ERROR;
    }

    //printk("[dtv] en-gpio(%d) reset-gpio(%d) and-sw-gpio(%d) lna-gc-gpio(%d) lna-en-gpio(%d) \n"
    //    , tpdata->en_gpio, tpdata->reset_gpio, tpdata->antsw_gpio, tpdata->lna_gc_gpio, tpdata->lna_en_gpio);
    //printk("[dtv] ldo-en-gpio(%d) ant-sw-active-value(%d) isdbt-en-lna-en-same-gpio(%d)\n"
    //    , tpdata->ldo_en_gpio, tpdata->dmb_ant_sw_active_value, tpdata->isdbt_en_lna_en_use_same_gpio);
    //printk("[dtv] use-xtal(%d) xtal-freq(%d) ctrl-isdbt-ldo(%d) ctrl-lna-ldo(%d) ctrl-ant-sw-ldo(%d)\n"
    //    , tpdata->dtv_use_xtal, tpdata->dtv_xtal_freq, tpdata->use_isdbt_ldo tpdata->use_lna_ldo, tpdata->use_ant_sw_ldo);

    return rc;
}

static int isdbt_pinctrl_init(struct isdbt_platform_data* tpdata)
{
    tpdata->pinctrl = devm_pinctrl_get(&(tpdata->pdev->dev));
    if(IS_ERR_OR_NULL(tpdata->pinctrl)) {
        pr_err("[dtv] Getting pinctrl handle failed\n");
        return -EINVAL;
    }

    tpdata->pins_active
    = pinctrl_lookup_state(tpdata->pinctrl, "isdbt_pin_active");

    if(IS_ERR_OR_NULL(tpdata->pins_active)) {
         pr_err("[dtv] Failed to get the active state pinctrl handle\n");
         return -EINVAL;
    }

    tpdata->pins_sleep
    = pinctrl_lookup_state(tpdata->pinctrl, "isdbt_pin_sleep");

    if(IS_ERR_OR_NULL(tpdata->pins_sleep)) {
         pr_err("[dtv] Failed to get the sleep state pinctrl handle\n");
         return -EINVAL;
    }
    return 0;
}

static int isdbt_request_gpios(struct isdbt_platform_data* tpdata)
{
    if(pinctrl_select_state(tpdata->pinctrl, tpdata->pins_active)) {
        pr_err("[dtv] error on pinctrl_select_state to active\n");
        return -EINVAL;
    }
    return 0;
}

static int isdbt_free_gpios(struct isdbt_platform_data* tpdata)
{
    if(pinctrl_select_state(tpdata->pinctrl, tpdata->pins_sleep)) {
        pr_err("[dtv] error on pinctrl_select_state to sleep\n");
        return -EINVAL;
    }
    return 0;
}

/**
 * isdbt_initialize_powsource: tuner, lna power source init
 */
static int isdbt_power_init(struct isdbt_platform_data* tpdata)
{
    int rc = OK;

    if(tpdata == NULL) {
        printk("[dtv] isdbt_platform_data is null\n");
        return ERROR;
    }

    printk("[dtv] ctrl-dmb-ldo : %d\n", tpdata->use_isdbt_ldo);
    if(tpdata->use_isdbt_ldo == 1) {
        tpdata->isdbt_ldo_reg = devm_regulator_get(&tpdata->spidev->dev, "dmb-ldo");
        if(IS_ERR(tpdata->isdbt_ldo_reg)) {
            rc = PTR_ERR(tpdata->isdbt_ldo_reg);
            dev_err(&tpdata->spidev->dev, "regulator isdbt_ldo_reg failed : %d\n", rc);
            return ERROR;
        }
    }

    printk("[dtv] ctrl-lna-ldo : %d\n", tpdata->use_lna_ldo);
    if(tpdata->use_lna_ldo == 1) {
        tpdata->lna_ldo_reg= devm_regulator_get(&tpdata->spidev->dev, "lna-ldo");
        if(IS_ERR(tpdata->lna_ldo_reg)) {
            rc = PTR_ERR(tpdata->lna_ldo_reg);
            dev_err(&tpdata->spidev->dev, "regulator lna_ldo_reg failed : %d\n", rc);
            return ERROR;
        }
    }

    printk("[dtv] ctrl-ant-sw-ldo : %d\n", tpdata->use_ant_sw_ldo);
    if(tpdata->use_ant_sw_ldo == 1) {
        tpdata->ant_sw_ldo_reg = devm_regulator_get(&tpdata->spidev->dev, "ant-sw-ldo");
        if(IS_ERR(tpdata->ant_sw_ldo_reg)) {
            rc = PTR_ERR(tpdata->ant_sw_ldo_reg);
            dev_err(&tpdata->spidev->dev, "regulator ant_sw_ldo_reg failed : %d\n", rc);
            return ERROR;
        }
    }

    return OK;
}

static int isdbt_clk_init(struct isdbt_platform_data* tpdata)
{
    int rc = OK;

    if(tpdata == NULL) {
        printk("[dtv] isdbt_platform_data is null\n");
        return ERROR;
    }

    if(tpdata->dtv_use_xtal == FALSE) {
        tpdata->clk = clk_get(&tpdata->spidev->dev, "isdbt_xo");
        if (IS_ERR(tpdata->clk)) {
            rc = PTR_ERR(tpdata->clk);
            dev_err(&tpdata->spidev->dev, "could not get clock : %d\n", rc);
            return ERROR;
        }

        /* We enable/disable the clock only to assure it works */
        rc = clk_prepare_enable(tpdata->clk);
        if (rc) {
            dev_err(&tpdata->spidev->dev, "could not enable clock : %d\n", rc);
            return ERROR;
        }
        clk_disable_unprepare(tpdata->clk);
    }
    return rc;
}

static int isdbt_ldo_power_on(struct isdbt_platform_data* tpdata)
{
    int rc = OK;

    if(tpdata == NULL) {
        printk("[dtv] isdbt_platform_data is null\n");
        return ERROR;
    }

    if(tpdata->use_isdbt_ldo == TRUE) {
        rc = regulator_enable(tpdata->isdbt_ldo_reg);
        if(rc) {
            dev_err(&tpdata->spidev->dev, "unable to enable isdbt_ldo_reg : %d\n", rc);
            return ERROR;
        } else {
            printk("[dtv] isdbt_ldo_reg enable\n");
        }
    }

    if(tpdata->ldo_en_gpio > 0) {
        gpio_set_value(tpdata->ldo_en_gpio, 1);
        printk("[dtv] set ldo_en_gpio to 1\n");
        mdelay(10);
    }
    return rc;
}

static void isdbt_ldo_power_off(struct isdbt_platform_data* tpdata)
{
    if(tpdata == NULL) {
        printk("[dtv] isdbt_platform_data is null\n");
    }

    if(tpdata->ldo_en_gpio > 0) {
        gpio_set_value(tpdata->ldo_en_gpio, 0);
        printk("[dtv] set isdbt_ldo_en_gpio_value to 0\n");
        mdelay(10);
    }

    if(tpdata->use_isdbt_ldo == TRUE) {
        regulator_disable(tpdata->isdbt_ldo_reg);
        printk("[dtv] isdbt_ldo_reg disable\n");
    }
}

static int isdbt_lna_power_on(struct isdbt_platform_data* tpdata)
{
    int rc = OK;

    if(tpdata == NULL) {
        printk("[dtv] isdbt_platform_data is null\n");
        return ERROR;
    }
    
    if(tpdata->use_lna_ldo == TRUE) {
        rc = regulator_enable(tpdata->lna_ldo_reg);
        if(rc) {
            dev_err(&tpdata->spidev->dev, "unable to enable lna_ldo_reg\n");
            return rc;
        } else {
            printk("[dtv] lna_ldo_reg enable\n");
        }
    }

    if(tpdata->lna_gc_gpio > 0) {
        gpio_set_value(tpdata->lna_gc_gpio, 0);
    }

    if(tpdata->lna_en_gpio > 0) {
        gpio_set_value(tpdata->lna_en_gpio, 1);
    }

    return rc;
}

static void isdbt_lna_power_off(struct isdbt_platform_data* tpdata)
{
    if(tpdata == NULL) {
        printk("[dtv] isdbt_platform_data is null\n");
    }

    if(tpdata->lna_gc_gpio > 0) {
        if(tpdata->lna_en_gpio > 0) {
            gpio_set_value(tpdata->lna_gc_gpio, 0); // OFF mode
        } else {
            gpio_set_value(tpdata->lna_gc_gpio, 1); // Bypass mode
        }
    }

    if(tpdata->lna_en_gpio > 0) {
        gpio_set_value(tpdata->lna_en_gpio, 0);
    }

    if(tpdata->use_lna_ldo == TRUE) {
        regulator_disable(tpdata->lna_ldo_reg);
        printk("[dtv] lna_ldo_reg disable\n");
    }
}

static int isdbt_ant_sw_power_on(struct isdbt_platform_data* tpdata)
{
    int rc = OK;

    if(tpdata == NULL) {
        printk("[dtv] isdbt_platform_data is null\n");
        return ERROR;
    }
    
    if (tpdata->use_ant_sw_ldo == TRUE) {
        rc = regulator_enable(tpdata->ant_sw_ldo_reg);
        if(rc) {
            dev_err(&tpdata->spidev->dev, "unable to enable ant_sw_ldo_reg : %d\n", rc);
            return ERROR;
        } else {
            printk("[dtv] ant_sw_ldo_reg enable\n");
        }
    }

    if(tpdata->antsw_gpio > 0) {
        gpio_set_value(tpdata->antsw_gpio, tpdata->dmb_ant_sw_active_value);
        printk("[dtv] ant_sw_gpio_value : %d\n", tpdata->dmb_ant_sw_active_value);
    }

    return rc;
}

static void isdbt_ant_sw_power_off(struct isdbt_platform_data* tpdata)
{
    if(tpdata == NULL) {
        printk("[dtv] isdbt_platform_data is null\n");
    }

    if(tpdata->antsw_gpio > 0) {
        gpio_set_value(tpdata->antsw_gpio, !(tpdata->dmb_ant_sw_active_value));
        printk("[dtv] ant_sw_gpio_value : %d\n", !(tpdata->dmb_ant_sw_active_value));
    }

    if(tpdata->use_ant_sw_ldo == TRUE) {
        regulator_disable(tpdata->ant_sw_ldo_reg);
        printk("[dtv] ant_sw_ldo_reg disable\n");
    }
}

static inline void isdbt_print_driver_version(void)
{
#if defined(CONFIG_ARCH_MSM) || defined(CONFIG_ARCH_QCOM)
    printk("[dtv] LGE_FC8180_DRV_VER (QCOM)  : %s\n", LGE_FC8180_DRV_VER);
#else
    printk("[dtv] LGE_FC8180_DRV_VER (MTK or other)  : %s\n", LGE_FC8180_DRV_VER);
#endif
}

int isdbt_fc8180_power_on(void)
{
    int i = 0;
    int rc = OK;
    struct isdbt_platform_data* tpdata = isdbt_get_platform_data();

    printk("[dtv] %s\n", __func__);

    if(tpdata == NULL) {
        printk("[dtv] isdbt_get_platform_data error\n");
        return ERROR;
    }

    rc = isdbt_request_gpios(tpdata);
    printk("[dtv] isdbt_request_gpios : %d\n", rc);
    mdelay(5);

    if (tpdata->powstate == TRUE) {
        printk("[dtv] the power already turn on \n");
        return rc;
    }

    __pm_stay_awake(tpdata->wakelock);

    if(tpdata->reset_gpio > 0) {
        gpio_set_value(tpdata->reset_gpio, 0);
    }

    rc = isdbt_ldo_power_on(tpdata);
    printk("[dtv] isdbt_ldo_power_on : %d\n", rc);

    while (driver_mode == ISDBT_DATAREAD) {
        ms_wait(100);
        printk("[dtv] ISDBT_DATARREAD mode i(%d)\n", i);
        if(i++ > 5)
            break;
    }

    if(tpdata->en_gpio > 0) {
        gpio_set_value(tpdata->en_gpio, 0);
        mdelay(5);
        gpio_set_value(tpdata->en_gpio, 1);
        mdelay(5);
    }

    //[TDMBDEV-2766] (issue) DMB_EN control that FC8080 LDO_EN & 1.8V regulator
    //So add 100ms delay for power sequence before clk enable
    if(tpdata->isdbt_en_lna_en_use_same_gpio == TRUE) {
        mdelay(100);
    }

    if(tpdata->dtv_use_xtal == FALSE) {
        if(tpdata->clk != NULL) {
            rc = clk_prepare_enable(tpdata->clk);
            if(rc) {
                if(tpdata->en_gpio > 0) {
                    gpio_set_value(tpdata->en_gpio, 0);
                }
                dev_err(&tpdata->spidev->dev, "[dtv] could not enable clock : %d\n", rc);
                return ERROR;
            }
        }
    }
    // For T1 time (>= 1msec)
    msleep(30);

    rc = isdbt_lna_power_on(tpdata);
    printk("[dtv] isdbt_lna_power_on : %d\n", rc);
    rc = isdbt_ant_sw_power_on(tpdata);
    printk("[dtv] isdbt_ant_sw_power_on : %d\n", rc);

    // For T4 time (>=20 msec)
    msleep(30);

    driver_mode = ISDBT_POWERON;

    tpdata->powstate = TRUE;
    printk("[dtv] %s completed rc : %d\n", __func__, rc);

    return rc;
}

int isdbt_fc8180_is_power_on()
{
    return (int)fc8180_pdata.powstate;
}

unsigned int isdbt_fc8180_get_xtal_freq(void)
{
    return (unsigned int)fc8180_pdata.dtv_xtal_freq;
}

int isdbt_fc8180_stop(void)
{
    if (driver_mode == ISDBT_DATAREAD) {
            driver_mode = ISDBT_POWEROFF;
            ms_wait(200);
    }
    driver_mode = ISDBT_POWEROFF;

    return OK;
}

int isdbt_fc8180_power_off(void)
{
    int rc = OK;
    struct isdbt_platform_data* tpdata = isdbt_get_platform_data();
    driver_mode = ISDBT_POWEROFF;

    printk("[dtv] %s\n", __func__);

    if(tpdata == NULL) {
        printk("[dtv] isdbt_get_platform_data error\n");
        return ERROR;
    }

    if (tpdata->powstate == TRUE ) {
        isdbt_lna_power_off(tpdata);
        isdbt_ant_sw_power_off(tpdata);

        if(tpdata->dtv_use_xtal == FALSE) {
            if(tpdata->clk != NULL) {
                clk_disable_unprepare(tpdata->clk);
            }
        }

        tpdata->powstate = FALSE;
        if(tpdata->en_gpio > 0) {
            gpio_set_value(tpdata->en_gpio, 0);
        }
        mdelay(1);
        if(tpdata->reset_gpio > 0) {
            gpio_set_value(tpdata->reset_gpio, 0);
        }
        mdelay(5);

        isdbt_ldo_power_off(tpdata);
    } else {
        printk("[dtv] the power already turn off \n");
    }

    __pm_relax(tpdata->wakelock);

    rc = isdbt_free_gpios(tpdata);
    printk("[dtv] isdbt_free_gpios : %d\n", rc);
    printk("[dtv] %s completed rc : %d\n", __func__, rc);

    return rc;
}

unsigned int isdbt_fc8180_get_ts(void *buf, unsigned int size)
{
    s32 avail;
    ssize_t len, total_len = 0;

    if (fci_ringbuffer_empty(&RingBuffer))
        return 0;

    mutex_lock(&ringbuffer_lock);

    avail = fci_ringbuffer_avail(&RingBuffer);

    if (size >= avail)
        len = avail;
    else
        len = size - (size % 188);

    total_len = fci_ringbuffer_read_user(&RingBuffer, buf, len);

    mutex_unlock(&ringbuffer_lock);

    return total_len;
}

static irqreturn_t isdbt_irq(int irq, void *dev_id)
{
    if ((driver_mode == ISDBT_POWEROFF) || (isdbt_isr_start)) {
        printk("[dtv] isdbt_irq : abnormal Interrupt, driver_mode : %d state.cnt : %d\n", driver_mode, irq_error_cnt);
        irq_error_cnt++;
        isdbt_isr_start = 0;
    } else {
        isdbt_isr_sig++;
        wake_up(&isdbt_isr_wait);
    }

    return IRQ_HANDLED;
}

int data_callback(u32 hDevice, u8 *data, int len)
{
    int i;
    unsigned long ts_error_count = 0;
    totalTS += (len / 188);

    for (i = 0; i < len; i += 188) {
        if ((data[i+1] & 0x80) || data[i] != 0x47)
            ts_error_count++;
    }

    if (ts_error_count > 0) {
        totalErrTS += ts_error_count;
        printk("[dtv] data_callback totalErrTS : %d, len : %d\n", totalErrTS, len);
    }

    mutex_lock(&ringbuffer_lock);

    if (fci_ringbuffer_free(&RingBuffer) < len)
        FCI_RINGBUFFER_SKIP(&RingBuffer, len);

    fci_ringbuffer_write(&RingBuffer, data, len);
    
    wake_up_interruptible(&(RingBuffer.queue));

    mutex_unlock(&ringbuffer_lock);

    return 0;
}

static int isdbt_thread(void *hDevice)
{
    struct ISDBT_INIT_INFO_T *hInit = (struct ISDBT_INIT_INFO_T *)hDevice;

#ifdef MTK_FTRACE_TEST
    unsigned long previous_time = 0;
    unsigned int isdbt_ftrace_mode = 0;
#endif

    set_user_nice(current, -20);

    print_log(hInit, "[dtv] isdbt_kthread enter\n");

    bbm_com_ts_callback_register((intptr_t)hInit, data_callback);

    init_waitqueue_head(&isdbt_isr_wait);

    while (1) {
        wait_event_interruptible(isdbt_isr_wait
            , isdbt_isr_sig || kthread_should_stop());

        if (driver_mode == ISDBT_POWERON) {
                    driver_mode = ISDBT_DATAREAD;

                    //print_log(hInit, "isdbt_kthread isr\n");

                    bbm_com_isr(NULL);
                    if (driver_mode == ISDBT_DATAREAD)
                        driver_mode = ISDBT_POWERON;
        }

        if (isdbt_isr_sig > 0) {
#ifdef MTK_FTRACE_TEST
            isdbt_isr_sig--;
            if (isdbt_isr_sig > 0) {
                if (isdbt_ftrace_mode == 0) {
                    if (isdbt_isr_sig > 1) {
                        tracing_off();
                        isdbt_ftrace_mode = 1;
                    } else if (isdbt_isr_sig) {
                        if ((previous_time) &&
                            ((unsigned long)jiffies
                            - previous_time)
                            < 300) {
                            tracing_off();
                            isdbt_ftrace_mode = 1;
                        }
                        previous_time
                        = (unsigned long)jiffies;
                    }
                }
                isdbt_isr_sig = 0;
            }
#else
            isdbt_isr_sig--;
            if (isdbt_isr_sig > 0)
                isdbt_isr_sig = 0;
#endif
        }

        if (kthread_should_stop())
            break;
    }

    bbm_com_ts_callback_deregister();

    printk("[dtv] isdbt_kthread exit\n");

    return 0;
}

void fci_irq_disable(void)
{
    isdbt_isr_sig = 0;

    printk("[dtv] fci_irq_disable %d\n", fc8180_pdata.spidev->irq);
}

void fci_irq_enable(void)
{
    isdbt_isr_sig = 0;

    printk("[dtv] fci_irq_enable %d\n", fc8180_pdata.spidev->irq);
}

void broadcast_fci_ringbuffer_flush(void)
{
    fci_ringbuffer_flush(&RingBuffer);
}

static int broadcast_spi_remove(struct spi_device *spi)
{
    struct isdbt_platform_data* tpdata = isdbt_get_platform_data();
    printk("[dtv] broadcast_spi_remove\n");
    if(tpdata == NULL) {
        printk("[dtv] isdbt_platform_data null\n");
        return ERROR;
    }

    free_irq(tpdata->spidev->irq, NULL);

    if (isdbt_kthread) {
        kthread_stop(isdbt_kthread);
        isdbt_kthread = NULL;
    }
    bbm_com_hostif_deselect(NULL);

//change wakeup_source_trash() -> wakeup_source_unregister() by SM8350 (kernel 5.4)
    wakeup_source_unregister(tpdata->wakelock);

    memset((void*)tpdata, 0x0, sizeof(struct isdbt_platform_data));
    return OK;
}

static int broadcast_spi_probe(struct spi_device *spi)
{
    int rc = 0;
    struct isdbt_platform_data* tpdata = isdbt_get_platform_data();
    if(spi == NULL || tpdata == NULL){
        printk("[dtv] spi is NULL, so spi can not be set\n");
        return -1;
    }

    tpdata->pdev = to_platform_device(&spi->dev);

    /*
     * Read DeviceTree Info. about GPIO/U32/bool property
     */
    if(isdbt_dt_to_platform_data(tpdata) != OK) {
        printk("[dtv] dt_to_platform_data error\n");
        return -1;
    }

    tpdata->spidev = spi;
    tpdata->spidev->mode = SPI_MODE_0;
    tpdata->spidev->bits_per_word = 8;

    /*
     * Initialize Power(LDO)
     */
    if(isdbt_power_init(tpdata)!= OK) {
        printk("[dtv] isdbt_power_init error\n");
        return ERROR;
    }

    /*
     * Initialize Clock Source
     */
    if(isdbt_clk_init(tpdata) != OK) {
        printk("[dtv] isdbt_clk_init error\n");
        return ERROR;
    }

    /*
     * Initialize pintctrl
     */
    if(isdbt_pinctrl_init(tpdata) != OK) {
        printk("[dtv] isdbt_pinctrl_init error\n");
        return ERROR;
    }

    rc = spi_setup(spi);
    if (rc) {
        printk("[dtv] Spi setup error(%d)\n", rc);
        return rc;
    }

//Use to request_threaded_irq with kerenl version 3.18
//    rc = request_irq(spi->irq, isdbt_irq, IRQF_DISABLED | IRQF_TRIGGER_FALLING, spi->dev.driver->name, NULL);

//Use to request_threaded_irq with kerenl version 4.9
    rc = request_threaded_irq(spi->irq, NULL, isdbt_irq, IRQF_ONESHOT | IRQF_TRIGGER_FALLING, spi->dev.driver->name, tpdata);
    if (rc){
        printk("[dtv] dmb request irq fail : %d\n", rc);
    }

//change wakeup_source_init() -> wakeup_source_register() by SM8350 (kernel 5.4)
    tpdata->wakelock = wakeup_source_register(&tpdata->spidev->dev, dev_name(&spi->dev));
    if(!tpdata->wakelock) {
        printk("[%s]wakeup_source_register failed\n", __func__);
        return -ENOMEM;
    }

    fci_ringbuffer_init(&RingBuffer, &static_ringbuffer[0], RING_BUFFER_SIZE);
    if (!isdbt_kthread) {
        printk("[dtv] kthread run\n");
        isdbt_kthread = kthread_run(isdbt_thread, NULL, "isdbt_thread");
    }

    rc = broadcast_dmb_drv_start(&device_fc8180);
    if (rc) {
        printk("[dtv] Failed to load Device (%d)\n", rc);
        rc = ERROR;
    }

    isdbt_print_driver_version();

    if (rc < 0)
        goto free_irq;

    fci_irq_disable(); /* Must disabled */

    if(broadcast_check_chip_id() != OK) {
        printk("[dtv] Chip ID read fail!!\n");
        rc = ERROR;
    }

    printk("[dtv] broadcast_spi_probe End.\n");
    return rc;

free_irq:
    broadcast_spi_remove(tpdata->spidev);

    return rc;
}

static int broadcast_check_chip_id(void)
{
    int rc = ERROR;

    rc = isdbt_fc8180_power_on();
    print_log(0, "[dtv] isdbt_fc8180_power_on rc : %d \n", rc);

    rc |= bbm_com_hostif_select(NULL, BBM_SPI);
    print_log(0, "[dtv] bbm_com_hostif_select rc : %d \n", rc);

    rc |= bbm_com_i2c_init(NULL, FCI_HPI_TYPE);
    print_log(0, "[dtv] bbm_com_i2c_init rc : %d \n", rc);

#ifdef BBM_SPI_IF
    rc |= bbm_com_byte_write(NULL, BBM_DM_DATA, 0x00);
    print_log(0, "[dtv] bbm_com_byte_write rc : %d \n", rc);
#endif

    rc |= bbm_com_probe(NULL);
    print_log(0, "[dtv] bbm_com_probe rc : %d \n", rc);

    rc |= isdbt_fc8180_power_off();

    if(rc != OK) {
        print_log(0, "[dtv] broadcast_check_chip_id error rc : %d \n", rc);
        rc = ERROR;
    }

    return rc;
}

static void async_fc8080_drv_init(void *data, async_cookie_t cookie)
{
    int rc;

    rc = spi_register_driver(&broadcast_spi_driver);
    if (rc < 0)
        printk("[dtv] SPI driver register failed(%d)\n", rc);

    return;
}

static int __broadcast_dev_init broadcast_dmb_fc8180_drv_init(void)
{
    struct isdbt_platform_data* tpdata = isdbt_get_platform_data();
    if(tpdata == NULL || broadcast_dmb_drv_check_module_init() != OK) {
        return ERROR;
    }

    memset((void*)tpdata, 0x0, sizeof(struct isdbt_platform_data));
    async_schedule(async_fc8080_drv_init, NULL);

    return OK;
}

static void __exit broadcast_dmb_fc8180_drv_exit(void)
{
    spi_unregister_driver(&broadcast_spi_driver);
    printk("[dtv] %s\n", __func__);
}

module_init(broadcast_dmb_fc8180_drv_init);
module_exit(broadcast_dmb_fc8180_drv_exit);
MODULE_DESCRIPTION("broadcast_dmb_drv_init");
MODULE_LICENSE("FCI");

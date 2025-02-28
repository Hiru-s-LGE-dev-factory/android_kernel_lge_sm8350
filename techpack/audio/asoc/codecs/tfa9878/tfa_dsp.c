/*
 * Copyright (C) 2014-2020 NXP Semiconductors, All Rights Reserved.
 * Copyright 2020 GOODIX, All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#if !defined(DEBUG)
#define DEBUG
#endif

#include "inc/dbgprint.h"
#include "inc/tfa_device.h"
#include "inc/tfa_container.h"
#include "inc/tfa.h"
#include "inc/tfa98xx_tfafieldnames.h"
#include "inc/tfa_internal.h"

/* handle macro for bitfield */
#define TFA_MK_BF(reg, pos, len) ((reg<<8)|(pos<<4)|(len-1))

/* abstract family for register */
#define FAM_TFA98XX_CF_CONTROLS (TFA_FAM(tfa,RST) >> 8)
#define FAM_TFA98XX_CF_MEM      (TFA_FAM(tfa,MEMA)>> 8)
#define FAM_TFA98XX_MTP0        (TFA_FAM(tfa,MTPOTC) >> 8)
#define FAM_TFA98xx_INT_EN      (TFA_FAM(tfa,INTENVDDS) >> 8)

#define CF_STATUS_I2C_CMD_ACK 0x01

/* Defines below are used for irq function (this removed the genregs include) */
#define TFA98XX_INTERRUPT_ENABLE_REG1		0x48
#define TFA98XX_INTERRUPT_IN_REG1		0x44
#define TFA98XX_INTERRUPT_OUT_REG1		0x40
#define TFA98XX_STATUS_POLARITY_REG1		0x4c
#define TFA98XX_KEY2_PROTECTED_MTP0_MTPEX_MSK	0x2
#define TFA98XX_KEY2_PROTECTED_MTP0_MTPOTC_MSK	0x1
#define TFA98XX_KEY2_PROTECTED_MTP0_MTPEX_POS	1
#define TFA98XX_KEY2_PROTECTED_MTP0_MTPOTC_POS	0

void tfanone_ops(struct tfa_device_ops *ops);
void tfa9872_ops(struct tfa_device_ops *ops);
void tfa9874_ops(struct tfa_device_ops *ops);
void tfa9878_ops(struct tfa_device_ops *ops);
void tfa9912_ops(struct tfa_device_ops *ops);
void tfa9888_ops(struct tfa_device_ops *ops);
void tfa9891_ops(struct tfa_device_ops *ops);
void tfa9897_ops(struct tfa_device_ops *ops);
void tfa9896_ops(struct tfa_device_ops *ops);
void tfa9890_ops(struct tfa_device_ops *ops);
void tfa9895_ops(struct tfa_device_ops *ops);
void tfa9894_ops(struct tfa_device_ops *ops);

#ifndef MIN
#define MIN(A, B) ((A < B) ? A : B)
#endif

/* retry values */
#define CFSTABLE_TRIES		10
#define AMPOFFWAIT_TRIES	50
#define MTPBWAIT_TRIES		50
#define MTPEX_WAIT_NTRIES	50
#define ACS_RESET_WAIT_NTRIES 10

/* read interval */
#define BUSLOAD_INTERVAL	10
#define CAL_STATUS_INTERVAL	100
#define RAMPING_INTERVAL	1

#define REDUCED_REGISTER_SETTING
#define WRITE_CALIBRATION_DATA_TO_MTP
#define CHECK_CALIBRATION_DATA_RANGE
#define SET_CALIBRATION_AT_ALL_DEVICE_READY
#undef SET_DUMMY_CALIBRATION_VALUE
#define WRITE_CALIBRATION_DATA_PARTLY
#define DETECT_DAMAGE_WITH_EVENT

#if defined(CHECK_CALIBRATION_DATA_RANGE)
static enum tfa98xx_error
tfa_calibration_range_check(struct tfa_device *tfa,
	unsigned int channel, int mohm);
#endif

/* calibration done executed */
#define TFA_MTPEX_POS TFA98XX_KEY2_PROTECTED_MTP0_MTPEX_POS /**/

/*
 * static variables
 */
static DEFINE_MUTEX(dev_lock);
static int devcount; /* total number of devices in use */
static int activecount; /* total number of active devices */
static int config_count; /* check the number of configured devices */
static int set_count; /* check the number of set devices */
static int dsp_cal_value[MAX_HANDLES] = {-1, -1, -1, -1};

static enum tfa98xx_error tfa_process_re25(struct tfa_device *tfa);

#if defined(MPLATFORM)
/* enqueue work to separate thread after calibration */
static void tfa_wait_cal_work(struct work_struct *work)
{
	enum tfa98xx_error cal_err = TFA98XX_ERROR_OK;
	struct tfa_device *tfa
		= container_of(work, struct tfa_device, wait_cal_work.work);
	int i;

	pr_info("%s: enter with dev_idx %d\n", __func__, tfa->dev_idx);

	cal_err = tfa_wait_cal(tfa);

	for (i = 0; i < devcount; i++) {
		struct tfa_device *ntfa
			= tfa98xx_get_tfa_device_from_index(i);

		if (ntfa == NULL)
			continue;

		if (cal_err != TFA98XX_ERROR_OK)
			ntfa->is_configured = -1;

#if defined(TFA_USE_DEVICE_SPECIFIC_CONTROL)
		if ((ntfa->active_handle != -1)
			&& (ntfa->active_handle != i))
			continue;
#endif

		if (cal_err == TFA98XX_ERROR_OK
			&& !ntfa->spkr_damaged) {
			/* force UNMUTE after processing calibration */
			pr_debug("%s: [%d] force UNMUTE after processing calibration\n",
				__func__, ntfa->dev_idx);
			tfa_dev_set_state(ntfa, TFA_STATE_UNMUTE, 1);
			continue;
		}

#if defined(TFA_BYPASS_AT_START_FAILURE)
		if (ntfa->spkr_damaged) {
			pr_info("[%d] stop damaged device!\n",
				ntfa->dev_idx);
			tfa_dev_stop(ntfa);
			continue;
		}
#if defined(TFA_REDUCE_BYPASS_GAIN)
		else {
			int cur_tdmdspkg
				= TFA7x_GET_BF(ntfa, TDMSPKG);

			if (cur_tdmdspkg < TFA_REDUCED_TDMSPKG_IN_BYPASS) {
				pr_info("[%d] reduce TDMSPKG (%d to %d) in bypass!\n",
					ntfa->dev_idx, cur_tdmdspkg,
					TFA_REDUCED_TDMSPKG_IN_BYPASS);

				/* set TDMSPKG as 7 dB for safety */
				TFA7x_SET_BF(ntfa, TDMSPKG,
					TFA_REDUCED_TDMSPKG_IN_BYPASS);

				/* reload configuration from scratch next time */
				ntfa->first_after_boot = 1;
			}
		}
#endif /* TFA_REDUCE_BYPASS_GAIN */
		tfa_dev_set_state(ntfa, TFA_STATE_UNMUTE, 1);
#endif /* TFA_BYPASS_AT_START_FAILURE */
#if defined(TFA_STOP_AT_START_FAILURE)
		pr_info("[%d] stop damaged device!\n",
			ntfa->dev_idx);
		tfa_dev_stop(ntfa);
#endif /* TFA_STOP_AT_START_FAILURE */
	}
}
#endif

#if defined(USE_TFA9891)
/* return sign extended tap pattern */
int tfa_get_tap_pattern(struct tfa_device *tfa)
{
	int value = tfa_get_bf(tfa, TFA9912_BF_CFTAPPAT);
	int bitshift;
	uint8_t field_len = 1 + (TFA9912_BF_CFTAPPAT & 0x0f);
	/* length of bitfield */

	bitshift = 8 * sizeof(int) - field_len;
	/* signextend */
	value = (value << bitshift) >> bitshift;

	return value;
}
#endif /* USE_TFA9891 */

/*
 * interrupt bit function to clear
 */
int tfa_irq_clear(struct tfa_device *tfa, enum tfa9912_irq bit)
{
	unsigned char reg;

	/* make bitfield enum */
	if (bit == tfa9912_irq_all) {
		/* operate on all bits */
		for (reg = TFA98XX_INTERRUPT_IN_REG1;
			reg < TFA98XX_INTERRUPT_IN_REG1 + 3; reg++)
			reg_write(tfa, reg, 0xffff); /* all bits */
	} else if (bit < tfa9912_irq_max) {
		reg = (unsigned char)
			(TFA98XX_INTERRUPT_IN_REG1 + (bit >> 4));
		reg_write(tfa, reg, 1 << (bit & 0x0f));
		/* only this bit */
	} else {
		return TFA_ERROR;
	}

	return 0;
}

/*
 * return state of irq or -1 if illegal bit
 */
int tfa_irq_get(struct tfa_device *tfa, enum tfa9912_irq bit)
{
	uint16_t value;
	int reg, mask;

	if (bit < tfa9912_irq_max) {
		/* only this bit */
		reg = TFA98XX_INTERRUPT_OUT_REG1 + (bit >> 4);
		mask = 1 << (bit & 0x0f);
		reg_read(tfa, (unsigned char)reg, &value);
	} else {
		return TFA_ERROR;
	}

	return (value & mask) != 0;
}

/*
 * interrupt bit function that operates on the shadow regs in the handle
 */
int tfa_irq_ena(struct tfa_device *tfa, enum tfa9912_irq bit, int state)
{
	uint16_t value, new_value;
	int reg = 0, mask;

	/* */
	if (bit == tfa9912_irq_all) {
		/* operate on all bits */
		for (reg = TFA98XX_INTERRUPT_ENABLE_REG1;
			reg <= TFA98XX_INTERRUPT_ENABLE_REG1
			+ tfa9912_irq_max / 16; reg++) {
			reg_write(tfa,
				(unsigned char)reg,
				state ? 0xffff : 0); /* all bits */
			tfa->interrupt_enable
				[reg - TFA98XX_INTERRUPT_ENABLE_REG1]
				= state ? 0xffff : 0; /* all bits */
		}
	} else if (bit < tfa9912_irq_max) {
		/* only this bit */
		reg = TFA98XX_INTERRUPT_ENABLE_REG1 + (bit >> 4);
		mask = 1 << (bit & 0x0f);
		reg_read(tfa, (unsigned char)reg, &value);
		if (state) //set
			new_value = (uint16_t)(value | mask);
		else // clear
			new_value = value & ~mask;
		if (new_value != value) {
			reg_write(tfa,
				(unsigned char)reg,
				new_value); /* only this bit */
			tfa->interrupt_enable
				[reg - TFA98XX_INTERRUPT_ENABLE_REG1]
				= new_value;
		}
	} else {
		return TFA_ERROR;
	}

	return 0;
}

/*
 * mask interrupts by disabling them
 */
int tfa_irq_mask(struct tfa_device *tfa)
{
	int reg;

	/* operate on all bits */
	for (reg = TFA98XX_INTERRUPT_ENABLE_REG1;
		reg <= TFA98XX_INTERRUPT_ENABLE_REG1
			+ tfa9912_irq_max / 16; reg++)
		reg_write(tfa, (unsigned char)reg, 0);

	return 0;
}

/*
 * unmask interrupts by enabling them again
 */
int tfa_irq_unmask(struct tfa_device *tfa)
{
	int reg;

	/* operate on all bits */
	for (reg = TFA98XX_INTERRUPT_ENABLE_REG1;
		reg <= TFA98XX_INTERRUPT_ENABLE_REG1
		+ tfa9912_irq_max / 16; reg++)
		reg_write(tfa, (unsigned char)reg,
			tfa->interrupt_enable
			[reg - TFA98XX_INTERRUPT_ENABLE_REG1]);

	return 0;
}

/*
 * interrupt bit function that sets the polarity
 */
int tfa_irq_set_pol(struct tfa_device *tfa, enum tfa9912_irq bit, int state)
{
	uint16_t value, new_value;
	int reg = 0, mask;

	if (bit == tfa9912_irq_all) {
		/* operate on all bits */
		for (reg = TFA98XX_STATUS_POLARITY_REG1;
			reg <= TFA98XX_STATUS_POLARITY_REG1
			+ tfa9912_irq_max / 16; reg++) {
			reg_write(tfa,
				(unsigned char)reg,
				state ? 0xffff : 0); /* all bits */
		}
	} else if (bit < tfa9912_irq_max) {
		/* only this bit */
		reg = TFA98XX_STATUS_POLARITY_REG1 + (bit >> 4);
		mask = 1 << (bit & 0x0f);
		reg_read(tfa, (unsigned char)reg, &value);
		if (state) /* Active High */
			new_value = (uint16_t)(value | mask);
		else /* Active Low */
			new_value = value & ~mask;
		if (new_value != value)
			reg_write(tfa, (unsigned char)reg, new_value);
			/* only this bit */
	} else {
		return TFA_ERROR;
	}

	return 0;
}

/*
 * set device info and register device ops
 */
void tfa_set_query_info(struct tfa_device *tfa)
{
	/* invalidate device struct cached values */
	tfa->hw_feature_bits = -1;
	tfa->sw_feature_bits[0] = -1;
	tfa->sw_feature_bits[1] = -1;
	tfa->profile = -1;
	tfa->next_profile = -1;
	tfa->vstep = -1;
	/* defaults */
	tfa->is_probus_device = 0;
	tfa->advance_keys_handling = 0; /*artf65038*/
	tfa->tfa_family = 1;
	tfa->daimap = TFA98XX_DAI_I2S;		/* all others */
	tfa->spkr_count = 1;
	tfa->spkr_select = 0;
	tfa->spkr_damaged = 0;
	tfa->support_tcoef = SUPPORT_YES;
	tfa->support_drc = SUPPORT_NOT_SET;
	tfa->support_saam = SUPPORT_NOT_SET;
	/* respond to external DSP: -1:none, 0:no_dsp, 1:cold, 2:warm */
	tfa->ext_dsp = -1;
	tfa->bus = 0;
	tfa->partial_enable = 0;
#if defined(TFADSP_32BITS)
	tfa->convert_dsp32 = 1; /* conversion in kernel */
#else
	tfa->convert_dsp32 = 0;
#endif
	tfa->sync_iv_delay = 0;

	tfa->temp = 0xffff;
	tfa->is_cold = 1;
	tfa->is_bypass = 0;
	tfa->is_configured = 0;
	tfa->reset_mtpex = 0;
	tfa->stream_state = 0;
	tfa->prev_samstream = -1; /* not in use */
	tfa->first_after_boot = 1;
#if defined(TFA_USE_DEVICE_SPECIFIC_CONTROL)
	tfa->active_handle = -1;
#endif
	tfa->ampgain = -1;
	tfa->individual_msg = 0;
	tfa->fw_itf_ver[0] = 0xff;
	tfa->fw_lib_ver[0] = 0xff;
#if defined(TFA_BLACKBOX_LOGGING)
	tfa->blackbox_enable = 0;
	memset(tfa->log_data, 0, LOG_BUFFER_SIZE * sizeof(int));
#endif

#if defined(MPLATFORM)
	INIT_DELAYED_WORK(&tfa->wait_cal_work, tfa_wait_cal_work);
#endif

	/* TODO use the getfeatures() for retrieving the features [artf103523]
	 * tfa->support_drc = SUPPORT_NOT_SET;
	 */

	pr_info("%s: device type (0x%04x)\n", __func__, tfa->rev);
	switch (tfa->rev & 0xff) {
	case 0: /* tfanone : non-i2c external DSP device */
		/* e.g. qc adsp */
		tfa->support_drc = SUPPORT_YES;
		tfa->tfa_family = 0;
		tfa->spkr_count = 0;
		tfa->daimap = 0;
		tfanone_ops(&tfa->dev_ops); /* register device operations via tfa hal */
		tfa->bus = 1;
		break;
	case 0x72:
		/* tfa9872 */
		tfa->support_drc = SUPPORT_YES;
		tfa->tfa_family = 2;
		tfa->spkr_count = 1;
		tfa->is_probus_device = 1;
		tfa->ext_dsp = 1; /* set DSP-free by force */
		tfa->daimap = TFA98XX_DAI_TDM;
		tfa9872_ops(&tfa->dev_ops); /* register device operations */
		break;
	case 0x74:
		/* tfa9874 */
		tfa->support_drc = SUPPORT_YES;
		tfa->tfa_family = 2;
		tfa->spkr_count = 1;
		tfa->is_probus_device = 1;
		tfa->ext_dsp = 1; /* set DSP-free by force */
		tfa->daimap = TFA98XX_DAI_TDM;
		tfa9874_ops(&tfa->dev_ops); /* register device operations */
		break;
	case 0x78:
		/* tfa9878 */
		tfa->support_drc = SUPPORT_YES;
		tfa->tfa_family = 2;
		tfa->spkr_count = 1;
		tfa->is_probus_device = 1;
		tfa->ext_dsp = 1; /* set DSP-free by force */
		tfa->advance_keys_handling = 1; /*artf65038*/
		tfa->daimap = TFA98XX_DAI_TDM;
		tfa9878_ops(&tfa->dev_ops); /* register device operations */
		break;
	case 0x88:
		/* tfa9888 */
		tfa->tfa_family = 2;
		tfa->spkr_count = 2;
		tfa->daimap = TFA98XX_DAI_TDM;
		tfa9888_ops(&tfa->dev_ops); /* register device operations */
		break;
	case 0x97:
		/* tfa9897 */
		tfa->support_drc = SUPPORT_NO;
		tfa->spkr_count = 1;
		tfa->daimap = TFA98XX_DAI_TDM;
		tfa9897_ops(&tfa->dev_ops); /* register device operations */
		break;
	case 0x96:
		/* tfa9896 */
		tfa->support_drc = SUPPORT_NO;
		tfa->spkr_count = 1;
		tfa->daimap = TFA98XX_DAI_TDM;
		tfa9896_ops(&tfa->dev_ops); /* register device operations */
		break;
	case 0x92:
		/* tfa9891 */
		tfa->spkr_count = 1;
		tfa->daimap = (TFA98XX_DAI_PDM | TFA98XX_DAI_I2S);
		tfa9891_ops(&tfa->dev_ops); /* register device operations */
		break;
	case 0x91:
		/* tfa9890B */
		tfa->spkr_count = 1;
		tfa->daimap = (TFA98XX_DAI_PDM | TFA98XX_DAI_I2S);
		break;
	case 0x80:
	case 0x81:
		/* tfa9890 */
		tfa->spkr_count = 1;
		tfa->daimap = TFA98XX_DAI_I2S;
		tfa->support_drc = SUPPORT_NO;
		tfa->support_framework = SUPPORT_NO;
		tfa9890_ops(&tfa->dev_ops); /* register device operations */
		break;
	case 0x12:
		/* tfa9895 */
		tfa->spkr_count = 1;
		tfa->daimap = TFA98XX_DAI_I2S;
		tfa9895_ops(&tfa->dev_ops); /* register device operations */
		break;
	case 0x13:
		/* tfa9912 */
		tfa->tfa_family = 2;
		tfa->spkr_count = 1;
		tfa->daimap = TFA98XX_DAI_TDM;
		tfa9912_ops(&tfa->dev_ops); /* register device operations */
		break;
	case 0x94:
		/* tfa9894 */
		tfa->tfa_family = 2;
		tfa->spkr_count = 1;
		tfa->daimap = TFA98XX_DAI_TDM;
		tfa9894_ops(&tfa->dev_ops); /* register device operations */
		break;
	default:
		pr_err("unknown device type : 0x%02x\n", tfa->rev);
		_ASSERT(0);
		break;
	}
}

#if defined(TFADSP_DSP_BUFFER_POOL)
enum tfa98xx_error tfa_buffer_pool(struct tfa_device *tfa,
	int index, int size, int control)
{
	if (tfa->dev_idx != 0) { /* use master device only */
		pr_err("%s: failed to set master device (%d)\n",
			__func__, tfa->dev_idx);
		return TFA_ERROR;
	}

	switch (control) {
	case POOL_ALLOC: // allocate
		tfa->buf_pool[index].pool =
			kmalloc(size, GFP_KERNEL);
		if (tfa->buf_pool[index].pool == NULL) {
			tfa->buf_pool[index].size = 0;
			tfa->buf_pool[index].in_use = 1;
			pr_err("%s: buffer_pool[%d] - kmalloc error %d bytes\n",
				__func__, index, size);
			return TFA98XX_ERROR_FAIL;
		}
		pr_debug("%s: buffer_pool[%d] - kmalloc allocated %d bytes\n",
			__func__, index, size);
		tfa->buf_pool[index].size = size;
		tfa->buf_pool[index].in_use = 0;
		break;

	case POOL_FREE: // deallocate
		if (tfa->buf_pool[index].pool != NULL)
			kfree(tfa->buf_pool[index].pool);
		pr_debug("%s: buffer_pool[%d] - kfree\n",
			__func__, index);
		tfa->buf_pool[index].pool = NULL;
		tfa->buf_pool[index].size = 0;
		tfa->buf_pool[index].in_use = 0;
		break;

	default:
		pr_err("%s: wrong control\n", __func__);
		break;
	}

	return TFA98XX_ERROR_OK;
}

int tfa98xx_buffer_pool_access(int r_index,
	size_t g_size, uint8_t **buf, int control)
{
	int index;
	struct tfa_device *tfa
		= tfa98xx_get_tfa_device_from_index(0);

	if (tfa == NULL) {
		pr_err("%s: failed to get master device\n",
			__func__);
		return TFA_ERROR;
	}
	if (tfa->dev_idx != 0) {
		pr_err("%s: failed to get master device (%d)\n",
			__func__, tfa->dev_idx);
		return TFA_ERROR;
	}

	switch (control) {
	case POOL_GET: /* get */
		pr_debug("%s: dev %d, request buffer_pool, size=%d\n",
			 __func__, tfa->dev_idx, (int)g_size);
		*buf = NULL;
		for (index = 0; index < POOL_MAX_INDEX; index++) {
			if (tfa->buf_pool[index].in_use) {
				pr_debug("%s: buffer_pool[%d] is in use\n",
					 __func__, index);
				continue;
			}
			if (tfa->buf_pool[index].size < (int)g_size) {
				pr_debug("%s: buffer_pool[%d] size %d < %d\n",
					 __func__, index,
					 tfa->buf_pool[index].size, (int)g_size);
				continue;
			}
			*buf = (uint8_t *)(tfa->buf_pool[index].pool);
			if (*buf == NULL) {
				pr_err("%s: found NULL in buffer_pool[%d]\n",
					__func__, index);
				continue;
			}
			tfa->buf_pool[index].in_use = 1;
			pr_debug("%s: get buffer_pool[%d]\n",
				 __func__, index);
			return index;
		}

		pr_err("%s: failed to get buffer_pool\n",
			__func__);
		break;

	case POOL_RETURN: /* return */
		pr_debug("%s: dev %d, release buffer_pool[%d]\n",
			 __func__, tfa->dev_idx, r_index);
		if (r_index < 0 || r_index >= POOL_MAX_INDEX) {
			pr_err("%s: out of range [%d]\n", __func__, r_index);
			return -1;
		}
		if (tfa->buf_pool[r_index].in_use == 0
			|| tfa->buf_pool[r_index].size == 0) {
			pr_debug("%s: buffer_pool[%d] is not in use\n",
				__func__, r_index);
			return -1; /* reset by force */
		}

		pr_debug("%s: return buffer_pool[%d]\n",
			 __func__, r_index);
		memset(tfa->buf_pool[r_index].pool,
			0, tfa->buf_pool[r_index].size);
		tfa->buf_pool[r_index].in_use = 0;

		return -1;

	default:
		pr_err("%s: wrong control\n", __func__);
		break;
	}

	return TFA_ERROR;
}
#endif /* TFADSP_DSP_BUFFER_POOL */

/*
 * lookup the device type and return the family type
 */
int tfa98xx_dev2family(int dev_type)
{
	/* only look at the die ID part (lsb byte) */
	switch (dev_type & 0xff) {
	case 0x12:
	case 0x80:
	case 0x81:
	case 0x91:
	case 0x92:
	case 0x97:
	case 0x96:
		return 1;
	case 0x88:
	case 0x72:
	case 0x13:
	case 0x74:
	case 0x94:
	case 0x78:
		return 2;
	case 0x50:
		return 3;
	default:
		return 0;
	}
}

/*
 * 	return the target address for the filter on this device
 *
 * filter_index:
 *	[0..9] reserved for EQ (not deployed, calc. is available)
 *	[10..12] anti-alias filter
 *	[13] integrator filter
 */
enum tfa98xx_dmem tfa98xx_filter_mem(struct tfa_device *tfa,
	int filter_index, unsigned short *address, int channel)
{
	enum tfa98xx_dmem dmem = -1;
	int idx;
	unsigned short bq_table[7][4] = {
		/* index: 10, 11, 12, 13 */
		{ 346,  351,  356,  288}, /* 87 BRA_MAX_MRA4-2_7.00 */
		{ 346,  351,  356,  288}, /* 90 BRA_MAX_MRA6_9.02 */
		{ 467,  472,  477,  409}, /* 95 BRA_MAX_MRA7_10.02 */
		{ 406,  411,  416,  348}, /* 97 BRA_MAX_MRA9_12.01 */
		{ 467,  472,  477,  409}, /* 91 BRA_MAX_MRAA_13.02 */
		{8832, 8837, 8842, 8847}, /* 88 part1 */
		{8853, 8858, 8863, 8868}  /* 88 part2 */
		/* Since the 88 is stereo we have 2 parts.
		 * Every index has 5 values except index 13
		 * this one has 6 values
		 */
	};

	if ((filter_index >= 10) && (filter_index <= 13)) {
		dmem = TFA98XX_DMEM_YMEM; /* for all devices */
		idx = filter_index - 10;

		switch (tfa->rev & 0xff) {
		/* only compare lower byte */
		case 0x12:
			*address = bq_table[2][idx];
			break;
		case 0x97:
			*address = bq_table[3][idx];
			break;
		case 0x96:
			*address = bq_table[3][idx];
			break;
		case 0x80:
		case 0x81: /* for the RAM version */
		case 0x91:
			*address = bq_table[1][idx];
			break;
		case 0x92:
			*address = bq_table[4][idx];
			break;
		case 0x88:
			/* Channel 1 = primary, 2 = secondary */
			if (channel == 1)
				*address = bq_table[5][idx];
			else
				*address = bq_table[6][idx];
			break;
		case 0x72:
		case 0x74:
		case 0x13:
		case 0x78:
		default:
			/* unsupported case, possibly intermediate version */
			// _ASSERT(0);
			return TFA98XX_DMEM_ERR;
		}
	}
	return dmem;
}

/************************ query functions *******************************/
/* no device involved
 * return revision
 * Used by the LTT
 */
void tfa98xx_rev(int *major, int *minor, int *revision)
{
	char version_str[] = TFA98XX_API_REV_STR;
	char residual[20] = {'\0'};
	int ret;

	ret = sscanf(version_str, "v%d.%d.%d-%s",
		major, minor, revision, residual);
	if (ret != 4)
		pr_err("%s: failure reading API revision", __func__);
}

/*
 * tfa_supported_speakers
 * returns the number of the supported speaker count
 */
enum tfa98xx_error tfa_supported_speakers(struct tfa_device *tfa, int *spkr_count)
{
	if (tfa->in_use == 0)
		return TFA98XX_ERROR_NOT_OPEN;
	else
		*spkr_count = tfa->spkr_count;

	return TFA98XX_ERROR_OK;
}

/*
 * tfa98xx_supported_saam
 * returns the supportedspeaker as microphone feature
 */
enum tfa98xx_error tfa98xx_supported_saam(struct tfa_device *tfa,
	enum tfa98xx_saam *saam)
{
	int features;
	enum tfa98xx_error error;

	if (tfa->support_saam == SUPPORT_NOT_SET) {
		error = tfa98xx_dsp_get_hw_feature_bits(tfa, &features);
		if (error != TFA98XX_ERROR_OK)
			return error;
		tfa->support_saam
			= (features & 0x8000) ? SUPPORT_YES : SUPPORT_NO;
		/* SAAM is bit15 */
	}
	*saam = tfa->support_saam == SUPPORT_YES
		? TFA98XX_SAAM : TFA98XX_SAAM_NONE;

	return TFA98XX_ERROR_OK;
}

/*
 * tfa98xx_set_stream_state
 * sets the stream: b0: pstream (Rx), b1: cstream (Tx), b2: samstream (SaaM)
 */
enum tfa98xx_error tfa98xx_set_stream_state(struct tfa_device *tfa,
	int stream_state)
{
#if defined(REDUCED_REGISTER_SETTING)
	int cur_samstream = (stream_state & BIT_SAMSTREAM) ? 1 : 0;
#endif

	pr_debug("%s: set stream_state=0x%04x\n", __func__, stream_state);

	tfa->stream_state = stream_state;
#if defined(REDUCED_REGISTER_SETTING)
	if (tfa->prev_samstream != cur_samstream) {
		pr_debug("%s: samstream toggled: %d -> %d\n",
			__func__, tfa->prev_samstream, cur_samstream);

		/* reload reg at toggling SAMSTREAM */
		tfa->first_after_boot = 1;

		tfa->prev_samstream = cur_samstream;
	}
#endif /* REDUCED_REGISTER_SETTING */

	return TFA98XX_ERROR_OK;
}

/*
 * tfa98xx_compare_features
 * Obtains features_from_MTP and features_from_cnt
 */
enum tfa98xx_error tfa98xx_compare_features(struct tfa_device *tfa,
	int features_from_MTP[3], int features_from_cnt[3])
{
	enum tfa98xx_error error = TFA98XX_ERROR_OK;
	uint32_t value;
	uint16_t mtpbf;
	unsigned char bytes[2 * 3];
	int status;

	tfa98xx_dsp_system_stable(tfa, &status);
	if (!status)
		/* Only test when we have a clock. */
		return TFA98XX_ERROR_NO_CLOCK;

	/* Set proper MTP location per device: */
	if (tfa->tfa_family == 1)
		mtpbf = 0x850f; /* MTP5 for tfa1,16 bits */
	else
		mtpbf = 0xf907; /* MTP9 for tfa2, 8 bits */

	/* Read HW features from MTP: */
	value = tfa_read_reg(tfa, mtpbf) & 0xffff;
	features_from_MTP[0] = tfa->hw_feature_bits = value;

	/* Read SW features: */
	error = tfa_dsp_cmd_id_write_read(tfa, MODULE_FRAMEWORK,
		FW_PAR_ID_GET_FEATURE_INFO, sizeof(bytes), bytes);
	if (error != TFA98XX_ERROR_OK)
		/* old ROM code may respond with TFA98XX_ERROR_RPC_PARAM_ID */
		return error;

	tfa98xx_convert_bytes2data(sizeof(bytes), bytes, &features_from_MTP[1]);

	/* check if feature bits from MTP match feature bits from cnt file: */
	get_hw_features_from_cnt(tfa, &features_from_cnt[0]);
	get_sw_features_from_cnt(tfa, &features_from_cnt[1]);

	return error;
}

/*************************** device specific ops ***************************/
/* the wrapper for DspReset, in case of full */
enum tfa98xx_error tfa98xx_dsp_reset(struct tfa_device *tfa, int state)
{
	if (!tfa->dev_ops.dsp_reset)
		return TFA98XX_ERROR_NOT_IMPLEMENTED;

	return (tfa->dev_ops.dsp_reset)(tfa, state);
}

/* the ops wrapper for tfa98xx_dsp_SystemStable */
enum tfa98xx_error tfa98xx_dsp_system_stable(struct tfa_device *tfa,
	int *ready)
{
	if (!tfa->dev_ops.dsp_system_stable)
		return TFA98XX_ERROR_NOT_IMPLEMENTED;

	return (tfa->dev_ops.dsp_system_stable)(tfa, ready);
}

/* the ops wrapper for tfa98xx_dsp_system_stable */
enum tfa98xx_error tfa98xx_auto_copy_mtp_to_iic(struct tfa_device *tfa)
{
	if (!tfa->dev_ops.auto_copy_mtp_to_iic)
		return TFA98XX_ERROR_NOT_IMPLEMENTED;

	return (tfa->dev_ops.auto_copy_mtp_to_iic)(tfa);
}

/* the ops wrapper for tfa98xx_faim_protect */
enum tfa98xx_error tfa98xx_faim_protect(struct tfa_device *tfa, int state)
{
	if (!tfa->dev_ops.faim_protect)
		return TFA98XX_ERROR_NOT_IMPLEMENTED;

	return (tfa->dev_ops.faim_protect)(tfa, state);
}

enum tfa98xx_error tfa98xx_dsp_write_tables(struct tfa_device *tfa,
	int sample_rate)
{
	if (!tfa->dev_ops.dsp_write_tables)
		return TFA98XX_ERROR_NOT_IMPLEMENTED;

	return (tfa->dev_ops.dsp_write_tables)(tfa, sample_rate);
}

/* set internal oscillator into power down mode.
 *
 * @param[in] tfa device description structure
 * @param[in] state new state 0 - oscillator is on, 1 oscillator is off.
 *
 * @return TFA98XX_ERROR_OK when successfull, error otherwise.
 */
enum tfa98xx_error tfa98xx_set_osc_powerdown(struct tfa_device *tfa, int state)
{
	if (!tfa->dev_ops.set_osc_powerdown)
		return TFA98XX_ERROR_NOT_IMPLEMENTED;

	return (tfa->dev_ops.set_osc_powerdown)(tfa, state);
}

/* update low power mode of the device.
 *
 * @param[in] tfa device description structure
 * @param[in] state new state 0 - LPMODE is on, 1 LPMODE is off.
 *
 * @return TFA98XX_ERROR_OK when successfull, error otherwise.
 */
enum tfa98xx_error tfa98xx_update_lpm(struct tfa_device *tfa, int state)
{
	if (!tfa->dev_ops.update_lpm)
		return TFA98XX_ERROR_NOT_IMPLEMENTED;

	return (tfa->dev_ops.update_lpm)(tfa, state);
}

/*
 * bring the device into a state similar to reset
 */
enum tfa98xx_error tfa98xx_init(struct tfa_device *tfa)
{
	enum tfa98xx_error error = TFA98XX_ERROR_OK;
	uint16_t value = 0;

	/* reset all i2C registers to default
	 * Write the register directly to avoid read in the bitfield function.
	 * The I2CR bit may overwrite full register because it is reset anyway.
	 * This will save a reg read transaction.
	 */
	TFA_SET_BF_VALUE(tfa, I2CR, 1, &value);
	TFA_WRITE_REG(tfa, I2CR, value);

#if 0
	if (tfa->tfa_family == 2) {
		/* restore MANSCONF and MANCOLD to POR state */
		TFA_SET_BF_VOLATILE(tfa, MANSCONF, 0);
		TFA_SET_BF_VOLATILE(tfa, MANCOLD, 1);
	}
#endif

	/* Put DSP in reset */
	tfa98xx_dsp_reset(tfa, 1); /* in pair of tfa_run_start_dsp() */

	/* some other registers must be set for optimal amplifier behaviour
	 * This is implemented in a file specific for the type number
	 */
	if (tfa->dev_ops.tfa_init)
		error = (tfa->dev_ops.tfa_init)(tfa);
	else
		pr_debug("%s: no init code\n", __func__);

	return error;
}

#if (defined(USE_TFA9892) || defined(USE_TFA9888))
/* check presence of powerswitch=1 in configuration and optimal setting.
 *
 * @param[in] tfa device description structure
 *
 * @return -1 when error, 0 or 1 depends on switch settings.
 */
int tfa98xx_powerswitch_is_enabled(struct tfa_device *tfa)
{
	uint16_t value;
	enum tfa98xx_error ret;

	if (((tfa->rev & 0xff) == 0x13) /* tfa9912 */
		|| ((tfa->rev & 0xff) == 0x88)) /* tfa9888 */ {
		ret = reg_read(tfa, 0xc6, &value);
		if (ret != TFA98XX_ERROR_OK)
			return TFA_ERROR;
		/* PLMA5539: Check actual value of powerswitch. */
		/* TODO: regmap v1.40 should make this bit public. */

		return (int)(value & (1u << 6));
	}

	return 1;
}
#endif /* USE_TFA9892 || USE_TFA9888 */

/********************* new tfa2 ****************************/
/* newly added messaging for tfa2 tfa1? */
enum tfa98xx_error tfa98xx_dsp_get_memory(struct tfa_device *tfa,
	int memory_type, int offset, int length, unsigned char bytes[])
{
	enum tfa98xx_error error = TFA98XX_ERROR_OK;
	char msg[4 * 3];
	int nr = 0;

	msg[nr++] = 8;
	msg[nr++] = MODULE_FRAMEWORK + 0x80;
	msg[nr++] = FW_PAR_ID_GET_MEMORY;

	msg[nr++] = 0;
	msg[nr++] = 0;
	msg[nr++] = (char)memory_type;

	msg[nr++] = 0;
	msg[nr++] = (offset >> 8) & 0xff;
	msg[nr++] = offset & 0xff;

	msg[nr++] = 0;
	msg[nr++] = (length >> 8) & 0xff;
	msg[nr++] = length & 0xff;

	/* send msg */
	error = dsp_msg(tfa, nr, (char *)msg);

	if (error != TFA98XX_ERROR_OK)
		return error;

	/* read the data from the device (length * 3 = words) */
	error = dsp_msg_read(tfa, length * 3, bytes);

	return error;
}

enum tfa98xx_error tfa98xx_dsp_set_memory(struct tfa_device *tfa,
	int memory_type, int offset, int length, int value)
{
	enum tfa98xx_error error = TFA98XX_ERROR_OK;
	int nr = 0;
	char msg[5 * 3];

	msg[nr++] = 8;
	msg[nr++] = MODULE_FRAMEWORK + 0x80;
	msg[nr++] = FW_PAR_ID_SET_MEMORY;

	msg[nr++] = 0;
	msg[nr++] = 0;
	msg[nr++] = (char)memory_type;

	msg[nr++] = 0;
	msg[nr++] = (offset >> 8) & 0xff;
	msg[nr++] = offset & 0xff;

	msg[nr++] = 0;
	msg[nr++] = (length >> 8) & 0xff;
	msg[nr++] = length & 0xff;

	msg[nr++] = (value >> 16) & 0xff;
	msg[nr++] = (value >> 8) & 0xff;
	msg[nr++] = value & 0xff;

	/* send msg */
	error = dsp_msg(tfa, nr, (char *)msg);

	return error;
}

/****************************** calibration support **************************/
/*
 * get/set the mtp with user controllable values
 * check if the relevant clocks are available
 */
enum tfa98xx_error tfa98xx_get_mtp(struct tfa_device *tfa, uint16_t *value)
{
	int status;
	int result;

	/* not possible if PLL in powerdown */
	if (TFA_GET_BF(tfa, PWDN)) {
		pr_debug("PLL in powerdown\n");
		return TFA98XX_ERROR_NO_CLOCK;
	}

	tfa98xx_dsp_system_stable(tfa, &status);
	if (status == 0) {
		pr_debug("PLL not running\n");
		return TFA98XX_ERROR_NO_CLOCK;
	}

	result = TFA_READ_REG(tfa, MTP0);
	if (result < 0)
		return -result;
	*value = (uint16_t)result;

	return TFA98XX_ERROR_OK;
}

/*
 * lock or unlock KEY2
 * lock = 1 will lock
 * lock = 0 will unlock
 * note that on return all the hidden key will be off
 */
void tfa98xx_key2(struct tfa_device *tfa, int lock)
{
	/* unhide lock registers */
	reg_write(tfa, (tfa->tfa_family == 1) ? 0x40 : 0x0f, 0x5a6b);
	/* lock/unlock key2 MTPK */
	TFA_WRITE_REG(tfa, MTPKEY2, lock ? 0 : 0x5a);
	/* unhide lock registers */
	if (!tfa->advance_keys_handling) /*artf65038*/
		reg_write(tfa, (tfa->tfa_family == 1) ? 0x40 : 0x0f, 0);
}

void tfa2_manual_mtp_cpy(struct tfa_device *tfa,
	uint16_t reg_row_to_keep, uint16_t reg_row_to_set, uint8_t row)
{
	uint16_t value;
	int loop = 0;
	enum tfa98xx_error error;

	/* Assure FAIM is enabled (enable it when neccesery) */
	if (tfa->is_probus_device)
	{
		error = tfa98xx_faim_protect(tfa, 1);
		if (tfa->verbose)
			pr_debug("FAIM enabled (err:%d).\n", error);
	}
	reg_read(tfa, (unsigned char)reg_row_to_keep, &value);
	if (!row)
	{
		reg_write(tfa, 0xa7, value);
		reg_write(tfa, 0xa8, reg_row_to_set);
	}
	else
	{
		reg_write(tfa, 0xa7, reg_row_to_set);
		reg_write(tfa, 0xa8, value);
	}
	reg_write(tfa, 0xa3, 0x10 | row);
	if (tfa->is_probus_device)
	{
		/* Assure FAIM is enabled (enable it when neccesery) */
		for (loop = 0; loop < 100 /* x 10ms */; loop++) {
			/* wait 10ms to avoid busload */
			msleep_interruptible(BUSLOAD_INTERVAL);
			if (tfa_dev_get_mtpb(tfa) == 0)
				break;
		}
		error = tfa98xx_faim_protect(tfa, 0);
		if (tfa->verbose)
			pr_debug("FAIM disabled (err:%d).\n", error);
	}
}

enum tfa98xx_error tfa98xx_set_mtp(struct tfa_device *tfa,
	uint16_t value, uint16_t mask)
{
	unsigned short mtp_old, mtp_new;
	int loop, status;
	enum tfa98xx_error error;

	error = tfa98xx_get_mtp(tfa, &mtp_old);

	if (error != TFA98XX_ERROR_OK)
		return error;

	mtp_new = (value & mask) | (mtp_old & ~mask);

	if (mtp_old == mtp_new) /* no change */ {
		if (tfa->verbose)
			pr_info("No change in MTP. Value not written!\n");
		return TFA98XX_ERROR_OK;
	}
	error = tfa98xx_update_lpm(tfa, 1);
	if (error)
		return error;
	/* Assure FAIM is enabled (enable it when neccesery) */
	error = tfa98xx_faim_protect(tfa, 1);
	if (error)
		return error;
	if (tfa->verbose)
		pr_debug("MTP clock enabled.\n");

	/* assure that the clock is up, else we can't write MTP */
	error = tfa98xx_dsp_system_stable(tfa, &status);
	if (error)
		return error;
	if (status == 0)
		return TFA98XX_ERROR_NO_CLOCK;

	tfa98xx_key2(tfa, 0); /* unlock */
	TFA_WRITE_REG(tfa, MTP0, mtp_new); 	/* write to i2c shadow reg */
	/* CIMTP=1 start copying all the data from i2c regs_mtp to mtp*/
	if (tfa->tfa_family == 2)
		tfa2_manual_mtp_cpy(tfa, 0xf1, mtp_new, 0);
	else
		TFA_SET_BF(tfa, CIMTP, 1);
	/* wait until MTP write is done */
	error = TFA98XX_ERROR_STATE_TIMED_OUT;
	for (loop = 0; loop < 100 /* x 10ms */; loop++) {
		/* wait 10ms to avoid busload */
		msleep_interruptible(BUSLOAD_INTERVAL);
		if (tfa_dev_get_mtpb(tfa) == 0) {
			error = TFA98XX_ERROR_OK;
			break;
		}
	}
	tfa98xx_key2(tfa, 1); /* lock */
	/* MTP setting failed due to timeout ?*/
	if (error) {
		tfa98xx_faim_protect(tfa, 0);
		return error;
	}

	/* Disable the FAIM, if this is neccessary */
	error = tfa98xx_faim_protect(tfa, 0);
	if (error)
		return error;
	if (tfa->verbose)
		pr_debug("MTP clock disabled.\n");
	error = tfa98xx_update_lpm(tfa, 0);

	return error;
}

/*
 * clear mtpex
 * set ACS
 * start tfa
 */
int tfa_calibrate(struct tfa_device *tfa)
{
	enum tfa98xx_error error;

	tfa->is_cold = 1;

	/* clear mtpex */
	error = tfa98xx_set_mtp(tfa, 0,
		TFA98XX_KEY2_PROTECTED_MTP0_MTPEX_MSK);
	if (error) {
		pr_info("resetting MTPEX failed (%d)\n", error);
		tfa->reset_mtpex = 1; /* suspend until TFA98xx is active */
		return error;
	}

	/* set ACS/coldboot state */
	if (!tfa->is_probus_device) {
		/* put DSP in reset state */
		error = tfa98xx_dsp_reset(tfa, 1);

		/* force cold boot */
		error = tfa_run_coldboot(tfa, 1);
		if (error)
			pr_info("coldboot failed (%d)\n", error);
	}

	/* start tfa by playing */
	return error;
}

static short twos(short x)
{
	return (x < 0) ? x + 512 : x;
}

void tfa98xx_set_exttemp(struct tfa_device *tfa, short ext_temp)
{
	if ((-256 <= ext_temp) && (ext_temp <= 255)) {
		/* make twos complement */
		pr_debug("Using ext temp %d C\n", twos(ext_temp));
		TFA_SET_BF(tfa, TROS, 1);
		TFA_SET_BF(tfa, EXTTS, twos(ext_temp));
	} else {
		pr_debug("Clearing ext temp settings\n");
		TFA_SET_BF(tfa, TROS, 0);
	}
}

short tfa98xx_get_exttemp(struct tfa_device *tfa)
{
	short ext_temp = (short)TFA_GET_BF(tfa, EXTTS);

	return twos(ext_temp);
}

/************* tfa simple bitfield interfacing ********************/
/* convenience functions */
enum tfa98xx_error tfa98xx_set_volume_level(struct tfa_device *tfa,
	unsigned short vol)
{
	if (tfa->in_use == 0)
		return TFA98XX_ERROR_NOT_OPEN;

	if (vol > 255)	/* restricted to 8 bits */
		vol = 255;

	/* 0x00 -> 0.0 dB
	 * 0x01 -> -0.5 dB
	 * ...
	 * 0xfe -> -127dB
	 * 0xff -> muted
	 */

	/* volume value is in the top 8 bits of the register */
	return -TFA_SET_BF(tfa, VOL, (uint16_t)vol);
}

static enum tfa98xx_error
tfa98xx_set_mute_tfa2(struct tfa_device *tfa, enum tfa98xx_mute mute)
{
	enum tfa98xx_error error;

	if (tfa->dev_ops.set_mute == NULL)
		return TFA98XX_ERROR_NOT_SUPPORTED;

	switch (mute) {
	case TFA98XX_MUTE_OFF:
		error = tfa->dev_ops.set_mute(tfa, 0);
		TFA_SET_BF(tfa, AMPE, 1);
		break;
	case TFA98XX_MUTE_AMPLIFIER:
	case TFA98XX_MUTE_DIGITAL:
		error = tfa->dev_ops.set_mute(tfa, 1);
		TFA_SET_BF(tfa, AMPE, 0);
		break;
	default:
		return TFA98XX_ERROR_BAD_PARAMETER;
	}

	return error;
}

static enum tfa98xx_error
tfa98xx_set_mute_tfa1(struct tfa_device *tfa, enum tfa98xx_mute mute)
{
	enum tfa98xx_error error;
	unsigned short audioctrl_value;
	unsigned short sysctrl_value;
	int value;

	value = TFA_READ_REG(tfa, CFSM); /* audio control register */
	if (value < 0)
		return -value;
	audioctrl_value = (unsigned short)value;
	value = TFA_READ_REG(tfa, AMPE); /* system control register */
	if (value < 0)
		return -value;
	sysctrl_value = (unsigned short)value;

	switch (mute) {
	case TFA98XX_MUTE_OFF:
		/* previous state can be digital or amplifier mute,
		 * clear the cf_mute and set the enbl_amplifier bits
		 *
		 * To reduce PLOP at power on it is needed to switch the
		 * amplifier on with the DCDC in follower mode
		 * (enbl_boost = 0 ?).
		 * This workaround is also needed when toggling the
		 * powerdown bit!
		 */
		TFA_SET_BF_VALUE(tfa, CFSM, 0, &audioctrl_value);
		TFA_SET_BF_VALUE(tfa, AMPE, 1, &sysctrl_value);
		TFA_SET_BF_VALUE(tfa, DCA, 1, &sysctrl_value);
		break;
	case TFA98XX_MUTE_DIGITAL:
		/* expect the amplifier to run */
		/* set the cf_mute bit */
		TFA_SET_BF_VALUE(tfa, CFSM, 1, &audioctrl_value);
		/* set the enbl_amplifier bit */
		TFA_SET_BF_VALUE(tfa, AMPE, 1, &sysctrl_value);
		/* clear active mode */
		TFA_SET_BF_VALUE(tfa, DCA, 0, &sysctrl_value);
		break;
	case TFA98XX_MUTE_AMPLIFIER:
		/* clear the cf_mute bit */
		TFA_SET_BF_VALUE(tfa, CFSM, 0, &audioctrl_value);
		/* clear the enbl_amplifier bit and active mode */
		TFA_SET_BF_VALUE(tfa, AMPE, 0, &sysctrl_value);
		TFA_SET_BF_VALUE(tfa, DCA, 0, &sysctrl_value);
		break;
	default:
		return TFA98XX_ERROR_BAD_PARAMETER;
	}

	error = -TFA_WRITE_REG(tfa, CFSM, audioctrl_value);
	if (error)
		return error;
	error = -TFA_WRITE_REG(tfa, AMPE, sysctrl_value);
	return error;
}

enum tfa98xx_error
tfa98xx_set_mute(struct tfa_device *tfa, enum tfa98xx_mute mute)
{
	int cur_ampe;

	if (tfa->in_use == 0) {
		pr_err("device is not opened\n");
		return TFA98XX_ERROR_NOT_OPEN;
	}

	cur_ampe = TFA_GET_BF(tfa, AMPE);
	if ((mute == TFA98XX_MUTE_OFF && cur_ampe == 1)
		|| (mute == TFA98XX_MUTE_AMPLIFIER && cur_ampe == 0)) {
		pr_info("%s: skip mute request (%d, AMPE %d)\n",
			__func__, mute, cur_ampe);
		return TFA98XX_ERROR_OK;
	}

	if (tfa->tfa_family == 1)
		return tfa98xx_set_mute_tfa1(tfa, mute);
	else
		return tfa98xx_set_mute_tfa2(tfa, mute);
}

/****************** patching ******************/
static enum tfa98xx_error
tfa98xx_process_patch_file(struct tfa_device *tfa, int length,
	const unsigned char *bytes)
{
	unsigned short size;
	int index = 0;
	enum tfa98xx_error error = TFA98XX_ERROR_OK;

	while (index < length) {
		size = bytes[index] + bytes[index + 1] * 256;
		index += 2;
		if ((index + size) > length) {
			/* outside the buffer, error in the input data */
			return TFA98XX_ERROR_BAD_PARAMETER;
		}

		if (size > tfa->buffer_size) {
			/* too big, must fit buffer */
			return TFA98XX_ERROR_BAD_PARAMETER;
		}

		error = tfa98xx_write_raw(tfa, size, &bytes[index]);
		if (error != TFA98XX_ERROR_OK)
			break;
		index += size;
	}

	return error;
}

/* the patch contains a header with the following
 * IC revision register: 1 byte, 0xff means don't care
 * XMEM address to check: 2 bytes, big endian, 0xffff means don't care
 * XMEM value to expect: 3 bytes, big endian
 */
static enum tfa98xx_error
tfa98xx_check_ic_rom_version(struct tfa_device *tfa,
	const unsigned char patchheader[])
{
	enum tfa98xx_error error = TFA98XX_ERROR_OK;
	unsigned short checkrev, revid;
	unsigned char lsb_revid;
	unsigned short checkaddress;
	int checkvalue;
	int value = 0;
	int status;
	checkrev = patchheader[0];
	lsb_revid = tfa->rev & 0xff; /* only compare lower byte */

	if ((checkrev != 0xff) && (checkrev != lsb_revid))
		return TFA98XX_ERROR_NOT_SUPPORTED;

	checkaddress = (patchheader[1] << 8) + patchheader[2];
	checkvalue =
		(patchheader[3] << 16) + (patchheader[4] << 8) + patchheader[5];
	if (checkaddress != 0xffff) {
		/* before reading XMEM, check if we can access the DSP */
		error = tfa98xx_dsp_system_stable(tfa, &status);
		if (error == TFA98XX_ERROR_OK) {
			if (!status)
				/* DSP subsys not running */
				error = TFA98XX_ERROR_DSP_NOT_RUNNING;
		}
		/* read register to check the correct ROM version */
		if (error == TFA98XX_ERROR_OK)
			error = mem_read(tfa, checkaddress, 1, &value);
		if (error == TFA98XX_ERROR_OK) {
			if (value != checkvalue) {
				pr_err("patch file romid type check failed [0x%04x]: expected 0x%02x, actual 0x%02x\n",
					checkaddress, value, checkvalue);
				error = TFA98XX_ERROR_NOT_SUPPORTED;
			}
		}
	} else { /* == 0xffff */
		/* check if the revid subtype is in there */
		if (checkvalue != 0xffffff && checkvalue != 0) {
			revid = patchheader[5] << 8 | patchheader[0];
			/* full revid */
			if (revid != tfa->rev) {
				pr_err("patch file device type check failed: expected 0x%02x, actual 0x%02x\n",
					tfa->rev, revid);
				return TFA98XX_ERROR_NOT_SUPPORTED;
			}
		}
	}

	return error;
}

#define PATCH_HEADER_LENGTH 6
enum tfa98xx_error
tfa_dsp_patch(struct tfa_device *tfa, int patch_length,
	const unsigned char *patch_bytes)
{
	enum tfa98xx_error error;
	int status;

	if (tfa->in_use == 0)
		return TFA98XX_ERROR_NOT_OPEN;

	if (patch_length < PATCH_HEADER_LENGTH)
		return TFA98XX_ERROR_BAD_PARAMETER;

	error = tfa98xx_check_ic_rom_version(tfa, patch_bytes);
	if (TFA98XX_ERROR_OK != error)
		return error;

	tfa98xx_dsp_system_stable(tfa, &status);
	if (!status)
		/* Only test when we have a clock. */
		return TFA98XX_ERROR_NO_CLOCK;
	/******* TO_TEST *******/
	if (error == TFA98XX_ERROR_OK) {
		error = tfa_run_coldboot(tfa, 1);
		if (error)
			return TFA98XX_ERROR_DSP_NOT_RUNNING;
	}
	/***********************/
	error = tfa98xx_process_patch_file(tfa,
		patch_length - PATCH_HEADER_LENGTH,
		patch_bytes + PATCH_HEADER_LENGTH);

	return error;
}

/****************** end patching ******************/

TFA_INTERNAL enum tfa98xx_error
tfa98xx_wait_result(struct tfa_device *tfa, int wait_retry_count)
{
	enum tfa98xx_error error = TFA98XX_ERROR_OK;
	int cf_status; /* the contents of the CF_STATUS register */
	int tries = 0;

	do {
		/* i2c_cmd_ack */
		cf_status = TFA_GET_BF(tfa, ACK);
		if (cf_status < 0)
			error = -cf_status;
		tries++;
		/* don't wait forever, DSP is pretty quick to respond (< 1ms) */
	} while ((error == TFA98XX_ERROR_OK)
		&& ((cf_status & CF_STATUS_I2C_CMD_ACK) == 0)
		&& (tries < wait_retry_count));

	if (tries >= wait_retry_count)
		/* something wrong with communication with DSP */
		error = TFA98XX_ERROR_DSP_NOT_RUNNING;

	return error;
}

/*
 * support functions for data conversion
 * convert memory bytes to signed 24 bit integers
 *	input:  bytes contains "num_bytes" byte elements
 *	output: data contains "num_bytes/3" int24 elements
 */
void tfa98xx_convert_bytes2data(int num_bytes,
	const unsigned char bytes[], int data[])
{
	int i;			/* index for data */
	int k;			/* index for bytes */
	int d;
	int num_data = num_bytes / 3;

	_ASSERT((num_bytes % 3) == 0);

#if defined(TFA_CUSTOM_FORMAT_AT_RESPONSE)
	/* shift one data if reading contains status*/
	i = 1;
	k = 3;
#else
	i = 0;
	k = 0;
#endif /* TFA_CUSTOM_FORMAT_AT_RESPONSE */

	for (; i < num_data; ++i, k += 3) {
		d = (bytes[k] << 16) | (bytes[k + 1] << 8) | (bytes[k + 2]);
		_ASSERT(d >= 0);
		_ASSERT(d < (1 << 24));	/* max 24 bits in use */
		if (bytes[k] & 0x80)	/* sign bit was set */
			d = -((1 << 24) - d);

		data[i] = d;
	}
}

/*
 * convert signed 32 bit integers to 24 bit aligned bytes
 *	input:   data contains "num_data" int elements
 *	output:  bytes contains "3 * num_data" byte elements
 */
void tfa98xx_convert_data2bytes(int num_data, const int data[],
	unsigned char bytes[])
{
	int i; /* index for data */
	int k; /* index for bytes */
	int d;

	/* note: cannot just take the lowest 3 bytes from the 32 bit
	 * integer, because also need to take care of clipping any
	 * value > 2 & 23
	 */
	for (i = 0, k = 0; i < num_data; ++i, k += 3) {
		if (data[i] >= 0)
			d = MIN(data[i], (1 << 23) - 1);
		else {
			/* 2's complement */
			d = (1 << 24) - MIN(-data[i], 1 << 23);
		}
		_ASSERT(d >= 0);
		_ASSERT(d < (1 << 24));	/* max 24 bits in use */
		bytes[k] = (d >> 16) & 0xff;	/* MSB */
		bytes[k + 1] = (d >> 8) & 0xff;
		bytes[k + 2] = (d) & 0xff;	/* LSB */
	}
}

/*
 * DSP RPC message support functions
 * depending on framework to be up and running
 * need base i2c of memaccess (tfa1=0x70/tfa2=0x90)
 *
 * write dsp messages in function tfa_dsp_msg()
 * note the 'old' write_parameter() was more efficient
 * because all i2c was in one burst transaction
 */

/* TODO properly handle bitfields: state should be restored! */
/* (now it will change eg dmesg field to xmem) */
enum tfa98xx_error tfa_dsp_msg_write(struct tfa_device *tfa,
	int length, const char *buffer)
{
	int offset = 0;
	int chunk_size = ROUND_DOWN(tfa->buffer_size, 3);
	/* XMEM word size */
	int remaining_bytes = length;
	enum tfa98xx_error error = TFA98XX_ERROR_OK;
	uint16_t cfctl;
	int value;

	value = TFA_READ_REG(tfa, DMEM);
	if (value < 0) {
		error = -value;
		return error;
	}
	cfctl = (uint16_t)value;
	/* assume no I2C errors from here */

	TFA_SET_BF_VALUE(tfa, DMEM, (uint16_t)TFA98XX_DMEM_XMEM, &cfctl);
	/* set cf ctl to DMEM */
	TFA_SET_BF_VALUE(tfa, AIF, 0, &cfctl); /* set to autoincrement */
	TFA_WRITE_REG(tfa, DMEM, cfctl);

	/* xmem[1] is start of message
	 * direct write to register to save cycles avoiding read-modify-write
	 */
	TFA_WRITE_REG(tfa, MADD, 1);

	/* due to autoincrement in cf_ctrl, next write will happen at
	 * the next address
	 */
	while ((error == TFA98XX_ERROR_OK) && (remaining_bytes > 0)) {
		if (remaining_bytes < chunk_size)
			chunk_size = remaining_bytes;
		/* else chunk_size remains at initialize value above */
		error = tfa98xx_write_data(tfa, FAM_TFA98XX_CF_MEM,
			chunk_size, (const unsigned char *)buffer + offset);
		remaining_bytes -= chunk_size;
		offset += chunk_size;
	}

	/* notify the DSP */
	if (error == TFA98XX_ERROR_OK) {
		/* cf_int=0, cf_aif=0, cf_dmem=XMEM=01, cf_rst_dsp=0 */
		/* set the cf_req1 and cf_int bit */
		TFA_SET_BF_VALUE(tfa, REQCMD, 0x01, &cfctl); /* bit 0 */
		TFA_SET_BF_VALUE(tfa, CFINT, 1, &cfctl);
		error = -TFA_WRITE_REG(tfa, CFINT, cfctl);
	}

	return error;
}

enum tfa98xx_error tfa_dsp_msg_write_id(struct tfa_device *tfa,
	int length, const char *buffer, uint8_t cmdid[3])
{
	int offset = 0;
	int chunk_size = ROUND_DOWN(tfa->buffer_size, 3);
	/* XMEM word size */
	int remaining_bytes = length;
	enum tfa98xx_error error = TFA98XX_ERROR_OK;
	uint16_t cfctl;
	int value;

	value = TFA_READ_REG(tfa, DMEM);
	if (value < 0) {
		error = -value;
		return error;
	}
	cfctl = (uint16_t)value;
	/* assume no I2C errors from here */

	TFA_SET_BF_VALUE(tfa, DMEM, (uint16_t)TFA98XX_DMEM_XMEM, &cfctl);
	/* set cf ctl to DMEM */
	TFA_SET_BF_VALUE(tfa, AIF, 0, &cfctl); /* set to autoincrement */
	TFA_WRITE_REG(tfa, DMEM, cfctl);

	/* xmem[1] is start of message
	 * direct write to register to save cycles avoiding read-modify-write
	 */
	TFA_WRITE_REG(tfa, MADD, 1);

	/* write cmd-id */
	error = tfa98xx_write_data(tfa, FAM_TFA98XX_CF_MEM,
		3, (const unsigned char *)cmdid);

	/* due to autoincrement in cf_ctrl, next write will happen at
	 * the next address
	 */
	while ((error == TFA98XX_ERROR_OK) && (remaining_bytes > 0)) {
		if (remaining_bytes < chunk_size)
			chunk_size = remaining_bytes;
		/* else chunk_size remains at initialize value above */
		error = tfa98xx_write_data(tfa, FAM_TFA98XX_CF_MEM,
			chunk_size, (const unsigned char *)buffer + offset);
		remaining_bytes -= chunk_size;
		offset += chunk_size;
	}

	/* notify the DSP */
	if (error == TFA98XX_ERROR_OK) {
		/* cf_int=0, cf_aif=0, cf_dmem=XMEM=01, cf_rst_dsp=0 */
		/* set the cf_req1 and cf_int bit */
		TFA_SET_BF_VALUE(tfa, REQCMD, 0x01, &cfctl); /* bit 0 */
		TFA_SET_BF_VALUE(tfa, CFINT, 1, &cfctl);
		error = -TFA_WRITE_REG(tfa, CFINT, cfctl);
	}

	return error;
}

/*
 * status function used by tfa_dsp_msg() to retrieve command/msg status:
 * return a <0 status of the DSP did not ACK.
 */
enum tfa98xx_error tfa_dsp_msg_status(struct tfa_device *tfa, int *p_rpc_status)
{
	enum tfa98xx_error error = TFA98XX_ERROR_OK;

	error = tfa98xx_wait_result(tfa, 2); /* 2 is only one try */
	if (error == TFA98XX_ERROR_DSP_NOT_RUNNING) {
		*p_rpc_status = -1;
		return TFA98XX_ERROR_OK;
	} else if (error != TFA98XX_ERROR_OK) {
		return error;
	}

	error = tfa98xx_check_rpc_status(tfa, p_rpc_status);

	return error;
}

const char *tfa98xx_get_i2c_status_id_string(int status)
{
	const char *p_id_str;

	switch (status) {
	case TFA98XX_DSP_NOT_RUNNING:
		p_id_str = "No response from DSP";
		break;
	case TFA98XX_I2C_REQ_DONE:
		p_id_str = "Ok";
		break;
	case TFA98XX_I2C_REQ_BUSY:
		p_id_str = "Request is being processed";
		break;
	case TFA98XX_I2C_REQ_INVALID_M_ID:
		p_id_str =
			"Provided M-ID does not fit in valid range [0..2]";
		break;
	case TFA98XX_I2C_REQ_INVALID_P_ID:
		p_id_str =
			"Provided P-ID is not valid in the given M-ID context";
		break;
	case TFA98XX_I2C_REQ_INVALID_CC:
		p_id_str =
			"Invalid channel configuration bits (SC|DS|DP|DC) combination";
		break;
	case TFA98XX_I2C_REQ_INVALID_SEQ:
		p_id_str =
			"Invalid sequence of commands, in case DSP expects some commands in a specific order";
		break;
	case TFA98XX_I2C_REQ_INVALID_PARAM:
		p_id_str = "Generic error, invalid parameter";
		break;
	case TFA98XX_I2C_REQ_BUFFER_OVERFLOW:
		p_id_str =
			"I2C buffer overflowed: host sent too many parameters, memory integrity is not guaranteed";
		break;
	case TFA98XX_I2C_REQ_CALIB_BUSY:
		p_id_str = "Calibration not completed";
		break;
	case TFA98XX_I2C_REQ_CALIB_FAILED:
		p_id_str = "Calibration failed";
		break;
	default:
		p_id_str = "Unspecified error";
		break;
	}

	return p_id_str;
}

enum tfa98xx_error dsp_msg(struct tfa_device *tfa,
	int length24, const char *buf24)
{
	enum tfa98xx_error error = TFA98XX_ERROR_OK;
	static int lastmessage;
	uint8_t *blob = NULL;
	int i;
	int *intbuf = NULL;
	char *buf = (char *)buf24;
	int length = length24;
#if defined(TFADSP_DSP_BUFFER_POOL)
	int buf_p_index = -1;
#endif

	if ((tfa->stream_state & BIT_PSTREAM) == 0) {
		pr_info("%s: skip if PSTREAM is lost\n", __func__);
		return error;
	}

	if (tfa->verbose == 8) {
		pr_info("%s: skip dsp_msg(algorithm bypass) if verbose is 8\n",
			__func__);
		return error;
	}

	if (tfa->convert_dsp32) {
		int idx = 0;

		length = 4 * length24 / 3;
		intbuf = kmem_cache_alloc(tfa->cachep, GFP_KERNEL);
		buf = (char *)intbuf;

		/* convert 24 bit DSP messages to a 32 bit integer */
		for (i = 0; i < length24; i += 3) {
			int tmp = (buf24[i] << 16) + (buf24[i + 1] << 8) + buf24[i + 2];
			/* Sign extend to 32-bit from 24-bit */
			intbuf[idx++] = ((int32_t)tmp << 8) >> 8;
		}
	}

	/* Only create multi-msg when the dsp is cold */
	if (tfa->ext_dsp == 1) {
		/* Creating the multi-msg */
		error = tfa_tib_dsp_msgmulti(tfa, length, buf);
		if (error == TFA98XX_ERROR_FAIL)
			return TFA98XX_ERROR_FAIL;

		/* if the buffer is full we need to send the existing message and add the current message */
		if (error == TFA98XX_ERROR_BUFFER_TOO_SMALL) {
			int len;

			/* (a) send the existing (full) message */
#if defined(TFADSP_DSP_BUFFER_POOL)
			buf_p_index = tfa98xx_buffer_pool_access
				(-1, 64 * 1024, &blob, POOL_GET);
			if (buf_p_index != -1) {
				pr_debug("%s: allocated from buffer_pool[%d] for %d bytes\n",
					__func__, buf_p_index, length);
			} else {
				blob = kmalloc(64 * 1024, GFP_KERNEL); // max length is 64k
				if (blob == NULL)
					pr_err("%s: kmalloc error\n", __func__);
			}
#else
			blob = kmalloc(64 * 1024, GFP_KERNEL); // max length is 64k
#endif /* TFADSP_DSP_BUFFER_POOL */

			len = tfa_tib_dsp_msgmulti(tfa, -1, (const char *)blob);
			if (tfa->verbose)
				pr_debug("Multi-message buffer full. Sending multi-message, length=%d\n",
					len);

			/* Send to the target selected */
			if ((tfa->stream_state & BIT_PSTREAM) == 1) {
#if defined(TFADSP_DSP_MSG_PACKET_STRATEGY)
				error = dsp_msg_packet(tfa, blob, len);
#else
				if (tfa->has_msg == 0) { /* via i2c */
					/* Send to the target selected */
					if (tfa->dev_ops.dsp_msg)
						error = (tfa->dev_ops.dsp_msg)((void *)tfa,
							len, (const char *)blob);
				} else { /* via msg hal */
					error = tfa98xx_write_dsp((void *)tfa,
						len, (const char *)blob);
				}
#endif /* TFADSP_DSP_MSG_PACKET_STRATEGY */
			} else {
				pr_info("%s: skip if PSTREAM is lost\n", __func__);
			}

#if defined(TFADSP_DSP_BUFFER_POOL)
			if (buf_p_index != -1)
				buf_p_index = tfa98xx_buffer_pool_access(buf_p_index,
					0, &blob, POOL_RETURN);
			else
				kfree(blob);
#else
			kfree(blob);
#endif /* TFADSP_DSP_BUFFER_POOL */
			blob = NULL;

			/* (b) add the current DSP message to a new multi-message */
			error = tfa_tib_dsp_msgmulti(tfa, length, buf);
			if (error == TFA98XX_ERROR_FAIL)
				return TFA98XX_ERROR_FAIL;
		}

		lastmessage = error;

		/* When the lastmessage is done we can send the multi-msg to the target */
		if (lastmessage == 1) {
			/* Get the full multi-msg data */
#if defined(TFADSP_DSP_BUFFER_POOL)
			buf_p_index = tfa98xx_buffer_pool_access
				(-1, 64 * 1024, &blob, POOL_GET);
			if (buf_p_index != -1) {
				pr_debug("%s: allocated from buffer_pool[%d] for %d bytes\n",
					__func__, buf_p_index, length);
			} else {
				blob = kmalloc(64 * 1024, GFP_KERNEL); // max length is 64k
				if (blob == NULL)
					pr_err("%s: kmalloc error\n", __func__);
			}
#else
			blob = kmalloc(64 * 1024, GFP_KERNEL); // max length is 64k
#endif /* TFADSP_DSP_BUFFER_POOL */

			length = tfa_tib_dsp_msgmulti(tfa, -1, (const char *)blob);
			if (tfa->verbose)
				pr_debug("Last message for the multi-message received. Multi-message length=%d\n",
					length);

			/* Send to the target selected */
			if ((tfa->stream_state & BIT_PSTREAM) == 1) {
#if defined(TFADSP_DSP_MSG_PACKET_STRATEGY)
				error = dsp_msg_packet(tfa, blob, length);
#else
				if (tfa->has_msg == 0) { /* via i2c */
					if (tfa->dev_ops.dsp_msg)
						error = (tfa->dev_ops.dsp_msg)((void *)tfa,
							length, (const char *)blob);
				} else { /* via msg hal */
					error = tfa98xx_write_dsp((void *)tfa,
						length, (const char *)blob);
				}
#endif /* TFADSP_DSP_MSG_PACKET_STRATEGY */
			} else {
				pr_info("%s: skip if PSTREAM is lost\n", __func__);
			}

#if defined(TFADSP_DSP_BUFFER_POOL)
			if (buf_p_index != -1)
				buf_p_index = tfa98xx_buffer_pool_access(buf_p_index,
					0, &blob, POOL_RETURN);
			else
				kfree(blob);
#else
			kfree(blob);
#endif // TFADSP_DSP_BUFFER_POOL
			blob = NULL;

			lastmessage = 0; /* reset to be able to re-start */
		}
	} else {
		if ((tfa->stream_state & BIT_PSTREAM) == 1) {
			if (tfa->has_msg == 0) { /* via i2c */
				if (tfa->dev_ops.dsp_msg)
					error = (tfa->dev_ops.dsp_msg)((void *)tfa,
						length, (const char *)buf);
			} else { /* via msg hal */
				error = tfa98xx_write_dsp((void *)tfa,
					length, (const char *)buf);
			}
		} else {
			pr_info("%s: skip if PSTREAM is lost\n", __func__);
		}
	}

	if (error != TFA98XX_ERROR_OK)
		/* Get actual error code from softDSP */
		error = (enum tfa98xx_error)
			(error + TFA98XX_ERROR_BUFFER_RPC_BASE);

	/* DSP verbose has argument 0x04 */
	if ((tfa->verbose & 0x04) != 0) {
		pr_debug("DSP W [%d]: ", length);
		// for (i = 0; i < ((length > 3) ? 3 : length); i++)
		// 	pr_debug("0x%02x ", (uint8_t)buf[i]);
		// pr_debug("\n");
	}

	if (tfa->convert_dsp32) {
		kmem_cache_free(tfa->cachep, intbuf);
	}

	return error;
}

#if defined(TFADSP_DSP_MSG_PACKET_STRATEGY)
enum tfa98xx_error dsp_msg_packet(struct tfa_device *tfa,
	uint8_t *blob, int tfadsp_buf_size)
{
	enum tfa98xx_error error = 0;
	uint8_t *pkt_buff = NULL;
#if defined(TFADSP_DSP_BUFFER_POOL)
	int pkt_buff_p_index = -1;
#endif
	int loop = 0;
	int remaining_blob_size, tfadsp_buf_offset;
	int packet_id, packet_size;

	tfadsp_buf_offset = 0;
	remaining_blob_size = tfadsp_buf_size;
	packet_size = MAX_PKT_MSG_SIZE - 4;
#if defined(TFADSP_DSP_BUFFER_POOL)
	pkt_buff_p_index = tfa98xx_buffer_pool_access
		(-1, MAX_PKT_MSG_SIZE, &pkt_buff, POOL_GET);
	if (pkt_buff_p_index != -1) {
		pr_debug("%s: allocated from buffer_pool[%d] - pkt_buff\n",
			__func__, pkt_buff_p_index);
	} else {
		pkt_buff = kmalloc(MAX_PKT_MSG_SIZE, GFP_KERNEL);
		if (pkt_buff == NULL) {
			pr_err("%s: cannot allocate memory\n", __func__);
			error = TFA98XX_ERROR_FAIL;
			goto dsp_msg_packet_error_exit;
		}
	}
#else
	pkt_buff = kmalloc(MAX_PKT_MSG_SIZE, GFP_KERNEL);
	if (pkt_buff == NULL) {
		pr_err("%s: cannot allocate memory\n", __func__);
		error = TFA98XX_ERROR_FAIL;
		goto dsp_msg_packet_error_exit;
	}
#endif /* TFADSP_DSP_BUFFER_POOL */

	loop = (tfadsp_buf_size / packet_size)
		+ (tfadsp_buf_size % packet_size) ? 1 : 0;

	for (packet_id = 0; packet_id < loop; packet_id++) {
		if (packet_id < loop - 1) {
			pr_debug("packet[%d]: size (%d)\n",
				packet_id, packet_size);

			pkt_buff[0] = (uint8_t)(((packet_id + 1) >> 8) & 0xff);
			pkt_buff[1] = (uint8_t)((packet_id + 1) & 0xff);
		} else {
			packet_size = remaining_blob_size;
			pr_debug("packet[%d]: size (%d) - last\n",
				packet_id, packet_size);

			pkt_buff[0] = 0xff;
			pkt_buff[1] = 0xff;
		}

		pkt_buff[2] = (uint8_t)((packet_size >> 8) & 0xff);
		pkt_buff[3] = (uint8_t) (packet_size & 0xff);

		memcpy(pkt_buff + 4, blob + tfadsp_buf_offset, packet_size);

		if (tfa->has_msg == 0) { /* via i2c */
			/* Send to the target selected */
			if (tfa->dev_ops.dsp_msg)
				error = (tfa->dev_ops.dsp_msg)((void *)tfa,
					packet_size + 4, (const char *)pkt_buff);
		} else { /* via msg hal */
			error = tfa98xx_write_dsp((void *)tfa,
				packet_size + 4, (const char *)pkt_buff);
		}

		tfadsp_buf_offset += packet_size;
		remaining_blob_size -= packet_size;
	}

	pr_info("%s: sent %d packets: size (%d:%d)\n",
		__func__, packet_id,
		(packet_id - 1) * (MAX_PKT_MSG_SIZE - 4) + packet_size,
		tfadsp_buf_size);

dsp_msg_packet_error_exit:
#if defined(TFADSP_DSP_BUFFER_POOL)
	if (pkt_buff_p_index != -1)
		pkt_buff_p_index = tfa98xx_buffer_pool_access
			(pkt_buff_p_index, 0, &pkt_buff, POOL_RETURN);
	else
		kfree(pkt_buff);
#else
	kfree(pkt_buff);
#endif /* TFADSP_DSP_BUFFER_POOL */
	pkt_buff = NULL;

	return error;
}
#endif /* TFADSP_DSP_MSG_PACKET_STRATEGY */

enum tfa98xx_error dsp_msg_read(struct tfa_device *tfa,
	int length24, unsigned char *bytes24)
{
	enum tfa98xx_error error = TFA98XX_ERROR_OK;
	int i;
	int length = length24;
	unsigned char *bytes = bytes24;

	if (tfa->convert_dsp32) {
		length = 4 * length24 / 3;
		bytes = kmem_cache_alloc(tfa->cachep, GFP_KERNEL);
	}

	if ((tfa->stream_state & BIT_PSTREAM) == 1) {
		if (tfa->has_msg == 0) { /* via i2c */
			if (tfa->dev_ops.dsp_msg_read)
				error = (tfa->dev_ops.dsp_msg_read)((void *)tfa,
					length, bytes);
		} else { /* via msg hal */
			error = tfa98xx_read_dsp((void *)tfa, length, bytes);
		}
		if (error == TFA98XX_ERROR_OK)
			pr_debug("%s: OK\n", __func__);
	} else {
		pr_info("%s: skip if PSTREAM is lost\n", __func__);
	}

	if (error != TFA98XX_ERROR_OK)
		/* Get actual error code from softDSP */
		error = (enum tfa98xx_error)
			(error + TFA98XX_ERROR_BUFFER_RPC_BASE);

	/* DSP verbose has argument 0x04 */
	if ((tfa->verbose & 0x04) != 0) {
		pr_debug("DSP R [%d]: ", length);
		// for (i = 0; i < ((length > 3) ? 3 : length); i++)
		// 	pr_debug("0x%02x ", (uint8_t)bytes[i]);
		// pr_debug("\n");
	}

	if (tfa->convert_dsp32) {
		int idx = 0;

		/* convert 32 bit LE to 24 bit BE */
		for (i = 0; i < length; i += 4) {
			bytes24[idx++] = bytes[i + 2];
			bytes24[idx++] = bytes[i + 1];
			bytes24[idx++] = bytes[i + 0];
		}

		kmem_cache_free(tfa->cachep, bytes);
	}

	return error;
}

enum tfa98xx_error reg_read(struct tfa_device *tfa,
	unsigned char subaddress, unsigned short *value)
{
	enum tfa98xx_error error;

	error = (tfa->dev_ops.reg_read)(tfa, subaddress, value);
	if (error != TFA98XX_ERROR_OK)
		/* Get actual error code from softDSP */
		error = (enum tfa98xx_error)
			(error + TFA98XX_ERROR_BUFFER_RPC_BASE);

	return error;
}

enum tfa98xx_error reg_write(struct tfa_device *tfa,
	unsigned char subaddress, unsigned short value)
{
	enum tfa98xx_error error;

	error = (tfa->dev_ops.reg_write)(tfa, subaddress, value);
	if (error != TFA98XX_ERROR_OK)
		/* Get actual error code from softDSP */
		error = (enum tfa98xx_error)
			(error + TFA98XX_ERROR_BUFFER_RPC_BASE);

	return error;
}

enum tfa98xx_error mem_read(struct tfa_device *tfa,
	unsigned int start_offset, int num_words, int *p_values)
{
	enum tfa98xx_error error;

	error = (tfa->dev_ops.mem_read)(tfa, start_offset, num_words, p_values);
	if (error != TFA98XX_ERROR_OK)
		/* Get actual error code from softDSP */
		error = (enum tfa98xx_error)
			(error + TFA98XX_ERROR_BUFFER_RPC_BASE);

	return error;
}

enum tfa98xx_error mem_write(struct tfa_device *tfa,
	unsigned short address, int value, int memtype)
{
	enum tfa98xx_error error;

	error = (tfa->dev_ops.mem_write)(tfa, address, value, memtype);
	if (error != TFA98XX_ERROR_OK)
		/* Get actual error code from softDSP */
		error = (enum tfa98xx_error)
			(error + TFA98XX_ERROR_BUFFER_RPC_BASE);

	return error;
}

/*
 * write/read raw msg functions :
 * the buffer is provided in little endian format, each word
 * occupying 3 bytes, length is in bytes.
 * functions will return immediately and do not not wait for DSP response.
 */
#define MAX_WORDS (300)
int tfa_dsp_msg(void *data, int length, const char *buf)
{
	enum tfa98xx_error error;
	struct tfa_device *tfa = (struct tfa_device *)data;
	int tries, rpc_status = TFA98XX_I2C_REQ_DONE;

	/* write the message and notify the DSP */
	error = tfa_dsp_msg_write(tfa, length, buf);
	if (error != TFA98XX_ERROR_OK)
		return (int)error;

	/* get the result from the DSP (polling) */
	for (tries = TFA98XX_WAITRESULT_NTRIES; tries > 0; tries--) {
		error = tfa_dsp_msg_status(tfa, &rpc_status);
		if (error == TFA98XX_ERROR_OK
			&& rpc_status == TFA98XX_I2C_REQ_DONE)
			break;
		/* If the rpc status is a specific error we want to know it.
		 * If it is busy or not running it should retry
		 */
		if (rpc_status != TFA98XX_I2C_REQ_BUSY
			&& rpc_status != TFA98XX_DSP_NOT_RUNNING)
			break;
	}

	if (rpc_status != TFA98XX_I2C_REQ_DONE) {
		/* DSP RPC call returned an error */
		error = (enum tfa98xx_error)
			(rpc_status + TFA98XX_ERROR_BUFFER_RPC_BASE);
		pr_debug("DSP msg status: %d (%s)\n", rpc_status,
			tfa98xx_get_i2c_status_id_string(rpc_status));
	}
	return (int)error;
}

/*
 * Read a message from dsp
 */
int tfa_dsp_msg_read(void *data, int length, unsigned char *bytes)
{
	enum tfa98xx_error error = TFA98XX_ERROR_OK;
	struct tfa_device *tfa = (struct tfa_device *)data;
	int burst_size;		/* number of words per burst size */
	int bytes_per_word = 3;
	int num_bytes;
	int offset = 0;
	unsigned short start_offset = 2; /* msg starts @xmem[2], [1]=cmd */

	if (length > TFA2_MAX_PARAM_SIZE)
		return TFA98XX_ERROR_BAD_PARAMETER;

	TFA_SET_BF(tfa, DMEM, (uint16_t)TFA98XX_DMEM_XMEM);
	error = -TFA_WRITE_REG(tfa, MADD, start_offset);
	if (error != TFA98XX_ERROR_OK)
		return (int)error;

	num_bytes = length; /* input param */
	while (num_bytes > 0) {
		burst_size = ROUND_DOWN(tfa->buffer_size, bytes_per_word);
		if (num_bytes < burst_size)
			burst_size = num_bytes;
		error = tfa98xx_read_data(tfa, FAM_TFA98XX_CF_MEM,
			burst_size, bytes + offset);
		if (error != TFA98XX_ERROR_OK)
			return (int)error;

		num_bytes -= burst_size;
		offset += burst_size;
	}

	return (int)error;
}

/*
 * write/read raw msg functions:
 * the buffer is provided in little endian format, each word
 * occupying 3 bytes, length is in bytes.
 * functions will return immediately and do not not wait for DSP ressponse.
 * An ID is added to modify the command-ID
 */
enum tfa98xx_error tfa_dsp_msg_id(struct tfa_device *tfa,
	int length, const char *buf, uint8_t cmdid[3])
{
	enum tfa98xx_error error;
	int tries, rpc_status = TFA98XX_I2C_REQ_DONE;

	/* write the message and notify the DSP */
	error = tfa_dsp_msg_write_id(tfa, length, buf, cmdid);
	if (error != TFA98XX_ERROR_OK)
		return error;

	/* get the result from the DSP (polling) */
	for (tries = TFA98XX_WAITRESULT_NTRIES; tries > 0; tries--) {
		error = tfa_dsp_msg_status(tfa, &rpc_status);
		if (error == TFA98XX_ERROR_OK
			&& rpc_status == TFA98XX_I2C_REQ_DONE)
			break;
	}

	if (rpc_status != TFA98XX_I2C_REQ_DONE) {
		/* DSP RPC call returned an error */
		error = (enum tfa98xx_error)
			(rpc_status + TFA98XX_ERROR_BUFFER_RPC_BASE);
		pr_debug("DSP msg status: %d (%s)\n", rpc_status,
			tfa98xx_get_i2c_status_id_string(rpc_status));
	}
	return error;
}

/* read the return code for the RPC call */
TFA_INTERNAL enum tfa98xx_error
tfa98xx_check_rpc_status(struct tfa_device *tfa, int *p_rpc_status)
{
	enum tfa98xx_error error = TFA98XX_ERROR_OK;
	/* the value to sent to the * CF_CONTROLS register: cf_req=00000000,
	 * cf_int=0, cf_aif=0, cf_dmem=XMEM=01, cf_rst_dsp=0 */
	unsigned short cf_ctrl = 0x0002;
	/* memory address to be accessed (0: Status, 1: ID, 2: parameters) */
	unsigned short cf_mad = 0x0000;

	if (tfa->in_use == 0)
		return TFA98XX_ERROR_NOT_OPEN;
	if (p_rpc_status == NULL)
		return TFA98XX_ERROR_BAD_PARAMETER;

	/* 1) write DMEM=XMEM to the DSP XMEM */
	{
		/* minimize the number of I2C transactions
		 * by making use of the autoincrement in I2C
		 */
		unsigned char buffer[4];
		/* first the data for CF_CONTROLS */
		buffer[0] = (unsigned char)((cf_ctrl >> 8) & 0xff);
		buffer[1] = (unsigned char)(cf_ctrl & 0xff);
		/* write the contents of CF_MAD which is the subaddress
		 * following CF_CONTROLS
		 */
		buffer[2] = (unsigned char)((cf_mad >> 8) & 0xff);
		buffer[3] = (unsigned char)(cf_mad & 0xff);
		error = tfa98xx_write_data(tfa,
			FAM_TFA98XX_CF_CONTROLS, sizeof(buffer), buffer);
	}

	if (error == TFA98XX_ERROR_OK)
		/* read 1 word (24 bit) from XMEM */
		error = tfa98xx_dsp_read_mem(tfa, 0, 1, p_rpc_status);

	return error;
}

/***************************** xmem only **********************************/
enum tfa98xx_error
tfa98xx_dsp_read_mem(struct tfa_device *tfa,
	unsigned int start_offset, int num_words, int *p_values)
{
	enum tfa98xx_error error = TFA98XX_ERROR_OK;
	unsigned char *bytes;
	int burst_size;		/* number of words per burst size */
	const int bytes_per_word = 3;
	int dmem;
	int num_bytes;
	int *p;

	bytes = (unsigned char *)kmem_cache_alloc(tfa->cachep, GFP_KERNEL);
	if (bytes == NULL)
		return TFA98XX_ERROR_FAIL;

	/* If no offset is given, assume XMEM! */
	if (((start_offset >> 16) & 0xf) > 0)
		dmem = (start_offset >> 16) & 0xf;
	else
		dmem = TFA98XX_DMEM_XMEM;

	/* Remove offset from address */
	start_offset = start_offset & 0xffff;
	num_bytes = num_words * bytes_per_word;
	p = p_values;

	TFA_SET_BF(tfa, DMEM, (uint16_t)dmem);
	error = -TFA_WRITE_REG(tfa, MADD, (unsigned short)start_offset);
	if (error != TFA98XX_ERROR_OK)
		goto tfa98xx_dsp_read_mem_exit;

	for (; num_bytes > 0;) {
		burst_size = ROUND_DOWN(tfa->buffer_size, bytes_per_word);
		if (num_bytes < burst_size)
			burst_size = num_bytes;

		_ASSERT(burst_size <= sizeof(bytes));
		error = tfa98xx_read_data(tfa, FAM_TFA98XX_CF_MEM,
			burst_size, bytes);
		if (error != TFA98XX_ERROR_OK)
			goto tfa98xx_dsp_read_mem_exit;

		tfa98xx_convert_bytes2data(burst_size, bytes, p);

		num_bytes -= burst_size;
		p += burst_size / bytes_per_word;
	}

tfa98xx_dsp_read_mem_exit:
	kmem_cache_free(tfa->cachep, bytes);

	return error;
}

enum tfa98xx_error
tfa98xx_dsp_write_mem_word(struct tfa_device *tfa,
	unsigned short address, int value, int memtype)
{
	enum tfa98xx_error error = TFA98XX_ERROR_OK;
	unsigned char bytes[3];

	TFA_SET_BF(tfa, DMEM, (uint16_t)memtype);

	error = -TFA_WRITE_REG(tfa, MADD, address);
	if (error != TFA98XX_ERROR_OK)
		return error;

	tfa98xx_convert_data2bytes(1, &value, bytes);
	error = tfa98xx_write_data(tfa, FAM_TFA98XX_CF_MEM, 3, bytes);

	return error;
}

enum tfa98xx_error
tfa_cont_write_filterbank(struct tfa_device *tfa, struct tfa_filter *filter)
{
	unsigned char biquad_index;
	enum tfa98xx_error error = TFA98XX_ERROR_OK;

	for (biquad_index = 0; biquad_index < 10; biquad_index++) {
		if (filter[biquad_index].enabled)
			error = tfa_dsp_cmd_id_write(tfa,
				MODULE_BIQUADFILTERBANK,
				biquad_index + 1, /* start @1 */
				sizeof(filter[biquad_index].biquad.bytes),
				filter[biquad_index].biquad.bytes);
		else
			error = tfa98xx_dsp_biquad_disable(tfa,
				biquad_index + 1);
		if (error)
			return error;
	}

	return error;
}

enum tfa98xx_error
tfa98xx_dsp_biquad_disable(struct tfa_device *tfa, int biquad_index)
{
	enum tfa98xx_error error = TFA98XX_ERROR_OK;
	int coeff_buffer[BIQUAD_COEFF_SIZE];
	unsigned char bytes[3 + BIQUAD_COEFF_SIZE * 3];
	int nr = 0;

	if (biquad_index > TFA98XX_BIQUAD_NUM)
		return TFA98XX_ERROR_BAD_PARAMETER;
	if (biquad_index < 1)
		return TFA98XX_ERROR_BAD_PARAMETER;

	/* make opcode */
	bytes[nr++] = 0;
	bytes[nr++] = MODULE_BIQUADFILTERBANK + 0x80;
	bytes[nr++] = (unsigned char)biquad_index;

	/* set in correct order and format for the DSP */
	coeff_buffer[0] = (int)-8388608;	/* -1.0f */
	coeff_buffer[1] = 0;
	coeff_buffer[2] = 0;
	coeff_buffer[3] = 0;
	coeff_buffer[4] = 0;
	coeff_buffer[5] = 0;

	/* convert to packed 24 */
	tfa98xx_convert_data2bytes(BIQUAD_COEFF_SIZE, coeff_buffer, &bytes[nr]);
	nr += BIQUAD_COEFF_SIZE * 3;

	error = dsp_msg(tfa, nr, (char *)bytes);

	return error;
}

/* wrapper for dsp_msg that adds opcode */
enum tfa98xx_error tfa_dsp_cmd_id_write(struct tfa_device *tfa,
	unsigned char module_id,
	unsigned char param_id, int num_bytes,
	const unsigned char data[])
{
	enum tfa98xx_error error;
	unsigned char *buffer;
	int nr = 0;

	buffer = kmem_cache_alloc(tfa->cachep, GFP_KERNEL);
	if (buffer == NULL)
		return TFA98XX_ERROR_FAIL;

	buffer[nr++] = tfa->spkr_select;
	buffer[nr++] = module_id + 0x80;
	buffer[nr++] = param_id;

	memcpy(&buffer[nr], data, num_bytes);
	nr += num_bytes;

	error = dsp_msg(tfa, nr, (char *)buffer);

	kmem_cache_free(tfa->cachep, buffer);

	return error;
}

/* wrapper for dsp_msg that adds opcode */
/* this is as the former tfa98xx_dsp_get_param() */
enum tfa98xx_error tfa_dsp_cmd_id_write_read(struct tfa_device *tfa,
	unsigned char module_id,
	unsigned char param_id, int num_bytes,
	unsigned char data[])
{
	enum tfa98xx_error error;
	unsigned char buffer[3];
	int nr = 0;

	if (num_bytes <= 0) {
		pr_debug("Error: The number of READ bytes is smaller or equal to 0!\n");
		return TFA98XX_ERROR_FAIL;
	}

	if ((tfa->is_probus_device) && (devcount == 1)
		&& (param_id == SB_PARAM_GET_RE25C
		|| param_id == SB_PARAM_GET_LSMODEL
		|| param_id == SB_PARAM_GET_ALGO_PARAMS)) {
		/* Modifying the ID for GetRe25C */
		pr_debug("%s: CC bit: 4 (DS) for mono\n", __func__);
		buffer[nr++] = 4; /* CC: 4 (DS) for mono */
	} else {
		pr_debug("%s: CC bit: %d\n", __func__, tfa->spkr_select);
		buffer[nr++] = tfa->spkr_select; /* CC: 0 (reset all) for stereo */
	}

	buffer[nr++] = module_id + 0x80;
	buffer[nr++] = param_id;

	error = dsp_msg(tfa, nr, (char *)buffer);
	if (error != TFA98XX_ERROR_OK)
		return error;

	/* read the data from the dsp */
	error = dsp_msg_read(tfa, num_bytes, data);

	return error;
}

/* wrapper for dsp_msg that adds opcode and 3 bytes required for coefs */
enum tfa98xx_error tfa_dsp_cmd_id_coefs(struct tfa_device *tfa,
	unsigned char module_id,
	unsigned char param_id, int num_bytes,
	unsigned char data[])
{
	enum tfa98xx_error error;
	unsigned char buffer[2 * 3];
	int nr = 0;

	buffer[nr++] = tfa->spkr_select;
	buffer[nr++] = module_id + 0x80;
	buffer[nr++] = param_id;

	buffer[nr++] = 0;
	buffer[nr++] = 0;
	buffer[nr++] = 0;

	error = dsp_msg(tfa, nr, (char *)buffer);
	if (error != TFA98XX_ERROR_OK)
		return error;

	/* read the data from the dsp */
	error = dsp_msg_read(tfa, num_bytes, data);

	return error;
}

/* wrapper for dsp_msg that adds opcode and 3 bytes required for MBDrcDynamics */
enum tfa98xx_error tfa_dsp_cmd_id_mbdrc_dynamics(struct tfa_device *tfa,
	unsigned char module_id,
	unsigned char param_id, int index_subband,
	int num_bytes, unsigned char data[])
{
	enum tfa98xx_error error;
	unsigned char buffer[2 * 3];
	int nr = 0;

	buffer[nr++] = tfa->spkr_select;
	buffer[nr++] = module_id + 0x80;
	buffer[nr++] = param_id;

	buffer[nr++] = 0;
	buffer[nr++] = 0;
	buffer[nr++] = (unsigned char)index_subband;

	error = dsp_msg(tfa, nr, (char *)buffer);
	if (error != TFA98XX_ERROR_OK)
		return error;

	/* read the data from the dsp */
	error = dsp_msg_read(tfa, num_bytes, data);

	return error;
}

enum tfa98xx_error
tfa98xx_dsp_write_preset(struct tfa_device *tfa, int length,
	const unsigned char *p_preset_bytes)
{
	enum tfa98xx_error error = TFA98XX_ERROR_OK;

	if (p_preset_bytes != NULL)
		/* by design: keep the data opaque and no
		 * interpreting/calculation
		 */
		error = tfa_dsp_cmd_id_write(tfa, MODULE_SPEAKERBOOST,
			SB_PARAM_SET_PRESET, length,
			p_preset_bytes);
	else
		error = TFA98XX_ERROR_BAD_PARAMETER;

	return error;
}

/*
 * get features from MTP
 */
enum tfa98xx_error
tfa98xx_dsp_get_hw_feature_bits(struct tfa_device *tfa, int *features)
{
	enum tfa98xx_error error = TFA98XX_ERROR_OK;
	uint32_t value;
	uint16_t mtpbf;

	/* return the cache data if it's valid */
	if (tfa->hw_feature_bits != -1) {
		*features = tfa->hw_feature_bits;
	} else {
		/* for tfa1 check if we have clock */
		if (tfa->tfa_family == 1) {
			int status;

			tfa98xx_dsp_system_stable(tfa, &status);
			if (!status) {
				get_hw_features_from_cnt(tfa, features);
				/* skip reading MTP: */
				return (*features == -1)
					? TFA98XX_ERROR_FAIL : TFA98XX_ERROR_OK;
			}

			mtpbf = 0x850f; /* MTP5 for tfa1,16 bits */
		} else {
			mtpbf = 0xf907; /* MTP9 for tfa2, 8 bits */
		}

		value = tfa_read_reg(tfa, mtpbf) & 0xffff;
		*features = tfa->hw_feature_bits = value;
	}

	return error;
}

enum tfa98xx_error
tfa98xx_dsp_get_sw_feature_bits(struct tfa_device *tfa, int features[2])
{
	enum tfa98xx_error error = TFA98XX_ERROR_OK;
	const int byte_size = 2 * 3;
	unsigned char bytes[2 * 3];

	/* return the cache data if it's valid */
	if (tfa->sw_feature_bits[0] != -1) {
		features[0] = tfa->sw_feature_bits[0];
		features[1] = tfa->sw_feature_bits[1];
	} else {
		/* for tfa1 check if we have clock */
		if (tfa->tfa_family == 1) {
			int status;

			tfa98xx_dsp_system_stable(tfa, &status);
			if (!status) {
				get_sw_features_from_cnt(tfa, features);
				/* skip reading MTP: */
				return (features[0] == -1)
					? TFA98XX_ERROR_FAIL : TFA98XX_ERROR_OK;
			}
		}

		error = tfa_dsp_cmd_id_write_read(tfa, MODULE_FRAMEWORK,
			FW_PAR_ID_GET_FEATURE_INFO, byte_size, bytes);

		if (error != TFA98XX_ERROR_OK)
			/* old ROM code may respond
			 * with TFA98XX_ERROR_RPC_PARAM_ID
			 */
			return error;

		tfa98xx_convert_bytes2data(byte_size, bytes, features);
	}

	return error;
}

enum tfa98xx_error
tfa98xx_dsp_get_state_info(struct tfa_device *tfa,
	unsigned char bytes[], unsigned int *statesize)
{
	enum tfa98xx_error err = TFA98XX_ERROR_OK;
	int b_support_framework = 0;
	unsigned int state_size = 9;

	err = tfa98xx_dsp_support_framework(tfa, &b_support_framework);
	if (err == TFA98XX_ERROR_OK) {
		if (b_support_framework) {
			err = tfa_dsp_cmd_id_write_read(tfa,
				MODULE_FRAMEWORK,
				FW_PARAM_GET_STATE, 3 * state_size, bytes);
		} else {
			/* old ROM code, ask SpeakerBoost and
			 * only do first portion
			 */
			state_size = 8;
			err = tfa_dsp_cmd_id_write_read(tfa,
				MODULE_SPEAKERBOOST,
				SB_PARAM_GET_STATE, 3 * state_size, bytes);
		}
	}

	*statesize = state_size;

	return err;
}

enum tfa98xx_error
tfa98xx_dsp_support_drc(struct tfa_device *tfa, int *pb_support_drc)
{
	enum tfa98xx_error error = TFA98XX_ERROR_OK;

	*pb_support_drc = 0;

	if (tfa->in_use == 0)
		return TFA98XX_ERROR_NOT_OPEN;

	if (tfa->support_drc != SUPPORT_NOT_SET) {
		*pb_support_drc = (tfa->support_drc == SUPPORT_YES);
	} else {
		int feature_bits[2];

		error = tfa98xx_dsp_get_sw_feature_bits(tfa, feature_bits);
		if (error == TFA98XX_ERROR_OK) {
			/* easy case: new API available */
			/* bit=0 means DRC enabled */
			*pb_support_drc
				= (feature_bits[0] & FEATURE1_DRC) == 0;
		} else if (error == TFA98XX_ERROR_RPC_PARAM_ID) {
			/* older ROM code, doesn't support it */
			*pb_support_drc = 0;
			error = TFA98XX_ERROR_OK;
		}
		/* else some other error, return transparently */
		/* pb_support_drc only changed when error == TFA98XX_ERROR_OK */

		if (error == TFA98XX_ERROR_OK)
			tfa->support_drc = *pb_support_drc
				? SUPPORT_YES : SUPPORT_NO;
	}

	return error;
}

enum tfa98xx_error
tfa98xx_dsp_support_framework(struct tfa_device *tfa,
	int *pb_support_framework)
{
	int feature_bits[2] = {0, 0};
	enum tfa98xx_error error = TFA98XX_ERROR_OK;

	_ASSERT(pb_support_framework != 0);

	if (tfa->in_use == 0)
		return TFA98XX_ERROR_NOT_OPEN;

	if (tfa->support_framework != SUPPORT_NOT_SET) {
		if (tfa->support_framework == SUPPORT_NO)
			*pb_support_framework = 0;
		else
			*pb_support_framework = 1;
	} else {
		error = tfa98xx_dsp_get_sw_feature_bits(tfa, feature_bits);
		if (error == TFA98XX_ERROR_OK) {
			*pb_support_framework = 1;
			tfa->support_framework = SUPPORT_YES;
		} else {
			*pb_support_framework = 0;
			tfa->support_framework = SUPPORT_NO;
			error = TFA98XX_ERROR_OK;
		}
	}

	/* *pb_support_framework only changed when error == TFA98XX_ERROR_OK */
	return error;
}

enum tfa98xx_error
tfa98xx_dsp_write_speaker_parameters(struct tfa_device *tfa,
	int length, const unsigned char *p_speaker_bytes)
{
	enum tfa98xx_error error;
	int b_support_drc;

	if (p_speaker_bytes != NULL)
		/* by design: keep the data opaque and no
		 * interpreting/calculation
		 * Use long WaitResult retry count
		 */
		error = tfa_dsp_cmd_id_write(tfa,
			MODULE_SPEAKERBOOST,
			SB_PARAM_SET_LSMODEL, length,
			p_speaker_bytes);
	else
		error = TFA98XX_ERROR_BAD_PARAMETER;

	if (error != TFA98XX_ERROR_OK)
		return error;

	error = tfa98xx_dsp_support_drc(tfa, &b_support_drc);
	if (error != TFA98XX_ERROR_OK)
		return error;

	if (b_support_drc) {
		/* Need to set AgcGainInsert back to PRE,
		 * as the SetConfig forces it to POST
		 */
		uint8_t bytes[3] = {0, 0, 0};

		error = tfa_dsp_cmd_id_write(tfa,
			MODULE_SPEAKERBOOST,
			SB_PARAM_SET_AGCINS,
			3,
			bytes);
	}

	return error;
}

enum tfa98xx_error
tfa98xx_dsp_write_config(struct tfa_device *tfa, int length,
	const unsigned char *p_config_bytes)
{
	enum tfa98xx_error error = TFA98XX_ERROR_OK;
	int b_support_drc;

	error = tfa_dsp_cmd_id_write(tfa,
		MODULE_SPEAKERBOOST,
		SB_PARAM_SET_CONFIG, length,
		p_config_bytes);
	if (error != TFA98XX_ERROR_OK)
		return error;

	error = tfa98xx_dsp_support_drc(tfa, &b_support_drc);
	if (error != TFA98XX_ERROR_OK)
		return error;

	if (b_support_drc) {
		/* Need to set AgcGainInsert back to PRE,
		 * as the SetConfig forces it to POST
		 */
		uint8_t bytes[3] = {0, 0, 0};

		error = tfa_dsp_cmd_id_write(tfa,
			MODULE_SPEAKERBOOST,
			SB_PARAM_SET_AGCINS,
			3,
			bytes);
	}

	return error;
}

/* load all the parameters for the DRC settings from a file */
enum tfa98xx_error tfa98xx_dsp_write_drc(struct tfa_device *tfa,
	int length, const unsigned char *p_drc_bytes)
{
	enum tfa98xx_error error = TFA98XX_ERROR_OK;

	if (p_drc_bytes != NULL)
		error = tfa_dsp_cmd_id_write(tfa,
			MODULE_SPEAKERBOOST,
			SB_PARAM_SET_DRC, length,
			p_drc_bytes);
	else
		error = TFA98XX_ERROR_BAD_PARAMETER;

	return error;
}

enum tfa98xx_error tfa98xx_powerdown(struct tfa_device *tfa, int powerdown)
{
	enum tfa98xx_error error = TFA98XX_ERROR_OK;

	if (tfa->in_use == 0)
		return TFA98XX_ERROR_NOT_OPEN;

	error = TFA_SET_BF(tfa, PWDN, (uint16_t)powerdown);

	if (powerdown) {
		/* Workaround for ticket PLMA5337 */
		if (tfa->tfa_family == 2)
			TFA_SET_BF_VOLATILE(tfa, AMPE, 0);
	}

	return error;
}

enum tfa98xx_error
tfa98xx_select_mode(struct tfa_device *tfa, enum tfa98xx_mode mode)
{
	enum tfa98xx_error error = TFA98XX_ERROR_OK;

	if (tfa->in_use == 0)
		return TFA98XX_ERROR_NOT_OPEN;

	if (error == TFA98XX_ERROR_OK) {
		switch (mode) {
		default:
			error = TFA98XX_ERROR_BAD_PARAMETER;
			break;
		}
	}

	return error;
}

int tfa_set_bf(struct tfa_device *tfa,
	const uint16_t bf, const uint16_t value)
{
	enum tfa98xx_error err;
	uint16_t regvalue, msk, oldvalue;

	/*
	 * bitfield enum:
	 * - 0..3  : len
	 * - 4..7  : pos
	 * - 8..15 : address
	 */
	uint8_t len = bf & 0x0f;
	uint8_t pos = (bf >> 4) & 0x0f;
	uint8_t address = (bf >> 8) & 0xff;

	err = reg_read(tfa, address, &regvalue);
	if (err) {
		pr_err("Error getting bf :%d\n", -err);
		return -err;
	}

	oldvalue = regvalue;
	msk = ((1 << (len + 1)) - 1) << pos;
	regvalue &= ~msk;
	regvalue |= value << pos;

	/* Only write when the current register value is
	 * not the same as the new value
	 */
	if (oldvalue != regvalue) {
		err = reg_write(tfa, address, regvalue);
		if (err) {
			pr_err("Error setting bf :%d\n", -err);
			return -err;
		}
	}

	return 0;
}

int tfa_set_bf_volatile(struct tfa_device *tfa,
	const uint16_t bf, const uint16_t value)
{
	enum tfa98xx_error err;
	uint16_t regvalue, msk;

	/*
	 * bitfield enum:
	 * - 0..3  : len
	 * - 4..7  : pos
	 * - 8..15 : address
	 */
	uint8_t len = bf & 0x0f;
	uint8_t pos = (bf >> 4) & 0x0f;
	uint8_t address = (bf >> 8) & 0xff;

	err = reg_read(tfa, address, &regvalue);
	if (err) {
		pr_err("Error getting bf :%d\n", -err);
		return -err;
	}

	msk = ((1 << (len + 1)) - 1) << pos;
	regvalue &= ~msk;
	regvalue |= value << pos;

	err = reg_write(tfa, address, regvalue);
	if (err) {
		pr_err("Error setting bf :%d\n", -err);
		return -err;
	}

	return 0;
}

int tfa_get_bf(struct tfa_device *tfa, const uint16_t bf)
{
	enum tfa98xx_error err;
	uint16_t regvalue, msk;
	uint16_t value;

	/*
	 * bitfield enum:
	 * - 0..3  : len
	 * - 4..7  : pos
	 * - 8..15 : address
	 */
	uint8_t len = bf & 0x0f;
	uint8_t pos = (bf >> 4) & 0x0f;
	uint8_t address = (bf >> 8) & 0xff;

	err = reg_read(tfa, address, &regvalue);
	if (err) {
		pr_err("Error getting bf :%d\n", -err);
		return -err;
	}

	msk = ((1 << (len + 1)) - 1) << pos;
	regvalue &= msk;
	value = regvalue >> pos;

	return value;
}

int tfa_set_bf_value(const uint16_t bf,
	const uint16_t bf_value, uint16_t *p_reg_value)
{
	uint16_t regvalue, msk;

	/*
	 * bitfield enum:
	 * - 0..3  : len
	 * - 4..7  : pos
	 * - 8..15 : address
	 */
	uint8_t len = bf & 0x0f;
	uint8_t pos = (bf >> 4) & 0x0f;

	regvalue = *p_reg_value;

	msk = ((1 << (len + 1)) - 1) << pos;
	regvalue &= ~msk;
	regvalue |= bf_value << pos;

	*p_reg_value = regvalue;

	return 0;
}

uint16_t tfa_get_bf_value(const uint16_t bf, const uint16_t reg_value)
{
	uint16_t msk, value;

	/*
	 * bitfield enum:
	 * - 0..3  : len
	 * - 4..7  : pos
	 * - 8..15 : address
	 */
	uint8_t len = bf & 0x0f;
	uint8_t pos = (bf >> 4) & 0x0f;

	msk = ((1 << (len + 1)) - 1) << pos;
	value = (reg_value & msk) >> pos;

	return value;
}


int tfa_write_reg(struct tfa_device *tfa,
	const uint16_t bf, const uint16_t reg_value)
{
	enum tfa98xx_error err;

	/* bitfield enum - 8..15 : address */
	uint8_t address = (bf >> 8) & 0xff;

	err = reg_write(tfa, address, reg_value);
	if (err)
		return -err;

	return 0;
}

int tfa_read_reg(struct tfa_device *tfa, const uint16_t bf)
{
	enum tfa98xx_error err;
	uint16_t regvalue;

	/* bitfield enum - 8..15 : address */
	uint8_t address = (bf >> 8) & 0xff;

	err = reg_read(tfa, address, &regvalue);
	if (err)
		return -err;

	return regvalue;
}

/*
 * powerup the coolflux subsystem and wait for it
 */
enum tfa98xx_error tfa_cf_powerup(struct tfa_device *tfa)
{
	enum tfa98xx_error err = TFA98XX_ERROR_OK;
	int tries, status;

	/* power on the sub system */
	TFA_SET_BF_VOLATILE(tfa, PWDN, 0);

	// wait until everything is stable, in case clock has been off
	if (tfa->verbose)
		pr_info("Waiting for DSP system stable...\n");

	for (tries = CFSTABLE_TRIES; tries > 0; tries--) {
		err = tfa98xx_dsp_system_stable(tfa, &status);
		_ASSERT(err == TFA98XX_ERROR_OK);
		if (status)
			break;

		/* wait 10ms to avoid busload */
		msleep_interruptible(BUSLOAD_INTERVAL);
	}

	if (tries == 0) { // timedout
		pr_err("DSP subsystem start timed out\n");
		return TFA98XX_ERROR_STATE_TIMED_OUT;
	}

	return err;
}

/*
 * Enable/Disable the I2S output for TFA1 devices
 * without TDM interface
 */
static enum tfa98xx_error
tfa98xx_aec_output(struct tfa_device *tfa, int enable)
{
	enum tfa98xx_error err = TFA98XX_ERROR_OK;

	if ((tfa->daimap & TFA98XX_DAI_TDM) == TFA98XX_DAI_TDM)
		/* No action for TDM interface; only for I2S */
		return err;

	if (tfa->tfa_family == 1)
		err = -tfa_set_bf(tfa, TFA1_BF_I2SDOE, (enable != 0));
	else {
		pr_err("I2SDOE on unsupported family\n");
		err = TFA98XX_ERROR_NOT_SUPPORTED;
	}

	return err;
}

/*
 * Print the current state of the hardware manager
 * Device manager status information, man_state from TFA9888_N1B_I2C_regmap_V12
 */
int is_94_N2_device(struct tfa_device *tfa)
{
	return ((((tfa->rev) & 0xff) == 0x94)
		&& (((tfa->rev >> 8) & 0xff) > 0x1a));
}

enum tfa98xx_error show_current_state(struct tfa_device *tfa)
{
	enum tfa98xx_error err = TFA98XX_ERROR_OK;
	int manstate = -1;

	if (tfa->tfa_family == 2 && tfa->verbose) {
		if (is_94_N2_device(tfa))
			manstate = tfa_get_bf(tfa, TFA9894N2_BF_MANSTATE);
		else
			manstate = TFA_GET_BF(tfa, MANSTATE);
		if (manstate < 0)
			return -manstate;

		pr_debug("tfa (dev %d): current HW manager state: %d\n",
			tfa->dev_idx, manstate);

		switch (manstate) {
		case 0:
			pr_debug("power_down_state\n");
			break;
		case 1:
			pr_debug("wait_for_source_settings_state\n");
			break;
		case 2:
			pr_debug("connnect_pll_input_state\n");
			break;
		case 3:
			pr_debug("disconnect_pll_input_state\n");
			break;
		case 4:
			pr_debug("enable_pll_state\n");
			break;
		case 5:
			pr_debug("enable_cgu_state\n");
			break;
		case 6:
			pr_debug("init_cf_state\n");
			break;
		case 7:
			pr_debug("enable_amplifier_state\n");
			break;
		case 8:
			pr_debug("alarm_state\n");
			break;
		case 9:
			pr_debug("operating_state\n");
			break;
		case 10:
			pr_debug("mute_audio_state\n");
			break;
		case 11:
			pr_debug("disable_cgu_pll_state\n");
			break;
		default:
			pr_debug("unable to find current state\n");
			break;
		}
	}

	return err;
}

#define VERSION_BIG_M_FILTER 0xff0000
#define VERSION_BIG_M_INDEX 16
#define VERSION_SMALL_M_FILTER 0x00ff00
#define VERSION_SMALL_M_INDEX 8
#define VERSION_BIG_U_FILTER 0x0000c0
#define VERSION_BIG_U_INDEX 6
#define VERSION_SMALL_U_FILTER 0x00003f
#define VERSION_SMALL_U_INDEX 0

enum tfa98xx_error tfa_get_fw_api_version(struct tfa_device *tfa,
	unsigned char *pfw_version)
{
	enum tfa98xx_error err = 0;
	int res_len = 3;
#if defined(TFA_CUSTOM_FORMAT_AT_RESPONSE)
	unsigned char buf[2 * 3] = {0};
#else
	unsigned char buf[3] = {0};
#endif /* TFA_CUSTOM_FORMAT_AT_RESPONSE */
	int data;

	if (tfa == NULL)
		return TFA98XX_ERROR_BAD_PARAMETER;

	if (!tfa->is_probus_device) {
		err = mem_read(tfa, FW_VAR_API_VERSION,
			1, (int *)buf);
		if (err) {
			pr_debug("%s Error: Unable to get API Version from DSP\n",
				__func__);
			return err;
		}
	} else {
		/* Read the API Value */
#if defined(TFA_CUSTOM_FORMAT_AT_RESPONSE)
		res_len = 2 * 3;
#else
		res_len = 3;
#endif /* TFA_CUSTOM_FORMAT_AT_RESPONSE */
		err = tfa_dsp_cmd_id_write_read(tfa,
			MODULE_FRAMEWORK,
			FW_PAR_ID_GET_API_VERSION,
			res_len, buf);
		if (err != 0) {
			pr_err("%s: failed to read value\n",
				__func__);
			return err;
		}
	}

	tfa98xx_convert_bytes2data(res_len, buf, &data);
#if defined(TFA_CUSTOM_FORMAT_AT_RESPONSE)
	memcpy(pfw_version, buf + 3, 3);
#else
	memcpy(pfw_version, buf, 3);
#endif /* TFA_CUSTOM_FORMAT_AT_RESPONSE */

	pr_info("%s: fw api (itf) version %d.%d.%d.%d\n",
		__func__,
		(data & VERSION_BIG_M_FILTER) >> VERSION_BIG_M_INDEX,
		(data & VERSION_SMALL_M_FILTER) >> VERSION_SMALL_M_INDEX,
		(data & VERSION_BIG_U_FILTER) >> VERSION_BIG_U_INDEX,
		(data & VERSION_SMALL_U_FILTER) >> VERSION_SMALL_U_INDEX);

	return err;
}

#define VERSION_STRING_LENGTH 20
#define VERSION_WORD "tfadsp"

enum tfa98xx_error tfa_get_fw_lib_version(struct tfa_device *tfa,
	unsigned char *plib_version)
{
	enum tfa98xx_error err = 0;
	int res_len;
	unsigned char buf[VERSION_STRING_LENGTH * 3] = {'\0'};
	char *version_word = VERSION_WORD;
	int i, j = 0, k = 0;
	int num = 0;

	if (tfa == NULL)
		return TFA98XX_ERROR_BAD_PARAMETER;

	/*
	 * need to set the address to read library version
	 * if (!tfa->is_probus_device) {
	 * 	err = mem_read(tfa, FW_VAR_LIB_VERSION,
	 * 		VERSION_STRING_LENGTH, (int *)buf);
	 * 	if (err) {
	 * 		pr_debug("%s Error: Unable to get LIB Version from DSP\n",
	 * 			__func__);
	 * 		return err;
	 * 	}
	 * } else {
	 */
	{
		/* Read the LIB version string */
		res_len = VERSION_STRING_LENGTH * 3;
		err = tfa_dsp_cmd_id_write_read(tfa,
			MODULE_FRAMEWORK,
			FW_PAR_ID_GET_LIBRARY_VERSION,
			res_len, buf);
		if (err != 0) {
			pr_err("%s: failed to read value\n",
				__func__);
			return err;
		}
	}

	for (i = 0; i < VERSION_STRING_LENGTH; i++)
	{
		char token = buf[i * 3 + 2];

		if (version_word[j] == token) {
			j++;
			continue;
		}
		if ((j < strlen(version_word) - 1)
			|| !((token >= '0' && token <= '9')
			|| token == '.'
			|| (token == 0 && num > 0)))
			continue;

		if (token == '.' || token == 0) {
			plib_version[k++] = num;
			num = 0;
			continue;
		}
		num = num * 10 + (token - '0');
	}

	if (k != 3) {
		pr_err("%s: invalid format for library version (%d entities)\n",
			__func__, k);
		return TFA98XX_ERROR_BAD_PARAMETER;
	}

	pr_info("%s: %s library version %d.%d.%d\n",
		__func__, version_word,
		plib_version[0], plib_version[1], plib_version[2]);

	return err;
}

/*
 * Write calibration values for probus / ext_dsp, to feed RE25C to algorithm
 */
enum tfa98xx_error tfa_set_calibration_values(struct tfa_device *tfa)
{
	enum tfa98xx_error err = TFA98XX_ERROR_OK;
	unsigned char bytes[2 * 3] = {0};
	unsigned short value = 0, channel = 0;
	int active_pstream = 0;
	int mtpex = 0;
	static int need_cal, is_bypass, is_damaged;
	int i;
	char reg_state[50] = {0};
#if defined(CHECK_CALIBRATION_DATA_RANGE)
	enum tfa_error ret = tfa_error_ok;
#endif

	active_pstream = tfa98xx_count_active_stream(BIT_PSTREAM);
	if (active_pstream <= 1) {
		pr_info("%s: reset config_count (%d) - active stream %d\n",
			__func__, config_count, active_pstream);
		config_count = 0;
		dsp_cal_value[0] = dsp_cal_value[1] = -1;
	}

	 /* at initial device only: to reset need_cal and is_bypass */
	if (tfa_nr_config_done(tfa) == 0) {
		need_cal = 0;
		is_bypass = 0;
		is_damaged = 0;

		for (i = 0; i < devcount; i++) {
			struct tfa_device *ntfa
				= tfa98xx_get_tfa_device_from_index(i);

			if (ntfa == NULL)
				continue;
#if defined(TFA_USE_DEVICE_SPECIFIC_CONTROL)
			if ((ntfa->active_handle != -1)
				&& (ntfa->active_handle != i))
				continue;
#endif

			mtpex = tfa_dev_mtp_get(ntfa, TFA_MTP_EX);
			need_cal |= (mtpex == 0) ? 1 : 0;
			is_bypass |= ntfa->is_bypass;
			is_damaged |= ntfa->spkr_damaged;
		}

		pr_debug("%s: device %s calibrated; session %s; speaker %s\n",
			__func__,
			(need_cal) ? "needs to be" : "is already",
			(is_bypass) ? "runs in bypass" : "needs configuration",
			(is_damaged) ? "is damaged": "has no problem");
	}

	config_count++;

	if (is_bypass
		|| (is_damaged && tfa->is_configured < 0)) {
		pr_info("%s: [%d] skip sending calibration data: bypass %d, damaged %d\n",
			__func__, tfa->dev_idx, is_bypass, is_damaged);

		/* CHECK: only once after buffering fully */
		/* at last device only: to reset and flush buffer */
		if (tfa_nr_config_done(tfa) == activecount) {
			config_count = 0;

			if (tfa->ext_dsp == 1) {
				pr_info("%s: flush buffer in blob, in bypass\n",
					__func__);
				err = tfa_tib_dsp_msgmulti(tfa, -2, NULL);
			}

			goto set_calibration_values_exit;
		}

		return err;
	}

	mtpex = tfa_dev_mtp_get(tfa, TFA_MTP_EX);

	pr_info("%s: dev %d, MTPEX=%d\n", __func__, tfa->dev_idx, mtpex);

	channel = tfa98xx_get_cnt_bitfield(tfa, TFA7x_FAM(tfa, TDMSPKS));
	value = tfa_dev_mtp_get(tfa, TFA_MTP_RE25);
	pr_info("%s: extract from MTP - %d mOhms\n", __func__, value);

	if (channel >= 0 && channel < devcount) {
		if (dsp_cal_value[channel] != -1) {
			config_count--; /* discount config_count if duplicated */
			pr_debug("%s: channel %d - duplicated (%d - %d mOhm), skip counting config_count\n",
				__func__, channel, value,
				TFA_ReZ_FP_INT(dsp_cal_value[channel],
				TFA_FW_ReZ_SHIFT) * 1000
				+ TFA_ReZ_FP_FRAC(dsp_cal_value[channel],
				TFA_FW_ReZ_SHIFT));
		}

		dsp_cal_value[channel] = TFA_ReZ_CALC(value, TFA_FW_ReZ_SHIFT);
#if defined(CHECK_CALIBRATION_DATA_RANGE)
		if (mtpex) {
			err = tfa_calibration_range_check(tfa, channel, value);
			if (err) {
				need_cal |= 1;
				err = TFA98XX_ERROR_OK;
				pr_info("%s: run calibration because of out-of-range\n",
					__func__);

				/* reset MTPEX to force calibration */
				ret = tfa_dev_mtp_set(tfa, TFA_MTP_EX, 0);
				if (ret != tfa_error_ok) {
					pr_err("%s: resetting MPTEX failed, device %d err (%d)\n",
						__func__, tfa->dev_idx, ret);
					tfa->reset_mtpex = 1;
				}
			}
		}
#endif
		if (value == 0) /* run equivalent with calibration */
			need_cal |= 1;

		pr_info("%s: dev %d, channel %d - calibration data: %d [%s]\n",
			__func__, tfa->dev_idx, channel, value,
			(channel == 0) ? "Primary" : "Secondary");
	}
	pr_info("%s: config_count=%d\n", __func__, config_count);

#if defined(SET_DUMMY_CALIBRATION_VALUE)
	if (need_cal == 1 && tfa->verbose != 5 && tfa->verbose != 7) { // [temp]
		value = 7000; /* nominal calibration data */
		dsp_cal_value[channel] = TFA_ReZ_CALC(value, TFA_FW_ReZ_SHIFT);
		need_cal = 0;
		pr_info("%s: set dummy value to void calibration by force - for test\n",
			__func__);
	}
#endif

	if (tfa->verbose) {
		/* check the configuration for cal profile */
		snprintf(reg_state, 50, "DCA %d", TFA_GET_BF(tfa, DCA));
		switch (tfa->rev & 0xff) {
#if defined(USE_TFA9878)
		case 0x78:
			snprintf(reg_state, 50, "%s, IPM %d", reg_state,
				TFA7x_GET_BF(tfa, IPM));
			// FALL-THROUGH
#endif /* USE_TFA9878 */
		case 0x74:
		case 0x72:
			snprintf(reg_state, 50, "%s, LPM1MODE %d", reg_state,
				TFA7x_GET_BF(tfa, LPM1MODE));
			snprintf(reg_state, 50, "%s, LNMODE %d", reg_state,
				TFA7x_GET_BF(tfa, LNMODE));
			break;
		default:
			/* neither TFA987x */
			break;
		}
		pr_debug("%s: %s\n", __func__, reg_state);
	}

#if defined(SET_CALIBRATION_AT_ALL_DEVICE_READY)
	if (need_cal == 1) {
		int tfa_state = tfa_dev_get_state(tfa);

		if (tfa_state != TFA_STATE_OPERATING) {
			pr_debug("%s: [%d] device is not ready though calibration is required\n",
				__func__, tfa->dev_idx);
			config_count--; /* revert counter, expecting retrial */
			dsp_cal_value[channel] = -1;
			return err;
		}
	}

	/* trigger at the last device */
	if (tfa_nr_config_done(tfa) < activecount) {
		pr_debug("%s: suspend setting calibration data till all device is enabled\n",
			__func__);
		return err;
	}
#endif /* SET_CALIBRATION_AT_ALL_DEVICE_READY */

	pr_info("%s: devcount=%d, activecount=%d\n",
		__func__, devcount, activecount);

	/* If calibration is set to once we load from MTP, else send zero's */
	if (need_cal == 0) {
		pr_info("%s: last dev %d - MTPEX=1\n", __func__, tfa->dev_idx);
		if (devcount == 1) /* mono */
			dsp_cal_value[1] = dsp_cal_value[0];
#if defined(TFA_USE_DEVICE_SPECIFIC_CONTROL)
		else if (devcount == 2) { /* stereo */
			switch (tfa->active_handle) {
			case 0:
				pr_info("%s: copy cal from dev 0 to dev 1\n", __func__);
				dsp_cal_value[1] = dsp_cal_value[0];
				break;
			case 1:
				pr_info("%s: copy cal from dev 1 to dev 0\n", __func__);
				dsp_cal_value[0] = dsp_cal_value[1];
				break;
			case -1:
				/* individually configured */
			default:
				/* wrong handle */
				break;
			}
		}
#endif
		else {
			pr_err("%s: more than 2 devices were selected (devcount %d)\n",
				__func__, devcount);
		}

		/* We have to copy it for both channels. Even when MONO! */
		bytes[0] = (uint8_t) ((dsp_cal_value[0] >> 16) & 0xffff);
		bytes[1] = (uint8_t) ((dsp_cal_value[0] >> 8) & 0xff);
		bytes[2] = (uint8_t) (dsp_cal_value[0] & 0xff);

		bytes[3] = (uint8_t) ((dsp_cal_value[1] >> 16) & 0xffff);
		bytes[4] = (uint8_t) ((dsp_cal_value[1] >> 8) & 0xff);
		bytes[5] = (uint8_t) (dsp_cal_value[1] & 0xff);

#if defined(TFA_BLACKBOX_LOGGING)
		if (tfa->blackbox_enable) {
			/* set logging once before configuring */
			pr_info("%s: set blackbox logging\n", __func__);
			tfa_configure_log(tfa->blackbox_enable);
		}
#endif
	} else { /* calibration is required */
		pr_info("%s: config ResetRe25C to do calibration\n", __func__);
		bytes[0] = 0;
		bytes[1] = 0;
		bytes[2] = 0;
		bytes[3] = 0;
		bytes[4] = 0;
		bytes[5] = 0;

		/* force UNMUTE state before calibration, when INIT_CF done */
		for (i = 0; i < devcount; i++) {
			struct tfa_device *ntfa
				= tfa98xx_get_tfa_device_from_index(i);

			if (ntfa == NULL)
				continue;

			ntfa->spkr_damaged = 0; /* reset before calibration */

			pr_debug("%s: [%d] force UNMUTE before calibration\n",
				__func__, ntfa->dev_idx);
			tfa_dev_set_state(ntfa, TFA_STATE_UNMUTE, 1);
		}
	}

	config_count = 0;
	dsp_cal_value[0] = dsp_cal_value[1] = -1;

	err = tfa_dsp_cmd_id_write
		(tfa, MODULE_SPEAKERBOOST, SB_PARAM_SET_RE25C,
		sizeof(bytes), bytes);
	if (err != TFA98XX_ERROR_OK)
		goto set_calibration_values_exit;

#if defined(WRITE_CALIBRATION_DATA_TO_MTP)
	if (need_cal == 0)
		goto set_calibration_values_exit;

#if !defined(MPLATFORM)
	err = tfa_wait_cal(tfa);
#else
	pr_info("%s: [%d] queue post-process to calibration\n",
		__func__, tfa->dev_idx);
	queue_delayed_work(tfa->tfacal_wq, &tfa->wait_cal_work, 0);
#endif
#endif /* WRITE_CALIBRATION_DATA_TO_MTP */

set_calibration_values_exit:
	dsp_cal_value[0] = dsp_cal_value[1] = -1;

#if defined(TFA_USE_WAITQUEUE_SEQ)
	for (i = 0; i < devcount; i++) {
		struct tfa_device *ntfa
			= tfa98xx_get_tfa_device_from_index(i);

		if (ntfa == NULL)
			continue;

		wake_up_interruptible(&ntfa->waitq_seq);
	}
#endif

	return err;
}


/*
 * Call tfa_set_calibration_values at once with loop
 */
enum tfa98xx_error tfa_set_calibration_values_once(struct tfa_device *tfa)
{
	enum tfa98xx_error err = TFA98XX_ERROR_OK;
	int i;

	if (tfa_nr_device_set(tfa) == 1
		|| tfa_nr_config_done(tfa) > 0) { /* first device or stopped */
		pr_info("%s: tfa_set_calibration_values\n", __func__);

		for (i = 0; i < tfa->cnt->ndev; i++) {
			struct tfa_device *ntfa
				= tfa98xx_get_tfa_device_from_index(i);

			if (ntfa == NULL)
				continue;
#if defined(TFA_USE_DEVICE_SPECIFIC_CONTROL)
			if ((ntfa->active_handle != -1)
				&& (ntfa->active_handle != i))
				continue;
#endif

			ntfa->is_bypass = tfa->is_bypass;

			err = tfa_set_calibration_values(ntfa);
			if (err)
				pr_err("%s: dev %d, set calibration values error = %d\n",
					__func__, i, err);
		}
	}

	return err;
}

/*
 * start the speakerboost algorithm
 * this implies a full system startup when the system was not already started
 */
enum tfa98xx_error tfa_run_speaker_boost(struct tfa_device *tfa,
	int force, int profile)
{
	enum tfa98xx_error err = TFA98XX_ERROR_OK;
	int value;

	if (force) {
		tfa->is_cold = 1;
		err = tfa_run_coldstartup(tfa, profile);
		if (err)
			return err;
	}

	/* Returns 1 when device is "cold" and 0 when device is warm */
	value = tfa_is_cold(tfa);
	if (value < 0)
		err = value;
	else
		tfa->is_cold = value;

	pr_info("%s: %s boot, ext_dsp = %d, profile = %d\n",
		__func__, value ? "cold" : "warm",
		tfa->ext_dsp, profile);

	pr_debug("Startup of device [%s] is a %sstart\n",
		tfa_cont_device_name(tfa->cnt, tfa->dev_idx),
		value ? "cold" : "warm");

	/* CHECK: only once before buffering */
	/* at initial device only: to flush buffer */
	if (tfa_nr_device_set(tfa) == 1) {
		/* flush message buffer */
		pr_debug("%s: flush buffer in blob, in cold start\n",
			__func__);
		err = tfa_tib_dsp_msgmulti(tfa, -2, NULL);
	}

	/* cold start */
	if (value) {
		/* Run startup and write all files */
		pr_info("%s: cold start, speaker startup\n", __func__);
		err = tfa_run_speaker_startup(tfa, force, profile);
		if (err) {
			pr_info("%s: dev %d, speaker startup error = %d\n",
				__func__, tfa->dev_idx, err);
			return err;
		}

		/* Save the current profile and set the vstep to 0 */
		/* This needs to be overwritten even in CF bypass */
		tfa_dev_set_swprof(tfa, (unsigned short)profile);
		tfa_dev_set_swvstep(tfa, 0);

#if defined(TFADSP_CONFIGURE_AT_FIRST_DEVICE)
		/* always send the SetRe25 message
		 * to indicate all messages are sent
		 */
		if (tfa->ext_dsp)
			err = tfa_set_calibration_values_once(tfa);
#endif /* TFADSP_CONFIGURE_AT_FIRST_DEVICE */
	}

	/* Synchonize I/V delay on 96/97 at cold start */
	if ((tfa->tfa_family == 1)
		&& (tfa->daimap == TFA98XX_DAI_TDM))
		tfa->sync_iv_delay = 1;

#if !defined(TFADSP_CONFIGURE_AT_FIRST_DEVICE)
	/* cold start */
	/* always send the SetRe25 message
	 * to indicate all messages are sent
	 */
	if (value && tfa->ext_dsp) {
		pr_info("%s: [%d] tfa_set_calibration_values\n",
			__func__, tfa->dev_idx);

		err = tfa_set_calibration_values(tfa);
		if (err)
			pr_err("%s: set calibration values error = %d\n",
				__func__, err);
	}
#endif /* TFADSP_CONFIGURE_AT_FIRST_DEVICE */

	return err;
}

enum tfa98xx_error
tfa_run_speaker_startup(struct tfa_device *tfa, int force, int profile)
{
	enum tfa98xx_error err = TFA98XX_ERROR_OK;
	enum tfa_error ret = tfa_error_ok;
	int i;
	struct tfa_device *tfa0
		= tfa98xx_get_tfa_device_from_index(0);

	pr_debug("coldstart%s :", force? " (forced)" : "");

	if (!force) { /* in case of force CF already runnning */
		err = tfa_run_startup(tfa, profile);
		PRINT_ASSERT(err);
		if (err) {
			pr_info("%s: tfa_run_startup error = %d\n",
				__func__, err);
			return err;
		}

		/* Startup with CF in bypass then return here */
		if (tfa_cf_enabled(tfa) == 0)
			return err;

		/* respond to external DSP: -1:none, 0:no_dsp, 1:cold, 2:warm */
		if (tfa->ext_dsp == -1) {
			err = tfa_run_start_dsp(tfa);
			if (err)
				return err;
		}
	}

	if (tfa->reset_mtpex) { /* reset MTPEX, if suspended */
		tfa->reset_mtpex = 0;
		ret = tfa_dev_mtp_set(tfa, TFA_MTP_EX, 0);
		if (ret)
			pr_err("%s: resetting MTPEX failed (%d)",
				__func__, ret);
		ret = tfa_dev_mtp_set(tfa, TFA_MTP_RE25, 0);
		if (ret)
			pr_err("%s: resetting MTP data failed (%d)",
				__func__, ret);
	}

	/* Set auto_copy_mtp_to_iic (bit 5 of 0xa3) to 1 */
	tfa98xx_auto_copy_mtp_to_iic(tfa);

	/* at initial device only: write files only once */
	if (tfa_nr_device_set(tfa) > 1
		|| tfa_nr_config_done(tfa) > 0)
		return err;

#if !defined(TFA_VOID_APIV_IN_FILE)
	if (tfa->fw_itf_ver[0] == 0xff) {
		err = tfa_get_fw_api_version(tfa,
			(unsigned char *)&tfa->fw_itf_ver[0]);
		if (err) {
			pr_debug("[%s] cannot get FWAPI error = %d\n",
				__func__, err);
			// return err;
			err = TFA98XX_ERROR_OK;
		} else {
			for (i = 0; i < devcount; i++) {
				struct tfa_device *ntfa
					= tfa98xx_get_tfa_device_from_index(i);

				if (ntfa == NULL)
					continue;
				if (ntfa->dev_idx == tfa->dev_idx)
					continue;

				memcpy(&ntfa->fw_itf_ver[0],
					&tfa->fw_itf_ver[0], 4);
			}
		}
	} else {
		pr_debug("%s: checked - itf v%d.%d.%d.%d\n",
			__func__,
			tfa->fw_itf_ver[0],
			tfa->fw_itf_ver[1],
			tfa->fw_itf_ver[2],
			tfa->fw_itf_ver[3]);
	}
#endif

	if (tfa->fw_lib_ver[0] == 0xff) {
		err = tfa_get_fw_lib_version(tfa,
			(unsigned char *)&tfa->fw_lib_ver[0]);
		if (err) {
			pr_debug("[%s] cannot get FWLIB error = %d\n",
				__func__, err);
			// return err;
			err = TFA98XX_ERROR_OK;
		} else {
			for (i = 0; i < devcount; i++) {
				struct tfa_device *ntfa
					= tfa98xx_get_tfa_device_from_index(i);

				if (ntfa == NULL)
					continue;
				if (ntfa->dev_idx == tfa->dev_idx)
					continue;

				memcpy(&ntfa->fw_lib_ver[0],
					&tfa->fw_lib_ver[0], 3);
			}
		}
	} else {
		pr_debug("%s: checked - library v%d.%d.%d\n",
			__func__,
			tfa->fw_lib_ver[0],
			tfa->fw_lib_ver[1],
			tfa->fw_lib_ver[2]);
	}

	/* DSP is running now */
	/* write all the files from the device list */
	if (tfa0 != NULL) {
		pr_info("%s: load dev files from master device\n",
			__func__);
		/* CHECK: loading master device */
		err = tfa_cont_write_files(tfa0);
	} else {
		pr_info("%s: load dev files from device %d (tfa0 == NULL)\n",
			__func__, tfa->dev_idx);
		err = tfa_cont_write_files(tfa);
	}
	if (err) {
		pr_debug("[%s] tfa_cont_write_files error = %d\n",
			__func__, err);
		return err;
	}

	tfa->is_bypass = 1; /* reset before start */
	/* skip sending messages in bypass */
	// if (TFA_GET_BF(tfa, MTPEX) == 0) /* need calibration */
	// 	tfa->is_bypass = 0; /* forced for calibration */

	/* write all the files from the profile list (use volumstep 0) */
	pr_info("%s: load prof files (device %d, profile %d)\n",
		__func__, tfa->dev_idx, profile);
	err = tfa_cont_write_files_prof(tfa, profile, 0);
	if (err) {
		pr_debug("[%s] tfa_cont_write_files_prof error = %d\n",
			__func__, err);
		return err;
	}

	return err;
}

/*
 * Run calibration
 */
enum tfa98xx_error
tfa_run_speaker_calibration(struct tfa_device *tfa)
{
	enum tfa98xx_error err = TFA98XX_ERROR_OK;
	int calibrate_done;

	/* return if there is no audio running */
	if ((tfa->tfa_family == 2) && TFA_GET_BF(tfa, NOCLK))
		return TFA98XX_ERROR_NO_CLOCK;

	/* When MTPOTC is set (cal=once) unlock key2 */
	if (TFA_GET_BF(tfa, MTPOTC) == 1)
		tfa98xx_key2(tfa, 0);

	/* await calibration, this should return ok */
	err = tfa_run_wait_calibration(tfa, &calibrate_done);
#if defined(WRITE_CALIBRATION_DATA_PARTLY)
	if (!tfa->spkr_damaged)
#else
	if (err == TFA98XX_ERROR_OK)
#endif /* WRITE_CALIBRATION_DATA_PARTLY */
	{
		err = tfa_dsp_get_calibration_impedance(tfa);
		PRINT_ASSERT(err);
	}

	/* When MTPOTC is set (cal=once) re-lock key2 */
	if (TFA_GET_BF(tfa, MTPOTC) == 1)
		tfa98xx_key2(tfa, 1);

	return err;
}

enum tfa98xx_error tfa_run_coldboot(struct tfa_device *tfa, int state)
{
#define CF_CONTROL 0x8100
	enum tfa98xx_error err = TFA98XX_ERROR_OK;
	int tries = 10;

	/* repeat set ACS bit until set as requested */
	while (state != TFA_GET_BF(tfa, ACS)) {
		/* set colstarted in CF_CONTROL to force ACS */
		err = mem_write(tfa, CF_CONTROL, state, TFA98XX_DMEM_IOMEM);
		PRINT_ASSERT(err);

		if (tries-- == 0) {
			pr_debug("coldboot (ACS) did not %s\n",
				state ? "set" : "clear");
			return TFA98XX_ERROR_OTHER;
		}
	}

	return err;
}

/*
 * load the patch if any
 * else tell no loaded
 */
static enum tfa98xx_error tfa_run_load_patch(struct tfa_device *tfa)
{
	return tfa_cont_write_patch(tfa);
}

/*
 * this will load the patch witch will implicitly start the DSP
 * if no patch is available the DSP is started immediately
 */
enum tfa98xx_error tfa_run_start_dsp(struct tfa_device *tfa)
{
	enum tfa98xx_error err = TFA98XX_ERROR_OK;

	err = tfa_run_load_patch(tfa);
	if (err) { /* patch load is fatal so return immediately */
		pr_err("%s: failed to load patch\n", __func__);
		return err;
	}

	/* Clear count_boot, should be reset to 0
	 * before DSP reset is released
	 */
	err = mem_write(tfa, 512, 0, TFA98XX_DMEM_XMEM);
	PRINT_ASSERT(err);

	/* Reset DSP once for sure after initializing */
	if (err == TFA98XX_ERROR_OK) {
		err = tfa98xx_dsp_reset(tfa, 0);
		/* in pair of tfa98xx_init() - tfa_run_startup() */
		PRINT_ASSERT(err);
	}

	/* Sample rate is needed to set the correct tables */
	err = tfa98xx_dsp_write_tables(tfa, TFA_GET_BF(tfa, AUDFS));
	PRINT_ASSERT(err);

	return err;
}

/*
 * start the clocks and wait until the AMP is switching
 * on return the DSP sub system will be ready for loading
 */
enum tfa98xx_error tfa_run_startup(struct tfa_device *tfa, int profile)
{
	enum tfa98xx_error err = TFA98XX_ERROR_OK;
	struct tfa_device_list *dev = tfa_cont_device(tfa->cnt, tfa->dev_idx);
	int i, noinit = 0, audfs = 0, fractdel = 0;
	char prof_name[MAX_CONTROL_NAME] = {0};
#if defined(REDUCED_REGISTER_SETTING)
	int is_cold_amp;
#endif
	int tfa_state;

	if (dev == NULL)
		return TFA98XX_ERROR_FAIL;

	if (dev->bus) /* no i2c device, do nothing */
		return TFA98XX_ERROR_OK;

#if defined(REDUCED_REGISTER_SETTING)
	is_cold_amp = tfa_is_cold_amp(tfa);
	pr_info("%s: is_cold_amp %d, first_after_boot %d\n",
		__func__, is_cold_amp, tfa->first_after_boot);
	if (tfa->first_after_boot || is_cold_amp == 1)
#endif /* REDUCED_REGISTER_SETTING */
	{
		/* process the device list
		 * to see if the user implemented the noinit
		 */
		for (i = 0; i < dev->length; i++) {
			if (dev->list[i].type == dsc_no_init) {
				noinit = 1;
				break;
			}
		}

		if (!noinit) {
			/* Read AUDFS & FRACTDEL prior to (re)init. */
			audfs = TFA_GET_BF(tfa, AUDFS);
			fractdel = TFA_GET_BF(tfa, FRACTDEL);
			/* load the optimal TFA98XX in HW settings */
			err = tfa98xx_init(tfa);
			PRINT_ASSERT(err);

			/* Restore audfs & fractdel after coldboot,
			 * so we can calibrate with correct fs setting.
			 * in case something else was given in cnt file,
			 * profile below will apply this.
			 */
			TFA_SET_BF(tfa, AUDFS, audfs);
			TFA_SET_BF(tfa, FRACTDEL, fractdel);
		} else {
			pr_debug("Warning: No init keyword found in the cnt file. Init is skipped!\n");
		}

		/* I2S settings to define the audio input properties
		 * these must be set before the subsys is up
		 * this will run the list
		 * until a non-register item is encountered
		 */
		pr_info("%s: write registers under dev to device %d\n",
			__func__, tfa->dev_idx);
		err = tfa_cont_write_regs_dev(tfa);
		/* write device register settings */
		PRINT_ASSERT(err);
	}
#if defined(REDUCED_REGISTER_SETTING)
	else {
		pr_info("%s: skip_init and writing registers under dev (%d:%d)\n",
			__func__, tfa->first_after_boot, is_cold_amp);
	}
#endif /* REDUCED_REGISTER_SETTING */

#if defined(REDUCED_REGISTER_SETTING)
	if ((tfa->first_after_boot || (is_cold_amp == 1))
		|| (profile != tfa_dev_get_swprof(tfa)))
#endif /* REDUCED_REGISTER_SETTING */
	{
		/* also write register the settings from the default profile
		 * NOTE we may still have ACS=1
		 * so we can switch sample rate here
		 */
		pr_info("%s: write registers under profile (%d) to device %d\n",
			__func__, profile, tfa->dev_idx);
		err = tfa_cont_write_regs_prof(tfa, profile);
		PRINT_ASSERT(err);
	}
#if defined(REDUCED_REGISTER_SETTING)
	else {
		pr_info("%s: skip writing registers under profile (%d) to device %d\n",
			__func__, profile, tfa->dev_idx);
	}
#endif /* REDUCED_REGISTER_SETTING */

	/* Factory trimming for the Boost converter */
	tfa98xx_factory_trimmer(tfa);

#if defined(REDUCED_REGISTER_SETTING)
	tfa->first_after_boot = 0;
#endif

#if defined(TFA_CHANGE_PCM_FORMAT)
	/* update PCM format, if changed, before power up */
	tfa_overwrite_pcm_format(tfa);
#endif

	tfa_state = tfa_dev_get_state(tfa);
	if (tfa_state == TFA_STATE_INIT_CF
		|| tfa_state == TFA_STATE_INIT_FW
		|| tfa_state == TFA_STATE_OPERATING) {
		pr_info("%s: skip setting state to INIT_CF (%d)\n",
			__func__, tfa_state);
		err = show_current_state(tfa);

		return err;
	}

	/* Go to the initCF state */
	strlcpy(prof_name, tfa_cont_profile_name(tfa->cnt,
		tfa->dev_idx, profile), MAX_CONTROL_NAME);
	tfa_dev_set_state(tfa, TFA_STATE_INIT_CF,
		strnstr(prof_name, ".cal", strlen(prof_name)) != NULL);

	err = show_current_state(tfa);

	return err;
}

/*
 * run the startup/init sequence and set ACS bit
 */
enum tfa98xx_error tfa_run_coldstartup(struct tfa_device *tfa, int profile)
{
	enum tfa98xx_error err = TFA98XX_ERROR_OK;

#if defined(REDUCED_REGISTER_SETTING)
	tfa->first_after_boot = 1;
#endif
	err = tfa_run_startup(tfa, profile);
	PRINT_ASSERT(err);
	if (err)
		return err;

	if (!tfa->is_probus_device) {
		/* force cold boot */
		err = tfa_run_coldboot(tfa, 1); /* set ACS */
		PRINT_ASSERT(err);
		if (err)
			return err;
	}

	/* start */
	err = tfa_run_start_dsp(tfa);
	PRINT_ASSERT(err);

	return err;
}

/*
 * run mute (optionally, with ramping down)
 */
enum tfa98xx_error tfa_run_mute(struct tfa_device *tfa)
{
	enum tfa98xx_error ret = TFA98XX_ERROR_OK;
	enum tfa_error err = tfa_error_ok;
	int status;
	int tries = 0;
#if defined(TFA_RAMPDOWN_BEFORE_MUTE)
	int i = 0, cur_ampe;

	cur_ampe = TFA_GET_BF(tfa, AMPE);
	if (cur_ampe == 0)
		tfa_gain_rampdown(tfa, 0, -1);
	else
		for (i = 0; i < RAMPDOWN_MAX; i++) {
			ret = tfa_gain_rampdown(tfa, i, RAMPDOWN_MAX);
			if (ret == TFA98XX_ERROR_OTHER)
				break;
			/*
			 * practically, msleep takes 20 msec
			 * need to use usleep_range if it works
			 */
			msleep_interruptible(RAMPING_INTERVAL);
		}
#endif /* TFA_RAMPDOWN_BEFORE_MUTE */

	/* signal the TFA98XX to mute */
	/* err = tfa98xx_set_mute(tfa, TFA98XX_MUTE_AMPLIFIER); */
	err = tfa_dev_set_state(tfa, TFA_STATE_MUTE, 0);
	if (err != tfa_error_ok) {
		pr_err("%s: failed to set mute state (err %d)\n",
			__func__, err);
		return TFA98XX_ERROR_OTHER;
	}

	if (tfa->tfa_family == 1) {
		/* now wait for the amplifier to turn off */
		do {
			status = TFA_GET_BF(tfa, SWS);
			if (status == 0)
				break;

			/* wait 10ms to avoid busload */
			msleep_interruptible(BUSLOAD_INTERVAL);

			tries++;
		} while (tries < AMPOFFWAIT_TRIES);

		/*The amplifier is always switching*/
		if (tries == AMPOFFWAIT_TRIES) {
			pr_err("%s: timeout in stopping amplifier switching\n",
				__func__);
			return TFA98XX_ERROR_OTHER;
		}
	}

	if (tfa->verbose)
		pr_debug("-------------------- muted ------------------\n");

	return ret;
}

/*
 * run unmute
 */
enum tfa98xx_error tfa_run_unmute(struct tfa_device *tfa)
{
	enum tfa_error err = tfa_error_ok;

	/* signal the TFA98XX to mute */
	/* err = tfa98xx_set_mute(tfa, TFA98XX_MUTE_OFF); */
	err = tfa_dev_set_state(tfa, TFA_STATE_UNMUTE, 0);
	if (err != tfa_error_ok) {
		pr_err("%s: failed to set unmute state (err %d)\n",
			__func__, err);
		return TFA98XX_ERROR_OTHER;
	}

	if (tfa->verbose)
		pr_debug("------------------- unmuted -----------------\n");

	return TFA98XX_ERROR_OK;
}

#if defined(TFA_RAMPDOWN_BEFORE_MUTE)
enum tfa98xx_error tfa_gain_rampdown(struct tfa_device *tfa,
	int step, int count)
{
	enum tfa98xx_error err = TFA98XX_ERROR_OK;
	int cur_ampgain;

	if (tfa->tfa_family != 2) {
		pr_debug("%s: rampdown only for tfa2\n",
			__func__);
		return TFA98XX_ERROR_BAD_PARAMETER;
	}

	if (step == 0) {
		cur_ampgain = tfa_get_bf(tfa, TFA2_BF_AMPGAIN);
		if (cur_ampgain <= 0) {
			pr_debug("%s: ampgain is already rampdown (%d)\n",
				__func__, cur_ampgain);
			return TFA98XX_ERROR_OTHER;
		}
		tfa->ampgain = cur_ampgain;
		if (count < 0)
			pr_debug("%s: direct drop ampgain (%d to 0)\n",
				__func__, tfa->ampgain);
		else
			pr_debug("%s: ramp down ampgain (%d)\n",
				__func__, tfa->ampgain);
	}

	if (tfa->ampgain == -1) {
		pr_debug("%s: reference gain is not valid\n", __func__);
		return TFA98XX_ERROR_BAD_PARAMETER;
	}

	/* ramp down amplifier gain for "count" msec */
	if (count < 0) /* direct set */
		err = tfa_set_bf(tfa, TFA2_BF_AMPGAIN, 0);
	else /* stepwise set */
		err = tfa_set_bf(tfa, TFA2_BF_AMPGAIN,
			tfa->ampgain * (count - step - 1) / count);
	if (err)
		pr_err("%s: error in setting AMPGAIN\n",
			__func__);

	return err;
}

enum tfa98xx_error tfa_gain_restore(struct tfa_device *tfa,
	int step, int count)
{
	enum tfa98xx_error err = TFA98XX_ERROR_OK;
	int cur_ampgain;

	if (tfa->tfa_family != 2) {
		pr_debug("%s: restore only for tfa2\n",
			__func__);
		return TFA98XX_ERROR_BAD_PARAMETER;
	}

	if (step == 0) {
		cur_ampgain = tfa_get_bf(tfa, TFA2_BF_AMPGAIN);
		if ((cur_ampgain == tfa->ampgain)
			|| (cur_ampgain > 0 && tfa->ampgain == -1)) {
			pr_debug("%s: ampgain is already restorted (%d; %d)\n",
				__func__, cur_ampgain, tfa->ampgain);
			return TFA98XX_ERROR_OTHER;
		}
		if (count < 0)
			pr_debug("%s: direct set ampgain (0 to %d)\n",
				__func__, tfa->ampgain);
		else
			pr_debug("%s: restore ampgain (%d)\n",
				__func__, tfa->ampgain);
	}

	/* ramp up amplifier gain for "count" msec */
	if (count < 0) /* direct set */
		err = tfa_set_bf(tfa, TFA2_BF_AMPGAIN, tfa->ampgain);
	else /* stepwise set */
		err = tfa_set_bf(tfa, TFA2_BF_AMPGAIN,
			tfa->ampgain * (step + 1) / count);
	if (err)
		pr_err("%s: error in setting AMPGAIN\n",
			__func__);

	return err;
}
#endif /* TFA_RAMPDOWN_BEFORE_MUTE */

#if defined(USE_TFA9888)
static void individual_calibration_results(struct tfa_device *tfa)
{
	int value_p, value_s;

	/* Read the calibration result in xmem (529=primary channel) (530=secondary channel) */
	mem_read(tfa, 529, 1, &value_p);
	mem_read(tfa, 530, 1, &value_s);

	if (value_p != 1 && value_s != 1)
		pr_debug("Calibration failed on both channels!\n");
	else if (value_p != 1) {
		pr_debug("Calibration failed on Primary (Left) channel!\n");
		TFA_SET_BF_VOLATILE(tfa, SSLEFTE, 0); /* Disable the sound for the left speaker */
	}
	else if (value_s != 1) {
		pr_debug("Calibration failed on Secondary (Right) channel!\n");
		TFA_SET_BF_VOLATILE(tfa, SSRIGHTE, 0); /* Disable the sound for the right speaker */
	}

	TFA_SET_BF_VOLATILE(tfa, AMPINSEL, 0); /* Set amplifier input to TDM */
	TFA_SET_BF_VOLATILE(tfa, SBSL, 1);
}
#endif /* USE_TFA9888 */

enum tfa98xx_error tfa_wait_cal(struct tfa_device *tfa)
{
	enum tfa98xx_error cal_err = TFA98XX_ERROR_OK;
	int calibration_done = 0;
	int need_restore = 0;
	int pre_set_count;
#if defined(WRITE_CALIBRATION_DATA_TO_MTP)
	enum tfa98xx_error err = TFA98XX_ERROR_OK;
	int i;
#endif /* WRITE_CALIBRATION_DATA_TO_MTP */

	pr_info("%s: [%d] triggered\n",
		__func__, tfa->dev_idx);

	cal_err = tfa_run_wait_calibration(tfa, &calibration_done);
	if (cal_err != TFA98XX_ERROR_OK || calibration_done == 0) {
		pr_err("%s: calibration is not done; stop processing\n", __func__);
#if !defined(WRITE_CALIBRATION_DATA_PARTLY)
		return cal_err;
#endif
	}

	for (i = 0; i < devcount; i++) {
		struct tfa_device *ntfa
			= tfa98xx_get_tfa_device_from_index(i);

		if (ntfa == NULL)
			continue;
#if defined(TFA_USE_DEVICE_SPECIFIC_CONTROL)
		if ((ntfa->active_handle != -1)
			&& (ntfa->active_handle != i))
			continue;
#endif

#if !defined(MPLATFORM)
		/* force MUTE after calibration, to set UNMUTE at sync later */
		pr_debug("%s: [%d] force MUTE after calibration\n",
			__func__, ntfa->dev_idx);
		tfa_dev_set_state(ntfa, TFA_STATE_MUTE, 1);
#endif

#if defined(WRITE_CALIBRATION_DATA_TO_MTP)
#if defined(WRITE_CALIBRATION_DATA_PARTLY)
		if (ntfa->spkr_damaged)
			continue;
#endif

		pr_debug("%s: [%d] process calibration data\n",
			__func__, ntfa->dev_idx);
		err = tfa_dsp_get_calibration_impedance(ntfa);
		if (err != TFA98XX_ERROR_OK) {
			cal_err = err;
			PRINT_ASSERT(err);
		}
#endif /* WRITE_CALIBRATION_DATA_TO_MTP */

		if (ntfa->next_profile == ntfa->profile
			|| ntfa->next_profile < 0)
			continue;

		need_restore = 1;
	}

	if (!need_restore)
		return cal_err;

	/* reset counter */
	pre_set_count = set_count;
	set_count = 0;

	/*
	 * restore profile after calibration.
	 * typically, when calibration is done,
	 * profile should be updated in warm state.
	 */
	for (i = 0; i < devcount; i++) {
		struct tfa_device *ntfa
			= tfa98xx_get_tfa_device_from_index(i);

		if (ntfa == NULL)
			continue;
#if defined(TFA_USE_DEVICE_SPECIFIC_CONTROL)
		if ((ntfa->active_handle != -1)
			&& (ntfa->active_handle != i))
			continue;
#endif

		set_count++;

		pr_info("%s: [%d] restore profile after calibration (active %d; next %d)\n",
			__func__, ntfa->dev_idx,
			ntfa->profile, ntfa->next_profile);

		/* switch profile */
		if (cal_err != TFA98XX_ERROR_OK || calibration_done == 0) {
			/* only with the register setting in failure case */
			pr_info("%s: apply only register setting at failure\n",
				__func__);

			err = tfa_cont_write_regs_prof(ntfa,
				ntfa->next_profile);
			if (err != TFA98XX_ERROR_OK)
				pr_err("%s: error in writing regs (%d)\n",
					__func__, err);
		} else {
			/* with the entire setting in success case */
			pr_info("%s: apply the whole profile setting at success\n",
				__func__);

			err = tfa_dev_switch_profile(ntfa,
				ntfa->next_profile, ntfa->vstep);
			if (err != TFA98XX_ERROR_OK)
				pr_err("%s: error in switch profile (%d)\n",
					__func__, err);
		}
	}

	/* restore counter */
	set_count = pre_set_count;

	return cal_err;
}

/*
 * wait for calibrate_done
 */
enum tfa98xx_error
tfa_run_wait_calibration(struct tfa_device *tfa, int *calibrate_done)
{
	enum tfa98xx_error err = TFA98XX_ERROR_OK;
	int tries = 0, mtp_busy = 1, tries_mtp_busy = 0;
#if defined(TFA_CUSTOM_FORMAT_AT_RESPONSE)
	char buffer[(1 + 2) * 3] = {0};
#else
	char buffer[2 * 3] = {0};
#endif /* TFA_CUSTOM_FORMAT_AT_RESPONSE */
	int res_len;
	int fw_status[2] = {0};
	int dsp_event = 0, dsp_status = 0;
	int i;
	int damaged = 0;

	*calibrate_done = 0;

	/* in case of calibrate once wait for MTPEX */
	if (!tfa->is_probus_device
		&& TFA_GET_BF(tfa, MTPOTC)) {
		/* Check if MTP_busy is clear! */
		while (tries_mtp_busy < MTPBWAIT_TRIES) {
			mtp_busy = tfa_dev_get_mtpb(tfa);
			if (mtp_busy == 1)
				/* wait 10ms to avoid busload */
				msleep_interruptible(BUSLOAD_INTERVAL);
			else
				break;
			tries_mtp_busy++;
		}

		if (tries_mtp_busy < MTPBWAIT_TRIES) {
			/* Because of the msleep
			 * TFA98XX_API_WAITRESULT_NTRIES is way to long!
			 * Setting this to 25 will take
			 * at least 25*50ms = 1.25 sec
			 */
			while ((*calibrate_done == 0)
				&& (tries < MTPEX_WAIT_NTRIES)) {
				*calibrate_done = TFA_GET_BF(tfa, MTPEX);
				if (*calibrate_done == 1)
					break;
				/* wait 50ms to avoid busload */
				msleep_interruptible(5 * BUSLOAD_INTERVAL);
				tries++;
			}

			if (tries >= MTPEX_WAIT_NTRIES)
				tries = TFA98XX_API_WAITRESULT_NTRIES;
		} else {
			pr_err("MTP busy after %d tries\n", MTPBWAIT_TRIES);
		}
	}

	if (!tfa->is_probus_device) {
		/* poll xmem for calibrate always
		 * calibrate_done = 0 means "calibrating",
		 * calibrate_done = -1 (or 0xffffff) means "fails"
		 * calibrate_done = 1 means calibration done
		 */
		while ((*calibrate_done != 1)
			&& (tries < TFA98XX_API_WAITRESULT_NTRIES)) {
			err = mem_read(tfa, TFA_FW_XMEM_CALIBRATION_DONE,
				1, calibrate_done);
			if (*calibrate_done == -1)
				break;
			tries++;
		}
	} else {
		while ((*calibrate_done != 1)
			&& (tries < TFA98XX_API_WAITCAL_NTRIES)) {
			msleep_interruptible(CAL_STATUS_INTERVAL);

#if defined(TFA_CUSTOM_FORMAT_AT_RESPONSE)
			res_len = (1 + 2) * 3;
#else
			res_len = 2 * 3;
#endif /* TFA_CUSTOM_FORMAT_AT_RESPONSE */
			err = tfa_dsp_cmd_id_write_read
				(tfa, MODULE_FRAMEWORK, FW_PAR_ID_GET_STATUS_CHANGE,
				res_len, (unsigned char *)buffer);
			if (err != TFA98XX_ERROR_OK)
				break;

			tfa98xx_convert_bytes2data(res_len,
				buffer, fw_status);
			dsp_event = fw_status[0];
			dsp_status = fw_status[1];
			pr_debug("%s: err (%d), status (0x%06x:0x%06x), count (%d)\n",
				__func__, err,
				dsp_event, dsp_status,
				tries + 1);

			if ((dsp_event & 0x6) != 0) { /* damage event */
				for (i = 0; i < devcount; i++) {
					struct tfa_device *ntfa
						= tfa98xx_get_tfa_device_from_channel(i);
					int damage_event = 0;

					if (ntfa == NULL)
						continue;

					damage_event = (TFA_GET_BIT_VALUE(dsp_event,
						i + 1)) ? 1 : 0;
					if (damage_event) {
#if defined(DETECT_DAMAGE_WITH_EVENT)
						/* set damage flag with event */
						ntfa->spkr_damaged |= damage_event;
#else
						/* set damage flag with status at event */
						ntfa->spkr_damaged
							= (TFA_GET_BIT_VALUE(dsp_status,
							i + 1))
							? 1 : 0;
#endif /* DETECT_DAMAGE_WITH_EVENT */
						pr_info("%s: damage flag update %d (dev %d, channel %d)\n",
							__func__, ntfa->spkr_damaged,
							ntfa->dev_idx, i);
						damaged = 1;
					}
				}

				if (damaged)
					break; /* exit loop */
			}

			/* set success if damage is not detected */
			if (dsp_status & 0x1) {
				*calibrate_done = 1;
				break;
			}

			tries++;
		}

		if (tries >= TFA98XX_API_WAITCAL_NTRIES)
			tries = TFA98XX_API_WAITRESULT_NTRIES;
	}

	if (*calibrate_done != 1) {
		pr_err("Calibration failed!\n");
		err = TFA98XX_ERROR_BAD_PARAMETER;
	} else if (tries == TFA98XX_API_WAITRESULT_NTRIES) {
		pr_err("Calibration has timedout!\n");
		err = TFA98XX_ERROR_STATE_TIMED_OUT;
	} else if (tries_mtp_busy == MTPBWAIT_TRIES) {
		pr_err("Calibrate Failed: MTP_busy stays high!\n");
		err = TFA98XX_ERROR_STATE_TIMED_OUT;
	} else {
		pr_info("Calibration succeeded!\n");
	}

	/* Give reason why calibration failed! */
	if (err != TFA98XX_ERROR_OK) {
		if ((tfa->tfa_family == 2)
			&& (TFA_GET_BF(tfa, REFCKSEL) == 1))
			pr_err("Unable to calibrate the device with the internal clock!\n");
	}

	if (!tfa->is_probus_device) {
#if defined(USE_TFA9888)
		/* Check which speaker calibration failed. Only for 88C */
		if ((err != TFA98XX_ERROR_OK) && ((tfa->rev & 0x0fff) == 0xc88))
			individual_calibration_results(tfa);
#endif
		return err;
	}

	/* success: probus device */
	if (err == TFA98XX_ERROR_OK) {
		/* reset damage flag at success */
		for (i = 0; i < devcount; i++) {
			struct tfa_device *ntfa
				= tfa98xx_get_tfa_device_from_index(i);

			if (ntfa == NULL)
				continue;

			ntfa->spkr_damaged = 0;
		}

		return err;
	}

	/* falure: probus device */
#if defined(DETECT_DAMAGE_WITH_EVENT)
	/* show damage status at failure */
	for (i = 0; i < devcount; i++) {
		struct tfa_device *ntfa
			= tfa98xx_get_tfa_device_from_channel(i);

		if (ntfa == NULL)
			continue;

		if (ntfa->spkr_damaged)
			pr_info("%s: speaker damage is detected [%s] (dev %d, channel %d)\n",
				__func__,
				tfa_cont_device_name(ntfa->cnt, ntfa->dev_idx),
				ntfa->dev_idx, i);
	}
#else
	/* set damage status with status at failure */
	pr_info("%s: speaker damage flag 0x%06x:0x%06x\n",
		__func__, fw_status[0], fw_status[1]);

	if ((fw_status[1] & 0x6) == 0)
		return err;

	for (i = 0; i < devcount; i++) {
		struct tfa_device *ntfa
			= tfa98xx_get_tfa_device_from_channel(i);

		if (ntfa == NULL)
			continue;

		ntfa->spkr_damaged
			= (TFA_GET_BIT_VALUE(fw_status[1], i + 1))
			? 1 : 0;

		if (ntfa->spkr_damaged)
			pr_info("%s: speaker damage is detected [%s] (dev %d, channel %d)\n",
				__func__,
				tfa_cont_device_name(ntfa->cnt, ntfa->dev_idx),
				ntfa->dev_idx, i);
	}
#endif /* DETECT_DAMAGE_WITH_EVENT */

	return err;
}

/*
 * tfa_dev_start will only do the basics:
 * Going from powerdown to operating or a profile switch.
 * for calibrating or acoustic shock
 * handling use tfa98xxCalibration function.
 */
enum tfa_error tfa_dev_start(struct tfa_device *tfa,
	int next_profile, int vstep)
{
	enum tfa98xx_error err = TFA98XX_ERROR_OK;
	int active_profile = -1;
	int mtpex = 0, cal_profile = -1;
	static int tfa98xx_log_start_cnt;
#if defined(TFA_USE_DEVICE_SPECIFIC_CONTROL)
#if defined(TFA_PROFILE_ON_DEVICE)
	int dev;
#endif
#endif
	int tfa_state_before, tfa_state;

	tfa98xx_log_start_cnt++;

	pr_debug("%s: tfa98xx_log_tfa_family=%d,",
		__func__, tfa->tfa_family);
	pr_debug("%s: tfa98xx_log_revision=0x%x,",
		__func__, tfa->rev & 0xff);
	pr_debug("%s: tfa98xx_log_subrevision=0x%x,",
		__func__, (tfa->rev >> 8) & 0xff);
	pr_debug("%s: tfa98xx_log_i2c_slaveaddress=0x%x,",
		__func__, tfa->slave_address);
	pr_info("%s: tfa98xx_log_start_cnt=%d next_profile %d\n",
		__func__, tfa98xx_log_start_cnt, next_profile);

	if (devcount <= 0)
		devcount = tfa->cnt->ndev;
	activecount = devcount;

	tfa->next_profile = next_profile;

#if defined(TFA_USE_DEVICE_SPECIFIC_CONTROL)
	tfa->active_handle = -1;

#if defined(TFA_PROFILE_ON_DEVICE)
	/* check activeness with profile */
	for (dev = 0; dev < devcount; dev++) {
		if (tfa_cont_is_dev_specific_profile(tfa->cnt,
			dev, next_profile) != 0) {
			tfa->active_handle = dev;
			activecount = 1;
			break;
		}
	}
	if (tfa->active_handle != -1)
		pr_info("%s: active handle [%s]", __func__,
			tfa_cont_device_name(tfa->cnt, tfa->active_handle));
#endif

	if (tfa->active_handle != -1) {
		if (tfa->dev_idx != tfa->active_handle) {
			tfa_dev_set_swprof(tfa,
				(unsigned short)next_profile);
			tfa_dev_set_swvstep(tfa,
				(unsigned short)vstep);

			pr_info("%s: stop unused dev %d, by force\n",
				__func__, tfa->dev_idx);
			err = (enum tfa98xx_error)
				tfa_dev_stop(tfa); /* stop inactive handle */

			return err;
		}
	}
#endif /* TFA_USE_DEVICE_SPECIFIC_CONTROL */

	/* Get currentprofile */
	active_profile = tfa_dev_get_swprof(tfa);
	if (active_profile == 0xff)
		active_profile = -1;

#if defined(TFA_RAMPDOWN_BEFORE_MUTE)
	/* restore amplifier gain, if it's touched before */
	if (tfa->ampgain != -1) {
		int i = 0, cur_ampe;

		cur_ampe = TFA_GET_BF(tfa, AMPE);
		if (cur_ampe == 0)
			tfa_gain_restore(tfa, 0, -1);
		else
			for (i = 0; i < RAMPDOWN_MAX; i++) {
				err = tfa_gain_restore(tfa, i, RAMPDOWN_MAX);
				if (err == TFA98XX_ERROR_OTHER)
					break;
				/*
				 * practically, msleep takes 20 msec
				 * need to use usleep_range if it works
				 */
				msleep_interruptible(RAMPING_INTERVAL);
			}
	}
	tfa->ampgain = -1;
#endif /* TFA_RAMPDOWN_BEFORE_MUTE */

	mtpex = tfa_dev_mtp_get(tfa, TFA_MTP_EX);
	if (mtpex == 0 || tfa->reset_mtpex) {
		pr_info("%s: dev %d, MTPEX=%d%s\n",
			__func__, tfa->dev_idx, mtpex,
			(tfa->reset_mtpex) ? " (forced)" : "");
#if !defined(SET_DUMMY_CALIBRATION_VALUE)
		cal_profile = tfa_cont_get_cal_profile(tfa);
		if (cal_profile >= 0) {
			pr_info("%s: set profile for calibration profile %d\n",
				__func__, cal_profile);
			next_profile = cal_profile;
		}
#else
		if (tfa->verbose == 5 || tfa->verbose == 7) { // [temp]
			cal_profile = tfa_cont_get_cal_profile(tfa);
			if (cal_profile >= 0) {
				pr_info("%s: set profile for calibration profile %d\n",
					__func__, cal_profile);
				next_profile = cal_profile;
			}
		}
#endif /* SET_DUMMY_CALIBRATION_VALUE */
	}

	/* TfaRun_SpeakerBoost implies un-mute */
	pr_debug("active_profile:%s, next_profile:%s\n",
		tfa_cont_profile_name(tfa->cnt, tfa->dev_idx, active_profile),
		tfa_cont_profile_name(tfa->cnt, tfa->dev_idx, next_profile));
	pr_debug("Starting device [%s]\n",
		tfa_cont_device_name(tfa->cnt, tfa->dev_idx));

	err = show_current_state(tfa);

	if (tfa->tfa_family == 1) { /* TODO move this to ini file */
		/* Enable I2S output on TFA1 devices without TDM */
		err = tfa98xx_aec_output(tfa, 1);
		if (err != TFA98XX_ERROR_OK)
			goto tfa_dev_start_exit;
	}

	mutex_lock(&dev_lock);

	set_count++;

	if (tfa->bus != 0) { /* non i2c */
#ifndef __KERNEL__
		tfadsp_fw_start(tfa, next_profile, vstep);
#endif /* __KERNEL__ */
	} else {
		pr_debug("%s: device[%d] [%s] - tfa_run_speaker_boost profile=%d\n",
			__func__, tfa->dev_idx,
			tfa_cont_device_name(tfa->cnt, tfa->dev_idx),
			next_profile);

		tfa_state_before = tfa_dev_get_state(tfa);

		/* Check if we need coldstart or ACS is set */
		err = tfa_run_speaker_boost(tfa, 0, next_profile);
		if (err != TFA98XX_ERROR_OK) {
			mutex_unlock(&dev_lock);
			goto tfa_dev_start_exit;
		}

		pr_info("%s:[%s] device:%s profile:%s\n",
			__func__, (tfa->is_cold) ? "cold" : "warm",
			tfa_cont_device_name(tfa->cnt, tfa->dev_idx),
			tfa_cont_profile_name(tfa->cnt, tfa->dev_idx,
			next_profile));

		/* Make sure internal oscillator is running
		 * for DSP devices (non-dsp and max1 this is no-op)
		 */
		tfa98xx_set_osc_powerdown(tfa, 0);

		/* Go to the Operating state */
		tfa_state = tfa_dev_get_state(tfa);
		if (tfa_state == TFA_STATE_OPERATING) {
			pr_info("%s: skip setting state to OPERATING (%d)\n",
				__func__, tfa_state);
			if (tfa_state_before == TFA_STATE_OPERATING) {
				pr_debug("%s: device already active\n", __func__);
			} else {
				/* at last device only: to skip mute */
				if (tfa_nr_device_set(tfa) == activecount) {
					pr_debug("%s: skip MUTE at the last device\n",
						__func__);
				} else {
					pr_debug("%s: set MUTE until all are ready\n",
						__func__);
					tfa_dev_set_state(tfa, TFA_STATE_MUTE, 0);
				}
			}
		} else {
			tfa_dev_set_state(tfa,
				TFA_STATE_OPERATING | TFA_STATE_MUTE, 0);
		}
	}

	mutex_unlock(&dev_lock);

	/* skip as profile is restored by tfa_wait_cal */
	if (cal_profile >= 0)
		goto tfa_dev_start_exit;

	/* Profile switching in call */
	mutex_lock(&dev_lock);
	err = (enum tfa98xx_error)
		tfa_dev_switch_profile(tfa, next_profile, vstep);
	mutex_unlock(&dev_lock);
	if (err != TFA98XX_ERROR_OK)
		goto tfa_dev_start_exit;

#if (defined(USE_TFA9892) || defined(USE_TFA9888))
	/* PLMA5539: Gives information about current setting of powerswitch */
	if (tfa->verbose) {
		if (!tfa98xx_powerswitch_is_enabled(tfa))
			pr_info("Device start without powerswitch enabled!\n");
	}
#endif

tfa_dev_start_exit:
	show_current_state(tfa);

	mutex_lock(&dev_lock);
	if (tfa_nr_device_set(tfa) == activecount)
		/* reset counter */
		set_count = 0;
	mutex_unlock(&dev_lock);

	return err;
}

enum tfa_error tfa_dev_switch_profile(struct tfa_device *tfa,
	int next_profile, int vstep)
{
	enum tfa98xx_error err = TFA98XX_ERROR_OK;
	int active_profile = -1;
	char prof_name[MAX_CONTROL_NAME] = {0};

#if defined(TFA_USE_WAITQUEUE_SEQ)
	if (tfa->ext_dsp) {
		int rc;

		pr_info("%s: wait until set_calibration\n",
			__func__);

		/* wait until done for last device */
		rc = wait_event_interruptible_timeout(tfa->waitq_seq,
			(tfa_nr_device_set(tfa) == activecount) ? 1 : 0,
			msecs_to_jiffies(TFA98XX_API_WAITCAL_NTRIES * 120));

		pr_info("%s: waken up at set_calibration\n",
			__func__);
	}
#endif

	active_profile = tfa_dev_get_swprof(tfa);
	pr_info("%s: profile (active %d; next %d)\n",
		__func__, active_profile, next_profile);

	/* Profile switching */
	if (next_profile != active_profile && active_profile >= 0) {
		pr_debug("%s: switch profile from %d to %d\n",
			__func__, active_profile, next_profile);

		/* at initial device only: write files only once */
		if (tfa_nr_device_set(tfa) == 1
			&& tfa_nr_config_done(tfa) == 0)
			tfa->is_bypass = 1; /* reset beforehand */

		err = tfa_cont_write_profile(tfa, next_profile, vstep);
		if (err != TFA98XX_ERROR_OK)
			return err;
	}

	/* If the profile contains the .standby suffix go
	 * to powerdown else we should be in operating state
	 */
	strlcpy(prof_name, tfa_cont_profile_name(tfa->cnt,
		tfa->dev_idx, next_profile), MAX_CONTROL_NAME);
	if (strnstr(prof_name, ".standby", strlen(prof_name)) != NULL) {
		tfa_dev_set_swprof(tfa, (unsigned short)next_profile);
		tfa_dev_set_swvstep(tfa, (unsigned short)vstep);
		return err;
	} else if (TFA_GET_BF(tfa, PWDN) != 0) {
		err = tfa98xx_powerdown(tfa, 0);
	}

	err = show_current_state(tfa);

	tfa->vstep = tfa_dev_get_swvstep(tfa);
	if ((TFA_GET_BF(tfa, CFE) != 0)
		&& (vstep != tfa->vstep) && (vstep != -1)) {
		err = tfa_cont_write_files_vstep(tfa, next_profile, vstep);
		if (err != TFA98XX_ERROR_OK)
			return err;
	}

	/* Always search and apply filters after a startup */
	err = tfa_set_filters(tfa, next_profile);
	if (err != TFA98XX_ERROR_OK)
		return err;

	tfa_dev_set_swprof(tfa, (unsigned short)next_profile);
	tfa_dev_set_swvstep(tfa, (unsigned short)vstep);

	return err;
}

enum tfa_error tfa_dev_stop(struct tfa_device *tfa)
{
	enum tfa98xx_error err = TFA98XX_ERROR_OK;

	if (devcount <= 0)
		devcount = tfa->cnt->ndev;

	pr_debug("Stopping device [%s]\n",
		tfa_cont_device_name(tfa->cnt, tfa->dev_idx));

	/* mute */
	tfa_run_mute(tfa);

#if defined(MPLATFORM)
	/* cancel other pending wait_cal works */
	cancel_delayed_work(&tfa->wait_cal_work);
#endif

	/* Make sure internal oscillator is not running
	 * for DSP devices (non-dsp and max1 this is no-op)
	 */
	tfa98xx_set_osc_powerdown(tfa, 1);

	/* powerdown CF */
	err = tfa98xx_powerdown(tfa, 1);
	if (err != TFA98XX_ERROR_OK)
		return err;

	/* disable I2S output on TFA1 devices without TDM */
	err = tfa98xx_aec_output(tfa, 0);

	/* CHECK: only once after buffering fully */
	/* at last device only: to flush buffer */
	if (tfa_nr_device_set(tfa) == activecount
		|| tfa_nr_config_done(tfa) > 0) {
		// config_count = 0;
		if (tfa->ext_dsp == 1) {
			/* flush message buffer */
			pr_debug("%s: flush buffer in blob, at stop\n",
				__func__);
			err = tfa_tib_dsp_msgmulti(tfa, -2, NULL);
		}
	}

	if (tfa98xx_count_active_stream(BIT_PSTREAM) == 0
		&& tfa98xx_count_active_stream(BIT_CSTREAM) == 0) {
		pr_info("%s: all stopped: no active stream\n",
			__func__);
		/* reset counters */
		set_count = 0;
		config_count = 0;
		dsp_cal_value[0] = dsp_cal_value[1] = -1;
	}

	return err;
}

#if defined(TFA_CHANGE_PCM_FORMAT)
enum tfa_error tfa_dev_config_pcm_format(struct tfa_device *tfa,
	int ndev, int sample_size, int slot_size)
{
	pr_info("%s: ndev %d, sample %d bits, slot %d bits\n",
		__func__, ndev, sample_size, slot_size);

	tfa->tdm_config.ssize = sample_size - 1;
	tfa->tdm_config.slln = slot_size - 1;
	tfa->tdm_config.srcmap = 0xff;

	if (sample_size > slot_size)
		goto config_pcm_format_error_exit;

	/* read # of slots from container / device */
	/* default: stereo input & 2 slots for V/I */
	tfa->tdm_config.slots = tfa98xx_get_cnt_bitfield(tfa,
		TFA7x_FAM(tfa, TDMSLOTS));
	pr_info("%s: number of slots %d\n",
		__func__, tfa->tdm_config.slots);

	switch (slot_size) {
	case 16:
		if (ndev > 1 && tfa->is_probus_device) /* only 32-bit for compress */
			goto config_pcm_format_error_exit;

		if (tfa->tdm_config.slots == 1)
			tfa->tdm_config.nbck = 0; /* 32 fs */
		else if (tfa->tdm_config.slots == 2)
			tfa->tdm_config.nbck = 1; /* 48 fs */
		else if (tfa->tdm_config.slots == 3)
			tfa->tdm_config.nbck = 2; /* 64 fs */
		else
			goto config_pcm_format_error_exit;
		break;
	case 24:
		if (ndev > 1 && tfa->is_probus_device) /* only 32-bit for compress */
			goto config_pcm_format_error_exit;

		if (tfa->tdm_config.slots == 1)
			tfa->tdm_config.nbck = 1; /* 48 fs */
		else if (tfa->tdm_config.slots == 3)
			tfa->tdm_config.nbck = 3; /* 96 fs */
		else
			goto config_pcm_format_error_exit;
		break;
	case 32:
		if (!tfa->is_probus_device) {
			tfa->tdm_config.ssize = (sample_size > 24)
				? 23 : tfa->tdm_config.ssize; // CF
		} else {
			if (ndev == 1) { /* mono */
				if (tfa->tdm_config.slots == 0
					&& tfa->tdm_config.ssize == 31) { /* 32-bit mono slot */
					tfa->tdm_config.nbck = 0; /* 32 fs */
					tfa->tdm_config.srcmap = 3; /* 32-bit (compress) */
					break;
				}
			} else if (ndev == 2) { /* stereo */
				if (tfa->tdm_config.slots == 1) {
					tfa->tdm_config.ssize = 31; /* for V/I */
					tfa->tdm_config.srcmap = 3; /* 32-bit (compress) */
				}
			}
		}

		if (tfa->tdm_config.slots == 1)
			tfa->tdm_config.nbck = 2; /* 64 fs */
		else if (tfa->tdm_config.slots == 2)
			tfa->tdm_config.nbck = 3; /* 96 fs */
		else if (tfa->tdm_config.slots == 3)
			tfa->tdm_config.nbck = 4; /* 128 fs */
		else
			goto config_pcm_format_error_exit;
		break;
	default:
		goto config_pcm_format_error_exit;
	}

	if (tfa->tdm_config.srcmap == 0xff) {
		switch (sample_size) {
		case 16:
			tfa->tdm_config.srcmap = 2; /* 16-bit */
			break;
		case 24:
			tfa->tdm_config.srcmap = 1; /* 24-bit */
			break;
		case 32:
			tfa->tdm_config.srcmap = 0; /* 32-bit */
			break;
		default:
			goto config_pcm_format_error_exit;
		}
	}

	pr_info("%s: nbck %u, slln %u, ssize %u, slots %u, srcmap %u\n",
		__func__, tfa->tdm_config.nbck,
		tfa->tdm_config.slln, tfa->tdm_config.ssize,
		tfa->tdm_config.slots, tfa->tdm_config.srcmap);

	return tfa_error_ok;

config_pcm_format_error_exit:
	pr_err("%s: invalid configuration\n", __func__);
	tfa->tdm_config.ssize = 0;
	tfa->tdm_config.slln = 0;
	tfa->tdm_config.srcmap = 0xff;

	return tfa_error_bad_param;
}
#endif /* TFA_CHANGE_PCM_FORMAT */

/*
 * int registers and coldboot dsp
 */
int tfa_reset(struct tfa_device *tfa)
{
	enum tfa98xx_error err = TFA98XX_ERROR_OK;
	int state = -1;
	int retry_cnt = 0;

	/* Check device state.
	 * Print warning if reset is done
	 * from other state than powerdown (when verbose)
	 */
	state = tfa_dev_get_state(tfa);
	if (tfa->verbose) {
		if (((tfa->tfa_family == 1)
			&& state != TFA_STATE_RESET)
			|| ((tfa->tfa_family == 2)
			&& state != TFA_STATE_POWERDOWN))
			pr_info("WARNING: Device reset should be performed in POWERDOWN state\n");
	}

	/* Split TFA1 behavior from TFA2*/
	if (tfa->tfa_family == 1) {
		err = TFA_SET_BF(tfa, I2CR, 1);
		if (err)
			return err;
		err = tfa98xx_powerdown(tfa, 0);
		if (err)
			return err;
		err = tfa_cf_powerup(tfa);
		if (err) {
			PRINT_ASSERT(err);
			return err;
		}
		err = tfa_run_coldboot(tfa, 1);
		if (err) {
			PRINT_ASSERT(err);
			return err;
		}
		err = TFA_SET_BF(tfa, I2CR, 1);
	} else {
		/* Probus devices needs extra protection to ensure proper reset
		 * behavior, this step is valid only in state other than powerdown
		 */
		if (tfa->is_probus_device && state != TFA_STATE_POWERDOWN) {
			err = TFA_SET_BF_VOLATILE(tfa, AMPE, 0);
			if (err)
				return err;
			err = tfa98xx_powerdown(tfa, 1);
			if (err) {
				PRINT_ASSERT(err);
				return err;
			}
		}

		err = TFA_SET_BF_VOLATILE(tfa, I2CR, 1);
		if (err)
			return err;

		/* Restore MANSCONF to POR state */
		err = TFA_SET_BF_VOLATILE(tfa, MANSCONF, 0);
		if (err)
			return err;

		/* Probus devices HW are already reseted here,
		 * Last step is to send init message to softDSP
		 */
		if (tfa->is_probus_device) {
			if (tfa->ext_dsp > 0) {
				err = tfa98xx_init_dsp(tfa);
				/* ext_dsp status from warm to cold after reset */
				if (tfa->ext_dsp == 2)
					tfa->ext_dsp = 1;
			}
		} else {
			/* Restore MANCOLD to POR state */
			TFA_SET_BF_VOLATILE(tfa, MANCOLD, 1);

			/* Coolflux has to be powered on to ensure proper ACS
			 * bit state
			 */

			/* Powerup CF to access CF io */
			err = tfa98xx_powerdown(tfa, 0);
			if (err) {
				PRINT_ASSERT(err);
				return err;
			}

			/* For clock */
			err = tfa_cf_powerup(tfa);
			if (err) {
				PRINT_ASSERT(err);
				return err;
			}

			/* Force cold boot */
			err = tfa_run_coldboot(tfa, 1); /* Set ACS */
			if (err) {
				PRINT_ASSERT(err);
				return err;
			}

			/* Set PWDN = 1, this will transfer device into powerdown state */
			err = TFA_SET_BF_VOLATILE(tfa, PWDN, 1);
			if (err)
				return err;

			/* 88 needs SBSL on top of PWDN bit to start transition,
			 * for 92 and 94 this doesn't matter
			 */
			err = TFA_SET_BF_VOLATILE(tfa, SBSL, 1);
			if (err)
				return err;

			/* Powerdown state should be reached within 1ms */
			for (retry_cnt = 0;
				retry_cnt < TFA98XX_WAITRESULT_NTRIES; retry_cnt++) {
				if (is_94_N2_device(tfa))
					state = tfa_get_bf(tfa, TFA9894N2_BF_MANSTATE);
				else
					state = TFA_GET_BF(tfa, MANSTATE);
				if (state < 0)
					return err;

				/* Check for MANSTATE=Powerdown (0) */
				if (state == 0)
					break;
				msleep_interruptible(BUSLOAD_INTERVAL);
			}

			/* Reset all I2C registers to default values,
			 * now device state is consistent, same as after powerup
			 */
			err = TFA_SET_BF(tfa, I2CR, 1);
		}
	}

	return err;
}

/*
 * Write all the bytes specified by num_bytes and data
 */
enum tfa98xx_error
tfa98xx_write_data(struct tfa_device *tfa,
	unsigned char subaddress, int num_bytes,
	const unsigned char data[])
{
	enum tfa98xx_error error = TFA98XX_ERROR_OK;
	/* subaddress followed by data */
	const int bytes2write = num_bytes + 1;
	unsigned char *write_data;

	if (num_bytes > TFA2_MAX_PARAM_SIZE)
		return TFA98XX_ERROR_BAD_PARAMETER;

	write_data = (unsigned char *)
		kmem_cache_alloc(tfa->cachep, GFP_KERNEL);
	if (write_data == NULL)
		return TFA98XX_ERROR_FAIL;

	write_data[0] = subaddress;
	memcpy(&write_data[1], data, num_bytes);

	error = tfa98xx_write_raw(tfa, bytes2write, write_data);

	kmem_cache_free(tfa->cachep, write_data);
	return error;
}

/*
 * fill the calibration value as milli ohms in the struct
 * assume that the device has been calibrated
 */
enum tfa98xx_error
tfa_dsp_get_calibration_impedance(struct tfa_device *tfa)
{
	enum tfa98xx_error error = TFA98XX_ERROR_OK;
	int calibrate_done, i, spkr_count = 0;
	int tries = 0;

	error = tfa_supported_speakers(tfa, &spkr_count);
	if (error != TFA98XX_ERROR_OK) {
		pr_err("error in checking supported speakers\n");
		return error;
	}

	if (tfa_dev_mtp_get(tfa, TFA_MTP_OTC)
		&& tfa_dev_mtp_get(tfa, TFA_MTP_EX) != 0) {
		pr_debug("Getting calibration values from MTP\n");

		if ((tfa->rev & 0xff) == 0x88) {
			for (i = 0; i < spkr_count; i++) {
				if (i == 0)
					tfa->mohm[i] = tfa_dev_mtp_get(tfa,
						TFA_MTP_RE25_PRIM);
				else
					tfa->mohm[i] = tfa_dev_mtp_get(tfa,
						TFA_MTP_RE25_SEC);
			}
		} else {
			tfa->mohm[0] = tfa_dev_mtp_get(tfa, TFA_MTP_RE25);
		}

		if (tfa->mohm[0] != 0)
			return error;
	}

	pr_debug("Getting calibration values from Speakerboost\n");

	/* Make sure the calibrate_done bit is set
	 * before getting the values from speakerboost!
	 * This does not work for 72
	 * (because the dsp cannot set this bit)
	 */
	if (!tfa->is_probus_device) {
		/* poll xmem for calibrate always
		 * calibrate_done = 0 means "calibrating",
		 * calibrate_done = -1 (or 0xffffff) means "fails"
		 * calibrate_done = 1 means calibration done
		 */
		calibrate_done = 0;
		while ((calibrate_done != 1)
			&& (tries < TFA98XX_API_WAITRESULT_NTRIES)) {
			error = mem_read(tfa,
				TFA_FW_XMEM_CALIBRATION_DONE,
				1, &calibrate_done);
			if (calibrate_done == 1)
				break;
			tries++;
		}

		if (calibrate_done != 1) {
			pr_err("Calibration failed!\n");
			error = TFA98XX_ERROR_BAD_PARAMETER;
		} else if (tries == TFA98XX_API_WAITRESULT_NTRIES) {
			pr_debug("Calibration has timedout!\n");
			error = TFA98XX_ERROR_STATE_TIMED_OUT;
		}
	}

	error = tfa_process_re25(tfa);

	return error;
}

static enum tfa98xx_error tfa_process_re25(struct tfa_device *tfa)
{
	enum tfa98xx_error error = TFA98XX_ERROR_OK;
	enum tfa_error ret = tfa_error_ok;
#if defined(TFA_CUSTOM_FORMAT_AT_RESPONSE)
	unsigned char bytes[(1 + 2) * 3] = {0};
#else
	unsigned char bytes[2 * 3] = {0};
#endif /* TFA_CUSTOM_FORMAT_AT_RESPONSE */
	int data[2];
	int nr_bytes, i, spkr_count = 0, cal_idx = 0;
	int scaled_data;
	unsigned int channel;
#if defined(WRITE_CALIBRATION_DATA_TO_MTP)
	int tries = 0;
	int readback = -1;
#endif

	error = tfa_supported_speakers(tfa, &spkr_count);
	if (error != TFA98XX_ERROR_OK) {
		pr_err("error in checking supported speakers\n");
		return error;
	}

	/* SoftDSP interface differs from hw-dsp interfaces */
	if (tfa->is_probus_device && devcount > 1)
		spkr_count = devcount;

	pr_info("%s: read SB_PARAM_GET_RE25C\n", __func__);

#if defined(TFA_CUSTOM_FORMAT_AT_RESPONSE)
	nr_bytes = (1 + spkr_count) * 3;
#else
	nr_bytes = spkr_count * 3;
#endif /* TFA_CUSTOM_FORMAT_AT_RESPONSE */
	channel = tfa98xx_get_cnt_bitfield(tfa, TFA7x_FAM(tfa, TDMSPKS));
	error = tfa_dsp_cmd_id_write_read(tfa,
		MODULE_SPEAKERBOOST, SB_PARAM_GET_RE25C, nr_bytes, bytes);
	if (error == TFA98XX_ERROR_OK) {
		tfa98xx_convert_bytes2data(nr_bytes, bytes, data);

		pr_debug("%s: RE25C - spkr_count %d, channel %d\n",
			__func__, spkr_count, channel);
		pr_debug("%s: RE25C - data[0]=%d - data[1]=%d\n",
			__func__, data[0], data[1]);

		for (i = 0; i < spkr_count; i++) {
			/* for probus devices, calibration values
			 * coming from soft-dsp speakerboost,
			 * are ordered in a different way.
			 * Re-align to standard representation.
			 */
			cal_idx = i;
			if ((tfa->is_probus_device && tfa->dev_idx >= 1))
				cal_idx = 0;

			/* signed data has a limit of 30 Ohm */
			// scaled_data = data[i];
			scaled_data = data[channel];

			tfa->mohm[cal_idx]
				= TFA_ReZ_FP_INT(scaled_data,
				TFA_FW_ReZ_SHIFT) * 1000
				+ TFA_ReZ_FP_FRAC(scaled_data,
				TFA_FW_ReZ_SHIFT);
		}

		pr_info("%s: %d mOhms\n", __func__, tfa->mohm[cal_idx]);

		/* special case for stereo calibration, from SB 3.5 PRC1 */
		if (tfa->mohm[cal_idx] == -128000) {
			pr_err("%s: wrong calibration! (damaged speaker)\n",
				__func__);
			tfa->spkr_damaged = 1;
		}
	} else {
		pr_err("%s: tfa_dsp_cmd_id_write_read is failed, err=%d\n",
			__func__, error);

		for (i = 0; i < spkr_count; i++) {
			cal_idx = i;
			if ((tfa->is_probus_device && tfa->dev_idx >= 1))
				cal_idx = 0;

			tfa->mohm[cal_idx] = -1;
		}

		return TFA98XX_ERROR_BAD_PARAMETER;
	}

#if defined(CHECK_CALIBRATION_DATA_RANGE)
	error = tfa_calibration_range_check(tfa,
		channel, tfa->mohm[cal_idx]);
	if (error != TFA98XX_ERROR_OK) {
		pr_err("%s: calibration data is out of range: device %d\n",
			__func__, tfa->dev_idx);
		return TFA98XX_ERROR_BAD_PARAMETER;
	}
#endif /* CHECK_CALIBRATION_DATA_RANGE */

#if defined(WRITE_CALIBRATION_DATA_TO_MTP)
	if (!tfa->is_probus_device)
		return error;

	/*
	if (tfa_dev_mtp_get(tfa, TFA_MTP_OTC) != 0) {
		pr_debug("%s: skip writing calibration data to MTP\n",
			__func__);
		return error;
		}
	*/
	/* store calibration data to MTP */
	ret = tfa_dev_mtp_set(tfa, TFA_MTP_OTC, 1);
	if (ret != tfa_error_ok) {
		pr_debug("%s: error in setting MTPOTC\n",
			__func__);
	}

	while (++tries < TFA98XX_API_REWRTIE_MTP_NTRIES) {
		msleep_interruptible(BUSLOAD_INTERVAL);

		if (tfa->verbose == 7) {
			pr_debug("%s: skip writing TFA_MTP_RE25 if verbose is 7\n",
				__func__);
			break;
		}

		/* set RE25 in shadow regiser */
		ret = tfa_dev_mtp_set(tfa,
			TFA_MTP_RE25, tfa->mohm[cal_idx]);
		if (ret != tfa_error_ok) {
			pr_err("%s: writing calibration data failed to MTP, device %d err (%d)\n",
				__func__, tfa->dev_idx, ret);
			return TFA98XX_ERROR_RPC_CALIB_FAILED;
		}

		msleep_interruptible(BUSLOAD_INTERVAL);

		readback = tfa_dev_mtp_get(tfa, TFA_MTP_RE25);
		if (readback < 0) {
			pr_err("%s: reading calibration data back failed from MTP, device %d readback (%d)\n",
				__func__, tfa->dev_idx, readback);
			return TFA98XX_ERROR_RPC_CALIB_FAILED;
		}
		pr_info("%s: readback from MTP - %d mOhms\n", __func__, readback);
#if defined(CHECK_CALIBRATION_DATA_RANGE)
		error = tfa_calibration_range_check(tfa,
			channel, readback);
		if (error != TFA98XX_ERROR_OK) {
			pr_err("%s: calibration data is out of range: device %d (to rewrite)\n",
				__func__, tfa->dev_idx);
			continue;
		}
#endif /* CHECK_CALIBRATION_DATA_RANGE */

		if (readback != tfa->mohm[cal_idx]) {
			pr_err("%s: calibration data was not written to MTP, device %d (%d != %d)\n",
				__func__, tfa->dev_idx, tfa->mohm[cal_idx], readback);
			continue;
		}

		break;
	}

	if (tries >= TFA98XX_API_REWRTIE_MTP_NTRIES) {
		pr_err("%s: writing calibration data timed out, device %d\n",
			__func__, tfa->dev_idx);
		return TFA98XX_ERROR_BAD_PARAMETER;
	}

	/* set MTPEX */
	tries = 0;
	while (++tries < TFA98XX_API_REWRTIE_MTP_NTRIES) {
		msleep_interruptible(BUSLOAD_INTERVAL);

		if (tfa->verbose == 7) {
			pr_debug("%s: skip writing TFA_MTP_EX if verbose is 7\n",
				__func__);
			break;
		}

		ret = tfa_dev_mtp_set(tfa, TFA_MTP_EX, 1);
		if (ret != tfa_error_ok) {
			pr_err("%s: setting MPTEX failed, device %d err (%d)\n",
				__func__, tfa->dev_idx, ret);
			continue;
		}

		msleep_interruptible(BUSLOAD_INTERVAL);

		readback = tfa_dev_mtp_get(tfa, TFA_MTP_EX);
		if (readback < 0) {
			pr_err("%s: reading MTPEX back failed from MTP, device %d readback (%d)\n",
				__func__, tfa->dev_idx, readback);
			return TFA98XX_ERROR_RPC_CALIB_FAILED;
		}

		if (readback != 1) {
			pr_err("%s: setting MTPEX failed, device %d (readback %d)\n",
				__func__, tfa->dev_idx, readback);
			continue;
		}

		break;
	}

	if (tries >= TFA98XX_API_REWRTIE_MTP_NTRIES) {
		pr_err("%s: setting MTPEX timed out, device %d\n",
			__func__, tfa->dev_idx);
		return TFA98XX_ERROR_BAD_PARAMETER;
	}
#endif /* WRITE_CALIBRATION_DATA_TO_MTP */

	return error;
}

/* start count from 1, 0 is invalid */
int tfa_dev_get_swprof(struct tfa_device *tfa)
{
	if (!tfa->dev_ops.get_swprof)
		return TFA98XX_ERROR_NOT_IMPLEMENTED;

	return (tfa->dev_ops.get_swprof)(tfa);
}

int tfa_dev_set_swprof(struct tfa_device *tfa, unsigned short new_value)
{
	if (!tfa->dev_ops.set_swprof)
		return TFA98XX_ERROR_NOT_IMPLEMENTED;

	return (tfa->dev_ops.set_swprof)(tfa, new_value + 1);
}

/* same value for all channels */
/* start count from 1, 0 is invalid */
int tfa_dev_get_swvstep(struct tfa_device *tfa)
{
	if (!tfa->dev_ops.get_swvstep)
		return TFA98XX_ERROR_NOT_IMPLEMENTED;

	return (tfa->dev_ops.get_swvstep)(tfa);
}

int tfa_dev_set_swvstep(struct tfa_device *tfa, unsigned short new_value)
{
	if (!tfa->dev_ops.set_swvstep)
		return TFA98XX_ERROR_NOT_IMPLEMENTED;

	return (tfa->dev_ops.set_swvstep)(tfa, new_value + 1);
}

/*
 * function overload for MTPB
 */
int tfa_dev_get_mtpb(struct tfa_device *tfa)
{
	if (!tfa->dev_ops.get_mtpb)
		return TFA98XX_ERROR_NOT_IMPLEMENTED;

	return (tfa->dev_ops.get_mtpb)(tfa);
}

int tfa_is_cold(struct tfa_device *tfa)
{
	int value;

	/*
	 * check for cold boot status
	 */
	if (tfa->is_probus_device) {
		if (tfa->ext_dsp > 0) {
#if !defined(TFA_USE_MANSTATE_TO_CHECK_COLD)
			if (tfa->ext_dsp == 2)
				value = 0; // warm
			else /* no dsp or cold */
				value = 1; // cold
#else
			int tfa_state;

			tfa_state = tfa_dev_get_state(tfa);
			if (tfa_state == TFA_STATE_OPERATING)
				value = 0; /* warm */
			else
				value = 1; /* cold */
#endif
		} else {
			value = (TFA_GET_BF(tfa, MANSCONF) == 0);
		}
	} else {
		value = TFA_GET_BF(tfa, ACS);
	}

#if defined(TFA_CHANGE_PCM_FORMAT)
	/* set cold by force if PCM format is changed */
	value |= tfa_is_pcm_format_changed(tfa);
#endif

	return value;
}

int tfa_needs_reset(struct tfa_device *tfa)
{
	int value;

	/* checks if the DSP commands SetAlgoParams and SetMBDrc
	 * need a DSP reset (now: at coldstart or during calibration)
	 */
	if (tfa_is_cold(tfa) == 1 || tfa->needs_reset == 1)
		value = 1;
	else
		value = 0;

	return value;
}

int tfa_is_cold_amp(struct tfa_device *tfa)
{
	int value;

	if (tfa->tfa_family == 2)
		/*
		 * for non-dsp device reading MANSCONF
		 * (src_set_configured) is a way
		 * to check for cold boot status
		 */
		value = (TFA_GET_BF(tfa, MANSCONF) == 0);
	else
		value = TFA_GET_BF(tfa, ACS);

	return value;
}

int tfa_nr_config_done(struct tfa_device *tfa)
{
	int value = 0;

	if (config_count > tfa->cnt->ndev)
		config_count %= tfa->cnt->ndev;

	value = config_count;

	if (tfa->verbose)
		pr_debug("%s: config_done %d\n", __func__, value);

	return value;
}

int tfa_nr_device_set(struct tfa_device *tfa)
{
	int value = 0;

	if (set_count > tfa->cnt->ndev)
		set_count %= tfa->cnt->ndev;

	value = set_count;

	if (tfa->verbose)
		pr_debug("%s: device_set %d\n", __func__, value);

	return value;
}

#if defined(TFA_CHANGE_PCM_FORMAT)
int tfa_is_pcm_format_changed(struct tfa_device *tfa)
{
	int value;

	if (tfa->tdm_config.slln == 0
		|| tfa->tdm_config.ssize == 0
		|| tfa->tdm_config.srcmap == 0xff) {
		pr_debug("%s: no valid config to update\n", __func__);
		return 0;
	}

	value = TFA_GET_BF(tfa, TDMNBCK);
	if (value != tfa->tdm_config.nbck)
		goto is_pcm_format_changed_exit;
	value = TFA_GET_BF(tfa, TDMSLLN);
	if (value != tfa->tdm_config.slln)
		goto is_pcm_format_changed_exit;
	value = TFA_GET_BF(tfa, TDMSSIZE);
	if (value != tfa->tdm_config.ssize)
		goto is_pcm_format_changed_exit;

	switch (tfa->rev & 0xff) {
	case 0x78:
	case 0x74:
	case 0x72:
		value = TFA7x_GET_BF(tfa, TDMSRCMAP);
		if (value != tfa->tdm_config.srcmap)
			goto is_pcm_format_changed_exit;
		break;
	default:
		break;
	}

	return 0;

is_pcm_format_changed_exit:
	pr_info("%s: config is updated for overwriting (dev %d)\n",
		__func__, tfa->dev_idx);

	return 1;
}

void tfa_overwrite_pcm_format(struct tfa_device *tfa)
{
	int value, tdm_state;
	int dirt = 0;

	if (tfa->tdm_config.slln == 0
		|| tfa->tdm_config.ssize == 0
		|| tfa->tdm_config.srcmap == 0xff) {
		pr_debug("%s: no valid config to update\n", __func__);
		return;
	}

	/* disable for reconfig */
	switch (tfa->rev & 0xff) {
	case 0x78:
	case 0x74:
		tdm_state = TFA7x_GET_BF(tfa, TDME);
		if (tdm_state != 0)
			TFA7x_SET_BF(tfa, TDME, 0);
 		break;
	case 0x72:
	default:
		tdm_state = TFA_GET_BF(tfa, TDME);
		if (tdm_state != 0)
			TFA_SET_BF(tfa, TDME, 0);
		break;
	}

	pr_debug("%s: [original] nbck %u, slln %u, ssize %u srcmap %u\n",
		__func__, TFA_GET_BF(tfa, TDMNBCK),
		TFA_GET_BF(tfa, TDMSLLN), TFA_GET_BF(tfa, TDMSSIZE),
		TFA7x_GET_BF(tfa, TDMSRCMAP));

	value = TFA_GET_BF(tfa, TDMNBCK);
	if (value != tfa->tdm_config.nbck) {
		dirt = 1;
		TFA_SET_BF(tfa, TDMNBCK,
			tfa->tdm_config.nbck);
	}
	value = TFA_GET_BF(tfa, TDMSLLN);
	if (value != tfa->tdm_config.slln) {
		dirt = 1;
		TFA_SET_BF(tfa, TDMSLLN,
			tfa->tdm_config.slln);
	}
	value = TFA_GET_BF(tfa, TDMSSIZE);
	if (value != tfa->tdm_config.ssize) {
		dirt = 1;
		TFA_SET_BF(tfa, TDMSSIZE,
			tfa->tdm_config.ssize);
	}

	switch (tfa->rev & 0xff) {
	case 0x78:
	case 0x74:
	case 0x72:
		value = TFA7x_GET_BF(tfa, TDMSRCMAP);
		if (value != tfa->tdm_config.srcmap) {
			dirt = 1;
			TFA7x_SET_BF(tfa, TDMSRCMAP,
				tfa->tdm_config.srcmap);
		}
		break;
	default:
		break;
	}

	if (dirt)
		pr_info("%s: [changed] nbck %u, slln %u, ssize %u, srcmap %u\n",
			__func__, TFA_GET_BF(tfa, TDMNBCK),
			TFA_GET_BF(tfa, TDMSLLN), TFA_GET_BF(tfa, TDMSSIZE),
			TFA7x_GET_BF(tfa, TDMSRCMAP));

	/* restore after config */
	if (tdm_state != 0) {
		switch (tfa->rev & 0xff) {
		case 0x78:
		case 0x74:
			TFA7x_SET_BF(tfa, TDME, 1);
			break;
		case 0x72:
		default:
			TFA_SET_BF(tfa, TDME, 1);
			break;
		}
	}

	return;
}
#endif /* TFA_CHANGE_PCM_FORMAT */

int tfa_cf_enabled(struct tfa_device *tfa)
{
	int value;

	/* For probus, there is no CF */
	if (tfa->is_probus_device)
		value = (tfa->ext_dsp != 0);
	else
		value = TFA_GET_BF(tfa, CFE);

	return value;
}

#define NR_COEFFS 6
#define NR_BIQUADS 28
#define BQ_SIZE (3 * NR_COEFFS)
#define DSP_MSG_OVERHEAD 27

#pragma pack(push, 1)
struct dsp_msg_all_coeff {
	uint8_t select_eq[3];
	uint8_t biquad[NR_BIQUADS][NR_COEFFS][3];
};
#pragma pack(pop)

/* number of biquads for each equalizer */
static const int eq_biquads[] = {
	10, 10, 2, 2, 2, 2
};

#define NR_EQ (int)(sizeof(eq_biquads) / sizeof(int))

enum tfa98xx_error
dsp_partial_coefficients(struct tfa_device *tfa,
	uint8_t *prev, uint8_t *next)
{
	uint8_t bq, eq;
	int eq_offset;
	int new_cost, old_cost;
	uint32_t eq_biquad_mask[NR_EQ];
	enum tfa98xx_error err = TFA98XX_ERROR_OK;
	struct dsp_msg_all_coeff *data1 = (struct dsp_msg_all_coeff *)prev;
	struct dsp_msg_all_coeff *data2 = (struct dsp_msg_all_coeff *)next;

	old_cost = DSP_MSG_OVERHEAD + 3 + sizeof(struct dsp_msg_all_coeff);
	new_cost = 0;

	eq_offset = 0;
	for (eq = 0; eq < NR_EQ; eq++) {
		uint8_t *eq1 = &data1->biquad[eq_offset][0][0];
		uint8_t *eq2 = &data2->biquad[eq_offset][0][0];

		eq_biquad_mask[eq] = 0;

		if (memcmp(eq1, eq2, BQ_SIZE*eq_biquads[eq]) != 0) {
			int nr_bq = 0;
			int bq_sz, eq_sz;

			for (bq = 0; bq < eq_biquads[eq]; bq++) {
				uint8_t *bq1 = &eq1[bq*BQ_SIZE];
				uint8_t *bq2 = &eq2[bq*BQ_SIZE];

				if (memcmp(bq1, bq2, BQ_SIZE) != 0) {
					eq_biquad_mask[eq] |= (1 << bq);
					nr_bq++;
				}
			}

			bq_sz = (2 * 3 + BQ_SIZE) * nr_bq;
			eq_sz = 2 * 3 + BQ_SIZE * eq_biquads[eq];

			/* dsp message i2c transaction overhead */
			bq_sz += DSP_MSG_OVERHEAD * nr_bq;
			eq_sz += DSP_MSG_OVERHEAD;

			if (bq_sz >= eq_sz) {
				eq_biquad_mask[eq] = 0xffffffff;
				new_cost += eq_sz;
			} else {
				new_cost += bq_sz;
			}
		}
		pr_debug("eq_biquad_mask[%d] = 0x%.8x\n",
			eq, eq_biquad_mask[eq]);

		eq_offset += eq_biquads[eq];
	}

	pr_debug("cost for writing all coefficients = %d\n", old_cost);
	pr_debug("cost for writing changed coefficients = %d\n", new_cost);

	if (new_cost >= old_cost) {
		const int buffer_sz = 3 + sizeof(struct dsp_msg_all_coeff);
		uint8_t *buffer;

		buffer = kmalloc(buffer_sz, GFP_KERNEL);
		if (buffer == NULL)
			return TFA98XX_ERROR_FAIL;

		/* cmd id */
		buffer[0] = 0x00;
		buffer[1] = MODULE_BIQUADFILTERBANK + 0x80;
		buffer[2] = 0x00;

		/* parameters */
		memcpy(&buffer[3], data2, sizeof(struct dsp_msg_all_coeff));

		err = dsp_msg(tfa, buffer_sz, (const char *)buffer);

		kfree(buffer);

		return err;
	}

	/* (new_cost < old_cost) */
	eq_offset = 0;
	for (eq = 0; eq < NR_EQ; eq++) {
		uint8_t *eq2 = &data2->biquad[eq_offset][0][0];

		if (eq_biquad_mask[eq] == 0xffffffff) {
			const int msg_sz = 6 + BQ_SIZE * eq_biquads[eq];
			uint8_t *msg;

			msg = kmalloc(msg_sz, GFP_KERNEL);
			if (msg == NULL)
				return TFA98XX_ERROR_FAIL;

			/* cmd id */
			msg[0] = 0x00;
			msg[1] = MODULE_BIQUADFILTERBANK + 0x80;
			msg[2] = 0x00;

			/* select eq and bq */
			msg[3] = 0x00;
			msg[4] = eq + 1;
			msg[5] = 0x00; /* all biquads */

			/* biquad parameters */
			memcpy(&msg[6], eq2, BQ_SIZE * eq_biquads[eq]);

			err = dsp_msg(tfa, msg_sz, (const char *)msg);

			kfree(msg);
			if (err)
				return err;
		} else if (eq_biquad_mask[eq] != 0) {
			const int msg_sz = 6 + BQ_SIZE;
			uint8_t *msg;

			msg = kmem_cache_alloc(tfa->cachep, GFP_KERNEL);
			if (msg == NULL)
				return TFA98XX_ERROR_FAIL;

			for (bq = 0; bq < eq_biquads[eq]; bq++) {
				if (eq_biquad_mask[eq] & (1 << bq)) {
					uint8_t *bq2
						= &eq2[bq * BQ_SIZE];

					/* cmd id */
					msg[0] = 0x00;
					msg[1] = MODULE_BIQUADFILTERBANK + 0x80;
					msg[2] = 0x00;

					/* select eq and bq */
					msg[3] = 0x00;
					msg[4] = eq + 1;
					msg[5] = bq + 1;

					/* biquad parameters */
					memcpy(&msg[6], bq2, BQ_SIZE);

					err = dsp_msg(tfa, msg_sz,
						(const char *)msg);

					if (err) {
						kmem_cache_free(tfa->cachep, msg);
						return err;
					}
				}
			}
			kmem_cache_free(tfa->cachep, msg);
		}
		eq_offset += eq_biquads[eq];
	}

	return err;
}

/* fill context info */
int tfa_dev_probe(int slave, struct tfa_device *tfa)
{
	uint16_t rev;

	tfa->slave_address = (unsigned char)slave;

	/* read revid via low level hal, register 3 */
	if (tfa98xx_read_register16(tfa, 0x03, &rev)
		!= TFA98XX_ERROR_OK) {
		pr_debug("\nError: Unable to read revid from slave:0x%02x\n",
			slave);
		return TFA_ERROR;
	}

	tfa->rev = rev;
	tfa->dev_idx = -1;
	tfa->state = TFA_STATE_UNKNOWN;
	tfa->p_reg_info = NULL;

	tfa_set_query_info(tfa);

	tfa->in_use = 1;

	return 0;
}

enum tfa_error
tfa_dev_set_state(struct tfa_device *tfa,
	enum tfa_state state, int is_calibration)
{
	enum tfa98xx_error ret = TFA98XX_ERROR_OK;
	int loop = 50, ready = 0;
	int count;

	pr_info("%s: state = 0x%02x\n", __func__, state & 0xff);

	/* Base states */
	/* Do not change the order of setting bits as this is important! */
	switch (state & 0x0f) {
	case TFA_STATE_POWERDOWN: /* PLL in powerdown, Algo up */
		break;
	case TFA_STATE_INIT_HW: /* load I2C/PLL hardware setting (~wait2srcsettings) */
		break;
	case TFA_STATE_INIT_CF: /* coolflux HW access possible (~initcf) */
		/* Start with SBSL=0 to stay in initCF state */
		if (!tfa->is_probus_device)
			TFA_SET_BF(tfa, SBSL, 0);

		/* We want to leave Wait4SrcSettings state for max2 */
		if (tfa->tfa_family == 2)
			TFA_SET_BF(tfa, MANSCONF, 1);

		/* And finally set PWDN to 0 to leave powerdown state */
		TFA_SET_BF(tfa, PWDN, 0);

		/* Make sure the DSP is running! */
		do {
			ret = tfa98xx_dsp_system_stable(tfa, &ready);
			if (ret != TFA98XX_ERROR_OK)
				return tfa_error_dsp;
			if (ready)
				break;
		} while (loop--);

		if (((!tfa->is_probus_device) && (is_calibration))
			|| ((tfa->rev & 0xff) == 0x13)) {
			/* Enable FAIM when clock is stable, to avoid MTP corruption */
			ret = tfa98xx_faim_protect(tfa, 1);
			if (tfa->verbose)
				pr_debug("FAIM enabled (err %d).\n", ret);
		}
		break;
	case TFA_STATE_INIT_FW: /* DSP framework active (~patch loaded) */
		break;
	case TFA_STATE_OPERATING: /* Amp and Algo running */
		/* Depending on our previous state we need to set 3 bits */
		TFA_SET_BF(tfa, PWDN, 0);	/* Coming from state 0 */
		TFA_SET_BF(tfa, MANSCONF, 1);	/* Coming from state 1 */
		if (!tfa->is_probus_device)
			TFA_SET_BF(tfa, SBSL, 1);	/* Coming from state 6 */
		else
			TFA_SET_BF(tfa, AMPE, 1);	/* No SBSL for probus device, we set AMPE to 1 */

		/*
		 * Disable MTP clock to protect memory.
		 * However in case of calibration wait for DSP!
		 * (This should be case only during calibration).
		 */
		if (TFA_GET_BF(tfa, MTPOTC) == 1 && tfa->tfa_family == 2) {
			count = MTPEX_WAIT_NTRIES * 4; /* Calibration takes a lot of time */
			while ((TFA_GET_BF(tfa, MTPEX) != 1) && count) {
				msleep_interruptible(BUSLOAD_INTERVAL);
				count--;
			}
		}
		if (((!tfa->is_probus_device) && (is_calibration))
			|| ((tfa->rev & 0xff) == 0x13)) {
			ret = tfa98xx_faim_protect(tfa, 0);
			if (tfa->verbose)
				pr_debug("FAIM disabled (err %d).\n", ret);
		}
		/* Synchonize I/V delay on 96/97 at cold start */
		if (tfa->sync_iv_delay) {
			if (tfa->verbose)
				pr_debug("syncing I/V delay for %x\n",
					(tfa->rev & 0xff));

			/* wait for ACS to be cleared */
			count = ACS_RESET_WAIT_NTRIES;
			while ((TFA_GET_BF(tfa, ACS) == 1) &&
				(count-- > 0))
				msleep_interruptible(BUSLOAD_INTERVAL / 10);

			tfa98xx_dsp_reset(tfa, 1);
			tfa98xx_dsp_reset(tfa, 0);
			tfa->sync_iv_delay = 0;
		}
		break;
	case TFA_STATE_FAULT: /* An alarm or error occurred */
		break;
	case TFA_STATE_RESET: /* I2C reset and ACS set */
		tfa98xx_init(tfa);
		break;
	default:
		if (state & 0x0f)
			return tfa_error_bad_param;
		break;
	}

	/* state modifiers */

	if (state & TFA_STATE_MUTE)
		tfa98xx_set_mute(tfa, TFA98XX_MUTE_AMPLIFIER);

	if (state & TFA_STATE_UNMUTE)
		tfa98xx_set_mute(tfa, TFA98XX_MUTE_OFF);

	/* tfa->state = state; */ /* to correct with real state of device */
	tfa->state = tfa_dev_get_state(tfa);

	return tfa_error_ok;
}

enum tfa_state tfa_dev_get_state(struct tfa_device *tfa)
{
	int cold = 0;
	int manstate;

	/* different per family type */
	if (tfa->tfa_family == 1) {
		cold = TFA_GET_BF(tfa, ACS);
		if (cold && TFA_GET_BF(tfa, PWDN))
			tfa->state = TFA_STATE_RESET;
		else if (!cold && TFA_GET_BF(tfa, SWS))
			tfa->state = TFA_STATE_OPERATING;
	} else /* family 2 */ {
		if (is_94_N2_device(tfa))
			manstate = tfa_get_bf(tfa, TFA9894N2_BF_MANSTATE);
		else
			manstate = TFA_GET_BF(tfa, MANSTATE);

		pr_debug("%s: manstate = %d\n", __func__, manstate);

		switch (manstate) {
		case 0:
			tfa->state = TFA_STATE_POWERDOWN;
			break;
		case 8: /* if dsp reset if off assume framework is running */
			tfa->state = TFA_GET_BF(tfa, RST)
				? TFA_STATE_INIT_CF : TFA_STATE_INIT_FW;
			break;
		case 9:
			tfa->state = TFA_STATE_OPERATING;
			break;
		default:
			tfa->state = TFA_STATE_UNKNOWN;
			break;
		}
	}

	return tfa->state;
}

int tfa_dev_mtp_get(struct tfa_device *tfa, enum tfa_mtp item)
{
	int value = 0;

	switch (item) {
	case TFA_MTP_OTC:
		value = TFA_GET_BF(tfa, MTPOTC);
		break;
	case TFA_MTP_EX:
		value = TFA_GET_BF(tfa, MTPEX);
		break;
	case TFA_MTP_RE25:
	case TFA_MTP_RE25_PRIM:
		if (tfa->tfa_family == 2) {
			if ((tfa->rev & 0xff) == 0x88)
				value = TFA_GET_BF(tfa, R25CL);
			else if ((tfa->rev & 0xff) == 0x13)
				value = tfa_get_bf(tfa, TFA9912_BF_R25C);
			else
				value = TFA_GET_BF(tfa, R25C);
		} else {
			reg_read(tfa, 0x83, (unsigned short*)&value);
		}
		break;
	case TFA_MTP_RE25_SEC:
		if ((tfa->rev & 0xff) == 0x88) {
			value = TFA_GET_BF(tfa, R25CR);
		} else {
			pr_debug("Error: Current device has no secondary Re25 channel\n");
		}
		break;
	case TFA_MTP_LOCK:
		break;
	default:
		/* wrong item */
		break;
	}

	return value;
}

enum tfa_error tfa_dev_mtp_set(struct tfa_device *tfa,
	enum tfa_mtp item, int value)
{
	enum tfa_error err = tfa_error_ok;

	switch (item) {
	case TFA_MTP_OTC:
		err = tfa98xx_set_mtp(tfa,
			(uint16_t)(value << TFA98XX_KEY2_PROTECTED_MTP0_MTPOTC_POS),
			TFA98XX_KEY2_PROTECTED_MTP0_MTPOTC_MSK);
		break;
	case TFA_MTP_EX:
		err = tfa98xx_set_mtp(tfa,
			(uint16_t)(value << TFA98XX_KEY2_PROTECTED_MTP0_MTPEX_POS),
			TFA98XX_KEY2_PROTECTED_MTP0_MTPEX_MSK);
		if (err == tfa_error_ok && value == 0)
			tfa->reset_mtpex = 0;
		break;
	case TFA_MTP_RE25:
	case TFA_MTP_RE25_PRIM:
		if (tfa->tfa_family == 2) {
			tfa98xx_key2(tfa, 0); /* unlock */
			if ((tfa->rev & 0xff) == 0x88)
				TFA_SET_BF(tfa, R25CL, (uint16_t)value);
			else {
				TFA_SET_BF(tfa, R25C, (uint16_t)value);
				if (tfa->is_probus_device == 1
					&& TFA_GET_BF(tfa, MTPOTC) == 1) {
					pr_info("%s: write Re25 to MTP\n", __func__);
					tfa2_manual_mtp_cpy(tfa, 0xf4, value, 2);
				} else
					TFA_SET_BF(tfa, CIMTP, 1);
			}
			tfa98xx_key2(tfa, 1); /* lock */
		}
		break;
	case TFA_MTP_RE25_SEC:
		if ((tfa->rev & 0xff) == 0x88)
			TFA_SET_BF(tfa, R25CR, (uint16_t)value);
		else {
			pr_debug("Error: Current device has no secondary Re25 channel\n");
			err = tfa_error_bad_param;
		}
		break;
	case TFA_MTP_LOCK:
		break;
	default:
		/* wrong item */
		break;
	}

	return err;
}

int tfa_get_pga_gain(struct tfa_device *tfa)
{
	return TFA_GET_BF(tfa, SAAMGAIN);
}

int tfa_set_pga_gain(struct tfa_device *tfa, uint16_t value)
{
	return TFA_SET_BF(tfa, SAAMGAIN, value);
}

int tfa_get_noclk(struct tfa_device *tfa)
{
	return TFA_GET_BF(tfa, NOCLK);
}

enum tfa98xx_error tfa_status(struct tfa_device *tfa)
{
	int value;
	uint16_t val;

	/*
	 * check IC status bits: cold start
	 * and DSP watch dog bit to re init
	 */
	value = TFA_READ_REG(tfa, VDDS); /* STATUSREG */
	if (value < 0)
		return -value;
	val = (uint16_t)value;

	/* pr_debug("SYS_STATUS0: 0x%04x\n", val); */
	if (TFA_GET_BF_VALUE(tfa, ACS, val)
		|| TFA_GET_BF_VALUE(tfa, WDS, val)) {
		if (TFA_GET_BF_VALUE(tfa, ACS, val))
			pr_err("ERROR: ACS\n");
		if (TFA_GET_BF_VALUE(tfa, WDS, val))
			pr_err("ERROR: WDS\n");

		return TFA98XX_ERROR_DSP_NOT_RUNNING;
	}

	if (TFA_GET_BF_VALUE(tfa, SPKS, val))
		pr_err("ERROR: SPKS\n");
	if (!TFA_GET_BF_VALUE(tfa, SWS, val))
		pr_err("ERROR: SWS\n");

	/* Check secondary errors */
	if (!TFA_GET_BF_VALUE(tfa, CLKS, val)
		|| !TFA_GET_BF_VALUE(tfa, UVDS, val)
		|| !TFA_GET_BF_VALUE(tfa, OVDS, val)
		|| !TFA_GET_BF_VALUE(tfa, OTDS, val)
		|| !TFA_GET_BF_VALUE(tfa, PLLS, val)
		|| (!(tfa->daimap & TFA98XX_DAI_TDM)
		&& !TFA_GET_BF_VALUE(tfa, VDDS, val)))
		pr_err("Misc errors detected: STATUS_FLAG0 = 0x%x\n", val);

	if ((tfa->daimap & TFA98XX_DAI_TDM)
		&& (tfa->tfa_family == 2)) {
		value = TFA_READ_REG(tfa, TDMERR); /* STATUS_FLAGS1 */
		if (value < 0)
			return -value;
		val = (uint16_t)value;
		if (TFA_GET_BF_VALUE(tfa, TDMERR, val)
			|| TFA_GET_BF_VALUE(tfa, TDMLUTER, val))
			pr_err("TDM related errors: STATUS_FLAG1 = 0x%x\n", val);
	}

	return TFA98XX_ERROR_OK;
}

/* instance for TFA9874 / TFA9878 */
#if defined(USE_TFA9874) || defined(USE_TFA9878)
enum tfa98xx_error tfa7x_status(struct tfa_device *tfa)
{
	int value;
	uint16_t val;
	int state, control;
	char reg_state[50] = {0};
	int idle_power = 0, low_power = 0, low_noise = 0;

	/*
	 * check IC status bits: cold start
	 * and DSP watch dog bit to re init
	 */
	value = TFA7x_READ_REG(tfa, VDDS); /* STATUS_FLAGS0 */
	if (value < 0)
		return -value;
	val = (uint16_t)value;

	/* Check secondary errors */
	if (!TFA7x_GET_BF_VALUE(tfa, CLKS, val)
		|| !TFA7x_GET_BF_VALUE(tfa, UVDS, val)
		|| !TFA7x_GET_BF_VALUE(tfa, OTDS, val)
		|| (!(tfa->daimap & TFA98XX_DAI_TDM)
		&& !TFA7x_GET_BF_VALUE(tfa, VDDS, val)))
		pr_err("Misc errors detected: STATUS_FLAG0 = 0x%x\n", val);

	value = TFA7x_READ_REG(tfa, PLLS); /* STATUS_FLAGS1 */
	if (value < 0)
		return -value;
	val = (uint16_t)value;

	snprintf(reg_state, 50, "device [%d]", tfa->dev_idx);
	switch (tfa->rev & 0xff) {
#if defined(USE_TFA9878)
	case 0x78:
		state = TFA7x_GET_BF(tfa, LP0);
		snprintf(reg_state, 50, "%s, LP0 %d", reg_state,
			state);
		control = TFA7x_GET_BF(tfa, IPM);
		if ((control == 0x0 || control == 0x3)
			&& (state == 0x1))
			idle_power = 1;
		// FALL-THROUGH
#endif /* USE_TFA9878 */
	case 0x74:
	case 0x72:
		state = TFA7x_GET_BF(tfa, LP1);
		snprintf(reg_state, 50, "%s, LP1 %d", reg_state,
			state);
		control = TFA7x_GET_BF(tfa, LPM1MODE);
		if ((control == 0x0 || control == 0x3)
			&& (state == 0x1))
			low_power = 1;
		state = TFA7x_GET_BF(tfa, LA);
		snprintf(reg_state, 50, "%s, LA %d", reg_state,
			state);
		control = TFA7x_GET_BF(tfa, LNMODE);
		if ((control == 0x0)
			&& (state == 0x1))
			low_noise = 1;
		break;
	default:
		/* neither TFA987x */
		break;
	}
	pr_debug("%s: %s\n", __func__, reg_state);

	if (!TFA7x_GET_BF_VALUE(tfa, SWS, val)) {
		if (idle_power)
			pr_info("%s: idle power disabled amplifier\n", __func__);
		else
			pr_err("%s: ERROR: SWS\n", __func__);
	}
	if (!TFA7x_GET_BF_VALUE(tfa, PLLS, val))
		pr_err("%s: ERROR: PLLS\n", __func__);

	if ((tfa->daimap & TFA98XX_DAI_TDM)
		&& (tfa->tfa_family == 2)) {
			if (TFA7x_GET_BF(tfa, TDMERR)) {
				if ((low_power || idle_power)
					&& TFA7x_GET_BF(tfa, TDMSTAT) == 0x7)
					pr_info("%s: low power disabled sensing block\n",
						__func__);
				else
					pr_err("%s: TDM related errors: STATUS_FLAG0 = 0x%x\n",
						__func__, TFA7x_READ_REG(tfa, TDMERR));
			}
			if (TFA7x_GET_BF(tfa, TDMLUTER))
				pr_err("%s: TDM related errors: STATUS_FLAG1 = 0x%x\n",
					__func__, TFA7x_READ_REG(tfa, TDMLUTER));
	}

	value = TFA7x_READ_REG(tfa, OVDS); /* STATUS_FLAGS3 */
	if (value < 0)
		return -value;
	val = (uint16_t)value;

	if (!TFA7x_GET_BF_VALUE(tfa, OVDS, val))
		pr_err("Misc errors detected: STATUS_FLAG3 = 0x%x\n", val);

	return TFA98XX_ERROR_OK;
}
#endif /* (USE_TFA9874) || (USE_TFA9878) */

int tfa_plop_noise_interrupt(struct tfa_device *tfa,
	int profile, int vstep)
{
	enum tfa98xx_error err;
	int no_clk = 0;
	char prof_name[MAX_CONTROL_NAME] = {0};

	/* Remove sticky bit by reading it once */
	TFA_GET_BF(tfa, NOCLK);

	/* No clock detected */
	if (tfa_irq_get(tfa, tfa9912_irq_stnoclk)) {
		no_clk = TFA_GET_BF(tfa, NOCLK);

		/* Detect for clock is lost! (clock is not stable) */
		if (no_clk == 1) {
			/* Clock is lost. Set I2CR to remove POP noise */
			pr_info("No clock detected. Resetting the I2CR to avoid pop on 72!\n");
			err = (enum tfa98xx_error)
				tfa_dev_start(tfa, profile, vstep);
			if (err != TFA98XX_ERROR_OK)
				pr_err("Error loading i2c registers (tfa_dev_start), err=%d\n",
					err);
			else
				pr_info("Setting i2c registers after I2CR succesfull\n");
				tfa_dev_set_state(tfa, TFA_STATE_UNMUTE, 0);

			/* Remove sticky bit by reading it once */
			tfa_get_noclk(tfa);

			/* This is only for SAAM on the 72.
			 * Since the NOCLK interrupt is only enabled for 72 this is the place
			 * However: Not tested yet! But also does not harm normal flow!
			 */
			strlcpy(prof_name, tfa_cont_profile_name(tfa->cnt,
				tfa->dev_idx, profile), MAX_CONTROL_NAME);
			if (strnstr(prof_name, ".saam", strlen(prof_name))) {
				pr_info("Powering down from a SAAM profile, workaround PLMA4766 used!\n");
				TFA_SET_BF(tfa, PWDN, 1);
				TFA_SET_BF(tfa, AMPE, 0);
				TFA_SET_BF(tfa, SAMMODE, 0);
			}
		}

		/* If clk is stable set polarity to check for LOW (no clock)*/
		tfa_irq_set_pol(tfa, tfa9912_irq_stnoclk, (no_clk == 0));

		/* clear interrupt */
		tfa_irq_clear(tfa, tfa9912_irq_stnoclk);
	}

	/* return no_clk to know we called tfa_dev_start */
	return no_clk;
}

void tfa_lp_mode_interrupt(struct tfa_device *tfa)
{
	/* FIXME: this 72 interrupt does not exist for 9912 */
	const int irq_stclp0 = 36;
	int lp0, lp1;

	if (tfa_irq_get(tfa, irq_stclp0)) {
		lp0 = TFA_GET_BF(tfa, LP0);
		if (lp0 > 0)
			pr_info("lowpower mode 0 detected\n");
		else
			pr_info("lowpower mode 0 not detected\n");

		tfa_irq_set_pol(tfa, irq_stclp0, (lp0 == 0));

		/* clear interrupt */
		tfa_irq_clear(tfa, irq_stclp0);
	}

	if (tfa_irq_get(tfa, tfa9912_irq_stclpr)) {
		lp1 = TFA_GET_BF(tfa, LP1);
		if (lp1 > 0)
			pr_info("lowpower mode 1 detected\n");
		else
			pr_info("lowpower mode 1 not detected\n");

		tfa_irq_set_pol(tfa, tfa9912_irq_stclpr, (lp1 == 0));

		/* clear interrupt */
		tfa_irq_clear(tfa, tfa9912_irq_stclpr);
	}
}

int tfa_ext_event_handler(enum tfadsp_event_en tfadsp_event)
{
	int dirt_flag = 0;

	pr_info("%s: tfadsp event 0x%04x\n", __func__, tfadsp_event);

	if (tfadsp_event & TFADSP_CMD_ACK) {
		/* action for TFADSP_CMD_ACK */
		dirt_flag = 1;
	}
	if (tfadsp_event & TFADSP_SOFT_MUTE_READY) {
		/* action for TFADSP_SOFT_MUTE_READY */
		dirt_flag = 1;
	}
	if (tfadsp_event & TFADSP_VOLUME_READY) {
		/* action for TFADSP_VOLUME_READY */
		dirt_flag = 1;
	}
	if (tfadsp_event & TFADSP_DAMAGED_SPEAKER) {
		/* action for TFADSP_DAMAGED_SPEAKER */
		dirt_flag = 1;
	}
	if (tfadsp_event & TFADSP_CALIBRATE_DONE) {
		/* action for TFADSP_CALIBRATE_DONE */
		dirt_flag = 1;
	}
	if (tfadsp_event & TFADSP_SPARSESIG_DETECTED) {
		/* action for TFADSP_SPARSESIG_DETECTED */
		dirt_flag = 1;
	}
	if (tfadsp_event & TFADSP_CMD_READY) {
		/* action for TFADSP_CMD_READY */
		dirt_flag = 1;
	}
	if (tfadsp_event & TFADSP_EXT_PWRUP) {
		/* action for TFADSP_EXT_PWRUP */
		dirt_flag = 1;
	}
	if (tfadsp_event & TFADSP_EXT_PWRDOWN) {
		/* action for TFADSP_EXT_PWRDOWN */
		dirt_flag = 1;
	}
	if (tfadsp_event & TFADSP_EXT_PWRDOWN) {
		/* action for TFADSP_EXT_PWRDOWN */
		dirt_flag = 1;
	}

	if (!dirt_flag) {
		pr_err("%s: undefined (0x%04x)\n", __func__, tfadsp_event);
	}

	return 0;
}

#if defined(CHECK_CALIBRATION_DATA_RANGE)
#if !defined(LIMIT_CAL_FROM_DTS)
#define TFA_CAL_RANGE(max_devcnt, channel, type, revid) \
	((max_devcnt != 2) ? type##_LIMIT_CAL_##revid : \
	((channel == 0) ? \
	type##_LIMIT_CAL_P_##revid : type##_LIMIT_CAL_S_##revid))
#if defined(USE_TFA9878)
/* mono */
#define LOWER_LIMIT_CAL_N0A 0
#define UPPER_LIMIT_CAL_N0A 32000
#define LOWER_LIMIT_CAL_N1A 0
#define UPPER_LIMIT_CAL_N1A 32000
/* stereo */
#define LOWER_LIMIT_CAL_P_N0A 0
#define UPPER_LIMIT_CAL_P_N0A 32000
#define LOWER_LIMIT_CAL_S_N0A 0
#define UPPER_LIMIT_CAL_S_N0A 32000
#if defined(QPLATFORM)
#if defined(CONFIG_MACH_LAHAINA_BLM)
#define LOWER_LIMIT_CAL_P_N1A 5300
#define UPPER_LIMIT_CAL_P_N1A 7200
#define LOWER_LIMIT_CAL_S_N1A 5300
#define UPPER_LIMIT_CAL_S_N1A 7200
#elif defined(CONFIG_MACH_LAHAINA_RAINBOWLM)
#define LOWER_LIMIT_CAL_P_N1A 5040
#define UPPER_LIMIT_CAL_P_N1A 7560
#define LOWER_LIMIT_CAL_S_N1A 5360
#define UPPER_LIMIT_CAL_S_N1A 8040
#else
#define LOWER_LIMIT_CAL_P_N1A 0 // Top
#define UPPER_LIMIT_CAL_P_N1A 32000 // Top
#define LOWER_LIMIT_CAL_S_N1A 0 // Bottom
#define UPPER_LIMIT_CAL_S_N1A 32000 // Bottom
#endif
#elif defined(MPLATFORM)
#define LOWER_LIMIT_CAL_P_N1A 0 // Top
#define UPPER_LIMIT_CAL_P_N1A 32000 // Top
#define LOWER_LIMIT_CAL_S_N1A 0 // Bottom
#define UPPER_LIMIT_CAL_S_N1A 32000 // Bottom
#else
#define LOWER_LIMIT_CAL_P_N1A 0 // Top
#define UPPER_LIMIT_CAL_P_N1A 32000 // Top
#define LOWER_LIMIT_CAL_S_N1A 0 // Bottom
#define UPPER_LIMIT_CAL_S_N1A 32000 // Bottom
#endif
#endif /* USE_TFA9878 */
#endif /* LIMIT_CAL_FROM_DTS */

static enum tfa98xx_error
tfa_calibration_range_check(struct tfa_device *tfa,
	unsigned int channel, int mohm)
{
	enum tfa98xx_error err = TFA98XX_ERROR_OK;
#if !defined(LIMIT_CAL_FROM_DTS)
	int spkr_count = 0;
#endif
	int lower_limit_cal, upper_limit_cal;

#if defined(LIMIT_CAL_FROM_DTS)
	lower_limit_cal = tfa->lower_limit_cal;
	upper_limit_cal = tfa->upper_limit_cal;
#else
	spkr_count = devcount;

	switch (tfa->rev) {
#if defined(USE_TFA9878)
	case 0x0a78: /* Initial revision ID */
		lower_limit_cal = TFA_CAL_RANGE(spkr_count, channel, LOWER, N0A);
		upper_limit_cal = TFA_CAL_RANGE(spkr_count, channel, UPPER, N0A);
		break;
	case 0x1a78: /* Initial revision ID */
		lower_limit_cal = TFA_CAL_RANGE(spkr_count, channel, LOWER, N1A);
		upper_limit_cal = TFA_CAL_RANGE(spkr_count, channel, UPPER, N1A);
		break;
#endif
	default:
		lower_limit_cal = 0;
		upper_limit_cal = 32000;
		break;
	}
#endif /* LIMIT_CAL_FROM_DTS */

	pr_info("%s: 0x%04x [%d] - calibration range check [%d, %d] with %d\n",
		__func__, tfa->rev, channel,
		lower_limit_cal, upper_limit_cal, mohm);
	if (mohm < lower_limit_cal || mohm > upper_limit_cal)
		err = TFA98XX_ERROR_BAD_PARAMETER;

	return err;
}
#endif /* CHECK_CALIBRATION_DATA_RANGE */

#if defined(TFA_BLACKBOX_LOGGING)
enum tfa98xx_error tfa_configure_log(int enable)
{
	enum tfa98xx_error err;
	struct tfa_device *tfa = NULL;
	uint8_t cmd_buf[3];

	tfa = tfa98xx_get_tfa_device_from_index(0);
	if (tfa == NULL)
		return TFA98XX_ERROR_DEVICE; /* unused device */

	cmd_buf[0] = 0;
	cmd_buf[1] = 0;
	/* 0 - disable logger, 1 - enable logger */
	cmd_buf[2] = (enable) ? 1 : 0;

	pr_info("%s: set blackbox (%d)\n",
		__func__, enable);
	err = tfa_dsp_cmd_id_write
		(tfa, MODULE_SPEAKERBOOST,
		 SB_PARAM_SET_DATA_LOGGER, 3, cmd_buf);
	if (err) {
		pr_err("%s: error in setting blackbox, err = %d\n",
			__func__, err);
		return err;
	}

	tfa->blackbox_enable = enable;

	return TFA98XX_ERROR_OK;
}
EXPORT_SYMBOL(tfa_configure_log);

enum tfa98xx_error tfa_update_log(void)
{
	enum tfa98xx_error err;
	struct tfa_device *tfa = NULL;
	int ndev, idx, offset, i;
#if defined(TFA_CUSTOM_FORMAT_AT_RESPONSE)
	uint8_t cmd_buf[(1 + TFA_LOG_MAX_COUNT * MAX_HANDLES) * 3] = {0};
#else
	uint8_t cmd_buf[TFA_LOG_MAX_COUNT * MAX_HANDLES * 3] = {0};
#endif
	int data[MAX_HANDLES * TFA_LOG_MAX_COUNT];
	int read_size;

	tfa = tfa98xx_get_tfa_device_from_index(0);
	if (tfa == NULL)
		return TFA98XX_ERROR_DEVICE; /* unused device */

	if (!tfa->blackbox_enable) {
		pr_info("%s: blackbox is inactive\n", __func__);
		return TFA98XX_ERROR_OK;
	}

	ndev = tfa->cnt->ndev;
	if (ndev < 1)
		return TFA98XX_ERROR_DEVICE;

#if defined(TFA_CUSTOM_FORMAT_AT_RESPONSE)
	read_size = (1 + TFA_LOG_MAX_COUNT * ndev) * 3;
#else
	read_size = TFA_LOG_MAX_COUNT * ndev * 3;
#endif

	pr_info("%s: read from blackbox\n", __func__);
	err = tfa_dsp_cmd_id_write_read
		(tfa, MODULE_SPEAKERBOOST,
		 SB_PARAM_GET_DATA_LOGGER,
		 read_size, cmd_buf);
	if (err) {
		pr_err("%s: failed to read data from blackbox, err = %d\n",
			__func__, err);
		return err;
	}

	tfa98xx_convert_bytes2data(read_size, cmd_buf, data);

	for (idx = 0; idx < ndev; idx++) {
		offset = idx * TFA_LOG_MAX_COUNT;
		/* maximum x */
		data[offset] = (int)(data[offset] / (TFA2_FW_X_DATA_SCALE / 1000));
		if (tfa->log_data[offset] < data[offset])
			tfa->log_data[offset] = data[offset];

		/* maximum t */
		data[offset + 1] = (int)(data[offset + 1] / TFA2_FW_T_DATA_SCALE);
		if (tfa->log_data[offset + 1] < data[offset + 1])
			tfa->log_data[offset + 1] = data[offset + 1];

		/* counter of x > x_max */
		tfa->log_data[offset + 2] += data[offset + 2];

		/* counter of t > t_max */
		tfa->log_data[offset + 3] += data[offset + 3];

		for (i = 0; i < TFA_LOG_MAX_COUNT; i++)
			pr_info("dev %d - blackbox: data[%d] = %d (<- %d)\n",
				idx, i, tfa->log_data[offset + i], data[offset + i]);
	}

#if defined(TFA_USE_TFALOG_NODE)
	tfa_store_log_to_node(ndev, data);
#endif

	return err;
}
#endif /* TFA_BLACKBOX_LOGGING */


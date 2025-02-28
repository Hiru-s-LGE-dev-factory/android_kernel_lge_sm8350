/*****************************************************************************
    Copyright(c) 2014 FCI Inc. All Rights Reserved

    File name : fc8180_tun.c

    Description : source of FC8180 tuner driver

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

    History :
    ----------------------------------------------------------------------
    V0p1 2014-06-10 Initial Driver
    V0p2 2014-06-26 Block Control ADD
    V0p3 2014-06-27 AGC Control Tunning
    V1p3 2014-07-03
    V1p7 2014-07-14
    V1p9 2014-07-25
    V2p0 2014-08-04
    V2p1 2014-08-12
    S0p3 2014-11-20
    S0p6 2015-01-20
*******************************************************************************/

#include "../inc/fci_types.h"
#include "../inc/fci_oal.h"
#include "../inc/fci_tun.h"
#include "../inc/fc8180_regs.h"
#include "../inc/fci_hal.h"

#define DRIVER_VERSION 6

extern unsigned int freq_xtal;

static u32 reg_table[57][8] = {
    {473143, 0x1f, 0x50, 0xa0, 0x50, 0xa0, 0x50, 0x02},
    {479143, 0x1f, 0x50, 0xa0, 0x50, 0xa0, 0x50, 0x02},
    {485143, 0x1f, 0x50, 0xa0, 0x50, 0xa0, 0x50, 0x02},
    {491143, 0x1f, 0x50, 0xa0, 0x50, 0xa0, 0x50, 0x02},
    {497143, 0x1f, 0x50, 0xa0, 0x50, 0xa0, 0x50, 0x02},
    {503143, 0x1c, 0x50, 0xa0, 0x50, 0xa0, 0x50, 0x02},
    {509143, 0x1b, 0x50, 0xa0, 0x50, 0xa0, 0x50, 0x02},
    {515143, 0x1b, 0x50, 0xa0, 0x50, 0xa0, 0x50, 0x02},
    {521143, 0x1a, 0x50, 0xa0, 0x50, 0xa0, 0x50, 0x02},
    {527143, 0x1a, 0x50, 0xa0, 0x50, 0xa0, 0x50, 0x01},
    {533143, 0x19, 0x50, 0xa0, 0x50, 0xa0, 0x50, 0x01},
    {539143, 0x19, 0x50, 0xa0, 0x50, 0xa0, 0x50, 0x01},
    {545143, 0x1f, 0x50, 0xa0, 0x50, 0xa0, 0x50, 0x01},
    {551143, 0x1f, 0x50, 0xa0, 0x50, 0xa0, 0x50, 0x02},
    {557143, 0x1f, 0x50, 0xa0, 0x50, 0xa0, 0x50, 0x02},
    {563143, 0x1f, 0x50, 0xa0, 0x50, 0xa0, 0x50, 0x01},
    {569143, 0x1f, 0x50, 0xa0, 0x50, 0xa0, 0x50, 0x01},
    {575143, 0x1f, 0x50, 0xa0, 0x50, 0xa0, 0x50, 0x01},
    {581143, 0x1e, 0x50, 0xa0, 0x50, 0xa0, 0x50, 0x01},
    {587143, 0x1e, 0x50, 0xa0, 0x50, 0xa0, 0x50, 0x02},
    {593143, 0x1d, 0x50, 0xa0, 0x50, 0xa0, 0x50, 0x02},
    {599143, 0x1d, 0x50, 0xa0, 0x50, 0xa0, 0x50, 0x02},
    {605143, 0x1c, 0x50, 0xa0, 0x50, 0xa0, 0x50, 0x01},
    {611143, 0x1c, 0x50, 0xa0, 0x50, 0xa0, 0x50, 0x01},
    {617143, 0x1b, 0x50, 0xa0, 0x50, 0xa0, 0x50, 0x01},
    {623143, 0x19, 0x50, 0xa0, 0x50, 0xa0, 0x50, 0x01},
    {629143, 0x15, 0x50, 0x32, 0x14, 0xa0, 0x14, 0x02},
    {635143, 0x15, 0x50, 0x32, 0x14, 0xa0, 0x14, 0x02},
    {641143, 0x15, 0x50, 0x32, 0x14, 0xa0, 0x14, 0x02},
    {647143, 0x15, 0x50, 0x32, 0x14, 0xa0, 0x14, 0x02},
    {653143, 0x15, 0x50, 0x32, 0x14, 0xa0, 0x14, 0x02},
    {659143, 0x15, 0x50, 0x32, 0x14, 0xa0, 0x14, 0x02},
    {665143, 0x15, 0x50, 0x32, 0x14, 0xa0, 0x14, 0x02},
    {671143, 0x10, 0x50, 0x32, 0x14, 0xa0, 0x14, 0x02},
    {677143, 0x10, 0x50, 0x32, 0x14, 0xa0, 0x14, 0x02},
    {683143, 0x10, 0x50, 0x32, 0x14, 0xa0, 0x14, 0x02},
    {689143, 0x10, 0x50, 0x32, 0x14, 0xa0, 0x14, 0x02},
    {695143, 0x0e, 0x74, 0x32, 0x14, 0xa0, 0x14, 0x02},
    {701143, 0x0e, 0x74, 0x32, 0x14, 0xa0, 0x14, 0x02},
    {707143, 0x0e, 0x74, 0x32, 0x14, 0xa0, 0x14, 0x02},
    {713143, 0x04, 0x50, 0x32, 0x14, 0xa0, 0x14, 0x02},
    {719143, 0x04, 0x50, 0x32, 0x14, 0xa0, 0x14, 0x02},
    {725143, 0x04, 0x50, 0x32, 0x14, 0xa0, 0x14, 0x02},
    {731143, 0x03, 0x50, 0x32, 0x14, 0xa0, 0x14, 0x02},
    {737143, 0x03, 0x50, 0x32, 0x14, 0xa0, 0x14, 0x02},
    {743143, 0x03, 0x50, 0x32, 0x14, 0xa0, 0x14, 0x02},
    {749143, 0x03, 0x50, 0x32, 0x14, 0xa0, 0x14, 0x02},
    {755143, 0x02, 0x50, 0x32, 0x14, 0xa0, 0x14, 0x02},
    {761143, 0x02, 0x50, 0x32, 0x14, 0xa0, 0x14, 0x02},
    {767143, 0x02, 0x50, 0x32, 0x14, 0xa0, 0x14, 0x02},
    {773143, 0x01, 0x50, 0x32, 0x14, 0xa0, 0x14, 0x02},
    {779143, 0x01, 0x50, 0x32, 0x14, 0xa0, 0x14, 0x02},
    {785143, 0x01, 0x50, 0x32, 0x14, 0xa0, 0x14, 0x02},
    {791143, 0x00, 0x50, 0x32, 0x14, 0xa0, 0x14, 0x02},
    {797143, 0x00, 0x50, 0x32, 0x14, 0xa0, 0x14, 0x02},
    {803143, 0x00, 0x50, 0x32, 0x14, 0xa0, 0x14, 0x02},
    {809143, 0x00, 0x50, 0x32, 0x14, 0xa0, 0x14, 0x02}
};

static s32 fc8180_write(HANDLE handle, u8 addr, u8 data)
{
    s32 res;

    res = tuner_i2c_write(handle, addr, 1, &data, 1);

    return res;
}

static s32 fc8180_read(HANDLE handle, u8 addr, u8 *data)
{
    s32 res;

    res = tuner_i2c_read(handle, addr, 1, data, 1);

    return res;
}

static u32 fc8180_set_filter(HANDLE handle, u32 ref_clock, u32 filter_bw)
{

    u32 div_cal = 0;
    u8 div = 0;

    div_cal = (378000 / filter_bw) * ref_clock / 10000;

    if ((div_cal % 10) >= 5)
        div = (u8) (div_cal / 10) + 1;
    else
        div = (u8) div_cal / 10;

    return div;

}

s32 fc8180_tuner_init(HANDLE handle, enum BAND_TYPE band)
{
    u32 bw = 900;
    u8 filter_cal = 0;

    fc8180_write(handle, 0x00, 0xff);
    fc8180_write(handle, 0x2a, 0xe9);
    fc8180_write(handle, 0x2f, 0x15);

    fc8180_write(handle, 0x76, 0x88);
    fc8180_write(handle, 0x9b, 0x21);
    fc8180_write(handle, 0x9c, 0x21);

    fc8180_write(handle, 0x68, 0x01);
    fc8180_write(handle, 0x69, 0x83);
    fc8180_write(handle, 0xa5, 0x33);
    fc8180_write(handle, 0xa6, 0x03);
    fc8180_write(handle, 0xa8, 0x01);
    fc8180_write(handle, 0xa9, 0x02);
    fc8180_write(handle, 0xaa, 0x03);
    fc8180_write(handle, 0xab, 0x04);
    fc8180_write(handle, 0xac, 0x0c);
    fc8180_write(handle, 0xad, 0x14);
    fc8180_write(handle, 0xae, 0x34);
    fc8180_write(handle, 0xaf, 0x54);
    fc8180_write(handle, 0xb0, 0x74);
    fc8180_write(handle, 0xb1, 0x94);
    fc8180_write(handle, 0xb2, 0x9c);
    fc8180_write(handle, 0xb3, 0x9c);

    fc8180_write(handle, 0x28, 0x0f);
    fc8180_write(handle, 0x29, 0x1d);
    fc8180_write(handle, 0x2a, 0xf1);
    fc8180_write(handle, 0x2b, 0x10);
    fc8180_write(handle, 0x86, 0x00);
    fc8180_write(handle, 0x8b, 0x00);
    fc8180_write(handle, 0x8d, 0x0f);

    fc8180_write(handle, 0x9a, 0x12);
    fc8180_write(handle, 0xa3, 0x00);
    fc8180_write(handle, 0xca, 0x06);

    /* AGC Block Control */

    print_log(0, "[dtv][dbg] freq_xtal %d\n", freq_xtal);

    if (freq_xtal <= 16000) {
        fc8180_write(handle, 0x95, 0xa1);
        fc8180_write(handle, 0x96, 0x03);
    } else if (freq_xtal <= 17500) {
        fc8180_write(handle, 0x95, 0xa1);
        fc8180_write(handle, 0x96, 0x04);
    } else if (freq_xtal <= 20000) {
        fc8180_write(handle, 0x95, 0xa1);
        fc8180_write(handle, 0x96, 0x05);
    } else if (freq_xtal <= 25500) {
        fc8180_write(handle, 0x95, 0xa1);
        fc8180_write(handle, 0x96, 0x09);
    } else if (freq_xtal <= 26500) {
        fc8180_write(handle, 0x95, 0xa1);
        fc8180_write(handle, 0x96, 0x0a);
    } else if (freq_xtal <= 29000) {
        fc8180_write(handle, 0x95, 0xa1);
        fc8180_write(handle, 0x96, 0x0b);
    } else if (freq_xtal <= 35000) {
        fc8180_write(handle, 0x95, 0xa2);
        fc8180_write(handle, 0x96, 0x03);
    } else {
        fc8180_write(handle, 0x95, 0xa2);
        fc8180_write(handle, 0x96, 0x05);
    }

    fc8180_write(handle, 0xba, 0x01);
    fc8180_write(handle, 0x1d, 0x45);
    fc8180_write(handle, 0x1e, 0x55);
    fc8180_write(handle, 0x1f, 0x45);
    fc8180_write(handle, 0x20, 0x55);
    fc8180_write(handle, 0x23, 0x0b);

    fc8180_write(handle, 0x31, 0x0f);
    fc8180_write(handle, 0x34, 0x17);

    /* AGC Control */
    fc8180_write(handle, 0x38, 0x78);
    fc8180_write(handle, 0x39, 0x32);
    fc8180_write(handle, 0x3a, 0xf2);
    fc8180_write(handle, 0x3b, 0x64);
    fc8180_write(handle, 0x3c, 0x14);
    fc8180_write(handle, 0x3d, 0xf2);
    fc8180_write(handle, 0x3e, 0x00);
    fc8180_write(handle, 0x3f, 0x00);
    fc8180_write(handle, 0x40, 0x00);
    fc8180_write(handle, 0x41, 0x00);
    fc8180_write(handle, 0x42, 0x11);
    fc8180_write(handle, 0x43, 0x11);
    fc8180_write(handle, 0x44, 0x11);
    fc8180_write(handle, 0x45, 0x11);
    fc8180_write(handle, 0x46, 0xff);
    fc8180_write(handle, 0x47, 0x00);
    fc8180_write(handle, 0x48, 0xa0);
    fc8180_write(handle, 0x49, 0x28);
    fc8180_write(handle, 0x4a, 0xff);
    fc8180_write(handle, 0x4b, 0x00);
    fc8180_write(handle, 0x4c, 0xa0);
    fc8180_write(handle, 0x4d, 0x28);
    fc8180_write(handle, 0x4e, 0xb4);
    fc8180_write(handle, 0x4f, 0x14);
    fc8180_write(handle, 0x50, 0xa0);
    fc8180_write(handle, 0x51, 0x50);
    fc8180_write(handle, 0x52, 0x64);
    fc8180_write(handle, 0x53, 0x14);
    fc8180_write(handle, 0x54, 0x3c);
    fc8180_write(handle, 0x55, 0x28);
    fc8180_write(handle, 0x56, 0xff);
    fc8180_write(handle, 0x57, 0x00);
    fc8180_write(handle, 0x58, 0x50);
    fc8180_write(handle, 0x59, 0x14);
    fc8180_write(handle, 0x5a, 0xff);
    fc8180_write(handle, 0x5b, 0x00);
    fc8180_write(handle, 0x5c, 0x50);
    fc8180_write(handle, 0x5d, 0x14);
    fc8180_write(handle, 0x5e, 0x8c);
    fc8180_write(handle, 0x5f, 0x1e);
    fc8180_write(handle, 0x60, 0x3c);
    fc8180_write(handle, 0x61, 0x14);
    fc8180_write(handle, 0x62, 0x64);
    fc8180_write(handle, 0x63, 0x0f);
    fc8180_write(handle, 0x64, 0x3c);
    fc8180_write(handle, 0x65, 0x14);
    fc8180_write(handle, 0xb5, 0x88);
    fc8180_write(handle, 0xb6, 0xd3);
    fc8180_write(handle, 0xb7, 0xce);
    fc8180_write(handle, 0xb8, 0x08);
    fc8180_write(handle, 0xbb, 0xb5);
    fc8180_write(handle, 0xbc, 0xb0);
    fc8180_write(handle, 0xbd, 0x00);
    fc8180_write(handle, 0xc5, 0x2b);
    fc8180_write(handle, 0xc6, 0x00);
    fc8180_write(handle, 0xc7, 0x70);
    fc8180_write(handle, 0xcc, 0x11);
    fc8180_write(handle, 0xcd, 0x11);

    fc8180_write(handle, 0x71, 0x75);
    fc8180_write(handle, 0x72, 0x04);
    fc8180_write(handle, 0x7a, 0x6a);
    fc8180_write(handle, 0x7b, 0x4d);
    fc8180_write(handle, 0x7c, 0x4e);
    fc8180_write(handle, 0x7d, 0x3d);
    fc8180_write(handle, 0x7e, 0x2e);
    fc8180_write(handle, 0x7f, 0x2d);
    fc8180_write(handle, 0xf4, 0x21);
    fc8180_write(handle, 0xf5, 0x71);

    fc8180_write(handle, 0xfc, DRIVER_VERSION);

    fc8180_write(handle, 0x02, 0x01);
    fc8180_write(handle, 0x66, 0x03);

    filter_cal = fc8180_set_filter(handle, freq_xtal, bw);

    fc8180_write(handle, 0x17, filter_cal);

    fc8180_write(handle, 0x66, 0x00);

    ms_wait(10);

    fc8180_write(handle, 0x3e, 0x1d);
    fc8180_write(handle, 0x3f, 0x19);
    fc8180_write(handle, 0x40, 0x1d);
    fc8180_write(handle, 0x41, 0x19);

#ifdef BBM_EXT_LNA
    fc8180_write(handle, 0xbe, 0x53);
    fc8180_write(handle, 0xc2, 0xb9);
    fc8180_write(handle, 0xc3, 0xd7);
    fc8180_write(handle, 0xf3, 0x0f);
    fc8180_write(handle, 0xf5, 0x7a);
#endif

    return BBM_OK;
}

s32 fc8180_set_freq(HANDLE handle, u32 freq)
{
    u32 f_diff, f_diff_shift;
    u32 n_val, k_val;
    u32 f_vco, f_comp;
    u8 lock_detect[5] = {0, 0, 0, 0, 0};
    u8 init_bank = 0;
    u8 div_ratio = 4;
    u8 k0 = 1;
    u8 ref_div = 1;
    u8 i = 0;
    u8 z = 0;

    for (i = 0; i < 57; i++) {
        if (((reg_table[i][0] + 3000) > freq) &&
            ((reg_table[i][0] - 3000) <= freq))
            break;
    }

    if (i == 57)
    {
        print_log(0, "[FC8180] It's not correct freq \n");
        return BBM_NOK;
    }

    fc8180_write(handle, 0x67, reg_table[i][1]);
    fc8180_write(handle, 0xc7, reg_table[i][2]);
    fc8180_write(handle, 0x4c, reg_table[i][3]);
    fc8180_write(handle, 0x4d, reg_table[i][4]);
    fc8180_write(handle, 0x48, reg_table[i][5]);
    fc8180_write(handle, 0x49, reg_table[i][6]);

    if (freq < 596000) {
        fc8180_write(handle, 0x86, 0x00);
        fc8180_write(handle, 0x8e, 0x08);
    } else if (freq < 710000) {
        fc8180_write(handle, 0x86, 0x01);
        fc8180_write(handle, 0x8e, 0x0e);
    } else if (freq < 776000) {
        fc8180_write(handle, 0x86, 0x00);
        fc8180_write(handle, 0x8e, 0x0e);
    } else {
        fc8180_write(handle, 0x86, 0x00);
        fc8180_write(handle, 0x8e, 0x08);
    }

    if (freq < 710000)
        fc8180_write(handle, 0xc5, 0x25);
    else
        fc8180_write(handle, 0xc5, 0x2b);

    if (freq >= 694000 && freq <= 710000) {
        fc8180_write(handle, 0xcf, 0x44);
        fc8180_write(handle, 0x7c, 0x4e);
        fc8180_write(handle, 0x7e, 0x4e);

        fc8180_write(handle, 0x60, 0x64);
        fc8180_write(handle, 0x61, 0x32);
    } else {
        fc8180_write(handle, 0xcf, 0x54);
        fc8180_write(handle, 0x7c, 0x4e);
        fc8180_write(handle, 0x7e, 0x2e);

        fc8180_write(handle, 0x60, 0x3c);
        fc8180_write(handle, 0x61, 0x14);
    }

    fc8180_write(handle, 0x8b, 0x00);
    fc8180_write(handle, 0x8c, 0x00);

    f_vco = freq * div_ratio;
    f_comp = freq_xtal / ref_div;
    n_val = (f_vco / f_comp);
    f_diff = f_vco - (f_comp * n_val);
    f_diff_shift = f_diff << 16;
    k_val = (f_diff_shift / (f_comp >> 4)) | k0;

    init_bank = (u8) ((34 * (((f_vco / 1000) - 3500) * ((f_vco / 1000) -
                        3500))) / 1000000) + 7;

    if (init_bank > 255)
        fc8180_write(handle, 0x37, 0xff);
    else
        fc8180_write(handle, 0x37, init_bank);

    if (freq <= 564000)
        fc8180_write(handle, 0x90, 0x0f);
    else if (freq <= 660000)
        fc8180_write(handle, 0x90, 0x0d);
    else if (freq <= 690000)
        fc8180_write(handle, 0x90, 0x0b);
    else
        fc8180_write(handle, 0x90, 0x09);

    fc8180_write(handle, 0x84, ref_div - 1);
    fc8180_write(handle, 0x87, (u8) ((k_val >> 16) & 0xff));
    fc8180_write(handle, 0x88, (u8) ((k_val >> 8) & 0xff));
    fc8180_write(handle, 0x89, (u8) (k_val & 0xff));
    fc8180_write(handle, 0x8a, (u8) n_val);

    fc8180_write(handle, 0x03, 0x04);
    fc8180_write(handle, 0x03, 0x00);

    fc8180_write(handle, 0x76, 0x44);
    fc8180_write(handle, 0xa3, 0x01);

    for (i = 0; i < 10; i++) {
        fc8180_read(handle, 0xd2, &lock_detect[0]);
        fc8180_read(handle, 0xd2, &lock_detect[1]);
        fc8180_read(handle, 0xd2, &lock_detect[2]);
        fc8180_read(handle, 0xd2, &lock_detect[3]);
        fc8180_read(handle, 0xd2, &lock_detect[4]);

        for (z = 0; z < 5; z++)
            lock_detect[0] &= lock_detect[z];

        if ((lock_detect[0] & 0x01) != 0x01) {
            if (freq <= 564000)
                fc8180_write(handle, 0x90, 0x0f + (i * 2));
            else if (freq <= 660000)
                fc8180_write(handle, 0x90, 0x0d + (i * 2));
            else if (freq <= 690000)
                fc8180_write(handle, 0x90, 0x0b + (i * 2));
            else
                fc8180_write(handle, 0x90, 0x09 + (i * 2));
        } else {
            break;
        }
    }

    if (freq_xtal == 19200) {
        fc8180_write(handle, 0x8b, reg_table[i][7]);
        fc8180_write(handle, 0x8c, 0x7f);
    } else if (freq_xtal == 32000) {
        fc8180_write(handle, 0x8b, 0x01);
        fc8180_write(handle, 0x8c, 0x7c);
    }

    return BBM_OK;
}

u32 fc8180_get_rssi(HANDLE handle, s32 *rssi)
{
    s8 tmp = 0;
    u8 adj = 0;

    fc8180_read(handle, 0xf0, &adj);
    fc8180_read(handle, 0xf8, &tmp);
    *rssi = tmp;

    if (adj > 5) {
        if (tmp < 0)
            *rssi -= 3;
    } else {
        if (tmp < 0)
            *rssi += 1;
    }

    return BBM_OK;
}

s32 fc8180_tuner_deinit(HANDLE handle)
{
    return BBM_OK;
}


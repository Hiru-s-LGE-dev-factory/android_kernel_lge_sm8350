/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <sound/soc.h>

/* FE dai-links */
SND_SOC_DAILINK_DEFS(multimedia1,
	DAILINK_COMP_ARRAY(COMP_CPU("MultiMedia1")),
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-dsp.0")));

SND_SOC_DAILINK_DEFS(multimedia2,
	DAILINK_COMP_ARRAY(COMP_CPU("MultiMedia2")),
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-dsp.0")));

SND_SOC_DAILINK_DEFS(voicemmode1,
	DAILINK_COMP_ARRAY(COMP_CPU("VoiceMMode1")),
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-voice")));

SND_SOC_DAILINK_DEFS(msmvoip,
	DAILINK_COMP_ARRAY(COMP_CPU("VoIP")),
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-voip-dsp")));

SND_SOC_DAILINK_DEFS(multimedia3,
	DAILINK_COMP_ARRAY(COMP_CPU("MultiMedia3")),
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-dsp.2")));

SND_SOC_DAILINK_DEFS(afepcm_rx,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-dev.241")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-afe")));

SND_SOC_DAILINK_DEFS(afepcm_tx,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-dev.240")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-tx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-afe")));

SND_SOC_DAILINK_DEFS(multimedia4,
	DAILINK_COMP_ARRAY(COMP_CPU("MultiMedia4")),
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-compress-dsp")));

SND_SOC_DAILINK_DEFS(auxpcm_hostless,
	DAILINK_COMP_ARRAY(COMP_CPU("AUXPCM_HOSTLESS")),
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-hostless")));

SND_SOC_DAILINK_DEFS(multimedia5,
	DAILINK_COMP_ARRAY(COMP_CPU("MultiMedia5")),
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-dsp.1")));

SND_SOC_DAILINK_DEFS(listen1,
	DAILINK_COMP_ARRAY(COMP_CPU("LSM1")),
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-lsm-client")));

SND_SOC_DAILINK_DEFS(multimedia7,
	DAILINK_COMP_ARRAY(COMP_CPU("MultiMedia7")),
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-compress-dsp")));

SND_SOC_DAILINK_DEFS(multimedia10,
	DAILINK_COMP_ARRAY(COMP_CPU("MultiMedia10")),
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-dsp.1")));

SND_SOC_DAILINK_DEFS(multimedia8,
	DAILINK_COMP_ARRAY(COMP_CPU("MultiMedia8")),
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-dsp-noirq")));

#ifdef CONFIG_MACH_LGE
SND_SOC_DAILINK_DEFS(pri_mi2s_rx_hostless,
	DAILINK_COMP_ARRAY(COMP_CPU("PRI_MI2S_RX_HOSTLESS")),
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-hostless")));
#endif

SND_SOC_DAILINK_DEFS(hdmi_rx_hostless,
	DAILINK_COMP_ARRAY(COMP_CPU("HDMI_HOSTLESS")),
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-hostless")));

SND_SOC_DAILINK_DEFS(display_port_hostless,
	DAILINK_COMP_ARRAY(COMP_CPU("DISPLAY_PORT_HOSTLESS")),
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-hostless")));

SND_SOC_DAILINK_DEFS(voicemmode2,
	DAILINK_COMP_ARRAY(COMP_CPU("VoiceMMode2")),
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-voice")));

SND_SOC_DAILINK_DEFS(listen2,
	DAILINK_COMP_ARRAY(COMP_CPU("LSM2")),
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-lsm-client")));

SND_SOC_DAILINK_DEFS(listen3,
	DAILINK_COMP_ARRAY(COMP_CPU("LSM3")),
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-lsm-client")));

SND_SOC_DAILINK_DEFS(listen4,
	DAILINK_COMP_ARRAY(COMP_CPU("LSM4")),
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-lsm-client")));

SND_SOC_DAILINK_DEFS(listen5,
	DAILINK_COMP_ARRAY(COMP_CPU("LSM5")),
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-lsm-client")));

SND_SOC_DAILINK_DEFS(listen6,
	DAILINK_COMP_ARRAY(COMP_CPU("LSM6")),
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-lsm-client")));

SND_SOC_DAILINK_DEFS(listen7,
	DAILINK_COMP_ARRAY(COMP_CPU("LSM7")),
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-lsm-client")));

SND_SOC_DAILINK_DEFS(listen8,
	DAILINK_COMP_ARRAY(COMP_CPU("LSM8")),
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-lsm-client")));

SND_SOC_DAILINK_DEFS(multimedia9,
	DAILINK_COMP_ARRAY(COMP_CPU("MultiMedia9")),
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-dsp.0")));

SND_SOC_DAILINK_DEFS(multimedia11,
	DAILINK_COMP_ARRAY(COMP_CPU("MultiMedia11")),
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-compress-dsp")));

SND_SOC_DAILINK_DEFS(multimedia12,
	DAILINK_COMP_ARRAY(COMP_CPU("MultiMedia12")),
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-compress-dsp")));

SND_SOC_DAILINK_DEFS(multimedia13,
	DAILINK_COMP_ARRAY(COMP_CPU("MultiMedia13")),
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-compress-dsp")));

SND_SOC_DAILINK_DEFS(multimedia14,
	DAILINK_COMP_ARRAY(COMP_CPU("MultiMedia14")),
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-compress-dsp")));

SND_SOC_DAILINK_DEFS(multimedia15,
	DAILINK_COMP_ARRAY(COMP_CPU("MultiMedia15")),
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-compress-dsp")));

SND_SOC_DAILINK_DEFS(multimedia16,
	DAILINK_COMP_ARRAY(COMP_CPU("MultiMedia16")),
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-dsp-noirq")));

SND_SOC_DAILINK_DEFS(cdcdma_hostless,
	DAILINK_COMP_ARRAY(COMP_CPU("CDC_DMA_HOSTLESS")),
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-hostless")));

SND_SOC_DAILINK_DEFS(tx3_cdcdma_hostless,
	DAILINK_COMP_ARRAY(COMP_CPU("TX3_CDC_DMA_HOSTLESS")),
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-hostless")));

SND_SOC_DAILINK_DEFS(tert_mi2s_tx_hostless,
	DAILINK_COMP_ARRAY(COMP_CPU("TERT_MI2S_TX_HOSTLESS")),
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-hostless")));

SND_SOC_DAILINK_DEFS(wsa_cdcdma0_capture,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-cdc-dma-dev.45057")),
#ifdef CONFIG_WSA_MACRO
	DAILINK_COMP_ARRAY(COMP_CODEC("bolero_codec", "wsa_macro_vifeedback")),
#else
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
#endif
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-hostless")));

SND_SOC_DAILINK_DEFS(wsa_cdcdma0_capture_stub,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-cdc-dma-dev.45057")),
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-hostless")));

SND_SOC_DAILINK_DEFS(multimedia6,
	DAILINK_COMP_ARRAY(COMP_CPU("MultiMedia6")),
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-loopback")));

SND_SOC_DAILINK_DEFS(usbaudio_hostless,
	DAILINK_COMP_ARRAY(COMP_CPU("USBAUDIO_HOSTLESS")),
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-hostless")));

SND_SOC_DAILINK_DEFS(slimbus7_hostless,
	DAILINK_COMP_ARRAY(COMP_CPU("SLIMBUS7_HOSTLESS")),
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-hostless")));

SND_SOC_DAILINK_DEFS(multimedia17,
	DAILINK_COMP_ARRAY(COMP_CPU("MultiMedia17")),
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-compress-dsp")));

SND_SOC_DAILINK_DEFS(slimbus8_hostless,
	DAILINK_COMP_ARRAY(COMP_CPU("SLIMBUS8_HOSTLESS")),
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-hostless")));

SND_SOC_DAILINK_DEFS(tx_cdcdma5_tx,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-cdc-dma-dev.45115")),
	DAILINK_COMP_ARRAY(COMP_CODEC("bolero_codec", "tx_macro_tx3"),
			   COMP_CODEC("wcd938x_codec", "wcd938x_cdc"),
			   COMP_CODEC("wcd937x_codec", "wcd937x_cdc")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-hostless")));

SND_SOC_DAILINK_DEFS(multimedia31,
	DAILINK_COMP_ARRAY(COMP_CPU("MultiMedia31")),
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-dsp.0")));

SND_SOC_DAILINK_DEFS(multimedia32,
	DAILINK_COMP_ARRAY(COMP_CPU("MultiMedia32")),
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-dsp.0")));

SND_SOC_DAILINK_DEFS(afepcm_tx1,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-dev.242")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-tx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-afe")));

SND_SOC_DAILINK_DEFS(multimedia20,
	DAILINK_COMP_ARRAY(COMP_CPU("MultiMedia20")),
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-loopback")));

SND_SOC_DAILINK_DEFS(multimedia21,
	DAILINK_COMP_ARRAY(COMP_CPU("MultiMedia21")),
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-loopback")));

SND_SOC_DAILINK_DEFS(pri_tdm_rx_0_hostless,
	DAILINK_COMP_ARRAY(COMP_CPU("PRI_TDM_RX_0_HOSTLESS")),
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-hostless")));

SND_SOC_DAILINK_DEFS(pri_tdm_tx_0_hostless,
	DAILINK_COMP_ARRAY(COMP_CPU("PRI_TDM_TX_0_HOSTLESS")),
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-hostless")));

SND_SOC_DAILINK_DEFS(sec_tdm_rx_0_hostless,
	DAILINK_COMP_ARRAY(COMP_CPU("SEC_TDM_RX_0_HOSTLESS")),
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-hostless")));

SND_SOC_DAILINK_DEFS(sec_tdm_tx_0_hostless,
	DAILINK_COMP_ARRAY(COMP_CPU("SEC_TDM_TX_0_HOSTLESS")),
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-hostless")));

SND_SOC_DAILINK_DEFS(tert_tdm_rx_0_hostless,
	DAILINK_COMP_ARRAY(COMP_CPU("TERT_TDM_RX_0_HOSTLESS")),
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-hostless")));

SND_SOC_DAILINK_DEFS(tert_tdm_tx_0_hostless,
	DAILINK_COMP_ARRAY(COMP_CPU("TERT_TDM_TX_0_HOSTLESS")),
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-hostless")));

SND_SOC_DAILINK_DEFS(quat_tdm_rx_0_hostless,
	DAILINK_COMP_ARRAY(COMP_CPU("QUAT_TDM_RX_0_HOSTLESS")),
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-hostless")));

SND_SOC_DAILINK_DEFS(quat_tdm_tx_0_hostless,
	DAILINK_COMP_ARRAY(COMP_CPU("QUAT_TDM_TX_0_HOSTLESS")),
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-hostless")));

SND_SOC_DAILINK_DEFS(quat_mi2s_rx_hostless,
	DAILINK_COMP_ARRAY(COMP_CPU("QUAT_MI2S_RX_HOSTLESS")),
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-hostless")));

SND_SOC_DAILINK_DEFS(sec_mi2s_tx_hostless,
	DAILINK_COMP_ARRAY(COMP_CPU("SEC_MI2S_TX_HOSTLESS")),
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-hostless")));

SND_SOC_DAILINK_DEFS(dtmf_rx_hostless,
	DAILINK_COMP_ARRAY(COMP_CPU("DTMF_RX_HOSTLESS")),
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-dtmf")));

SND_SOC_DAILINK_DEFS(sec_tdm_rx_7_hostless,
	DAILINK_COMP_ARRAY(COMP_CPU("SEC_TDM_RX_7_HOSTLESS")),
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-hostless")));

SND_SOC_DAILINK_DEFS(tert_tdm_tx_7_hostless,
	DAILINK_COMP_ARRAY(COMP_CPU("TERT_TDM_TX_7_HOSTLESS")),
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-hostless")));

SND_SOC_DAILINK_DEFS(quat_tdm_rx_7_hostless,
	DAILINK_COMP_ARRAY(COMP_CPU("QUAT_TDM_RX_7_HOSTLESS")),
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-hostless")));

SND_SOC_DAILINK_DEFS(quat_tdm_tx_7_hostless,
	DAILINK_COMP_ARRAY(COMP_CPU("QUAT_TDM_TX_7_HOSTLESS")),
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-hostless")));

SND_SOC_DAILINK_DEFS(quin_tdm_rx_7_hostless,
	DAILINK_COMP_ARRAY(COMP_CPU("QUIN_TDM_RX_7_HOSTLESS")),
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-hostless")));

SND_SOC_DAILINK_DEFS(quin_tdm_tx_7_hostless,
	DAILINK_COMP_ARRAY(COMP_CPU("QUIN_TDM_TX_7_HOSTLESS")),
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-hostless")));

SND_SOC_DAILINK_DEFS(multimedia22,
	DAILINK_COMP_ARRAY(COMP_CPU("MultiMedia22")),
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-dsp.0")));

SND_SOC_DAILINK_DEFS(multimedia23,
	DAILINK_COMP_ARRAY(COMP_CPU("MultiMedia23")),
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-dsp.1")));

SND_SOC_DAILINK_DEFS(multimedia24,
	DAILINK_COMP_ARRAY(COMP_CPU("MultiMedia24")),
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-loopback")));

SND_SOC_DAILINK_DEFS(multimedia25,
	DAILINK_COMP_ARRAY(COMP_CPU("MultiMedia25")),
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-loopback")));

SND_SOC_DAILINK_DEFS(multimedia33,
	DAILINK_COMP_ARRAY(COMP_CPU("MultiMedia33")),
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-dsp.0")));

SND_SOC_DAILINK_DEFS(multimedia34,
	DAILINK_COMP_ARRAY(COMP_CPU("MultiMedia34")),
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-dsp.0")));

SND_SOC_DAILINK_DEFS(int_hfp_bt_hostless,
	DAILINK_COMP_ARRAY(COMP_CPU("INT_HFP_BT_HOSTLESS")),
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-hostless")));

SND_SOC_DAILINK_DEFS(ll_loopback,
	DAILINK_COMP_ARRAY(COMP_CPU("MultiMedia9")),
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-loopback.1")));

SND_SOC_DAILINK_DEFS(multimedia36,
	DAILINK_COMP_ARRAY(COMP_CPU("MultiMedia6")),
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-dsp.1")));

SND_SOC_DAILINK_DEFS(multimedia38,
	DAILINK_COMP_ARRAY(COMP_CPU("MultiMedia8")),
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-dsp.1")));

SND_SOC_DAILINK_DEFS(multimedia39,
	DAILINK_COMP_ARRAY(COMP_CPU("MultiMedia9")),
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-dsp.1")));

/* BE dai-links */
SND_SOC_DAILINK_DEFS(afe_pcm_rx,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-dev.224")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(afe_pcm_rx1,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-dev.226")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(afe_pcm_tx,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-dev.225")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-tx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(incall_record_tx,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-dev.32772")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-tx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(incall_record_rx,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-dev.32771")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-tx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(voice_playback_tx,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-dev.32773")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(voice2_playback_tx,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-dev.32770")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(usb_audio_rx,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-dev.28672")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(usb_audio_tx,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-dev.28673")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-tx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(pri_tdm_rx_0,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.36864")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(pri_tdm_tx_0,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.36865")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-tx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(sec_tdm_rx_0,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.36880")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(sec_tdm_tx_0,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.36881")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-tx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(tert_tdm_rx_0,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.36896")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(tert_tdm_tx_0,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.36897")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-tx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(quat_tdm_rx_0,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.36912")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(quat_tdm_tx_0,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.36913")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-tx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(quin_tdm_rx_0,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.36928")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(quin_tdm_tx_0,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.36929")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-tx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(sen_tdm_rx_0,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.36944")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(sen_tdm_rx_1,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.36946")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(sen_tdm_rx_2,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.36948")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(sen_tdm_rx_3,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.36950")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(sen_tdm_rx_4,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.36952")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(sen_tdm_rx_5,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.36954")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(sen_tdm_rx_6,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.36956")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(sen_tdm_rx_7,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.36958")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(sen_tdm_tx_0,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.36945")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-tx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(sen_tdm_tx_1,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.36947")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-tx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(sen_tdm_tx_2,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.36949")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-tx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(sen_tdm_tx_3,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.36951")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-tx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(sen_tdm_tx_4,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.36953")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-tx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(sen_tdm_tx_5,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.36955")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-tx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(sen_tdm_tx_6,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.36957")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-tx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(sen_tdm_tx_7,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.36959")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-tx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(sep_tdm_rx_0,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.36960")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(sep_tdm_rx_1,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.36962")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(sep_tdm_rx_2,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.36964")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(sep_tdm_rx_3,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.36966")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(sep_tdm_rx_4,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.36968")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(sep_tdm_rx_5,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.36970")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(sep_tdm_rx_6,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.36972")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(sep_tdm_rx_7,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.36974")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(sep_tdm_tx_0,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.36961")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-tx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(sep_tdm_tx_1,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.36963")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-tx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(sep_tdm_tx_2,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.36965")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-tx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(sep_tdm_tx_3,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.36967")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-tx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(sep_tdm_tx_4,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.36969")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-tx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(sep_tdm_tx_5,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.36971")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-tx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(sep_tdm_tx_6,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.36973")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-tx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(sep_tdm_tx_7,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.36975")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-tx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(hsif0_tdm_rx_0,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.36976")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(hsif0_tdm_rx_1,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.36978")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(hsif0_tdm_rx_2,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.36980")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(hsif0_tdm_rx_3,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.36982")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(hsif0_tdm_rx_4,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.36984")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(hsif0_tdm_rx_5,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.36986")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(hsif0_tdm_rx_6,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.36988")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(hsif0_tdm_rx_7,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.36990")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(hsif0_tdm_tx_0,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.36977")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-tx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(hsif0_tdm_tx_1,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.36979")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-tx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(hsif0_tdm_tx_2,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.36981")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-tx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(hsif0_tdm_tx_3,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.36983")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-tx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(hsif0_tdm_tx_4,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.36985")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-tx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(hsif0_tdm_tx_5,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.36987")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-tx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(hsif0_tdm_tx_6,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.36989")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-tx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(hsif0_tdm_tx_7,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.36991")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-tx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(hsif1_tdm_rx_0,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.36992")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(hsif1_tdm_rx_1,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.36994")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(hsif1_tdm_rx_2,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.36996")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(hsif1_tdm_rx_3,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.36998")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(hsif1_tdm_rx_4,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.37000")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(hsif1_tdm_rx_5,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.37002")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(hsif1_tdm_rx_6,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.37004")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(hsif1_tdm_rx_7,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.37006")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(hsif1_tdm_tx_0,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.36993")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-tx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(hsif1_tdm_tx_1,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.36995")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-tx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(hsif1_tdm_tx_2,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.36997")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-tx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(hsif1_tdm_tx_3,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.36999")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-tx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(hsif1_tdm_tx_4,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.37001")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-tx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(hsif1_tdm_tx_5,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.37003")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-tx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(hsif1_tdm_tx_6,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.37005")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-tx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(hsif1_tdm_tx_7,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.37007")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-tx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(hsif2_tdm_rx_0,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.37008")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(hsif2_tdm_rx_1,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.37010")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(hsif2_tdm_rx_2,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.37012")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(hsif2_tdm_rx_3,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.37014")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(hsif2_tdm_rx_4,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.37016")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(hsif2_tdm_rx_5,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.37018")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(hsif2_tdm_rx_6,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.37020")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(hsif2_tdm_rx_7,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.37022")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(hsif2_tdm_tx_0,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.37009")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-tx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(hsif2_tdm_tx_1,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.37011")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-tx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(hsif2_tdm_tx_2,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.37013")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-tx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(hsif2_tdm_tx_3,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.37015")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-tx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(hsif2_tdm_tx_4,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.37017")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-tx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(hsif2_tdm_tx_5,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.37019")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-tx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(hsif2_tdm_tx_6,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.37021")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-tx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(hsif2_tdm_tx_7,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.37023")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-tx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(wsa_dma_tx0_vi,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-cdc-dma-dev.45057")),
	DAILINK_COMP_ARRAY(COMP_CODEC("bolero_codec", "wsa_macro_vifeedback")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(slimbus_7_rx,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-dev.16398")),
	DAILINK_COMP_ARRAY(COMP_CODEC("btfmslim_slave",
			"btfm_bt_sco_a2dp_slim_rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(slimbus_7_tx,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-dev.16399")),
	DAILINK_COMP_ARRAY(COMP_CODEC("btfmslim_slave",
			"btfm_bt_sco_slim_tx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(slimbus_8_tx,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-dev.16401")),
	DAILINK_COMP_ARRAY(COMP_CODEC("btfmslim_slave",
			"btfm_fm_slim_tx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(display_port,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-dp.0")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-ext-disp-audio-codec-rx",
			"msm_dp_audio_codec_rx_dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(display_port1,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-dp.1")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-ext-disp-audio-codec-rx",
			"msm_dp_audio_codec_rx1_dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));
#if defined(CONFIG_SND_SOC_TFA9878)
SND_SOC_DAILINK_DEFS(pri_mi2s_rx,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-mi2s.0")),
	DAILINK_COMP_ARRAY(COMP_CODEC("tfa98xx-codec.0034", "tfa98xx-aif-34"),COMP_CODEC("tfa98xx-codec.0035", "tfa98xx-aif-35")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(pri_mi2s_tx,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-mi2s.0")),
	DAILINK_COMP_ARRAY(COMP_CODEC("tfa98xx-codec.0034", "tfa98xx-aif-34"),COMP_CODEC("tfa98xx-codec.0035", "tfa98xx-aif-35")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-hostless")));
#elif defined(CONFIG_SND_SOC_CS35L41)
SND_SOC_DAILINK_DEFS(pri_mi2s_rx,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-mi2s.0")),
	DAILINK_COMP_ARRAY(COMP_CODEC("cs35l41-codec.3-0040", "cs35l41-pcm"),COMP_CODEC("cs35l41-codec.3-0041", "cs35l41-pcm")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(pri_mi2s_tx,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-mi2s.0")),
	DAILINK_COMP_ARRAY(COMP_CODEC("cs35l41-codec.3-0040", "cs35l41-pcm"),COMP_CODEC("cs35l41-codec.3-0041", "cs35l41-pcm")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-hostless")));
#else
SND_SOC_DAILINK_DEFS(pri_mi2s_rx,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-mi2s.0")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(pri_mi2s_tx,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-mi2s.1")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-tx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));
#endif
SND_SOC_DAILINK_DEFS(sec_mi2s_rx,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-mi2s.2")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(sec_mi2s_tx,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-mi2s.3")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-tx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

#if defined(CONFIG_SND_SOC_ES9218P)
SND_SOC_DAILINK_DEFS(tert_mi2s_rx_dac,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-mi2s.2")),
	DAILINK_COMP_ARRAY(COMP_CODEC("es9218-codec.0048", "es9218-hifi")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));
#else
SND_SOC_DAILINK_DEFS(tert_mi2s_rx,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-mi2s.4")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));
#endif

SND_SOC_DAILINK_DEFS(tert_mi2s_tx,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-mi2s.5")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-tx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(quat_mi2s_rx,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-mi2s.6")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(quat_mi2s_tx,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-mi2s.7")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-tx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(quin_mi2s_rx,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-mi2s.8")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(quin_mi2s_tx,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-mi2s.9")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-tx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(sen_mi2s_rx,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-mi2s.10")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(sen_mi2s_tx,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-mi2s.11")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-tx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(auxpcm_rx,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-auxpcm.1")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(auxpcm_tx,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-auxpcm.1")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-tx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(sec_auxpcm_rx,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-auxpcm.2")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(sec_auxpcm_tx,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-auxpcm.2")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-tx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(tert_auxpcm_rx,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-auxpcm.3")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(tert_auxpcm_tx,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-auxpcm.3")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-tx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(quat_auxpcm_rx,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-auxpcm.4")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(quat_auxpcm_tx,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-auxpcm.4")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-tx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(quin_auxpcm_rx,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-auxpcm.5")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(quin_auxpcm_tx,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-auxpcm.5")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-tx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(sen_auxpcm_rx,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-auxpcm.6")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(sen_auxpcm_tx,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-auxpcm.6")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-tx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(wsa_dma_rx0,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-cdc-dma-dev.45056")),
#ifdef CONFIG_WSA_MACRO
	DAILINK_COMP_ARRAY(COMP_CODEC("bolero_codec", "wsa_macro_rx1"),
			   COMP_CODEC("wsa-codec1", "wsa_rx1"),
			   COMP_CODEC("wsa-codec2", "wsa_rx2"),
			   COMP_CODEC("wsa-codec3", "wsa_rx3"),
			   COMP_CODEC("wsa-codec4", "wsa_rx4")),
#else
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
#endif
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(wsa_dma_rx1,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-cdc-dma-dev.45058")),
#ifdef CONFIG_WSA_MACRO
	DAILINK_COMP_ARRAY(COMP_CODEC("bolero_codec", "wsa_macro_rx_mix"),
			   COMP_CODEC("wsa-codec1", "wsa_rx1"),
			   COMP_CODEC("wsa-codec2", "wsa_rx2"),
			   COMP_CODEC("wsa-codec3", "wsa_rx3"),
			   COMP_CODEC("wsa-codec4", "wsa_rx4")),
#else
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
#endif
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(wsa_dma_tx1,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-cdc-dma-dev.45059")),
#ifdef CONFIG_WSA_MACRO
	DAILINK_COMP_ARRAY(COMP_CODEC("bolero_codec", "wsa_macro_echo"),
			   COMP_CODEC("wsa-codec1", "wsa_rx1"),
			   COMP_CODEC("wsa-codec2", "wsa_rx2"),
			   COMP_CODEC("wsa-codec3", "wsa_rx3"),
			   COMP_CODEC("wsa-codec4", "wsa_rx4")),
#else
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
#endif
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(rx_dma_rx0,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-cdc-dma-dev.45104")),
	DAILINK_COMP_ARRAY(COMP_CODEC("bolero_codec", "rx_macro_rx1"),
			   COMP_CODEC("wcd938x_codec", "wcd938x_cdc"),
			   COMP_CODEC("wcd937x_codec", "wcd937x_cdc")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(rx_dma_rx1,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-cdc-dma-dev.45106")),
	DAILINK_COMP_ARRAY(COMP_CODEC("bolero_codec", "rx_macro_rx2"),
			   COMP_CODEC("wcd938x_codec", "wcd938x_cdc"),
			   COMP_CODEC("wcd937x_codec", "wcd937x_cdc")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(rx_dma_rx2,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-cdc-dma-dev.45108")),
	DAILINK_COMP_ARRAY(COMP_CODEC("bolero_codec", "rx_macro_rx3"),
			   COMP_CODEC("wcd938x_codec", "wcd938x_cdc"),
			   COMP_CODEC("wcd937x_codec", "wcd937x_cdc")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(rx_dma_rx3,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-cdc-dma-dev.45110")),
	DAILINK_COMP_ARRAY(COMP_CODEC("bolero_codec", "rx_macro_rx4"),
			   COMP_CODEC("wcd938x_codec", "wcd938x_cdc"),
			   COMP_CODEC("wcd937x_codec", "wcd937x_cdc")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(rx_dma_rx5,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-cdc-dma-dev.45114")),
	DAILINK_COMP_ARRAY(COMP_CODEC("bolero_codec", "rx_macro_rx5"),
			   COMP_CODEC("wcd938x_codec", "wcd938x_cdc"),
			   COMP_CODEC("wcd937x_codec", "wcd937x_cdc")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(rx_dma_rx6,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-cdc-dma-dev.45116")),
	DAILINK_COMP_ARRAY(COMP_CODEC("bolero_codec", "rx_macro_rx6"),
			   COMP_CODEC("swr-haptics", "swr_haptics")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(tx_dma_tx3,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-cdc-dma-dev.45111")),
	DAILINK_COMP_ARRAY(COMP_CODEC("bolero_codec", "tx_macro_tx1"),
			   COMP_CODEC("wcd938x_codec", "wcd938x_cdc"),
			   COMP_CODEC("wcd937x_codec", "wcd937x_cdc")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(tx_dma_tx4,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-cdc-dma-dev.45113")),
#ifdef CONFIG_MACH_LGE
	DAILINK_COMP_ARRAY(COMP_CODEC("bolero_codec", "tx_macro_tx2"),
			   COMP_CODEC("wcd938x_codec", "wcd938x_cdc")),
#else
	DAILINK_COMP_ARRAY(COMP_CODEC("bolero_codec", "tx_macro_tx2"),
			   COMP_CODEC("wcd938x_codec", "wcd938x_cdc"),
			   COMP_CODEC("wcd937x_codec", "wcd937x_cdc"),
			   COMP_CODEC("swr-dmic.01", "swr_dmic_tx0"),
			   COMP_CODEC("swr-dmic.02", "swr_dmic_tx1"),
			   COMP_CODEC("swr-dmic.03", "swr_dmic_tx2"),
			   COMP_CODEC("swr-dmic.04", "swr_dmic_tx3")),
#endif
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(va_dma_tx0,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-cdc-dma-dev.45089")),
	DAILINK_COMP_ARRAY(COMP_CODEC("bolero_codec", "va_macro_tx1")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(va_dma_tx1,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-cdc-dma-dev.45091")),
	DAILINK_COMP_ARRAY(COMP_CODEC("bolero_codec", "va_macro_tx2")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(va_dma_tx2,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-cdc-dma-dev.45093")),
	DAILINK_COMP_ARRAY(COMP_CODEC("bolero_codec", "va_macro_tx3")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(afe_loopback_tx,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-dev.24577")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-tx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(proxy_tx,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-dev.8195")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-tx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(proxy_rx,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-dev.8194")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(pri_tdm_rx_1,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.36866")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(pri_tdm_rx_2,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.36868")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(pri_tdm_rx_3,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.36870")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(pri_tdm_tx_1,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.36867")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(pri_tdm_tx_2,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.36869")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(pri_tdm_tx_3,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.36871")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(sec_tdm_rx_1,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.36882")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(sec_tdm_rx_2,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.36884")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(sec_tdm_rx_3,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.36886")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(sec_tdm_rx_7,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.36894")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(sec_tdm_tx_1,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.36883")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(sec_tdm_tx_2,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.36885")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(sec_tdm_tx_3,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.36887")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(tert_tdm_rx_1,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.36898")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(tert_tdm_rx_2,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.36900")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(tert_tdm_rx_3,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.36902")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(tert_tdm_rx_4,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.36904")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(tert_tdm_tx_1,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.36899")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(tert_tdm_tx_2,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.36901")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(tert_tdm_tx_3,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.36903")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(tert_tdm_tx_7,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.36911")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(quat_tdm_rx_1,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.36914")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(quat_tdm_rx_2,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.36916")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(quat_tdm_rx_3,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.36918")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(quat_tdm_rx_7,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.36926")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(quat_tdm_tx_1,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.36915")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(quat_tdm_tx_2,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.36917")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(quat_tdm_tx_3,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.36919")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(quat_tdm_tx_7,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.36927")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(quin_tdm_rx_7,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.36942")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(quin_tdm_tx_7,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.36943")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(ext_display_port,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-dp.24608")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-ext-disp-audio-codec-rx",
			"msm_dp_audio_codec_rx_dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(lpass_be_auxpcm_rx_dummy,
	DAILINK_COMP_ARRAY(COMP_CPU("snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("snd-soc-dummy")));

SND_SOC_DAILINK_DEFS(lpass_be_auxpcm_tx_dummy,
	DAILINK_COMP_ARRAY(COMP_CPU("snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-tx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("snd-soc-dummy")));

SND_SOC_DAILINK_DEFS(primary_tdm_rx_0_dummy,
	DAILINK_COMP_ARRAY(COMP_CPU("snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("snd-soc-dummy")));

SND_SOC_DAILINK_DEFS(primary_tdm_tx_0_dummy,
	DAILINK_COMP_ARRAY(COMP_CPU("snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-tx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("snd-soc-dummy")));

SND_SOC_DAILINK_DEFS(secondary_tdm_rx_0_dummy,
	DAILINK_COMP_ARRAY(COMP_CPU("snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("snd-soc-dummy")));

SND_SOC_DAILINK_DEFS(secondary_tdm_tx_0_dummy,
	DAILINK_COMP_ARRAY(COMP_CPU("snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-tx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("snd-soc-dummy")));

SND_SOC_DAILINK_DEFS(tert_tdm_rx_0_dummy,
	DAILINK_COMP_ARRAY(COMP_CPU("snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("snd-soc-dummy")));

SND_SOC_DAILINK_DEFS(tert_tdm_tx_0_dummy,
	DAILINK_COMP_ARRAY(COMP_CPU("snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-tx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("snd-soc-dummy")));

SND_SOC_DAILINK_DEFS(quat_tdm_rx_0_dummy,
	DAILINK_COMP_ARRAY(COMP_CPU("snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("snd-soc-dummy")));

SND_SOC_DAILINK_DEFS(quat_tdm_tx_0_dummy,
	DAILINK_COMP_ARRAY(COMP_CPU("snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-tx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("snd-soc-dummy")));

SND_SOC_DAILINK_DEFS(quin_tdm_rx_0_dummy,
	DAILINK_COMP_ARRAY(COMP_CPU("snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("snd-soc-dummy")));

SND_SOC_DAILINK_DEFS(quin_tdm_tx_0_dummy,
	DAILINK_COMP_ARRAY(COMP_CPU("snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-tx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("snd-soc-dummy")));

SND_SOC_DAILINK_DEFS(sen_tdm_rx_0_dummy,
	DAILINK_COMP_ARRAY(COMP_CPU("snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("snd-soc-dummy")));

SND_SOC_DAILINK_DEFS(sen_tdm_tx_0_dummy,
	DAILINK_COMP_ARRAY(COMP_CPU("snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-tx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("snd-soc-dummy")));

SND_SOC_DAILINK_DEFS(sep_tdm_rx_0_dummy,
	DAILINK_COMP_ARRAY(COMP_CPU("snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("snd-soc-dummy")));

SND_SOC_DAILINK_DEFS(sep_tdm_tx_0_dummy,
	DAILINK_COMP_ARRAY(COMP_CPU("snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-tx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("snd-soc-dummy")));

SND_SOC_DAILINK_DEFS(hs_if0_tdm_rx_0_dummy,
	DAILINK_COMP_ARRAY(COMP_CPU("snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("snd-soc-dummy")));

SND_SOC_DAILINK_DEFS(hs_if0_tdm_tx_0_dummy,
	DAILINK_COMP_ARRAY(COMP_CPU("snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-tx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("snd-soc-dummy")));

SND_SOC_DAILINK_DEFS(hs_if1_tdm_rx_0_dummy,
	DAILINK_COMP_ARRAY(COMP_CPU("snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("snd-soc-dummy")));

SND_SOC_DAILINK_DEFS(hs_if1_tdm_tx_0_dummy,
	DAILINK_COMP_ARRAY(COMP_CPU("snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-tx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("snd-soc-dummy")));

SND_SOC_DAILINK_DEFS(hs_if2_tdm_rx_0_dummy,
	DAILINK_COMP_ARRAY(COMP_CPU("snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("snd-soc-dummy")));

SND_SOC_DAILINK_DEFS(hs_if2_tdm_tx_0_dummy,
	DAILINK_COMP_ARRAY(COMP_CPU("snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-tx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("snd-soc-dummy")));

SND_SOC_DAILINK_DEFS(voicemmode1_hostless,
	DAILINK_COMP_ARRAY(COMP_CPU("VoiceMMode1")),
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-hostless")));

SND_SOC_DAILINK_DEFS(msmvoip_hostless,
	DAILINK_COMP_ARRAY(COMP_CPU("VoIP")),
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-hostless")));

SND_SOC_DAILINK_DEFS(voicemmode2_hostless,
	DAILINK_COMP_ARRAY(COMP_CPU("VoiceMMode2")),
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-hostless")));

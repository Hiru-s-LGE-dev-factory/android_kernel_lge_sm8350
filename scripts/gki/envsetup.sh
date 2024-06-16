#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-only
# Copyright (c) 2019, The Linux Foundation. All rights reserved.

SCRIPT_DIR=$(readlink -f $(dirname $0)/)
cd ${SCRIPT_DIR}
cd ../../
KERN_SRC=`pwd`

: ${ARCH:=arm64}
: ${CROSS_COMPILE:=aarch64-linux-gnu-}
: ${CLANG_TRIPLE:=aarch64-linux-gnu-}
: ${REAL_CC:=clang}
: ${HOSTCC:=gcc}
: ${HOSTLD:=ld}
: ${HOSTAR:=ar}
: ${KERN_OUT:=}

CONFIGS_DIR=${KERN_SRC}/arch/${ARCH}/configs/vendor

PLATFORM_NAME=$1

BASE_DEFCONFIG=${KERN_SRC}/arch/${ARCH}/configs/${4:-gki_defconfig}

# Fragements that are available for the platform
QCOM_GKI_FRAG=${CONFIGS_DIR}/${PLATFORM_NAME}_GKI.config
QCOM_QGKI_FRAG=${CONFIGS_DIR}/${PLATFORM_NAME}_QGKI.config
QCOM_DEBUG_FRAG=${CONFIGS_DIR}/${PLATFORM_NAME}_debug.config

if [[ $2 == ${PLATFORM_NAME} ]] # $2 is TARGET_PRODUCT
then
	LGE_GKI_FRAG=
	LGE_QGKI_FRAG=
	LGE_DEBUG_FRAG=
else
	LGE_QGKI_FRAG=${CONFIGS_DIR}/${PLATFORM_NAME}_$2.config
	LGE_GKI_FRAG=${CONFIGS_DIR}/${PLATFORM_NAME}_$2_GKI.config
	LGE_DEBUG_FRAG=${CONFIGS_DIR}/${PLATFORM_NAME}_$2_debug.config
fi

# For user variant build merge debugfs.config fragment.
if [ ${TARGET_BUILD_VARIANT} == "user" ]; then
	QCOM_DEBUG_FS_FRAG=`ls ${CONFIGS_DIR}/debugfs.config 2> /dev/null`
else
	QCOM_DEBUG_FS_FRAG=" "
fi

# For MME_TEST
if [[ $3 == "true" ]]; then
	LGE_MME_TEST_FLAG=${CONFIGS_DIR}/lge_mme_test.config
else
	LGE_MME_TEST_FLAG=
fi

# Consolidate fragment may not be present for all platforms.
QCOM_CONSOLIDATE_FRAG=`ls ${CONFIGS_DIR}/${PLATFORM_NAME}_consolidate.config 2> /dev/null`

QCOM_GENERIC_PERF_FRAG=${CONFIGS_DIR}/${PLATFORM_NAME}.config
QCOM_GENERIC_DEBUG_FRAG=${CONFIGS_DIR}/${PLATFORM_NAME}-debug.config

export ARCH CROSS_COMPILE REAL_CC HOSTCC HOSTLD HOSTAR KERN_SRC KERN_OUT \
	CONFIGS_DIR BASE_DEFCONFIG QCOM_GKI_FRAG QCOM_QGKI_FRAG LGE_MME_TEST_FLAG LGE_GKI_FRAG LGE_QGKI_FRAG LGE_DEBUG_FRAG QCOM_DEBUG_FRAG

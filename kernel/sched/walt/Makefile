# SPDX-License-Identifier: GPL-2.0
obj-$(CONFIG_SCHED_WALT) += walt.o boost.o sched_avg.o qc_vas.o core_ctl.o trace.o
ifeq ($(CONFIG_LGE_TOUCH_INPUT_CPU_BOOST), y)
obj-$(CONFIG_LGE_TOUCH_INPUT_CPU_BOOST) += cpu-boost_lge.o
else
obj-$(CONFIG_CPU_FREQ) += cpu-boost.o
endif

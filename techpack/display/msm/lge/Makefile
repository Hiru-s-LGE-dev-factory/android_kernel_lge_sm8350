# SPDX-License-Identifier: GPL-2.0
ccflags-y := -I$(srctree)/include/drm -I$(srctree)/techpack/display/msm -I$(srctree)/techpack/display/msm/dsi -I$(srctree)/techpack/display/msm/dp
ccflags-y += -I$(srctree)/techpack/display/msm/sde
ccflags-y += -I$(srctree)/techpack/display/rotator
ccflags-y += -I$(srctree)/drivers/clk/qcom/

lge_display_common-objs := lge_dsi_panel.o
lge_display_common-objs += lge_panel_reg.o
lge_display_common-objs += lge_ddic_ops.o
lge_display_common-objs += lge_ddic_ops_sw43103.o
lge_display_common-objs += lge_ddic_ops_nt37800a.o
lge_display_common-objs += lge_ddic_ops_nt37800a_rdk.o
lge_display_common-objs += lge_ddic_ops_nt37701.o
lge_display_common-objs += ambient/lge_backlight_ex.o
lge_display_common-objs += ambient/lge_ambient.o
lge_display_common-objs += cm/lge_color_manager.o
lge_display_common-objs += drs/lge_drs_mngr.o
lge_display_common-objs += notifier/lge_panel_notifier.o
lge_display_common-objs += brightness/lge_brightness.o
lge_display_common-objs += factory/lge_factory.o
lge_display_common-objs += err_detect/lge_err_detect.o
lge_display_common-objs += dp/lge_dp.o
ifeq ($(CONFIG_LGE_DUAL_SCREEN),y)
lge_display_common-objs += ds3/lge_backlight_ds3.o
lge_display_common-objs += cover/lge_cover_ctrl.o
lge_display_common-objs += cover/lge_cover_panel.o
endif

obj-$(CONFIG_LGE_DISPLAY_COMMON) += lge_display_common.o

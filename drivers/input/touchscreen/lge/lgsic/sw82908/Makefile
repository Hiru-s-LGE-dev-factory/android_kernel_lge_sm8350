#
## Makefile for the touchscreen drivers.
#
#
## Each configuration option enables a list of files.

# Touch Core Driver Common Header Code..!!
ccflags-y += -I$(srctree)/drivers/input/touchscreen/lge/

obj-$(CONFIG_LGE_TOUCH_LGSIC_SW82908)           += touch_sw82908.o
obj-$(CONFIG_LGE_TOUCH_LGSIC_SW82908)           += touch_sw82908_prd.o
obj-$(CONFIG_LGE_TOUCH_LGSIC_SW82908)           += touch_sw82908_abt.o

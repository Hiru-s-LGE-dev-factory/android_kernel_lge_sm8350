#
# Makefile for lge touch core driver
#

# Each configuration option enables a list of files.

ifdef CONFIG_LGE_TOUCH_CORE_MTK
	subdir-ccflags-y += -Werror
	subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include
	subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/
endif

# Touch Core Driver Common Header Code..!!
ccflags-y += -I$(srctree)/drivers/input/touchscreen/lge/

ifdef CONFIG_LGE_TOUCH_DLKM
obj-$(CONFIG_LGE_TOUCH_CORE)			+= touch_module_core.o
touch_module_core-objs					+= touch_core.o
touch_module_core-objs					+= touch_core_sysfs.o
touch_module_core-objs					+= touch_common.o
touch_module_core-objs					+= touch_i2c.o
touch_module_core-objs					+= touch_spi.o
touch_module_core-objs					+= touch_model.o
touch_module_core-objs					+= touch_notify.o
touch_module_core-objs					+= module/touch_module_builtIn.o
ifdef CONFIG_LGE_TOUCH_CORE_QCT
touch_module_core-objs					+= touch_hwif.o
else ifdef CONFIG_LGE_TOUCH_CORE_MTK
touch_module_core-objs					+= touch_hwif_mtk.o
endif
ifdef CONFIG_LGE_TOUCH_HIDDEN
touch_module_core-objs					+= touch_hidden.o
endif
ifdef CONFIG_LGE_TOUCH_PEN
touch_module_core-objs					+= touch_pen.o
endif
ifdef CONFIG_LGE_TOUCH_DEX
touch_module_core-objs					+= touch_dex.o
endif

obj-$(CONFIG_LGE_TOUCH_CORE_BASE)	+= module/

else
obj-$(CONFIG_LGE_TOUCH_CORE_BASE)	+= touch_core.o
obj-$(CONFIG_LGE_TOUCH_CORE_BASE)	+= touch_core_sysfs.o
obj-$(CONFIG_LGE_TOUCH_CORE_BASE)	+= touch_common.o
obj-$(CONFIG_LGE_TOUCH_CORE_BASE)	+= touch_i2c.o
obj-$(CONFIG_LGE_TOUCH_CORE_BASE)	+= touch_spi.o
obj-$(CONFIG_LGE_TOUCH_CORE_BASE)	+= touch_model.o
obj-$(CONFIG_LGE_TOUCH_CORE_BASE)	+= touch_notify.o
obj-$(CONFIG_LGE_TOUCH_HIDDEN)		+= touch_hidden.o
obj-$(CONFIG_LGE_TOUCH_CORE_BASE)	+= module/touch_module_builtIn.o

obj-$(CONFIG_LGE_TOUCH_CORE_QCT)	+= touch_hwif.o
obj-$(CONFIG_LGE_TOUCH_CORE_MTK)	+= touch_hwif_mtk.o
obj-$(CONFIG_LGE_TOUCH_PEN)		+= touch_pen.o
obj-$(CONFIG_LGE_TOUCH_DEX)		+= touch_dex.o
obj-$(CONFIG_LGE_TOUCH_CORE_BASE)	+= lgsic/
obj-$(CONFIG_LGE_TOUCH_CORE_BASE)	+= module/
endif

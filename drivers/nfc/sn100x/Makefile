#
# Makefile for nfc devices
#

obj-$(CONFIG_LGE_NFC_SN100X)             += nfcsn100.o

nfcsn100-objs := sn100x_lge.o cold_reset.o

ifdef CONFIG_NFC_SELECTIVE_DRIVER
nfcsn100-objs += ../lge/lgenfc.o
endif

#ccflags-$(CONFIG_NFC_PN553_DEVICES) := -DDEBUG

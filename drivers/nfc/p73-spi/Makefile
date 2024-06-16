#
# Makefile for embedded secure element devices
#


obj-$(CONFIG_NXP_P73_DEVICES)		+= nfcp73.o

nfcp73-objs := p73.o
ifdef CONFIG_NFC_SELECTIVE_DRIVER
nfcp73-objs += ../lge/lgenfc.o
endif

#ccflags-$(CONFIG_NXP_P73_DEVICES_DEBUG) := -DDEBUG


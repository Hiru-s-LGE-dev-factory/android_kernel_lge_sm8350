#
# Makefile for nfc devices
#

obj-$(CONFIG_LGE_NFC_PN553)             += nfcpn553.o

nfcpn553-objs := pn553_lge.o pn553_lge_hwadapter.o

ifdef CONFIG_NFC_SELECTIVE_DRIVER
nfcpn553-objs += ../lge/lgenfc.o
endif

ccflags-$(CONFIG_NFC_PN553_DEVICES) := -DDEBUG

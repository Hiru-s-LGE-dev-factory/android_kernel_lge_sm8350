obj-$(CONFIG_LGE_PM_ACTM_V2) += adaptive-charging-thermal-v2.o
obj-y		+= charging-ceiling.o
obj-y		+= protection-battemp.o
ifeq ($(CONFIG_LGE_PM_PROT_BATVOLT_V2), y)
obj-y		+= protection-batvolt-v2.o
else
obj-y		+= protection-batvolt.o
endif
obj-y		+= unified-bootmode.o
obj-y		+= unified-nodes.o
obj-y		+= unified-sysfs.o
obj-y		+= veneer-psy.o
obj-y		+= veneer-uevent.o
obj-y		+= veneer-voter.o
obj-y		+= veneer-param.o
obj-y		+= pmic-voter.o
obj-y		+= protection-showcase.o
ifeq ($(CONFIG_LGE_PM_CCD), y)
obj-y		+= charging-time-ccd.o
else
obj-y		+= charging-time-v3.o
endif

ifeq ($(CONFIG_BUILD_ARM64_DT_OVERLAY),y)

# Per Platform
ifeq ($(CONFIG_MACH_LAHAINA_BLM),y)
DTS_PROJECT := lahaina-blm
DTS_TARGET := lahaina-blm

ifeq ($(CONFIG_MACH_LAHAINA_BLM_DCM_JP),y)
DTS_TARGET := lahaina-blm_dcm_jp
endif

ifeq ($(CONFIG_MACH_LAHAINA_BLM_SB_JP),y)
DTS_TARGET := lahaina-blm_sb_jp
endif
endif # CONFIG_MACH_LAHAINA_BLM

ifeq ($(CONFIG_MACH_LAHAINA_RAINBOWLM),y)
DTS_PROJECT := lahaina-rainbowlm
DTS_TARGET := lahaina-rainbowlm

ifeq ($(CONFIG_MACH_LAHAINA_RAINBOWLM_DCM_JP),y)
DTS_TARGET := lahaina-rainbowlm_dcm_jp
endif

ifeq ($(CONFIG_MACH_LAHAINA_RAINBOWLM_SB_JP),y)
DTS_TARGET := lahaina-rainbowlm_sb_jp
endif
endif # CONFIG_MACH_LAHAINA_RAINBOWLM

ALL_DTS := $(notdir $(wildcard echo $(srctree)/$(src)/$(DTS_PROJECT)/$(DTS_TARGET)/*.dts))
dtbo-y += $(patsubst %.dts,%.dtbo,$(ALL_DTS))

DTB_NAMES := $(subst $\",,$(CONFIG_BUILD_ARM_APPENDED_DTB_IMAGE_NAMES))
ifneq ($(DTB_NAMES),)
DTB_LIST := $(addsuffix .dtbo,$(DTB_NAMES))
else
DTB_LIST := $(dtbo-y)
endif

always  := $(dtbo-y)

# below code is based on kernel/msm-5.4/scripts/Makefile.dtbo
$(obj)/%.dtbo: $(src)/$(DTS_PROJECT)/$(DTS_TARGET)/%.dts FORCE
	$(call if_changed_dep,dtc,dtb,-@)
	$(call if_changed,dtbo_verify)

clean-files := *.dtb
endif

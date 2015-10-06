ifeq ($(ASUS_PROJECT), ME372CL)
    KBUILD_CFLAGS += -DASUS_PROJECT_ME372CL_DISPLAY
endif

ifeq ($(ASUS_PROJECT), ME372CG)
    KBUILD_CFLAGS += -DASUS_PROJECT_ME372CG_DISPLAY
endif

ifeq ($(ASUS_PROJECT), TX201LA)
    KBUILD_CFLAGS += -DASUS_PROJECT_TX201LA_DISPLAY
endif

ifeq ($(ASUS_PROJECT), ME175CG)
    KBUILD_CFLAGS += -DASUS_PROJECT_ME175CG_DISPLAY
endif

ifeq ($(ASUS_PROJECT), ME302C)
    KBUILD_CFLAGS += -DASUS_PROJECT_ME302C_DISPLAY
endif

ifeq ($(ASUS_PROJECT), PF400CG)
    KBUILD_CFLAGS += -DASUS_PROJECT_PF400CG_DISPLAY
endif

ifeq ($(ASUS_PROJECT), A400CG)
    KBUILD_CFLAGS += -DASUS_PROJECT_A400CG_DISPLAY
endif

obj-$(CONFIG_DRM_MDFLD)		+= build/clovertrail/pnw/
obj-$(CONFIG_DRM_MDFLD)		+= build/clovertrail/sgx/
obj-$(CONFIG_DRM_CTP)		+= build/clovertrail/pnw/
obj-$(CONFIG_DRM_CTP)		+= build/clovertrail/sgx/
obj-$(CONFIG_DRM_INTEL_HANDSET)	+= build/merrifield/tng/
obj-$(CONFIG_SUPPORT_HDMI)	+= build/hdmi_audio/
obj-$(CONFIG_DRM_VXD_BYT)	+= build/baytrail/
obj-$(CONFIG_GFX_RGX_DEVFREQ)   += graphics/dfrgx/

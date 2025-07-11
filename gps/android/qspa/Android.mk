LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE       := qspa-nav.xml
LOCAL_MODULE_STEM  := qspa-nav.xml
LOCAL_SRC_FILES    := qspa-nav.xml
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH  := $(TARGET_OUT_VENDOR)/etc/vintf/manifest/qspa
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE       := nav_disabled.rc
LOCAL_MODULE_TAGS  := optional
LOCAL_MODULE_CLASS := ETC
LOCAL_SRC_FILES    := nav_disabled.rc
LOCAL_MODULE_PATH  := $(TARGET_OUT_VENDOR_ETC)/qspa
include $(BUILD_PREBUILT)

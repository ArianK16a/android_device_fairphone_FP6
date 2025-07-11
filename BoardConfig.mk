#
# SPDX-FileCopyrightText: The LineageOS Project
# SPDX-License-Identifier: Apache-2.0
#

DEVICE_PATH := device/fairphone/FP6

# Architecture
TARGET_ARCH := arm64
TARGET_ARCH_VARIANT := armv8-a-branchprot
TARGET_CPU_ABI := arm64-v8a
TARGET_CPU_ABI2 :=
TARGET_CPU_VARIANT := kryo

# Board
TARGET_BOARD_PLATFORM := volcano
TARGET_BOOTLOADER_BOARD_NAME := FP6

# Boot image
BOARD_RAMDISK_USE_LZ4 := true
BOARD_BOOT_HEADER_VERSION := 4
BOARD_MKBOOTIMG_ARGS := --header_version $(BOARD_BOOT_HEADER_VERSION)

# Bootloader
TARGET_NO_BOOTLOADER := true

# Device Tree
BOARD_INCLUDE_DTB_IN_BOOTIMG := true
TARGET_NEEDS_DTBOIMAGE := true

# Kernel
BOARD_KERNEL_IMAGE_NAME := Image
BOARD_USES_GENERIC_KERNEL_IMAGE := true
BOARD_USES_QCOM_MERGE_DTBS_SCRIPT := true

BOARD_KERNEL_BASE        := 0x00000000
BOARD_KERNEL_PAGESIZE    := 4096

TARGET_KERNEL_SOURCE := kernel/fairphone/sm7635
TARGET_KERNEL_CONFIG := \
    gki_defconfig \
    vendor/fps_GKI.config

BOARD_KERNEL_CMDLINE := \
    video=vfb:640x400,bpp=32,memsize=3072000 \
    nosoftlockup
BOARD_BOOTCONFIG := \
    androidboot.hardware=qcom \
    androidboot.memcg=1 \
    androidboot.usbcontroller=a600000.dwc3 \
    androidboot.load_modules_parallel=true \
    androidboot.console=0

# Kernel modules
BOARD_SYSTEM_KERNEL_MODULES_BLOCKLIST_FILE := $(TARGET_KERNEL_SOURCE)/modules.systemdlkm_blocklist.msm.pineapple
BOARD_VENDOR_KERNEL_MODULES_BLOCKLIST_FILE := $(TARGET_KERNEL_SOURCE)/modules.vendor_blocklist.msm.pineapple
BOARD_VENDOR_RAMDISK_KERNEL_MODULES_BLOCKLIST_FILE := $(BOARD_VENDOR_KERNEL_MODULES_BLOCKLIST_FILE)

first_stage_modules := $(strip $(shell cat $(DEVICE_PATH)/modules/modules.list.first_stage))
second_stage_modules := $(strip $(shell cat $(DEVICE_PATH)/modules/modules.list.second_stage))
vendor_dlkm_modules := $(strip $(shell cat $(DEVICE_PATH)/modules/modules.list.vendor_dlkm))
system_dlkm_modules := $(strip $(shell cat $(DEVICE_PATH)/modules/modules.list.system_dlkm))

BOARD_SYSTEM_KERNEL_MODULES_LOAD := $(system_dlkm_modules)
BOARD_VENDOR_RAMDISK_KERNEL_MODULES_LOAD := $(first_stage_modules)
BOARD_VENDOR_RAMDISK_RECOVERY_KERNEL_MODULES_LOAD := $(first_stage_modules) $(second_stage_modules)
BOARD_VENDOR_KERNEL_MODULES_LOAD := $(second_stage_modules) $(vendor_dlkm_modules)

SYSTEM_KERNEL_MODULES := $(BOARD_SYSTEM_KERNEL_MODULES_LOAD)
BOOT_KERNEL_MODULES := $(BOARD_VENDOR_RAMDISK_RECOVERY_KERNEL_MODULES_LOAD)

TARGET_KERNEL_EXT_MODULE_ROOT := kernel/fairphone/sm7635-modules
TARGET_KERNEL_EXT_MODULES := \
    nxp/opensource/driver \
    qcom/opensource/mmrm-driver \
    qcom/opensource/audio-kernel \
    qcom/opensource/bt-kernel \
    qcom/opensource/camera-kernel \
    qcom/opensource/dataipa/drivers/platform/msm \
    qcom/opensource/datarmnet-ext/aps \
    qcom/opensource/datarmnet-ext/mem \
    qcom/opensource/datarmnet-ext/offload \
    qcom/opensource/datarmnet-ext/perf \
    qcom/opensource/datarmnet-ext/perf_tether \
    qcom/opensource/datarmnet-ext/sch \
    qcom/opensource/datarmnet-ext/shs \
    qcom/opensource/datarmnet-ext/wlan \
    qcom/opensource/datarmnet/core \
    qcom/opensource/display-drivers/msm \
    qcom/opensource/dsp-kernel \
    qcom/opensource/eva-kernel \
    qcom/opensource/graphics-kernel \
    qcom/opensource/mm-drivers/hw_fence \
    qcom/opensource/mm-drivers/msm_ext_display \
    qcom/opensource/mm-drivers/sync_fence \
    qcom/opensource/mm-sys-kernel/ubwcp \
    qcom/opensource/securemsm-kernel \
    qcom/opensource/spu-kernel \
    qcom/opensource/synx-kernel \
    qcom/opensource/touch-drivers \
    qcom/opensource/video-driver \
    qcom/opensource/wlan/platform \
    qcom/opensource/wlan/qcacld-3.0/.peach_v2 \
    qcom/opensource/wlan/qcacld-3.0/.qca6750 \
    qcom/opensource/wlan/qcacld-3.0/.wcn6450 \
    samsung_slsi/nfc/driver

# Metadata
BOARD_USES_METADATA_PARTITION := true

# Partitions
BOARD_FLASH_BLOCK_SIZE := 0x020000 # (BOARD_KERNEL_PAGESIZE * 64)
BOARD_BOOTIMAGE_PARTITION_SIZE := 0x06000000
BOARD_DTBOIMG_PARTITION_SIZE := 0x01E00000
BOARD_INIT_BOOT_IMAGE_PARTITION_SIZE := 0x800000
BOARD_RECOVERYIMAGE_PARTITION_SIZE := 0x06400000
BOARD_SUPER_PARTITION_SIZE := 9663676416 # 0x240000000
BOARD_VENDOR_BOOTIMAGE_PARTITION_SIZE := 0x06000000

BOARD_SUPER_PARTITION_GROUPS := qti_dynamic_partitions
BOARD_QTI_DYNAMIC_PARTITIONS_PARTITION_LIST := odm product system system_dlkm system_ext vendor vendor_dlkm
BOARD_QTI_DYNAMIC_PARTITIONS_SIZE := 9659482112 # 0x23FC00000 # BOARD_SUPER_PARTITION_SIZE - overhead (4MiB)

BOARD_ODMIMAGE_FILE_SYSTEM_TYPE := ext4
BOARD_PRODUCTIMAGE_FILE_SYSTEM_TYPE := ext4
BOARD_SYSTEMIMAGE_FILE_SYSTEM_TYPE := ext4
BOARD_SYSTEM_DLKMIMAGE_FILE_SYSTEM_TYPE := ext4
BOARD_SYSTEM_EXTIMAGE_FILE_SYSTEM_TYPE := ext4
BOARD_VENDORIMAGE_FILE_SYSTEM_TYPE := ext4
BOARD_VENDOR_DLKMIMAGE_FILE_SYSTEM_TYPE := ext4

TARGET_COPY_OUT_ODM := odm
TARGET_COPY_OUT_PRODUCT := product
TARGET_COPY_OUT_SYSTEM_DLKM := system_dlkm
TARGET_COPY_OUT_SYSTEM_EXT := system_ext
TARGET_COPY_OUT_VENDOR := vendor
TARGET_COPY_OUT_VENDOR_DLKM := vendor_dlkm

-include vendor/lineage/config/BoardConfigReservedSize.mk

# Verified Boot
STOCK_SECURITY_PATCH_TIMESTAMP := $(shell date -d 'TZ="GMT" 2025-05-05' +%s)
BOARD_AVB_ENABLE := true
BOARD_AVB_MAKE_VBMETA_IMAGE_ARGS += --flags 3

BOARD_AVB_RECOVERY_KEY_PATH := external/avb/test/data/testkey_rsa4096.pem
BOARD_AVB_RECOVERY_ALGORITHM := SHA256_RSA4096
BOARD_AVB_RECOVERY_ROLLBACK_INDEX := 1
BOARD_AVB_RECOVERY_ROLLBACK_INDEX_LOCATION := 1

BOARD_AVB_VBMETA_SYSTEM := system system_ext product
BOARD_AVB_VBMETA_SYSTEM_KEY_PATH := external/avb/test/data/testkey_rsa4096.pem
BOARD_AVB_VBMETA_SYSTEM_ALGORITHM := SHA256_RSA4096
BOARD_AVB_VBMETA_SYSTEM_ROLLBACK_INDEX := $(STOCK_SECURITY_PATCH_TIMESTAMP)
BOARD_AVB_VBMETA_SYSTEM_ROLLBACK_INDEX_LOCATION := 2

BOARD_AVB_BOOT_KEY_PATH := external/avb/test/data/testkey_rsa4096.pem
BOARD_AVB_BOOT_ALGORITHM := SHA256_RSA4096
BOARD_AVB_BOOT_ROLLBACK_INDEX := $(STOCK_SECURITY_PATCH_TIMESTAMP)
BOARD_AVB_BOOT_ROLLBACK_INDEX_LOCATION := 3

BOARD_AVB_INIT_BOOT_KEY_PATH := external/avb/test/data/testkey_rsa2048.pem
BOARD_AVB_INIT_BOOT_ALGORITHM := SHA256_RSA2048
BOARD_AVB_INIT_BOOT_ROLLBACK_INDEX := $(STOCK_SECURITY_PATCH_TIMESTAMP)
BOARD_AVB_INIT_BOOT_ROLLBACK_INDEX_LOCATION := 4

# Inherit the proprietary files BoardConfig
include vendor/fairphone/FP6/BoardConfigVendor.mk

#
# SPDX-FileCopyrightText: The LineageOS Project
# SPDX-License-Identifier: Apache-2.0
#

# Inherit from FP6 device
$(call inherit-product, device/fairphone/FP6/device.mk)

# Inherit from common lineage configuration
$(call inherit-product, vendor/lineage/config/common_full_phone.mk)

PRODUCT_NAME := lineage_FP6
PRODUCT_DEVICE := FP6
PRODUCT_MANUFACTURER := Fairphone
PRODUCT_BRAND := Fairphone
PRODUCT_MODEL := Fairphone 6

PRODUCT_BRAND_FOR_ATTESTATION := Fairphone
PRODUCT_MODEL_FOR_ATTESTATION := Fairphone 6
PRODUCT_NAME_FOR_ATTESTATION := FP6

PRODUCT_GMS_CLIENTID_BASE := android-fairphone

PRODUCT_BUILD_PROP_OVERRIDES += \
    BuildDesc="qssi_64-user 15 FP6.QREL.15.91.0 VS15 release-keys" \
    BuildFingerprint=Fairphone/FP6/FP6:15/FP6.QREL.15.91.0/VS15:user/release-keys \
    DeviceName=FP6 \
    DeviceProduct=FP6 \
    SystemDevice=FP6 \
    SystemName=FP6

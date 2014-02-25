#
# Copyright (C) 2013 The CyanogenMod Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

$(call inherit-product, frameworks/native/build/phone-xhdpi-1024-dalvik-heap.mk)

$(call inherit-product, $(SRC_TARGET_DIR)/product/languages_full.mk)

$(call inherit-product, device/motorola/msm8226-common/msm8226.mk)

LOCAL_PATH := device/motorola/falcon

# falcon specific overlay
DEVICE_PACKAGE_OVERLAYS += $(LOCAL_PATH)/overlay

PRODUCT_LOCALES := en_US
PRODUCT_LOCALES += xhdpi
PRODUCT_AAPT_CONFIG := normal hdpi xhdpi
PRODUCT_AAPT_PREF_CONFIG := xhdpi

# CDMA, GSM/WCDMA
PRODUCT_PROPERTY_OVERRIDES += \
	ro.telephony.default_network=5 \
	telephony.lteOnCdmaDevice=1 \
	persist.radio.mode_pref_nv10=1 \
	persist.radio.no_wait_for_card=1 \
	persist.radio.dfr_mode_set=1

$(call inherit-product, device/motorola/msm8226-common/keylayout/keylayout.mk)
$(call inherit-product, vendor/motorola/falcon/falcon-vendor.mk)

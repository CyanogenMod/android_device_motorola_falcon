/*
   Copyright (c) 2014, The Linux Foundation. All rights reserved.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are
   met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above
      copyright notice, this list of conditions and the following
      disclaimer in the documentation and/or other materials provided
      with the distribution.
    * Neither the name of The Linux Foundation nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
   WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
   ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
   BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
   CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
   SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
   BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
   WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
   OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
   IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdlib.h>
#include <stdio.h>

#include "vendor_init.h"
#include "property_service.h"
#include "log.h"
#include "util.h"

#include "init_msm.h"

enum supported_carrier {
    UNKNOWN = -1,
    VERIZON,
    BOOST,
    USC,
};

/* Motorola relies on system properties to detect the carrier, however we
 * don't have those properties set and we actually want to set them here.
 * Calculate the md5 of /fsg/0.img.gz and compare it with a list of known
 * checksums to detect the carrier.
 */
static struct {
    const char *md5sum;
    enum supported_carrier carrier;
} fsg_md5_list[] = {
    { "034c470658f4db2302de4266af89a6bd", VERIZON },
    { "46ac94a79513898901b3b37cf7d9c3c8", BOOST },
    { "f8e4cfd380062e643cfb460c15d08a48", USC },
};

enum supported_carrier detect_carrier()
{
    char fsg_md5[33];
    FILE *fp;

    fp = popen("/system/bin/md5sum -b /fsg/0.img.gz", "r");
    fgets(fsg_md5, sizeof(fsg_md5), fp);
    pclose(fp);

    INFO("md5sum of /fsg/0.img.gz: %s\n", fsg_md5);
    for (uint i = 0; i < ARRAY_SIZE(fsg_md5_list); i++) {
        if (ISMATCH(fsg_md5_list[i].md5sum, fsg_md5))
            return fsg_md5_list[i].carrier;
    }
    ERROR("Unknwon carrier, md5sum: %s\n", fsg_md5);

    return UNKNOWN;
}

void init_msm_properties(unsigned long msm_id, unsigned long msm_ver, char *board_type)
{
    char platform[PROP_VALUE_MAX];
    char radio[PROP_VALUE_MAX];
    char device[PROP_VALUE_MAX];
    char fstype[92];
    int rc;

    UNUSED(msm_id);
    UNUSED(msm_ver);
    UNUSED(board_type);

    rc = property_get("ro.board.platform", platform);
    if (!rc || !ISMATCH(platform, ANDROID_TARGET))
        return;
    property_get("ro.boot.radio", radio);

    property_set("ro.product.model", "Moto G");
    if (ISMATCH(radio, "0x1")) {
        FILE *fp = popen("/system/bin/blkid /dev/block/platform/msm_sdcc.1/by-name/userdata | /system/bin/cut -d'\"' -f4", "r");
        fgets(fstype, sizeof(fstype), fp);
        pclose(fp);
        if (ISMATCH(fstype, "ext4")) {
            /* xt1032 GPE */
            property_set("ro.product.device", "falcon_gpe");
            property_set("ro.build.description", "falcon_gpe-user 5.1 LMY47M.M005 10 release-keys");
            property_set("ro.build.fingerprint", "motorola/falcon_gpe/falcon_umts:5.1/LMY47M.M005/10:user/release-keys");
            property_set("ro.build.product", "falcon_gpe");
            property_set("ro.mot.build.customerid", "retusa_glb");
            property_set("ro.telephony.default_network", "0");
            property_set("persist.radio.multisim.config", "");
        } else {
            /* xt1032 */
            property_set("ro.product.device", "falcon_umts");
            property_set("ro.build.description", "falcon_retuglb-user 5.1 LPB23.13-58 58 release-keys");
            property_set("ro.build.fingerprint", "motorola/falcon_retuglb/falcon_umts:5.1/LPB23.13-58/58:user/release-keys");
            property_set("ro.build.product", "falcon_umts");
            property_set("ro.mot.build.customerid", "retusa_glb");
            property_set("ro.telephony.default_network", "0");
            property_set("persist.radio.multisim.config", "");
        }
    } else if (ISMATCH(radio, "0x3")) {
        enum supported_carrier carrier = detect_carrier();
        if (carrier == VERIZON) {
            /* xt1028 */
            property_set("ro.build.description", "falcon_verizon-user 5.1 LPB23.13-33.7 7 release-keys");
            property_set("ro.build.fingerprint", "motorola/falcon_verizon/falcon_cdma:5.1/LPB23.13-33.7/7:user/release-keys");
            property_set("ro.mot.build.customerid", "verizon");
            property_set("ro.cdma.home.operator.alpha", "Verizon");
            property_set("ro.cdma.home.operator.numeric", "310004");
            property_set("ro.com.google.clientidbase.ms", "android-verizon");
            property_set("ro.com.google.clientidbase.am", "android-verizon");
            property_set("ro.com.google.clientidbase.yt", "android-verizon");
            property_set("persist.radio.nw_mtu_enabled", "true");
        } else if (carrier == BOOST) {
            /* xt1031 */
            property_set("ro.build.description", "falcon_boost-user 5.1 LPB23.13-56 55 release-keys");
            property_set("ro.build.fingerprint", "motorola/falcon_boost/falcon_cdma:5.1/LPB23.13-56/55:user/release-keys");
            property_set("ro.mot.build.customerid", "sprint");
            property_set("ro.cdma.home.operator.alpha", "Boost Mobile");
            property_set("ro.cdma.home.operator.numeric", "311870");
        } else if (carrier == USC) {
            property_set("ro.build.description", "falcon_usc-user 5.1 LPB23.13-33.6 8 release-keys");
            property_set("ro.build.fingerprint", "motorola/falcon_usc/falcon_cdma:5.1/LPB23.13-33.6/8:user/release-keys");
            property_set("ro.mot.build.customerid", "usc");
            property_set("ro.cdma.home.operator.alpha", "U.S. Cellular");
            property_set("ro.cdma.home.operator.numeric", "311220");
        }
        property_set("ro.product.device", "falcon_cdma");
        property_set("ro.build.product", "falcon_cdma");
        property_set("ro.telephony.default_cdma_sub", "1");
        property_set("ro.telephony.default_network", "4");
        property_set("ro.telephony.gsm-routes-us-smsc", "1");
        property_set("persist.radio.vrte_logic", "2");
        property_set("persist.radio.0x9e_not_callname", "1");
        property_set("persist.radio.multisim.config", "");
        property_set("persist.radio.skip_data_check", "1");
        property_set("persist.ril.max.crit.qmi.fails", "4");
        property_set("persist.radio.call_type", "1");
        property_set("persist.radio.mode_pref_nv10", "1");
        property_set("persist.radio.snapshot_timer", "22");
        property_set("persist.radio.snapshot_enabled", "1");
        property_set("ro.cdma.home.operator.isnan", "1");
        property_set("ro.cdma.otaspnumschema", "SELC,1,80,99");
        property_set("ro.cdma.data_retry_config", "max_retries=infinite,0,0,10000,10000,100000,10000,10000,10000,10000,140000,540000,960000");
        property_set("ro.gsm.data_retry_config", "default_randomization=2000,max_retries=infinite,1000,1000,80000,125000,485000,905000");
        property_set("ro.mot.ignore_csim_appid", "true");
        property_set("telephony.lteOnCdmaDevice", "0");
    } else if (ISMATCH(radio, "0x5")) {
        /* xt1033 */
        property_set("ro.product.device", "falcon_umtsds");
        property_set("ro.build.description", "falcon_retbr_ds-user 5.1 LPB23.13-56 58 release-keys");
        property_set("ro.build.fingerprint", "motorola/falcon_retbr_ds/falcon_umtsds:5.1/LPB23.13-56/58:user/release-keys");
        property_set("ro.build.product", "falcon_umtsds");
        property_set("ro.mot.build.customerid", "RETBR");
        property_set("ro.telephony.default_network", "0,1");
        property_set("ro.telephony.ril.config", "simactivation");
        property_set("persist.radio.multisim.config", "dsds");
        property_set("persist.radio.dont_use_dsd", "true");
        property_set("persist.radio.plmn_name_cmp", "1");
    } else if (ISMATCH(radio, "0x6")) {
        /* xt1034 */
        property_set("ro.product.device", "falcon_umts");
        property_set("ro.build.description", "falcon_retuaws-user 5.1 LPB23.13-58 61 release-keys");
        property_set("ro.build.fingerprint", "motorola/falcon_retuaws/falcon_umts:5.1/LPB23.13-58/61:user/release-keys");
        property_set("ro.build.product", "falcon_umts");
        property_set("ro.mot.build.customerid", "retusa_aws");
        property_set("ro.telephony.default_network", "0");
        property_set("persist.radio.multisim.config", "");
    }

    property_get("ro.product.device", device);
    INFO("Found radio id: %s data %s setting build properties for %s device\n", radio, fstype, device);
}

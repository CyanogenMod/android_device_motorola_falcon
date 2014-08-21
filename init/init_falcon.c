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

void init_msm_properties(unsigned long msm_id, unsigned long msm_ver, char *board_type)
{
    char platform[PROP_VALUE_MAX];
    char radio[PROP_VALUE_MAX];
    char device[PROP_VALUE_MAX];
    char devicename[PROP_VALUE_MAX];
    char cdma_variant[92];
    char fstype[92];
    FILE *fp;
    int rc;

    UNUSED(msm_id);
    UNUSED(msm_ver);
    UNUSED(board_type);

    rc = property_get("ro.board.platform", platform);
    if (!rc || !ISMATCH(platform, ANDROID_TARGET))
        return;

    property_get("ro.boot.radio", radio);
    fp = popen("/system/bin/ls -la /fsg/falcon_3.img.gz | /system/xbin/cut -d '_' -f3", "r");
    fgets(cdma_variant, sizeof(cdma_variant), fp);
    pclose(fp);
    fp = popen("/system/bin/blkid /dev/block/platform/msm_sdcc.1/by-name/userdata | /system/xbin/cut -d ' ' -f3", "r");
    fgets(fstype, sizeof(fstype), fp);
    pclose(fp);
    if (strstr(fstype, "ext4")) {
        /* xt1032 GPE */
        property_set("ro.product.device", "falcon_gpe");
        property_set("ro.product.model", "Moto G");
        property_set("ro.build.description", "falcon_gpe-user 4.4.4 KTU84P.M003 18 release-keys");
        property_set("ro.build.fingerprint", "motorola/falcon_gpe/falcon_umts:4.4.4/KTU84P.M003/18:user/release-keys");
        property_set("ro.mot.build.customerid", "retusa_glb");
        property_set("ro.telephony.default_network", "0");
        property_set("persist.radio.multisim.config", "");
    } else if (ISMATCH(radio, "0x1")) {
        /* xt1032 */
        property_set("ro.product.device", "falcon_umts");
        property_set("ro.product.model", "Moto G");
        property_set("ro.build.description", "falcon_retgb-user 4.4.4 KXB21.14-L1.40 36 release-keys");
        property_set("ro.build.fingerprint", "motorola/falcon_retgb/falcon_umts:4.4.4/KXB21.14-L1.40/36:user/release-keys");
        property_set("ro.mot.build.customerid", "RTGB");
        property_set("ro.telephony.default_network", "0");
        property_set("persist.radio.multisim.config", "");
    } else if (ISMATCH(radio, "0x3")) {
        /* cdma */
        INFO("CDMA variant=%s", cdma_variant);
        if (ISMATCH(cdma_variant, "verizon")) {
            /* xt1028 */
            property_set("ro.product.device", "falcon_cdma");
            property_set("ro.product.model", "Moto G");
            property_set("ro.build.description", "falcon_verizon-user 4.3 14.10.0Q3.X-84-14 16 release-keys");
            property_set("ro.build.fingerprint", "motorola/falcon_verizon/falcon_cdma:4.3/14.10.0Q3.X-84-14/16:user/release-keys");
            property_set("persist.radio.multisim.config", "");
            property_set("ro.mot.build.customerid", "verizon");
            property_set("ro.cdma.home.operator.alpha", "Verizon");
            property_set("ro.cdma.home.operator.numeric", "310004");
            property_set("ro.com.google.clientidbase.ms", "android-verizon");
            property_set("ro.com.google.clientidbase.am", "android-verizon");
            property_set("ro.com.google.clientidbase.yt", "android-verizon");
        } else {
            /* xt1031 */
            property_set("ro.product.device", "falcon_cdma");
            property_set("ro.product.model", "Moto G");
            property_set("ro.build.description", "falcon_boost-user 4.4.2 KXB20.9-1.10-1.18 18 release-keys");
            property_set("ro.build.fingerprint", "motorola/falcon_boost/falcon_cdma:4.4.2/KXB20.9-1.10-1.18/18:user/release-keys");
            property_set("persist.radio.multisim.config", "");
            property_set("ro.mot.build.customerid", "sprint");
            property_set("ro.cdma.home.operator.alpha", "Boost Mobile");
            property_set("ro.cdma.home.operator.numeric", "311870");
        }
        property_set("ro.telephony.default_network", "4");
        property_set("ro.telephony.gsm-routes-us-smsc", "1");
        property_set("persist.radio.vrte_logic", "2");
        property_set("persist.radio.0x9e_not_callname", "1");
        property_set("persist.radio.skip_data_check", "1");
        property_set("persist.ril.max.crit.qmi.fails", "4");
        property_set("ro.cdma.home.operator.isnan", "1");
        property_set("ro.cdma.otaspnumschema", "SELC,1,80,99");
        property_set("ro.cdma.data_retry_config", "max_retries=infinite,0,0,10000,10000,100000,10000,10000,10000,10000,140000,540000,960000");
        property_set("ro.gsm.data_retry_config", "default_randomization=2000,max_retries=infinite,1000,1000,80000,125000,485000,905000");
    } else if (ISMATCH(radio, "0x5")) {
        /* xt1033 */
        property_set("ro.product.device", "falcon_umtsds");
        property_set("ro.product.model", "Moto G");
        property_set("ro.build.description", "falcon_retbr_ds-user 4.4.3 KXB21.14-L1.32 30 release-keys");
        property_set("ro.build.fingerprint", "motorola/falcon_retbr_ds/falcon_umtsds:4.4.3/KXB21.14-L1.32/30:user/release-keys");
        property_set("ro.mot.build.customerid", "RETBR");
        property_set("ro.telephony.default_network", "0");
        property_set("persist.radio.multisim.config", "dsds");
        property_set("persist.radio.dont_use_dsd", "true");
        property_set("persist.radio.plmn_name_cmp", "1");
    } else if (ISMATCH(radio, "0x6")) {
        /* xt1034 */
        property_set("ro.product.device", "falcon_umts");
        property_set("ro.product.model", "Moto G");
        property_set("ro.build.description", "falcon_retuaws-user 4.4.3 KXB21.14-L1.32 30 release-keys");
        property_set("ro.build.fingerprint", "motorola/falcon_retuaws/falcon_umts:4.4.3/KXB21.14-L1.32/30:user/release-keys");
        property_set("ro.mot.build.customerid", "retusa_aws");
        property_set("ro.telephony.default_network", "0");
        property_set("persist.radio.multisim.config", "");
    }
    property_get("ro.product.device", device);
    strlcpy(devicename, device, sizeof(devicename));
    INFO("Found radio id: %s data %s setting build properties for %s device\n", radio, fstype, devicename);
}

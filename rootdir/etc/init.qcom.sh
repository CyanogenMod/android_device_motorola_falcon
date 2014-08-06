#!/system/bin/sh
# Copyright (c) 2009-2013, The Linux Foundation. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in the
#       documentation and/or other materials provided with the distribution.
#     * Neither the name of The Linux Foundation nor
#       the names of its contributors may be used to endorse or promote
#       products derived from this software without specific prior written
#       permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
# NON-INFRINGEMENT ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
# OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

target=`getprop ro.board.platform`
if [ -f /sys/devices/soc0/soc_id ]; then
    platformid=`cat /sys/devices/soc0/soc_id`
else
    platformid=`cat /sys/devices/system/soc/soc0/id`
fi
#
# Function to start sensors for DSPS enabled platforms
#
start_sensors()
{
    if [ -c /dev/msm_dsps -o -c /dev/sensors ]; then
        mkdir -p /data/system/sensors
        touch /data/system/sensors/settings
        chmod -h 775 /data/system/sensors
        chmod -h 664 /data/system/sensors/settings
        chown -h system /data/system/sensors/settings

        mkdir -p /data/misc/sensors
        chmod -h 775 /data/misc/sensors

        if [ ! -s /data/system/sensors/settings ]; then
            # If the settings file is empty, enable sensors HAL
            # Otherwise leave the file with it's current contents
            echo 1 > /data/system/sensors/settings
        fi
        start sensors
    fi
}

start_battery_monitor()
{
	if ls /sys/bus/spmi/devices/qpnp-bms-*/fcc_data ; then
		chown -h root.system /sys/module/pm8921_bms/parameters/*
		chown -h root.system /sys/module/qpnp_bms/parameters/*
		chown -h root.system /sys/bus/spmi/devices/qpnp-bms-*/fcc_data
		chown -h root.system /sys/bus/spmi/devices/qpnp-bms-*/fcc_temp
		chown -h root.system /sys/bus/spmi/devices/qpnp-bms-*/fcc_chgcyl
		chmod -h 0660 /sys/module/qpnp_bms/parameters/*
		chmod -h 0660 /sys/module/pm8921_bms/parameters/*
		mkdir -p /data/bms
		chown -h root.system /data/bms
		chmod -h 0770 /data/bms
		start battery_monitor
	fi
}

start_charger_monitor()
{
	if ls /sys/module/qpnp_charger/parameters/charger_monitor; then
		chown -h root.system /sys/module/qpnp_charger/parameters/*
		chown -h root.system /sys/class/power_supply/battery/input_current_max
		chown -h root.system /sys/class/power_supply/battery/input_current_trim
		chown -h root.system /sys/class/power_supply/battery/input_current_settled
		chown -h root.system /sys/class/power_supply/battery/voltage_min
		chmod -h 0664 /sys/class/power_supply/battery/input_current_max
		chmod -h 0664 /sys/class/power_supply/battery/input_current_trim
		chmod -h 0664 /sys/class/power_supply/battery/input_current_settled
		chmod -h 0664 /sys/class/power_supply/battery/voltage_min
		chmod -h 0664 /sys/module/qpnp_charger/parameters/charger_monitor
		start charger_monitor
	fi
}

baseband=`getprop ro.baseband`
izat_premium_enablement=`getprop ro.qc.sdk.izat.premium_enabled`
izat_service_mask=`getprop ro.qc.sdk.izat.service_mask`

#
# Suppress default route installation during RA for IPV6; user space will take
# care of this
# exception default ifc
for file in /proc/sys/net/ipv6/conf/*
do
  echo 0 > $file/accept_ra_defrtr
done
echo 1 > /proc/sys/net/ipv6/conf/default/accept_ra_defrtr

#
# Start gpsone_daemon for SVLTE Type I & II devices
#

# platform id 126 is for MSM8974
case "$platformid" in
        "126")
        start gpsone_daemon
esac
case "$target" in
        "msm7630_fusion")
        start gpsone_daemon
esac
case "$baseband" in
        "svlte2a")
        start gpsone_daemon
        start bridgemgrd
        ;;
        "sglte" | "sglte2")
        start gpsone_daemon
        ;;
esac

let "izat_service_gtp_wifi=$izat_service_mask & 2#1"
let "izat_service_gtp_wwan_lite=($izat_service_mask & 2#10)>>1"
let "izat_service_pip=($izat_service_mask & 2#100)>>2"

if [ "$izat_premium_enablement" -ne 1 ]; then
    if [ "$izat_service_gtp_wifi" -ne 0 ]; then
# GTP WIFI bit shall be masked by the premium service flag
        let "izat_service_gtp_wifi=0"
    fi
fi

if [ "$izat_service_gtp_wwan_lite" -ne 0 ] ||
   [ "$izat_service_gtp_wifi" -ne 0 ] ||
   [ "$izat_service_pip" -ne 0 ]; then
# OS Agent would also be started under the same condition
    start location_mq
fi

if [ "$izat_service_gtp_wwan_lite" -ne 0 ] ||
   [ "$izat_service_gtp_wifi" -ne 0 ]; then
# start GTP services shared by WiFi and WWAN Lite
    start xtwifi_inet
    start xtwifi_client
fi

if [ "$izat_service_gtp_wifi" -ne 0 ] ||
   [ "$izat_service_pip" -ne 0 ]; then
# advanced WiFi scan service shared by WiFi and PIP
    start lowi-server
fi

if [ "$izat_service_pip" -ne 0 ]; then
# PIP services
    start quipc_main
    start quipc_igsn
fi

# Qcom sensor support disabled
# start_sensors

case "$target" in
    "msm7630_surf" | "msm7630_1x" | "msm7630_fusion")
        if [ -f /sys/devices/soc0/hw_platform ]; then
            value=`cat /sys/devices/soc0/hw_platform`
        else
            value=`cat /sys/devices/system/soc/soc0/hw_platform`
        fi
        case "$value" in
            "Fluid")
             start profiler_daemon;;
        esac
        ;;
    "msm8660" )
        if [ -f /sys/devices/soc0/hw_platform ]; then
            platformvalue=`cat /sys/devices/soc0/hw_platform`
        else
            platformvalue=`cat /sys/devices/system/soc/soc0/hw_platform`
        fi
        case "$platformvalue" in
            "Fluid")
                start profiler_daemon;;
        esac
        ;;
    "msm8960")
        case "$baseband" in
            "msm")
                start_battery_monitor;;
        esac

        if [ -f /sys/devices/soc0/hw_platform ]; then
            platformvalue=`cat /sys/devices/soc0/hw_platform`
        else
            platformvalue=`cat /sys/devices/system/soc/soc0/hw_platform`
        fi
        case "$platformvalue" in
             "Fluid")
                 start profiler_daemon;;
             "Liquid")
                 start profiler_daemon;;
        esac
        ;;
    "msm8974")
        platformvalue=`cat /sys/devices/soc0/hw_platform`
        case "$platformvalue" in
             "Fluid")
                 start profiler_daemon;;
             "Liquid")
                 start profiler_daemon;;
        esac
        case "$baseband" in
            "msm")
                start_battery_monitor
                ;;
        esac
        start_charger_monitor
        ;;
    "msm8226")
        start_charger_monitor
        ;;
    "msm8610")
        start_charger_monitor
        ;;
esac

#!/system/bin/sh
# Copyright (c) 2012, The Linux Foundation. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
#       copyright notice, this list of conditions and the following
#       disclaimer in the documentation and/or other materials provided
#       with the distribution.
#     * Neither the name of The Linux Foundation nor the names of its
#       contributors may be used to endorse or promote products derived
#       from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
# WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
# ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
# BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
# BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
# OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
# IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
#

# No path is set up at this point so we have to do it here.
PATH=/sbin:/system/sbin:/system/bin:/system/xbin
export PATH

THERMALD_CONF_SYMLINK=/etc/thermald.conf

# Set a default value
setprop qcom.thermal thermald

# Figure out if thermal-engine should start
platformid=`cat /sys/devices/system/soc/soc0/id`
hw_platform=`cat /sys/devices/system/soc/soc0/hw_platform`
case "$platformid" in
    "153") #APQ/MPQ8064ab
    setprop qcom.thermal thermal-engine
    ;;
esac

# Check if symlink does not exist
if [ ! -h $THERMALD_CONF_SYMLINK ]; then
 # create symlink to target-specific config file
 case "$platformid" in
     "109" | "130" | "172") #APQ/MPQ8064 & APQ/MPQ8064aa
     ln -s /etc/thermald-8064.conf $THERMALD_CONF_SYMLINK 2>/dev/null
     ;;

     "153") #APQ/MPQ8064ab
     ln -s /etc/thermald-8064ab.conf $THERMALD_CONF_SYMLINK 2>/dev/null
     ;;

     "116" | "117" | "118" | "119" | "120" | "121" | "142" | "143" | "144" | "160" | "179" | "180") #MSM8x30&MSM8x27
     case "$hw_platform" in
          "QRD") #8x30 QRD
          ln -s /etc/thermald-8930-qrd.conf $THERMALD_CONF_SYMLINK 2>/dev/null
          ;;

          *)
          ln -s /etc/thermald-8930.conf $THERMALD_CONF_SYMLINK 2>/dev/null
	  ;;
     esac
     ;;

     "154" | "155" | "156" | "157" | "181") #MSM8930ab
     ln -s /etc/thermald-8930ab.conf $THERMALD_CONF_SYMLINK 2>/dev/null
     ;;

     "139" | "140" | "141") #MSM8960ab
     ln -s /etc/thermald-8960ab.conf $THERMALD_CONF_SYMLINK 2>/dev/null
     ;;

     "158") #falcon do nothing
     ;;

     *) #MSM8960, etc
     ln -s /etc/thermald-8960.conf $THERMALD_CONF_SYMLINK 2>/dev/null
     ;;
 esac
fi

# Check if symlink does not exist
if [ ! -h /dev/thermald.conf ]; then
 # create symlink to target-specific config file
 case "$platformid" in
     "87") #msm8960 - 3.4
     ln -s /etc/thermald-8960.conf /dev/thermald.conf 2>/dev/null
     ;;

     "138") #ghost
     ln -s /etc/thermald-ghost.conf /dev/thermald.conf 2>/dev/null
     ;;
 esac
fi

THERMAL_ENGINE_CONF_SYMLINK=/etc/thermal-engine.conf
# Check if symlink does not exist
if [ ! -h $THERMAL_ENGINE_CONF_SYMLINK ]; then
 # create symlink to target-specific config file
 case "$platformid" in
     "109" | "130" | "172") #APQ/MPQ8064 & APQ/MPQ8064aa
     ln -s /etc/thermal-engine-8064.conf $THERMAL_ENGINE_CONF_SYMLINK 2>/dev/null
     ;;

     "153") #APQ/MPQ8064ab
     ln -s /etc/thermal-engine-8064ab.conf $THERMAL_ENGINE_CONF_SYMLINK 2>/dev/null
     ;;

     "116" | "117" | "118" | "119" | "120" | "121" | "142" | "143" | "144" | "160" | "179" | "180" | "181") #MSM8x30&MSM8x27
     ln -s /etc/thermal-engine-8930.conf $THERMAL_ENGINE_CONF_SYMLINK 2>/dev/null
     ;;

     *) #MSM8960, etc
     ln -s /etc/thermal-engine-8960.conf $THERMAL_ENGINE_CONF_SYMLINK 2>/dev/null
     ;;
 esac
fi


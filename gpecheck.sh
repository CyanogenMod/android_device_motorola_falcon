#!/sbin/sh
#
# Check if this is a GPE device and set a prop.
# If userdata is type ext4 we will install the gpe-boot.img
#

FSTYPE=`/sbin/blkid /dev/block/mmcblk0p36 | cut -d ' ' -f3 | cut -d '"' -f2`

if [ "$FSTYPE" == "ext4" ]
then
  setprop ro.gpe.device true
else
  setprop ro.gpe.device false
fi

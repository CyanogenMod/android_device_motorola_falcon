#!/sbin/bbx sh
#
# Check if this is a GPE device and copy in the right fstab
#

BDEV=`/sbin/bbx readlink /dev/block/platform/msm_sdcc.1/by-name/userdata`
FSTYPE=`/sbin/bbx blkid $BDEV | /sbin/bbx cut -d ' ' -f3 | /sbin/bbx cut -d '"' -f2`

if [ "$FSTYPE" == "ext4" ]
then
  /sbin/bbx cp -f /gpe-fstab.qcom /etc/recovery.fstab
  /sbin/bbx mv -f /gpe-fstab.qcom /fstab.qcom
else
  /sbin/bbx rm /gpe-fstab.qcom
fi

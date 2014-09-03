#!/bin/sh

set -e

export DEVICE=falcon
export VENDOR=motorola
./../../$VENDOR/msm8226-common/setup-makefiles.sh $@

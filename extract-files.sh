#!/bin/sh

set -e

export VENDOR=motorola
export DEVICE=falcon
./../../$VENDOR/msm8226-common/extract-files.sh $@

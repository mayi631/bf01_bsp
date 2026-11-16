#!/bin/sh
if [ -r /mnt/data/bf01-boot-priority ] && [ -x /system/ko/bf01-modules ]; then
    exec /system/ko/bf01-modules core
fi
if [ -r /mnt/data/bf01-boot-priority ] && [ -x /mnt/system/ko/bf01-modules ]; then
    exec /mnt/system/ko/bf01-modules core
fi
${CVI_SHOPTS}
#
# Define kernel modules sequence
#

modules_seq="
/mnt/system/ko/cv181x_sys.ko
/mnt/system/ko/cv181x_clock_cooling.ko
/mnt/system/ko/cvi_ipcm.ko
/mnt/system/ko/cv181x_tpu.ko
/mnt/system/ko/cv181x_ive.ko
/mnt/system/ko/cv181x_wdt.ko
"

#
# Start to insert kernel modules
#
if [ -n "$modules_seq" ]; then
    for mod in $modules_seq; do
        insmod "$mod"
    done
fi

dmesg -n 4

exit $?
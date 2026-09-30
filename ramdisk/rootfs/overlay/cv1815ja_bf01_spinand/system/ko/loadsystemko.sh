#!/bin/sh
${CVI_SHOPTS}
#
# Define kernel modules sequence
#

modules_seq="
/mnt/system/ko/cv181x_sys.ko
/mnt/system/ko/cv181x_clock_cooling.ko
/mnt/system/ko/cv181x_sys.ko
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

echo 3 > /proc/sys/vm/drop_caches
dmesg -n 4

exit $?
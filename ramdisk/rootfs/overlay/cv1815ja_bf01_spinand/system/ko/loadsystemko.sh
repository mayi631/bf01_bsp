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

# cv181x_sys / cv181x_clock_cooling / cvi_ipcm / cv181x_tpu / cv181x_ive and
# cv181x_wdt are built into the kernel (CONFIG_CV181X_* / CONFIG_CV181X_WDT = y
# in the board defconfig), so the .ko files no longer exist. The script stays
# because S99user calls it and the bf01-modules early-boot path above uses it.
modules_seq=""

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
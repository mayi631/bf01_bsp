#!/bin/sh
${CVI_SHOPTS}
#
# Define kernel modules sequence
#
# cv181x_sys / cv181x_clock_cooling / cvi_ipcm / cv181x_tpu / cv181x_ive and
# cv181x_wdt are built into the kernel (CONFIG_CV181X_* / CONFIG_CV181X_WDT = y
# in the board fastboot defconfig), so nothing is insmod'ed here any more.
# The script is kept because S99user and board-side tooling call it.

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

echo "loadsystemko: done uptime=$(cut -d' ' -f1 /proc/uptime)" > /dev/kmsg

exit $?

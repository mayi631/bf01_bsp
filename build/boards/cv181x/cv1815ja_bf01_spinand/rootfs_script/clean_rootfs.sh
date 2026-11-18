#!/bin/bash

SYSTEM_DIR=$1
# Refuse to run with an empty or non-directory argument: every rm below is
# rooted at $SYSTEM_DIR, so an empty value would operate on the host /.
if [ -z "$SYSTEM_DIR" ] || [ ! -d "$SYSTEM_DIR" ]; then
    echo "clean_rootfs.sh: invalid SYSTEM_DIR '$SYSTEM_DIR'" >&2
    exit 1
fi
rm -f $SYSTEM_DIR/mnt/system/usr/lib/libz*
rm -f $SYSTEM_DIR/mnt/system/lib/libz*

rm -f $SYSTEM_DIR/mnt/system/usr/lib/3rd/libcvi*
rm -f $SYSTEM_DIR/mnt/system/usr/lib/3rd/libmad*
rm -f $SYSTEM_DIR/mnt/system/usr/lib/3rd/libmp3*
rm -f $SYSTEM_DIR/mnt/system/usr/lib/3rd/libopencv*
rm -f $SYSTEM_DIR/mnt/system/usr/lib/libopencv*
rm -f $SYSTEM_DIR/mnt/system/usr/lib/libcvi_rtsp.so
rm -f $SYSTEM_DIR/mnt/system/data/install/lib/libcvi_rtsp.so
rm -f $SYSTEM_DIR/mnt/system/usr/lib/libcvikernel.so
rm -f $SYSTEM_DIR/mnt/system/usr/lib/*.a
rm -f $SYSTEM_DIR/mnt/system/usr/lib/libgst*
rm -f $SYSTEM_DIR/mnt/system/usr/lib/libg*
rm -f $SYSTEM_DIR/mnt/system/usr/lib/gstreamer-1.0/libgst*
rm -f $SYSTEM_DIR/mnt/system/data/install/lib/libcrypto.so*
rm -f $SYSTEM_DIR/mnt/system/data/install/lib/libssl.so*
rm -f $SYSTEM_DIR/mnt/system/data/install/lib/libcvi_protobuf.so*
rm -f $SYSTEM_DIR/mnt/system/data/install/lib/libprotobuf-lite.so*
rm -f $SYSTEM_DIR/mnt/system/usr/lib/libcviai*
rm -f $SYSTEM_DIR/mnt/system/usr/lib/libcvi_ispd.so*
rm -f $SYSTEM_DIR/mnt/system/usr/lib/libraw_replay.so*
rm -f $SYSTEM_DIR/mnt/system/usr/lib/libcvi_ive_tpu.so*
rm -rf $SYSTEM_DIR/mnt/system/usr/lib/gio
rm -rf $SYSTEM_DIR/mnt/system/usr/lib/glib*
rm -rf $SYSTEM_DIR/mnt/system/usr/lib/gstreamer-1.0*
rm -rf $SYSTEM_DIR/mnt/system/usr/libexec*
cp $SYSTEM_DIR/mnt/system/usr/bin/alios_cli $SYSTEM_DIR/bin/
rm -rf $SYSTEM_DIR/mnt/system/usr/bin
rm -f $SYSTEM_DIR/mnt/system/usr/lib/libcvimath.so
rm -f $SYSTEM_DIR/mnt/system/usr/lib/libcviruntime.so
rm -f $SYSTEM_DIR/mnt/system/usr/lib/libcnpy.so
rm -f $SYSTEM_DIR/mnt/system/usr/lib/libcipher.so

rm -f $SYSTEM_DIR/mnt/system/lib/libcipher.so*
rm -f $SYSTEM_DIR/mnt/system/lib/libcvi_ispd.so*
rm -f $SYSTEM_DIR/mnt/system/lib/libraw_replay.so*
rm -f $SYSTEM_DIR/mnt/system/lib/libmad.so*
rm -f $SYSTEM_DIR/mnt/system/lib/libmp3*
rm -f $SYSTEM_DIR/mnt/system/lib/libnanomsg*

rm -rf $SYSTEM_DIR/etc/init.d/S01syslogd
rm -rf $SYSTEM_DIR/etc/init.d/S02klogd
rm -rf $SYSTEM_DIR/etc/init.d/S40network

#del 3rdparty lib
#del thttpd/libwebsockets lib
rm -f $SYSTEM_DIR/mnt/system/lib/libthttpd*
rm -f $SYSTEM_DIR/mnt/system/lib/libwebsocket*
if [ $SDK_VER = "uclibc" ]
then
    #del opencv lib
    rm -f $SYSTEM_DIR/mnt/system/lib/libopencv*
    #del ffmpeg lib
    rm -f $SYSTEM_DIR/mnt/system/lib/libav*
    #save /mnt/system/lib/ openssl lib; need by ntpdate/wpa_supplicant
else
    #glibc ramdisk(rootfs/common_arm/usr/lib/) has libcrypto.so and libssl.so
    #del openssl
    rm -f $SYSTEM_DIR/mnt/system/lib/libssl*
    rm -f $SYSTEM_DIR/mnt/system/lib/libcrypto*
fi

# BF01: these drivers are built into the kernel, the .ko files are dead weight
for k in cv181x_sys cv181x_clock_cooling cvi_ipcm cv181x_tpu cv181x_ive cv181x_wdt; do
    rm -f $SYSTEM_DIR/mnt/system/ko/$k.ko
done

du -sh $SYSTEM_DIR/* |sort -rh
du -sh $SYSTEM_DIR/mnt/* |sort -rh
du -sh $SYSTEM_DIR/mnt/system/* |sort -rh
du -sh $SYSTEM_DIR/mnt/system/lib/*  |sort -rh
du -sh $SYSTEM_DIR/mnt/system/data/install/* |sort -rh
du -sh $SYSTEM_DIR/usr/* |sort -rh

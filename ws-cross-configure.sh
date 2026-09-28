#!/bin/bash
# Cross-configure Wireshark 4.2.5 for aarch64-linux-ohos inside WSL.
# All paths literal: variable expansion gets mangled when passed via wsl.exe.
set -e

NDK=/opt/ohos/sdk-6.1.0.830/command-line-tools/sdk/default/openharmony/native
WS=/home/mu/wireshark-4.2.5

export PKG_CONFIG_PATH=/home/mu/glib-ohos/lib/pkgconfig:/home/mu/gcrypt-ohos/lib/pkgconfig:/home/mu/gpgerr-ohos/lib/pkgconfig:/home/mu/pcap-ohos/lib/pkgconfig:/home/mu/zlib-ohos/lib/pkgconfig:/home/mu/cares-ohos/lib/pkgconfig
export PKG_CONFIG_LIBDIR="$PKG_CONFIG_PATH"
export PKG_CONFIG_SYSROOT_DIR=

cd "$WS"
rm -rf build-ohos

cmake -S . -B build-ohos -G Ninja \
  -DCMAKE_MAKE_PROGRAM=/usr/bin/ninja \
  -DCMAKE_TOOLCHAIN_FILE=/mnt/c/Users/mu/Desktop/code/thirty/ws-ohos.toolchain.cmake \
  -DCMAKE_MODULE_PATH=/mnt/c/Users/mu/Desktop/code/thirty/cmake-modules \
  -DOHOS_ARCH=arm64-v8a \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="/home/mu/pcap-ohos;/home/mu/zlib-ohos;/home/mu/glib-ohos;/home/mu/gcrypt-ohos;/home/mu/gpgerr-ohos;/home/mu/cares-ohos" \
  -DCMAKE_FIND_ROOT_PATH_MODE_PROGRAM=NEVER \
  -DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=BOTH \
  -DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=BOTH \
  -DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=BOTH \
  -DBUILD_wireshark=OFF \
  -DBUILD_sharkd=OFF \
  -DBUILD_rawshark=OFF \
  -DBUILD_corbaidl2wireshark=OFF \
  -DBUILD_dcerpidl2wireshark=OFF \
  -DBUILD_tshark=ON \
  -DBUILD_dumpcap=ON \
  -DBUILD_editcap=OFF \
  -DBUILD_capinfos=OFF \
  -DBUILD_captype=OFF \
  -DBUILD_dftest=OFF \
  -DBUILD_randpkt=OFF \
  -DBUILD_reordercap=OFF \
  -DBUILD_text2pcap=OFF \
  -DBUILD_mergecap=OFF \
  -DBUILD_writecap=OFF \
  -DBUILD_androiddump=OFF \
  -DBUILD_sshdump=OFF \
  -DBUILD_ciscodump=OFF \
  -DBUILD_dpauxmon=OFF \
  -DBUILD_randpktdump=OFF \
  -DBUILD_sdjournal=OFF \
  -DBUILD_udpdump=OFF \
  -DBUILD_wifidump=OFF \
  -DBUILD_etwdump=OFF \
  -DBUILD_falcodump=OFF \
  -DBUILD_mmsdump=OFF \
  -DBUILD_manpages=OFF \
  -DBUILD_html_docs=OFF \
  -DBUILD_pcapng_common=OFF \
  -DENABLE_GNUTLS=OFF \
  -DENABLE_SMI=OFF \
  -DENABLE_LUA=OFF \
  -DENABLE_LZ4=OFF \
  -DENABLE_NGHTTP2=OFF \
  -DENABLE_NGHTTP3=OFF \
  -DENABLE_BROTLI=OFF \
  -DENABLE_ZSTD=OFF \
  -DENABLE_SNAPPY=OFF \
  -DENABLE_LIBSSH=OFF \
  -DENABLE_LIBXML2=OFF \
  -DENABLE_KERBEROS=OFF \
  -DENABLE_SBC=OFF \
  -DENABLE_BCG729=OFF \
  -DENABLE_OPUS=OFF \
  -DENABLE_SPANDSP=OFF \
  -DENABLE_LIBCAP=OFF \
  -DENABLE_LIBNL=OFF \
  -DENABLE_SYSPROF=OFF \
  -DENABLE_LIBTRACEEVENT=OFF \
  -DENABLE_SPEEXDSP=OFF \
  -DENABLE_ILBC=OFF \
  -DENABLE_PLAY=OFF \
  -DENABLE_SHELL_LIKE=OFF \
  -DENABLE_PLUGINS=OFF \
  -DUSE_qt6=OFF \
  -DENABLE_PCAP=ON \
  -DENABLE_ZLIB=ON \
  -DENABLE_GCRYPT=ON \
  -DENABLE_CARES=ON \
  > /tmp/wscfg.log 2>&1 || { echo CONFIGURE_FAILED; tail -40 /tmp/wscfg.log; exit 1; }

echo CONFIGURE_OK
grep -E '^CMAKE_(C|CXX)_COMPILER:|^CMAKE_SYSTEM_NAME' build-ohos/CMakeCache.txt

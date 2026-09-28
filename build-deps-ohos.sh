#!/bin/bash
# Rebuild zlib, libpcap and c-ares as aarch64-linux-ohos.
# The earlier builds of these three silently used the host compiler (x86-64),
# which is why Wireshark's pcap probe died with "incompatible with aarch64linux".

NDK=/opt/ohos/sdk-6.1.0.830/command-line-tools/sdk/default/openharmony/native
CLANG="$NDK/llvm/bin/clang"
SYSROOT="$NDK/sysroot"
TOOLCHAIN=/mnt/c/Users/mu/Desktop/code/thirty/ws-ohos.toolchain.cmake
TARGET="--target=aarch64-linux-ohos --sysroot=$SYSROOT"

set -e

echo "===== zlib ====="
rm -rf /home/mu/zlib-ohos
cd /home/mu/zlib-1.3.1
make distclean >/dev/null 2>&1 || true
CC="$CLANG" \
CFLAGS="$TARGET -O2 -fPIC -D__MUSL__" \
LDFLAGS="$TARGET -fuse-ld=lld -rtlib=compiler-rt" \
AR="$NDK/llvm/bin/llvm-ar" \
RANLIB="$NDK/llvm/bin/llvm-ranlib" \
./configure --prefix=/home/mu/zlib-ohos > /tmp/zlib-cfg.log 2>&1 || { echo ZLIB_CONFIGURE_FAILED; tail -20 /tmp/zlib-cfg.log; exit 1; }
make -j"$(nproc)" > /tmp/zlib-build.log 2>&1 || { echo ZLIB_BUILD_FAILED; tail -20 /tmp/zlib-build.log; exit 1; }
make install > /tmp/zlib-install.log 2>&1 || { echo ZLIB_INSTALL_FAILED; tail -20 /tmp/zlib-install.log; exit 1; }
file /home/mu/zlib-ohos/lib/libz.so.* | sed 's/^[^:]*: //' | head -2

echo "===== libpcap ====="
rm -rf /home/mu/pcap-ohos /home/mu/libpcap-ohos/build-ohos
cmake -S /home/mu/libpcap-ohos -B /home/mu/libpcap-ohos/build-ohos -G Ninja \
  -DCMAKE_MAKE_PROGRAM=/usr/bin/ninja \
  -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" \
  -DCMAKE_MODULE_PATH=/mnt/c/Users/mu/Desktop/code/thirty/cmake-modules \
  -DOHOS_ARCH=arm64-v8a \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/home/mu/pcap-ohos \
  -DBUILD_SHARED_LIBS=ON \
  -DBUILD_WITH_LIBNL=OFF \
  -DENABLE_REMOTE=OFF \
  -DCMAKE_DISABLE_FIND_PACKAGE_OpenSSL=TRUE \
  -DDISABLE_BLUETOOTH=ON \
  -DDISABLE_DBUS=ON \
  -DDISABLE_NETMAP=ON \
  -DDISABLE_RDMA=ON \
  -DDISABLE_DPDK=ON \
  -DDISABLE_USBCAP=ON \
  -DINET6=ON \
  > /tmp/pcap-cfg.log 2>&1 || { echo PCAP_CONFIGURE_FAILED; tail -25 /tmp/pcap-cfg.log; exit 1; }
ninja -C /home/mu/libpcap-ohos/build-ohos -j"$(nproc)" > /tmp/pcap-build.log 2>&1 || { echo PCAP_BUILD_FAILED; grep -m5 -B3 'error:' /tmp/pcap-build.log; tail -5 /tmp/pcap-build.log; exit 1; }
ninja -C /home/mu/libpcap-ohos/build-ohos install > /tmp/pcap-install.log 2>&1 || { echo PCAP_INSTALL_FAILED; tail -20 /tmp/pcap-install.log; exit 1; }
file /home/mu/pcap-ohos/lib/libpcap.so* | sed 's/^[^:]*: //' | head -2

echo "===== c-ares ====="
rm -rf /home/mu/cares-ohos /home/mu/c-ares-1.19.1/build-ohos
cmake -S /home/mu/c-ares-1.19.1 -B /home/mu/c-ares-1.19.1/build-ohos -G Ninja \
  -DCMAKE_MAKE_PROGRAM=/usr/bin/ninja \
  -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" \
  -DCMAKE_MODULE_PATH=/mnt/c/Users/mu/Desktop/code/thirty/cmake-modules \
  -DOHOS_ARCH=arm64-v8a \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/home/mu/cares-ohos \
  -DCARES_STATIC=ON \
  -DCARES_SHARED=OFF \
  -DCARES_BUILD_TOOLS=OFF \
  -DCARES_BUILD_CONTAINER_TESTS=OFF \
  > /tmp/cares-cfg.log 2>&1 || { echo CARES_CONFIGURE_FAILED; tail -25 /tmp/cares-cfg.log; exit 1; }
ninja -C /home/mu/c-ares-1.19.1/build-ohos -j"$(nproc)" > /tmp/cares-build.log 2>&1 || { echo CARES_BUILD_FAILED; grep -m5 -B3 'error:' /tmp/cares-build.log; tail -5 /tmp/cares-build.log; exit 1; }
ninja -C /home/mu/c-ares-1.19.1/build-ohos install > /tmp/cares-install.log 2>&1 || { echo CARES_INSTALL_FAILED; tail -20 /tmp/cares-install.log; exit 1; }

echo "DEPS_OK"

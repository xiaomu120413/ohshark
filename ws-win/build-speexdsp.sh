#!/bin/bash
# Cross-compile libspeexdsp for aarch64-linux-ohos (Wireshark's GUI requires it
# for RTP audio playback) and ship the prefix to the Windows build tree.
set -e
NDK=/opt/ohos/sdk-6.1.0.830/command-line-tools/sdk/default/openharmony/native
CLANG="$NDK/llvm/bin/clang"
SYSROOT="$NDK/sysroot"
T="--target=aarch64-linux-ohos --sysroot=$SYSROOT"

cd ~/speexdsp-src
./autogen.sh >/dev/null 2>&1 || true
CC="$CLANG" \
CFLAGS="$T -O2 -fPIC -D__MUSL__" \
LDFLAGS="$T -fuse-ld=lld" \
./configure --host=aarch64-linux --prefix="$HOME/speexdsp-ohos" \
  --disable-shared --enable-static > /tmp/speexdsp-cfg.log 2>&1 \
  || { echo CONFIGURE_FAILED; tail -20 /tmp/speexdsp-cfg.log; exit 1; }
make -j"$(nproc)" > /tmp/speexdsp-build.log 2>&1 \
  || { echo BUILD_FAILED; tail -20 /tmp/speexdsp-build.log; exit 1; }
make install > /tmp/speexdsp-install.log 2>&1

# Mirror to the Windows tree.
rm -rf /mnt/c/Users/mu/Desktop/code/thirty/ws-win/speexdsp-ohos
cp -r ~/speexdsp-ohos /mnt/c/Users/mu/Desktop/code/thirty/ws-win/speexdsp-ohos
echo SPEEXDSP_OK
ls /mnt/c/Users/mu/Desktop/code/thirty/ws-win/speexdsp-ohos/lib/

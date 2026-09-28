#!/bin/bash
# Configure the REAL upstream Wireshark Qt GUI (BUILD_wireshark=ON) for
# aarch64-linux-ohos on Windows, against the cross-built Qt (qtbase-ohos),
# qt5compat, the stub LinguistTools package, and the same dependency prefixes
# used for the engine build. Only the `wireshark` target is built later; CLI
# tools stay off.
set -e
cd "$(dirname "$0")/.."   # repo root: thirty/

export PATH="/c/Users/mu/Desktop/code/thirty/ws-win/winflexbison:/c/Users/mu/Desktop/code/thirty/qtbase-host/bin:$PATH"
SRC="C:/Users/mu/Desktop/code/thirty/ws-win/wireshark-4.2.5"
DEPS="C:/Users/mu/Desktop/code/thirty/ws-win"
QT="C:/Users/mu/Desktop/code/thirty/qtbase-ohos"

rm -rf "$SRC/build-gui"

./mingw/mingw64/bin/cmake.exe -S "$SRC" -B "$SRC/build-gui" -G Ninja \
  -DCMAKE_MAKE_PROGRAM="C:/ohos-cc/ninja.exe" \
  -DCMAKE_TOOLCHAIN_FILE="C:/Users/mu/Desktop/code/thirty/ws-win/ws-gui.toolchain.cmake" \
  -DOHOS_ARCH=arm64-v8a \
  -DCMAKE_BUILD_TYPE=Release \
  -DQt6_DIR="$QT/lib/cmake/Qt6" \
  -DQt6LinguistTools_DIR="C:/Users/mu/Desktop/code/thirty/ws-win/fake-linguist/lib/cmake/Qt6LinguistTools" \
  -DQt6Core5Compat_DIR="C:/Users/mu/Desktop/code/thirty/ws-win/qt5compat-install/lib/cmake/Qt6Core5Compat" \
  -DQT_HOST_PATH="C:/Users/mu/Desktop/code/thirty/qtbase-host" \
  -DCMAKE_PREFIX_PATH="$QT;$DEPS/qt5compat-install;$DEPS/fake-linguist;$DEPS/glib-ohos;$DEPS/pcap-ohos;$DEPS/zlib-ohos;$DEPS/gcrypt-ohos;$DEPS/gpgerr-ohos;$DEPS/cares-ohos;$DEPS/speexdsp-ohos" \
  -DPKG_CONFIG_EXECUTABLE="PKG_CONFIG_EXECUTABLE-NOTFOUND" \
  -DFLEX_EXECUTABLE="C:/Users/mu/Desktop/code/thirty/ws-win/winflexbison/win_flex.exe" \
  -DYACC_EXECUTABLE="C:/Users/mu/Desktop/code/thirty/ws-win/winflexbison/win_bison.exe" \
  -DBUILD_wireshark=ON \
  -DBUILD_sharkd=OFF -DBUILD_rawshark=OFF \
  -DBUILD_corbaidl2wireshark=OFF -DBUILD_dcerpidl2wireshark=OFF \
  -DBUILD_tshark=OFF -DBUILD_dumpcap=OFF \
  -DBUILD_editcap=OFF -DBUILD_capinfos=OFF -DBUILD_captype=OFF \
  -DBUILD_dftest=OFF -DBUILD_randpkt=OFF -DBUILD_reordercap=OFF \
  -DBUILD_text2pcap=OFF -DBUILD_mergecap=OFF -DBUILD_writecap=OFF \
  -DBUILD_androiddump=OFF -DBUILD_sshdump=OFF -DBUILD_ciscodump=OFF \
  -DBUILD_dpauxmon=OFF -DBUILD_randpktdump=OFF -DBUILD_sdjournal=OFF \
  -DBUILD_udpdump=OFF -DBUILD_wifidump=OFF -DBUILD_etwdump=OFF \
  -DBUILD_falcodump=OFF -DBUILD_mmsdump=OFF \
  -DBUILD_manpages=OFF -DBUILD_html_docs=OFF -DBUILD_pcapng_common=OFF \
  -DENABLE_GNUTLS=OFF -DENABLE_SMI=OFF -DENABLE_LUA=OFF -DENABLE_LZ4=OFF \
  -DENABLE_NGHTTP2=OFF -DENABLE_NGHTTP3=OFF -DENABLE_BROTLI=OFF \
  -DENABLE_ZSTD=OFF -DENABLE_SNAPPY=OFF -DENABLE_LIBSSH=OFF \
  -DENABLE_LIBXML2=OFF -DENABLE_KERBEROS=OFF -DENABLE_SBC=OFF \
  -DENABLE_BCG729=OFF -DENABLE_OPUS=OFF -DENABLE_SPANDSP=OFF \
  -DENABLE_LIBCAP=OFF -DENABLE_LIBNL=OFF -DENABLE_SYSPROF=OFF \
  -DENABLE_LIBTRACEEVENT=OFF -DENABLE_SPEEXDSP=OFF -DENABLE_ILBC=OFF \
  -DENABLE_PLAY=OFF -DENABLE_SHELL_LIKE=OFF -DENABLE_PLUGINS=OFF \
  -DUSE_qt6=ON \
  -DENABLE_PCAP=ON -DENABLE_ZLIB=ON -DENABLE_GCRYPT=ON -DENABLE_CARES=ON \
  > /tmp/ws-gui-cfg.log 2>&1 || { echo CONFIGURE_FAILED; tail -40 /tmp/ws-gui-cfg.log; exit 1; }

echo CONFIGURE_OK
grep -E "Qt6|Core5Compat|Linguist|w ireshark|GUI" /tmp/ws-gui-cfg.log | head -10

#!/bin/bash
# Cross-compile qtsvg (libQt6Svg.so + the qsvg icon engine plugin) for
# aarch64-linux-ohos so the original Wireshark toolbar icons render.
set -e
cd "C:/Users/mu/Desktop/code/thirty"
QT="C:/Users/mu/Desktop/code/thirty/qtbase-ohos"

rm -rf ws-win/qtsvg-build
./mingw/mingw64/bin/cmake.exe -S ws-win/qtsvg-dev -B ws-win/qtsvg-build -G Ninja \
  -DCMAKE_MAKE_PROGRAM="C:/ohos-cc/ninja.exe" \
  -DCMAKE_TOOLCHAIN_FILE="C:/Users/mu/Desktop/code/thirty/qtbase-oh2/qt_ohos_toolchain_wrapper.cmake" \
  -DOHOS_ARCH=arm64-v8a \
  -DCMAKE_BUILD_TYPE=Release \
  -DQt6_DIR="$QT/lib/cmake/Qt6" \
  -DQT_HOST_PATH="C:/Users/mu/Desktop/code/thirty/qtbase-host" \
  -DCMAKE_PREFIX_PATH="$QT" \
  -DCMAKE_FIND_ROOT_PATH="$QT" \
  -DCMAKE_INSTALL_PREFIX="C:/Users/mu/Desktop/code/thirty/ws-win/qtsvg-install" \
  > /tmp/qtsvg-cfg.log 2>&1 || { echo CONFIGURE_FAILED; tail -20 /tmp/qtsvg-cfg.log; exit 1; }

./mingw/mingw64/bin/cmake.exe --build ws-win/qtsvg-build > /tmp/qtsvg-build.log 2>&1 || {
  echo BUILD_FAILED; grep -m5 -B3 "error" /tmp/qtsvg-build.log | tail -30; exit 1; }
./mingw/mingw64/bin/cmake.exe --install ws-win/qtsvg-build > /tmp/qtsvg-install.log 2>&1

echo QTSVG_OK
find ws-win/qtsvg-install -name "*.so" | head -6

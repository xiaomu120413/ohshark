#!/bin/bash
# Build the real Wireshark GUI (target: wireshark) for aarch64-linux-ohos.
#
# tools/lemon is compiled with the target compiler by Wireshark's build, which
# produces an aarch64 binary the Windows host cannot run; a native lemon.exe
# (mingw) is pre-placed so ninja considers the output up to date, and re-placed
# after a failure in case ninja rebuilt it.
set -e
cd "C:/Users/mu/Desktop/code/thirty"
SRC="C:/Users/mu/Desktop/code/thirty/ws-win/wireshark-4.2.5"
B="$SRC/build-gui"

mkdir -p "$B/run"
place_host_lemon() {
  ./mingw/mingw64/bin/gcc.exe -O2 -w -o "$B/run/lemon.exe" "$SRC/tools/lemon/lemon.c"
  touch "$B/run/lemon.exe"
}

place_host_lemon
rc=0
./mingw/mingw64/bin/cmake.exe --build "$B" --target wireshark > /tmp/ws-gui-build.log 2>&1 || rc=$?

if [ $rc -ne 0 ] && grep -q "lemon" /tmp/ws-gui-build.log; then
  place_host_lemon
  rc=0
  ./mingw/mingw64/bin/cmake.exe --build "$B" --target wireshark > /tmp/ws-gui-build.log 2>&1 || rc=$?
fi

echo "BUILD_RC=$rc"
if [ $rc -ne 0 ]; then
  grep -m6 -B4 -E "error:|FAILED:" /tmp/ws-gui-build.log | tail -60
  exit 1
fi
ls -la "$B/run/" | grep -iE "wireshark|\.so" | head -10

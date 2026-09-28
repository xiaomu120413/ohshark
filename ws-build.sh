#!/bin/bash
# Build tshark + dumpcap for aarch64-linux-ohos.
grep -E '^CMAKE_C_COMPILER:|^CMAKE_SYSTEM_NAME:' /home/mu/wireshark-4.2.5/build-ohos/CMakeCache.txt

cd /home/mu/wireshark-4.2.5
ninja -C build-ohos -j"$(nproc)" tshark dumpcap > /tmp/wsb.log 2>&1
rc=$?
echo "NINJA_RC=$rc"
if [ $rc -ne 0 ]; then
  grep -m8 -B4 -E 'error:|FAILED:' /tmp/wsb.log | tail -60
fi
tail -3 /tmp/wsb.log
ls -la build-ohos/run/tshark build-ohos/run/dumpcap 2>/dev/null
file build-ohos/run/tshark 2>/dev/null | sed 's/^[^:]*: //'

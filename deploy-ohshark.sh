#!/bin/bash
# One-shot deploy of the instrumented ohshark build. Run only when hdc sees the device.
set -e
HDC="/c/Program Files/Huawei/DevEco Studio/sdk/default/openharmony/toolchains/hdc.exe"
H="$HDC -t 3QC0124C11000711"
HAP="/c/Users/mu/Desktop/code/thirty/ohshark-qt-hap/entry-default-signed.hap"

echo "== install =="
MSYS_NO_PATHCONV=1 "$HDC" install -r "$(cygpath -w "$HAP")"
echo "== truncate old debug log =="
MSYS_NO_PATHCONV=1 "$H" shell ": > /data/app/el2/100/base/com.ohshark.qt/files/qt_debug.log" || true
echo "== start =="
MSYS_NO_PATHCONV=1 "$H" shell "aa start -a QAbility -b com.ohshark.qt -m entry"
sleep 12
MSYS_NO_PATHCONV=1 "$H" shell "pidof ohshark; wc -l /data/app/el2/100/base/com.ohshark.qt/files/qt_debug.log 2>/dev/null" || true
echo "== deploy done =="

#!/bin/bash
# Wireshark 4.2.5 builds tools/lemon (the dfilter grammar generator, executed at
# build time) with CMAKE_C_COMPILER. When cross-compiling it becomes an aarch64
# binary the x86-64 host cannot run: "Exec format error".
#
# Rebuild it natively in place and retry. A loop is needed because on a fresh
# build dir ninja compiles lemon.c.o and relinks *after* our replacement, so the
# first attempt overwrites it; once the object exists and is older than the host
# binary, ninja leaves it alone.
set -e

WS=/home/mu/wireshark-4.2.5

rebuild_host_lemon() {
  gcc -O2 -o "$WS/build-ohos/run/lemon" "$WS/tools/lemon/lemon.c"
  touch "$WS/build-ohos/run/lemon"
}

cd "$WS"
rc=1
for attempt in 1 2 3; do
  rebuild_host_lemon
  echo "--- attempt $attempt: lemon is $(file -b build-ohos/run/lemon | cut -d, -f2) ---"
  rc=0
  ninja -C build-ohos -j"$(nproc)" tshark dumpcap > /tmp/wsb.log 2>&1 || rc=$?
  if [ $rc -eq 0 ]; then
    break
  fi
  if ! grep -q "run/lemon: Exec format error" /tmp/wsb.log; then
    break
  fi
done

echo "NINJA_RC=$rc"
if [ $rc -ne 0 ]; then
  grep -m8 -B4 -E 'error:|FAILED:' /tmp/wsb.log | tail -60
fi
tail -3 /tmp/wsb.log
echo "=== artifacts ==="
ls -la build-ohos/run/ | grep -E 'tshark|dumpcap|\.so'
file build-ohos/run/tshark 2>/dev/null | sed 's/^[^:]*: //' || true

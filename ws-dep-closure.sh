#!/bin/bash
# Walk DT_NEEDED transitively (an aarch64 ldd cannot run on the x86-64 host) and
# stage tshark/dumpcap plus every runtime library into /home/mu/ohos-deploy,
# stripped, with soname symlinks. Libraries that live in the OHOS sysroot are
# provided by the device image and are not staged.
READELF=/opt/ohos/sdk-6.1.0.830/command-line-tools/sdk/default/openharmony/native/llvm/bin/llvm-readelf
STRIP=/opt/ohos/sdk-6.1.0.830/command-line-tools/sdk/default/openharmony/native/llvm/bin/llvm-strip
RUN=/home/mu/wireshark-4.2.5/build-ohos/run
DEPLOY=/home/mu/ohos-deploy

SEARCH_DIRS="$RUN
/home/mu/pcap-ohos/lib
/home/mu/zlib-ohos/lib
/home/mu/glib-ohos/lib
/home/mu/gcrypt-ohos/lib
/home/mu/gpgerr-ohos/lib
/home/mu/cares-ohos/lib
/opt/ohos/sdk-6.1.0.830/command-line-tools/sdk/default/openharmony/native/sysroot/usr/lib/aarch64-linux-ohos"

rm -rf $DEPLOY
mkdir -p $DEPLOY/bin $DEPLOY/lib

needed() {
  $READELF -d "$1" 2>/dev/null | sed -n 's/.*NEEDED.*\[\(.*\)\]/\1/p'
}

resolve() {
  local n="$1" d
  for d in $SEARCH_DIRS; do
    [ -e "$d/$n" ] && { readlink -f "$d/$n"; return 0; }
  done
  return 1
}

declare -A seen
declare -A alias
queue="tshark dumpcap"
missing=""

while [ -n "$queue" ]; do
  next=""
  for item in $queue; do
    [ -n "${seen[$item]}" ] && continue
    seen[$item]=1
    path=$(resolve "$item") || { missing="$missing $item"; continue; }
    case "$path" in
      */sysroot/*) continue ;;
    esac
    real=$(basename "$path")
    # Stage under the DT_NEEDED name (the soname) rather than the versioned real
    # file name: symlinks do not survive drvfs, hdc push, or HAP packaging, and
    # the loader only ever asks for the soname.
    cp -L "$path" "$DEPLOY/lib/$item"
    next="$next $(needed "$path" | xargs)"
  done
  queue="$next"
done

cp -L $RUN/tshark $DEPLOY/bin/tshark
cp -L $RUN/dumpcap $DEPLOY/bin/dumpcap
rm -f $DEPLOY/lib/tshark $DEPLOY/lib/dumpcap

echo "=== unresolved (should be empty) ==="
echo "missing:$missing"

echo "=== before strip ==="
du -sh $DEPLOY
find $DEPLOY -type f \( -name '*.so*' -o -name tshark -o -name dumpcap \) -print0 \
  | xargs -0 $STRIP --strip-all
echo "=== after strip ==="
du -sh $DEPLOY
ls -la $DEPLOY/bin $DEPLOY/lib
echo "=== tshark NEEDED ==="
$READELF -d $DEPLOY/bin/tshark | sed -n 's/.*NEEDED.*\[\(.*\)\]/  \1/p'
echo "=== dumpcap NEEDED ==="
$READELF -d $DEPLOY/bin/dumpcap | sed -n 's/.*NEEDED.*\[\(.*\)\]/  \1/p'

# hdc runs on the Windows side, so mirror the tree onto drvfs for pushing.
WIN=/mnt/c/Users/mu/Desktop/code/thirty/ohos-deploy
rm -rf "$WIN"
mkdir -p "$WIN"
cp -r $DEPLOY/bin $DEPLOY/lib "$WIN"/
echo "=== mirrored to Windows ==="
du -sh "$WIN"
ls "$WIN"/lib

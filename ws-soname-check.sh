#!/bin/bash
# A missing DT_SONAME makes the linker record whatever path it was given into the
# consumer's DT_NEEDED, which is unresolvable on device. Report SONAMEs so we know
# which libraries need fixing before packaging.
READELF=/opt/ohos/sdk-6.1.0.830/command-line-tools/sdk/default/openharmony/native/llvm/bin/llvm-readelf

for f in \
  /home/mu/wireshark-4.2.5/build-ohos/run/libwireshark.so \
  /home/mu/wireshark-4.2.5/build-ohos/run/libwiretap.so \
  /home/mu/wireshark-4.2.5/build-ohos/run/libwsutil.so \
  /home/mu/pcap-ohos/lib/libpcap.so \
  /home/mu/zlib-ohos/lib/libz.so \
  /home/mu/glib-ohos/lib/libglib-2.0.so.0.7800.4 \
  /home/mu/glib-ohos/lib/libintl.so.8 \
  /home/mu/glib-ohos/lib/libffi.so.7.1.0
do
  printf '%-30s ' "$(basename "$f")"
  soname=$($READELF -d "$f" 2>/dev/null | grep SONAME | sed 's/.*\[\(.*\)\]/\1/')
  if [ -n "$soname" ]; then echo "SONAME=$soname"; else echo "NO SONAME"; fi
done
echo "=== which patchelf ==="
which patchelf || echo "patchelf not installed"

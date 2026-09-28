#!/bin/bash
# Report what tshark/dumpcap need at runtime so the HAP can ship every .so.
READELF=/opt/ohos/sdk-6.1.0.830/command-line-tools/sdk/default/openharmony/native/llvm/bin/llvm-readelf
RUN=/home/mu/wireshark-4.2.5/build-ohos/run

echo "=== tshark NEEDED ==="
$READELF -d $RUN/tshark | grep -E 'NEEDED|RUNPATH|RPATH'
echo "=== dumpcap NEEDED ==="
$READELF -d $RUN/dumpcap | grep -E 'NEEDED|RUNPATH|RPATH'
echo "=== build-ohos/run contents ==="
ls -la $RUN | head -40
echo "=== wireshark shared libs ==="
for f in $RUN/*.so*; do
  printf '%-46s ' "$(basename "$f")"
  file -L "$f" | sed 's/^[^:]*: //' | cut -c1-40
done

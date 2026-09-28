#!/bin/bash
# Pull the larger on-device capture and regenerate the embedded sample header.
# Prefers big2.pcap (mixed DNS/TCP/TLS/ICMP/ARP traffic); falls back to big.pcap.
set -e
cd "$(dirname "$0")"

if MSYS_NO_PATHCONV=1 hdc file recv /data/local/tmp/big2.pcap ./ohshark-qt/sample.pcap 2>&1 | grep -q "FileTransfer finish"; then
  echo "pulled big2.pcap"
elif MSYS_NO_PATHCONV=1 hdc file recv /data/local/tmp/big.pcap ./ohshark-qt/sample.pcap 2>&1 | grep -q "FileTransfer finish"; then
  echo "pulled big.pcap"
else
  echo "PULL FAILED"; exit 1
fi

ls -la ohshark-qt/sample.pcap

MSYS_NO_PATHCONV=1 wsl.exe -d Ubuntu-24.04 -- bash -lc 'python3 - <<PY
data = open("/mnt/c/Users/mu/Desktop/code/thirty/ohshark-qt/sample.pcap","rb").read()
out = []
out.append("// Real traffic captured on the HarmonyOS device itself by the cross-built")
out.append("// upstream dumpcap, embedded so the app can hand it to the ported tshark for")
out.append("// dissection inside its own sandbox.")
out.append("#pragma once")
out.append("")
out.append("static const unsigned char kSamplePcap[] = {")
for i in range(0, len(data), 16):
    chunk = data[i:i+16]
    out.append("    " + ", ".join("0x%02x" % b for b in chunk) + ",")
out.append("};")
out.append("static const unsigned int kSamplePcapLen = %du;" % len(data))
open("/mnt/c/Users/mu/Desktop/code/thirty/ohshark-qt/sample_pcap.h","w",newline="\n").write("\n".join(out) + "\n")
print("wrote header with", len(data), "bytes")
PY'

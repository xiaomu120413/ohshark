#!/bin/bash
# Report the ELF/arch of every cross-built dependency so host (x86-64) artifacts
# masquerading as target ones are caught before Wireshark tries to link them.
for f in \
  /home/mu/pcap-ohos/lib/libpcap.so \
  /home/mu/pcap-ohos/lib/libpcap.a \
  /home/mu/zlib-ohos/lib/libz.so \
  /home/mu/zlib-ohos/lib/libz.a \
  /home/mu/cares-ohos/lib/libcares.a \
  /home/mu/gcrypt-ohos/lib/libgcrypt.a \
  /home/mu/gpgerr-ohos/lib/libgpg-error.a \
  /home/mu/glib-ohos/lib/libglib-2.0.so.0.7800.4 \
  /home/mu/glib-ohos/lib/libpcre2-8.a
do
  printf '%-52s ' "$f"
  if [ -e "$f" ]; then
    file -L "$f" | sed 's/^[^:]*: //' | cut -c1-60
  else
    echo "MISSING"
  fi
done

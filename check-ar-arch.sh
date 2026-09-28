#!/bin/bash
# Report the object arch inside each static archive: extract the first member and
# run `file` on it, since `file` on the .a only says "current ar archive".
for f in \
  /home/mu/pcap-ohos/lib/libpcap.a \
  /home/mu/zlib-ohos/lib/libz.a \
  /home/mu/cares-ohos/lib/libcares.a \
  /home/mu/gcrypt-ohos/lib/libgcrypt.a \
  /home/mu/gpgerr-ohos/lib/libgpg-error.a \
  /home/mu/glib-ohos/lib/libpcre2-8.a \
  /home/mu/glib-ohos/lib/libintl.a \
  /home/mu/glib-ohos/lib/libffi.a
do
  printf '%-52s ' "$f"
  if [ ! -e "$f" ]; then
    echo "MISSING"
    continue
  fi
  d=$(mktemp -d)
  ( cd "$d" && ar x "$f" >/dev/null 2>&1 )
  m=$(ls "$d"/*.o 2>/dev/null | head -1)
  if [ -n "$m" ]; then
    file "$m" | sed 's/^[^:]*: //' | cut -c1-50
  else
    echo "no members extracted"
  fi
  rm -rf "$d"
done

#!/bin/bash
# Full chain: rebuild deps (now with SONAMEs), cross-configure Wireshark, build a
# native lemon, then build tshark + dumpcap for aarch64-linux-ohos.
S=/mnt/c/Users/mu/Desktop/code/thirty

echo "########## 1/4 deps ##########"
bash $S/build-deps-ohos.sh 2>&1 | tail -8
echo "########## 2/4 sonames ##########"
bash $S/ws-soname-check.sh 2>&1 | head -8
echo "########## 3/4 wireshark configure ##########"
bash $S/ws-cross-configure.sh 2>&1 | grep -vE 'System is unknown|discourse.cmake.org' | tail -6
echo "########## 4/4 tshark build ##########"
bash $S/ws-fix-lemon-build.sh 2>&1 | tail -25

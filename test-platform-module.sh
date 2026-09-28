#!/bin/bash
# Minimal probe: does CMake pick up our Platform/OHOS.cmake, and does the drvfs
# (/mnt/c) location matter? Prints the soname flag the platform module should set.

mkdir -p /tmp/tmod /home/mu/cmake-modules/Platform
cp /mnt/c/Users/mu/Desktop/code/thirty/cmake-modules/Platform/OHOS.cmake /home/mu/cmake-modules/Platform/OHOS.cmake

cat > /tmp/tmod/CMakeLists.txt <<'EOF'
cmake_minimum_required(VERSION 3.16)
project(t C)
message(STATUS "MODULE_PATH=[${CMAKE_MODULE_PATH}]")
message(STATUS "SONAME_FLAG=[${CMAKE_SHARED_LIBRARY_SONAME_C_FLAG}]")
message(STATUS "SHLIB_NAME=[${CMAKE_SHARED_LIBRARY_NAME_C}]")
add_library(foo SHARED foo.c)
set_target_properties(foo PROPERTIES VERSION 1.2.3 SOVERSION 1)
EOF
echo 'int foo(void){return 1;}' > /tmp/tmod/foo.c

TOOLCHAIN=/mnt/c/Users/mu/Desktop/code/thirty/ws-ohos.toolchain.cmake

echo "########## A: module path on /mnt/c ##########"
rm -rf /tmp/tmod/bA
cmake -S /tmp/tmod -B /tmp/tmod/bA -DCMAKE_TOOLCHAIN_FILE=$TOOLCHAIN -DOHOS_ARCH=arm64-v8a \
  -DCMAKE_MODULE_PATH=/mnt/c/Users/mu/Desktop/code/thirty/cmake-modules 2>&1 \
  | grep -iE "system is unknown|MODULE_PATH=|SONAME_FLAG=|SHLIB_NAME=" | head -6

echo "########## B: module path in WSL home ##########"
rm -rf /tmp/tmod/bB
cmake -S /tmp/tmod -B /tmp/tmod/bB -DCMAKE_TOOLCHAIN_FILE=$TOOLCHAIN -DOHOS_ARCH=arm64-v8a \
  -DCMAKE_MODULE_PATH=/home/mu/cmake-modules 2>&1 \
  | grep -iE "system is unknown|MODULE_PATH=|SONAME_FLAG=|SHLIB_NAME=" | head -6

echo "########## resulting soname in B ##########"
cmake --build /tmp/tmod/bB >/dev/null 2>&1
ls /tmp/tmod/bB/ | grep libfoo

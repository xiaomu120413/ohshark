#!/bin/bash
# Build libohsharkcore.so: upstream tshark's main() compiled as a shared library,
# plus a test harness that calls it three times in one process.
set -e

WS=/home/mu/wireshark-4.2.5
SRC=/mnt/c/Users/mu/Desktop/code/thirty/ws-ohos-core
READELF=/opt/ohos/sdk-6.1.0.830/command-line-tools/sdk/default/openharmony/native/llvm/bin/llvm-readelf

mkdir -p "$WS/ohshark"
cp "$SRC/ohshark_tshark.c" "$SRC/ohshark_core_test.c" "$WS/ohshark/"

MARKER="OhShark: in-process tshark"
if ! grep -q "$MARKER" "$WS/CMakeLists.txt"; then
  cat >> "$WS/CMakeLists.txt" <<'EOF'

# --- OhShark: in-process tshark for HarmonyOS (appended by ws-build-core.sh) --
# HarmonyOS blocks execve() of app payloads, so tshark's main() is also built as
# a library that the Qt app dlopen()s from the signed bundle.
if(OHSHARK_CORE)
	add_library(ohsharkcore SHARED
		$<TARGET_OBJECTS:capture_opts>
		$<TARGET_OBJECTS:shark_common>
		ohshark/ohshark_tshark.c
		tshark-tap-register.c
		extcap.c
		${TSHARK_TAP_SRC}
	)
	target_link_libraries(ohsharkcore ${tshark_LIBS})
	set_target_properties(ohsharkcore PROPERTIES
		VERSION 1.0.0 SOVERSION 1
		LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/run
	)
	if(OHSHARK_CORE_TEST)
		add_executable(ohsharkcore_test ohshark/ohshark_core_test.c)
		target_link_libraries(ohsharkcore_test ohsharkcore)
		set_target_properties(ohsharkcore_test PROPERTIES
			RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/run)
	endif()
endif()
EOF
  echo "CMakeLists patched"
fi

cd "$WS"
cmake -S . -B build-ohos -DOHSHARK_CORE=ON -DOHSHARK_CORE_TEST=ON > /tmp/core-cfg.log 2>&1 \
  || { echo CORE_CONFIGURE_FAILED; tail -25 /tmp/core-cfg.log; exit 1; }

# The build dir is reused, so ninja may want to relink the cross-built lemon;
# keep a native one in place (see ws-fix-lemon-build.sh).
gcc -O2 -o build-ohos/run/lemon tools/lemon/lemon.c
touch build-ohos/run/lemon

rc=0
ninja -C build-ohos -j"$(nproc)" ohsharkcore ohsharkcore_test > /tmp/core-build.log 2>&1 || rc=$?
echo "NINJA_RC=$rc"
if [ $rc -ne 0 ]; then
  grep -m8 -B4 -E 'error:|FAILED:' /tmp/core-build.log | tail -50
  exit 1
fi

ls -la build-ohos/run/libohsharkcore.so* build-ohos/run/ohsharkcore_test
file build-ohos/run/libohsharkcore.so.1.0.0 | sed 's/^[^:]*: //' | cut -c1-60
echo "=== ohsharkcore NEEDED ==="
$READELF -d build-ohos/run/libohsharkcore.so.1.0.0 | sed -n 's/.*NEEDED.*\[\(.*\)\]/  \1/p'
echo "=== exported entry points ==="
/opt/ohos/sdk-6.1.0.830/command-line-tools/sdk/default/openharmony/native/llvm/bin/llvm-nm -D --defined-only build-ohos/run/libohsharkcore.so.1.0.0 | grep -i ohshark

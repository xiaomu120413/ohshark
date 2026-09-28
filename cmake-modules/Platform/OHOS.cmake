# CMake platform module for HarmonyOS / OpenHarmony (CMAKE_SYSTEM_NAME=OHOS).
#
# Without this, CMake prints "System is unknown to cmake, create Platform/OHOS"
# and leaves CMAKE_SHARED_LIBRARY_SONAME_<LANG>_FLAG undefined. The consequence is
# silent: VERSION/SOVERSION target properties are ignored, shared libraries get no
# DT_SONAME, and the linker then records the *path it was handed* into consumers'
# DT_NEEDED (e.g. "run/libwireshark.so", "/home/mu/pcap-ohos/lib/libpcap.so"),
# which cannot be resolved on the device.
#
# OHOS is Linux-like: ELF, musl libc, GNU-style shared library naming and the
# same -Wl,-soname flag, so reuse the Linux module.
include(Platform/Linux)

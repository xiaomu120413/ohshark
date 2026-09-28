# Wrapper around the OHOS NDK toolchain file.
#
# The stock ohos.toolchain.cmake hard-sets CMAKE_FIND_ROOT_PATH_MODE_{LIBRARY,
# INCLUDE,PACKAGE} to ONLY with plain set() calls, which overrides the -D cache
# entries and confines every find_library/find_path to the NDK sysroot. Our
# cross-built dependencies (glib, libpcap, zlib, libgcrypt, libgpg-error,
# c-ares) live outside the sysroot, so relax the modes to BOTH and add their
# prefixes to the root path.

include("/opt/ohos/sdk-6.1.0.830/command-line-tools/sdk/default/openharmony/native/build/cmake/ohos.toolchain.cmake")

# Provide Platform/OHOS so CMake knows -Wl,-soname and versioned shared library
# naming. It is appended here rather than passed as -DCMAKE_MODULE_PATH because
# projects such as libpcap do `set(CMAKE_MODULE_PATH ...)` before project(),
# which discards the cache value; the toolchain file is read during project(),
# i.e. after that clobbering.
list(APPEND CMAKE_MODULE_PATH "/mnt/c/Users/mu/Desktop/code/thirty/cmake-modules")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY BOTH)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE BOTH)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE BOTH)

# CMake has no Platform/OHOS module, so CMAKE_SYSTEM_{INCLUDE,LIBRARY}_PATH stay
# empty and plain probes such as FindM's `find_path(math.h)` find nothing. Point
# them at the OHOS sysroot for the target arch.
set(OHOS_SYSROOT "/opt/ohos/sdk-6.1.0.830/command-line-tools/sdk/default/openharmony/native/sysroot")
set(CMAKE_SYSTEM_INCLUDE_PATH "${OHOS_SYSROOT}/usr/include")
set(CMAKE_SYSTEM_LIBRARY_PATH "${OHOS_SYSROOT}/usr/lib/aarch64-linux-ohos")

list(APPEND CMAKE_FIND_ROOT_PATH
    /home/mu/glib-ohos
    /home/mu/pcap-ohos
    /home/mu/zlib-ohos
    /home/mu/gcrypt-ohos
    /home/mu/gpgerr-ohos
    /home/mu/cares-ohos
)

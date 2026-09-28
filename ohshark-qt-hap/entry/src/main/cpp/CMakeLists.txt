# Builds libohsharkapi.so: the NAPI bridge between the ArkTS UI and the
# cross-compiled upstream Wireshark engine (libohsharkcore.so, which the app
# dlopen()s at runtime — no link-time dependency needed here).
cmake_minimum_required(VERSION 3.5.0)
project(ohsharkapi)

add_library(ohsharkapi SHARED ohshark_api.cpp)

target_include_directories(ohsharkapi PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(ohsharkapi PUBLIC libace_napi.z.so)

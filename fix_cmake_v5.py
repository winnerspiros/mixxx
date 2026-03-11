import re

def fix():
    with open('CMakeLists.txt', 'r') as f:
        content = f.read()

    # 1. Make the Android architecture forcing block more robust
    # It currently looks like:
    # if(ANDROID)
    #   if(VCPKG_TARGET_TRIPLET MATCHES "arm64")
    #     set(ANDROID_ABI "arm64-v8a" CACHE STRING "" FORCE)
    #     set(CMAKE_ANDROID_ARCH_ABI "arm64-v8a" CACHE STRING "" FORCE)
    #     set(CMAKE_SYSTEM_PROCESSOR "aarch64" CACHE STRING "" FORCE)
    #   ...
    # endif()

    content = content.replace('if(ANDROID)', 'if(ANDROID OR (CMAKE_SYSTEM_NAME STREQUAL "Android"))')

    # 2. Ensure CMAKE_TOOLCHAIN_FILE is set even if it was defined as empty
    # Change if(NOT DEFINED CMAKE_TOOLCHAIN_FILE) to if(NOT CMAKE_TOOLCHAIN_FILE)
    content = content.replace('if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)', 'if(NOT CMAKE_TOOLCHAIN_FILE)')

    # 3. Add more debug messages to trace what's happening early on
    content = content.replace('project(mixxx VERSION 2.7.0 LANGUAGES C CXX)',
                              'message(STATUS "Early CMAKE_SYSTEM_NAME: ${CMAKE_SYSTEM_NAME}")\n'
                              'message(STATUS "Early CMAKE_SYSTEM_PROCESSOR: ${CMAKE_SYSTEM_PROCESSOR}")\n'
                              'project(mixxx VERSION 2.7.0 LANGUAGES C CXX)')

    with open('CMakeLists.txt', 'w') as f:
        f.write(content)

if __name__ == "__main__":
    fix()

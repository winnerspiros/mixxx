import sys

with open('CMakeLists.txt', 'r') as f:
    lines = f.readlines()

new_lines = []
skip_until = -1

for i, line in enumerate(lines):
    if i < skip_until:
        continue

    # Fix download condition
    if '((APPLE AND NOT IOS) OR WIN32 OR ANDROID)' in line:
        line = line.replace('((APPLE AND NOT IOS) OR WIN32 OR ANDROID)',
                            '((APPLE AND NOT IOS) OR WIN32 OR ANDROID OR (CMAKE_SYSTEM_NAME STREQUAL "Android"))')

    # After download block, derive MIXXX_VCPKG_ROOT
    if 'set(_dummy "${BUILDENV_URL}" "${BUILDENV_BASEPATH}" "${BUILDENV_SHA256}")' in line:
        # Check if next line is endif()
        if i + 1 < len(lines) and 'endif()' in lines[i+1]:
            new_lines.append(line)
            new_lines.append(lines[i+1])
            new_lines.append('\n')
            new_lines.append('if(NOT DEFINED MIXXX_VCPKG_ROOT AND NOT DEFINED ENV{MIXXX_VCPKG_ROOT})\n')
            new_lines.append('  if(DEFINED BUILDENV_BASEPATH AND DEFINED BUILDENV_NAME)\n')
            new_lines.append('    if(IS_DIRECTORY "${BUILDENV_BASEPATH}/${BUILDENV_NAME}")\n')
            new_lines.append('      set(MIXXX_VCPKG_ROOT "${BUILDENV_BASEPATH}/${BUILDENV_NAME}")\n')
            new_lines.append('    endif()\n')
            new_lines.append('  endif()\n')
            new_lines.append('endif()\n')
            skip_until = i + 2
            continue

    # Detect redundant VCPKG block and skip it
    if 'if(DEFINED MIXXX_VCPKG_ROOT)' in line and i > 210: # heuristic for the second block
        # Check if it's the redundant one by looking ahead
        if i + 1 < len(lines) and 'if(NOT DEFINED VCPKG_OVERLAY_TRIPLETS)' in lines[i+1]:
             # Read ahead to find the end of this block
             j = i
             # First inner if ends
             while j < len(lines) and 'endif()' not in lines[j]:
                 j += 1
             j += 1
             # Second inner if starts
             while j < len(lines) and 'if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)' not in lines[j]:
                 j += 1
             # Second inner if ends
             while j < len(lines) and 'endif()' not in lines[j]:
                 j += 1
             j += 1
             # Outer if ends
             while j < len(lines) and 'endif()' not in lines[j]:
                 j += 1
             skip_until = j + 1
             continue

    # Force processor after project()
    if 'project(mixxx VERSION 2.7.0 LANGUAGES C CXX)' in line:
        new_lines.append(line)
        new_lines.append('if(ANDROID)\n')
        new_lines.append('  if(VCPKG_TARGET_TRIPLET MATCHES "arm64")\n')
        new_lines.append('    set(ANDROID_ABI "arm64-v8a" CACHE STRING "" FORCE)\n')
        new_lines.append('    set(CMAKE_ANDROID_ARCH_ABI "arm64-v8a" CACHE STRING "" FORCE)\n')
        new_lines.append('    set(CMAKE_SYSTEM_PROCESSOR "aarch64" CACHE STRING "" FORCE)\n')
        new_lines.append('  elseif(VCPKG_TARGET_TRIPLET MATCHES "x64")\n')
        new_lines.append('    set(ANDROID_ABI "x86_64" CACHE STRING "" FORCE)\n')
        new_lines.append('    set(CMAKE_ANDROID_ARCH_ABI "x86_64" CACHE STRING "" FORCE)\n')
        new_lines.append('  endif()\n')
        new_lines.append('endif()\n')
        continue

    new_lines.append(line)

with open('CMakeLists.txt', 'w') as f:
    f.writelines(new_lines)

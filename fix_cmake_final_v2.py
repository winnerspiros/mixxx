import re
import os

with open("CMakeLists.txt", "r") as f:
    content = f.read()

# Fix whitespace warnings in buildenv logic
content = re.sub(r'"Downloading file "\${BUILDENV_URL}" to "\${BUILDENV_BASEPATH}" \.\.\."',
                 r'"Downloading file " "${BUILDENV_URL}" " to " "${BUILDENV_BASEPATH}" " ..."', content)
content = re.sub(r'"Verify SHA256 of downloaded file "\${BUILDENV_BASEPATH}/\${BUILDENV_NAME}"\.zip \.\.\."',
                 r'"Verify SHA256 of downloaded file " "${BUILDENV_BASEPATH}/${BUILDENV_NAME}.zip" " ..."', content)
content = re.sub(r'"SHA256 "\${BUILDENV_SHA256}" is correct!"',
                 r'"SHA256 " "${BUILDENV_SHA256}" " is correct!"', content)
content = re.sub(r'"Unpacking file "\${BUILDENV_BASEPATH}/\${BUILDENV_NAME}"\.zip \.\.\."',
                 r'"Unpacking file " "${BUILDENV_BASEPATH}/${BUILDENV_NAME}.zip" " ..."', content)
content = re.sub(r'expected: "\${BUILDENV_SHA256}"',
                 r'expected: "${BUILDENV_SHA256}"', content)
# Fix the tar command working directory if it was broken
content = content.replace('COMMAND ${CMAKE_COMMAND} -E tar xzf "${BUILDENV_NAME}.zip"', 'COMMAND ${CMAKE_COMMAND} -E tar xzf "${BUILDENV_NAME}.zip"')

# Improve VCPKG path detection
vcpkg_logic = """
if(DEFINED MIXXX_VCPKG_ROOT AND DEFINED VCPKG_TARGET_TRIPLET)
  set(TRIPLET_VARIANTS "${VCPKG_TARGET_TRIPLET}")
  string(REPLACE "-release" "" TRIPLET_BASE "${VCPKG_TARGET_TRIPLET}")
  string(REPLACE "-debug" "" TRIPLET_BASE "${TRIPLET_BASE}")
  list(APPEND TRIPLET_VARIANTS "${TRIPLET_BASE}")
  list(REMOVE_DUPLICATES TRIPLET_VARIANTS)

  set(VCPKG_INSTALLED_PATH_FOUND FALSE)
  foreach(variant ${TRIPLET_VARIANTS})
    set(VCPKG_INSTALLED_PATH_TMP "${MIXXX_VCPKG_ROOT}/installed/${variant}")
    file(TO_CMAKE_PATH "${VCPKG_INSTALLED_PATH_TMP}" VCPKG_INSTALLED_PATH_TMP)
    if(IS_DIRECTORY "${VCPKG_INSTALLED_PATH_TMP}")
      set(VCPKG_INSTALLED_PATH "${VCPKG_INSTALLED_PATH_TMP}")
      set(VCPKG_INSTALLED_PATH_FOUND TRUE)
      message(STATUS "Found vcpkg installed path: ${VCPKG_INSTALLED_PATH}")
      break()
    endif()
  endforeach()

  if(VCPKG_INSTALLED_PATH_FOUND)
    set(CMAKE_PREFIX_PATH "${VCPKG_INSTALLED_PATH}" "${VCPKG_INSTALLED_PATH}/share" ${CMAKE_PREFIX_PATH})
    set(CMAKE_PREFIX_PATH "${CMAKE_PREFIX_PATH}" CACHE STRING "Prefix paths for package discovery" FORCE)
  else()
    message(WARNING "VCPKG_INSTALLED_PATH not found for variants: ${TRIPLET_VARIANTS}")
  endif()
endif()
"""

# Replace old vcpkg prefix logic (approx lines 111-122)
content = re.sub(r'if\(DEFINED MIXXX_VCPKG_ROOT AND DEFINED VCPKG_TARGET_TRIPLET\).*?endif\(\s*\)\s*endif\(\s*\)',
                 vcpkg_logic, content, flags=re.DOTALL)

# Fix librespot-cpp / spdlog
# We want to insert spdlog fetch before librespot-cpp
spdlog_fetch = """
  FetchContent_Declare(
    spdlog
    URL https://github.com/gabime/spdlog/archive/refs/tags/v1.12.0.zip
  )
  FetchContent_MakeAvailable(spdlog)
"""
# If spdlog is already there, we just make sure it's MakeAvailable'd
# Looking at line 3619 in trace, it's already there.
# But maybe it's not being found by librespot-cpp.

# Let's ensure target_link_libraries(mixxx ...) uses PRIVATE consistently
content = content.replace('target_link_libraries(mixxx mixxx-lib mixxx-gitinfostore)', 'target_link_libraries(mixxx PRIVATE mixxx-lib mixxx-gitinfostore)')

with open("CMakeLists.txt", "w") as f:
    f.write(content)

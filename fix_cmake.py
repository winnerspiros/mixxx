import sys
import re

with open('CMakeLists.txt', 'r') as f:
    content = f.read()

# 1. Update QT_COMPONENTS order
old_qt = """    QT_COMPONENTS
    Concurrent
    Core"""
new_qt = """    QT_COMPONENTS
    Core
    Concurrent"""
content = content.replace(old_qt, new_qt)

# 2. Fix case sensitivity in QT6_SEARCH_PATH
content = content.replace('share/qt6"', 'share/Qt6"')

# 3. Add VCPKG_INSTALLED_PATH to CMAKE_FIND_ROOT_PATH for Android
# Find where CMAKE_PREFIX_PATH is set for VCPKG
find_root_block = r"""    set(
      CMAKE_PREFIX_PATH
      "${VCPKG_INSTALLED_PATH}"
      "${VCPKG_INSTALLED_PATH}/share"
      ${CMAKE_PREFIX_PATH}
    )"""

replacement_block = r"""    set(
      CMAKE_PREFIX_PATH
      "${VCPKG_INSTALLED_PATH}"
      "${VCPKG_INSTALLED_PATH}/share"
      ${CMAKE_PREFIX_PATH}
    )
    if(ANDROID OR (CMAKE_SYSTEM_NAME STREQUAL "Android"))
      list(APPEND CMAKE_FIND_ROOT_PATH "${VCPKG_INSTALLED_PATH}" "${VCPKG_INSTALLED_PATH}/share")
      list(REMOVE_DUPLICATES CMAKE_FIND_ROOT_PATH)
    endif()"""

content = content.replace(find_root_block, replacement_block)

with open('CMakeLists.txt', 'w') as f:
    f.write(content)

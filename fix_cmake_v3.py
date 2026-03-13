import re

with open('CMakeLists.txt', 'r') as f:
    content = f.read()

# Fix cmakelint issues on line 92 (approx)
# Old: if( ((APPLE AND NOT IOS) OR WIN32 OR ANDROID OR (CMAKE_SYSTEM_NAME STREQUAL "Android"))
# New: if(((APPLE AND NOT IOS) OR WIN32 OR ANDROID OR (CMAKE_SYSTEM_NAME STREQUAL "Android")))

content = re.sub(
    r'if\(\s*\(\(APPLE AND NOT IOS\) OR WIN32 OR ANDROID OR \(CMAKE_SYSTEM_NAME STREQUAL "Android"\)\)',
    r'if(((APPLE AND NOT IOS) OR WIN32 OR ANDROID OR (CMAKE_SYSTEM_NAME STREQUAL "Android")))',
    content
)

# Fix qt6_android_apply_arch_suffix issues
# It should be provided by Qt6Core. Let's make sure find_package(Qt6Core) or similar happened.
# Actually, it might be that find_package(Qt6 ...) didn't load the Android specific macros.
# Let's try to add find_package(Qt6 COMPONENTS Core REQUIRED) before using it if on Android.

old_android_block = r"""  if(ANDROID OR (CMAKE_SYSTEM_NAME STREQUAL "Android"))
    target_compile_definitions(
      mixxx-lib
      PUBLIC __STDC_CONSTANT_MACROS __STDC_LIMIT_MACROS __STDC_FORMAT_MACROS
    )"""

new_android_block = r"""  if(ANDROID OR (CMAKE_SYSTEM_NAME STREQUAL "Android"))
    find_package(Qt6 COMPONENTS Core REQUIRED)
    target_compile_definitions(
      mixxx-lib
      PUBLIC __STDC_CONSTANT_MACROS __STDC_LIMIT_MACROS __STDC_FORMAT_MACROS
    )"""

if new_android_block not in content:
    content = content.replace(old_android_block, new_android_block)

with open('CMakeLists.txt', 'w') as f:
    f.write(content)

import re
import os

def replace_in_file(filepath, pattern, replacement, flags=0):
    if not os.path.exists(filepath):
        print(f"File not found: {filepath}")
        return
    with open(filepath, 'r') as f:
        content = f.read()
    new_content = re.sub(pattern, replacement, content, flags=flags)
    with open(filepath, 'w') as f:
        f.write(new_content)

with open('CMakeLists.txt', 'r') as f:
    content = f.read()

# 1. Move project() and MIXXX_VERSION calculation at top
content = re.sub(r'project\(mixxx VERSION 2\.7\.0 LANGUAGES C CXX\)\s*', '', content)
content = re.sub(r'set\(MIXXX_VERSION_PRERELEASE "alpha"\) # set to "alpha" "beta" or ""\s+if\(MIXXX_VERSION_PRERELEASE STREQUAL ""\)\s+set\(MIXXX_VERSION "\${CMAKE_PROJECT_VERSION}"\)\s+else\(\)\s+set\(MIXXX_VERSION "\${CMAKE_PROJECT_VERSION}-\${MIXXX_VERSION_PRERELEASE}"\)\s+endif\(\)\s*', '', content, flags=re.DOTALL)

content = re.sub(r'(cmake_minimum_required\(VERSION 3\.21\))',
    r'''\1

project(mixxx VERSION 2.7.0 LANGUAGES C CXX)

set(MIXXX_VERSION_PRERELEASE "alpha") # set to "alpha" "beta" or ""
if(MIXXX_VERSION_PRERELEASE STREQUAL "")
  set(MIXXX_VERSION "${CMAKE_PROJECT_VERSION}")
else()
  set(MIXXX_VERSION "${CMAKE_PROJECT_VERSION}-${MIXXX_VERSION_PRERELEASE}")
endif()''', content)

# 2. SOURCE_TARGET for Android
content = re.sub(r'(if\(CMAKE_SYSTEM_NAME STREQUAL Android\)\s+if\(NOT DEFINED ENV\{JAVA_HOME\}\))',
    r'if(CMAKE_SYSTEM_NAME STREQUAL Android)\n  set(SOURCE_TARGET mixxx)\n  if(NOT DEFINED ENV{JAVA_HOME})', content, flags=re.DOTALL)

# 3. Robust BUILDENV_SHA256 check
content = content.replace('if(NOT ${BUILDENV_SHA256} STREQUAL "")', 'if(NOT "${BUILDENV_SHA256}" STREQUAL "")')

# 4. Android build configuration (OpenMP and toolchain)
# Broad pattern to catch the manual toolchain overrides
android_toolchain_pattern = r'set\(\s*CMAKE_LINKER.*?\)\s*set\(\s*CMAKE_C_COMPILER.*?\)\s*set\(\s*CMAKE_CXX_COMPILER.*?\)\s*set\(CMAKE_CXX_FLAGS.*?\)\s*set_target_properties\(\s*mixxx\s*PROPERTIES\s*QT_ANDROID_EXTRA_LIBS.*?\)'

android_new_config = r'''    find_package(OpenMP REQUIRED COMPONENTS CXX)
    target_link_libraries(mixxx-lib PUBLIC OpenMP::OpenMP_CXX)
    file(GLOB LIBOMP_PATH "${CMAKE_ANDROID_NDK}/toolchains/llvm/prebuilt/${ANDROID_NDK_HOST_SYSTEM_NAME}/lib/clang/*/lib/linux/aarch64/libomp.so")
    set_target_properties(
      mixxx
      PROPERTIES
        QT_ANDROID_EXTRA_LIBS "${LIBOMP_PATH}"
    )'''

content = re.sub(android_toolchain_pattern, android_new_config, content, flags=re.DOTALL)
content = content.replace('target_link_libraries(mixxx PUBLIC omp)', '')

# 5. Missing include directories for mixxx-lib
content = re.sub(r'target_include_directories\(\s+mixxx-lib\s+PUBLIC\s+src\s+"\$\{CMAKE_CURRENT_BINARY_DIR\}/src"\s+\)',
    r'''target_include_directories(
  mixxx-lib
  PUBLIC
    src
    "${CMAKE_CURRENT_BINARY_DIR}/src"
    lib/replaygain
    lib/qm-dsp
    lib/reverb
    src/rendergraph/common
    src/rendergraph/opengl
    src/rendergraph/scenegraph
)''', content)

# 6. Qt Linking for mixxx-lib
content = re.sub(r'target_link_libraries\(mixxx-lib PUBLIC mixxx-proto\)',
    r'''target_link_libraries(
  mixxx-lib
  PUBLIC
    mixxx-proto
    Qt${QT_VERSION_MAJOR}::Concurrent
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Network
    Qt${QT_VERSION_MAJOR}::OpenGL
    Qt${QT_VERSION_MAJOR}::Sql
    Qt${QT_VERSION_MAJOR}::Svg
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Xml
    Qt${QT_VERSION_MAJOR}::Qml
)''', content)

# 7. librespot-cpp URL download
content = re.sub(r'FetchContent_Declare\(\s+librespot-cpp\s+GIT_REPOSITORY https://github\.com/librespot-org/librespot-cpp\.git\s+GIT_TAG master\s+\)',
    r'''FetchContent_Declare(
    librespot-cpp
    URL https://github.com/librespot-org/librespot-cpp/archive/refs/heads/master.zip
  )''', content, flags=re.DOTALL)

with open('CMakeLists.txt', 'w') as f:
    f.write(content)

# --- Other files ---

replace_in_file('tools/android_buildenv.sh',
    r'google-android-cmdline-tools-13\.0-installer',
    'google-android-cmdline-tools-11.0-installer')

replace_in_file('packaging/android/AndroidManifest.xml',
    r'<application',
    r'<application android:extractNativeLibs="true"')

replace_in_file('res/linux/org.mixxx.Mixxx.metainfo.xml',
    r'https://mixxx.org/donate/',
    r'https://mixxx.org/donate')

import re

with open('CMakeLists.txt', 'r') as f:
    content = f.read()

# 1. Conditionally include wglwidgetqglwidget.cpp
content = content.replace(
    '  src/widget/wglwidgetqglwidget.cpp',
    '  $<$<NOT:$<BOOL:${QT6}>>:src/widget/wglwidgetqglwidget.cpp>'
)

# 2. Patch librespot-cpp logic
old_librespot = """  FetchContent_MakeAvailable(spdlog)
  FetchContent_Populate(librespot-cpp)
  if(NOT EXISTS "${librespot-cpp_SOURCE_DIR}/lib/spdlog/CMakeLists.txt")
    file(MAKE_DIRECTORY "${librespot-cpp_SOURCE_DIR}/lib/spdlog")
    file(
      WRITE "${librespot-cpp_SOURCE_DIR}/lib/spdlog/CMakeLists.txt"
      "add_library(spdlog INTERFACE)"
    )
  endif()
  add_subdirectory(${librespot-cpp_SOURCE_DIR} ${librespot-cpp_BINARY_DIR})
  target_link_libraries(mixxx-lib PRIVATE librespot-cpp)"""

new_librespot = """  FetchContent_MakeAvailable(spdlog)
  FetchContent_Populate(librespot-cpp)
  if(NOT EXISTS "${librespot-cpp_SOURCE_DIR}/lib/spdlog/CMakeLists.txt")
    file(MAKE_DIRECTORY "${librespot-cpp_SOURCE_DIR}/lib/spdlog")
    file(
      WRITE "${librespot-cpp_SOURCE_DIR}/lib/spdlog/CMakeLists.txt"
      "if(NOT TARGET spdlog)\\n  add_library(spdlog INTERFACE)\\nendif()"
    )
  endif()
  # Patch librespot-cpp to avoid target name collision and link signature mismatch
  file(READ "${librespot-cpp_SOURCE_DIR}/CMakeLists.txt" LIBRESPOT_CMAKELISTS)
  string(REPLACE "project(mixxx)" "project(librespot-cpp)" LIBRESPOT_CMAKELISTS "${LIBRESPOT_CMAKELISTS}")
  string(REPLACE "add_executable(mixxx" "add_library(librespot-cpp-lib STATIC" LIBRESPOT_CMAKELISTS "${LIBRESPOT_CMAKELISTS}")
  string(REPLACE "target_link_libraries(mixxx" "target_link_libraries(librespot-cpp-lib PRIVATE" LIBRESPOT_CMAKELISTS "${LIBRESPOT_CMAKELISTS}")
  file(WRITE "${librespot-cpp_SOURCE_DIR}/CMakeLists.txt" "${LIBRESPOT_CMAKELISTS}")

  add_subdirectory(${librespot-cpp_SOURCE_DIR} ${librespot-cpp_BINARY_DIR})
  target_link_libraries(mixxx-lib PRIVATE librespot-cpp-lib)"""

content = content.replace(old_librespot, new_librespot)

with open('CMakeLists.txt', 'w') as f:
    f.write(content)

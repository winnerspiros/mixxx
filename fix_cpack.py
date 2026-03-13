with open('CMakeLists.txt', 'r') as f:
    content = f.read()

content = content.replace('set(CPACK_PROJECT_CONFIG_FILE "${CMAKE_CURRENT_SOURCE_DIR}/packaging/CPackConfig.cmake")',
                          'set(\n  CPACK_PROJECT_CONFIG_FILE\n  "${CMAKE_CURRENT_SOURCE_DIR}/packaging/CPackConfig.cmake"\n)')

with open('CMakeLists.txt', 'w') as f:
    f.write(content)

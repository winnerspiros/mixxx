import sys

with open('CMakeLists.txt', 'r') as f:
    content = f.read()

# Make NetworkAuth discovery more robust
import_line = 'find_package(Qt${QT_VERSION_MAJOR} COMPONENTS NetworkAuth REQUIRED)'
if import_line in content:
    content = content.replace(
        import_line,
        'find_package(Qt${QT_VERSION_MAJOR} COMPONENTS NetworkAuth QUIET)'
    )

# Guard target_compile_definitions with BOTH the option and the found status
def_block = """if(NETWORKAUTH)
  target_compile_definitions(mixxx-lib PUBLIC NETWORKAUTH)
endif()"""

new_def_block = """if(NETWORKAUTH AND Qt${QT_VERSION_MAJOR}NetworkAuth_FOUND)
  target_compile_definitions(mixxx-lib PUBLIC NETWORKAUTH)
else()
  if(NETWORKAUTH)
    message(WARNING "QtNetworkAuth not found, disabling NETWORKAUTH features")
  endif()
endif()"""

if def_block in content:
    content = content.replace(def_block, new_def_block)

with open('CMakeLists.txt', 'w') as f:
    f.write(content)

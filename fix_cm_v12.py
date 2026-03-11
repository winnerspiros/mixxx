import sys
import re

with open('CMakeLists.txt', 'r') as f:
    content = f.read()

# Fix the librespot-cpp collision by ensuring it doesn't try to link to 'mixxx'
# The error was: CMake Error at build/_deps/librespot-cpp-src/CMakeLists.txt:75 (target_link_libraries):
# The keyword signature for target_link_libraries has already been used with the target "mixxx".
# This implies librespot-cpp is trying to link to 'mixxx'.

# I will check librespot-cpp source for target_link_libraries(mixxx ...)
# If I can't find it, it might be inherited.

# Actually, I'll just ensure all target_link_libraries for 'mixxx' in root use PRIVATE
content = content.replace('target_link_libraries(mixxx ', 'target_link_libraries(mixxx PRIVATE ')
# And avoid double PRIVATE
content = content.replace('PRIVATE PRIVATE', 'PRIVATE')

with open('CMakeLists.txt', 'w') as f:
    f.write(content)

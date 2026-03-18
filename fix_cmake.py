import re

with open('CMakeLists.txt', 'r') as f:
    lines = f.readlines()

new_lines = []
for line in lines:
    # Fix the strip calls duplication
    if 'string(STRIP "${MIXXX_VCPKG_ROOT}" MIXXX_VCPKG_ROOT)' in line:
        if not new_lines or 'string(STRIP "${MIXXX_VCPKG_ROOT}" MIXXX_VCPKG_ROOT)' not in new_lines[-1]:
            new_lines.append(line)
    else:
        new_lines.append(line)

with open('CMakeLists.txt', 'w') as f:
    f.writelines(new_lines)

import os

def fix_cmake():
    with open('CMakeLists.txt', 'r') as f:
        lines = f.readlines()

    new_lines = []
    for line in lines:
        # Resolve spdlog for librespot-cpp differently if FetchContent_Declare is used
        # Check if spdlog is already there to avoid duplicates
        if 'FetchContent_Declare(' in line and 'spdlog' in line:
            continue
        if 'FetchContent_MakeAvailable(spdlog)' in line:
            continue

        if 'FetchContent_Declare(' in line and 'librespot-cpp' in lines[lines.index(line)+1]:
             new_lines.append('  FetchContent_Declare(\n')
             new_lines.append('    spdlog\n')
             new_lines.append('    URL https://github.com/gabime/spdlog/archive/refs/tags/v1.12.0.zip\n')
             new_lines.append('  )\n')
             new_lines.append('  FetchContent_MakeAvailable(spdlog)\n')

        new_lines.append(line)

    with open('CMakeLists.txt', 'w') as f:
        f.writelines(new_lines)

fix_cmake()

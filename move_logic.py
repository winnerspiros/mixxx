import sys

with open('CMakeLists.txt', 'r') as f:
    lines = f.readlines()

# fatal_error_missing_env is 46-105 (0-indexed: 45-105)
# Actually let's find it by content to be sure.
start_func = -1
end_func = -1
for i, line in enumerate(lines):
    if 'function(fatal_error_missing_env)' in line:
        start_func = i
    if start_func != -1 and 'endfunction()' in line and i > start_func:
        end_func = i
        break

func_block = lines[start_func:end_func+1]

# VCPKG logic starts with if(NOT DEFINED VCPKG_TARGET_TRIPLET)
# and ends around message(STATUS "Early CMAKE_SYSTEM_NAME: ${CMAKE_SYSTEM_NAME}")
# which I saw was around 285.
start_vcpkg = -1
for i, line in enumerate(lines):
    if 'if(NOT DEFINED VCPKG_TARGET_TRIPLET)' in line and i > end_func:
        start_vcpkg = i
        break

# The block ends after the toolchain file setup.
# Let's find the last line of that block we saw.
end_vcpkg = -1
for i, line in enumerate(lines):
    if 'message(STATUS "Early CMAKE_SYSTEM_NAME: ${CMAKE_SYSTEM_NAME}")' in line:
        end_vcpkg = i
        break

vcpkg_block = lines[start_vcpkg:end_vcpkg+1]

# Remove the blocks from original positions.
# Remove vcpkg block first so indices don't shift for it yet,
# but wait, it's easier to remove from bottom to top.

new_lines = lines[:start_vcpkg] + lines[end_vcpkg+1:]
new_lines = new_lines[:start_func] + new_lines[end_func+1:]

# Find where to insert. Just before project()
insert_idx = -1
for i, line in enumerate(new_lines):
    if 'project(mixxx VERSION' in line:
        insert_idx = i
        break

# Insert func_block then vcpkg_block
combined_block = func_block + ["\n"] + vcpkg_block + ["\n"]
final_lines = new_lines[:insert_idx] + combined_block + new_lines[insert_idx:]

# Update CMAKE_SOURCE_DIR to CMAKE_CURRENT_SOURCE_DIR in func_block
for i in range(len(final_lines)):
    if '${CMAKE_SOURCE_DIR}' in final_lines[i]:
        final_lines[i] = final_lines[i].replace('${CMAKE_SOURCE_DIR}', '${CMAKE_CURRENT_SOURCE_DIR}')

with open('CMakeLists.txt', 'w') as f:
    f.writelines(final_lines)

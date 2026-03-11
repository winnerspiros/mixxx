import re
import sys

def extract_and_remove(content, start_marker, end_marker_regex):
    start_idx = content.find(start_marker)
    if start_idx == -1:
        return content, None

    # search for end marker after start
    match = re.search(end_marker_regex, content[start_idx:], re.DOTALL)
    if not match:
        return content, None

    end_idx = start_idx + match.end()
    block = content[start_idx:end_idx]
    new_content = content[:start_idx] + content[end_idx:]
    return new_content, block

with open('CMakeLists.txt', 'r') as f:
    content = f.read()

# 1. Extract download block from end
# It starts with if( followed by ((APPLE AND NOT IOS) OR WIN32 OR ANDROID)
# and ends with endif() else() ... endif()
dl_start_marker = 'if(\n  ((APPLE AND NOT IOS) OR WIN32 OR ANDROID)'
dl_end_regex = r'endif\(\)\nelse\(\)\n  # Reference to suppress intentionally unused variable warnings\n  set\(_dummy "\${BUILDENV_URL}" "\${BUILDENV_BASEPATH}" "\${BUILDENV_SHA256}"\)\nendif\(\)\n'
content, dl_block = extract_and_remove(content, dl_start_marker, dl_end_regex)

if not dl_block:
    print("Failed to extract download block")
    # try looser
    dl_start_marker = 'if(\n  ((APPLE AND NOT IOS) OR WIN32 OR ANDROID)'
    dl_end_regex = r'endif\(\)\nelse\(\)\n.*?endif\(\)\n'
    content, dl_block = extract_and_remove(content, dl_start_marker, dl_end_regex)

if not dl_block:
    print("CRITICAL: Could not find download block")
    sys.exit(1)

# 2. Extract android architecture block
# It starts with if(ANDROID) and contains arm64-v8a
and_start_marker = 'if(ANDROID)\n  if(VCPKG_TARGET_TRIPLET MATCHES "arm64")'
and_end_regex = r'endif\(\)\nendif\(\)\n'
content, and_block = extract_and_remove(content, and_start_marker, and_end_regex)

if not and_block:
    print("CRITICAL: Could not find android architecture block")
    sys.exit(1)

# 3. Find insertion point for download block
# after the initial MIXXX_VCPKG_ROOT from ENV
env_pull_marker = 'if(DEFINED ENV{MIXXX_VCPKG_ROOT} AND NOT DEFINED MIXXX_VCPKG_ROOT)\n  set(MIXXX_VCPKG_ROOT "$ENV{MIXXX_VCPKG_ROOT}")\nendif()\n'
insertion_point = content.find(env_pull_marker)
if insertion_point == -1:
    print("CRITICAL: Could not find env pull block")
    sys.exit(1)

insertion_point += len(env_pull_marker)

env_fix = '''
if(NOT DEFINED MIXXX_VCPKG_ROOT AND NOT DEFINED ENV{MIXXX_VCPKG_ROOT})
  if(DEFINED ENV{BUILDENV_BASEPATH})
    set(BUILDENV_BASEPATH "$ENV{BUILDENV_BASEPATH}")
  else()
    set(BUILDENV_BASEPATH "${CMAKE_SOURCE_DIR}/buildenv")
  endif()
  if(DEFINED ENV{BUILDENV_NAME})
    set(MIXXX_VCPKG_ROOT "${BUILDENV_BASEPATH}/$ENV{BUILDENV_NAME}")
  endif()
endif()
'''

content = content[:insertion_point] + env_fix + '\n' + dl_block + content[insertion_point:]

# 4. Find insertion point for android block after project()
project_match = re.search(r'project\(mixxx VERSION .*?\)\n', content)
if not project_match:
    print("CRITICAL: Could not find project block")
    sys.exit(1)

# Find the end of the option block following project
search_start = project_match.end()
# It usually ends before the message(STATUS "MIXXX_VCPKG_ROOT...") or similar
# Let's look for the first message(STATUS "MIXXX_VCPKG_ROOT...")
vcpkg_msg_marker = 'message(STATUS "MIXXX_VCPKG_ROOT: ${MIXXX_VCPKG_ROOT}")'
insertion_point_and = content.find(vcpkg_msg_marker)

if insertion_point_and == -1:
    insertion_point_and = search_start

content = content[:insertion_point_and] + '\n' + and_block + '\n' + content[insertion_point_and:]

with open('CMakeLists.txt', 'w') as f:
    f.write(content)

print("Successfully updated CMakeLists.txt")

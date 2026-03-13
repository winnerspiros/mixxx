import re

with open('CMakeLists.txt', 'r') as f:
    content = f.read()

# Fix mixed case and whitespace in if statements
content = re.sub(r'if\(\s+([^\n]+)\s+AND\s+NOT\s+([^\n]+)\s+AND\s+NOT\s+([^\n]+)\)', r'if(\1 and not \2 and not \3)', content)
content = re.sub(r'if\(\s*\(\s*\n\s+\(\s*APPLE AND NOT IOS\s*\)\s*\n\s+OR WIN32\s*\n\s+OR ANDROID\s*\n\s+OR \(CMAKE_SYSTEM_NAME STREQUAL "Android"\)\s*\n\s+\)\s*\n\s+AND NOT IS_DIRECTORY',
                 'if(\n  (\n    (APPLE AND NOT IOS)\n    OR WIN32\n    OR ANDROID\n    OR (CMAKE_SYSTEM_NAME STREQUAL "Android")\n  )\n  AND NOT IS_DIRECTORY',
                 content)

# Lowercase operators
content = content.replace(' AND ', ' and ')
content = content.replace(' OR ', ' or ')
content = content.replace(' NOT ', ' not ')

with open('CMakeLists.txt', 'w') as f:
    f.write(content)

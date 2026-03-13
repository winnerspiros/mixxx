import re

with open('CMakeLists.txt', 'r') as f:
    content = f.read()

content = content.replace('STREQUAL', 'strequal')
content = content.replace('MATCHES', 'matches')
content = content.replace('DEFINED', 'defined')
content = content.replace('EXISTS', 'exists')
content = content.replace('IS_DIRECTORY', 'is_directory')

with open('CMakeLists.txt', 'w') as f:
    f.write(content)

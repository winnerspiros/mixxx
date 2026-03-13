import re

with open('CMakeLists.txt', 'r') as f:
    content = f.read()

# Commands already handled mostly by gersemi, but operators might not be
content = content.replace(' AND ', ' and ')
content = content.replace(' OR ', ' or ')
content = content.replace(' NOT ', ' not ')
content = content.replace('DEFINED', 'defined')
content = content.replace('STREQUAL', 'strequal')
content = content.replace('MATCHES', 'matches')
content = content.replace('EXISTS', 'exists')
content = content.replace('IS_DIRECTORY', 'is_directory')

# Fix cases like if(NOT defined ...) to if(not defined ...)
content = content.replace('NOT defined', 'not defined')
content = content.replace('NOT exists', 'not exists')
content = content.replace('NOT is_directory', 'not is_directory')

with open('CMakeLists.txt', 'w') as f:
    f.write(content)

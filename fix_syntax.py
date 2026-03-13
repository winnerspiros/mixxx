with open('CMakeLists.txt', 'r') as f:
    content = f.read()

# Fix the extra paren that broke the if statement
content = content.replace(
    'if(((APPLE AND NOT IOS) OR WIN32 OR ANDROID OR (CMAKE_SYSTEM_NAME STREQUAL "Android")))\n  AND NOT IS_DIRECTORY',
    'if(\n  (\n    (APPLE AND NOT IOS)\n    OR WIN32\n    OR ANDROID\n    OR (CMAKE_SYSTEM_NAME STREQUAL "Android")\n  )\n  AND NOT IS_DIRECTORY'
)

with open('CMakeLists.txt', 'w') as f:
    f.write(content)

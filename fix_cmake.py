import sys

with open('CMakeLists.txt', 'r') as f:
    content = f.read()

# Make NetworkAuth quiet and disable NETWORKAUTH if not found
if 'find_package(Qt${QT_VERSION_MAJOR} COMPONENTS NetworkAuth REQUIRED)' in content:
    content = content.replace(
        'find_package(Qt${QT_VERSION_MAJOR} COMPONENTS NetworkAuth REQUIRED)',
        'find_package(Qt${QT_VERSION_MAJOR} COMPONENTS NetworkAuth QUIET)'
    )
    content = content.replace(
        'if(NETWORKAUTH)',
        'if(NETWORKAUTH AND Qt${QT_VERSION_MAJOR}NetworkAuth_FOUND)'
    )

with open('CMakeLists.txt', 'w') as f:
    f.write(content)

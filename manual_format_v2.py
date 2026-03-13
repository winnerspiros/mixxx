import re

with open('CMakeLists.txt', 'r') as f:
    content = f.read()

def fix_list_append(match):
    indent = match.group(1)
    args = match.group(2)
    return f'list(\n{indent}  APPEND\n{indent}  CPACK_DEBIAN_PACKAGE_DEPENDS\n{indent}  {args}\n{indent})'

# Target: list(APPEND CPACK_DEBIAN_PACKAGE_DEPENDS "..." )
content = re.sub(r'list\(\s*APPEND\s+CPACK_DEBIAN_PACKAGE_DEPENDS\s+("[^"]+")\s*\)',
                 r'list(\n      APPEND\n      CPACK_DEBIAN_PACKAGE_DEPENDS\n      \1\n    )',
                 content)

# Target: list(\n      APPEND CPACK_DEBIAN_PACKAGE_DEPENDS\n      "..."\n    )
content = content.replace('APPEND CPACK_DEBIAN_PACKAGE_DEPENDS', 'APPEND\n      CPACK_DEBIAN_PACKAGE_DEPENDS')

# Fix string commands
content = content.replace('PREPEND CPACK_DEBIAN_PACKAGE_DESCRIPTION_MERGED', 'PREPEND\n  CPACK_DEBIAN_PACKAGE_DESCRIPTION_MERGED')
content = content.replace('REPLACE "\\n\\n"', 'REPLACE\n  "\\n\\n"')
content = content.replace('REPLACE "\\n"', 'REPLACE\n  "\\n"')

with open('CMakeLists.txt', 'w') as f:
    f.write(content)

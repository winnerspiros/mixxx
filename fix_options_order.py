import re

with open("CMakeLists.txt", "r") as f:
    content = f.read()

# Find option definitions
options = []
patterns = [
    r'option\(QT6.*?\)',
    r'option\(NETWORKAUTH.*?\)',
    r'cmake_dependent_option\(\s*QML.*?\)',
]

for p in patterns:
    match = re.search(p, content, re.DOTALL)
    if match:
        options.append(match.group(0))
        content = content.replace(match.group(0), "")

# Remove resulting empty lines where options were
content = re.sub(r'\n\s*\n\s*\n', '\n\n', content)

# Insert options after project() call
# Find project() call
project_match = re.search(r'project\(.*?\)', content)
if project_match:
    insertion_point = project_match.end()
    content = content[:insertion_point] + "\n\n" + "\n".join(options) + "\n" + content[insertion_point:]

with open("CMakeLists.txt", "w") as f:
    f.write(content)

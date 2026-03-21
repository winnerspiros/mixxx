import sys
import re

with open('CMakeLists.txt', 'r') as f:
    lines = f.readlines()

new_lines = []
indent_level = 0
for line in lines:
    # Track indentation based on if/endif/else etc.
    # Note: this is a simple heuristic
    stripped = line.strip()

    # Check for the list(REMOVE_ITEM QT_COMPONENTS ...) multi-line fix
    if 'REMOVE_ITEM QT_COMPONENTS' in line:
        line = line.replace('REMOVE_ITEM QT_COMPONENTS', 'REMOVE_ITEM\n      QT_COMPONENTS')

    # Fix the comments at start of line that should be indented
    if line.startswith('#'):
        # Only indent if the previous line was indented
        if len(new_lines) > 0 and new_lines[-1].startswith('  '):
            # Match common unindented comments from the report
            if re.match(r'#(TODO|https|shoutidjc|Force|Multimedia|Try again|Specifically|This is|qt_add_executable|below|According|builds|defines|_WIN32_WINNT|Helps|Need)', stripped):
                line = '  ' + line
            elif stripped.startswith('#') and stripped.endswith('0x0601'):
                line = '  ' + line

    new_lines.append(line)

with open('CMakeLists.txt', 'w') as f:
    f.writelines(new_lines)

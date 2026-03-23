import sys

with open('src/qml/qmlpreferencesproxy.cpp', 'r') as f:
    lines = f.readlines()

new_lines = []
for line in lines:
    if 'emit videoFrameAvailable' in line:
        if '#ifndef Q_OS_ANDROID' not in lines[lines.index(line)-1]:
             new_lines.append('#ifndef Q_OS_ANDROID\n')
             new_lines.append(line)
             new_lines.append('#endif\n')
             continue
    new_lines.append(line)

with open('src/qml/qmlpreferencesproxy.cpp', 'w') as f:
    f.writelines(new_lines)

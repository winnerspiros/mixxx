import re

with open('src/mixxxmainwindow.cpp', 'r') as f:
    content = f.read()

# Combine consecutive DBus include guards
pattern = r'#if defined\(__LINUX__\) && !defined\(__ANDROID__\)\n#include <QDBusConnection>\n#endif\n#if defined\(__LINUX__\) && !defined\(__ANDROID__\)\n#include <QDBusConnectionInterface>\n#endif'
replacement = '#if defined(__LINUX__) && !defined(__ANDROID__)\n#include <QDBusConnection>\n#include <QDBusConnectionInterface>\n#endif'
content = content.replace(pattern, replacement)

# Clean up nested blocks
content = re.sub(r'(#if defined\(__LINUX__\) && !defined\(__ANDROID__\)\n)+#if defined\(__LINUX__\) && !defined\(__ANDROID__\)\n', '#if defined(__LINUX__) && !defined(__ANDROID__)\n', content)

with open('src/mixxxmainwindow.cpp', 'w') as f:
    f.write(content)

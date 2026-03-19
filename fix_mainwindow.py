import re

with open('src/mixxxmainwindow.cpp', 'r') as f:
    content = f.read()

# Replace those nested identical blocks with a single one
pattern1 = r'(#if defined\(__LINUX__\) && !defined\(__ANDROID__\)\n)+#include <QDBusConnection>\n(#endif\n)+'
content = re.sub(pattern1, '#if defined(__LINUX__) && !defined(__ANDROID__)\n#include <QDBusConnection>\n#endif\n', content)

pattern2 = r'(#if defined\(__LINUX__\) && !defined\(__ANDROID__\)\n)+#include <QDBusConnectionInterface>\n(#endif\n)+'
content = re.sub(pattern2, '#if defined(__LINUX__) && !defined(__ANDROID__)\n#include <QDBusConnectionInterface>\n#endif\n', content)

# Clean up those empty blocks at lines 75-84
content = re.sub(r'#if defined\(__LINUX__\) && !defined\(__ANDROID__\)\n(#if defined\(__LINUX__\) && !defined\(__ANDROID__\)\n)+#endif\n(#endif\n)+', '', content)

with open('src/mixxxmainwindow.cpp', 'w') as f:
    f.write(content)

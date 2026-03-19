import sys
f = 'src/main.cpp'
lines = open(f).readlines()
# Remove lines 122-124 and 129-130 as suggested by bot
# Lines are 0-indexed in python list
# 122: #ifdef Q_OS_ANDROID
# 123:             mainWindow.showMaximized();
# 124: #else
# ...
# 129: #endif
# 130: #endif

# We need to be careful with indices.
# 121 (Displaying main window) is index 120.
# 122 (#ifdef) is index 121.
# 123 (showMaximized) is index 122.
# 124 (#else) is index 123.
# 125 (#ifdef) is index 124.
# 126 (showMaximized) is index 125.
# 127 (#else) is index 126.
# 128 (show) is index 127.
# 129 (#endif) is index 128.
# 130 (#endif) is index 129.

del lines[129] # 130
del lines[128] # 129
del lines[123] # 124
del lines[122] # 123
del lines[121] # 122

open(f, 'w').writelines(lines)

import re

with open("CMakeLists.txt", "r") as f:
    content = f.read()

# Fix the whitespace warnings in buildenv logic
content = re.sub(r'"Downloading file "\${BUILDENV_URL}" to "\${BUILDENV_BASEPATH}" \.\.\."',
                 r'"Downloading file " ${BUILDENV_URL} " to " ${BUILDENV_BASEPATH} " ..."', content)
content = re.sub(r'"Verify SHA256 of downloaded file "\${BUILDENV_BASEPATH}/\${BUILDENV_NAME}"\.zip \.\.\."',
                 r'"Verify SHA256 of downloaded file " ${BUILDENV_BASEPATH}/${BUILDENV_NAME} ".zip ..."', content)
content = re.sub(r'"SHA256 "\${BUILDENV_SHA256}" is correct!"',
                 r'"SHA256 " ${BUILDENV_SHA256} " is correct!"', content)
content = re.sub(r'"Unpacking file "\${BUILDENV_BASEPATH}/\${BUILDENV_NAME}"\.zip \.\.\."',
                 r'"Unpacking file " ${BUILDENV_BASEPATH}/${BUILDENV_NAME} ".zip ..."', content)
content = re.sub(r'expected: "\${BUILDENV_SHA256}"',
                 r'expected: ${BUILDENV_SHA256}', content)

# Fix librespot-cpp / spdlog issue
# librespot-cpp expects spdlog in lib/spdlog.
# We should probably use FetchContent to put spdlog there or just provide it as a target.
# Looking at the trace, it fails at build/_deps/librespot-cpp-src/CMakeLists.txt:60 (add_subdirectory)
# because lib/spdlog is empty.

# I will replace the FetchContent logic for librespot-cpp to not try to build its internal spdlog if possible,
# or better, just fetch spdlog and tell librespot-cpp where it is.
# Actually, if I declare spdlog first with FetchContent, FetchContent_MakeAvailable(spdlog)
# will create the spdlog target. Then I need to make sure librespot-cpp uses it.

# Let's check how librespot-cpp's CMakeLists.txt looks (I'll guess based on common patterns)
# If it has add_subdirectory(lib/spdlog), I can try to comment it out if the target already exists.

# Fix the keyword signature error for "mixxx" target.
# The error says Qt6CoreMacros.cmake:549 (target_link_libraries) used it.
# Usually Qt macros use keyword signatures if the target was created with one, or if they are told to.
# If I use PRIVATE in my first call to target_link_libraries(mixxx ...), then all subsequent calls must use it.
# The trace shows:
# 1903: qt_add_executable(mixxx src/main.cpp MANUAL_FINALIZATION)
# ...
# 2012: target_link_libraries(mixxx PRIVATE mixxx-lib mixxx-gitinfostore)

# Wait, if I'm on Android:
# 1903: qt_add_executable(mixxx src/main.cpp MANUAL_FINALIZATION)
# ... it might call target_link_libraries internally without keywords.
# Let's try to change line 2012 to be plain if it's the problem, OR make sure everything is consistent.
# Actually, the recommended way is to ALWAYS use keywords.

# Let's look at line 1861-1881 (Qt6 discovery)
# It seems my GLOB search might be finding multiple things or the path is wrong.

with open("CMakeLists.txt", "w") as f:
    f.write(content)

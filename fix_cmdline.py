import re

with open('src/util/cmdlineargs.h', 'r') as f:
    content = f.read()

# Fix the duplicate block at the end
content = re.sub(r'\} // namespace mixxx\n\nusing CmdlineArgs = mixxx::CmdlineArgs;\n\n\} // namespace mixxx\n\nusing CmdlineArgs = mixxx::CmdlineArgs;', '} // namespace mixxx\n\nusing CmdlineArgs = mixxx::CmdlineArgs;', content)

with open('src/util/cmdlineargs.h', 'w') as f:
    f.write(content)

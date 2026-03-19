import re

with open('src/util/cmdlineargs.h', 'r') as f:
    content = f.read()

# Remove the double namespace nesting
content = re.sub(r'namespace mixxx \{\s+namespace mixxx \{', 'namespace mixxx {', content)
# Fix the end to ensure only one closing brace and one using statement
content = re.sub(r'\} // namespace mixxx\s+\} // namespace mixxx\s+using CmdlineArgs = mixxx::CmdlineArgs;', '} // namespace mixxx\n\nusing CmdlineArgs = mixxx::CmdlineArgs;', content)

with open('src/util/cmdlineargs.h', 'w') as f:
    f.write(content)

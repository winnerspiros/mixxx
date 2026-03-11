import re
with open('CMakeLists.txt', 'r') as f:
    lines = f.readlines()
stack = []
pattern = re.compile(r'\b(if|foreach|while|function|macro|endif|endforeach|endwhile|endfunction|endmacro)\b\s*\(')
for i, line in enumerate(lines):
    line = line.split('#')[0]
    for match in pattern.finditer(line):
        word = match.group(1).lower()
        line_num = i + 1
        if word in ['if', 'foreach', 'while', 'function', 'macro']:
            stack.append((word, line_num))
        else:
            if not stack:
                print(f"Error: {word} at line {line_num} has no matching start")
            else:
                start_type, start_line = stack.pop()
                expected_end = "end" + start_type
                if word != expected_end:
                    print(f"Error: {word} at line {line_num} does not match {start_type} at line {start_line}")
if stack:
    for start_type, start_line in stack:
        print(f"Error: {start_type} at line {start_line} is not closed")

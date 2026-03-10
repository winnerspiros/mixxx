import sys

def find_mismatch(filename):
    with open(filename, 'r') as f:
        content = f.read()

    in_string = False
    escape = False
    line_num = 1
    col_num = 0
    string_start_line = 0
    string_start_col = 0

    for i, char in enumerate(content):
        if char == '\n':
            line_num += 1
            col_num = 0
        else:
            col_num += 1

        if escape:
            escape = False
            continue

        if char == '\\':
            escape = True
            continue

        if char == '"':
            if in_string:
                in_string = False
            else:
                in_string = True
                string_start_line = line_num
                string_start_col = col_num

    if in_string:
        print(f"Unclosed string starting at line {string_start_line}, column {string_start_col}")
        # Print some context
        lines = content.split('\n')
        start_line = max(0, string_start_line - 2)
        end_line = min(len(lines), string_start_line + 2)
        for i in range(start_line, end_line):
            prefix = "-> " if i == string_start_line - 1 else "   "
            print(f"{prefix}{i+1}: {lines[i]}")
    else:
        print("No unclosed strings found (considering escapes)")

if __name__ == "__main__":
    find_mismatch("CMakeLists.txt")

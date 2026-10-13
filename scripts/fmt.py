#!/usr/bin/env python3
import glob
import os
import re
import subprocess
import sys

def unalign_locals(content):
    lines = content.splitlines()
    out = []

    in_struct = False
    in_func = False
    brace_depth = 0
    in_preprocessor = False

    for line in lines:
        stripped = line.strip()

        # Check preprocessor line
        if stripped.startswith('#') or in_preprocessor:
            in_preprocessor = line.endswith('\\')
            out.append(line)
            continue
        in_preprocessor = False

        # Check struct/enum/union at file scope
        if brace_depth == 0:
            if re.match(r'^(typedef\s+)?(struct|union|enum)\b', stripped):
                in_struct = True
            elif '(' in line and '{' in line and not in_struct:
                in_func = True

        # Count braces outside comments/strings
        clean = re.sub(r'"[^"\\]*(?:\\.[^"\\]*)*"', '""', stripped)
        clean = re.sub(r"'[^'\\]*(?:\\.[^'\\]*)*'", "''", clean)
        clean = re.sub(r'//.*$', '', clean)

        depth_delta = clean.count('{') - clean.count('}')

        if brace_depth == 0 and '{' in clean:
            if not in_struct:
                in_func = True

        # Process line if inside function body and not in a struct
        if in_func and not in_struct and brace_depth > 0:
            if not stripped.startswith(('//', '/*', '*', '.')):
                indent = ''
                indent_m = re.match(r'^(\t+)', line)
                if indent_m:
                    indent = indent_m.group(1)

                rest = line[len(indent):]

                # Unalign assignment '=':
                rest = re.sub(r'(?<![=<>!+\-*/&|^%])[\t ]{2,}=([^=])', r' =\1', rest)
                rest = re.sub(r'(?<![=<>!+\-*/&|^%])\t+=([^=])', r' =\1', rest)

                # Unalign declarations:
                words = rest.split()
                if len(words) >= 2 and ('\t' in rest or '  ' in rest):
                    first = words[0]
                    if first not in ('return', 'goto', 'case', 'sizeof', 'typedef', 'if', 'while', 'for', 'switch'):
                        m = re.match(
                            r'^((?:(?:const|static|volatile|register|unsigned|signed|struct\s+\w+|enum\s+\w+|union\s+\w+|\w+)\s*[*]*\s*)+?)[\t ]{2,}([*]*\w+.*)$',
                            rest
                        )
                        if m:
                            t_part = re.sub(r'[\t ]+', ' ', m.group(1).rstrip())
                            rest = f"{t_part} {m.group(2)}"

                line = indent + rest

        brace_depth += depth_delta
        if brace_depth == 0:
            in_struct = False
            in_func = False

        out.append(line)

    return '\n'.join(out) + ('\n' if content.endswith('\n') else '')

def format_code(content, assume_filename='test.c'):
    # Determine repo root to find .clang-format
    script_dir = os.path.dirname(os.path.abspath(__file__))
    repo_root = os.path.abspath(os.path.join(script_dir, '..'))

    cmd = ['clang-format', f'--assume-filename={assume_filename}']
    try:
        proc = subprocess.run(
            cmd,
            input=content,
            cwd=repo_root,
            check=True,
            capture_output=True,
            text=True
        )
        formatted = proc.stdout
    except Exception as e:
        sys.stderr.write(f"clang-format error: {e}\n")
        return content

    # Collapse closing paren followed by newline brace: )\n{ -> ) {
    formatted = re.sub(r'(\n[ \t]*\))\n[ \t]*\{', r'\1 {', formatted)

    # Unalign local variable declarations and assignments
    return unalign_locals(formatted)

def format_file(filepath):
    norm = os.path.normpath(filepath)
    parts = norm.split(os.sep)
    if any(p in ('.git', 'build', 'fixtures', '.nour') for p in parts) or norm.endswith('nour_header.h'):
        return

    if not (filepath.endswith('.c') or filepath.endswith('.h')):
        return

    with open(filepath, 'r', encoding='utf-8') as f:
        original = f.read()

    final = format_code(original, assume_filename=os.path.abspath(filepath))

    if final != original:
        with open(filepath, 'w', encoding='utf-8') as f:
            f.write(final)

def find_c_files():
    patterns = [
        'nour-core/**/*.[ch]',
        'nour-cli/**/*.[ch]',
        'include/**/*.[ch]',
        'tests/**/*.[ch]',
        '*.[ch]',
    ]
    seen = set()
    files = []
    for pat in patterns:
        for f in glob.glob(pat, recursive=True):
            if os.path.isfile(f) and f not in seen:
                seen.add(f)
                files.append(f)
    return sorted(files)

def main():
    # Stdin mode (used by editor format-on-save)
    if len(sys.argv) > 1 and sys.argv[1] in ('-', '--stdin'):
        filename = sys.argv[2] if len(sys.argv) > 2 else 'test.c'
        content = sys.stdin.read()
        output = format_code(content, assume_filename=filename)
        sys.stdout.write(output)
        return

    # File arguments mode
    if len(sys.argv) > 1:
        files = sys.argv[1:]
    else:
        files = find_c_files()

    for f in files:
        format_file(f)

if __name__ == '__main__':
    main()

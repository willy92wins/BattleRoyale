"""Append the missing #endif lines at the end of Enforce files whose #ifdef blocks run to EOF.

The original sources open blocks such as `#ifdef SERVER` on line 1 and never close them; the
engine treats end of file as the close. Appending the closes at EOF keeps the same meaning.
Usage: python close_ifdefs.py <root> [--check]
"""
import os
import re
import sys

OPEN = re.compile(r"^\s*#\s*(ifdef|ifndef|if)\b")
CLOSE = re.compile(r"^\s*#\s*endif\b")


def depth_at_eof(text):
    depth = 0
    for line in text.splitlines():
        if OPEN.match(line):
            depth += 1
        elif CLOSE.match(line):
            depth -= 1
            if depth < 0:
                raise ValueError("unbalanced #endif")
    return depth


def main():
    root = sys.argv[1]
    check = "--check" in sys.argv
    changed = 0
    for base, _, files in os.walk(root):
        for name in files:
            if not name.endswith(".c"):
                continue
            path = os.path.join(base, name)
            raw = open(path, "rb").read()
            text = raw.decode("utf-8")
            missing = depth_at_eof(text)
            if missing <= 0:
                continue
            changed += 1
            rel = os.path.relpath(path, root)
            print(f"{rel}: {missing} missing #endif")
            if check:
                continue
            newline = "\r\n" if b"\r\n" in raw else "\n"
            body = text if text.endswith(("\n", "\r\n")) else text + newline
            body += ("#endif" + newline) * missing
            open(path, "wb").write(body.encode("utf-8"))
    print(f"files needing closes: {changed}")


if __name__ == "__main__":
    main()

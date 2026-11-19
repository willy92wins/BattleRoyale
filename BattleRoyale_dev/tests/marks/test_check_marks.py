"""Exit-code fixtures for check_marks.py.

    python -I BattleRoyale_dev/tests/marks/test_check_marks.py

Exit 0 when each fixture returns the code listed below. Exit 1 otherwise.
"""

import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
CHECKER = os.path.join(HERE, "check_marks.py")
SPEC = os.path.join(HERE, "specs", "f4_marks.json")
FIXTURES = os.path.join(HERE, "fixtures")

CASES = [
    ("pass.log", 0),
    ("missing.log", 1),
    ("inverted.log", 1),
    ("late.log", 1),
]


def main():
    failed = False
    for name, want in CASES:
        log_path = os.path.join(FIXTURES, name)
        result = subprocess.run(
            [sys.executable, "-I", CHECKER, "--log", log_path, "--spec", SPEC],
            capture_output=True,
            text=True,
        )
        if result.returncode != want:
            failed = True
            print("FAIL %s exit %s want %s" % (name, result.returncode, want))
            print(result.stdout)
            print(result.stderr)
        else:
            detail = (result.stdout or "").strip()
            print("OK %s exit %s %s" % (name, result.returncode, detail))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())

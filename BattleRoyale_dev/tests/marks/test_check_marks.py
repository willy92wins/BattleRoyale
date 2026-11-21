"""Exit-code and reason fixtures for check_marks.py.

    python -I BattleRoyale_dev/tests/marks/test_check_marks.py

Exit 0 when each fixture returns the code and the reason text listed below.
A negative that exits 1 with an unrelated message (for example a traceback)
does not pass. Exit 1 otherwise.
"""

import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
CHECKER = os.path.join(HERE, "check_marks.py")
SPEC = os.path.join(HERE, "specs", "f4_marks.json")
TIGHT = os.path.join(HERE, "specs", "f4_marks_tight.json")
EMPTY = os.path.join(HERE, "specs", "empty.json")
NAN = os.path.join(HERE, "specs", "nan_tolerance.json")
FIXTURES = os.path.join(HERE, "fixtures")

# (log, spec, exit, stdout substring)
CASES = [
    ("pass.log", SPEC, 0, "OK"),
    ("edge_60_100.log", TIGHT, 0, "OK"),
    ("missing.log", SPEC, 1, "missing:"),
    ("inverted.log", SPEC, 1, "out of order:"),
    ("late.log", SPEC, 1, "out of time:"),
    ("early.log", SPEC, 1, "out of time:"),
    ("repeated.log", SPEC, 1, "ambiguous:"),
    ("earlier_stamp.log", SPEC, 1, "out of time order:"),
    ("pass.log", EMPTY, 1, "empty spec"),
    ("pass.log", NAN, 1, "non-finite tolerance:"),
]


def main():
    failed = False
    for name, spec, want, reason in CASES:
        log_path = os.path.join(FIXTURES, name)
        result = subprocess.run(
            [sys.executable, "-I", CHECKER, "--log", log_path, "--spec", spec],
            capture_output=True,
            text=True,
        )
        detail = (result.stdout or "").strip()
        if result.returncode != want or reason not in detail:
            failed = True
            print("FAIL %s exit %s want %s" % (name, result.returncode, want))
            print("stdout: %s" % detail)
            print("stderr: %s" % (result.stderr or "").strip())
            print("missing reason: %s" % reason)
        else:
            print("OK %s exit %s %s" % (name, result.returncode, detail))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())

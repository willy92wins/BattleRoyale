"""Check that a DayZ script log contains marks in order, and on time.

Timestamps come from parse_timings.iter_script_lines (LINE_TS_RE: HH:MM:SS.mmm
at the start of a line). This file does not change that parser.

Spec JSON: {"marks": [{"name": "...", "contains": "substring",
  "after": "other-name", "delay_seconds": 60, "tolerance_seconds": 1}]}
"after" is optional. When set, this mark's stamp must sit within
delay_seconds +/- tolerance_seconds of that earlier mark.

    python -I BattleRoyale_dev/tests/marks/check_marks.py --log <script.log> --spec <spec.json>

Exit 0 when every mark is present, in order, and inside its window.
Exit 1 when one is missing, out of order, or outside the window. The failing
mark name is printed.
"""

import argparse
import json
import os
import sys

TIMING_DIR = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "timing")
if TIMING_DIR not in sys.path:
    sys.path.insert(0, TIMING_DIR)

import parse_timings  # noqa: E402


def load_events(path):
    lines = parse_timings.read_lines(path)
    events = []
    generator, _info = parse_timings.iter_script_lines(lines)
    for number, stamp, text in generator:
        events.append((number, stamp, text))
    return events


def check(events, spec):
    marks = spec.get("marks") or []
    found = {}
    cursor = 0
    for mark in marks:
        name = mark["name"]
        needle = mark["contains"]
        hit = None
        for index in range(cursor, len(events)):
            _number, stamp, text = events[index]
            if needle in text:
                hit = (index, stamp, text)
                break
        if hit is None:
            earlier = None
            for index in range(0, cursor):
                if needle in events[index][2]:
                    earlier = index
                    break
            if earlier is not None:
                return "out of order: %s" % name
            return "missing: %s" % name
        index, stamp, _text = hit
        if stamp is None:
            return "missing timestamp: %s" % name
        cursor = index + 1
        found[name] = stamp
        if "after" in mark:
            origin = found.get(mark["after"])
            if origin is None:
                return "missing reference: %s" % name
            delay = float(mark.get("delay_seconds", 0))
            tolerance = float(mark.get("tolerance_seconds", 0))
            delta = (stamp - origin).total_seconds()
            if abs(delta - delay) > tolerance:
                return "out of time: %s (delta %s s, want %s +/- %s)" % (name, delta, delay, tolerance)
    return None


def main(argv):
    parser = argparse.ArgumentParser(description="Check ordered script-log marks")
    parser.add_argument("--log", required=True)
    parser.add_argument("--spec", required=True)
    args = parser.parse_args(argv)
    with open(args.spec, "r", encoding="utf-8") as handle:
        spec = json.load(handle)
    problem = check(load_events(args.log), spec)
    if problem:
        print(problem)
        return 1
    print("OK %s marks" % len(spec.get("marks") or []))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

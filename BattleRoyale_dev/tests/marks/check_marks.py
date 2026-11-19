"""Check that a DayZ script log contains marks in order, and on time.

Timestamps come from parse_timings.iter_script_lines (LINE_TS_RE: HH:MM:SS.mmm
at the start of a line). This file does not change that parser.

Spec JSON: {"marks": [{"name": "...", "contains": "substring",
  "after": "other-name", "delay_seconds": 60, "tolerance_seconds": 1,
  "occurrence": "unique"}]}
"after" is optional. When set, this mark's stamp must sit within
delay_seconds +/- tolerance_seconds of that earlier mark.
"occurrence" is "unique" (default), "first" or "last". A needle that appears
more than once is an error unless the spec names first or last.
Times are compared in whole milliseconds. A tolerance that is not a finite
number, and a spec with no marks, are errors.

    python -I BattleRoyale_dev/tests/marks/check_marks.py --log <script.log> --spec <spec.json>

Exit 0 when every mark is present, in order, and inside its window.
Exit 1 otherwise. The failing mark name and the reason are printed.
"""

import argparse
import json
import math
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


def _delta_ms(later, earlier):
    # parse_timings datetimes already follow a midnight crossing. The log resolution is 1 ms.
    return int(round((later - earlier).total_seconds() * 1000.0))


def _finite_number(value):
    if isinstance(value, bool):
        return None
    if isinstance(value, str):
        try:
            value = float(value)
        except ValueError:
            return None
    if isinstance(value, (int, float)) and math.isfinite(value):
        return float(value)
    return None


def check(events, spec):
    if not isinstance(spec, dict):
        return "empty spec"
    marks = spec.get("marks")
    if not marks:
        return "empty spec"
    found = {}
    cursor = 0
    previous = None
    for mark in marks:
        name = mark.get("name", "?")
        needle = mark.get("contains")
        if not needle:
            return "missing needle: %s" % name
        occurrence = mark.get("occurrence", "unique")
        if occurrence not in ("unique", "first", "last"):
            return "bad occurrence: %s" % name
        hits = []
        for index in range(len(events)):
            _number, stamp, text = events[index]
            if needle in text:
                hits.append((index, stamp))
        if not hits:
            return "missing: %s" % name
        if occurrence == "unique" and len(hits) != 1:
            return "ambiguous: %s" % name
        if occurrence == "last":
            index, stamp = hits[-1]
        elif occurrence == "first":
            index, stamp = hits[0]
        else:
            index, stamp = hits[0]
        if index < cursor:
            return "out of order: %s" % name
        if stamp is None:
            return "missing timestamp: %s" % name
        if previous is not None and stamp < previous:
            return "out of time order: %s" % name
        cursor = index + 1
        found[name] = stamp
        previous = stamp
        if "after" in mark:
            origin = found.get(mark["after"])
            if origin is None:
                return "missing reference: %s" % name
            delay = _finite_number(mark.get("delay_seconds", 0))
            tolerance = _finite_number(mark.get("tolerance_seconds", 0))
            if delay is None or tolerance is None:
                return "non-finite tolerance: %s" % name
            # Whole milliseconds. 60.100 s against 60 +/- 0.1 s is 100 ms, on the boundary, and passes.
            delta_ms = _delta_ms(stamp, origin)
            delay_ms = int(round(delay * 1000.0))
            tolerance_ms = int(round(tolerance * 1000.0))
            if abs(delta_ms - delay_ms) > tolerance_ms:
                return "out of time: %s (delta %s ms, want %s +/- %s ms)" % (name, delta_ms, delay_ms, tolerance_ms)
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

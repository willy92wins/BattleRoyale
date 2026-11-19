"""Property checks for tools/flight_path.py (mirror of BattleRoyaleFlightPath).

    python -I tools/tests/test_flight_path.py
    python -I tools/tests/test_flight_path.py --self-test

Exit 0 when every seeded line meets the flight contract. Exit 1 otherwise.
--self-test feeds a same-edge segment to the opposite-edge rule and requires
that rule to reject it. Exit 0 means the rejection happened.
"""

import os
import random
import sys

TOOLS = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if TOOLS not in sys.path:
    sys.path.insert(0, TOOLS)

import flight_path as fp

WORLD = 15360.0
RADIUS = 3000.0  # BattleRoyaleZoneData.c static_sizes last entry, zone 1 when num_zones is 7
MARGIN = 600.0
SPEED = 100.0
MIN_ALT = 800.0
MAX_ALT = 1100.0
FACTOR = 1.0
SEEDS = 1000


def opposite_edges(start, end, world, margin):
    near = margin
    far = world - margin
    def on_pair(a, b):
        return (a == near and b == far) or (a == far and b == near)
    east_west = on_pair(start[0], end[0])
    north_south = on_pair(start[2], end[2])
    return east_west or north_south


def inside_inset(start, end, world, margin):
    near = margin
    far = world - margin
    for point in (start, end):
        if point[0] < near or point[0] > far or point[2] < near or point[2] > far:
            return False
    return True


def audit(path, zone_center, zone_radius, factor, min_alt, max_alt, world, margin, speed, check_opposite=True):
    errors = []
    start, end = path["start"], path["end"]
    if check_opposite and not opposite_edges(start, end, world, margin):
        errors.append("edges are not opposite")
    if not inside_inset(start, end, world, margin):
        errors.append("outside the edge margin")
    for point in (start, end):
        if point[1] < min_alt or point[1] > max_alt:
            errors.append("altitude %s outside [%s, %s]" % (point[1], min_alt, max_alt))
    if path["length_m"] <= 0:
        errors.append("length is not positive")
    if speed > 0 and abs(path["duration_s"] - path["length_m"] / speed) > 1e-6:
        errors.append("duration is not L/speed")
    if factor > 0:
        dist = fp.distance_xz_to_segment(zone_center, start, end)
        if dist > factor * zone_radius + 1e-6:
            errors.append("zone distance %s > %s" % (dist, factor * zone_radius))
    return errors


def realistic_center(rng):
    lo = RADIUS
    hi = WORLD - RADIUS
    return (rng.uniform(lo, hi), 0.0, rng.uniform(lo, hi))


def run():
    failures = []
    for seed in range(1, SEEDS + 1):
        rng = random.Random(seed)
        center = realistic_center(rng)
        path = fp.from_seed(seed, WORLD, center, RADIUS, SPEED, MIN_ALT, MAX_ALT, MARGIN, FACTOR)
        errors = audit(path, center, RADIUS, FACTOR, MIN_ALT, MAX_ALT, WORLD, MARGIN, SPEED)
        if errors:
            failures.append("seed %s: %s" % (seed, "; ".join(errors)))
            if len(failures) >= 8:
                break

    outside = fp.from_seed(1, WORLD, (-1000.0, 0.0, 8000.0), RADIUS, SPEED, MIN_ALT, MAX_ALT, MARGIN, FACTOR)
    if not outside["used_fallback"]:
        failures.append("centre outside the map did not use the fallback")

    zero_radius = fp.from_seed(2, WORLD, (7000.0, 0.0, 7000.0), 0.0, SPEED, MIN_ALT, MAX_ALT, MARGIN, FACTOR)
    if not zero_radius["used_fallback"]:
        failures.append("radius 0 with a positive factor did not use the fallback")

    unrestricted_far = 0
    for seed in range(1, 51):
        rng = random.Random(10000 + seed)
        center = realistic_center(rng)
        path = fp.from_seed(seed, WORLD, center, RADIUS, SPEED, MIN_ALT, MAX_ALT, MARGIN, 0.0)
        if path["used_fallback"]:
            failures.append("factor 0 used the fallback at seed %s" % seed)
            break
        dist = fp.distance_xz_to_segment(center, path["start"], path["end"])
        if dist > RADIUS:
            unrestricted_far += 1
        errors = audit(path, center, RADIUS, 0.0, MIN_ALT, MAX_ALT, WORLD, MARGIN, SPEED)
        if errors:
            failures.append("factor 0 seed %s: %s" % (seed, "; ".join(errors)))
            break
    if unrestricted_far == 0:
        failures.append("factor 0 never produced a line farther than R1 from the centre")

    return failures


def self_test():
    same_edge = {
        "start": (MARGIN, MIN_ALT, 4000.0),
        "end": (MARGIN, MAX_ALT, 9000.0),
        "length_m": 5000.0,
        "duration_s": 50.0,
        "used_fallback": False,
    }
    center = (7680.0, 0.0, 7680.0)
    caught = audit(same_edge, center, RADIUS, 0.0, MIN_ALT, MAX_ALT, WORLD, MARGIN, SPEED, check_opposite=True)
    if not any("opposite" in item for item in caught):
        print("self-test: opposite-edge rule did not fire")
        return 1
    muted = audit(same_edge, center, RADIUS, 0.0, MIN_ALT, MAX_ALT, WORLD, MARGIN, SPEED, check_opposite=False)
    if any("opposite" in item for item in muted):
        print("self-test: muted rule still reported opposite edges")
        return 1
    print("self-test: opposite-edge rule rejected a same-edge segment")
    return 0


def main(argv):
    if "--self-test" in argv:
        return self_test()
    failures = run()
    if failures:
        print("FAIL %s" % len(failures))
        for item in failures:
            print(item)
        return 1
    print("OK %s seeds plus negatives" % SEEDS)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

"""Property checks for tools/flight_path.py (mirror of BattleRoyaleFlightPath).

Distance and length are computed here. They do not call flight_path helpers.

    python -I tools/tests/test_flight_path.py
    python -I tools/tests/test_flight_path.py --self-test

Exit 0 when every seeded line meets the flight contract. Exit 1 otherwise.
--self-test feeds a same-edge segment to the opposite-edge rule and requires
that rule to reject it. Exit 0 means the rejection happened.
"""

import math
import os
import random
import struct
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


def xz_distance(point, start, finish):
    """Minimum XZ distance from a point to a segment. Local to this test."""
    abx = finish[0] - start[0]
    abz = finish[2] - start[2]
    ab2 = abx * abx + abz * abz
    t = 0.0
    if ab2 > 0.0:
        t = ((point[0] - start[0]) * abx + (point[2] - start[2]) * abz) / ab2
        if t < 0.0:
            t = 0.0
        elif t > 1.0:
            t = 1.0
    dx = start[0] + abx * t - point[0]
    dz = start[2] + abz * t - point[2]
    return math.sqrt(dx * dx + dz * dz)


def length_3d(start, end):
    """Euclidean length. Local to this test."""
    dx = end[0] - start[0]
    dy = end[1] - start[1]
    dz = end[2] - start[2]
    return math.sqrt(dx * dx + dy * dy + dz * dz)


def opposite_edges(start, end, world, margin):
    near = margin
    far = world - margin

    def on_pair(a, b):
        return (a == near and b == far) or (a == far and b == near)

    return on_pair(start[0], end[0]) or on_pair(start[2], end[2])


def axis_of(start, end, world, margin):
    near = margin
    far = world - margin

    def on_pair(a, b):
        return (a == near and b == far) or (a == far and b == near)

    if on_pair(start[0], end[0]):
        return "ew"
    if on_pair(start[2], end[2]):
        return "ns"
    return ""


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
    measured = length_3d(start, end)
    if measured <= 0:
        errors.append("length is not positive")
    if abs(path["length_m"] - measured) > 1e-4:
        errors.append("length %s != independent %s" % (path["length_m"], measured))
    if speed > 0 and abs(path["duration_s"] - measured / speed) > 1e-4:
        errors.append("duration %s != L/speed %s" % (path["duration_s"], measured / speed))
    if factor > 0:
        dist = xz_distance(zone_center, start, end)
        if dist > factor * zone_radius + 1e-3:
            errors.append("zone distance %s > %s" % (dist, factor * zone_radius))
    return errors


def realistic_center(rng):
    lo = RADIUS
    hi = WORLD - RADIUS
    return (rng.uniform(lo, hi), 0.0, rng.uniform(lo, hi))


class _FixedRaw:
    """Every draw returns the same raw value. Used to pin the coin."""

    def __init__(self, raw):
        self.raw = raw

    def next_raw(self):
        return self.raw

    def next01(self):
        return struct.unpack("f", struct.pack("f", self.raw / float(fp.MODULUS)))[0]


class _AlwaysMiss:
    """Even raw draws and a lateral coordinate on the near edge, so the line misses a central zone."""

    def next_raw(self):
        return 2

    def next01(self):
        return 0.0


def run():
    failures = []
    sampled = 0
    for seed in range(1, SEEDS + 1):
        rng = random.Random(seed)
        center = realistic_center(rng)
        path = fp.from_seed(seed, WORLD, center, RADIUS, SPEED, MIN_ALT, MAX_ALT, MARGIN, FACTOR)
        errors = audit(path, center, RADIUS, FACTOR, MIN_ALT, MAX_ALT, WORLD, MARGIN, SPEED)
        if path["used_fallback"]:
            if path["fallback_reason"] != "attempts_exhausted":
                errors.append("realistic seed used %s" % path["fallback_reason"])
        else:
            if path["fallback_reason"] != "":
                errors.append("sampled path has reason %s" % path["fallback_reason"])
            sampled += 1
        if errors:
            failures.append("seed %s: %s" % (seed, "; ".join(errors)))
            if len(failures) >= 8:
                break
    if sampled < 100:
        failures.append("only %s of %s seeds were sampled; the reserve is not the generator" % (sampled, SEEDS))

    for factor in (0.5, 2.0):
        factor_sampled = 0
        for seed in range(1, 201):
            rng = random.Random(5000 + seed)
            center = realistic_center(rng)
            path = fp.from_seed(seed, WORLD, center, RADIUS, SPEED, MIN_ALT, MAX_ALT, MARGIN, factor)
            errors = audit(path, center, RADIUS, factor, MIN_ALT, MAX_ALT, WORLD, MARGIN, SPEED)
            if path["used_fallback"]:
                if path["fallback_reason"] != "attempts_exhausted":
                    errors.append(path["fallback_reason"])
            else:
                factor_sampled += 1
            if errors:
                failures.append("factor %s seed %s: %s" % (factor, seed, "; ".join(errors)))
                break
        if factor_sampled == 0:
            failures.append("factor %s never sampled a line" % factor)

    outside = fp.from_seed(1, WORLD, (-1000.0, 0.0, 8000.0), RADIUS, SPEED, MIN_ALT, MAX_ALT, MARGIN, FACTOR)
    if outside["fallback_reason"] != "incompatible":
        failures.append("centre outside the map reason %s" % outside.get("fallback_reason"))

    zero_radius = fp.from_seed(2, WORLD, (7000.0, 0.0, 7000.0), 0.0, SPEED, MIN_ALT, MAX_ALT, MARGIN, FACTOR)
    if zero_radius["fallback_reason"] != "incompatible":
        failures.append("radius 0 reason %s" % zero_radius.get("fallback_reason"))

    wide_margin = fp.from_seed(3, 5000.0, (2500.0, 0.0, 2500.0), RADIUS, SPEED, MIN_ALT, MAX_ALT, 3000.0, FACTOR)
    if wide_margin["fallback_reason"] != "incompatible":
        failures.append("margin wider than the world reason %s" % wide_margin.get("fallback_reason"))

    # Every opposite-edge segment is at least 500 m from x=100, and the reserve is too.
    tight = fp.from_seed(4, WORLD, (100.0, 0.0, 7680.0), RADIUS, SPEED, MIN_ALT, MAX_ALT, MARGIN, 0.1)
    if tight["fallback_reason"] != "incompatible":
        failures.append("reserve past the distance limit reason %s" % tight.get("fallback_reason"))
    tight_dist = xz_distance((100.0, 0.0, 7680.0), tight["start"], tight["end"])
    if tight_dist <= 300.0:
        failures.append("incompatible reserve unexpectedly within 300 m (%s)" % tight_dist)

    missed = fp.from_random(_AlwaysMiss(), WORLD, (7680.0, 0.0, 7680.0), RADIUS, SPEED, MIN_ALT, MAX_ALT, MARGIN, FACTOR)
    if missed["fallback_reason"] != "attempts_exhausted":
        failures.append("exhausted attempts reason %s" % missed.get("fallback_reason"))
    else:
        miss_errors = audit(missed, (7680.0, 0.0, 7680.0), RADIUS, FACTOR, MIN_ALT, MAX_ALT, WORLD, MARGIN, SPEED)
        if miss_errors:
            failures.append("valid reserve: %s" % "; ".join(miss_errors))

    unrestricted_far = 0
    east_west = 0
    north_south = 0
    for seed in range(1, SEEDS + 1):
        rng = random.Random(10000 + seed)
        center = realistic_center(rng)
        path = fp.from_seed(seed, WORLD, center, RADIUS, SPEED, MIN_ALT, MAX_ALT, MARGIN, 0.0)
        if path["used_fallback"]:
            failures.append("factor 0 used the reserve at seed %s" % seed)
            break
        dist = xz_distance(center, path["start"], path["end"])
        if dist > RADIUS:
            unrestricted_far += 1
        errors = audit(path, center, RADIUS, 0.0, MIN_ALT, MAX_ALT, WORLD, MARGIN, SPEED)
        if errors:
            failures.append("factor 0 seed %s: %s" % (seed, "; ".join(errors)))
            break
        axis = axis_of(path["start"], path["end"], WORLD, MARGIN)
        if axis == "ew":
            east_west += 1
        elif axis == "ns":
            north_south += 1
    if unrestricted_far == 0:
        failures.append("factor 0 never produced a line farther than R1 from the centre")
    # Seeds 1..1000 must not collapse onto one axis. 35%..65% is the band.
    if east_west < 350 or north_south < 350:
        failures.append("axis split E-W %s N-S %s" % (east_west, north_south))

    stuck = fp.BRFlightRandom(2147483647)
    if stuck.state == 2147483647:
        failures.append("seed 2147483647 stayed on the modulus")
    first = stuck.next_raw()
    second = stuck.next_raw()
    if first == second or first == 2147483647:
        failures.append("seed 2147483647 did not advance (%s, %s)" % (first, second))

    # Low bit of 1073741824 is 0, so the axis is east-west. A >= 0.5 test on this
    # raw (float64 or binary32) would pick north-south. See flight_path.py.
    witness = fp.from_random(_FixedRaw(1073741824), WORLD, (7680.0, 0.0, 7680.0), RADIUS, SPEED, MIN_ALT, MAX_ALT, MARGIN, 0.0)
    if axis_of(witness["start"], witness["end"], WORLD, MARGIN) != "ew":
        failures.append("binary32 threshold witness picked %s" % axis_of(witness["start"], witness["end"], WORLD, MARGIN))
    ratio = 1073741824 / float(fp.MODULUS)
    ratio_f32 = struct.unpack("f", struct.pack("f", ratio))[0]
    if not (ratio >= 0.5 and ratio_f32 >= 0.5 and (1073741824 & 1) == 0):
        failures.append("threshold witness no longer separates the low bit from >= 0.5")

    return failures


def self_test():
    same_edge = {
        "start": (MARGIN, MIN_ALT, 4000.0),
        "end": (MARGIN, MAX_ALT, 9000.0),
        "length_m": length_3d((MARGIN, MIN_ALT, 4000.0), (MARGIN, MAX_ALT, 9000.0)),
        "duration_s": length_3d((MARGIN, MIN_ALT, 4000.0), (MARGIN, MAX_ALT, 9000.0)) / SPEED,
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

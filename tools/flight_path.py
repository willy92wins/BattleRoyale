"""Python mirror of BattleRoyaleFlightPath.

Each function names the Enforce method it copies. The recurrence is Park-Miller
via Schrage. Seeds outside 1..2147483646 become 1, including 2147483647, which
is a fixed point of the recurrence (product 0 mod m, then the <= 0 branch adds
m back).

The axis and the end swap use the low bit of the raw draw (Coin), not
raw/modulus >= 0.5. 1073741824/2147483647 is above 0.5 in float64 and in
binary32, but its low bit is 0. Both languages follow the bit, so they pick
the same axis. Next01 still returns the binary32 rounding of raw/modulus for
the lateral coordinates and the altitudes.

  BRFlightRandom.__init__ / Coin / next01    BattleRoyaleFlightPath.c BRFlightRandom
  from_random                                BattleRoyaleFlightPath.FromRandom
  _fallback                                  BattleRoyaleFlightPath.SetFallback
  _set_length                                BattleRoyaleFlightPath.SetLength
  distance_xz_to_segment                     BattleRoyaleFlightPath.DistanceXZToSegment

fallback_reason is "" when the line was sampled, "attempts_exhausted" when the
reserve still meets the margin, altitudes and distance limit, and
"incompatible" when the request cannot be met.

No match state calls the Enforce class. This module is the offline stand-in.
"""

import math
import struct

SAMPLE_ATTEMPTS = 32  # BattleRoyaleFlightPath.SAMPLE_ATTEMPTS
MODULUS = 2147483647


def _f32(value):
    return struct.unpack("f", struct.pack("f", float(value)))[0]


class BRFlightRandom:
    def __init__(self, seed):
        # BattleRoyaleFlightPath.c BRFlightRandom — 2147483647 would stick.
        if seed <= 0 or seed >= MODULUS:
            seed = 1
        self.state = seed

    def next_raw(self):
        hi = self.state // 127773
        lo = self.state % 127773
        nxt = 16807 * lo - 2836 * hi
        if nxt <= 0:
            nxt = nxt + MODULUS
        self.state = nxt
        return nxt

    def next01(self):
        # Binary32, matching Enforce float.
        return _f32(self.next_raw() / float(MODULUS))

    def coin(self):
        # Low bit. See the module note on 1073741824.
        return self.next_raw() & 1


def distance_xz_to_segment(point, start, finish):
    abx = finish[0] - start[0]
    abz = finish[2] - start[2]
    ab2 = abx * abx + abz * abz
    t = 0.0
    if ab2 > 0:
        t = ((point[0] - start[0]) * abx + (point[2] - start[2]) * abz) / ab2
        if t < 0:
            t = 0.0
        if t > 1:
            t = 1.0
    dx = start[0] + abx * t - point[0]
    dz = start[2] + abz * t - point[2]
    return math.sqrt(dx * dx + dz * dz)


def _length(start, end):
    dx = end[0] - start[0]
    dy = end[1] - start[1]
    dz = end[2] - start[2]
    return math.sqrt(dx * dx + dy * dy + dz * dz)


def _set_length(path, speed_mps):
    path["length_m"] = _length(path["start"], path["end"])
    if speed_mps > 0:
        path["duration_s"] = path["length_m"] / speed_mps
    else:
        path["duration_s"] = 0.0
    return path


def _fallback(world_size, zone_center, min_alt_m, edge_margin_m, speed_mps, reason):
    inset = edge_margin_m
    if inset < 0:
        inset = 0.0
    if world_size <= 0:
        world_size = inset * 2 + 1
    if inset * 2 >= world_size:
        inset = world_size * 0.25
    cross = zone_center[2]
    if cross < inset:
        cross = inset
    if cross > world_size - inset:
        cross = world_size - inset
    path = {
        "start": (inset, min_alt_m, cross),
        "end": (world_size - inset, min_alt_m, cross),
        "used_fallback": True,
        "fallback_reason": reason,
    }
    return _set_length(path, speed_mps)


def from_seed(seed, world_size, zone_center, zone_radius, speed_mps, min_alt_m, max_alt_m, edge_margin_m, max_zone_distance_factor):
    return from_random(
        BRFlightRandom(seed),
        world_size,
        zone_center,
        zone_radius,
        speed_mps,
        min_alt_m,
        max_alt_m,
        edge_margin_m,
        max_zone_distance_factor,
    )


def from_random(random, world_size, zone_center, zone_radius, speed_mps, min_alt_m, max_alt_m, edge_margin_m, max_zone_distance_factor):
    if random is None or world_size <= 0 or edge_margin_m < 0 or edge_margin_m * 2 >= world_size:
        return _fallback(world_size, zone_center, min_alt_m, edge_margin_m, speed_mps, "incompatible")

    impossible = zone_radius <= 0 and max_zone_distance_factor > 0
    if zone_center[0] < 0 or zone_center[2] < 0 or zone_center[0] > world_size or zone_center[2] > world_size:
        impossible = True
    if impossible:
        return _fallback(world_size, zone_center, min_alt_m, edge_margin_m, speed_mps, "incompatible")

    limit = max_zone_distance_factor * zone_radius
    check_distance = max_zone_distance_factor > 0
    near_edge = edge_margin_m
    far_edge = world_size - edge_margin_m
    span = far_edge - near_edge
    attempt = 0
    while attempt < SAMPLE_ATTEMPTS:
        attempt += 1
        axis = random.next_raw() & 1
        flipped = random.next_raw() & 1
        start_along = near_edge
        end_along = far_edge
        if flipped == 1:
            start_along = far_edge
            end_along = near_edge
        cross_start = near_edge + random.next01() * span
        cross_end = near_edge + random.next01() * span
        alt_start = min_alt_m + random.next01() * (max_alt_m - min_alt_m)
        alt_end = min_alt_m + random.next01() * (max_alt_m - min_alt_m)
        if axis == 0:
            start = (start_along, alt_start, cross_start)
            finish = (end_along, alt_end, cross_end)
        else:
            start = (cross_start, alt_start, start_along)
            finish = (cross_end, alt_end, end_along)
        if check_distance and distance_xz_to_segment(zone_center, start, finish) > limit:
            continue
        path = {
            "start": start,
            "end": finish,
            "used_fallback": False,
            "fallback_reason": "",
        }
        return _set_length(path, speed_mps)

    path = _fallback(world_size, zone_center, min_alt_m, edge_margin_m, speed_mps, "attempts_exhausted")
    if check_distance and distance_xz_to_segment(zone_center, path["start"], path["end"]) > limit:
        path["fallback_reason"] = "incompatible"
    return path

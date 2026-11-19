"""Python mirror of BattleRoyaleFlightPath.

Each function names the Enforce lines it copies. The recurrence is Park-Miller
via Schrage so a seed produces the same draws a 32-bit signed divide would
for this modulus (every intermediate product fits in a signed 32-bit int).

  BRFlightRandom.BRFlightRandom     BattleRoyaleFlightPath.c:11-18
  BRFlightRandom.NextRaw            BattleRoyaleFlightPath.c:20-29
  BRFlightRandom.Next01             BattleRoyaleFlightPath.c:32-35
  FromRandom sampling loop          BattleRoyaleFlightPath.c:64-138
  SetFallback                       BattleRoyaleFlightPath.c:141-161
  SetLength                         BattleRoyaleFlightPath.c:163-170
  DistanceXZToSegment               BattleRoyaleFlightPath.c:172-189

No match state calls the Enforce class. This module is the offline stand-in.
"""

import math

SAMPLE_ATTEMPTS = 32  # BattleRoyaleFlightPath.c:40
MODULUS = 2147483647


class BRFlightRandom:
    def __init__(self, seed):
        # BattleRoyaleFlightPath.c:15 — non-positive seeds become 1. 2147483647 is not
        # rejected in Enforce (that comparison trips the int-boundary lint).
        if seed <= 0:
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
        return self.next_raw() / float(MODULUS)


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


def _fallback(world_size, zone_center, min_alt_m, edge_margin_m, speed_mps):
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
        return _fallback(world_size, zone_center, min_alt_m, edge_margin_m, speed_mps)

    impossible = zone_radius <= 0 and max_zone_distance_factor > 0
    if zone_center[0] < 0 or zone_center[2] < 0 or zone_center[0] > world_size or zone_center[2] > world_size:
        impossible = True

    limit = max_zone_distance_factor * zone_radius
    check_distance = max_zone_distance_factor > 0
    near_edge = edge_margin_m
    far_edge = world_size - edge_margin_m
    span = far_edge - near_edge
    attempt = 0
    while not impossible and attempt < SAMPLE_ATTEMPTS:
        attempt += 1
        axis = 0
        if random.next01() >= 0.5:
            axis = 1
        flipped = 0
        if random.next01() >= 0.5:
            flipped = 1
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
        path = {"start": start, "end": finish, "used_fallback": False}
        return _set_length(path, speed_mps)

    return _fallback(world_size, zone_center, min_alt_m, edge_margin_m, speed_mps)

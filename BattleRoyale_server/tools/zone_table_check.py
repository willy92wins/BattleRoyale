"""Offline check of the BattleRoyale zone tables for Chernarus (design aid, nothing runs in game).

It replays the zone generator of scripts_server/5_Mission/BattleRoyale/Server/BattleRoyaleZone.c
(GetBattleRoyalePlayAreas, GetValidPositionNewCircle, GetRandomPOI, IsSafeZoneCenter) many times and
reports, for the tables and the end_avoid_city list it is given:
  - how often the generator would give up (the code then calls RequestExit, the server stops at boot);
  - how much of the first (largest) zone falls on the sea;
  - how many towns can host spawns inside the first zone;
  - the length of a full match from the timers.
With --per-poi it also lists the POIs that make generation fail or put sea in the first zone.

Sea model: the coastline below is drawn by hand from vanilla data (southernmost and easternmost
loot buildings of mpmissions/dayzOffline.chernarusplus/mapgrouppos.xml, the coastal fresh spawn
points and the Marine names of the world config). It is approximate (about 200 m) and counts the
Skalisty and Storozh (prison) islands as sea. Ponds and lakes are ignored.

The tables come from a zone_settings.json (--zone-json, enough on its own: the file the mod writes
in the profile folder has every field) or, without it, from the BattleRoyaleZoneData.c defaults of the
repository (--zone-data).

Usage:
  python zone_table_check.py --world-config "P:\\DZ\\worlds\\chernarusplus\\world\\config.cpp" --zone-json "<profiles>\\Vigrid-BattleRoyale\\zone_settings.json" --num-zones 7
  python zone_table_check.py --world-config ... --zone-data "<repo>\\BattleRoyale\\addons\\scripts_server\\3_Game\\Config\\BattleRoyaleZoneData.c"
  python zone_table_check.py --world-config ... --zone-json ... --per-poi 200
Requires numpy and Pillow.
"""
import argparse
import json
import math
import os
import random
import re

import numpy as np
from PIL import Image, ImageDraw

WORLD = 15360
CELL = 30.0
N_CELLS = int(WORLD / CELL)
MIN_PCT, MAX_PCT = 0.25, 0.75  # DAYZBR_ZS_MIN/MAX_DISTANCE_PERCENT (scripts_client 2_GameLib)

COAST = [
    (0, 1900), (750, 1750), (1500, 1950), (2000, 2050), (2500, 1950), (3000, 2250), (3700, 2100),
    (4300, 2150), (4700, 2250), (5250, 2000), (5750, 1950), (6250, 1800), (6750, 2150), (7250, 2350),
    (7600, 2550), (8000, 2650), (8250, 2400), (8750, 2100), (9250, 1750), (9750, 1700), (10250, 1500),
    (10750, 1950), (11250, 2400), (11750, 3100), (12250, 3350), (12750, 3650), (13250, 3700),
    (13600, 3850), (13700, 4250), (13450, 4500), (13550, 5250), (13550, 5750), (13650, 6250),
    (13500, 6750), (13300, 7250), (13100, 7750), (13150, 8250), (12900, 8600), (12900, 8750),
    (13050, 9250), (13250, 9750), (13300, 10250), (13250, 10750), (14000, 11250), (13700, 11750),
    (13700, 12250), (14450, 12750), (14450, 13250), (15360, 13600),
]
TOWN_PAD = {"Capital": 500.0, "City": 300.0, "Village": 150.0}  # 4_BattleRoyalePrepare.c GetRandomVillage
EXTRA_SPAWN_RADIUS = 250.0  # BattleRoyaleSpawnsData.extra_spawn_radius


class GenFail(Exception):
    pass


def build_sea():
    img = Image.new("L", (N_CELLS, N_CELLS), 0)
    pts = [(x / CELL, (WORLD - z) / CELL) for x, z in COAST + [(WORLD, 0), (0, 0)]]
    ImageDraw.Draw(img).polygon(pts, fill=1)
    return np.array(img, dtype=np.uint8)


SEA = build_sea()
GX = (np.arange(N_CELLS) + 0.5) * CELL
GZ = WORLD - (np.arange(N_CELLS) + 0.5) * CELL


def is_sea(x, z):
    if x < 0 or z < 0 or x >= WORLD or z >= WORLD:
        return False
    return SEA[int((WORLD - z) / CELL), int(x / CELL)] == 1


def sea_fraction(cx, cz, r):
    x0, x1 = max(0, int((cx - r) / CELL)), min(N_CELLS, int((cx + r) / CELL) + 1)
    r0, r1 = max(0, int((WORLD - (cz + r)) / CELL)), min(N_CELLS, int((WORLD - (cz - r)) / CELL) + 1)
    inside = (GX[x0:x1][None, :] - cx) ** 2 + (GZ[r0:r1][:, None] - cz) ** 2 < r * r
    n = inside.sum()
    return float(SEA[r0:r1, x0:x1][inside].sum()) / float(n) if n else 0.0


def is_safe_zone_center(x, z):
    if is_sea(x, z):
        return False
    edge = int(WORLD * 0.85)
    if x > edge or z > edge:
        return random.random() < 0.5
    return True


def new_circle(cx, cz, r_old, r_new):
    """GetValidPositionNewCircle: two valid candidates toward the map centre; the code keeps the farther one."""
    max_distance = r_new - r_old
    potential = None
    max_try = 500
    for _ in range(20000):
        max_try -= 1
        dist = random.uniform(MIN_PCT * max_distance, MAX_PCT * max_distance)
        move = math.atan2((WORLD // 2) - cx, (WORLD // 2) - cz) + math.radians(random.uniform(-45, 45))
        nx, nz = cx + dist * math.sin(move), cz + dist * math.cos(move)
        if nx < r_new or nz < r_new or nx + r_new > WORLD or nz + r_new > WORLD:
            if max_try <= 0:
                if potential is not None:
                    return potential
                raise GenFail("exit")  # the game calls RequestExit(0) here
            continue
        if not is_safe_zone_center(nx, nz):
            continue
        if potential is None:
            potential = (nx, nz)
            continue
        # BattleRoyaleZone.c:474-482 keeps potentialpos when it is farther, otherwise the new one
        if math.hypot(potential[0] - cx, potential[1] - cz) > math.hypot(nx - cx, nz - cz):
            return potential
        return nx, nz
    raise GenFail("hang")  # the game loops forever here


def random_poi(pois):
    while True:
        px, pz = random.choice(pois)
        rr, th = 10 * math.sqrt(random.random()), random.random() * 2 * math.pi
        x, z = px + rr * math.cos(th), pz + rr * math.sin(th)
        if is_safe_zone_center(x, z):
            return x, z


def generate(sizes, start):
    areas = [(start[0], start[1], sizes[0])]
    for r in sizes[1:]:
        nx, nz = new_circle(areas[-1][0], areas[-1][1], areas[-1][2], r)
        areas.append((nx, nz, r))
    return areas


def load_names(path):
    text = open(path, encoding="utf-8", errors="replace").read()
    start = text.index("class Names") + len("class Names")
    end = text.find("class ChernarusPlusIntro", start)
    block = text[start:end if end != -1 else len(text)]
    heads = list(re.finditer(r"class\s+(\w+)\s*\{", block))
    names = []
    for k, m in enumerate(heads):
        body = block[m.end(): heads[k + 1].start() if k + 1 < len(heads) else len(block)]
        pos = re.search(r"position\[\]\s*=\s*\{\s*([-\d.]+)\s*,\s*([-\d.]+)\s*\}", body)
        typ = re.search(r'type\s*=\s*"([^"]*)"', body)
        if pos and typ:
            names.append((m.group(1), typ.group(1), float(pos.group(1)), float(pos.group(2))))
    return names


def read_c_defaults(path):
    text = open(path, encoding="ascii").read()
    def array(name):
        m = re.search(r"\b%s\s*=\s*\{(.*?)\};" % name, text, re.S)
        body = re.sub(r"//[^\n]*", "", m.group(1))
        return [t.strip().strip('"') for t in body.split(",") if t.strip()]
    return {
        "static_sizes": [float(v) for v in array("static_sizes")],
        "static_timers": [int(v) for v in array("static_timers")],
        "end_avoid_city": array("end_avoid_city"),
        "end_avoid_type": array("end_avoid_type"),
    }


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--world-config", required=True, help="DZ\\worlds\\chernarusplus\\world\\config.cpp (unpacked game data)")
    ap.add_argument("--zone-json", help="zone_settings.json to check (enough on its own)")
    ap.add_argument("--zone-data", help="BattleRoyaleZoneData.c to read the defaults from, and the fields a partial --zone-json lacks (default: the repository copy next to this package, when present)")
    ap.add_argument("--num-zones", type=int, help="num_zones from general_settings.json (default: length of static_sizes)")
    ap.add_argument("--first-zone-delay", type=int, default=150, help="(60 * round_duration_minutes) / 2, seconds")
    ap.add_argument("--runs", type=int, default=8000)
    ap.add_argument("--per-poi", type=int, default=0, help="generations per POI for the POI table (0 = skip)")
    ap.add_argument("--seed", type=int, default=4242)
    args = ap.parse_args()

    fields = ("static_sizes", "static_timers", "end_avoid_city", "end_avoid_type")
    zone_data = args.zone_data or os.path.join(here, "..", "..", "BattleRoyale", "addons", "scripts_server", "3_Game", "Config", "BattleRoyaleZoneData.c")
    if args.zone_json:
        loaded = json.load(open(args.zone_json, encoding="utf-8"))
        cfg = {k: loaded[k] for k in fields if k in loaded}
        missing = [k for k in fields if k not in cfg]
        if missing:
            if not os.path.isfile(zone_data):
                ap.error("%s has no %s; add them, or pass --zone-data with BattleRoyaleZoneData.c" % (args.zone_json, ", ".join(missing)))
            defaults = read_c_defaults(zone_data)
            cfg.update({k: defaults[k] for k in missing})
            print("fields not in %s, taken from %s: %s" % (args.zone_json, zone_data, ", ".join(missing)))
    else:
        if not os.path.isfile(zone_data):
            ap.error("%s not found; pass --zone-json with a zone_settings.json, or --zone-data with BattleRoyaleZoneData.c" % zone_data)
        cfg = read_c_defaults(zone_data)
    cfg["static_sizes"] = [float(v) for v in cfg["static_sizes"]]
    cfg["static_timers"] = [int(v) for v in cfg["static_timers"]]
    n = args.num_zones or len(cfg["static_sizes"])
    sizes, timers = cfg["static_sizes"][:n], cfg["static_timers"][:n]

    names = load_names(args.world_config)
    pois = [(c, t, x, z) for c, t, x, z in names if t not in cfg["end_avoid_type"] and c not in cfg["end_avoid_city"]]
    towns = [(t, x, z) for c, t, x, z in names if t in TOWN_PAD]

    random.seed(args.seed)
    fails = {"exit": 0, "hang": 0}
    seas, spawn_towns = [], []
    for _ in range(args.runs):
        try:
            areas = generate(sizes, random_poi([(x, z) for _, _, x, z in pois]))
        except GenFail as e:
            fails[str(e)] += 1
            continue
        cx, cz, r = areas[-1]
        seas.append(sea_fraction(cx, cz, r))
        spawn_towns.append(sum(1 for t, x, z in towns if math.hypot(x - cx, z - cz) < r + TOWN_PAD[t] + EXTRA_SPAWN_RADIUS))
    seas = np.array(seas)

    match = args.first_zone_delay + sum(timers) + timers[0] / 2.0
    print("zones %d, radii %s, timers %s" % (n, sizes, timers))
    print("final-zone POIs %d of %d named locations" % (len(pois), len(names)))
    print("generations %d: give-up (RequestExit) %d, endless loop %d" % (args.runs, fails["exit"], fails["hang"]))
    if len(seas):
        print("first zone over the sea: any %.1f%%, >5%% of its area %.2f%%, >10%% %.2f%%, mean %.2f%%" % (100 * (seas > 0.005).mean(), 100 * (seas > 0.05).mean(), 100 * (seas > 0.10).mean(), 100 * seas.mean()))
        print("towns that can host spawns in the first zone: p10 %d, median %d" % (np.percentile(spawn_towns, 10), np.median(spawn_towns)))
    print("full match: %d s (%.1f min) from the start match state to the end of the last zone, plus up to ~50 s of damage" % (match, match / 60.0))

    if args.per_poi:
        print("\nPOIs that make generation fail (>= 5%) or put >5% sea in the first zone in >= 25% of runs:")
        for c, t, x, z in pois:
            ex, sea_hits, ok = 0, 0, 0
            for _ in range(args.per_poi):
                rr, th = 10 * math.sqrt(random.random()), random.random() * 2 * math.pi
                try:
                    areas = generate(sizes, (x + rr * math.cos(th), z + rr * math.sin(th)))
                except GenFail:
                    ex += 1
                    continue
                ok += 1
                sea_hits += sea_fraction(*areas[-1]) > 0.05
            if ex >= 0.05 * args.per_poi or (ok and sea_hits >= 0.25 * ok):
                print("  %-36s %-15s x=%6.0f z=%6.0f fail=%.2f sea>5%%=%.2f" % (c, t, x, z, ex / args.per_poi, sea_hits / ok if ok else 1.0))


if __name__ == "__main__":
    main()

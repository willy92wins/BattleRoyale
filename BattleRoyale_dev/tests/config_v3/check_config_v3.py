"""Mirror of BattleRoyaleGameData migration v1->v2->v3 and Validate().

Enforce source of truth (do not drift):
  version default and new keys
    BattleRoyale/addons/scripts_server/3_Game/Config/BattleRoyaleGameData.c:4
    BattleRoyaleGameData.c:44-52
  Upgrade v1 then v2, two separate ifs (not else-if)
    BattleRoyaleGameData.c:123-151
  Validate ranges, including one log when min altitude >= max
    BattleRoyaleGameData.c:196-247
  Load order: Load() calls Upgrade() before the mission file is applied;
  Validate() runs only after LoadMission().
    BattleRoyaleGameData.c:91
    BattleRoyale/addons/scripts_server/3_Game/Config/BattleRoyaleConfig.c:109
    BattleRoyaleConfig.c:129
    BattleRoyaleConfig.c:134
  Absent JSON keys keep the class defaults (JsonFileLoader). A mission file
  overwrites only the keys it contains, then Validate() repairs them.

Usage (from the repo root or from this directory):
    python -I BattleRoyale_dev/tests/config_v3/check_config_v3.py
    python -I BattleRoyale_dev/tests/config_v3/check_config_v3.py --self-test

Exit 0 when every fixture matches its expected JSON. Exit 1 otherwise.
--self-test mutates the speed ceiling and checks that the mirror then disagrees
with a known-good expected file. Exit 0 means the disagreement was detected.
"""

import copy
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
FIXTURES = os.path.join(HERE, "fixtures")
EXPECTED = os.path.join(HERE, "expected")

# BattleRoyaleGameData.c field defaults. Key order follows the class.
DEFAULTS = {
    "version": 3,
    "admins_steamid64": ["123456789123456789"],
    "num_zones": 7,
    "round_duration_minutes": 5,
    "enable_spawn_selection_menu": True,
    "spawn_selection_duration": 30,
    "spawn_selection_extra_time": 2,
    "spawn_selection_radius": 50,
    "show_spawn_heatmap": True,
    "time_until_teleport_unlock": 10,
    "enable_unstuck": True,
    "enable_spectator": True,
    "spectator_delay_seconds": 5,
    "enable_waiting_room": True,
    "zone_notification_minutes": [1, 2],
    "zone_notification_seconds": [30, 10],
    "debug_heal_tick_seconds": 5,
    "zone_damage_tick_seconds": 5,
    "zone_damage_delta": 0.1,
    "enable_zone_damage": True,
    "hide_players_endgame": False,
    "show_first_zone_at_start": True,
    "artillery_sound": True,
    "enable_plane_deployment": True,
    "plane_speed_mps": 100,
    "plane_min_altitude_m": 800,
    "plane_max_altitude_m": 1100,
    "plane_edge_margin_m": 600,
    "plane_path_max_zone_distance": 1.0,
    "parachute_auto_open_agl_m": 150,
    "first_zone_reveal_delay_seconds": 60,
    "first_zone_extra_seconds": 180,
    "airdrop_enabled": True,
    "airdrop_ignore_last_zones": 3,
    "player_starting_clothes": ["TrackSuitJacket_Red", "TrackSuitPants_Red", "JoggingShoes_Red"],
    "player_starting_items": ["HuntingKnife", "BandageDressing", "Compass", "Battery9V", "Battery9V"],
    "player_starting_items_shortcut": [0],
}

# Inclusive bounds. None means the field is not range-checked (bools, arrays).
RULES = {
    "plane_speed_mps": (40, 200),
    "plane_min_altitude_m": (400, 2000),
    "plane_max_altitude_m": (400, 2500),
    "plane_edge_margin_m": (100, 3000),
    "plane_path_max_zone_distance": (0, 3),
    "parachute_auto_open_agl_m": (30, 500),
    "first_zone_reveal_delay_seconds": (0, 600),
    "first_zone_extra_seconds": (0, 1800),
    "spectator_delay_seconds": (0, 10),
}


def fresh_defaults():
    return copy.deepcopy(DEFAULTS)


def load_object(raw):
    """JsonFileLoader into a new BattleRoyaleGameData: missing keys stay default."""
    data = fresh_defaults()
    if not isinstance(raw, dict):
        raise TypeError("config root must be an object")
    for key, value in raw.items():
        data[key] = copy.deepcopy(value)
    return data


def upgrade(data, logs):
    """BattleRoyaleGameData.Upgrade. Two separate steps so v1 reaches v3."""
    if data.get("version") == 1:
        shortcut = data.get("player_starting_items_shortcut")
        if shortcut is None:
            shortcut = []
            data["player_starting_items_shortcut"] = shortcut
        if len(shortcut) == 0:
            shortcut.append(0)
        data["version"] = 2
        logs.append("save-v2")
    if data.get("version") == 2:
        data["version"] = 3
        logs.append("save-v3")
    return data


def _reject(data, logs, field, rule):
    logs.append("invalid %s = %s (%s)" % (field, data.get(field), rule))
    data[field] = copy.deepcopy(DEFAULTS[field])


def validate(data, logs, rules=None):
    """BattleRoyaleGameData.Validate plus the legacy checks the class already had."""
    rules = rules if rules is not None else RULES
    if data["spectator_delay_seconds"] < rules["spectator_delay_seconds"][0] or data["spectator_delay_seconds"] > rules["spectator_delay_seconds"][1]:
        _reject(data, logs, "spectator_delay_seconds", "must be between 0 and 10")

    if data["time_until_teleport_unlock"] < 0:
        _reject(data, logs, "time_until_teleport_unlock", "must not be negative")

    half_round = (60 * data["round_duration_minutes"]) // 2
    if data["round_duration_minutes"] < 1 or half_round <= data["time_until_teleport_unlock"]:
        logs.append(
            "invalid round_duration_minutes / time_until_teleport_unlock = %s / %s"
            % (data["round_duration_minutes"], data["time_until_teleport_unlock"])
        )
        data["round_duration_minutes"] = DEFAULTS["round_duration_minutes"]
        data["time_until_teleport_unlock"] = DEFAULTS["time_until_teleport_unlock"]

    if data["spawn_selection_duration"] < 1:
        _reject(data, logs, "spawn_selection_duration", "must be positive")
    if data["spawn_selection_extra_time"] < 0:
        _reject(data, logs, "spawn_selection_extra_time", "must not be negative")
    if data["spawn_selection_radius"] <= 0:
        _reject(data, logs, "spawn_selection_radius", "must be positive")
    if data["debug_heal_tick_seconds"] < 1:
        _reject(data, logs, "debug_heal_tick_seconds", "must be positive")
    if data["zone_damage_tick_seconds"] < 1:
        _reject(data, logs, "zone_damage_tick_seconds", "must be positive")

    lo, hi = rules["plane_speed_mps"]
    if data["plane_speed_mps"] < lo or data["plane_speed_mps"] > hi:
        _reject(data, logs, "plane_speed_mps", "must be between 40 and 200")

    lo, hi = rules["plane_min_altitude_m"]
    if data["plane_min_altitude_m"] < lo or data["plane_min_altitude_m"] > hi:
        _reject(data, logs, "plane_min_altitude_m", "must be between 400 and 2000")

    lo, hi = rules["plane_max_altitude_m"]
    if data["plane_max_altitude_m"] < lo or data["plane_max_altitude_m"] > hi:
        _reject(data, logs, "plane_max_altitude_m", "must be between 400 and 2500")

    if data["plane_min_altitude_m"] >= data["plane_max_altitude_m"]:
        logs.append(
            "invalid plane_min_altitude_m / plane_max_altitude_m = %s / %s"
            % (data["plane_min_altitude_m"], data["plane_max_altitude_m"])
        )
        data["plane_min_altitude_m"] = DEFAULTS["plane_min_altitude_m"]
        data["plane_max_altitude_m"] = DEFAULTS["plane_max_altitude_m"]

    lo, hi = rules["plane_edge_margin_m"]
    if data["plane_edge_margin_m"] < lo or data["plane_edge_margin_m"] > hi:
        _reject(data, logs, "plane_edge_margin_m", "must be between 100 and 3000")

    lo, hi = rules["plane_path_max_zone_distance"]
    if data["plane_path_max_zone_distance"] < lo or data["plane_path_max_zone_distance"] > hi:
        _reject(data, logs, "plane_path_max_zone_distance", "must be between 0 and 3")

    lo, hi = rules["parachute_auto_open_agl_m"]
    auto_open = data["parachute_auto_open_agl_m"]
    if auto_open < lo or auto_open > hi or auto_open >= data["plane_min_altitude_m"] - 100:
        _reject(data, logs, "parachute_auto_open_agl_m", "must be between 30 and 500 and lower than plane_min_altitude_m - 100")

    lo, hi = rules["first_zone_reveal_delay_seconds"]
    if data["first_zone_reveal_delay_seconds"] < lo or data["first_zone_reveal_delay_seconds"] > hi:
        _reject(data, logs, "first_zone_reveal_delay_seconds", "must be between 0 and 600")

    lo, hi = rules["first_zone_extra_seconds"]
    if data["first_zone_extra_seconds"] < lo or data["first_zone_extra_seconds"] > hi:
        _reject(data, logs, "first_zone_extra_seconds", "must be between 0 and 1800")

    return data


def apply_mission(data, mission):
    """LoadMission: JsonFileLoader over the object already upgraded from the profile."""
    if mission is None:
        return data
    for key, value in mission.items():
        data[key] = copy.deepcopy(value)
    return data


def migrate(profile, mission=None, rules=None):
    logs = []
    data = load_object(profile)
    upgrade(data, logs)
    apply_mission(data, mission)
    validate(data, logs, rules)
    return data, logs


def _read(path):
    with open(path, "r", encoding="utf-8") as handle:
        return json.load(handle)


def _same(left, right):
    if isinstance(left, dict) and isinstance(right, dict):
        if set(left) != set(right):
            return False
        return all(_same(left[key], right[key]) for key in left)
    if isinstance(left, list) and isinstance(right, list):
        return len(left) == len(right) and all(_same(a, b) for a, b in zip(left, right))
    if isinstance(left, (int, float)) and isinstance(right, (int, float)) and not isinstance(left, bool) and not isinstance(right, bool):
        return float(left) == float(right)
    return left == right


CASES = [
    {"name": "v1_full", "profile": "v1_full.json", "expected": "expected_v1_full.json", "altitude_pair_logs": 0},
    {"name": "v2_missing_keys", "profile": "v2_missing_keys.json", "expected": "expected_v2_missing_keys.json"},
    {"name": "v2_custom_arrays", "profile": "v2_custom_arrays.json", "expected": "expected_v2_custom_arrays.json"},
    {"name": "v2_spawn_menu_off", "profile": "v2_spawn_menu_off.json", "expected": "expected_v2_spawn_menu_off.json"},
    {
        "name": "v2_mission_override",
        "profile": "v2_mission_profile.json",
        "mission": "v2_mission_override.json",
        "expected": "expected_v2_mission_override.json",
    },
    {"name": "invalid_speed", "profile": "invalid_speed.json", "expected": "expected_invalid_speed.json"},
    {
        "name": "invalid_altitudes",
        "profile": "invalid_altitudes.json",
        "expected": "expected_invalid_altitudes.json",
        "altitude_pair_logs": 1,
    },
    {"name": "invalid_auto_open", "profile": "invalid_auto_open.json", "expected": "expected_invalid_auto_open.json"},
    {"name": "invalid_reveal", "profile": "invalid_reveal.json", "expected": "expected_invalid_reveal.json"},
    {"name": "invalid_extra", "profile": "invalid_extra.json", "expected": "expected_invalid_extra.json"},
]


def run_cases(rules=None):
    failures = []
    for case in CASES:
        profile = _read(os.path.join(FIXTURES, case["profile"]))
        mission = None
        if "mission" in case:
            mission = _read(os.path.join(FIXTURES, case["mission"]))
        got, logs = migrate(profile, mission, rules)
        expected = _read(os.path.join(EXPECTED, case["expected"]))
        if not _same(got, expected):
            failures.append("%s: JSON mismatch\n got=%s\n expected=%s" % (case["name"], json.dumps(got, sort_keys=True), json.dumps(expected, sort_keys=True)))
        pair_logs = [line for line in logs if line.startswith("invalid plane_min_altitude_m / plane_max_altitude_m")]
        if "altitude_pair_logs" in case and len(pair_logs) != case["altitude_pair_logs"]:
            failures.append("%s: altitude pair log count %s, want %s (%s)" % (case["name"], len(pair_logs), case["altitude_pair_logs"], logs))
        # A single inverted pair must not also have been range-rejected before the pair log.
        if case["name"] == "invalid_altitudes":
            ranged = [line for line in logs if "must be between 400 and" in line]
            if ranged:
                failures.append("invalid_altitudes: range log beside the single pair log: %s" % logs)
    return failures


def self_test():
    """Break the speed ceiling. Default 100 must then be rejected and the expected file must not match."""
    broken = copy.deepcopy(RULES)
    broken["plane_speed_mps"] = (40, 50)
    # Replacement value stays the class default (100), which is now also out of range.
    # Force the mirror's replacement down so a silent no-op cannot hide the break.
    saved = DEFAULTS["plane_speed_mps"]
    DEFAULTS["plane_speed_mps"] = 40
    try:
        failures = run_cases(broken)
    finally:
        DEFAULTS["plane_speed_mps"] = saved
    if not failures:
        print("self-test: mutated speed rule still matched every expected file")
        return 1
    print("self-test: mutated rule detected (%s failing case(s))" % len(failures))
    return 0


def main(argv):
    if "--self-test" in argv:
        return self_test()
    failures = run_cases()
    if failures:
        print("FAIL %s" % len(failures))
        for item in failures:
            print(item)
        return 1
    print("OK %s fixtures" % len(CASES))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

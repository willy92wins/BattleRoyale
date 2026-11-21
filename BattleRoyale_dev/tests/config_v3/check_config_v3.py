"""Mirror of BattleRoyaleGameData migration v1->v2->v3 and Validate().

Enforce source of truth (do not drift):
  version default and new keys
    BattleRoyale/addons/scripts_server/3_Game/Config/BattleRoyaleGameData.c
  Upgrade v1 then v2, two separate ifs (not else-if), each Save()
    BattleRoyaleGameData.Upgrade
  LoadMission clamps version back to 3 after a mission file overwrites it
    BattleRoyaleGameData.LoadMission
  Validate: inverted altitudes (min >= max), including a side outside its
  own range, are one pair log and both defaults. Per-field range logs run
  only when min < max. Auto-open uses >= min - 100.
    BattleRoyaleGameData.Validate
  Each rejection is prefixed profile: or mission: from SettingFile, which
  reads the mission file text (ReadMissionText) so the serialized object
  gains no extra field. This mirror uses the mission object's keys the same way.
  Load order: Load() calls Upgrade() before the mission file is applied;
  Validate() runs only after LoadMission().
    BattleRoyaleConfig.c Load / LoadMission / Validate

Float fields are rounded to binary32 on load. Enforce stores them as float,
so 200.000001 becomes 200 and stays inside 40..200. Comparisons then use
those rounded values. Integers stay exact.

The saved document is the object after Upgrade and before the mission
override and before Validate. The effective document is after both.

Usage (from the repo root or from this directory):
    python -I BattleRoyale_dev/tests/config_v3/check_config_v3.py
    python -I BattleRoyale_dev/tests/config_v3/check_config_v3.py --self-test

Exit 0 when every fixture matches its expected JSON. Exit 1 otherwise.
--self-test changes only the speed ceiling to 175 (still a legal ceiling,
default 100 unchanged) and requires the fixture at 180 to disagree.
"""

import copy
import json
import os
import struct
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

FLOAT_FIELDS = {
    "spawn_selection_radius",
    "zone_damage_delta",
    "plane_speed_mps",
    "plane_min_altitude_m",
    "plane_max_altitude_m",
    "plane_edge_margin_m",
    "plane_path_max_zone_distance",
    "parachute_auto_open_agl_m",
}

PROFILE_FILE = "general_settings.json"
MISSION_FILE = "general_settings.json"


def f32(value):
    """Round a JSON number the way Enforce float (binary32) stores it."""
    return struct.unpack("f", struct.pack("f", float(value)))[0]


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
        if key in FLOAT_FIELDS and isinstance(value, (int, float)) and not isinstance(value, bool):
            value = f32(value)
        data[key] = copy.deepcopy(value)
    return data


def setting_file(mission, field):
    """Every rejection names general_settings.json, as LogInvalidSetting is called in Validate."""
    return PROFILE_FILE


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


def _reject(data, logs, mission, field, rule):
    logs.append("invalid %s %s = %s (%s)" % (setting_file(mission, field), field, data.get(field), rule))
    data[field] = copy.deepcopy(DEFAULTS[field])


def validate(data, logs, rules=None, mission=None):
    """BattleRoyaleGameData.Validate plus the legacy checks the class already had."""
    rules = rules if rules is not None else RULES
    if data["spectator_delay_seconds"] < rules["spectator_delay_seconds"][0] or data["spectator_delay_seconds"] > rules["spectator_delay_seconds"][1]:
        _reject(data, logs, mission, "spectator_delay_seconds", "must be between 0 and 10")

    if data["time_until_teleport_unlock"] < 0:
        _reject(data, logs, mission, "time_until_teleport_unlock", "must not be negative")

    half_round = (60 * data["round_duration_minutes"]) // 2
    if data["round_duration_minutes"] < 1 or half_round <= data["time_until_teleport_unlock"]:
        logs.append(
            "invalid %s round_duration_minutes / time_until_teleport_unlock = %s / %s"
            % (setting_file(mission, "round_duration_minutes"), data["round_duration_minutes"], data["time_until_teleport_unlock"])
        )
        data["round_duration_minutes"] = DEFAULTS["round_duration_minutes"]
        data["time_until_teleport_unlock"] = DEFAULTS["time_until_teleport_unlock"]

    if data["spawn_selection_duration"] < 1:
        _reject(data, logs, mission, "spawn_selection_duration", "must be positive")
    if data["spawn_selection_extra_time"] < 0:
        _reject(data, logs, mission, "spawn_selection_extra_time", "must not be negative")
    if data["spawn_selection_radius"] <= 0:
        _reject(data, logs, mission, "spawn_selection_radius", "must be positive")
    if data["debug_heal_tick_seconds"] < 1:
        _reject(data, logs, mission, "debug_heal_tick_seconds", "must be positive")
    if data["zone_damage_tick_seconds"] < 1:
        _reject(data, logs, mission, "zone_damage_tick_seconds", "must be positive")

    lo, hi = rules["plane_speed_mps"]
    if data["plane_speed_mps"] < lo or data["plane_speed_mps"] > hi:
        _reject(data, logs, mission, "plane_speed_mps", "must be between 40 and 200")

    # Pair first, so 3000/1000 does not become 800/1000 with only a min-range log.
    if data["plane_min_altitude_m"] >= data["plane_max_altitude_m"]:
        pair_file = setting_file(mission, "plane_min_altitude_m")
        if mission and "plane_max_altitude_m" in mission:
            pair_file = setting_file(mission, "plane_max_altitude_m")
        logs.append(
            "invalid %s plane_min_altitude_m / plane_max_altitude_m = %s / %s (plane_min_altitude_m must be lower than plane_max_altitude_m)"
            % (pair_file, data["plane_min_altitude_m"], data["plane_max_altitude_m"])
        )
        data["plane_min_altitude_m"] = DEFAULTS["plane_min_altitude_m"]
        data["plane_max_altitude_m"] = DEFAULTS["plane_max_altitude_m"]
    else:
        lo, hi = rules["plane_min_altitude_m"]
        if data["plane_min_altitude_m"] < lo or data["plane_min_altitude_m"] > hi:
            _reject(data, logs, mission, "plane_min_altitude_m", "must be between 400 and 2000")

        lo, hi = rules["plane_max_altitude_m"]
        if data["plane_max_altitude_m"] < lo or data["plane_max_altitude_m"] > hi:
            _reject(data, logs, mission, "plane_max_altitude_m", "must be between 400 and 2500")
    # Mirrors the second pair check after per-field defaults.
    if data["plane_min_altitude_m"] >= data["plane_max_altitude_m"]:
        logs.append(
            "invalid %s plane_min_altitude_m / plane_max_altitude_m = %s / %s (plane_min_altitude_m must be lower than plane_max_altitude_m)"
            % (setting_file(mission, "plane_min_altitude_m"), data["plane_min_altitude_m"], data["plane_max_altitude_m"])
        )
        data["plane_min_altitude_m"] = DEFAULTS["plane_min_altitude_m"]
        data["plane_max_altitude_m"] = DEFAULTS["plane_max_altitude_m"]

    lo, hi = rules["plane_edge_margin_m"]
    if data["plane_edge_margin_m"] < lo or data["plane_edge_margin_m"] > hi:
        _reject(data, logs, mission, "plane_edge_margin_m", "must be between 100 and 3000")

    lo, hi = rules["plane_path_max_zone_distance"]
    if data["plane_path_max_zone_distance"] < lo or data["plane_path_max_zone_distance"] > hi:
        _reject(data, logs, mission, "plane_path_max_zone_distance", "must be between 0 and 3")

    lo, hi = rules["parachute_auto_open_agl_m"]
    auto_open = data["parachute_auto_open_agl_m"]
    if auto_open < lo or auto_open > hi or auto_open >= data["plane_min_altitude_m"] - 100:
        _reject(data, logs, mission, "parachute_auto_open_agl_m", "must be between 30 and 500 and lower than plane_min_altitude_m - 100")

    lo, hi = rules["first_zone_reveal_delay_seconds"]
    if data["first_zone_reveal_delay_seconds"] < lo or data["first_zone_reveal_delay_seconds"] > hi:
        _reject(data, logs, mission, "first_zone_reveal_delay_seconds", "must be between 0 and 600")

    lo, hi = rules["first_zone_extra_seconds"]
    if data["first_zone_extra_seconds"] < lo or data["first_zone_extra_seconds"] > hi:
        _reject(data, logs, mission, "first_zone_extra_seconds", "must be between 0 and 1800")

    return data


def apply_mission(data, mission):
    """LoadMission: JsonFileLoader over the object already upgraded from the profile."""
    if mission is None:
        return data
    for key, value in mission.items():
        if key in FLOAT_FIELDS and isinstance(value, (int, float)) and not isinstance(value, bool):
            value = f32(value)
        data[key] = copy.deepcopy(value)
    return data


def migrate(profile, mission=None, rules=None):
    logs = []
    data = load_object(profile)
    upgrade(data, logs)
    saved = copy.deepcopy(data)
    apply_mission(data, mission)
    # BattleRoyaleGameData.LoadMission clamps an older mission version.
    if int(data.get("version", 0)) < 3:
        data["version"] = 3
    validate(data, logs, rules, mission)
    return saved, data, logs


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
        # Both sides go through binary32, matching Enforce float and the load rounding above.
        return f32(left) == f32(right)
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
    {
        "name": "equal_altitudes",
        "profile": "equal_altitudes.json",
        "expected": "expected_equal_altitudes.json",
        "altitude_pair_logs": 1,
    },
    {
        "name": "inverted_out_of_range",
        "profile": "inverted_out_of_range.json",
        "expected": "expected_inverted_out_of_range.json",
        "altitude_pair_logs": 1,
    },
    {"name": "auto_open_equal", "profile": "auto_open_equal.json", "expected": "expected_auto_open_equal.json"},
    {
        "name": "mission_version_2",
        "profile": "v2_mission_profile.json",
        "mission": "mission_version_2.json",
        "expected": "expected_mission_version_2.json",
    },
    {"name": "speed_180", "profile": "speed_180.json", "expected": "expected_speed_180.json"},
    {"name": "speed_above_ceiling", "profile": "speed_above_ceiling.json", "expected": "expected_speed_above_ceiling.json"},
    {"name": "speed_float32_ceiling", "profile": "speed_float32_ceiling.json", "expected": "expected_speed_float32_ceiling.json"},
    {
        "name": "mission_min_below_range",
        "profile": "altitudes_400_700.json",
        "mission": "mission_min_300.json",
        "expected": "expected_mission_min_below_range.json",
        "altitude_pair_logs": 1,
    },
    {
        "name": "mission_speed_float32",
        "profile": "v2_mission_profile.json",
        "mission": "mission_speed_float32.json",
        "expected": "expected_mission_speed_float32.json",
    },
]

# Substrings that must appear in the rejection log for that case. Empty means no rejection.
REJECT_LOGS = {
    "v1_full": [],
    "v2_missing_keys": [],
    "v2_custom_arrays": [],
    "v2_spawn_menu_off": [],
    "v2_mission_override": ["invalid " + MISSION_FILE + " plane_speed_mps"],
    "invalid_speed": ["invalid " + PROFILE_FILE + " plane_speed_mps"],
    "invalid_altitudes": ["invalid " + PROFILE_FILE + " plane_min_altitude_m / plane_max_altitude_m"],
    "invalid_auto_open": ["invalid " + PROFILE_FILE + " parachute_auto_open_agl_m"],
    "invalid_reveal": ["invalid " + PROFILE_FILE + " first_zone_reveal_delay_seconds"],
    "invalid_extra": ["invalid " + PROFILE_FILE + " first_zone_extra_seconds"],
    "equal_altitudes": ["invalid " + PROFILE_FILE + " plane_min_altitude_m / plane_max_altitude_m"],
    "inverted_out_of_range": ["invalid " + PROFILE_FILE + " plane_min_altitude_m / plane_max_altitude_m"],
    "auto_open_equal": ["invalid " + PROFILE_FILE + " parachute_auto_open_agl_m"],
    "mission_version_2": ["invalid " + MISSION_FILE + " plane_speed_mps"],
    "speed_180": [],
    "speed_above_ceiling": ["invalid " + PROFILE_FILE + " plane_speed_mps"],
    "speed_float32_ceiling": [],
    "mission_min_below_range": ["invalid " + PROFILE_FILE + " plane_min_altitude_m = 300", "invalid " + PROFILE_FILE + " plane_min_altitude_m / plane_max_altitude_m"],
    "mission_speed_float32": [],
}


def run_cases(rules=None):
    failures = []
    for case in CASES:
        profile = _read(os.path.join(FIXTURES, case["profile"]))
        mission = None
        if "mission" in case:
            mission = _read(os.path.join(FIXTURES, case["mission"]))
        saved, got, logs = migrate(profile, mission, rules)
        expected = _read(os.path.join(EXPECTED, case["expected"]))
        if not _same(got, expected):
            failures.append("%s: effective JSON mismatch\n got=%s\n expected=%s" % (case["name"], json.dumps(got, sort_keys=True), json.dumps(expected, sort_keys=True)))
        if saved.get("version") != 3:
            failures.append("%s: saved version %s" % (case["name"], saved.get("version")))
        missing = [key for key in DEFAULTS if key not in saved]
        if missing:
            failures.append("%s: saved JSON missing %s" % (case["name"], missing))
        if int(profile.get("version", 3)) < 3 and "save-v3" not in logs:
            failures.append("%s: upgrade did not record save-v3 (%s)" % (case["name"], logs))
        if profile.get("version") == 1 and "save-v2" not in logs:
            failures.append("%s: upgrade did not record save-v2 (%s)" % (case["name"], logs))
        # The mission overlay must not be what was written for the profile.
        if case["name"] == "v2_mission_override" and saved.get("player_starting_items") == mission.get("player_starting_items"):
            failures.append("v2_mission_override: saved JSON already contains the mission items")
        pair_logs = [line for line in logs if "plane_min_altitude_m / plane_max_altitude_m" in line]
        if "altitude_pair_logs" in case and len(pair_logs) != case["altitude_pair_logs"]:
            failures.append("%s: altitude pair log count %s, want %s (%s)" % (case["name"], len(pair_logs), case["altitude_pair_logs"], logs))
        if case["name"] in ("invalid_altitudes", "equal_altitudes", "inverted_out_of_range"):
            ranged = [line for line in logs if "must be between 400 and" in line]
            if ranged:
                failures.append("%s: range log beside the pair log: %s" % (case["name"], logs))
        invalid_lines = [line for line in logs if line.startswith("invalid ")]
        wanted = REJECT_LOGS[case["name"]]
        if len(invalid_lines) != len(wanted):
            failures.append("%s: rejection count %s want %s (%s)" % (case["name"], len(invalid_lines), len(wanted), logs))
        for fragment in wanted:
            if not any(fragment in line for line in invalid_lines):
                failures.append("%s: missing rejection %s in %s" % (case["name"], fragment, logs))
        if case["name"] == "mission_version_2" and got.get("version") != 3:
            failures.append("mission_version_2: effective version %s" % got.get("version"))
        if case["name"] == "inverted_out_of_range":
            if got.get("plane_min_altitude_m") != 800 or got.get("plane_max_altitude_m") != 1100:
                failures.append("inverted_out_of_range: altitudes %s/%s" % (got.get("plane_min_altitude_m"), got.get("plane_max_altitude_m")))
        if case["name"] == "equal_altitudes":
            if got.get("plane_min_altitude_m") != 800 or got.get("plane_max_altitude_m") != 1100:
                failures.append("equal_altitudes: altitudes %s/%s" % (got.get("plane_min_altitude_m"), got.get("plane_max_altitude_m")))
        if case["name"] == "auto_open_equal" and got.get("parachute_auto_open_agl_m") != 150:
            failures.append("auto_open_equal: auto-open %s" % got.get("parachute_auto_open_agl_m"))
        if case["name"] == "speed_float32_ceiling" and got.get("plane_speed_mps") != 200:
            failures.append("speed_float32_ceiling: speed %s" % got.get("plane_speed_mps"))
    return failures


def self_test():
    """Move only the speed ceiling to 175. The default stays 100. Speed 180 must then fail."""
    broken = copy.deepcopy(RULES)
    broken["plane_speed_mps"] = (40, 175)
    failures = run_cases(broken)
    speed_failures = [item for item in failures if item.startswith("speed_180")]
    if not speed_failures:
        print("self-test: ceiling 175 still accepted speed 180")
        return 1
    print("self-test: ceiling 175 rejected speed 180 (%s failing case(s))" % len(failures))
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

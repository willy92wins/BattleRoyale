"""Checks the lint reports written by tools/lint.sh against the known false positives.

Every script PBO that lint.sh lints must have a report in the output folder, written in this run
(gate.sh empties the folder first), that scanned at least one script and holds no error outside
the allowlist below. The allowlist names each false positive by rule and file, with how many times
it may appear; fewer is fine, any other error fails.

Usage: python tools/gate_check.py <repo_or_worktree_root> <lint_output_folder>
"""
import collections
import json
import os
import sys

LAYOUT = "ES-LAYOUT-PATH-PBOPREFIX-MISMATCH"  # layouts live in the gui PBO, not in the script PBO
PARAM = "ES-OVERRIDE-PARAM-NAME-MISMATCH"  # RemovePlayer overrides BattleRoyaleState, not vanilla

ALLOWED = {
    "scripts_client": {
        (LAYOUT, "5_mission/cot/gui/brmastercontrolsmodule.c"): 1,
        (LAYOUT, "5_mission/gui/hud/battleroyalekillfeed.c"): 1,
        (LAYOUT, "5_mission/gui/mapmenu.c"): 1,
        (LAYOUT, "5_mission/gui/spawnselectionmenu.c"): 1,
        (LAYOUT, "5_mission/mission/mainmenu.c"): 2,
        (LAYOUT, "5_mission/mission/missiongameplay.c"): 3,
    },
    "scripts_server": {
        (PARAM, "5_mission/battleroyale/server/states/1_battleroyaledebug.c"): 1,
        (PARAM, "5_mission/battleroyale/server/states/4_battleroyaleprepare.c"): 1,
    },
    "vpp_compat": {
        (LAYOUT, "5_mission/menubattleroyalemanager.c"): 1,
    },
}


def expected_pbos(root):
    """The script PBOs lint.sh lints: same selection rule, plus the VPP compatibility addon."""
    addons = os.path.join(root, "BattleRoyale", "addons")
    names = []
    for name in sorted(os.listdir(addons)):
        folder = os.path.join(addons, name)
        if not os.path.isfile(os.path.join(folder, "config.cpp")):
            continue
        entries = os.listdir(folder)
        has_scripts = any(e.lower() == "scripts" or (e[:1].isdigit() and e[1:2] == "_") for e in entries)
        if has_scripts or name.startswith("scripts_"):
            names.append(name)
    names.append("vpp_compat")
    return names


def main():
    root, out = sys.argv[1], sys.argv[2]
    bad = []
    for pbo in expected_pbos(root):
        path = os.path.join(out, pbo + ".json")
        if not os.path.isfile(path):
            bad.append(f"{pbo}: no lint report")
            continue
        try:
            report = json.load(open(path, encoding="utf-8"))
        except ValueError:
            bad.append(f"{pbo}: unreadable lint report")
            continue
        if not isinstance(report.get("errors"), list) or report.get("info", {}).get("files_scanned", 0) < 1:
            bad.append(f"{pbo}: report without errors list or without scanned files")
            continue
        found = collections.Counter(
            (e.get("rule_id"), str(e.get("file", "")).replace("\\", "/").lower()) for e in report["errors"])
        allowed = ALLOWED.get(pbo, {})
        for key, count in sorted(found.items()):
            if count > allowed.get(key, 0):
                bad.append(f"{pbo}: {count} x {key[0]} in {key[1]} (allowed {allowed.get(key, 0)})")
    if bad:
        print("GATE RED:", "; ".join(bad))
        sys.exit(1)
    print("GATE GREEN")


if __name__ == "__main__":
    main()

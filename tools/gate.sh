#!/bin/bash
# Lint gate: run tools/lint.sh and fail when any script PBO has more errors than the known baseline.
# Baseline: scripts_client 10 (ES-LAYOUT-PATH-PBOPREFIX-MISMATCH false positives, layouts live in the
# gui PBO; the 1.0 killfeed layout is one of them. It was 11 until 1.1 moved the VPP menu, and its
# false positive, to vpp_compat), scripts_server 2 (ES-OVERRIDE-PARAM-NAME-MISMATCH on RemovePlayer,
# false positives), vpp_compat 1 (that same false positive: the VPP menu layout stays in the core gui
# PBO; any other error fails), every other script PBO 0. A missing or unreadable report for
# scripts_client, scripts_server or vpp_compat fails.
# Usage: tools/gate.sh <repo_or_worktree_root>
ROOT="${1:-.}"; OUT="$ROOT/build/lint"; mkdir -p "$OUT"
bash "$ROOT/tools/lint.sh" "$ROOT" "$OUT" > "$OUT.summary.txt" 2>&1
cat "$OUT.summary.txt"
python - "$OUT" <<'EOF'
import json, os, sys
out = sys.argv[1]
base = {"scripts_client": 10, "scripts_server": 2, "vpp_compat": 1}
vpp_menu_layout = "Vigrid-BattleRoyale/GUI/layouts/BattleRoyaleManagerUI/MenuBattleRoyaleManager.layout"
bad = []
for pbo in ("scripts_client", "scripts_server", "vpp_compat"):
    if not os.path.isfile(os.path.join(out, pbo + ".json")):
        bad.append(f"{pbo}: no lint report")
for name in sorted(os.listdir(out)):
    if not name.endswith(".json"):
        continue
    pbo = name[:-5]
    try:
        d = json.load(open(os.path.join(out, name), encoding="utf-8"))
    except ValueError:
        bad.append(f"{pbo}: unreadable lint report")
        continue
    errors = len(d.get("errors", []))
    allowed = base.get(pbo, 0)
    if errors > allowed:
        bad.append(f"{pbo}: {errors} errors > baseline {allowed}")
    rules = {e.get("rule_id") for e in d.get("errors", [])}
    if pbo == "scripts_client" and rules - {"ES-LAYOUT-PATH-PBOPREFIX-MISMATCH"}:
        bad.append(f"{pbo}: unexpected rules {sorted(rules - {'ES-LAYOUT-PATH-PBOPREFIX-MISMATCH'})}")
    if pbo == "scripts_server" and rules - {"ES-OVERRIDE-PARAM-NAME-MISMATCH"}:
        bad.append(f"{pbo}: unexpected rules {sorted(rules - {'ES-OVERRIDE-PARAM-NAME-MISMATCH'})}")
    if pbo == "vpp_compat":
        for e in d.get("errors", []):
            if e.get("rule_id") != "ES-LAYOUT-PATH-PBOPREFIX-MISMATCH" or vpp_menu_layout not in e.get("message", ""):
                bad.append(f"{pbo}: unexpected error {e.get('rule_id')} at {e.get('file')}:{e.get('line')}")
if bad:
    print("GATE RED:", "; ".join(bad))
    sys.exit(1)
print("GATE GREEN")
EOF

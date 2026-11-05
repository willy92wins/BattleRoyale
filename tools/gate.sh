#!/bin/bash
# Lint gate: run tools/lint.sh and fail when any script PBO has more errors than the known baseline.
# Baseline: scripts_client 10 (ES-LAYOUT-PATH-PBOPREFIX-MISMATCH false positives, layouts live in the
# gui PBO), scripts_server 2 (ES-OVERRIDE-PARAM-NAME-MISMATCH on RemovePlayer, false positives),
# every other script PBO 0.
# Usage: tools/gate.sh <repo_or_worktree_root>
ROOT="${1:-.}"; OUT="$ROOT/build/lint"; mkdir -p "$OUT"
bash "$ROOT/tools/lint.sh" "$ROOT" "$OUT" > "$OUT.summary.txt" 2>&1
cat "$OUT.summary.txt"
python - "$OUT" <<'EOF'
import json, os, sys
out = sys.argv[1]
base = {"scripts_client": 10, "scripts_server": 2}
bad = []
for name in sorted(os.listdir(out)):
    if not name.endswith(".json"):
        continue
    pbo = name[:-5]
    d = json.load(open(os.path.join(out, name), encoding="utf-8"))
    errors = len(d.get("errors", []))
    allowed = base.get(pbo, 0)
    if errors > allowed:
        bad.append(f"{pbo}: {errors} errors > baseline {allowed}")
    rules = {e.get("rule_id") for e in d.get("errors", [])}
    if pbo == "scripts_client" and rules - {"ES-LAYOUT-PATH-PBOPREFIX-MISMATCH"}:
        bad.append(f"{pbo}: unexpected rules {sorted(rules - {'ES-LAYOUT-PATH-PBOPREFIX-MISMATCH'})}")
    if pbo == "scripts_server" and rules - {"ES-OVERRIDE-PARAM-NAME-MISMATCH"}:
        bad.append(f"{pbo}: unexpected rules {sorted(rules - {'ES-OVERRIDE-PARAM-NAME-MISMATCH'})}")
if bad:
    print("GATE RED:", "; ".join(bad))
    sys.exit(1)
print("GATE GREEN")
EOF

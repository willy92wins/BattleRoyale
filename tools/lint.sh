#!/bin/bash
# Offline Enforce lint of the BR script PBOs against vanilla 1.30 and the dependency sources.
# Usage: tools/lint.sh <repo_or_worktree_root> <out_dir>
ROOT="${1:-.}"; OUT="${2:-build/lint}"; mkdir -p "$OUT"
V="C:/Users/guill/DayZ-Modding-Knowledge-Pack/tools/dayz-script-validator/scripts/script_validator.py"
VR="C:/Users/guill/OneDrive/Documentos/DayZ Projects/scripts"
REF="C:/Users/guill/Repos/br-ref"
X=(--external-scripts "$REF/expansion_scripts" --external-scripts "$REF/exp_core_scripts" --external-scripts "$REF/cf_scripts" --external-scripts "$REF/cot/scripts" --external-scripts "$REF/vpp/VPPAdminTools" --external-scripts "$REF/dabs_scripts" --external-scripts "$REF/navigation_scripts")
A="$ROOT/BattleRoyale/addons"
for pbo in $(ls "$A"); do
  [ -f "$A/$pbo/config.cpp" ] || continue
  ls "$A/$pbo" | grep -qi -E "^scripts$|^[0-9]_" || [ -d "$A/$pbo/Scripts" ] || [ -d "$A/$pbo/scripts" ] || [[ "$pbo" == scripts_* ]] || continue
  python "$V" "$A/$pbo" --vanilla-root "$VR" "${X[@]}" --external-scripts "$A/scripts_client" > "$OUT/$pbo.json" 2> "$OUT/$pbo.err"
  python -c "import json,sys;d=json.load(open(sys.argv[1]));print(sys.argv[2],d['status'],'errors',len(d['errors']),'warnings',len(d['warnings']))" "$OUT/$pbo.json" "$pbo"
done

#!/bin/bash
# Offline Enforce lint of the BR script PBOs and of the VPP compatibility addon against vanilla 1.30
# and the dependency sources.
# Usage: tools/lint.sh <repo_or_worktree_root> <out_dir>
ROOT="${1:-.}"; OUT="${2:-build/lint}"; mkdir -p "$OUT"
V="C:/Users/guill/DayZ-Modding-Knowledge-Pack/tools/dayz-script-validator/scripts/script_validator.py"
VR="C:/Users/guill/OneDrive/Documentos/DayZ Projects/scripts"
REF="C:/Users/guill/Repos/br-ref"
X=(--external-scripts "$REF/expansion_scripts" --external-scripts "$REF/exp_core_scripts" --external-scripts "$REF/cf_scripts" --external-scripts "$REF/cot/scripts" --external-scripts "$REF/vpp/VPPAdminTools" --external-scripts "$REF/dabs_scripts" --external-scripts "$REF/navigation_scripts")
A="$ROOT/BattleRoyale/addons"
lint() {
  python "$V" "$1" --vanilla-root "$VR" "${X[@]}" --external-scripts "$A/scripts_client" > "$OUT/$2.json" 2> "$OUT/$2.err"
  python -c "import json,sys;d=json.load(open(sys.argv[1]));print(sys.argv[2],d['status'],'errors',len(d['errors']),'warnings',len(d['warnings']))" "$OUT/$2.json" "$2"
}
for pbo in $(ls "$A"); do
  [ -f "$A/$pbo/config.cpp" ] || continue
  ls "$A/$pbo" | grep -qi -E "^scripts$|^[0-9]_" || [ -d "$A/$pbo/Scripts" ] || [ -d "$A/$pbo/scripts" ] || [[ "$pbo" == scripts_* ]] || continue
  lint "$A/$pbo" "$pbo"
done
# Separate mod that requires VPPAdminTools (already in X) and the client scripts.
lint "$ROOT/BattleRoyale_VPP/addons/vpp_compat" vpp_compat

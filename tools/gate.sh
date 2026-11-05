#!/bin/bash
# Lint gate: lint every script PBO into an empty folder, then check the reports with
# tools/gate_check.py, which allows only the known false positives, each by rule and file:
# ES-LAYOUT-PATH-PBOPREFIX-MISMATCH where scripts load layouts from the gui PBO (scripts_client and
# vpp_compat) and ES-OVERRIDE-PARAM-NAME-MISMATCH on the two RemovePlayer overrides of
# BattleRoyaleState (scripts_server). A missing, stale or empty report fails.
# Usage: tools/gate.sh <repo_or_worktree_root>
ROOT="${1:-.}"; OUT="$ROOT/build/lint"
rm -rf "$OUT"; mkdir -p "$OUT"
bash "$ROOT/tools/lint.sh" "$ROOT" "$OUT" > "$OUT.summary.txt" 2>&1
cat "$OUT.summary.txt"
python "$ROOT/tools/gate_check.py" "$ROOT" "$OUT"

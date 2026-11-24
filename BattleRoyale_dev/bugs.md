# BattleRoyale — bug ledger

Live list of known defects outside the scope of the work in progress. One entry per defect: date, source, scenario, evidence and state.

## BR-001 — The Prepare worker can still be running when a match ends and the turnover exits the server (open)

- **Found:** 2026-11-19, by Sol in its review of the VPP panel plan (`br-runs/vpp-panel-2026-11-18/sol_plan_review_r3.log`). Pre-existing: it does not depend on the panel.
- **Scenario:** with serial matches on, the Prepare worker is still running when its watchdog moves the match on to StartMatch (`States/4_BattleRoyalePrepare.c:104-113`). If every player then leaves, StartMatch completes by itself (`States/5_BattleRoyaleStartMatch.c:123-133`) and the match reaches Restart. If the worker is still alive when the turnover starts, the turnover takes the exit path (`BRMatchTurnover.c:88`, `BattleRoyaleServer.c:606`) and the server shuts down instead of opening the next lobby.
- **Not reproduced** in the game. A cheap reproduction would hold the worker past the watchdog (a lot of players, or an artificial `Sleep`) and disconnect everyone in StartMatch.
- **State:** open. Outside the scope of the panel (R25).

## BR-002 — The 1.2.0 compat's static overrides are never called (being fixed on `claude/project-thread-55vbxj`)

- **Found:** 2026-11-19 in DayZDiag, run `c197ce48`, with the core compiled before VPP: «Admin status RPC denied» for a superadmin on every status request.
- **Cause:** a `modded class` that overrides a `static` method does not change the calls the original class makes, because the core is already compiled. `modded class BRAdminAuthorizer` in compat 1.2.0 (commits `fc21dbf`, `747d9e4`) never takes effect.
- **Fix:** an instance hook (`BRAdminFrameworkHook`), which the compat overrides with a `modded class`.
- **Rule:** to change core behaviour from a later mod, use instance methods, not static overrides.
- **State:** being fixed. The 1.2.0 build without the panel must not be published (`br-deploy/@BattleRoyale-VPP/NO_SUBIR.txt`).

## BR-003 — Resuming LastRound in the second before its shrink: client circle up to ~9 m ahead of the server one (open, minor)

- **Found:** 2026-11-19, Sol review round 2 of the panel diff (`br-runs/vpp-panel-2026-11-18/pr2/sol_diff_r2.log`).
- **Scenario:** freeze LastRound after its shrink timer has fired, but up to 1 s before the state-clock start. On resume, the negative elapsed time is clamped to 0 (`States/7_BattleRoyaleLastRound.c:286,434`), so the client interpolates up to 1 s ahead until the lock: about 9 m at 9 m/s. Damage uses the server circle.
- **State:** open, low priority. The window is 1 s and the effect is cosmetic. Not reproduced.

## BR-004 — The panel shows the wrong round total with more than 255 zones (open, minor)

- **Found:** 2026-11-19, Sol review round 2. The status packs the total into 8 bits (`BattleRoyaleServer.c:2044`), so `num_zones=257` shows «Ronda 2 / 1».
- **State:** open. No realistic config comes close: the tables have about 7 zones.

## BR-005 — Client crash on quitting after a parachute landing (fixed in `9621215`, confirming)

- Census of client sessions since 2026-11-20: shutdown leak `HumanCommandScript_Parachute (-1x)` closed cleanly 7/7; `-2x`/`-3x` crashed 6/7 (access violation in shutdown, `crash_<stamp>.log`); no parachute 0/5. Also happened with plane deployment off.
- Cause (hypothesis): Admiral's `HandleParachute` starts a command while `IsParachuteOpen()`, a flag only the server clears; on the client it stays set for some frames after landing, so a second command started on the ground.
- Fix: modded `DayZPlayerImplementParachute.HandleParachute` returns false when the player has landed and the flag is still set (`BattleRoyaleParachute.c`). In game 2026-11-24: 2 landings + quit, both `-1x`, no crash file. Close after 1-2 more.

## BR-006 — Client parachute command keeps running after the server's landing (open)

- Round 2026-11-24 15:19 (run `088e4583`): server logged `Landing fall=5.13` at 15:19:39; the client kept logging `[Parachute] Glide` until 15:19:54 (when Guillermo left the session), the last 11 lines at `fwd=8 vy=-8`, Admiral's defaults when the chute is no longer worn. Client script log `DayZ_MCP_dev\_client\profiles\script_2026-11-24_15-17-13.log`.
- So the client's command did not finish on landing (Admiral finishes it in `PostPhysUpdate` on `PlayerHasLanded`). Unknown whether the player saw it (stuck or sliding). Next: ask Guillermo; check the client's leak count for this session; consider finishing the command when `!IsWearingChute()` in plane mode.

### Update 2026-11-24 21:35 (BR-005 and BR-006)

- BR-005 reopened: the client session of 15:17 (run `088e4583`) quit with `HumanCommandScript_Parachute (-2x)` and `crash_2026-11-24_15-17-13.log` (access violation at 15:34:08). With the guard: 2 clean, 1 crash.
- That is the same session where BR-006 showed (client command alive 15 s after the server's landing), so BR-006 is the likely remaining path to the `-2x` leak. Fix BR-006 first, then re-measure exits.

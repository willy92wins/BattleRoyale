# server-states

Lane `lane/server-states`. Static review plus `bash tools/lint.sh . build/lint`. Nothing was run in DayZ.

Decisions: spawn search is capped at 40 villages x 15 points (600 checks) and the random fallback at 40; zone square search at 64, POI search at 40, next-circle search keeps 500. Godmode is cleared from the existing `RemovePlayer` / `RemoveAllPlayers` when `b_IsDebug` is set, instead of a new override, so the lint baseline stays at two `RemovePlayer` false positives. `SetMatchPhase` for spawn selection is not sent here.

## Items

### SRV-10
`4_BattleRoyalePrepare.c:562` walks `a_PlayerList` with a null guard, not `GetPlayers()[i]` against a stale count. `ProcessPlayersWatchdog` (`:83`) calls `Deactivate` and `Error` if the thread has not set `m_PlayersProcessed` within 120 s (`AddTimer` at `:79`).
Verified by reading those lines. Not verified in the engine: that an out-of-range array access aborts a `GameScript.Call` thread.

### SRV-11 / ZON-04
`BattleRoyaleZone.c` `GetValidPositionSquare` stops after 64 tries and uses the square midpoint (`Error`). `GetValidPositionNewCircle` is `while (max_try > 0)` (500) and then uses `potentialpos` or the clamped center. `GetRandomPOI` returns the map center and `Error` when `s_POI` is empty, and after 40 misses uses the first POI. `GetRandomSpawnPosition` (`4_BattleRoyalePrepare.c`) stops the village search at 40 x 15 and the last-resort loop at 40, then keeps the last candidate and `Error`.
Verified by reading the loops. `RequestExit` is gone from `BattleRoyaleZone.c` (grep). Not verified: that 40 or 64 tries are enough on Chernarus.

### ZON-05
The `GetGame().RequestExit(0)` in `GetValidPositionNewCircle` is removed. The fallback center is range-checked against `[new_radius, world_size - new_radius]` and logged with `BattleRoyaleUtils.Warn` (`BattleRoyaleZone.c` around the `max_try <= 0` block).
Verified by reading the block and by grep. Not verified in a running server.

### ZON-09
`BattleRoyaleZone.c:219` uses `i >= a_StaticSizes.Count()`, then `Error` and `break` before `a_StaticSizes[i]`. If the index is still past the generated list, `GetBattleRoyalePlayAreas` returns an empty area and `Error`. Config validation on load is not this lane.
Verified by reading `:219`. Not verified against a bad `zone_settings.json`.

### SRV-19 / ZON-11
`6_BattleRoyaleRound.c` last-zone branch uses `GetPreviousZone()` only when non-null; otherwise `Error` and no `UpdateCurrentPlayArea`. `7_BattleRoyaleLastRound.c:81` same guard. Last-round damage uses `GetPreviousZone()` only when the zone and its area exist (`:158`). `GetPreviousZone` (`6_BattleRoyaleRound.c:351`) still returns null when the previous state is not a round.
Verified by reading the call sites. Not verified with `num_zones = 1`.

### SRV-12
`5_BattleRoyaleStartMatch.c` `HandleUnlock` and `Unstuck` `continue` / `return` when the player or `GetIdentity()` is null. `TeleportPlayer` in Prepare does the same.
Verified by reading the guards. Not verified: the moment `GetIdentity()` becomes null during logout. `DisableInput` on the client is not this lane.

### SRV-16 / SRV-17 / SRV-18
`1_BattleRoyaleDebug.c:148` divides `ready_count / (player_count * 1.0)`. `KickWinner` (`8_BattleRoyaleWin.c:122`) walks the player list backwards and disconnects a copied identity. `HandleWinner` (`:70`) returns on a null winner or identity, and skips a null `CreateObject` chicken.
Verified by reading the three sites. Not verified in game (vote threshold, party winners, failed spawn).

### Rendimiento #3
`IsSafeForTeleport` (`0_BattleRoyaleState.c:319`) uses static `s_BadTeleportSurfaces` and reused `m_TeleportExcluded` / `m_TeleportCollided`. The collided-object debug text is inside `#ifdef BR_TRACE_ENABLED`. Village search is 40 x 15, and `m_SafeVillagePoints` (`4_BattleRoyalePrepare.c:381`) reuses a validated point for later players.
Verified by reading the function. Not verified: server frame time. Points are not precomputed across lobby frames; they are cached the first time a village is accepted.

### Rendimiento #4
Each Prepare pass calls `Sleep(100)` every 8 players (`SleepPlayerBatch`, `:94`) and once more for a remainder (`FlushPlayerBatch`). Order of `a_PlayerList` is unchanged.
Verified by reading the five passes. Not verified: wall time with 120 players.

### Rendimiento #8
`BattleRoyaleDebugState.AddPlayer` (`0_BattleRoyaleState.c`) sets `SetAllowDamage(false)` (`Object.c:1196` in vanilla). `b_IsDebug` is set in the debug-state constructor (`:773`). `RemovePlayer` / `RemoveAllPlayers` set `SetAllowDamage(true)` only when `b_IsDebug`. `Heal` runs only from `VitalsNeedHeal` (`:164`), which compares health, blood and shock with `GetHealth` / `GetMaxHealth` (`object.c:994` and `:1008`). The lobby tick uses the same check.
A new `RemovePlayer` override was avoided: the linter treats every override as a vanilla `UID` mismatch and the gate allows only the two existing false positives.
Verified by reading the methods. Not verified: network cost of `Heal`.

### Rendimiento #9
`MessagePlayerUntranslated(..., "STR_BR_TAKING_DAMAGE")` is removed from `6_BattleRoyaleRound.c` and `7_BattleRoyaleLastRound.c`. Symptom and `DecreaseHealthCoef` stay. The client indicator is another lane.
Verified by grep: no remaining `STR_BR_TAKING_DAMAGE` under `scripts_server`. Not verified on a client.

### Rendimiento #10
The Carim 5 s `AddTimer` that called `OnPlayerCountChanged` is removed (`0_BattleRoyaleState` constructor). `GetGroupsCount` (`:393`) returns a cache until `m_Players.Count()` changes. `SetTopPosition` is still one unicast per player inside `UpdateTopPosition`. `BattleRoyaleServer.Update` still calls `IsComplete` every 0.1 s; that file is not this lane. The cache makes that poll cheap.
Verified by reading `GetGroupsCount` and the constructor. Not verified under `#ifdef Carim` in the compiler (this lint invocation does not define `Carim`).

### SRV-41
`scripts_server/4_World/Entities/ManBase/PlayerBase.c:58` returns before `source.IsPlayer()` when `source` is null. The unknown-source log also guards `GetIdentity()`. Override parameters are unchanged (`playerbase.c:1255` vanilla).
Verified by reading the override. Not verified in combat.

### RPC contract
`BroadcastMatchPhase` (`0_BattleRoyaleState.c:69`) sends `SetMatchPhase` as `Param2<int, int>` with `GetRPCManager().SendRPC(RPC_DAYZBR_NAMESPACE, ..., true)`.
- Lobby: `1_BattleRoyaleDebug.c:68` `LOBBY, -1`
- Prepare: `4_BattleRoyalePrepare.c:50` `PREPARE, -1`
- StartMatch: `5_BattleRoyaleStartMatch.c:50` `STARTING, i_TimeToUnlock`
- Round: `6_BattleRoyaleRound.c:109` `ROUND, i_RoundTimeInSeconds`, and again at `:405` when `LockNewZone` rearms the timer
- LastRound: `7_BattleRoyaleLastRound.c:90` `LAST_ROUND, i_RoundTimeInSeconds`
- Win: `8_BattleRoyaleWin.c:20` `FINISHED, -1`
- Restart: `9_BattleRoyaleRestart.c:9` `RESTARTING, 10`

`BroadcastKillFeed` (`0_BattleRoyaleState.c:726`) sends `KillFeedEvent` as `Param4<string, string, string, float>` from `OnPlayerKilled` (`:646`). Killer name is empty unless another player identity exists. Weapon is `GetDisplayName` (`object.c:476`) only for a weapon, melee weapon, grenade or mine. Distance is `vector.Distance` to that player, otherwise 0. StartMatch still returns before `super.OnPlayerKilled` when gameplay has not started, so those deaths are not announced.
Verified by reading the send sites. Not verified: client handlers, or that both PBOs see `EBRMatchPhase` (`scripts_client/2_GameLib/BattleRoyaleEnums.c:31`).

## Lint
```
extra_changefeedbackurl PASS errors 0 warnings 0
extra_defaultfullauto PASS errors 0 warnings 0
extra_limitunconscioustime PASS errors 0 warnings 0
extra_preventplayermodifiers PASS errors 0 warnings 0
extra_preventweaponraise PASS errors 0 warnings 0
extra_spawncarfull PASS errors 0 warnings 0
extra_spawnweaponchambered PASS errors 0 warnings 0
extra_spawnwithammoandmagazine PASS errors 0 warnings 0
extra_spawnwithbattery PASS errors 0 warnings 0
extra_vehicle3ppdefaultfix PASS errors 0 warnings 0
scripts_client FAIL errors 10 warnings 3
scripts_server FAIL errors 2 warnings 12
```
The two server errors are the known `RemovePlayer` / `UID` false positives (`1_BattleRoyaleDebug.c:169`, `4_BattleRoyalePrepare.c:787`). No new error.

## CROSS-LANE REQUESTS
- Spawn selection `Activate` must send `SetMatchPhase` (`EBRMatchPhase.SPAWN_SELECTION`). File `3_BattleRoyaleSpawnSelection.c` is not this lane.
- Client lane registers `SetMatchPhase` and `KillFeedEvent` and draws the zone-damage indicator that used to be `STR_BR_TAKING_DAMAGE`.
- `BattleRoyaleServer.c` still polls `IsComplete` every 0.1 s (that is what used to recompute groups). No edit requested there; the count is cached in this lane.

## LO QUE NO PUDE VERIFICAR
- No DayZ, DayZServer or DayZDiag process, and no dayz-mcp.
- Thread abort on array OOB, logout timing of `GetIdentity()`, vote math, zone generation on Chernarus, and RPC delivery.
- Lint does not define `Carim`, so the group-count cache was not type-checked against Carim.
- `GetHealth("", "Health")` matches `proto native float GetHealth(string zoneName, string healthType)` at `object.c:994`. Same for `GetMaxHealth` at `object.c:1008` and `SetAllowDamage` at `object.c:1196`.

Analysed: 892188a (origin/main)

# Performance optimizations — BattleRoyale for DayZ 1.30, 100 players

All impact figures are static-analysis estimates — nobody has profiled this mod at 100 players: **[ESTIMATE]** throughout.

| Rank | ID | Side | Title | Estimated impact | Risk | Size | TOUCHES_BUSY_FILES |
|---|---|---|---|---|---|---|---|
| 1 | OPT-01 | server | Kill the per-player `SetTopPosition` unicast storm on every count change | high — O(N) RPCs per join/kill; ~10k RPCs/match at N=100 | low | 3 files, ~25 lines | no |
| 2 | OPT-02 | server | O(N²) membership/anchor/identity scans in the per-player tick | high — ~150 scheduled ticks/s each scanning ~100-entry lists | med | 4 files, ~60 lines | no |
| 3 | OPT-03 | server | Prepare teleports re-validate the same spawn point 3–4× with physics casts | high — thousands of physics queries in the match-start spike | med | 2 files, ~50 lines | no |
| 4 | OPT-04 | both | Heat-map broadcast resends the full point array to all clients at 1 Hz | med-high — 100 vectors × 100 clients per dirty second for 30 s | med | 2 files, ~30 lines | no |
| 5 | OPT-05 | server | Spectator anchors teleport every second and identities are re-scanned per event | med — up to ~99 anchor moves/s plus O(N) scans per death/click | med | 1 file, ~50 lines | no |
| 6 | OPT-06 | server | Trace/log arguments are built (string allocs) even when the log level rejects them | med in Prepare/zone checks, low steady-state | low | 3–4 files, ~40 lines | no |
| 7 | OPT-07 | server | Hit/kill path allocates names and webhook JSON on every hit and kill | med-low — per-bullet-hit strings in firefights; JSON map per kill even with API off | low | 2 files, ~30 lines | no |
| 8 | OPT-08 | server | `EEOnCECreate` weapon extras re-read server config for every CE weapon | low-med — 3 config lookups + config-path string per weapon spawn | low | 1 file, ~20 lines | no |
| 9 | OPT-09 | config | Shipped economy is vanilla Chernarus; only `globals.xml` is BR-tuned | med **[ESTIMATE]** — unmeasured CE/entity load at 100 players in one zone | high (gameplay) | config only | no |
| 10 | OPT-10 | client | Spawn-menu heat map fills each square with one `DrawLine` per pixel row | high (client hitch) — ~10⁴–10⁵ DrawLine calls per redraw worst case | low | 1 file, ~15 lines | no |
| 11 | OPT-11 | client | Minimap moves the map and redraws both zone ovals nearly every frame while moving | med — up to 2×512 canvas segments/frame at 60 fps | low | 1 file, ~25 lines | **yes** |
| 12 | OPT-12 | client | Spectator camera does a by-name bone lookup every post-frame | low — one string lookup + bone transform per frame per spectator | low | 1 file, ~15 lines | no |

---

## OPT-01 — Remove the `SetTopPosition` unicast loop from `OnPlayerCountChanged`

**Evidence**

[0_BattleRoyaleState.c](/C:/Users/guill/Repos/br-wt/opt-analysis/BattleRoyale/addons/scripts_server/5_Mission/BattleRoyale/Server/States/0_BattleRoyaleState.c:273):

```enforce
void UpdateTopPosition( int position )
{
    for(int i = 0; i < GetPlayers().Count(); i++)
    {
        PlayerBase player = GetPlayers()[i];
        player.SetBRPosition( position );
        GetRPCManager().SendRPC( RPC_DAYZBR_NAMESPACE, "SetTopPosition", new Param1<int>( position ), true, player.GetIdentity() );
    }
}
```

Called from `OnPlayerCountChanged` on every `AddPlayer`/`RemovePlayer` in an active state ([0_BattleRoyaleState.c:245](/C:/Users/guill/Repos/br-wt/opt-analysis/BattleRoyale/addons/scripts_server/5_Mission/BattleRoyale/Server/States/0_BattleRoyaleState.c:245)), which already broadcasts the same count:

```enforce
GetRPCManager().SendRPC( RPC_DAYZBR_NAMESPACE, "SetPlayerCount", new Param2<int, int>( nb_players, nb_groups ), true);
```

The client stores the unicast value in `top_position` ([BattleRoyaleRPC.c:304](/C:/Users/guill/Repos/br-wt/opt-analysis/BattleRoyale/addons/scripts_client/3_Game/BattleRoyale/RPC/BattleRoyaleRPC.c:304)) and `BattleRoyaleClient.Update` copies it to `player.position_top` every frame; its only use is the death-screen text.

**Why it costs at 100 players**

Every join during the lobby and every kill/disconnect during a round costs 1 broadcast **plus N unicast RPCs, all carrying the same integer**. Lobby fill is ΣN ≈ 5,050 unicasts; a full match's kills add a similar descending sum. **[ESTIMATE]** ~10k small RPCs per match, concentrated exactly in join bursts and final-circle kill bursts, for a value every client already receives in `SetPlayerCount`.

**Fix**

Keep the server-side loop only for the cheap field write `player.SetBRPosition(position)` (needed for score webhooks), delete the per-player `SendRPC`, and in the client `SetPlayerCount` handler set `top_position = data.param1` alongside `nb_players`. Remove the now-dead `SetTopPosition` handler/registration only if nothing else references it.

**Vanilla API relied on**

None new — it reuses the existing `SetPlayerCount` RPC and handler. The `RPCManager.SendRPC` proto itself was not located in `P:\scripts\` during this pass, so its exact signature/guarantee flag is **[UNVERIFIED]**; the fix does not depend on it beyond existing usage.

**Invariants**

Same RPC semantics for `SetPlayerCount`; death screen shows the same number (now set atomically with the count instead of by a second RPC that could interleave); `GetBRPosition()` values for score webhooks unchanged.

**Verify**

Two-client DayZDiag match: kill one player, check the death screen still shows the correct position and the win/score webhook position matches; count `SetTopPosition` RPCs in a script-log trace before/after during a 10-bot lobby fill (expect 0 after).

## OPT-02 — Make the per-player tick O(1) instead of O(N) per player

**Evidence**

Server per-player entry point, [PlayerBase.c:44](/C:/Users/guill/Repos/br-wt/opt-analysis/BattleRoyale/addons/scripts_server/4_World/Entities/ManBase/PlayerBase.c:44) (scripts_server):

```enforce
override void OnScheduledTick(float deltaTime)
{
    super.OnScheduledTick(deltaTime);
    BattleRoyaleBase m_BR = GetBR();
    if(m_BR)
        m_BR.OnPlayerTick(this, deltaTime);
}
```

[BattleRoyaleServer.c:578](/C:/Users/guill/Repos/br-wt/opt-analysis/BattleRoyale/addons/scripts_server/5_Mission/BattleRoyale/Server/BattleRoyaleServer.c:578):

```enforce
if (m_Spectators && m_Spectators.IsAnchor(player))
    return;
...
if(GetCurrentState().ContainsPlayer(player))
```

`ContainsPlayer` is a linear `m_Players.Find(player)` ([0_BattleRoyaleState.c:235](/C:/Users/guill/Repos/br-wt/opt-analysis/BattleRoyale/addons/scripts_server/5_Mission/BattleRoyale/Server/States/0_BattleRoyaleState.c:235)); `IsAnchor` iterates the whole anchor map ([BattleRoyaleSpectators.c:29](/C:/Users/guill/Repos/br-wt/opt-analysis/BattleRoyale/addons/scripts_server/5_Mission/BattleRoyale/Server/BattleRoyaleSpectators.c:29)); `temp_disconnecting.Find(player)` is a third linear scan. In the lobby, `BattleRoyaleDebugState.OnPlayerTick` additionally calls `CanGoOutsideLobby` per tick, which does `identity.GetPlainId()` (string allocation) plus an admin-list `Find` ([0_BattleRoyaleState.c:1059](/C:/Users/guill/Repos/br-wt/opt-analysis/BattleRoyale/addons/scripts_server/5_Mission/BattleRoyale/Server/States/0_BattleRoyaleState.c:1059)), although the steamid is already cached at connect: `player.player_steamid = player.GetIdentity().GetPlainId();` ([BattleRoyaleServer.c:238](/C:/Users/guill/Repos/br-wt/opt-analysis/BattleRoyale/addons/scripts_server/5_Mission/BattleRoyale/Server/BattleRoyaleServer.c:238)).

Vanilla cadence: `SCHEDULER_PLAYERS_PER_TICK = 5` ([P:\scripts\5_mission\mission\missionserver.c:12](/P:/scripts/5_mission/mission/missionserver.c:12)), `TickScheduler` calls `currentPlayer.OnTick()` ([P:\scripts\5_mission\mission\missionserver.c:766](/P:/scripts/5_mission/mission/missionserver.c:766)), which calls `OnScheduledTick(deltaT)` ([P:\scripts\4_world\entities\manbase\playerbase.c:3129](/P:/scripts/4_world/entities/manbase/playerbase.c:3129)); the BR override runs after vanilla's `OnScheduledTick` body ([P:\scripts\4_world\entities\manbase\playerbase.c:3007](/P:/scripts/4_world/entities/manbase\playerbase.c:3007)).

**Why it costs at 100 players**

At 100 players the scheduler must emit ~100 BR ticks per 20 server frames (**[ESTIMATE]** ~150/s at 30 fps server). Each tick currently does: `GetBR()` mission cast, an anchor-map iteration (0 early, up to ~99 entries late-game when most of the lobby is spectating), a ~100-entry `Find`, a second `Find` on `temp_disconnecting`, and state dispatch. That is O(N) work per player, i.e. O(N²) per full tick cycle, in Enforce where call/iteration overhead dominates. **[ESTIMATE]** tens of thousands of list steps per second, every second of the match.

**Fix**

- Maintain a `bool br_in_current_state` on `PlayerBase`, set in `BattleRoyaleState.AddPlayer`/`RemovePlayer`/`RemoveAllPlayers`; `OnPlayerTick` tests the flag instead of `ContainsPlayer`. Keep `ContainsPlayer` for the cold paths that already use it (RPC validation, disconnect).
- Replace the anchor value-scan with a `set<PlayerBase>` of anchors maintained next to `m_Anchors` in `BattleRoyaleSpectators` (insert in `Start`, remove in `OnIdentityDisconnected`); `IsAnchor` becomes a set `Find`.
- In `CanGoOutsideLobby`, compare against cached `player.player_steamid` and cache the admin verdict per player (admins list is static config) instead of `GetPlainId()` per tick.
- Cache the current state reference in `BattleRoyaleServer.Update` once per frame instead of re-calling `GetCurrentState()` inside every player tick (the state can only change in `Update`).

**Vanilla API relied on**

No new engine API; only the existing scheduled-tick contract above.

**Invariants**

Identical tick semantics: same players ticked, same order, zone damage/heal/teleport-out-of-lobby behaviour unchanged; anchor bodies still excluded. The membership flag must be updated on *every* path that mutates `m_Players` — audit `AddPlayer`, `RemovePlayer`, `RemoveAllPlayers` and state transfer in `BattleRoyaleServer.Update`.

**Verify**

Script-log timing: add a temporary trace counter around `OnPlayerTick` in a bot/solo test to confirm tick counts per state are unchanged; in-game check that lobby out-of-bounds teleport, zone damage and spectator exclusion still trigger; run the existing solo-match test addon (per `BattleRoyale_dev/HANDOFF.md`) through all states with 0 script errors.

## OPT-03 — Validate each spawn candidate once in Prepare instead of 3–4 times

**Evidence**

One candidate is checked repeatedly on its way to a teleport:

- `IsAcceptedSpawn` → `IsSafeForTeleport` does a sphere cast **and** a box-geometry query ([0_BattleRoyaleState.c:391](/C:/Users/guill/Repos/br-wt/opt-analysis/BattleRoyale/addons/scripts_server/5_Mission/BattleRoyale/Server/States/0_BattleRoyaleState.c:391), [0_BattleRoyaleState.c:407](/C:/Users/guill/Repos/br-wt/opt-analysis/BattleRoyale/addons/scripts_server/5_Mission/BattleRoyale/Server/States/0_BattleRoyaleState.c:407)):

```enforce
bool m_Hit = DayZPhysics.SphereCastBullet( start, end, radius, collisionLayerMask, NULL, m_HitObject, m_HitPosition, m_HitNormal, m_HitFraction );
...
if( GetGame().IsBoxCollidingGeometry(box_position, "0 0 0", "2 10 2", ObjIntersectFire, ObjIntersectGeom, m_TeleportExcluded, m_TeleportCollided) )
```

- Cached village points are re-validated after being fetched: `if( village_pos == "0 0 0" || !IsAcceptedSpawn(village_pos) )` ([4_BattleRoyalePrepare.c:434](/C:/Users/guill/Repos/br-wt/opt-analysis/BattleRoyale/addons/scripts_server/5_Mission/BattleRoyale/Server/States/4_BattleRoyalePrepare.c:434)).
- `ResolveFinalSpawn` re-checks: `if (IsAcceptedSpawn(position)) return position;` ([4_BattleRoyalePrepare.c:633](/C:/Users/guill/Repos/br-wt/opt-analysis/BattleRoyale/addons/scripts_server/5_Mission/BattleRoyale/Server/States/4_BattleRoyalePrepare.c:633)), and `TeleportPlayer` checks again at entry ([4_BattleRoyalePrepare.c:717](/C:/Users/guill/Repos/br-wt/opt-analysis/BattleRoyale/addons/scripts_server/5_Mission/BattleRoyale/Server/States/4_BattleRoyalePrepare.c:717)).
- The search itself is bounded but wide: up to 40 village tries × 15 position tries (`spawn_checks < 600`, [4_BattleRoyalePrepare.c:372](/C:/Users/guill/Repos/br-wt/opt-analysis/BattleRoyale/addons/scripts_server/5_Mission/BattleRoyale/Server/States/4_BattleRoyalePrepare.c:372)), plus `GetRandomSafePosition` 50 tries and group teleports 50 tries per member.

Vanilla cost basis: `SphereCastBullet` is a physics proto ([P:\scripts\3_game\global\dayzphysics.c:213](/P:/scripts/3_game\global\dayzphysics.c:213)), `IsBoxCollidingGeometry` likewise ([P:\scripts\3_game\global\game.c:1366](/P:/scripts/3_game\global\game.c:1366)), `SurfaceY` ([P:\scripts\3_game\global\game.c:1176](/P:/scripts/3_game\global\game.c:1176)). `ProcessPlayers` runs on a script thread — `ScriptModule.Call` "creates new thread, so it's legal to use sleep/wait" ([P:\scripts\1_core\proto\enscript.c:139](/P:/scripts/1_core\proto\enscript.c:139)) — and already sleeps 100 ms per 8 players, so the cost shows up as match-start duration and main-thread physics contention, not as a hard freeze.

**Why it costs at 100 players**

100 players × (search attempts + 2–3 redundant re-validations) × 2 physics queries per validation. **[ESTIMATE]** low thousands of sphere/box casts compressed into the Prepare window, on top of item creation for 100 inventories in the same window; this is the single largest avoidable CPU spike in a match lifecycle.

**Fix**

Thread a "already validated" token through the pipeline: make `ResolveFinalSpawn`/`TeleportPlayer` trust a position produced by `IsAcceptedSpawn` in the same call chain (e.g. an out-flag or a `TeleportValidatedPlayer` entry point), keeping only the cheap bounds checks (`IsOnMap`, `IsInsideFirstZone` — pure arithmetic) at the final entry. Do not re-validate cached village points on reuse; they were validated when inserted into `m_SafeVillagePoints` (keep the existing stale-entry removal only if a teleport actually fails). Keep all attempt caps unchanged.

**Vanilla API relied on**

The same physics protos above — the fix is calling them fewer times, not replacing them.

**Invariants**

A position is only ever teleported to after passing the same `IsAcceptedSpawn` test exactly once; fallback order (village → random → reserve → last-resort) and the last-resort "ignore collision" branch are unchanged. Spawn *distribution* may shift slightly because cached points are no longer re-rolled on revalidation failure — positions still come from the same previously-validated set; no player-visible rule changes.

**Verify**

Instrument (temporarily, trace level) a counter of `IsSafeForTeleport` calls per `ProcessPlayers` run before/after in a solo test: expect a large drop with identical success/failure outcomes; run the solo-match test through Prepare and confirm all players land inside the first zone; spot-check 20 teleports in-game for no in-geometry spawns.

## OPT-04 — Send heat-map deltas instead of the full array every second

**Evidence**

Server, repeating 1 s timer while the spawn menu is open ([3_BattleRoyaleSpawnSelection.c:93](/C:/Users/guill/Repos/br-wt/opt-analysis/BattleRoyale/addons/scripts_server/5_Mission/BattleRoyale/Server/States/3_BattleRoyaleSpawnSelection.c:93)):

```enforce
GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(this.BroadcastHeatMap, 1000, true);
```

[BroadCastHeatMap, 3_BattleRoyaleSpawnSelection.c:229](/C:/Users/guill/Repos/br-wt/opt-analysis/BattleRoyale/addons/scripts_server/5_Mission/BattleRoyale/Server/States/3_BattleRoyaleSpawnSelection.c:229):

```enforce
array<vector> heatmap_points = new array<vector>;
foreach (string id, vector point : spawnpoints)
    heatmap_points.Insert(point);
GetRPCManager().SendRPC( RPC_DAYZBR_NAMESPACE, "UpdateHeatMap", new Param1<array<vector>>(heatmap_points), true );
```

It is already dirty-gated (`m_HeatMapDirty`, set when a player picks a point), so idle seconds cost nothing. Client side replaces the whole list and rebuilds the grid: `m_HeatMapSpawnPoints = spawnPoints; b_HeatMapDirty = true;` ([SpawnSelectionMenu.c:517](/C:/Users/guill/Repos/br-wt/opt-analysis/BattleRoyale/addons/scripts_client/5_Mission/GUI/SpawnSelectionMenu.c:517)).

**Why it costs at 100 players**

During the 30 s selection window with ~100 players clicking, most seconds are dirty: each is a fresh array allocation plus serialization of up to 100 vectors broadcast to 100 clients, and each client rebuilds its density grid. **[ESTIMATE]** up to ~10k vector serializations per dirty second server-side, ~300k over the window, purely to convey the handful of points that changed.

**Fix**

Accumulate changed points in a `m_HeatMapDelta` array when `spawnpoints.Set(...)` runs in `OnPlayerSpawnSelected`; `BroadcastHeatMap` sends only the delta (same RPC name/payload type) and clears it. Client `UpdateHeatMap` appends (`InsertAll`) instead of replacing. The menu always opens before the state activates, so clients start from an empty list. If a guaranteed-delivery assumption is not acceptable, send one full resync as the first broadcast of the state (flag), deltas thereafter.

**Vanilla API relied on**

Existing RPC channel only; `ScriptCallQueue.CallLater` semantics unchanged. RPC delivery-guarantee flag meaning is **[UNVERIFIED]** (proto not located) — hence the first-broadcast full resync in the fix.

**Invariants**

Player-visible heat map identical (same points, same density grid, at most the same 1 s latency). RPC name and parameter type unchanged.

**Verify**

Two clients in spawn selection: both pick points alternately; both heat maps show both points after ≤1 s; log payload point-counts server-side before/after (expect delta sizes ≈ clicks per second, not ≈ players).

## OPT-05 — Spectator anchors: move only when the target moved, and stop re-scanning identities

**Evidence**

[BattleRoyaleSpectators.c:49](/C:/Users/guill/Repos/br-wt/opt-analysis/BattleRoyale/addons/scripts_server/5_Mission/BattleRoyale/Server/BattleRoyaleSpectators.c:49):

```enforce
protected void UpdateAnchors()
{
    foreach (string id, PlayerBase anchor : m_Anchors)
    {
        PlayerBase target = m_Targets.Get(id);
        if (anchor && target)
            anchor.SetPosition(AnchorPosition(target));
    }
}
```

on a 1 s repeating queue ([BattleRoyaleSpectators.c:19](/C:/Users/guill/Repos/br-wt/opt-analysis/BattleRoyale/addons/scripts_server/5_Mission/BattleRoyale/Server/BattleRoyaleSpectators.c:19)). `AnchorPosition` itself calls `GetGame().SurfaceY` per anchor ([BattleRoyaleSpectators.c:45](/C:/Users/guill/Repos/br-wt/opt-analysis/BattleRoyale/addons/scripts_server/5_Mission/BattleRoyale/Server/BattleRoyaleSpectators.c:45)). Separately, `FindIdentity` allocates a fresh identity array and linear-scans it on every `Watch`/`Start` ([BattleRoyaleSpectators.c:246](/C:/Users/guill/Repos/br-wt/opt-analysis/BattleRoyale/addons/scripts_server/5_Mission/BattleRoyale/Server/BattleRoyaleSpectators.c:246)):

```enforce
array<PlayerIdentity> identities = new array<PlayerIdentity>;
GetGame().GetPlayerIndentities(identities);
```

Vanilla: `GetPlayerIndentities( out array<PlayerIdentity> identities )` ([P:\scripts\3_game\global\game.c:1166](/P:/scripts/3_game\global\game.c:1166)); `SetPosition` on a networked `PlayerBase` implies a sync update — the entity-level dirty mechanism is `SetSynchDirty` ([P:\scripts\3_game\entities\entityai.c:3064](/P:/scripts/3_game\entities\entityai.c:3064)) **[ESTIMATE for the exact per-move network cost]**.

**Why it costs at 100 players**

Late match: most of ~100 dead players may be spectators. That is up to ~99 `SurfaceY` + `SetPosition` (physics placement + network update of a full player entity) every second even when the watched player is standing still, plus a fresh identity-array allocation and O(N) scan for every watcher retargeted on each death (`RetargetWatchersOf` → `Watch` → `FindIdentity`), i.e. O(watchers × players) per kill in the endgame.

**Fix**

Cache each anchor's last placed position; in `UpdateAnchors` skip `SetPosition` when the target has moved less than ~3 m horizontally from it (the anchor only needs to keep the spectator inside the streaming bubble — see the class comment, lines 4–6). Pass the already-known `PlayerIdentity` through `Cycle`→`Watch` (the RPC handler receives `sender`) instead of re-resolving by string id; keep `FindIdentity` only for the delayed `Start` path, or cache id→identity at `Start` and drop it in `OnIdentityDisconnected`.

**Invariants**

Same spectate targets, same RPC (`SpectateTarget` payload unchanged), same 0.4 s cycle rate-limit. Behaviour change to disclose: the anchor (invisible, 3 m underground) follows in ~3 m steps instead of continuously — the camera follows the target directly client-side, so nothing player-visible changes.

**Verify**

In-game with 3 players (the currently untested spectator case per HANDOFF): kill two, confirm both spectate, anchors still stream the target correctly when it runs 500 m; server script-log: count anchor `SetPosition` calls while the target stands still for 30 s (expect ~0 after the first).

## OPT-06 — Stop building log strings that the log level will throw away

**Evidence**

[BattleRoyaleUtils.c:63](/C:/Users/guill/Repos/br-wt/opt-analysis/BattleRoyale/addons/scripts_client/3_Game/BattleRoyaleUtils.c:63):

```enforce
static void Trace(string message)
{
    LogServerMessage(TRACE, message);
}
```

`LogServerMessage` checks the level only after the caller has built the string. Hot call sites build strings unconditionally, e.g. `IsInZone` on every teleport-safety check ([BattleRoyaleZone.c:157](/C:/Users/guill/Repos/br-wt/opt-analysis/BattleRoyale/addons/scripts_server/5_Mission/BattleRoyale/Server/BattleRoyaleZone.c:157)):

```enforce
BattleRoyaleUtils.Trace(d);
BattleRoyaleUtils.Trace(radius_pow);
```

(the `Trace(float)` overload itself does `f.ToString()` before the check, [BattleRoyaleUtils.c:73](/C:/Users/guill/Repos/br-wt/opt-analysis/BattleRoyale/addons/scripts_client/3_Game/BattleRoyaleUtils.c:73)), and `GetDynamicStartingZone` traces per zone tried ([0_BattleRoyaleState.c:648](/C:/Users/guill/Repos/br-wt/opt-analysis/BattleRoyale/addons/scripts_server/5_Mission/BattleRoyale/Server/States/0_BattleRoyaleState.c:648)) while being called per candidate position from `IsInsideExpandedFirstZone`/`IsSafeForTeleport`. Default level is Error-only: `BATTLEROYALE_LOG_LEVEL = 0` ([BattleRoyaleConstants.c:19](/C:/Users/guill/Repos/br-wt/opt-analysis/BattleRoyale/addons/scripts_client/2_GameLib/BattleRoyaleConstants.c:19)) — **TOUCHES_BUSY_FILES: no** (no change needed in that file).

**Why it costs at 100 players**

With default logging, none of this reaches disk, but every Prepare candidate (OPT-03's thousands of checks) still pays several string allocations and function calls, and any operator who sets `BRLogLevel` to trace in production multiplies it into actual I/O. **[ESTIMATE]** low steady-state, med during Prepare and zone generation.

**Fix**

At the hot sites only, guard with the already-cached level check: `if (BattleRoyaleUtils.CheckLogLevel(BattleRoyaleUtils.TRACE)) BattleRoyaleUtils.Trace(...)` (pattern already used on the client, e.g. [BattleRoyaleRPC.c:142](/C:/Users/guill/Repos/br-wt/opt-analysis/BattleRoyale/addons/scripts_client/3_Game/BattleRoyale/RPC/BattleRoyaleRPC.c:142)), and delete the two float traces in `IsInZone` outright. Cache `GetDynamicStartingZone(i_NumStartingPlayers)` once per Prepare activation instead of recomputing per candidate. Leave cold-path logging (connect, state changes, kills) untouched.

**Vanilla API relied on**

None.

**Invariants**

Log output identical at every level; no behaviour change. Do not change `BATTLEROYALE_LOG_LEVEL` or the level semantics in `BattleRoyaleUtils`.

**Verify**

Run a solo match with `-br-trace`: script log still contains zone/teleport traces; with default level, Prepare completes with the same spawn results and zero trace lines; diff the log before/after at trace level for missing hot-path lines only where intentionally removed (`IsInZone` floats).

## OPT-07 — Combat path: cache killer names and don't build webhook JSON when the API is off

**Evidence**

Per-hit, server [PlayerBase.c:88](/C:/Users/guill/Repos/br-wt/opt-analysis/BattleRoyale/addons/scripts_server/4_World/Entities/ManBase/PlayerBase.c:88) (scripts_server):

```enforce
br_last_player_hit_name = playerSource.GetIdentity().GetName();
```

`GetName()` builds a string on every `EEHitBy`, although the server already caches the name at connect as `m_BR_PlayerName` (client/server `BR_GetPlayerName`, [PlayerBase.c:87](/C:/Users/guill/Repos/br-wt/opt-analysis/BattleRoyale/addons/scripts_client/4_World/Entities/ManBase/PlayerBase.c:87)). Per kill, `OnPlayerKilled` builds the whole `json_data` map and position strings unconditionally ([0_BattleRoyaleState.c:718](/C:/Users/guill/Repos/br-wt/opt-analysis/BattleRoyale/addons/scripts_server/5_Mission/BattleRoyale/Server/States/0_BattleRoyaleState.c:718)) but only consumes it inside `if ( m_ServerData.enable_vigrid_api )` ([0_BattleRoyaleState.c:787](/C:/Users/guill/Repos/br-wt/opt-analysis/BattleRoyale/addons/scripts_server/5_Mission/BattleRoyale/Server/States/0_BattleRoyaleState.c:787)).

**Why it costs at 100 players**

Hits are the highest-frequency gameplay event in a firefight (automatic fire across ~100 players). **[ESTIMATE]** hundreds of small string allocations/s in big fights — minor individually, pure waste collectively. The kill-side map is ~100 allocations per match: negligible alone, free to remove.

**Fix**

Use `playerSource.BR_GetPlayerName()` (cached field on the server) for `br_last_player_hit_name`; move the `json_data` construction inside the existing `enable_vigrid_api` block (the `ScoreWebhook` send above it already reads the same flag).

**Invariants**

Kill-feed names, bleed-out credit and webhook payloads byte-identical; note `BR_GetPlayerName` on the server returns the connect-time name, which is the same identity name unless a rename mid-session is expected — flag that assumption in the PR.

**Verify**

Two-player test: bleed-out kill within 5 min still credits the shooter with the same name/weapon/distance in the kill feed; with the API enabled, capture one `player.kill` payload before/after and diff.

## OPT-08 — Cache the CE weapon-spawn config reads in `extra_spawnwithammoandmagazine`

**Evidence**

[ItemBase.c:9](/C:/Users/guill/Repos/br-wt/opt-analysis/BattleRoyale/addons/extra_spawnwithammoandmagazine/Scripts/4_World/Entities/ItemBase.c:9):

```enforce
bool config_disable_spawn_with_ammo = GetGame().ServerConfigGetInt( "BRDisableSpawnWithAmmo" ) == 1;
...
int config_min_spawn = GetGame().ServerConfigGetInt( "BRMinSpawnAmmo" );
int config_max_spawn = GetGame().ServerConfigGetInt( "BRMaxSpawnAmmo" );
```

plus `GetGame().ConfigGetInt("CfgMagazines " + magazineType + " count")` per spawn attempt ([ItemBase.c:60](/C:/Users/guill/Repos/br-wt/opt-analysis/BattleRoyale/addons/extra_spawnwithammoandmagazine/Scripts/4_World/Entities/ItemBase.c:60)), all inside `EEOnCECreate`, i.e. for every weapon the Central Economy creates. The created magazines themselves (`CreateObjectEx(..., ECE_CREATEPHYSICS|ECE_UPDATEPATHGRAPH)`, [ItemBase.c:66](/C:/Users/guill/Repos/br-wt/opt-analysis/BattleRoyale/addons/extra_spawnwithammoandmagazine/Scripts/4_World/Entities/ItemBase.c:66)) are loot and stay untouched.

**Why it costs at 100 players**

CE (re)spawns weapons continuously all match; each pays three server-config lookups and a string-concatenated config path per magazine attempt. **[ESTIMATE]** thousands of lookups per match — small, but the fix is free and riskless.

**Fix**

Read the three `BR*` values once into `static` fields on first use (server config does not change at runtime — same assumption `BattleRoyaleUtils.CheckLogLevel` already makes, [BattleRoyaleUtils.c:80](/C:/Users/guill/Repos/br-wt/opt-analysis/BattleRoyale/addons/scripts_client/3_Game/BattleRoyaleUtils.c:80)). Keep the per-attempt magazine-capacity check.

**Vanilla API relied on**

None new; `ServerConfigGetInt` is already used in place.

**Invariants**

Identical spawn counts and magazine selection; explicitly **not** proposing fewer magazines (that would be a loot change).

**Verify**

CE-spawn weapons in a solo test before/after: same magazine count distribution over ~50 weapons; `BRDisableSpawnWithAmmo=1` in serverDZ.cfg still suppresses spawns.

## OPT-09 — Ship a measured BR economy profile instead of unreviewed vanilla CE (config, behaviour-flagged)

**Evidence**

The mission package ships only two mission files (`db/globals.xml`, `cfggameplay.json`); the server guide states it plainly:

[BattleRoyale_server/README.md:266](/C:/Users/guill/Repos/br-wt/opt-analysis/BattleRoyale_server/README.md:266):

```text
- `types.xml`, events and vehicles of the vanilla mission are unchanged (loot volume and vehicles still to be reviewed for 100 players).
```

What is tuned: `ZombieMaxCount = 150`, `AnimalMaxCount = 0`, `CleanupLifetimeDeadPlayer = 3600` ([globals.xml:31](/C:/Users/guill/Repos/br-wt/opt-analysis/BattleRoyale_server/mpmissions/dayzOffline.chernarusplus/db/globals.xml:31), [globals.xml:7](/C:/Users/guill/Repos/br-wt/opt-analysis/BattleRoyale_server/mpmissions/dayzOffline.chernarusplus/db/globals.xml:7)). Server size itself is fine: `maxPlayers = 100` ([serverDZ.cfg.example:7](/C:/Users/guill/Repos/br-wt/opt-analysis/BattleRoyale_server/serverDZ.cfg.example:7)).

**Why it costs at 100 players**

Vanilla Chernarus CE maintains its full nominal loot/infected/vehicle population across the whole map while a BR match concentrates 100 players inside one shrinking circle; every persisted entity is ticked, cleaned and network-managed whether or not any player can reach it. **[ESTIMATE]** med, unmeasured — this is the largest *unknown* in the server budget, which is why it is ranked by risk-adjusted priority, not first.

**Fix (measurement first, then config)**

1. On a 100-bot or playtest server, record CE entity counts and server FPS per match phase with vanilla files as baseline.
2. Ship explicit `types.xml`/`events.xml` in the mission folder pinning the measured-acceptable values. Candidates that are pure load with identical player-visible behaviour inside the play area are hard to guarantee from static analysis; reducing infected caps, corpse lifetime (3600 s) or out-of-zone loot nominal **does change gameplay** (encounters, lootable bodies, loot density) and must be decided and announced as such, not smuggled in as an optimization.

**Vanilla API relied on**

None — mission economy XML only.

**Invariants**

None claimed: this point is explicitly behaviour-changing if tuned, and a no-op if only pinned after measurement. Do not merge it as a silent "optimization".

**Verify**

Before/after server FPS and CE counts in the same match phase with the same player count; playtest confirmation that loot/infected feel inside the first zone is unchanged at the chosen values.

## OPT-10 — Fill heat-map squares with one thick line instead of one line per pixel

**Evidence**

[SpawnSelectionMenu.c:510](/C:/Users/guill/Repos/br-wt/opt-analysis/BattleRoyale/addons/scripts_client/5_Mission/GUI/SpawnSelectionMenu.c:510):

```enforce
// Draw the filled square using horizontal lines
for (float y = top; y <= bottom; y += 1)
{
    canvas.DrawLine(left, y, right, y, 1, color);
}
```

Each density cell also emits ring squares at offsets 1 and 2 before the primary square ([SpawnSelectionMenu.c:295](/C:/Users/guill/Repos/br-wt/opt-analysis/BattleRoyale/addons/scripts_client/5_Mission/GUI/SpawnSelectionMenu.c:295)), and squares are redrawn whenever the canvas is dirty (new heat data at up to 1 Hz per OPT-04, or any map pan/zoom).

Vanilla: `CanvasWidget.DrawLine(float x1, float y1, float x2, float y2, float width, int color)` ([P:\scripts\1_core\proto\enwidgets.c:343](/P:/scripts/1_core/proto/enwidgets.c:343)) — width is a native parameter, so a filled square is one horizontal line with `width = screenHeight`.

**Why it costs at 100 players**

With ~100 spawn points the grid holds dozens of cells; **[ESTIMATE]** cells × ~25 squares × tens of pixel rows ≈ 10⁴–10⁵ `DrawLine` calls per redraw, repeated on every heat-map update and map movement during the 30 s menu — a client frame hitch exactly while players are choosing spawns.

**Fix**

Replace the per-pixel loop with a single `canvas.DrawLine(left, cy, right, cy, screenHeight, color)` per square (keep a step of 2–4 px as fallback if the thick-line caps render visibly different). Cell positions, sizes and colours come from the existing `m_HeatMapCellX/Z/Color` arrays — untouched. **TOUCHES_BUSY_FILES: no** — the fix needs no constant from `BattleRoyaleConstants.c`.

**Invariants**

Same squares at the same world positions with the same colours; only the rasterization changes.

**Verify**

Screenshot the spawn menu with a known set of spawn points before/after at two zoom levels and compare square coverage visually; count `DrawLine` calls (segments counter pattern already used by the minimap stats) — expect ~25× fewer.

## OPT-11 — Throttle the minimap's map moves and canvas redraws — TOUCHES_BUSY_FILES: yes

**Evidence**

**TOUCHES_BUSY_FILES** (`scripts_client/5_Mission/GUI/HUD/BattleRoyaleMiniMap.c` is under active change by another stream; coordinate before merging).

[BattleRoyaleMiniMap.c:173](/C:/Users/guill/Repos/br-wt/opt-analysis/BattleRoyale/addons/scripts_client/5_Mission/GUI/HUD/BattleRoyaleMiniMap.c:173):

```enforce
else if (!m_HasMapCenter || HasCenterMoved(center))
{
    m_Map.SetMapPos(center);
    m_MapCenter = center;
    m_HasMapCenter = true;
    m_CanvasDirty = true;
```

The move threshold is half a pixel (`MOVE_THRESHOLD_PX = 0.5`, [BattleRoyaleMiniMap.c:12](/C:/Users/guill/Repos/br-wt/opt-analysis/BattleRoyale/addons/scripts_client/5_Mission/GUI/HUD/BattleRoyaleMiniMap.c:12)), so while the player moves, `m_CanvasDirty` is set nearly every frame and `RedrawCanvas` re-draws both ovals ([BattleRoyaleMiniMap.c:275](/C:/Users/guill/Repos/br-wt/opt-analysis/BattleRoyale/addons/scripts_client/5_Mission/GUI/HUD/BattleRoyaleMiniMap.c:275)) — up to 512 segments per oval on a clipped view (`max_segments = 512`, [BattleRoyaleMapCanvas.c:57](/C:/Users/guill/Repos/br-wt/opt-analysis/BattleRoyale/addons/scripts_client/5_Mission/GUI/BattleRoyaleMapCanvas.c:57)). The file already has trace-level counters (`m_StatsMapMoves`, `m_StatsRedraws`) designed for exactly this measurement.

**Why it costs at 100 players**

Client-only, but it is a per-frame cost on every client for the whole match: **[ESTIMATE]** a full two-oval canvas redraw (up to ~1,000 line segments plus the `MapWidget` re-render) at frame rate while running, which is most of a match.

**Fix**

Coalesce: keep following the player, but only `SetMapPos` + redraw when the centre moved ≥ ~4 px **or** ≥100 ms elapsed since the last move; zone changes still set `m_CanvasDirty` immediately (existing path). Player-visible change to disclose: the minimap scrolls in ~10 Hz steps instead of continuously — the zone circles stay registered with the map because both move in the same step.

**Vanilla API relied on**

None new (`MapWidget.SetMapPos`/`CanvasWidget.DrawLine` already in use; DrawLine proto at [P:\scripts\1_core\proto\enwidgets.c:343](/P:/scripts/1_core\proto\enwidgets.c:343)).

**Invariants**

Same zoom levels, same circles, same heading arrow behaviour (heading already has its own 1° threshold).

**Verify**

Enable trace logging and run in place vs sprint 500 m before/after: compare the minimap's own `%1 s: %2 frames, %3 map moves, %4 canvas redraws` line ([BattleRoyaleMiniMap.c:303](/C:/Users/guill/Repos/br-wt/opt-analysis/BattleRoyale/addons/scripts_client/5_Mission/GUI/HUD/BattleRoyaleMiniMap.c:303)); expect redraws/frame to fall ~6× while moving, and visually confirm circles stay glued to the terrain.

## OPT-12 — Cache the spectator camera's head-bone index

**Evidence**

[BattleRoyaleSpectatorCamera.c:98](/C:/Users/guill/Repos/br-wt/opt-analysis/BattleRoyale/addons/scripts_client/4_World/Entities/BattleRoyaleSpectatorCamera.c:98):

```enforce
protected void FollowFirstPerson(Human target)
{
    vector head[4];
    target.GetBoneTransformWS(target.GetBoneIndexByName("Head"), head);
```

`EOnPostFrame` runs this every rendered frame while spectating in first person ([BattleRoyaleSpectatorCamera.c:25](/C:/Users/guill/Repos/br-wt/opt-analysis/BattleRoyale/addons/scripts_client/4_World/Entities/BattleRoyaleSpectatorCamera.c:25)); the target's network-id lookup per frame (`GetObjectByNetworkId`, line 45) is necessary, the by-name bone lookup is not.

**Why it costs at 100 players**

Client-only, one spectator at a time per client, but: **[ESTIMATE]** a string-hash bone lookup plus bone transform per frame for every spectating (i.e. most dead) client in first person — small, and the fix is trivial.

**Fix**

Cache the bone index the first time a non-null target is followed (all `Human` player skeletons share the bone layout; re-resolve only when the cached index is -1 or the target class changes). Keep the per-frame `GetBoneTransformWS` and weapon-barrel math.

**Vanilla API relied on**

None new.

**Invariants**

Identical camera position/orientation in both person modes; third-person raycast unchanged.

**Verify**

Spectate in first person before/after: camera still tracks the head/weapon through ADS and melee; no script errors when the target dies and retargets.

---

## Checked and fine

- **Zone wall** ([BattleRoyaleZoneWall.c](/C:/Users/guill/Repos/br-wt/opt-analysis/BattleRoyale/addons/scripts_client/5_Mission/BattleRoyale/Client/BattleRoyaleZoneWall.c)): already refresh-gated at 0.5 s, view-range culled at 500 m, panels persist in a map; per-refresh allocations (one `set<int>`) are negligible. No change.
- **HUD setters** ([BattleRoyaleHud.c](/C:/Users/guill/Repos/br-wt/opt-analysis/BattleRoyale/addons/scripts_client/5_Mission/GUI/HUD/BattleRoyaleHud.c)): text/colour/arrow writes are change-gated (`m_LastDistanceMeters`, `m_LastPhaseLabel/Seconds`, 0.5° arrow threshold); `BattleRoyaleClient.Update` similarly guards counts/kills/countdown by previous values, and kill feed rebuilds only on serial change or expiry ([BattleRoyaleKillFeed.c:57](/C:/Users/guill/Repos/br-wt/opt-analysis/BattleRoyale/addons/scripts_client/5_Mission/GUI/HUD/BattleRoyaleKillFeed.c:57)). The remaining per-frame zone-distance math is two sqrt/trig evaluations — not worth a throttle on its own.
- **Lobby player tags** ([IngameHud.c](/C:/Users/guill/Repos/br-wt/opt-analysis/BattleRoyale/addons/scripts_client/5_Mission/GUI/IngameHud.c)): the BR override already replaces vanilla's per-player raycast with one raycast per 0.15 s.
- **Spawn-selection and map canvases** ([SpawnSelectionMenu.c](/C:/Users/guill/Repos/br-wt/opt-analysis/BattleRoyale/addons/scripts_client/5_Mission/GUI/SpawnSelectionMenu.c), [MapMenu.c](/C:/Users/guill/Repos/br-wt/opt-analysis/BattleRoyale/addons/scripts_client/5_Mission/GUI/MapMenu.c)): redraws are view/content-dirty gated; only the per-pixel square fill (OPT-10) is wasteful.
- **State machine cadence**: `BattleRoyaleServer.Update` iterates ~15 states per frame with empty `Update()` bodies and polls `IsComplete()` at 0.1 s ([BattleRoyaleServer.c:174](/C:/Users/guill/Repos/br-wt/opt-analysis/BattleRoyale/addons/scripts_server/5_Mission/BattleRoyale/Server/BattleRoyaleServer.c:174)); under `Carim`, `GetGroupsCount` is already 5 s-cached against exactly this poll ([0_BattleRoyaleState.c:446](/C:/Users/guill/Repos/br-wt/opt-analysis/BattleRoyale/addons/scripts_server/5_Mission/BattleRoyale/Server/States/0_BattleRoyaleState.c:446)). No change.
- **Countdown notifications in StartMatch/Round**: timer-driven broadcasts at ≥1 s spacing; intended player-visible messaging, trivial volume.
- **Persistence**: `PlayerBase.Save()` returns `false` (no character DB writes) — already the cheapest option; lobby characters are created without the DB (`useDB = false`, [MissionServer.c:90](/C:/Users/guill/Repos/br-wt/opt-analysis/BattleRoyale_server/../BattleRoyale/addons/scripts_server/5_Mission/Mission/MissionServer.c:90)).
- **Config load**: the load-then-save of the six JSON settings happens once at startup ([BattleRoyaleConfig.c:106](/C:/Users/guill/Repos/br-wt/opt-analysis/BattleRoyale/addons/scripts_server/3_Game/Config/BattleRoyaleConfig.c:106)); not a runtime cost. Webhooks use async `POST` per kill except `CreateMatchWebhook.POST_now`, which runs once at server init — a slow API there delays startup, not matchplay.
- **Spectator camera streaming**: `UpdateSpectatorPosition` is already throttled to 0.5 s ([BattleRoyaleSpectatorCamera.c:69](/C:/Users/guill/Repos/br-wt/opt-analysis/BattleRoyale/addons/scripts_client/4_World/Entities/BattleRoyaleSpectatorCamera.c:69)); vanilla proto at [P:\scripts\3_game\global\game.c:385](/P:/scripts/3_game\global\game.c:385).
- **`extra_*` one-offs**: car/boat fill, chambered weapons, battery/ammo-on-spawn run once per CE creation and are cheap, apart from the config reads in OPT-08.

## WHAT I COULD NOT VERIFY

- `P:\scripts\` was reachable and used for the scheduler, physics, widget and game protos cited above. The `RPCManager.SendRPC` proto (parameter order, meaning of the `true` flag assumed to be guaranteed delivery) was **not located** in `P:\scripts\` in this pass — claims depending on it are marked **[UNVERIFIED]** (OPT-01, OPT-04).
- Exact server frame rate and therefore the absolute scheduled-tick rate in OPT-02 is **[ESTIMATE]**; `SCHEDULER_PLAYERS_PER_TICK = 5` is verified, the ticks/second conversion assumes ~30 fps.
- Whether spectator anchor bodies (`CreatePlayer` + `SelectPlayer`) themselves receive vanilla scheduled ticks/modifier updates could not be confirmed from the scripts read; OPT-05 only claims the anchor-move and identity-scan savings.
- The per-`SetPosition`/per-RPC network byte costs are **[ESTIMATE]** from call counts; no packet capture or 100-player profiling exists (consistent with `BattleRoyale_server/README.md:268`).
- `Carim`-guarded code (`GetGroupsCount`, party spawn sharing) only compiles with that mod loaded; its costs were read but could not be weighed against a confirmed load order.
- Busy files under change by another stream (`Inputs.xml`, `BattleRoyaleConstants.c`, `NotificationUI.c`, `BattleRoyaleMiniMap.c`) were analysed as-is; only OPT-11 touches one and is flagged.

END_REPORT
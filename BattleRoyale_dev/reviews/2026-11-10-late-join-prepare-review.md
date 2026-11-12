Three P2 findings. No definite compile defect found.

1. **P2 — The earlier check rejects COT admins.** [MissionServer.c:78](C:/Users/guill/Repos/br-wt/late-join-notice/BattleRoyale/addons/scripts_server/5_Mission/Mission/MissionServer.c:78) calls the existing permission predicate before COT registers the connecting player. COT’s [HasPermission:265](C:/Users/guill/Repos/br-ref/cot/scripts/4_World/CommunityOnlineTools/Classes/Permissions/JMPermissionManager.c:265) returns false for a missing player record; registration happens later in [COTModule.OnInvokeConnect:423](C:/Users/guill/Repos/br-ref/cot/scripts/4_World/CommunityOnlineTools/Classes/COTModule.c:423).

   **Repro:** grant `BattleRoyale.StateMachine.Update` through COT only, omit that Steam ID from `admins_steamid64`, and connect during a round → prepare sends `MatchInProgress`; the admin disconnects. HEAD checks after registration and allows them. Preserve the exemption using permissions available at prepare time, or defer rejection while authorization is unresolved.

2. **P2 — The last-round timer is not a match deadline.** [BattleRoyaleServer.c:345](C:/Users/guill/Repos/br-wt/late-join-notice/BattleRoyale/addons/scripts_server/5_Mission/BattleRoyale/Server/BattleRoyaleServer.c:345) treats `i_RoundTimeInSeconds` as an upper bound. However, [LastRound.c:73](C:/Users/guill/Repos/br-wt/late-join-notice/BattleRoyale/addons/scripts_server/5_Mission/BattleRoyale/Server/States/7_BattleRoyaleLastRound.c:73) only schedules zone locking; completion at `:132–144` depends on player/group elimination.

   **Repro:** `static_timers[0]=110`, valid `zone_damage_delta=0.01`, two healthy players in different groups entering the last round, neither fighting → popup says **at most 2 min**, but both survive past 120 seconds and the match continues. Pausing also defeats any finite bound. Use the no-time message unless an actual deadline is enforced, or describe the number as an estimate.

3. **P2 — Pre-round accounting omits configurable time.** [BattleRoyaleServer.c:358](C:/Users/guill/Repos/br-wt/late-join-notice/BattleRoyale/addons/scripts_server/5_Mission/BattleRoyale/Server/BattleRoyaleServer.c:358) omits `spawn_selection_extra_time`; the actual [selection timer:81](C:/Users/guill/Repos/br-wt/late-join-notice/BattleRoyale/addons/scripts_server/5_Mission/BattleRoyale/Server/States/3_BattleRoyaleSpawnSelection.c:81) includes it.

   **Repro:** defaults except valid `spawn_selection_extra_time=1800`; join at selection start → calculation returns **29 min**, while selection alone lasts **1,830 seconds (30.5 min)**. Also, summing raw `static_timers` at server `:364` misses the travel offset added by [GetZoneTimer:146](C:/Users/guill/Repos/br-wt/late-join-notice/BattleRoyale/addons/scripts_server/5_Mission/BattleRoyale/Server/BattleRoyaleZone.c:146). Account for actual scheduled durations.

Other checks:

- **Compile contracts:** static callbacks have vanilla precedent (`4_world/classes/basebuilding/rebuildingspawner.c:71–93`); `DisconnectSessionForce` is passed identically in vanilla `3_game/dayzgame.c:92`. Translation/format declarations exist in `1_core/proto/enwidgets.c:112` and `enstring.c:526`. The `SERVER` guard, shared constant visibility, and prepare override’s five parameters—including `out`—look correct.
- **RPC/UI:** vanilla sends targetless RPCs during prepare (`missionserver.c:320`, `cfggameplayhandler.c:97`) and handles them in `DayZGame.OnRPC` (`dayzgame.c:3159`). The custom handler bypasses [CF’s client/server dispatch checks](https://github.com/Arkensor/DayZ-CommunityFramework/blob/production/JM/CF/Scripts/3_Game/CommunityFramework/RPC/RPCManager.c#L87-L99). This supports the route, but **does not prove receipt before world loading or that native disconnect always recreates `MainMenu` and runs `Init`**. Those remain runtime-unverified.
- **Timer indexing:** reverse mapping and the remaining-prefix sum are correct, including dynamic starting-zone skips. The phase helper correctly preserves `-1` and clamps elapsed countdowns. Win/restart’s nominal delays are 15 + 10 seconds.
- **CSV:** all rows have 15 columns; quoting, strict UTF-8 without BOM, CRLF, and `%1` in all 14 timed-message fields pass.

The in-memory lint gate was **GREEN** across 13 addons; `git diff --check` passed. Lint cannot detect the permission lifecycle or false deadline assumptions above. No files changed; repro scenarios were source-derived, not executed in DayZ.

UNSOUND
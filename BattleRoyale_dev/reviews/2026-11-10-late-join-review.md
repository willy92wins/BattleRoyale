Two P2 findings. The repros below are source-derived; they were not run in-game.

1. **P2 — The one-second abort stops progressing during loading.** [BattleRoyaleRPC.c:418](C:/Users/guill/Repos/br-wt/late-join-notice/BattleRoyale/addons/scripts_client/3_Game/BattleRoyale/RPC/BattleRoyaleRPC.c:418) uses `CALL_CATEGORY_GUI`. Vanilla [dayzgame.c:3034](<C:/Users/guill/OneDrive/Documentos/DayZ Projects/scripts/3_game/dayzgame.c:3034>) skips that queue while loading.

   **Repro:** non-admin joins an active match; deliver `MatchInProgress` with `Param1<int>(0)` while loading remains active for 35 seconds → no one-second abort or immediate explanation; the server’s 30-second kick fires first. Lint cannot detect this lifecycle behavior. Make rejection timing explicit; changing queues alone would not establish preload safety.

2. **P2 — A stale abort can disconnect the next session.** [BattleRoyaleRPC.c:424](C:/Users/guill/Repos/br-wt/late-join-notice/BattleRoyale/addons/scripts_client/3_Game/BattleRoyale/RPC/BattleRoyaleRPC.c:424) aborts whichever mission is current. [MainMenu.c:146](C:/Users/guill/Repos/br-wt/late-join-notice/BattleRoyale/addons/scripts_client/5_Mission/Mission/MainMenu.c:146) clears the flag but leaves the callback queued.

   **Repro:** receive the rejection from server A; independently disconnect before the timer fires; from the returned menu immediately join lobby server B within the remaining GUI second → loading suspends the old timer, which resumes afterward and aborts the accepted B session. Cancel the callback when consuming the message and bind deferred work to its originating session.

Other checks:

- **Enforce:** no apparent new syntax, static, layering or guard defect. Bare method callbacks are supported by [tools.c:61](<C:/Users/guill/OneDrive/Documentos/DayZ Projects/scripts/2_gamelib/tools.c:61>). `Mission.AbortMission()` exists in [3_Game/gameplay.c:775](<C:/Users/guill/OneDrive/Documentos/DayZ Projects/scripts/3_game/gameplay.c:775>); both client files use `#ifndef SERVER`.
- **Menu recreation:** the ordinary gameplay return supports `Init()` running again. [MissionMainMenu.OnMissionFinish:93](<C:/Users/guill/OneDrive/Documentos/DayZ Projects/scripts/5_mission/mission/missionmainmenu.c:93>) closes/nulls the menu; `OnInit:25` re-enters it, and [MissionBase:208](<C:/Users/guill/OneDrive/Documentos/DayZ Projects/scripts/5_mission/mission/missionbase.c:208>) constructs a new `MainMenu`.
- **RPC registration:** initialization occurs through `MissionGameplay.OnInit()` → `BattleRoyaleClient.Init()`. Native arrival ordering remains unproven. [CF drops unregistered calls without buffering](https://github.com/Arkensor/DayZ-CommunityFramework/blob/production/JM/CF/Scripts/3_Game/CommunityFramework/RPC/RPCManager.c#L80-L107); sharing the existing phase-send hook does not prove readiness.
- **Admins/tick:** [BattleRoyaleServer.c:267](C:/Users/guill/Repos/br-wt/late-join-notice/BattleRoyale/addons/scripts_server/5_Mission/BattleRoyale/Server/BattleRoyaleServer.c:267) returns before the new send. The tick’s zero-time logout only exits when `m_Logout` exists ([vanilla missiongameplay.c:1422](<C:/Users/guill/OneDrive/Documentos/DayZ Projects/scripts/5_mission/mission/missiongameplay.c:1422>)); it is not an unconditional competing kick.
- **CSV:** strict parsing passed: all 79 records have 15 columns, valid quoting, UTF-8 without BOM, unique keys. Existing content is byte-identical after LF/CRLF normalization.

`git diff --check` passed. Fresh lint scanned 45 client and 51 server files, reporting only allowlisted errors. Engine compilation and connection-stage abort safety remain unverified.

SOUND-with-fixes
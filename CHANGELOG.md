# Changelog

## 1.7.0 (2026-11-15)

### Changed
- Dabs Framework is no longer needed. The two things the mod took from it, the list of named places of the map used
  to pick spawn villages and the query string of the webhooks, are now the mod's own server code and give the same
  results. Servers drop `@Dabs Framework` from `-mod`, and players no longer download it (34 MB, more than half of
  what a player downloaded for BattleRoyale, CF, Dabs and COT).
- Kill feed: a player run over by a vehicle with another player at the wheel is credited to the driver. The kill feed
  names the driver, the kill counts for them, the webhook names them, and a later bleed-out within the usual time
  credits them too. Without a driver, a run-over still names nobody.

### Fixed
- Kill feed: a player killed in a vehicle crash is shown as killed in the crash, not as bled out with the kill
  credited to the last player who hit them.

### Notes
- The message the server sends the clients for the kill feed keeps its format; a run-over now carries the driver's
  name, which the client already shows. Server and clients should run 1.7.0.
- Reviewed by a second model family (adversarial review of each change and of the three together) and tested in
  DayZDiag with one player: the mod starts without Dabs and the match preparation reads the same 77 places of the map
  as before. Not tested yet in game: a deadly crash (the test harness could not produce one), motorbikes, a run-over
  (needs two players) and the webhooks.
- `tools/check_standalone.py` now fails if a Dabs type or a `DF_*` required addon comes back.
- `@BattleRoyale-VPP` is unchanged: keep the 1.1.0 one.

## 1.6.1 (2026-11-15)

### Fixed
- Voice: back in the lobby, the microphone icon no longer fades out while push-to-talk is held. When the voice
  level drops to whisper, the icon only updates its range while the player talks, as when the range is changed
  by key.
- Serial matches: when a match starts and ends while the previous one is still being cleaned up, its spectators
  come back to the lobby with their own character instead of the default one. The character types of a match
  are now cleared as soon as every player is back in the lobby, not when the cleanup ends.
- The spawn trace of the match preparation (trace log level only) reuses the starting zone resolved when the
  preparation starts instead of resolving it again for every random spawn.
- The Carim party code (only compiled with Carim) declares the party members with the right type.

### Notes
- Nothing the client and the server send each other changed: the microphone fix is in the client, the other
  fixes are in the server. Server and clients should run 1.6.1.
- Reviewed by a second model family (adversarial review) and tested in DayZDiag with one player. Not tested yet:
  a spectator going back to the lobby (needs several players) and push-to-talk.
- `@BattleRoyale-VPP` is unchanged: keep the 1.1.0 one.

## 1.6.0 (2026-11-14)

### Added
- Waiting room in the lobby (`enable_waiting_room` in `general_settings.json`, on by default). While a match
  runs, a player who connects, a player who dies with spectating off (after `spectator_delay_seconds`) and a
  spectator who presses F1 wait in the lobby with a new character: immune, healed, kept inside the lobby
  radius and outside the match (no zone damage, not counted). Their HUD follows the match (phase, player
  count, kill feed) without zone distance or zone warnings. With serial matches they join the next lobby with
  the same character; with the classic restart the restart disconnects everyone as before. A waiting player
  who reconnects gets its state again. With the setting off, a player who connects during a match is told it
  is running and leaves, as in 1.5.0.
- Restart warning: when the server will restart after the match (no serial matches, or the serial restart
  count reached), players are told when the last round starts and when the match ends; the waiting room
  notice says so too.

### Changed
- Notifications are at the top left corner of the HUD. The kill feed is at the top right, level with the match
  icons and right next to what that corner shows (counters, zone distance, group count, minimap); its rows are
  right aligned, grow to the left and stay clear of the centred phase lines (a row too wide drops the weapon
  and distance, then shortens the names). Text widths are converted to layout units, so the rows keep their
  size at every resolution.
- The winner is immune and healed when the match is won.
- Unstuck: a pending unstuck is cancelled when the match start ends, and a player waits 30 s after an unstuck
  before asking for the next one.
- With `enable_zone_damage` off, the final zone no longer forces the end of the match by damage.
- When a serial match is the one before a scheduled restart, the winner is disconnected and the clients are
  told the server restarts, as with the classic restart.
- Admin "Next" does nothing during the restart state (it stopped the shutdown timer and left the server stuck).
- A player faces the village from its spawn point at the start of a match, not from the lobby.
- Back in the lobby after a match, voice is limited to whisper again, as in the first lobby.

### Performance and fixes (council review, 100 players)
- Log strings are only built when their log level is on (hits, players added to and removed from a state,
  spawn search traces).
- The spawn preparation resolves the first zone once; its periodic heal runs only when the vitals need it.
- Spawn with ammo: magazine capacity, chamberable ammo and the ammo economy check are cached per class; ammo
  without an economy profile is no longer created only to be deleted.
- The waiting room reads the ground height once per tick and heals only when needed.
- Serial turnover: the player count is broadcast once per batch of returning players instead of once per
  player; a turnover request that arrives while the previous turnover is still cleaning is kept instead of
  leaving the server stuck in the restart state; the cleaning no longer aborts after 120 s; an item dropped
  again in the same match is recorded once.
- Spectators: the anchors of spectators who left are swept every 10 s; the next target is found once per
  death instead of once per watcher; a player who is alive again is never put into spectator mode.
- The late-join kick timer is cancelled when that player leaves, and lobby request times are dropped on
  disconnect.
- Client: the player count starts unknown (no "0" before the first count), no ghost future zone after the
  final lock, and the spectator state is cleared when the mission ends.
- The death webhook checks its source before using it; a player's character type is remembered without
  serial matches too.
- Every change was reviewed by a second model family (adversarial review) and tested in DayZDiag.

### Not tested yet
- With several players: watching another player and the anchor sweep, F1 from spectating to the waiting
  room, a waiting player who reconnects within the logout time, 100 players.
- Keys and tools the test harness cannot drive: the unstuck key, the minimap key, admin "Next", voice and
  push-to-talk; resolutions other than 1920x1080.
- Server and clients must run 1.6.0. `@BattleRoyale-VPP` is unchanged: keep the 1.1.0 one.

## 1.5.0 (2026-11-14)

### Added
- Serial matches (`serial_matches` in `server_settings.json`, off by default): when a match ends the server
  stays up and the next match follows in the same run. The players still connected (winner, spectators)
  get a new character in the lobby without leaving the server; the bodies with their inventory, the items
  players dropped on the ground, the spectators' bodies and the win fireworks are deleted a few per update,
  from what the match recorded (no map scan); zones, weather and time are drawn again; the lobby's minimum
  wait counts from its opening. A turnover took 29-57 ms in tests. `serial_restart_every_matches` (6 by
  default, 0 = never) exits after that many matches so the restart loop can wipe the world; a turnover that fails or takes over
  120 s exits as before. Loot is left to the Central Economy. Ignored with `enable_vigrid_api` or
  `use_autolock`. With the setting off the server exits after every match as before.
- The winner gets fireworks instead of ten chickens: up to four vanilla launchers 8 m away, lit one after
  another; a launcher that lands under a roof is left out, as the vanilla one cannot be lit there.

### Changed
- Notifications start lower (at 12.8 % of the screen height instead of 5 %), under the phase and zone
  damage lines of the HUD that they used to cover.

### Not tested yet
- With several players: a spectator going back to the lobby with its camera and anchor body, a player who
  connects during a turnover, and a winner who reconnects during the logout timer; serial matches with 100
  players. Tested alone in DayZDiag: three matches in a row in one server run, a death in a round, the
  scheduled restart, the setting off, and the fireworks.
- The retry and abort path when a lobby character cannot be created (it cannot be triggered in a test).
- `@BattleRoyale-VPP` is unchanged: keep the 1.1.0 one.

## 1.4.0 (2026-11-14)

### Changed
- Kill feed: each death now shows an icon for its cause, a sentence that names killer and victim, and the
  weapon, the distance and "headshot" next to it. The server tells apart firearm, melee, fists, explosive,
  run over, vehicle crash, fall, zone, bleed out, infected, animal, suicide, drowning, fire and barbed
  wire. Six rows, newest on top, 6 s each; rows slide and fade in, and fade out; the rows that name the
  local player have a gold bar. Weapon names appear in the client's language. Sentences in English and
  Spanish.
- The `KillFeedEvent` RPC carries `Param7<int, string, string, string, int, int, int>` instead of
  `Param4<string, string, string, float>`: server and clients must run the same build.
- A death by a zone tick, a fall, fire, barbed wire, a vehicle or an explosion is no longer credited as a
  bleed out to the last player who hit the victim, even when the victim was low on blood.

### Performance (100 players)
- The spawn-selection heat map converts map coordinates once per redraw instead of twice per square
  (up to about 5,000 fewer engine calls per redraw while the map is panned or zoomed); a check point
  falls back to the old placement if the two ever disagree by more than 1 px.
- The per-player tick reads a flag to know whether a body is a spectator's anchor instead of scanning
  the anchor list.
- With Carim, an accepted spawn click looks the group up once instead of twice, and two trace strings
  are built only at trace level. Servers without Carim run the same code as before.

### Tools
- `tools/build.py` warns when a build from a git worktree lacks the gitignored assets (textures,
  imagesets, models) of the main checkout, and prints the command that copies them;
  `tools/sync_assets.py` also copies the VPP mod's assets.

### Not tested yet
- With several players: the kill feed with weapons, distance, headshot, melee, explosives, zone, fall
  and bleed-out credit, many deaths in a row, and the gold bar with repeated names; spectators and the
  heat map with these changes. Tested alone: a death in a match shows its row with no script error.
- The Carim branch of the group lookup (no Carim build available).
- Known and left for later: a vehicle occupant who dies in a crash while under the fatal blood level is
  still credited as a bleed out to the last attacker (as in 1.3.0); a run-over gives the driver no kill.
- `@BattleRoyale-VPP` is unchanged: keep the 1.1.0 one.

## 1.3.0 (2026-11-13)

### Changed
- DayZ Expansion is no longer needed. The mod runs with `@CF;@Dabs Framework;@BattleRoyale`, plus COT or
  VPP when used. Servers can drop `@DayZ-Expansion-Core`, `@DayZ-Expansion` and
  `@DayZ-Expansion-Navigation` from `-mod`.
- Map: the vanilla 2D map opens with the map key without a map or navigation item, shows the player's
  own position and draws the zone in force in red and the announced zone in yellow.
- Notifications go through CF's notification system, with the same title, text, colour and duration,
  and a vanilla sound (at most one per second). A new notification shows at once; with five on screen
  the oldest is removed.
- Faster restart and join: the Restart state waits 3 s instead of 10 before exiting (10 s when
  `enable_vigrid_api` is on), `restart_loop.ps1` waits 2 s instead of 10, `TimeLogin` goes from 15 to
  5 s and `serverDZ.cfg.example` sets `loginQueueConcurrentPlayers = 20`.
- The character is created directly in the lobby, so the client loads one world area: in a solo test,
  joining took 14 s instead of about 36 s.
- Placement updates in the lobby are no longer sent to every player on every join; each match state
  sends them once when it starts.

### Added
- Minimap in the HUD, top right: centred on the player (or on the camera when dead or spectating), north
  up, with the zone circles. J shows or hides it (hidden at start), Numpad + cycles four zoom levels
  from 150 m to 1.2 km.

### Removed
- The Expansion airdrop, which never ran (it needed Expansion mission scripts the mod never loaded).
  The `airdrop_*` settings are ignored; older JSON files still load.

### Performance (100 players)
- The per-player tick checks state membership in O(1) instead of walking the player list.
- Prepare validates each spawn point once instead of three or four times (about 400-600 fewer physics
  queries per 100-player match).
- Spectators reuse their identity instead of searching for it; the spectator camera caches the head
  bone.
- The kill webhook JSON is built only when the API is on; trace strings are not built when the log level
  drops them.
- The CE weapon-spawn config is read once instead of on every spawned item.
- Heat-map squares are drawn with one thick line instead of one line per pixel row.

### Not tested yet
- With several players: spectator with three or more, kill webhooks, the heat map's look, the
  notification sound and the J key for the minimap.
- `@BattleRoyale-VPP` is unchanged: keep the 1.1.0 one.

## 1.2.2 (2026-11-12)

### Added
- Dead players become spectators instead of leaving after 15 s: the camera follows a player still alive, in first person (at the eyes, along the weapon) or third person (orbit with the mouse), switched with the vanilla person view key; left and right click change player. `enable_spectator` and `spectator_delay_seconds` in `general_settings.json`.
- Late joiners are now told at once that a match is in progress, with an estimate of the minutes left: the server notifies them while they connect, before the world loads (and again on connect as a backstop), the client leaves to the main menu and shows the reason there once, and the 30 s kick stays as a safety net. While the client prepares only admins listed in `admins_steamid64` are exempt: admins known only to COT or VPP must be listed there to enter during a match.
- `@BattleRoyale-VPP` is unchanged: keep the 1.1.0 one.

## 1.2.1 (2026-11-11)

### Fixed
- Notifications no longer catch mouse clicks: a notification over the inventory or the map swallowed
  the clicks meant for what was under it.
- Closing the spawn selection no longer resets the input focus of the whole game.
- The player name label of the lobby, in the centre of the screen, is hidden when the match starts.
- `@BattleRoyale-VPP` is unchanged: keep the 1.1.0 one.

## 1.2.0 (2026-11-09)

### Changed
- Zone walls: each client builds a translucent wall from local panels along the zone edges within 500 m
  of its camera, with no server or network cost. The zone in force is red; the announced zone is yellow
  until it locks.
- The map and the spawn selection map draw the zone circles in the same colours: red for the zone in
  force, yellow for the announced zone.
- HUD: a kill counter with a skull icon, always shown under the player count, replaces the clock, whose
  time the phase text already showed. The phase text now turns orange or red when the zone is hard or
  impossible to reach in time. The arrow no longer shows "safe" inside the zone.
- Kill credit: a player who bleeds out less than 5 minutes after a hit from another player is that
  player's kill, in the kill counter and the kill feed (with the weapon and distance of the hit).
  Grenade and mine kills now also count in the kill counter.
- Zone tables: the first zone has a 3000 m radius (radii 3000, 2100, 1350, 750, 350, 150, 50) and a
  full match takes about 25 minutes instead of 32. The final-zone avoid list has 15 more places for the
  larger first zone. A server that already has a `zone_settings.json` keeps its old tables: delete the
  file to get the new defaults.
- Loading screens: two new images replace the twelve of 1.1 (DXT1, 1920x1080; `gui.pbo` goes from
  28 MB to 8.7 MB). `tools/make_loading_screens.py` builds them from `BattleRoyale_dev/assets/loading_screens`;
  the 1.1 textures are kept outside the repository in `br-assets-backup\loading_screens_1.1_dxt`.
- Target of 100 players per server: `serverDZ.cfg.example` sets `maxPlayers = 100` (was 120).
- No markers are added any more: Expansion's "You died here!" death marker is switched off whatever
  `CreateDeathMarker` says in `MapSettings.json`, and the 3D marker at the centre of the next zone is gone.
- The Spanish phase text says "LA ZONA SE CIERRA".
- `@BattleRoyale-VPP` is unchanged: keep the 1.1.0 one.

### Fixed after the adversarial review (Codex `gpt-6.1-sol`, SOUND-with-fixes, 0 P1, 3 P2, 3 P3)
- A death counts as a bleed out only when the blood is under the fatal level (`BLOOD_THRESHOLD_FATAL`, as
  vanilla's `BleedingCheckMdfr`), not whenever a wound is open: a thirst or zone death is no longer
  credited to the last shooter.
- Grenade and mine kills reach the kill counter of their thrower or owner also when that player is
  already dead but still connected.
- The zone wall works out which part of the edge is within 500 m with the real geometry; near the centre
  of a small zone (350 m and less) half of the wall was missing.
- Walls and the coloured phase text are cleared when the match is over or the local player is gone.
- The clock is also removed from the unused minimap HUD layout.

### Verified
- Build and lint gate green, with the same known false positives as 1.1.
- DayZDiag 1.30, server and client, one player: yellow and red zone walls, yellow first zone on the spawn
  map, kill counter under the player count, arrow without text, zone 1 of 3000 m, new loading screen, no
  death marker after a death; no script errors on server or client.
- Zone tables replayed offline with `zone_table_check.py`: 1495 s per full match, 1 fallback in 8000
  generations.

### Known issues
- Not seen in game yet: the circles on the map opened with M, the orange or red phase text, and the kill
  credit for bleed outs, grenades and mines (it needs two players).
- Large matches (100 players) are still being tested.

## 1.1.0 (2026-11-05, release candidate for the first player test)

The admin tool is optional: the mod runs with no admin tool, with Community Online Tools or with
VPPAdminTools.

Requires DayZ 1.30, CF, Dabs Framework, DayZ-Expansion-Core, DayZ-Expansion and
DayZ-Expansion-Navigation. Load order and admin permissions: `BattleRoyale_server/README.md`.

### Changed
- `enable_unstuck` in `general_settings.json` (default `true`) turns the F2 unstuck key off: the
  server ignores the request and the start of match message about F2 is not sent.
- Players are no longer frozen during the lobby countdown. The freeze was applied on the client
  only and made the character rubber band and slide without animation until the spawn map opened.
- The DayZDiag-only F3 debug key, which opened a spawn map that could not be used or closed, is
  removed.
- Community Online Tools is no longer required. With COT loaded, the `[BR] State Machine` module
  works as in 1.0; without it, that code is not compiled.
- The VPPAdminTools menu moved to a separate mod, `@BattleRoyale-VPP`, which requires VPPAdminTools
  and BattleRoyale so that it compiles after both. Load it after `@VPPAdminTools` and
  `@BattleRoyale`, and only with VPP. The menu still has only Next State.
- The menu music "Apocalypse" (credited to Desolation Redux, no licence found) is removed with its
  sounds PBO; the main menu plays the vanilla tracks.
- The build also packs and signs `@BattleRoyale-VPP` with the same key. The lint gate lints into an
  empty folder and allows each known false positive only by rule and file.

### Verified
- DayZDiag 1.30, server and client, with no admin tool, with COT loaded before or after
  `@BattleRoyale`, with VPPAdminTools alone and with VPPAdminTools plus `@BattleRoyale-VPP`: every
  setup compiles with no script error and the match starts.
- VPPAdminTools sets its `VPPADMINTOOLS` define in a script file, not in its config. Loaded before
  `@BattleRoyale`, the define reaches both sides: the VPP log registers `MenuBattleRoyaleManager` and
  the four `BattleRoyale:` permissions, and the client compiles the two classes of
  `@BattleRoyale-VPP` (1154 classes in the mission module with it, 1152 without).

### Known issues
- The VPP menu button and the COT module were not clicked in game: the VPP toolbar opens through an
  input the test tools cannot press. Admin actions with and without permission still need a check by
  hand.
- With VPPAdminTools loaded after `@BattleRoyale` (not tested), the server side of the VPP
  integration is not expected to see the define: no `BattleRoyale:` permission would be registered and
  every VPP admin action would be denied. Keep `@VPPAdminTools` before `@BattleRoyale`.

## 1.0.0 (2026-11-05, release candidate)

DayZ-BattleRoyale 0.1.0-Vigrid (Myst, KeganH and contributors, DSPL-SA) brought to DayZ 1.30 and
Chernarus for servers of 100-120 players, with the lowest server and client load we could get.
The server no longer trusts the client and the original author's backend is gone.

Requires DayZ 1.30, CF, Dabs Framework, DayZ-Expansion-Core, DayZ-Expansion,
DayZ-Expansion-Navigation and Community Online Tools. Load order, keys and server setup:
`BattleRoyale_server/README.md`.

### Removed
- The original backend (`api.vigrid.ovh`): matchmaking, remote lock and the match UUID check. Play in
  the main menu opens the server browser, and `-connect` works again through the vanilla
  `ConnectFromCLI` / `ConnectLaunch`.
- The obfuscated `nulledkillfeed` PBO, replaced by a killfeed of our own.
- `extra_preventweaponraise`, which replaced `CheckLiftWeapon` without calling `super`. Weapon
  obstruction now comes from `cfggameplay.json` (mode 2).
- The per-tick "taking damage" notification RPC, replaced by a client-side indicator.

### Security
- Ready-up, unstuck and spawn selection resolve the player from the sender identity and ignore
  requests made for anyone else.
- A selected spawn must be inside the world and inside the first circle plus `extra_spawn_radius`;
  one accepted selection per player every 500 ms.
- Admin actions (next state, pause, resume, airdrop) go through one authorizer that denies by
  default. With COT, the server checks `BattleRoyale.StateMachine.Update`; with VPPAdminTools loaded,
  the `BattleRoyale:*` permissions are registered and checked.
- No password in the logs. The autolock URL and the stats webhook URL are empty by default. The
  optional stats webhook (match start, winner) never blocks the server.
- Late joiners get one kick timer each instead of a single shared timer.

### Stability
- Every search loop is bounded (spawn villages, zone square, POI, next circle) and falls back to a
  valid position instead of hanging the server or calling `RequestExit`.
- Prepare has a 120 s watchdog. Players and identities that disappear during logout are skipped.
- Configuration is validated on load: an invalid value is logged and replaced by its default, and the
  server keeps running.
- The lobby no longer duplicates its objects on upgrade, POI overrides read `{ x, z }`, and the
  first-circle timer stops with the start state. A match that ends before the unlock gives the
  winner their input back.
- Start items are removed by top-level attachment, without the VM exception per player that
  `PlayerBase.RemoveAllItems` logged at Prepare.

### Performance
- Prepare handles players in batches of 8 with a short sleep between batches. The teleport safety
  check reuses its arrays and caches validated village points.
- Spawn heatmap broadcast at most once per second, only when it changed.
- Lobby godmode instead of periodic healing; the group count is cached instead of polled every 5 s.
- A death or a disconnection sends the player count and the positions once instead of twice.
- The 12 loading screens are compressed (DXT1, or DXT5 where their watermark needs alpha) instead of
  uncompressed 32-bit: `gui.pbo` goes from 126 MB to 28 MB, with no visible difference
  (`tools/compress_loading_screens.py`, `BattleRoyale_dev/reports/loading-screens-dxt1.md`).
- Client: player tags raycast every 0.15 s instead of every frame, heatmap and map circle rebuilt
  only on change, no per-frame allocations, log level read once and logs behind it.
- `extra_cardamage` scales the vanilla contact damage (0.5) instead of replacing it;
  `extra_preventplayermodifiers` disables 15 more modifiers, the sandstorm and flies ones included.

### Chernarus and mission
- Defaults for Chernarus: lobby, POIs and cities. Seven zones from 2500 m to 50 m radius, about
  32 minutes at most per match.
- The large cities of the coast cannot be the final zone (`end_avoid_city`, 130 entries). In a
  simulation of the zone generator (8000 matches) no first circle had more than 10 % of its area on
  the sea, against 19.5 % of matches with the 0.1.0 values. Edit `zone_settings.json` to change it.
- Server package in `BattleRoyale_server/`: example `serverDZ.cfg` for 120 players, 150 infected and
  no animals, `restart_loop.ps1` (moves the world storage of one instance aside and restarts after
  every match; `-InstanceId` is required) and `zone_table_check.py`.
- Loading screens and hints for Chernarus.

### Interface
- Own killfeed (five rows).
- Match phase and countdown in the HUD: lobby, spawn selection, prepare, start, round, last round,
  finished and restart. A player who connects during the lobby countdown, or an admin let in during
  a match, gets the current phase with the time left.
- Zone damage indicator while outside the zone in a round.
- Localized death messages, win screen fixes, 1.30 news carousel hidden, credits and version 1.0.0.

### Verified
- Offline script validator: no errors beyond the known false positives (client 11, server 2).
- DayZDiag 1.30, server and client, one player: lobby, count reached, spawn selection, prepare,
  start, win and restart, with no script errors on either side (commits 5ca2fec and 444b718).
- The same with the test addon `BattleRoyale_dev/tests/solo_match`, which lets one player play the
  rounds, in two matches: rounds 3 to 7 with their timers and zones (1200 m down to 50 m), zone
  damage with its HUD indicator, the last round and its final lock, a death by the zone with its
  killfeed event, then win and restart. The client received every phase in order and there were no
  script errors (commit f46fefc).
- `restart_loop.ps1`: 20 of 20 cases in a test harness, including links, a quoted `instanceId` and a
  clock rollback.

### Known issues
- Not played yet with more than one player. Kills between players, groups, late joins, the
  admin menu and the load with 100 or more players are only checked by reading the code. A test with
  20-40 players and then 100 or more is pending.
- With `enable_zone_damage` set to false the client still shows the zone damage indicator during
  the rounds.
- The Expansion airdrop has not been tested.
- Some non-fatal fallbacks inherited from 0.1.0 log with `Error()` and show up as script errors.
- The music "Apocalypse" is credited to Desolation Redux without a verified licence; check it before
  publishing.

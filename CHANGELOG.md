# Changelog

## 1.1.0 (unreleased)

The admin tool is optional: the mod runs with no admin tool, with Community Online Tools or with
VPPAdminTools.

Requires DayZ 1.30, CF, Dabs Framework, DayZ-Expansion-Core, DayZ-Expansion and
DayZ-Expansion-Navigation. Load order and admin permissions: `BattleRoyale_server/README.md`.

### Changed
- Community Online Tools is no longer required. With COT loaded, the `[BR] State Machine` module
  works as in 1.0; without it, that code is not compiled.
- The VPPAdminTools menu moved to a separate mod, `@BattleRoyale-VPP`, which requires VPPAdminTools
  and BattleRoyale so that it compiles after both. Load it after `@VPPAdminTools` and
  `@BattleRoyale`, and only with VPP. The menu still has only Next State.
- The build also packs and signs `@BattleRoyale-VPP` with the same key, and the lint gate checks it.

### Known issues
- Not tested in game yet: no admin tool, COT, and VPP with `@BattleRoyale-VPP`.
- VPPAdminTools sets its `VPPADMINTOOLS` define in a script file, not in its config, so the server
  side of the VPP integration in `@BattleRoyale` (permissions and the Next State RPC) may not see it
  unless VPP is compiled first. Load `@VPPAdminTools` before `@BattleRoyale` and check that the VPP
  log registers the `BattleRoyale:` permissions.

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
- `gui.pbo` is 126 MB because of uncompressed textures inherited from 0.1.0.
- The Expansion airdrop has not been tested.
- Some non-fatal fallbacks inherited from 0.1.0 log with `Error()` and show up as script errors.
- The music "Apocalypse" is credited to Desolation Redux without a verified licence; check it before
  publishing.

# DayZ-BattleRoyale 1.0 — server package (Chernarus)

Everything a DayZ 1.30 dedicated server needs on top of the vanilla files to run the BattleRoyale
mod on Chernarus (`chernarusplus`, 15360 m) for 100 players. It only holds what differs from the
vanilla mission `mpmissions\dayzOffline.chernarusplus` of the DayZ Server install, plus templates
and scripts. No binaries: the mods come from the Steam Workshop and from the BattleRoyale 1.0 build.

```
BattleRoyale_server\
  ADMParachute\
    ParachuteJSON.json                          measured Admiral's Parachute tuning for ADM_ParachuteBag_Fabric
  serverDZ.cfg.example                          stock serverDZ.cfg + maxPlayers 100 + enableCfgGameplayFile 1
  mpmissions\dayzOffline.chernarusplus\
    cfggameplay.json                            vanilla file, weapon obstruction mode 2 (see below)
    db\globals.xml                              vanilla file, 150 infected, no animals
  tools\
    restart_loop.ps1                            moves the world aside and restarts the server after every match
    zone_table_check.py                         offline check of the zone tables (optional, for admins)
  README.md
```

## Quick start

1. Install DayZ Server 1.30 and the mods of section 1, each with its key in `<server>\keys` (section 2).
2. Copy `@BattleRoyale` (this build) next to `DayZServer_x64.exe` and its key into `<server>\keys`.
3. Copy the mission files (section 3) and `serverDZ.cfg.example` as `serverDZ.cfg`, then set
   `hostname`, `password` and `passwordAdmin` (section 4).
4. Start the server with `tools\restart_loop.ps1` (section 5). The first start writes the mod settings
   to `<profiles>\Vigrid-BattleRoyale\` (section 6).
5. Players load the same mods in the same order, `@BattleRoyale` from the Steam Workshop
   (https://steamcommunity.com/sharedfiles/filedetails/?id=3834219016). The server must run the same
   version as the Workshop item.

A match starts once the server has been up for `min_waiting_time` (300 s) with at least two players,
in one of two ways (`lobby_settings.json`): more than `minimum_players` (10) are in the lobby and 80 % of
them are ready (F1), or by autostart, where the number of ready players needed falls from `maxPlayers`
to 2 over `autostart_delay` (750 s) after the first player joined. F2 is the unstuck key at the start of
a match; `enable_unstuck` in `general_settings.json` turns it off (on by default).
Dead players watch the players still alive until the match ends (left and right click change player,
the person view key switches first and third person); `enable_spectator` turns it off and
`spectator_delay_seconds` (5) sets the wait after a death, both in `general_settings.json`.
While a match runs, a player who connects, a player who dies with spectating off and a spectator who presses
F1 wait in the lobby (waiting room): immune, healed, kept inside the lobby and outside the match, with the
match on their HUD. With serial matches they join the next lobby; with the classic restart the restart
disconnects them like everyone. `enable_waiting_room` in `general_settings.json` turns it off (on by
default): a player who connects during a match is then told it is running and leaves. When the server will
restart after a match, players are told at the start of the last round and when the match ends.

## 1. Mods and load order

Load order for `-mod=` (client mods, every player needs them). The admin tool is optional: Community
Online Tools (COT), VPPAdminTools or none.

| # | Folder | Steam Workshop id | Key in its `keys` folder | Load it |
|---|---|---|---|---|
| 1 | `@CF` | 1559212036 | `Jacob_Mango_V3.bikey` | always |
| 2 | `@AdmiralsParachuteMod` | 2912241382 | `ADM.bikey` | always (BattleRoyale does not load without it, even with plane deployment off) |
| 3 | `@Community-Online-Tools` | 1564026768 | `Jacob_Mango_V3.bikey` | optional, admin tool |
| 4 | `@VPPAdminTools` | 1828439124 | `VPP.bikey` | optional, admin tool |
| 5 | `@BattleRoyale` | 3834219016 | the key shipped in `@BattleRoyale\keys` | always |
| 6 | `@BattleRoyale-VPP` | 3834226297 | the same key as `@BattleRoyale` | only with `@VPPAdminTools` |

DayZ Expansion is not needed: the map opens with the map key without a map item and shows the zones.

Without an admin tool:

```
-mod=@CF;@AdmiralsParachuteMod;@BattleRoyale
```

With COT, nothing else is needed:

```
-mod=@CF;@AdmiralsParachuteMod;@Community-Online-Tools;@BattleRoyale
```

With VPPAdminTools, `@VPPAdminTools` goes before `@BattleRoyale` (VPP sets its `VPPADMINTOOLS` define
in a script, which only reaches the mods compiled after it; without it the BattleRoyale permissions
are not registered and every VPP admin action is denied), and `@BattleRoyale-VPP` goes after both:

```
-mod=@CF;@AdmiralsParachuteMod;@VPPAdminTools;@BattleRoyale;@BattleRoyale-VPP
```

`@BattleRoyale` holds both the client and the server script PBOs, so nothing is needed in
`-serverMod=`. Use it only for server-only mods you add yourself. `@BattleRoyale-VPP` only adds the
BattleRoyale menu to the VPP toolbar; it requires VPPAdminTools, so load it only with VPP.

Copy every mod folder into the server folder (next to `DayZServer_x64.exe`) with exactly these names;
a name with spaces is fine, the restart script quotes it.

### Admin tools

The admin actions (next state, pause, resume, airdrop) are checked on the server and denied by
default.

- COT: the `[BR] State Machine` module shows with `BattleRoyale.StateMachine.View`; its actions need
  `BattleRoyale.StateMachine.Update`.
- VPPAdminTools with `@BattleRoyale-VPP`: the BattleRoyale button of the VPP toolbar needs
  `MenuBattleRoyaleManager`, and its Next State button needs `BattleRoyale:Next`. Grant them to a group
  in the VPP Permission Editor: VPP does not add new permissions to existing groups. The menu has no
  pause or resume button; `BattleRoyale:Pause` and `BattleRoyale:Resume` are registered but have no
  VPP button yet.
- No admin tool: nobody can skip, pause or resume a match.

Players listed in `admins_steamid64` (`general_settings.json`) can join while a match runs, and so can
players with an action permission (`BattleRoyale.StateMachine.Update` in COT, `BattleRoyale:Next`,
`:Pause` or `:Resume` in VPP). The list itself gives no admin action.

## 2. Signature keys

The stock config uses `verifySignatures = 2`: the server only accepts PBOs signed with a key found in
`<server>\keys\`. Copy into that folder the `.bikey` of every mod (table above; `Jacob_Mango_V3.bikey` is
shared by CF and COT, one copy is enough) and the key of the BattleRoyale build, which
also signs `@BattleRoyale-VPP`. The 1.0 PBOs are re-signed with the project's own key (audit EXT-18),
so the key of 0.1.0 does not apply. The key names above are those of the current Workshop versions:
check each mod's `keys` folder after an update.

## 3. Mission files

1. Take a clean copy of `mpmissions\dayzOffline.chernarusplus` from the same DayZ Server build.
2. Copy this package's `mpmissions\dayzOffline.chernarusplus\` over it (two files are replaced).

Use a mission folder that belongs to this server only: the restart loop moves its world storage aside
before every start and deletes the older copies (section 5).

What changes, and nothing else:

- `db\globals.xml`: `ZombieMaxCount` 1000 → 150 and `AnimalMaxCount` 200 → 0 (lines 31 and 3). The
  infected and animal AI of the vanilla mission is the largest server load and plays no part in a match.
- `cfggameplay.json`:
  - `PlayerData.WeaponObstructionData.staticMode` and `dynamicMode` 1 → 2 (`EWeaponObstructionMode.ALWAYS`,
    "weapon obstructs and never lifts", `scripts/3_game/cfggameplaydatajson.c`). This replaces the
    `extra_preventweaponraise` PBO, which removed the lift without obstruction and let players shoot
    through walls (audit EXT-08).
  - Three values that keep the server as it was with the file disabled, because once the file is read
    its values win over the legacy defaults:
    - `PlayerData.StaminaData.staminaWeightLimitThreshold` 6000 → 6000000: the BattleRoyale mod sets
      `GameConstants.STAMINA_WEIGHT_LIMIT_THRESHOLD` to 6000000 g (no weight stamina penalty); with the
      vanilla 6000 the file would bring the penalty back (audit SRV-03).
    - `PlayerData.disablePersonalLight` false → true: the stock `serverDZ.cfg` has
      `disablePersonalLight = 1`, but that key is only read when the file is disabled.
    - `UIData.HitIndicationData.hitIndicationPostProcessEnabled` true → false: it is false when the
      file is disabled.

`cfggameplay.json` is only read when `serverDZ.cfg` contains `enableCfgGameplayFile = 1`
(`scripts/3_game/cfggameplayhandler.c`); without it the server ignores the file and the weapon
obstruction stays in vanilla mode 1. The stock `serverDZ.cfg` does not have that key.

## 4. Server config

Copy `serverDZ.cfg.example` to `<server>\serverDZ.cfg` and set `hostname`, `password` and
`passwordAdmin`. Compared with the stock file it only changes `maxPlayers = 100` and adds
`enableCfgGameplayFile = 1`. Keep `template="dayzOffline.chernarusplus"` pointing at the mission of
section 3, and keep exactly one active `template=` and one active `instanceId` line: the world storage
folder is named after `instanceId` (`storage_1`), and the restart loop refuses a config where either is
missing or appears twice outside comments.

Optional: `BRLogLevel = 1;` (1 warn, 2 info, 3 debug, 4 trace, negative = no BattleRoyale log) sets
the mod's log level; it is read by the mod, not by the game.

## 5. Restart loop and world wipe

The mod shuts the server down when a match ends (`9_BattleRoyaleRestart.c`). Start the server with
`tools\restart_loop.ps1`; it keeps it running and starts every match on a clean world:

1. renames `mpmissions\<mission>\storage_<instanceId>`, the world storage (loot, dropped items,
   vehicles), to `storage_<instanceId>.prev-<yyyyMMdd-HHmmss>`. The server then starts with an empty
   storage, so nothing carries over from the previous match. Characters are not saved anyway: the mod
   disables player saving;
2. deletes the older `storage_<instanceId>.prev-*` folders and keeps the newest 3 (`-KeepPrevious`): the
   world of each of the last 3 matches can still be restored;
3. starts `DayZServer_x64.exe` with `-config`, `-port`, `-profiles`, `-mod`, `-serverMod` and
   `-dologs -adminlog -freezecheck`;
4. when the server exits, waits 2 s and starts again. Ctrl+C stops the loop (a running server keeps
   running in its own window).

```powershell
cd D:\DayZServer\BattleRoyale_server\tools
.\restart_loop.ps1 -ServerRoot 'D:\DayZServer' -InstanceId 1 -Mod '@CF;@Community-Online-Tools;@BattleRoyale'
```

This example uses the COT list; pass the `-mod=` list of section 1 that matches your admin tool.
Add `-WhatIf` to see what one pass would rename, delete and start without doing it. Other parameters:
`-Mission`, `-Config`, `-Profiles` (default `<server>\profiles`), `-ServerMod`, `-Port`,
`-RestartDelaySeconds`, `-KeepPrevious`, `-ExtraArgs` (`Get-Help .\restart_loop.ps1 -Full`). If Windows
blocks the script, run it with `powershell -ExecutionPolicy Bypass -File .\restart_loop.ps1 ...`.

Before renaming or deleting anything the script checks, and stops without touching any folder or
starting the server when a check fails:

- the mission folder exists and has `init.c`;
- the world storage to move is the one of `-InstanceId` (mandatory); the server config must have exactly
  one active `template=` naming the same mission and exactly one `instanceId = <n>;` at the start of a
  line equal to `-InstanceId` (lines inside `//` and `/* */` comments, and text inside quoted values, do
  not count);
- every `storage_<instanceId>.prev-*` folder is checked for links before anything is deleted, and the
  copy made by the current restart is never deleted, even if the clock went back;
- no `DayZServer_x64.exe` from this folder is running, and no `DayZServer_x64.exe` whose executable
  path cannot be read is running (a server of another account counts as unknown: start the loop from
  that account);
- `mpmissions`, the mission folder, the storage folder and every `.prev-*` folder it deletes are not
  junctions or symbolic links, the storage folders contain none, and each folder resolves physically to
  `<server>\mpmissions\<mission>\<folder>`.

The profiles folder (BattleRoyale settings and logs) is never touched. To restore the world of an earlier
match: stop the loop and the server, rename `storage_<instanceId>` away, and rename the wanted
`storage_<instanceId>.prev-*` back to `storage_<instanceId>`.

### Serial matches (no restart between matches)

With `"serial_matches": 1` in `<profiles>/Vigrid-BattleRoyale/server_settings.json` the server stays up when
a match ends, and the next match follows in the same server run:

- the players still connected (the winner, spectators) get a new character in the lobby without leaving the
  server; players who left come back to the open lobby whenever they want;
- the bodies of the match with their inventory, the items players dropped on the ground and that still lie
  there, the spectators' bodies and the winner's fireworks are deleted, a few per server update. The mod
  records up to 20,000 dropped items per match; later drops, and items put into a container of the world,
  are left to the Central Economy;
- zones, weather and time are drawn again, and the lobby's `min_waiting_time` counts from the moment the
  lobby opens.

Loot is left to the Central Economy as your mission sets it up: nothing is wiped or respawned by the mod
between serial matches. In a test, after about 2,300 loose items were deleted around Chernogorsk, the
vanilla economy had put back a third of them 8 minutes later, so after a big match the next one starts with
less loot. So that the world starts fresh now and then, the server makes a full restart after 6 matches by
default: it exits and the restart loop wipes the world as above. `"serial_restart_every_matches"` changes the
number (0 = never restart). Keep using
`restart_loop.ps1` either way: it also restarts the server if a match turnover fails, which ends in the usual
exit.

The setting is ignored while `enable_vigrid_api` or `use_autolock` is on: their match id and server lock
belong to one server run.

### Discord message when a match ends

Set `"match_end_webhook_url"` in `<profiles>/Vigrid-BattleRoyale/server_settings.json` to the URL of a
Discord webhook (channel settings > Integrations > Webhooks) and the server posts a message to that channel
at the end of every match. `"match_end_message"` is the text, with these placeholders:

- `{winner}`: the winner's name; when a team wins, the names of its players still alive, comma separated;
- `{kills}`: the kills credited to the winner in the match (a team's kills added together);
- `{players}`: the players at the start of the match;
- `{duration}`: the minutes from the start of the match to the win.

`"match_end_message_no_winner"` is posted instead when nobody wins (the last players died together or left
the server); leave it empty to post nothing then. For example:

```json
"match_end_webhook_url": "https://discord.com/api/webhooks/...",
"match_end_message": "**{winner}** won the match with {kills} kills ({players} players, {duration} min)",
"match_end_message_no_winner": ""
```

Keep the URL private: whoever has it can post to the channel. The server never sends it to the players nor
writes it to its logs. Names and mentions in the message ping nobody, and Discord refuses messages over 2000
characters. Unless the URL already sets `wait`, the server adds `wait=true` to it, so that Discord confirms each
message, and writes `Match end webhook sent` to its script log once Discord has answered. When the server exits after the match, it
waits up to 10 more seconds for a message still on its way. A server that ran an earlier version gets the three
fields, with an empty URL, added to its `server_settings.json` on its first start.

### Leaderboard

Players score points in every match: for each kill, and for their final position. The points are kept in
`<profiles>/Vigrid-BattleRoyale/leaderboard.json`, next to `server_settings.json`, and the settings are in
that same `server_settings.json`:

```json
"leaderboard_points_first": 10,
"leaderboard_points_step": 2,
"leaderboard_points_per_kill": 1,
"leaderboard_min_players_for_position": 20,
"leaderboard_wipe": "monthly",
"leaderboard_webhook_url": ""
```

- `"leaderboard_points_first"` is what 1st place gets, `"leaderboard_points_step"` how much less each
  later place gets (never below 0), and `"leaderboard_points_per_kill"` what each kill adds. With 10
  and 2 the first five places get 10, 8, 6, 4, 2 points, plus 1 per kill with the default kill value.
- `"leaderboard_min_players_for_position"` (20 by default): a match that starts with fewer players
  gives no position points, only the kill points; the win still counts. 0 scores positions always.
- A kill is the one the kill counter of the HUD counts. In teams, every player gets the points of the
  position they held when they left the match (died or disconnected); the winners share 1st place.
- `"leaderboard_wipe"` is `"monthly"` by default: the board starts over on the 1st of the month.
  `"weekly"` starts over on Monday, `"yearly"` on 1 January, always by the server's local date. The
  board that ends is kept next to it as `leaderboard_<period>.json`, and a `leaderboard.json` that
  cannot be read is copied to `leaderboard.broken.json` instead of being overwritten.
- Press T in game to show or hide the panel with the top 30 (position, name, points, kills, wins).
  The key can be changed in the game's control settings, in the BATTLE ROYALE group. The panel takes
  no input: the player keeps playing with it open.
  It names the season and the time it has left, and a player outside the top 30 sees their own place
  under the table.
- `"leaderboard_webhook_url"` is a Discord webhook like the one above; empty (the default) posts
  nothing. After a match, the server posts the top 30 there only if it changed since the last post
  Discord accepted. Keep the URL private, as with the match end one.

A server that ran an earlier version gets the new fields, with the defaults above, added to its
`server_settings.json` on its first start.

Each of the three point settings must be between 0 and 1000; a value outside that range is logged
and its default is used. Saving is recoverable rather than atomic, and no write or copy is trusted
until it has been read back: the new board is written to `leaderboard.json.new` and read back first,
and only a `leaderboard.json` already known to be good (it loaded whole, or the last save read it
back whole) is copied to `leaderboard.json.bak` and read back, so an unreadable main file never
overwrites the backup. Then `leaderboard.json.new` is copied over `leaderboard.json` and read back;
if that fails, the verified `leaderboard.json.new` is used on the next load. On load the first of
`leaderboard.json`, `leaderboard.json.new` and `leaderboard.json.bak` that reads whole is used; when
that is `leaderboard.json.new` or `leaderboard.json.bak`, the unreadable `leaderboard.json` is first
copied to `leaderboard.broken.json` (or the next free `leaderboard.broken` name), then the recovered
board is copied onto `leaderboard.json` and read back, so it lives in two places before any later
save. When no file reads whole, every unreadable file is copied to a `leaderboard.broken` name and
the board starts empty, or, if a copy fails, nothing is saved for the rest of that run. At a period
change the board in memory is written straight to its `leaderboard_<period>.json` archive and read
back before it is emptied; if that fails, the old board is kept and the match that just ended is not
scored into it.

## 6. BattleRoyale settings

### Admiral's Parachute configuration

Since 2.0.1, BattleRoyale sets the drop-plane parachute speed and acceleration directly from
`general_settings.json` (`parachute_fall_speed_multiplier`, `parachute_forward_speed_multiplier`,
`parachute_accel_multiplier`; set any key to `0` to fall back to Admiral's JSON). Admiral's
`ParachuteJSON.json` therefore no longer decides the drop-plane parachute speed, though it still
controls damage settings (`ParachuteDamage`, `LandingSpeedDamage`) and remains shipped in this
package.

BattleRoyale never writes or modifies Admiral's Parachute config file. Before starting the server with
plane deployment enabled (`legacy_spawn: 0` in `general_settings.json`, the default; set `legacy_spawn: 1`
for classic spawn selection and ground start), copy
`ADMParachute\ParachuteJSON.json` from this package into `<profiles>\ADMParachute\ParachuteJSON.json`
(`$profile:ADMParachute\ParachuteJSON.json`) so Admiral's uses the intended damage settings
(`ParachuteDamage: 2`, `LandingSpeedDamage: 1`).

On the first start the mod writes its settings to `<profiles>\Vigrid-BattleRoyale\`
(`general_settings.json`, `lobby_settings.json`, `spawns_settings.json`, `zone_settings.json`,
`pois_settings.json`, `server_settings.json`), with the 1.0 defaults for Chernarus. Files with the same
names in `mpmissions\<mission>\Vigrid-BattleRoyale\` override the profile values (optional).

Upgrading a server that ran 0.1.0: delete the old `general_settings.json`, `zone_settings.json`,
`pois_settings.json`, `spawns_settings.json` and `lobby_settings.json` from the profile folder before the
first start, or the old values stay (6 zones, the example POI overrides that moved Chernogorsk and
Novodmitrovsk to the map corner, the short final-zone avoid list). In `server_settings.json` keep
`enable_vigrid_api` and `use_autolock` false (audit ZON-02, ZON-03).

The settings are checked when they load. **A broken setting never stops the server:** it is logged as an
error that names the file, the field and the value, and the default of that field, or of the whole zone
block, is used for that start. The file is not rewritten, so the error comes back on every start until
the file is fixed. What is checked:

- zone tables: `num_zones` (at least 2), `static_sizes`, `static_timers` and `min_players` with at least
  `num_zones` entries, radii positive and growing, timers positive, `min_players` not negative and not
  decreasing, `min_zone_num` between 1 and `num_zones - 1`, first zone smaller than the world. Any failure
  replaces the whole block with the defaults;
- times and radii: `round_duration_minutes` (the first zone starts `(60 * round_duration_minutes) / 2`
  seconds after the match start and must come after `time_until_teleport_unlock`),
  `spawn_selection_duration`, `spawn_selection_extra_time`, `spawn_selection_radius`,
  `debug_heal_tick_seconds`, `zone_damage_tick_seconds`, `zone_shrink_speed` (not negative),
  `min_waiting_time`, `time_to_start_match_seconds`, `autostart_delay`, the lobby `radius`,
  `extra_spawn_radius`, and the `override_poi_positions` entries.

### Default zone tables

Index 0 is the final zone, the last index the first one.

| Zone | Radius (m) | Duration (s) | Locks at (s) | Dynamic start if players > |
|---|---|---|---|---|
| 1 | 3000 | 120 | 96 | 60 |
| 2 | 2100 | 270 | 216 | 30 |
| 3 | 1350 | 250 | 200 | (latest start: zone 3) |
| 4 | 750 | 225 | 180 | |
| 5 | 350 | 180 | 144 | |
| 6 | 150 | 135 | 108 | |
| 7 | 50 | 110 | 88 | |

With `round_duration_minutes = 5` the first zone starts 150 s after the match start. When the round of
zone 7 ends, the final zone stays 55 s more and then every player left takes damage until one remains.

### Zone closing

`zone_shrink_speed` in `zone_settings.json` (metres per second, default 9; 0 = the zone jumps to the next
one at the lock, as before 1.8.0). From zone 2 on, the zone in force shrinks to the announced one instead of
jumping to it: center and radius move together, and the edge point that moves the most covers the radius
difference plus the shift of the center at that speed, faster than a running player and slower than a
vehicle. The closing ends exactly when the zone locks and starts its duration before it, so a match lasts as
long as before. Players the edge passes over are outside the zone from that moment. The zone notices count
down to the start of the closing, when the danger starts ("The zone starts closing in ..."), and so does the
HUD timer (ZONE CLOSING), which then shows the seconds until the lock while the zone closes. For the final
zone the HUD timer (ZONE DISAPPEARS IN) still counts down to the moment it is gone. The first zone of the
match still appears at its lock, there is no zone before it. The final zone also shrinks to nothing on its center at the same speed and is gone when it
collapses today. If a closing does not fit before the lock, it starts with the round and the lock and the
end of the round come that much later.

With the default tables the closing lasts (the shift of the center is random between 25 % and 75 % of the
radius difference):

| Closing | Radius (m) | Closing at 9 m/s (s) | Starts after the round start (s) | Lock (s) |
|---|---|---|---|---|
| 1 to 2 | 3000 to 2100 | 125 to 175 | 41 to 91 | 216 |
| 2 to 3 | 2100 to 1350 | 104 to 146 | 54 to 96 | 200 |
| 3 to 4 | 1350 to 750 | 83 to 117 | 63 to 97 | 180 |
| 4 to 5 | 750 to 350 | 56 to 78 | 66 to 88 | 144 |
| 5 to 6 | 350 to 150 | 28 to 39 | 69 to 80 | 108 |
| 6 to 7 | 150 to 50 | 14 to 19 | 69 to 74 | 88 |
| final | 50 to 0 | 6 | 49 | 55 |
A full match takes about 25 minutes. The first zone is shown from the start, so players get about
4.5 minutes in it before zone 2 is announced. Every zone closes faster than in 1.1; the early ones deal
little damage (zone k deals k/7 of `zone_damage_delta`). The final-zone avoid list
(`end_avoid_city`) leaves out the lobby, the islands, the places too close to the map edge for the larger
zones to fit around them, and the coastal places that would put sea in the first zone; it is tuned for
these radii. After changing `static_sizes` or the list, check them with:

```
python tools\zone_table_check.py --world-config "P:\DZ\worlds\chernarusplus\world\config.cpp" --zone-json "<profiles>\Vigrid-BattleRoyale\zone_settings.json" --num-zones 7 --per-poi 200
```

The `zone_settings.json` the mod writes is enough; the mod sources are only needed without it
(`--zone-data`). The tool needs the unpacked game data (`P:\DZ\...`), Python 3, numpy and Pillow, and
replays the zone generator offline: how often it would give up (the server then logs a warning and uses
a fallback position instead of stopping, as 0.1.0 did), how much of the first zone falls on the sea, and
which places cause it.

## 7. Not in this package

- `types.xml`, events and vehicles of the vanilla mission are unchanged (loot volume and vehicles still
  to be reviewed for 100 players).
- Load figures with 100 or more players: the 1.0 was only played with one player so far.

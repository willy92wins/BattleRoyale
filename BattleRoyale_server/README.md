# DayZ-BattleRoyale 1.0 — server package (Chernarus)

Everything a DayZ 1.30 dedicated server needs on top of the vanilla files to run the BattleRoyale
mod on Chernarus (`chernarusplus`, 15360 m) for 100-120 players. It only holds what differs from the
vanilla mission `mpmissions\dayzOffline.chernarusplus` of the DayZ Server install, plus templates
and scripts. No binaries: the mods come from the Steam Workshop and from the BattleRoyale 1.0 build.

```
BattleRoyale_server\
  serverDZ.cfg.example                          stock serverDZ.cfg + maxPlayers 120 + enableCfgGameplayFile 1
  mpmissions\dayzOffline.chernarusplus\
    cfggameplay.json                            vanilla file, weapon obstruction mode 2 (see below)
    db\globals.xml                              vanilla file, 150 infected, no animals
  tools\
    restart_loop.ps1                            wipes the world and restarts the server after every match
    zone_table_check.py                         offline check of the zone tables (optional, for admins)
  README.md
```

## 1. Mods and load order

Load order for `-mod=` (client mods, every player needs them):

| # | Folder | Steam Workshop id | Key in its `keys` folder |
|---|---|---|---|
| 1 | `@CF` | 1559212036 | `Jacob_Mango_V3.bikey` |
| 2 | `@Dabs Framework` | 2545327648 | `dab.bikey` |
| 3 | `@DayZ-Expansion-Core` | 2291785308 | `Expansion.bikey` |
| 4 | `@DayZ-Expansion` | 2116151222 | `Expansion.bikey` |
| 5 | `@DayZ-Expansion-Navigation` | 2792984722 | `Expansion.bikey` |
| 6 | `@Community-Online-Tools` | 1564026768 | `Jacob_Mango_V3.bikey` |
| 7 | `@BattleRoyale` | (1.0 build) | the key shipped in `@BattleRoyale\keys` |

```
-mod=@CF;@Dabs Framework;@DayZ-Expansion-Core;@DayZ-Expansion;@DayZ-Expansion-Navigation;@Community-Online-Tools;@BattleRoyale
```

`@BattleRoyale` holds both the client and the server script PBOs, so nothing is needed in
`-serverMod=`. Use it only for server-only mods you add yourself.

Copy every mod folder into the server folder (next to `DayZServer_x64.exe`) with exactly these names;
a name with spaces (`@Dabs Framework`) is fine, the restart script quotes it.

## 2. Signature keys

The stock config uses `verifySignatures = 2`: the server only accepts PBOs signed with a key found in
`<server>\keys\`. Copy into that folder the `.bikey` of every mod (table above; `Expansion.bikey` and
`Jacob_Mango_V3.bikey` are shared, one copy is enough) and the key of the BattleRoyale 1.0 build. The
1.0 PBOs are re-signed with the project's own key (audit EXT-18), so the key of 0.1.0 does not
apply. The key names above are those of the current Workshop versions: check each mod's
`keys` folder after an update.

## 3. Mission files

1. Take a clean copy of `mpmissions\dayzOffline.chernarusplus` from the same DayZ Server build.
2. Copy this package's `mpmissions\dayzOffline.chernarusplus\` over it (two files are replaced).

Use a mission folder that belongs to this server only: the restart loop deletes its world storage
before every start (section 5).

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
`passwordAdmin`. Compared with the stock file it only changes `maxPlayers = 120` and adds
`enableCfgGameplayFile = 1`. Keep `template="dayzOffline.chernarusplus"` pointing at the mission of
section 3, and keep `instanceId`: the world storage folder is named after it (`storage_1`).

Optional: `BRLogLevel = 1;` (1 warn, 2 info, 3 debug, 4 trace, negative = no BattleRoyale log) sets
the mod's log level; it is read by the mod, not by the game.

## 5. Restart loop and world wipe

The mod shuts the server down when a match ends (`9_BattleRoyaleRestart.c`). Start the server with
`tools\restart_loop.ps1`; it keeps it running and starts every match on a clean world:

1. deletes `mpmissions\<mission>\storage_<instanceId>`, the world storage (loot, dropped items,
   vehicles), so nothing carries over from the previous match. Characters are not saved anyway: the
   mod disables player saving;
2. starts `DayZServer_x64.exe` with `-config`, `-port`, `-profiles`, `-mod`, `-serverMod` and
   `-dologs -adminlog -freezecheck`;
3. when the server exits, waits 10 s and starts again. Ctrl+C stops the loop (a running server keeps
   running in its own window).

```powershell
cd D:\DayZServer\BattleRoyale_server\tools
.\restart_loop.ps1 -ServerRoot 'D:\DayZServer' -Mod '@CF;@Dabs Framework;@DayZ-Expansion-Core;@DayZ-Expansion;@DayZ-Expansion-Navigation;@Community-Online-Tools;@BattleRoyale'
```

Add `-WhatIf` to see what one pass would delete and start without doing it. Other parameters:
`-Mission`, `-Config`, `-Profiles` (default `<server>\profiles`), `-ServerMod`, `-Port`,
`-RestartDelaySeconds`, `-ExtraArgs` (`Get-Help .\restart_loop.ps1 -Full`). If Windows blocks the
script, run it with `powershell -ExecutionPolicy Bypass -File .\restart_loop.ps1 ...`.

Before deleting anything the script checks that the mission folder exists and has `init.c`, that
`template=` in the server config names the same mission, that the config defines `instanceId`, that no
server from this folder is running, and that the storage folder is not a junction or symbolic link and
contains none. If a check fails it stops without deleting or starting anything. The profiles folder
(BattleRoyale settings and logs) is never touched.

## 6. BattleRoyale settings

On the first start the mod writes its settings to `<profiles>\Vigrid-BattleRoyale\`
(`general_settings.json`, `lobby_settings.json`, `spawns_settings.json`, `zone_settings.json`,
`pois_settings.json`, `server_settings.json`), with the 1.0 defaults for Chernarus. Files with the same
names in `mpmissions\<mission>\Vigrid-BattleRoyale\` override the profile values (optional).

Upgrading a server that ran 0.1.0: delete the old `general_settings.json`, `zone_settings.json`,
`pois_settings.json`, `spawns_settings.json` and `lobby_settings.json` from the profile folder before the
first start, or the old values stay (6 zones, the example POI overrides that moved Chernogorsk and
Novodmitrovsk to the map corner, the short final-zone avoid list). In `server_settings.json` keep
`enable_vigrid_api` and `use_autolock` false (audit ZON-02, ZON-03).

The settings are checked when they load. An invalid value is logged as an error that names the field and
the value, and the default is used for that match (the file is not rewritten):

- zone tables: `num_zones` (at least 2), `static_sizes`, `static_timers` and `min_players` with at least
  `num_zones` entries, radii positive and growing, timers positive, `min_players` not negative and not
  decreasing, `min_zone_num` between 1 and `num_zones - 1`, first zone smaller than the world. Any failure
  replaces the whole block with the defaults;
- times and radii: `round_duration_minutes` (the first zone starts `(60 * round_duration_minutes) / 2`
  seconds after the match start and must come after `time_until_teleport_unlock`),
  `spawn_selection_duration`, `spawn_selection_extra_time`, `spawn_selection_radius`,
  `debug_heal_tick_seconds`, `zone_damage_tick_seconds`, `min_waiting_time`,
  `time_to_start_match_seconds`, `autostart_delay`, the lobby `radius`, `extra_spawn_radius`, and the
  `override_poi_positions` entries.

### Default zone tables

Index 0 is the final zone, the last index the first one.

| Zone | Radius (m) | Duration (s) | Locks at (s) | Dynamic start if players > |
|---|---|---|---|---|
| 1 | 2500 | 240 | 192 | 60 |
| 2 | 1800 | 330 | 264 | 30 |
| 3 | 1200 | 300 | 240 | (latest start: zone 3) |
| 4 | 700 | 270 | 216 | |
| 5 | 350 | 225 | 180 | |
| 6 | 150 | 180 | 144 | |
| 7 | 50 | 135 | 108 | |

With `round_duration_minutes = 5` the first zone starts 150 s after the match start. When the round of
zone 7 ends, the final zone stays 67 s more and then every player left takes damage until one remains.
A full match takes about 32 minutes. The final-zone avoid list
(`end_avoid_city`) leaves out the lobby, the islands, the places too close to the map edge for the larger
zones to fit around them, and the coastal places that would put sea in the first zone; it is tuned for
these radii. After changing `static_sizes` or the list, check them with:

```
python tools\zone_table_check.py --world-config "P:\DZ\worlds\chernarusplus\world\config.cpp" --zone-json "<profiles>\Vigrid-BattleRoyale\zone_settings.json" --num-zones 7 --per-poi 200
```

It needs the unpacked game data (`P:\DZ\...`), Python 3, numpy and Pillow, and replays the zone generator
offline: how often it would give up (the server then stops at boot), how much of the first zone falls on
the sea, and which places cause it.

## 7. Not in this package

- `types.xml`, events and vehicles of the vanilla mission are unchanged (loot volume and vehicles still
  to be reviewed for 120 players).
- The `extra_preventweaponraise` PBO still has to be removed from the mod build for mode 2 to take
  effect: its script replaces the vanilla lift check.

# parse_timings.py

Per-stage timings of server boot and player join, read from DayZ script logs. Standard library only.

```
python -I BattleRoyale_dev/tests/timing/parse_timings.py --server <server script_*.log> [--client <client script_*.log>] [--loop <restart_loop log>] [--json]
python -I BattleRoyale_dev/tests/timing/parse_timings.py --compare before.json after.json
```

Each table lists the mark, its time, `delta_s` (seconds since the previous mark) and `from_start` (seconds since the log start). In `--json`, `from` names the mark that `delta_s` is measured from. Marks that do not appear are listed under `missing`; nothing is estimated. Repeated marks are numbered (`#1`, `#2`).

- **Server:** `Module: GameLib/Game/World/Mission`, `OnInit` (`Vigrid-BattleRoyale OnInit()`), `Debug Zone State` (`BattleRoyaleState::Activate: Debug Zone State`), `Restarting` (`[Restart State] Restarting!`).
  - `OnInit` is logged when `MissionServer::OnInit` is entered, before `super.OnInit()` and before `BattleRoyaleServer` is built (`BattleRoyale/addons/scripts_server/5_Mission/Mission/MissionServer.c:9`). It is not the end of boot; `Debug Zone State` comes later.
  - Each connection gets its own block, with id `conn:<name>#<n>` (or `conn<index>` when there is no name): `ClientPrepare` → `ClientNew` → `InvokeOnConnect` → `Connected` (`Player X connected!`) → `AddPlayer`. In this block, `delta_s` is measured from the previous stage of the same connection that is present.
- **Client:** modules, `MPSessionStart`, `LoginTime`, `MissionGameplay OnInit`, `HUD Initialized`, `Preload`, `MPSessionPlayerReady`.
- **Loop** (`restart_loop.ps1` output, `yyyy-MM-dd HH:mm:ss` with whole seconds): `Server exited`, `Start` (run number), `Storage moved`, `No storage to move`, `Storage deleted`, plus the downtime from each exit to the next start. UTF-16 files (from `>` in PowerShell 5.1) are read too.

## Pairing rules

- **ClientPrepare and ClientNew** are paired by the `PlayerIdentity<0x...>` pointer from the Community Framework debug lines `[CF] MissionServer::OnClientPrepareEvent/OnClientNewEvent`.
  - When an identity is unknown, they are paired in arrival order, and the block says `paired by order`.
  - A `ClientNew` whose identity differs from every pending `ClientPrepare` is never paired. Both stay incomplete: the `ClientNew` gets `paired by none` with `ClientPrepare` missing. A `CONFLICT` line is printed (`conflicts` in JSON).
  - A stage that comes before the previous stage is also reported as a `negative_interval` conflict.
- **AddPlayer** counts only when it is the join one. That is `BattleRoyaleState::AddPlayer: X` for the same player, after `Player X connected!` (`BattleRoyaleServer.c:284`), with no other connection trace and no state change in between. It must also come before the end of the ClientNew handler: in diag builds, `[EXTRACE] -<Mission>::OnEvent ClientNewEventTypeID <ms>` closes the join, because the join `AddPlayer` runs inside that handler.
  - A state change moves every player to the next state (`BattleRoyaleServer.c:199-208`): `Leaving State`, `Deactivate` and `RemoveAllPlayers` come first, then one `AddPlayer` per player. Those are counted under "AddPlayer outside a join" (`add_player_outside_join` in JSON) and never stand in for a missing join `AddPlayer`, which stays `missing`.

## Other limits

- The mod's trace lines (`[DayZ-BattleRoyale][n]`) only appear when the BattleRoyale log level prints them (Trace is level 4).
- Without the `started at` header, the start is the first timestamped line and the date is 1900-01-01.
- The header carries whole seconds, so `from_start` includes up to 1 s of truncation.
- When the time of day goes back by more than an hour, that is counted as a change of day.

## --compare

`--compare` takes two `--json` outputs and prints, for each stage, the A and B values and the B−A difference for `delta_s` and for `from_start`.

- A `delta_s` difference is only computed when both sides measure from the same mark. Otherwise the row shows `n/c` and names both starting marks. Older JSON without `from` always gives `n/c`.
- Stages found on only one side are marked.
- Connections are matched by id (player name and occurrence).
- The JSON does not record the conditions of the run (mods, cache, machine, load, order of joins). Equal conditions are the user's responsibility.

## Tests

`python -I -m unittest discover -s BattleRoyale_dev/tests/timing -p "test_*.py" -v`

- The real-log tests read `C:\Users\guill\Repos\br-runs\v13-f0\fixtures_logs` (another folder can be set with `BR_TIMING_FIXTURES`) and are skipped when the folder does not exist.
- Everything runs in memory except `CliCompareTest`, which needs a temporary folder and is skipped when there is none.

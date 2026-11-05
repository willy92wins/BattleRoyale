# Solo match test (local tests only)

`br_solotest` lets one player go through the rounds of a match: start, rounds, last round, zone
damage, death, win and restart. Without it a one-player match ends at the start state. It is a test
tool: it is not part of `BattleRoyale/addons` and must never be shipped.

Build and load:

1. `python -I BattleRoyale_dev/tests/solo_match/build_solotest.py <output @folder>` packs and signs
   it with the same key as the mod.
2. Load that folder after `@BattleRoyale` in `-mod` (local server and client).
3. Shorten the timers in the test profile only: `round_duration_minutes` 1 and `spawn_selection_duration`
   10 in `general_settings.json`, `static_timers` 40 for every zone in `zone_settings.json`.
   With one player the match starts in zone 3 (`num_zones - min_zone_num`), so it plays rounds 3 to 6
   and the last round, about 5 minutes in all.

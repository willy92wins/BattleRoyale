# Admiral's Parachute configuration

Copy `ParachuteJSON.json` to `<profiles>\ADMParachute\ParachuteJSON.json`
(`$profile:ADMParachute\ParachuteJSON.json`).

Since BattleRoyale 2.0.1, drop-plane parachute speed and acceleration are set directly by
BattleRoyale from `general_settings.json` (`parachute_fall_speed_multiplier`,
`parachute_forward_speed_multiplier`, `parachute_accel_multiplier`; `0` keeps the value from
Admiral's JSON). `ParachuteJSON.json` is still shipped because Admiral's Parachute reads damage
settings (`ParachuteDamage`, `LandingSpeedDamage`) from it.

# Lane extras

## EXT-02 / EXT-03 requiredAddons

`BattleRoyale/addons/scripts_client/config.cpp:7-14` ahora exige, además de `DZ_Data`, `DZ_Scripts` y `DayZExpansion_Scripts`: `JM_CF_Scripts`, `DF_Scripts`, `DayZExpansion_Core_Scripts`, `DayZExpansion_Navigation_Scripts` y `JM_COT_Scripts`. No se añadió VPP.

Nombres leídos en disco:

- `JM_CF_Scripts`: `C:\Users\guill\Repos\br-ref\cf_scripts\config.cpp:3`
- `DF_Scripts`: `C:\Users\guill\Repos\br-ref\dabs_scripts\config.cpp:3`
- `DayZExpansion_Core_Scripts`: `C:\Users\guill\Repos\br-ref\exp_core_scripts\config.cpp:3` (sus `requiredAddons` incluyen `DF_Scripts` en la línea 11)
- `DayZExpansion_Navigation_Scripts`: `C:\Users\guill\Repos\br-ref\navigation_scripts\config.cpp:3`
- `JM_COT_Scripts`: `C:\Users\guill\Repos\br-ref\cot\scripts\config.cpp:3`
- Parches BR: `BattleRoyale_Scripts_Client` en `scripts_client\config.cpp:3`; `BattleRoyale_Scripts_Server` en `scripts_server\config.cpp:3`

`extra_limitunconscioustime\config.cpp:10-12` requiere `BattleRoyale_Scripts_Client` (`BattleRoyaleUtils`) y `BattleRoyale_Scripts_Server` (`PlayerBase.m_UnconsciousStartTime`, `scripts_server\4_World\Entities\ManBase\PlayerBase.c:24`).

El resto de extras propios no usa símbolos de BR. Se dejan con `DZ_Scripts`. `extra_changefeedbackurl` sí usa `GITHUB_URL` y no es de esta lane.

Verificado por lectura de esos `config.cpp`. No se arrancó el motor, así que el orden real de carga no está probado.

## EXT-04 extra_cardamage

Nuevo addon `BattleRoyale/addons/extra_cardamage/`:

- `$PBOPREFIX$`: `Vigrid-BattleRoyale\Extra\CarDamage`
- `config.cpp`: CfgPatches `vigrid_car_damage` con `DZ_Data`, `DZ_Scripts`, `DZ_Vehicles_Wheeled`; CfgMods `worldScriptModule` → `Vigrid-BattleRoyale/Extra/CarDamage/Scripts/4_World`
- `Scripts/4_World/CarScript.c:3-8`: `modded class CarScript`, `override void EEInit()` (firma `void EEInit()` en `scripts\3_game\entities\entityai.c:905`), `m_dmgContactCoef *= 0.5` después de `super`

`DZ_Vehicles_Wheeled` existe en `C:\Users\guill\OneDrive\Documentos\DayZ Projects\DZ\vehicles\wheeled\config.cpp:3`.

Vanilla asigna `m_dmgContactCoef = 0.058` en `void CarScript()` (`scripts\4_world\entities\vehicles\carscript.c:201` y `:224`). `Sedan_02()` lo vuelve a poner a `0.0790` (`inheritedcars\sedan_02.c:7-9`) y su `EEInit` llama a `super.EEInit()` (`sedan_02.c:41-43`). Por eso el factor va en `EEInit` y no en el constructor: un constructor `modded` de `CarScript` corre antes del cuerpo de `Sedan_02` y el sedán pisaría el 0.5. No se toca `OnContact`. Campo `protected` en `scripts\3_game\vehicles\transport.c:91`.

No se midió el daño in-game. No se escala `MotorbikeScript` (no lo pedía el encargo).

## EXT-06 / fila #12 preventplayermodifiers

Un fichero por modificador, `ActivateCondition` devuelve `false`, misma firma que vanilla:

| Clase | Firma vanilla | Fichero nuevo (método en línea 3) |
| --- | --- | --- |
| HeatStrokePhase1Mdfr | `override protected bool ActivateCondition(PlayerBase player)` `heatstroke.c:59` | `...\Diseases\HeatStrokePhase1.c` |
| HeatStrokePhase2Mdfr | `heatstroke.c:101` | `HeatStrokePhase2.c` |
| HeatStrokePhase3Mdfr | `heatstroke.c:166` | `HeatStrokePhase3.c` |
| HeatStrokePhase4Mdfr | `heatstroke.c:262` | `HeatStrokePhase4.c` |
| IrritatedEyesMdfr | `irritatedeyes.c:19` | `IrritatedEyes.c` |
| SilicosisMdfr | `silicosis.c:36` | `Silicosis.c` |
| HeavyMetalPhase1Mdfr | `heavymetal.c:43` | `HeavyMetalPhase1.c` |
| HeavyMetalPhase2Mdfr | `heavymetal.c:103` | `HeavyMetalPhase2.c` |
| HeavyMetalPhase3Mdfr | `heavymetal.c:190` | `HeavyMetalPhase3.c` |
| CholeraMdfr | `cholera.c:31` | `Cholera.c` |
| SalmonellaMdfr | `salmonella.c:35` | `Salmonella.c` |
| BrainDiseaseMdfr | `braindisease.c:25` | `BrainDisease.c` |
| SandstormExposureStaticMdfr | `override bool ActivateCondition(PlayerBase player)` `sandstormexposure.c:66` (vanilla ya devuelve false y desactiva el check en `:62-63`) | `...\Conditions\SandstormExposureStatic.c` |
| SandstormExposureDynamicMdfr | `sandstormexposure.c:101` | `SandstormExposureDynamic.c` |
| FliesMdfr | `override bool ActivateCondition(PlayerBase player)` `flies.c:20` (vanilla ya devuelve false) | `...\Modifiers\Flies.c` |

Ruta base: `BattleRoyale/addons/extra_preventplayermodifiers/Scripts/4_World/Classes/PlayerModifiers/Modifiers/`. No se tocó `HeatStrokeMdfr` base: el encargo nombra las fases 1-4. El lint offline pasó con 0 errores; el compilador del juego no se ejecutó.

## EXT-12 / EXT-13 munición

`extra_spawnwithammoandmagazine\Scripts\4_World\Entities\ItemBase.c`:

- Quitados los `Print` del cargador (antes ~63-75) y no se dejó log por arma.
- `:68-70`: si `CreateObjectEx` del cargador devuelve null, `continue` (no cuenta como spawneado).
- `:75-93`: el aleatorio de pilas de munición se calcula una vez (`ammoSpawnCount`); `array` y `ConfigGetTextArray` salen del bucle; `if (!ammoPile) continue` antes de `GetEconomyProfile`.

`EEOnCECreate` de `ItemBase`: `scripts\4_world\entities\itembase.c:4111` (`override void EEOnCECreate()`).

`extra_spawnweaponchambered\Scripts\4_World\Entities\Firearms\Weapon_Base.c:4-10`: el trabajo pasa de `EEInit` a `EEOnCECreate` y solo llama a `FillChamber( "", WeaponWithAmmoFlags.CHAMBER )`. Se quitó `SpawnAmmo()`, que acoplaba otro cargador (`weapon_base.c:740`). `FillChamber`: `weapon_base.c:921` `bool FillChamber( string ammoType = "", int flags = WeaponWithAmmoFlags.CHAMBER )`. `EEOnCECreate` no está redefinido en `scripts\4_world\entities\firearms\` (búsqueda vacía); lo hereda el arma desde `ItemBase` (`:4110` lo documenta como alta de CE/debug). `Weapon_Base.EEInit` sigue en vanilla `weapon_base.c:135` y ya no se pisa.

No conté entidades al arrancar una partida.

## EXT-14 cache de inconsciencia

`Unconsciousness.c:8-30`: `s_DisableCached` / `s_DisableUnconsciousness` estáticos. `ServerConfigGetInt` e `IsCLIParam` (`scripts\1_core\proto\ensystem.c:141`, `proto native bool IsCLIParam(string param)`) se leen una vez por proceso, no en cada tick. Firmas que se mantienen: `override void Init()` (`unconsciousness.c:5`), `override bool ActivateCondition(PlayerBase player)` (`:14`), `override bool DeactivateCondition(PlayerBase player)` (`:46`).

## EXT-08 preventweaponraise

`git rm -r BattleRoyale/addons/extra_preventweaponraise` desde la raíz del worktree. El `cfggameplay.json` de la misión lo deja otra lane.

## Lint

`bash tools/lint.sh . build/lint` (exit 0):

```
extra_cardamage PASS errors 0 warnings 0
extra_changefeedbackurl PASS errors 0 warnings 0
extra_defaultfullauto PASS errors 0 warnings 0
extra_limitunconscioustime PASS errors 0 warnings 0
extra_preventplayermodifiers PASS errors 0 warnings 0
extra_spawncarfull PASS errors 0 warnings 0
extra_spawnweaponchambered PASS errors 0 warnings 0
extra_spawnwithammoandmagazine PASS errors 0 warnings 0
extra_spawnwithbattery PASS errors 0 warnings 0
extra_vehicle3ppdefaultfix PASS errors 0 warnings 0
scripts_client FAIL errors 10 warnings 3
scripts_server FAIL errors 2 warnings 12
```

La base de `common.md` es scripts_client 10 errores (ES-LAYOUT-PATH-PBOPREFIX-MISMATCH) y scripts_server 2 (ES-OVERRIDE-PARAM-NAME-MISMATCH en RemovePlayer). El conteo no sube. `extra_preventweaponraise` ya no sale porque el PBO se retiró. No reabrí el detalle de esos 12 errores de base en esta pasada: el script solo imprime el resumen.

## CROSS-LANE REQUESTS

- `extra_changefeedbackurl` usa `GITHUB_URL` (`scripts_client\2_GameLib\BattleRoyaleConstants.c`) y su `config.cpp` sigue exigiendo solo `DZ_Scripts`. Esta lane no puede editar esa carpeta. Quien la tenga debería añadir `BattleRoyale_Scripts_Client`.
- EXT-08: la misión debe dejar `WeaponObstructionData` static/dynamic en modo ALWAYS. No está en este worktree de addons.

## LO QUE NO PUDE VERIFICAR

- Compilación y carga con el motor DayZ 1.30 (prohibido lanzar el juego).
- Que `EEOnCECreate` en `Weapon_Base` no deje fuera armas creadas por script (kits). Es el comportamiento pedido para la economía; un kit de inicio ya no sale amartillado por este extra.
- Daño de choque real con factor 0.5, y motos (`MotorbikeScript` no hereda `CarScript`).
- Que el estático de inconsciencia se lea después de que `serverDZ.cfg` esté disponible. Se lee en el primer `ActivateCondition`, no en el arranque del módulo.
- Los 12 errores concretos del resumen de lint de scripts_client/server más allá del conteo y de la base escrita en `common.md`.

## Ficheros

Creados:

- `BattleRoyale/addons/extra_cardamage/$PBOPREFIX$`
- `BattleRoyale/addons/extra_cardamage/config.cpp`
- `BattleRoyale/addons/extra_cardamage/Scripts/4_World/CarScript.c`
- `BattleRoyale/addons/extra_preventplayermodifiers/Scripts/4_World/Classes/PlayerModifiers/Modifiers/Diseases/HeatStrokePhase1.c`
- `.../HeatStrokePhase2.c`, `HeatStrokePhase3.c`, `HeatStrokePhase4.c`
- `.../IrritatedEyes.c`, `Silicosis.c`, `Cholera.c`, `Salmonella.c`, `BrainDisease.c`
- `.../HeavyMetalPhase1.c`, `HeavyMetalPhase2.c`, `HeavyMetalPhase3.c`
- `.../Conditions/SandstormExposureStatic.c`, `SandstormExposureDynamic.c`
- `.../Modifiers/Flies.c`
- `BattleRoyale_dev/reports/extras.md`

Modificados:

- `BattleRoyale/addons/scripts_client/config.cpp`
- `BattleRoyale/addons/extra_limitunconscioustime/config.cpp`
- `BattleRoyale/addons/extra_limitunconscioustime/Scripts/4_World/Classes/PlayerModifiers/Modifiers/Unconsciousness.c`
- `BattleRoyale/addons/extra_spawnwithammoandmagazine/Scripts/4_World/Entities/ItemBase.c`
- `BattleRoyale/addons/extra_spawnweaponchambered/Scripts/4_World/Entities/Firearms/Weapon_Base.c`

Eliminados (`git rm -r`):

- `BattleRoyale/addons/extra_preventweaponraise/` (`$PBOPREFIX$`, `config.cpp`, `Scripts/4_World/Entities/ManBase/PlayerBase.c`)

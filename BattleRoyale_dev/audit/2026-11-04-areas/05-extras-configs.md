# Auditoría 05: addons extra, terceros, configs y empaquetado (DayZ-BattleRoyale 0.1.0-Vigrid → DayZ 1.30)

Auditoría de solo lectura. Rutas abreviadas:
- `SRC` = `C:\Users\guill\OneDrive\Documentos\DayZ Projects\BattleRoyale_dev\extracted_v0.1.0-vigrid\`
- `VAN` = `C:\Users\guill\OneDrive\Documentos\DayZ Projects\scripts\` (vanilla 1.30); `VAN129` = `...\scripts_1.29_backup_20261016\`
- `REF` = `C:\Users\guill\AppData\Local\Temp\claude\72e0954b065197d7\151ed9c3-2d0a-403f-9ce8-abb72cfff50c\scratchpad\ref\`
- `WS` = `C:\Program Files (x86)\Steam\steamapps\workshop\content\221100\2871877303\` (el mod publicado, con sus `.bisign` y `keys\`)

## Resumen

- **Ningún extra tiene un error de compilación probado contra 1.30.** Pasé el validador offline (`dayz_script_validator`, con `--vanilla-root` en VAN y `--external-scripts` en scripts_client y scripts_server) por los 10 extra_* y por reducecardamage: 0 errores en los 11. Avisos: 7 de `#ifdef` sin cerrar (EXT-05). Los informes están en `scratchpad\audit\lint_extras\*.txt`. Un PASS del validador no prueba que compile: no comprueba overrides de métodos que no existen ni la semántica.
- Todas las firmas que se sobrescriben existen en 1.30 con el mismo nombre y los mismos parámetros. Las cité una a una abajo.
- Los riesgos reales son cinco:
  - nulledkillfeed es un PBO ofuscado que no se puede auditar (EXT-01).
  - Falta Expansion Navigation en `requiredAddons` (EXT-02).
  - reducecardamage cambia la semántica del daño en 1.30 (EXT-04).
  - Tres terceros sin licencia o firmados con claves ajenas (EXT-18 a EXT-20).
  - Varios extras que vanilla 1.30 ya cubre con `cfggameplay.json` (EXT-07, EXT-08).

---

## 1) Necesario para 1.30

### EXT-01 nulledkillfeed: PBO de terceros ofuscado, sin fuente y con overrides de 2021 — **Alta (posible bloqueante que no se puede verificar)**
- Evidencia. Cabecera de `WS\addons\nulledkillfeed.pbo` (parseada con Python):
  - `prefix=NulledKillfeed`, dos propiedades `obfuscated`, `Mikero=DePbo.dll.8.07`, `version=21051520` (≈ mayo de 2021).
  - Hay entradas basura (`"\t\t"`, `"*.*"`) y unos 25 ficheros con nombres cirílicos y extensiones falsas (`.p3d`, `.rvmat`, `.ogg`, `.wss`, `.fxy`, `.jpg`) dentro de `scripts\4_World\...`.
  - El único `config.cpp` legible ocupa 26 bytes: `#include "<nombre cirílico>.p3d"`. La config real está escondida en un fichero disfrazado. No seguí más (no desofusqué).
  - Las entradas con nombre legible incluyen `scripts\4_World\Entities\PlayerBase.c` y `scripts\4_World\PluginBase\PluginManager.c`: hace `modded` de PlayerBase y de PluginManager con código compilado contra vanilla de 2021.
  - Ningún script de BR lo referencia: grep de `killfeed|Nulled` en scripts_client y scripts_server sin resultados.
- Riesgo:
  - Si cualquier override suyo choca con una firma de 1.30, el servidor no compila y no podemos ver ni arreglar el código.
  - Mientras compile, modifica PlayerBase sin que nadie pueda revisarlo, y eso es una superficie de seguridad y de rendimiento con 120 jugadores.
  - No tiene licencia visible y la redistribución empaquetada no consta como autorizada.
  - Va firmado con `napspublic` (`nulledkillfeed.pbo.napspublic.bisign`, `keys\napspublic.bikey`).
- Propuesta: **retirarlo**. Sustituto preferente: el killfeed de DayZ-Expansion, que ya es dependencia. El mod principal (`221100\2116151222\addons\scripts.pbo`) contiene `class ExpansionKillFeedModule` (grep `-ao` en el PBO) y además trae `0_killfeed_preload.pbo`. La alternativa es un killfeed propio mínimo: un hook en `PlayerBase.EEKilled` más una notificación. Quitarlo no rompe nada de BR porque BR no lo referencia.
- Esfuerzo: S (quitarlo) / M (killfeed propio). Confianza: alta en el riesgo; media en que la configuración del killfeed de Expansion encaje en BR.

### EXT-02 Falta Expansion Navigation en `requiredAddons` aunque se usa sin guarda — **Alta**
- Evidencia:
  - `SRC\scripts_client\config.cpp:5-10`: `requiredAddons` = DZ_Data, DZ_Scripts, DayZExpansion_Scripts.
  - `REF\expansion_scripts\config.cpp:8-11`: DayZExpansion_Scripts solo requiere `DZ_Radio` y `DayZExpansion_Core_Scripts`, no Navigation.
  - BR usa clases del mapa de Expansion sin `#ifdef EXPANSIONMODNAVIGATION`:
    - `SRC\scripts_client\5_Mission\GUI\Map\ExpansionMapMenu.c:2` (`modded class ExpansionMapMenu`)
    - `...\Map\BattleRoyaleMapMarkerPlayerArrow.c:2` (`: ExpansionMapMarker`)
    - `SRC\scripts_client\5_Mission\BattleRoyale\Client\BattleRoyaleClient.c:214` (`GetExpansionSettings().GetMap().AddServerMarker`)
  - Ni `class ExpansionMapMenu` ni la base `ExpansionMapMarker` aparecen en `REF\exp_core_scripts` ni en `REF\expansion_scripts`. Core solo tiene `ExpansionMapMarkerPlayerArrow : ExpansionMapWidgetBase` (`REF\exp_core_scripts\5_Mission\DayZExpansion_Core\ExpansionMapMarkerPlayerArrow.c:13`) y `#ifdef EXPANSIONMODNAVIGATION` (`...\ClientSettings\ExpansionClientSettings.c:162`). Por tanto viven en el módulo Navigation.
- Riesgo: sin la dependencia declarada, el orden de carga entre Navigation y `BattleRoyale_Scripts_Client` no está garantizado. Un `modded class` evaluado antes que su clase original falla al compilar o queda fuera de la cadena.
- Propuesta: añadir a `requiredAddons` el CfgPatches de Navigation (probablemente `DayZExpansion_Navigation_Scripts`; **nombre sin verificar**, ver más abajo) y documentar Navigation como dependencia dura en mod.cpp y en la descripción.
- Esfuerzo: S. Confianza: media. El PBO de Navigation no está en local: `!Workshop\@DayZ-Expansion-Navigation` es un enlace vacío y no aparece en `workshop\content\221100`. Hay que confirmar el nombre del CfgPatches antes de escribirlo.

### EXT-03 `requiredAddons` incompletos para el orden de carga (CF, COT y entre addons de BR) — **Media**
- Evidencia:
  - scripts_client usa COT bajo `#ifdef JM_COT`: `SRC\scripts_client\5_Mission\COT\JMModuleConstructor.c:1-2` (`modded class JMModuleConstructor`) y `...\COT\GUI\BRMasterControlsModule.c:1-2` (`class BRMasterControlsModule: JMRenderableModuleBase`). Sin embargo, `SRC\scripts_client\config.cpp:5-10` no lista `JM_COT_Scripts` (el nombre está verificado en `REF\cot\scripts\config.cpp:3`) ni `JM_CF_Scripts` (`REF\cf_scripts\config.cpp:3`).
  - Hoy CF entra en el orden de forma transitiva: Expansion Core → `DF_Scripts`/`ModStorage` (`REF\exp_core_scripts\config.cpp:8-12`). COT no entra.
  - Los extras solo requieren `DZ_Scripts` (por ejemplo `SRC\extra_limitunconscioustime\config.cpp:8-11`), pero dependen de símbolos de BR:
    - `GITHUB_URL`, en `SRC\scripts_client\2_GameLib\BattleRoyaleConstants.c:24` (lo usa `extra_changefeedbackurl\...\InGameMenu.c:5`)
    - `BattleRoyaleUtils.Trace` (`extra_limitunconscioustime\...\Unconsciousness.c:10`)
    - `player.m_UnconsciousStartTime`, declarado en `SRC\scripts_server\4_World\Entities\ManBase\PlayerBase.c:24` (`Unconsciousness.c:23,44`)
  - `extra_vehicle3ppdefaultfix` necesita cargar después de Vehicle3PP para quedar por encima en la cadena de `EEInit` (EXT-10).
- Propuesta:
  - scripts_client: añadir `"JM_CF_Scripts","JM_COT_Scripts"` (COT ya es dependencia declarada del proyecto).
  - Cada extra: añadir `"BattleRoyale_Scripts_Client"`, y `"BattleRoyale_Scripts_Server"` en limitunconscioustime.
  - vehicle3ppdefaultfix: añadir el CfgPatches de Vehicle3PP o retirar el extra.
- Esfuerzo: S. Confianza: alta en que falta la declaración; media en el efecto práctico, porque el mod funcionó en 1.2x con este orden implícito.

### EXT-04 reducecardamage (DaemonForge, 2019) cambia la semántica del daño de choque en 1.30 — **Alta**
- Evidencia:
  - `SRC\reducecardamage\scripts\4_World\ReduceCarDamage\ReduceCarDamage.c:3`: `override void OnContact(...)` en CarScript **sin llamar a super**.
  - En 1.30, `CarScript` ya no declara `OnContact`; el método vive en `Transport` (`VAN\3_game\vehicles\transport.c:318`) y solo llena `m_ContactCache` (`transport.c:330-341`, un contacto por tick). El daño lo aplica `CarScript.CheckContactCache()` (`VAN\4_world\entities\vehicles\carscript.c:1279-1371`, llamado en `:826`), con `DamageCrew` (`:1374`) y `ProcessDirectDamageFlags.NO_TRANSFER` (`:1346`).
  - En 1.29 `OnContact` estaba en CarScript (`VAN129\4_world\entities\vehicles\carscript.c:1454`). Por eso el override sigue compilando: el validador da PASS y el método existe en la base.
  - Efecto en 1.30:
    1. La caché vanilla nunca se llena: todo el daño de choque pasa por la lógica de 2019.
    2. Se aplica `AddHealth` en **cada** evento de contacto, sin el límite de uno por tick, así que un roce aplica daño muchas veces.
    3. Los umbrales están fijos en el código (`mindmg=1350`, `dmgThreshold=750`, `dmgKillCrew=3000`; `ReduceCarDamage.c:19-21` y `rcdsettingsloader.c:12`) y se calibraron con un coeficiente antiguo. En 1.30, `m_dmgContactCoef` vale 0.023 en Transport (`transport.c:91`) pero CarScript lo sube a 0.058 (`carscript.c:224`), y Sedan_02 a 0.0790 (`inheritedcars\sedan_02.c:9`). El mod «reduce» puede acabar **aumentando** el daño.
    4. Mata a la tripulación con `SetHealth(0.0)` (`ReduceCarDamage.c:63`), sin el `DamageCrew` vanilla.
    5. No cubre `MotorbikeScript` (nuevo en 1.30: `VAN\4_world\entities\vehicles\motorbikescript.c:53`, `class MotorbikeScript : Motorbike`) ni los barcos (`boatscript.c:426` tiene su propio `OnContact`).
- Propuesta: retirar reducecardamage y poner unas diez líneas propias: `modded class CarScript` cuyo constructor escale `m_dmgContactCoef` por un factor configurable, por ejemplo `m_dmgContactCoef *= 0.5`. Así se conserva la caché, `DamageCrew` y los umbrales `GameConstants.CARS_CONTACT_DMG_*` (`VAN\3_game\constants.c:888-890`). Si se quiere el mismo efecto en motos, añadir el análogo en MotorbikeScript.
- Esfuerzo: S. Confianza: alta en el cambio de ruta (lo he leído en las dos versiones); media en la magnitud del daño, que no he medido in-game.

### EXT-05 `#ifdef SERVER` / `#ifndef SERVER` sin `#endif` en 7 ficheros de extras — **Media**
- Evidencia:
  - En los PBO originales (`grep -c` sobre los bytes) hay `#ifdef`=1 y `#endif`=0 en defaultfullauto, limitunconscioustime, preventplayermodifiers, preventweaponraise, spawnweaponchambered, spawnwithammoandmagazine y spawnwithbattery. spawncarfull y vehicle3ppdefaultfix sí los cierran (2/2).
  - Por fichero: `extra_defaultfullauto\...\Weapon_Base.c:1`, `extra_limitunconscioustime\...\Unconsciousness.c:1`, `extra_preventplayermodifiers\...\BrokenLegs.c:1`, `extra_preventweaponraise\...\PlayerBase.c:1`, `extra_spawnweaponchambered\...\Weapon_Base.c:1`, `extra_spawnwithammoandmagazine\...\ItemBase.c:1`, `extra_spawnwithbattery\...\ItemBase.c:1`.
  - El mismo patrón aparece en unos 50 ficheros de scripts_client y scripts_server (`scratchpad\lint\lint_summary.txt`).
- Riesgo: el compilador de 1.2x lo toleraba (el mod funcionaba), pero depende de un comportamiento no documentado del preprocesador. Si 1.30 lo endurece, rompe la compilación de todo el módulo.
- Propuesta: añadir `#endif` al final de cada fichero, en una pasada mecánica sobre todo el mod.
- Esfuerzo: S. Confianza: alta en el hecho; baja en que 1.30 lo rechace (no ejecuté el juego).

### EXT-06 preventplayermodifiers no cubre los modificadores nuevos de 1.30 — **Media**
- Evidencia:
  - Las firmas que sobrescribe existen todas en 1.30: `HeatComfortAnimHandler.Update(float, HumanMovementState)` en `VAN\4_world\classes\heatcomfortanimhandler.c:35`; `BrokenLegsMdfr.OnActivate/OnReconnect` en `...\conditions\brokenlegs.c:23,30`; `ModifierBase.Activate()` en `modifierbase.c:193`; `ActivateCondition` en commoncold.c:29, influenza.c:29, pneumonia.c:27, woundinfection.c:39 y woundinfection2.c:30; `OnTick` en hunger.c:31 y thirst.c:30.
  - Al comparar los listados de 1.29 y 1.30 aparecen `heatstroke.c` (HeatStrokeMdfr y fases 1 a 4, `diseases\heatstroke.c:1,42,84,140,233`), `irritatedeyes.c` y `silicosis.c`, que el extra no desactiva. Tampoco desactiva cholera, salmonella ni heavymetal.
  - Al vaciar `BrokenLegsMdfr.Activate()`, `ModifierBase.Tick` (`modifierbase.c:91-93`) vuelve a llamarlo en cada tick mientras `m_ShouldBeActive` siga en true. El coste es bajo, pero el estado queda incoherente.
- Propuesta: decidir qué enfermedades importan en partidas de unos 20 a 30 minutos. Como mínimo, añadir `HeatStrokePhase1..4Mdfr` (dependen del confort térmico, que sigue activo). Para BrokenLegs, mejor `ActivateCondition` que devuelva false antes que vaciar `Activate`.
- Esfuerzo: S. Confianza: alta.

### EXT-07 limitunconscioustime: `StartCommand_Move` forzado en el servidor cada 0,5 s, y existe alternativa vanilla — **Media**
- Evidencia:
  - `SRC\extra_limitunconscioustime\...\Unconsciousness.c:26-36`: con la inconsciencia desactivada, mientras el shock esté por debajo del umbral llama a `player.StartCommand_Move()` y a `ForceStance(PRONE)` cada vez que vanilla evalúa `ActivateCondition`. Eso ocurre cada 0,5 s (`m_TickIntervalInactive = 0.5`, `VAN\...\modifiers\unconsciousness.c:10`).
  - Arrancar un comando de movimiento solo en el servidor sobre un jugador que controla el cliente puede cortar acciones en curso o desincronizar. Las APIs existen en 1.30 (`VAN\3_game\human.c:1479,1481,499`).
  - El modo con límite (`:44-48`) fija el shock en `CONSCIOUS_THRESHOLD+0.1` (`playerconstants.c:156`) y salta la condición vanilla de pulso regular (`unconsciousness.c:48`).
  - Vanilla 1.30 ofrece `cfggameplay.json` → `PlayerData.ShockHandlingData.shockRefillSpeedUnconscious` (`VAN\3_game\cfggameplayhandler.c:427`, usado en `modifiers\shock.c:43,47`) y `disableRespawnInUnconsciousness` (`cfggameplayhandler.c:188`).
- Propuesta: para «inconsciencia corta», subir `shockRefillSpeedUnconscious` en cfggameplay y retirar el extra. Si hace falta un tope duro de 5 s, conservar solo la rama `DeactivateCondition`. Para «sin inconsciencia», sustituir el `StartCommand_Move` repetido por un único cambio de postura o, mejor, subir el shock por encima del umbral. Cachear la lectura de `ServerConfigGetInt`/`IsCLIParam` (EXT-14).
- Esfuerzo: S-M. Confianza: media; el efecto sobre acciones en curso no está probado in-game.

### EXT-08 preventweaponraise: vanilla 1.30 ya lo cubre y quitar el levantado abre disparos a través de paredes — **Media**
- Evidencia:
  - El extra (`SRC\extra_preventweaponraise\...\PlayerBase.c:4-14`) reemplaza `CheckLiftWeapon` sin super. Ese método existe en 1.30 (`VAN\4_world\entities\manbase\playerbase.c:8816`), y `SendLiftWeaponSync(bool, float obstruct = 0)` también (`:8777`). Compila, pero anula la obstrucción de arma de 1.30 (`weap.UseWeaponObstruction`, `:8827`).
  - En 1.30, `cfggameplay.json` → `PlayerData.WeaponObstructionData.staticMode/dynamicMode = 2` (`EWeaponObstructionMode.ALWAYS`: «Weapon obstructs and never lifts», `VAN\3_game\cfggameplaydatajson.c:375-380`; se lee en `weapon_base.c:1421-1431`) hace exactamente esto sin script. El arma retrocede pero no se levanta.
- Riesgo del extra: el levantado vanilla existe para que el cañón no atraviese geometría. Sin levantado ni obstrucción, el proyectil sale desde la boca del cañón, que puede estar al otro lado de una pared.
- Propuesta: retirar el extra y fijar `WeaponObstructionData` en 2/2 en el `cfggameplay.json` de la misión.
- Esfuerzo: S. Confianza: alta en la equivalencia de configuración; media en el exploit de paredes (no lo probé).

### EXT-09 defaultfullauto: cambio de modo de fuego solo en el servidor — **Media**
- Evidencia: `SRC\extra_defaultfullauto\...\Weapon_Base.c:1-23` corre bajo `#ifdef SERVER` y llama a `SetCurrentMode` en `EEInit`. Las APIs existen (`VAN\4_world\entities\core\inherited\weapon.c:43-49`). En el script de 1.30, el modo solo se toca en `OnStoreLoad` (`weapon_base.c:521`) y con la acción del jugador (`SetNextMuzzleMode`, `:2247`); no encontré ninguna sincronización de red del modo por script.
- Riesgo: si el modo no se replica de forma nativa, el cliente dibuja y predice semiautomático mientras el servidor dispara en automático. Si se mueve el código al cliente, una recreación del arma en el cliente (entrar en la burbuja de red) la reiniciaría a automático aunque el jugador haya cambiado a otro modo.
- Propuesta: prueba in-game (spawnear un M4 y comparar el HUD de modo con la cadencia real). Si hay desincronía, aplicarlo en `EEOnCECreate` del arma o sincronizarlo con una RPC o variable propia.
- Esfuerzo: S (prueba) / M (arreglo). Confianza: **baja**; el comportamiento nativo no es verificable leyendo script.

### EXT-10 vehicle3ppdefaultfix: depende de Vehicle3PP, que no está en local — **Baja**
- Evidencia: el código va bajo `#ifdef Vehicle3PP` (`SRC\extra_vehicle3ppdefaultfix\...\CarScript.c:1`, `BoatScript.c:1`). Redeclara `protected bool m_Allow3PPCamera; m_DriverOnly;` (`CarScript.c:4-5`), asigna antes de `super.EEInit()` (`:49-51`) y mete entradas en la lista blanca que devuelve `GetVehicle3PPConfig()` (`:20-27`). `BoatScript` existe en 1.30 (`VAN\4_world\entities\vehicles\boatscript.c:41`).
- Riesgo: si Vehicle3PP declara esos mismos miembros en su `modded CarScript`, la redeclaración puede no compilar. Si Vehicle3PP queda por encima en la cadena, su `EEInit` (ejecutado vía super) sobrescribe el valor y el fix no hace nada. Sin Vehicle3PP cargado, el extra está vacío.
- Propuesta: si no se usa Vehicle3PP, retirarlo. Si se usa, añadir su CfgPatches a `requiredAddons` y quitar la redeclaración.
- Esfuerzo: S. Confianza: baja (el mod no está instalado: `!Workshop\@Vehicle3PP` está vacío).

### EXT-11 Las motos de 1.30 quedan fuera de los extras de vehículos — **Baja**
- Evidencia: `MotorbikeScript : Motorbike` (`VAN\...\motorbikescript.c:53`) no hereda de CarScript. spawncarfull (`extra_spawncarfull\...\CarScript.c:2-10`), reducecardamage y vehicle3ppdefaultfix solo tocan CarScript y BoatScript.
- Propuesta: si la economía central de BR genera motos, añadir el equivalente de llenado en `MotorbikeScript.EEOnCECreate`.
- Esfuerzo: S. Confianza: alta.

Comprobados y sin problema para 1.30:
- **changefeedbackurl**: `InGameMenu.OpenFeedback` (`VAN\5_mission\gui\ingamemenu.c:422`, protected), `MainMenu.OpenFeedback` (`...\mainmenu.c:560`, protected) y `MainMenuNewsfeed.OpenFeedback` (`...\mainmenunewsfeed.c:74`, public) coinciden.
- **spawncarfull**: `CarScript.EEOnCECreate` (`carscript.c:2412`), `BoatScript.EEOnCECreate` (`boatscript.c:342`), `BoatFluid : ETransportFluid` (`VAN\3_game\vehicles\boat.c:13`) y `CarFluid` (`car.c:18`) coinciden. Reemplaza el llenado vanilla (0-35 %) a propósito, sin super. Expansion no sobrescribe `EEOnCECreate` en REF (grep vacío).
- **spawnweaponchambered**: `FillChamber` (`weapon_base.c:921`), `SpawnAmmo` (`:740`) y `WeaponWithAmmoFlags` (`:11`) existen.
- **spawnwithammoandmagazine / spawnwithbattery**: `ItemBase.EEOnCECreate` (`VAN\4_world\entities\itembase.c:4111`), `GetRandomMagazineTypeName` (`weapon.c:268`), `GetEconomyProfile` (`entityai.c:888`), `CEItemProfile.GetNominal` (`centraleconomy.c:754`) y `HasEnergyManager` (`entityai.c:3420`) existen. Ninguna clase de arma vanilla sobrescribe `EEOnCECreate` sin llamar a super (solo hay 21 overrides, todos de comida, botellas, vehículos y fuegos artificiales).

---

## 2) Optimización para 100-120 jugadores

### EXT-12 spawnwithammoandmagazine: `Print` por cada arma generada, bucle mal acotado y un NULL sin comprobar — **Media**
- Evidencia (`SRC\extra_spawnwithammoandmagazine\Scripts\4_World\Entities\ItemBase.c`):
  - `Print` en las líneas 63, 68, 69 y 75: dos o tres líneas de log por cargador y por arma generada por la economía central.
  - Línea 80: `Math.RandomIntInclusive` se reevalúa en cada vuelta del `for`, con lo que el número de montones queda sesgado.
  - Líneas 82-83: `new array` y `ConfigGetTextArray` dentro del bucle.
  - Línea 88: `ammoPile.GetEconomyProfile()` sin comprobar NULL. Si `CreateObjectEx` falla, el script da error de puntero nulo y aborta (no tumba el proceso).
  - Líneas 9, 16 y 17: tres `ServerConfigGetInt` por arma.
- Propuesta: quitar los `Print` o pasarlos a `BattleRoyaleUtils.Trace`; calcular el aleatorio una sola vez; sacar el array y la lectura de configuración del bucle; comprobar `if (!ammoPile) continue;`; cachear las claves de configuración en estáticos.
- Esfuerzo: S. Confianza: alta en el código; el coste real no está medido.

### EXT-13 Acumulación de entidades por arma (spawnweaponchambered + spawnwithammoandmagazine) — **Media**
- Evidencia:
  - `SRC\extra_spawnweaponchambered\...\Weapon_Base.c:4-15` corre en `EEInit`, no en `EEOnCECreate`. Por tanto actúa sobre **toda** arma creada en el servidor (economía central, kits de inicio, armas generadas por script). Además, en `EEInit` los adjuntos de la economía central aún no existen, así que `GetMagazine(0)` suele valer NULL y `SpawnAmmo()` acopla un cargador lleno (`weapon_base.c:740-753` → `SpawnAttachedMagazine`).
  - A eso se suman los 1-2 cargadores sueltos de spawnwithammoandmagazine.
  - Resultado: unas 3 entidades extra por arma, más el `AssembleGun` que vanilla encola en `EEInit` (`weapon_base.c:135-142`).
  - spawnwithbattery (`SRC\extra_spawnwithbattery\...\ItemBase.c:9-15`) intenta dos `CreateInInventory` por cada objeto con gestor de energía. Coste bajo.
- Propuesta: pasar el cargado de recámara a `EEOnCECreate` (solo generación nueva) y fijar un único criterio: o cargador acoplado o cargadores sueltos, no los dos. Valorar `cfgspawnabletypes.xml` para los cargadores acoplados, sin script. Medir el número de entidades al arrancar la partida.
- Esfuerzo: S-M. Confianza: alta en el comportamiento; el impacto no está medido.

### EXT-14 limitunconscioustime lee configuración en cada tick de cada jugador — **Baja**
- Evidencia: `Unconsciousness.c:16-17` hace `ServerConfigGetInt("BRDisableUnconsciousness")` más `IsCLIParam` (`VAN\1_core\proto\ensystem.c:141`) cada 0,5 s por jugador. Con 120 jugadores son unas 240 llamadas de cada tipo por segundo.
- Propuesta: cachearlas en un estático al arrancar el módulo.
- Esfuerzo: S. Confianza: alta.

### EXT-15 gui.pbo de 126 MB con pantallas de carga EDDS sin comprimir — **Media**
- Evidencia:
  - `WS\addons\gui.pbo` ocupa 126 037 612 bytes y contiene 12 `textures\loading_screens\br_loading_N.edds` de entre 7,4 y 11,1 MB cada una, con su `.edds.meta`.
  - Los primeros DDS de la cabecera tienen fourcc = 0 (sin comprimir); 1920×1080×4×1,33 de mipmaps ≈ 11,06 MB, que coincide.
  - `imagesets\DayZBR_GUI.paa` (163 KB) no se referencia: el imageset apunta a `dayzbr_gui.edds` (`SRC\gui\imagesets\dayzbr_gui.imageset:7`) y el grep en .c, .layout e .imageset sale vacío.
- Propuesta: convertir las pantallas de carga a PAA o EDDS comprimido (DXT1/DXT5), quitar los `.meta` y el `.paa` huérfano, y reducir el número de pantallas. Se estima una bajada del PBO por debajo de unos 25 MB, lo que acorta la descarga y la entrada de 120 clientes.
- Esfuerzo: S-M. Confianza: alta en el tamaño; la estimación del ahorro no está medida.

### EXT-16 reducecardamage: el ajuste no se cachea en el cliente — **Baja** (desaparece con EXT-04)
- Evidencia: `rcdsettingsloader.c:16-42`. `initialized` solo pasa a true si el fichero existe, y en el cliente nunca existe. `OnContact` llama a `Get()` cinco veces por contacto (`ReduceCarDamage.c:5-9`), y cada llamada hace un `FileExist` y un `new`.
- Propuesta: retirarlo (EXT-04).

---

## 3) Licencias y dependencias

### EXT-17 Dependencia de Expansion: qué se usa y si se puede reducir — **Media**
- Evidencia (grep de `Expansion*` en scripts_client y scripts_server):
  - **Core**, verificado en REF:
    - `ExpansionNotification` (`REF\exp_core_scripts\3_Game\DayZExpansion_Core\Client\Notifications\ExpansionNotificationSystem.c:164-185`), usado en `BattleRoyaleRPC.c:292` y `0_BattleRoyaleState.c:179,191`
    - `ExpansionArray` (`...\ExpansionArray.c`), en `LoadingScreenBackground.c:7,18`
    - `ExpansionServerMarkerData` (`...\Markers\ExpansionServerMarkerData.c`), en `BattleRoyaleClient.c:13,208`
    - `ExpansionClientSettings`: los campos que toca existen (`...\ExpansionClientSettings.c:34,39,53,58,63,76,77,126`) y las firmas `OnSettingsUpdated`, `OnRead` y `OnSave` coinciden (`:1433,191,892`)
    - `ExpansionMapWidgetBase` (`REF\exp_core_scripts\5_Mission\...\ExpansionMapWidgetBase.c`)
    - `GetExpansionClientVersion` (`...\DayZExpansion_Core\DayZGame.c`)
  - **Navigation**, no disponible en local: `ExpansionMapMenu`, `ExpansionMapMarker` y `GetExpansionSettings().GetMap()` (EXT-02).
  - **Missions**, opcional y con guarda: `#ifdef EXPANSIONMODMISSIONS` (`SRC\scripts_server\5_Mission\COT\GUI\BRMasterControlsModule.c:45-48`); `ExpansionMissionModule` se encontró en `221100\2792984177\addons\missions_scripts.pbo`.
  - **DayZ-Expansion principal** (`DayZExpansion_Scripts`, `prefix=DayZExpansion\Scripts`): BR lo pone en `requiredAddons` (`scripts_client\config.cpp:9`), pero no encontré ninguna clase que BR use y que solo viva en él.
  - **El mapa no fuerza Expansion.** En el código, «Vigrid» es la marca y la API (`SRC\scripts_server\3_Game\Config\BattleRoyaleServerData.c:6-29`, `api.vigrid.ovh`), no un mapa. Los mapas de `SRC\data\LoadingScreens.json` son `default`, `chernarus`, `namalsk` y `sakhal`, y `BattleRoyaleServer.c:513` menciona `takistanplus`. No hay ninguna pista de que el mapa exija Expansion.
- Propuesta: dependencia mínima = Expansion-Core + Expansion-Navigation (+ Missions si se quieren airdrops desde COT). Cambiar `requiredAddons` a `DayZExpansion_Core_Scripts` + Navigation y probar sin el mod principal de Expansion. Quitar Expansion por completo exigiría reescribir las notificaciones (una función), los marcadores y el mapa, y la UI de ajustes de cliente: esfuerzo L, no recomendado ahora.
- Esfuerzo: S (reducir) / L (quitar). Confianza: media. La búsqueda en los PBO de Workshop es textual y algunos pueden estar comprimidos, así que un 0 no prueba ausencia.
- **Confirmar con el usuario**: ¿«Vigrid» es el nombre del servidor/API o un mapa propio? El código apunta a lo primero.

### EXT-18 Firmas: cuatro claves ajenas; hay que re-firmar con clave propia — **Media**
- Evidencia:
  - `WS\keys\` contiene `BattleRoyale.bikey`, `Munghard.bikey`, `daemonforge.bikey` y `napspublic.bikey`.
  - Los `.bisign` son todos v3 y de 1024 bits (parseados): 17 PBO con `BattleRoyale` (incluidos `gui.pbo` y `sounds.pbo`), más `nulledkillfeed` con `napspublic`, `reducecardamage` con `daemonforge` y `slowzombies` con `Munghard`.
- Propuesta: generar un par propio (DSCreateKey), firmar todos los PBO reconstruidos con DSSignFile y publicar solo esa `.bikey`. Si se retiran los tres terceros (EXT-01, EXT-04, EXT-19), queda una sola clave. Un PBO de terceros que se reempaquete pierde su firma original, porque no tenemos su clave privada.
- Esfuerzo: S. Confianza: alta.

### EXT-19 slowzombies (Munghard, «WalkingDeadZombies», 2019): válido en 1.30 pero parcial y sin licencia — **Media**
- Evidencia:
  - Cabecera: `prefix=WalkingDeadZombies\slowzombies`, `Mikero=DePbo.dll.7.16`, fecha `Fri Jun 28 23:50:27 2019`. Contiene solo `config.bin`, sin LICENSE.
  - `SRC\slowzombies\config.cpp:3-13` requiere `DZ_AI` y `DZ_Characters_Zombies` (correcto). Parchea `CfgAIBehaviours >> Infected >> BehaviourHLZombie`, cuyas claves existen en 1.30 (`C:\Users\guill\OneDrive\Documentos\DayZ Projects\DZ\AI\config.cpp:6121`).
  - Diff contra vanilla 1.30 (rango aproximado 6112-6300): Sprint `maxSpeed` 9 → 2,7; Run 3 → 2,6; visión `visionToAlertMultiplier` 20 → 1000; `NoiseStrengthTeamMultipliers.Zombies` 1 → 0. Las claves nuevas que vanilla tiene y el mod no (`attractedSearchDistance`, `radiusMin`, `radiusMax`, `radiusShotMin`, `radiusShotMax`, `shotVolExponent`) se conservan porque el parche se fusiona.
  - **No afecta** a las subclases vanilla que redefinen el movimiento: `InfectedRunner` (Run `maxSpeed=9`, AI\config.cpp ≈6699-6719), `InfectedSoldier_Heavy` y `InfectedMMummy` (Sprint=3).
- Propuesta: sustituirlo por un parche propio dentro de un addon de BR (unas 30 líneas, con los valores que se decidan para BR) que cubra también Runner, Heavy y Mummy. Así se elimina un PBO sin licencia y su clave.
- Esfuerzo: S. Confianza: alta en la validez del config; media en la cobertura (no revisé la herencia completa de todas las clases de infectados).

### EXT-20 reducecardamage: Apache-2.0, redistribuible con condiciones — **Baja**
- Evidencia: `SRC\reducecardamage\LICENSE` (Apache 2.0, 200 líneas) y `NOTICE` (Kevin Hoddinott «DaemonForge», enlace al repositorio). La cabecera del PBO dice `product=dayz ugc`, `prefix=ReduceCarDamage`.
- Análisis: Apache-2.0 permite redistribuir dentro de un paquete DSPL-SA siempre que se conserven LICENSE y NOTICE y se marquen los ficheros modificados (sección 4). El componente sigue siendo Apache y no pasa a DSPL-SA. `SRC\data\credits.json` no cita a DaemonForge, Munghard ni a naps.
- Propuesta: si se retira (EXT-04), no hace falta nada más. Si se conserva, añadir los créditos de terceros a `credits.json`.
- Esfuerzo: S. Confianza: alta; es una lectura de la licencia, no asesoría legal.

### EXT-21 DSPL-SA del propio mod y recursos de licencia dudosa — **Baja**
- Evidencia:
  - `WS\LICENSE` es DSPL-SA (atribución, no comercial, solo DayZ SA, compartir igual). El refurbish, por tanto, debe publicarse con DSPL-SA y con atribución a Myst y KeganH (también figuran en `credits.json`).
  - `sounds.pbo` contiene `Music\apocalypse.ogg` (2,4 MB), y `credits.json` lo atribuye a «Apocalypse - Desolation Redux», un mod de Arma 3, sin licencia visible.
- Propuesta: verificar la licencia de esa música o sustituirla. Mantener la atribución DSPL-SA.
- Esfuerzo: S. Confianza: media.

---

## 4) Otras mejoras

### EXT-22 Inputs.xml — **Baja**
- `SRC\data\Inputs.xml:1-42` sigue el formato `modded_inputs` (inputs/actions, sorting, preset/btn), coherente con el de Expansion en `REF\expansion_scripts\Data\Inputs.xml`. Los `loc` son literales («BR Ready Up») y no claves de stringtable, así que no se traducen.
- Usa F1/F2/F3 (`:23-31`) para ReadyUp, Unstuck y Debug. **No pude verificar** si vanilla 1.30 asigna F1-F12 por defecto (el preset vanilla no está en local); históricamente las ranuras de gestos usaban teclas F.
- Propuesta: claves `#STR_` en `languagecore\stringtable.csv`, comprobar conflictos in-game y valorar quitar `UADayZBRDebug` en release.

### EXT-23 Prefijos, estructura AddonBuilder y restos — **Baja**
- Los prefijos (cabeceras de PBO) son coherentes con los `files[]`:
  - `Vigrid-BattleRoyale\{Data,GUI,LanguageCore,Models\Shapes,Scripts\Client,Scripts\Server,Sounds}`
  - `Vigrid-BattleRoyale\Extra\<Nombre>` en los 10 extras
  - `ReduceCarDamage`, `NulledKillfeed` y `WalkingDeadZombies\slowzombies` en los terceros
- Hay 109 referencias de texto a `Vigrid-BattleRoyale` en el código extraído. Si se renombra el prefijo, hay que cambiarlas todas: `files[]`, imageset, `LoadingScreens.json`, layouts, `mod.cpp` y `creditsJson`.
- Para AddonBuilder: una carpeta por PBO en `P:\Vigrid-BattleRoyale\...` con `$PBOPREFIX$` igual al prefijo, y binarizar solo `models_shapes` (contiene `.p3d`).
- Restos:
  - `scripts_client\3_Game\PPEManager\PPERequesterBank.c.c` (doble extensión, cosmético).
  - `models_shapes\zone.p3d` sin referencias (grep de `zone.p3d` y `Models/Shapes` vacío): candidato a retirarlo.
  - `BlueZone` hereda de `EffectArea`, definido en `DZ\data\config.cpp`, y `DZ_Data` ya figura en `requiredAddons` (`scripts_client\config.cpp:7`). Correcto.

### EXT-24 URLs del proyecto original — **Baja**
- `GITHUB_URL = "https://github.com/LeMyst/Vigrid-BattleRoyale"` y `BATTLEROYALE_API_ENDPOINT = "https://api.vigrid.ovh/"` (`SRC\scripts_client\2_GameLib\BattleRoyaleConstants.c:24,28`). El botón de feedback (`extra_changefeedbackurl`) lleva al repositorio del autor original.
- Propuesta: apuntar a la URL propia o retirar el extra.

### EXT-25 Consolidar los 10 extras — **Baja**
- Son diez PBO de entre 1 y 5 KB, cada uno con su CfgPatches/CfgMods, su firma y su `requiredAddons` mínimo. Propuesta: un único addon `BR_Tweaks` con un interruptor por ajuste (en la configuración JSON de BR o en serverDZ.cfg, como ya hacen `BRDisableSpawnWithAmmo` y similares). Así hay menos PBO que firmar y un solo sitio para el orden de carga.
- Esfuerzo: M.

---

## Qué NO verifiqué
- No ejecuté el juego ni compilé con el motor 1.30. Los avisos de `#ifdef` y los riesgos de orden de carga son lecturas de código y config.
- No desofusqué nulledkillfeed: no conozco su CfgPatches, sus `requiredAddons` ni su código.
- No pude abrir Expansion-Navigation ni Vehicle3PP (no están en local): el nombre de CfgPatches de Navigation y los miembros de Vehicle3PP no están comprobados.
- No sé si el modo de fuego se sincroniza de forma nativa (EXT-09).
- No medí rendimiento, número de entidades ni daño de choque.
- No derapé el `config.bin` de `sounds.pbo` (solo vi que contiene `config.bin` y `Music\apocalypse.ogg`).
- No sé qué teclas trae por defecto el preset vanilla de 1.30.
- Las búsquedas en PBO de Workshop son textuales y algunos PBO pueden ir comprimidos, así que un 0 no es prueba de ausencia.

# Informe de la lane config-mission (BattleRoyale 1.0)

Rama `lane/config-mission`, worktree `C:\Users\guill\Repos\br-wt\config-mission`.
Commit con todos los cambios: **`5c821bd`** (`5c821bd3441829a258d23091375fc86c4d6deb99`, "Chernarus zone defaults, config validation and server package"). Este informe va en el commit siguiente, sin más cambios de código.

## Resumen

- Tablas de zona para Chernarus y 100-120 jugadores: 7 zonas, de 2500 m a 50 m de radio, unos 32 minutos por partida completa. Con los valores del autor, el **22 % de los arranques del servidor acaba en `RequestExit`** al generar los círculos y el 19,5 % de las partidas tiene más del 10 % del primer círculo en el mar. Con los nuevos: 0,025 % y 0 %. Son medidas de una simulación del generador, no del motor (ítem 2).
- **La decisión que más conviene revisar:** la lista `end_avoid_city` pasa de 3 a 130 entradas. Chernogorsk, Elektrozavodsk, Berezino, Solnechny, Svetloyarsk, Vysotovo y Novodmitrovsk dejan de poder ser zona final. Se puede jugar y aparecer en ellos, pero la partida no acaba allí. Es la única palanca que hay en datos para cumplir «primer círculo sin mar». Además evita los arranques fallidos. Se revierte editando `zone_settings.json`.
- Validación de la configuración al cargar, ZON-14, ZON-15 y paquete de servidor (`BattleRoyale_server\`).
- Gate: lint igual a la base, hallazgo por hallazgo. No se ha compilado con el motor.

---

## Ítem 1 — ZON-07 / SRV-42: valores por defecto para Chernarus

**Cambios**
- Overrides de POI de ejemplo eliminados: lista vacía por defecto (`BattleRoyale/addons/scripts_server/3_Game/Config/BattleRoyalePOIsData.c:8`). Antes movían Chernogorsk a {100, 200} y Novodmitrovsk a {300, 400}. El comentario de `:6-7` documenta el formato de una entrada.
- Spawn del lobby, **valor sin cambiar** (`"14829.2 0 14572.3"`). Solo cambia el comentario (`BattleRoyaleSpawnsData.c:6`). Motivos:
  - Está a 28 m del named location `Camp_Shkolnik` (14856,5 / 14571,0; `DZ\worlds\chernarusplus\world\config.cpp:1052`).
  - Está a 11-22 m de tres `Land_Camp_House_*` situados a ~75 m de altitud (`mpmissions\dayzOffline.chernarusplus\mapgrouppos.xml:11646,11648,11649`). Es tierra firme, un campamento en la esquina NE.
  - Ningún círculo llega allí: el centro del primer círculo (R 2500) está limitado a ≤ 12860 en x y z (`BattleRoyaleZone.c:414`), y el punto del lobby queda a 2609 m como mínimo.
- `avoid_city_spawn`, **valor sin cambiar**. Solo cambia el comentario (`BattleRoyaleSpawnsData.c:13-25`). Lo que he podido comprobar:
  - `Camp_Shkolnik` es el lobby.
  - `Ruin_Voron` y `Settlement_Skalisty` están en la isla de Skalisty: casas, barcas y faro en x 13050-14040, z 2760-3330 de `mapgrouppos.xml`.
  - `Settlement_Novoselki`, `_Dubovo` y `_Vysotovo` son entradas tipo City a 0,6-1,3 km de Chernogorsk.
  - De `Hill_Zelenayagora` y `Settlement_Kumyrna` no encuentro el motivo, así que se quedan.
  - Ojo: hoy la lista no tiene efecto por un fallo de `4_BattleRoyalePrepare.c:218` (ver CROSS-LANE 1).
- `end_avoid_city` pasa de 3 a 130 entradas, en tres grupos comentados (`BattleRoyaleZoneData.c:9-48`):
  1. Lobby e islas (4). Se añade `Ruin_Storozh`, la isla prisión. Su POI está en z 1278, unos 900 m al sur de la línea de costa de Kamenka y Komarovo (z ≈ 2200-2440). Alrededor hay barracones y garitas en x 2446-2718, z 1218-1405, a 23 m de altitud (`mapgrouppos.xml:1301-1302`), con barcas en la isla y en la orilla de enfrente. Es el mismo patrón de Skalisty, que el autor ya excluía.
  2. Demasiado cerca del borde del mapa (74). Con estas tablas, el generador falla en ≥ 5 % de las generaciones que empiezan en ese POI. Cada círculo mayor se desplaza hacia el centro del mapa entre el 25 y el 75 % de la diferencia de radios (`BattleRoyaleZone.c:397-411`; constantes en `scripts_client/2_GameLib/BattleRoyaleConstants.c:71-72`) y tiene que caber en el cuadrado de 15360 m (`:414`). Como nunca se desplaza la diferencia entera, una zona final pegada al borde pierde margen en cada paso hasta que el siguiente círculo no cabe: 500 intentos y `RequestExit(0)` (`:418-451`). Entran todos los pueblos del norte (Tisy, Krasnoye, Stary Yar, Dobroye…), Novodmitrovsk, Svetloyarsk y el borde oeste.
  3. Costeros (52). En ≥ 25 % de las generaciones, más del 5 % del primer círculo cae en el mar: toda la costa sur de Kamenka a Kamyshovo y la costa este de Solnechny a Olsha.

**Verificación**
- Posiciones y tipos de los 306 named locations leídos del config del mundo.
- Cada clasificación sale de 200 generaciones por POI con un port a Python del generador (`BattleRoyaleZone.c:188-363,384-494,541-573`; selección del candidato en `:469-482`). Resultado agregado en el ítem 2.
- La herramienta queda en el paquete (`BattleRoyale_server/tools/zone_table_check.py`). Con `--per-poi 200` sobre los valores nuevos no marca ningún POI restante.

**No verificado:** el terreno a la vista. La costa del modelo es aproximada (±200 m) y está dibujada a partir de los edificios extremos de `mapgrouppos.xml`, los spawns costeros de `cfgplayerspawnpoints.xml` y los nombres Marine. Que Storozh sea isla es una deducción por edificios y barcas, sin mirar el mapa. Los named locations son los de la extracción `P:\DZ` (abril, probablemente 1.29) y no los he contrastado con 1.30.

## Ítem 2 — ZON-06: tablas de zona para 100-120 jugadores

**Cambios**
- `num_zones` pasa de 6 a 7 (`BattleRoyaleGameData.c:11`).
- `static_sizes`, `static_timers` y `min_players` (`BattleRoyaleZoneData.c:69-71`). Las tres tablas tienen 7 entradas; el índice 0 es la zona final (`BattleRoyaleZone.c:71,146,154`).

| Zona | Radio (m) | Duración (s) | Cierra a los (s) | Viaje peor caso (m) | Velocidad necesaria |
|---|---|---|---|---|---|
| 1 | 2500 | 240 | 192 | (ya dentro, spawn en la zona +250 m) | — |
| 2 | 1800 | 330 | 264 | 1225 | 4,6 m/s |
| 3 | 1200 | 300 | 240 | 1050 | 4,4 m/s |
| 4 | 700 | 270 | 216 | 875 | 4,1 m/s |
| 5 | 350 | 225 | 180 | 612 | 3,4 m/s |
| 6 | 150 | 180 | 144 | 350 | 2,4 m/s |
| 7 | 50 | 135 | 108 | 175 | 1,6 m/s |

**Justificación de los números**
- **Mecánica** (leída en el código): la ronda k dura `static_timers[N-k]` y la zona se cierra al 80 % (`6_BattleRoyaleRound.c:76,105-107`). Antes de la ronda 1 hay `(60 * round_duration_minutes) / 2` = 150 s (`5_BattleRoyaleStartMatch.c:22,65`). La última ronda deja la zona final `static_timers[0] / 2` s y luego todo es daño (`7_BattleRoyaleLastRound.c:39,47,197-203`): 0,1 de vida por tick de 5 s, unos 50 s hasta morir.
- **Duración:** 150 + 1680 + 67 = **1897 s (31,6 min)**, más un máximo de unos 50 s de daño final. Queda dentro de 25-35. Es el máximo: la partida acaba antes en cuanto queda un jugador.
- **Primer círculo de 2500 m:** 19,6 km², con una mediana de 11 pueblos o ciudades donde se puede aparecer (p10: 8). Con 3375 m eran 18, pero el círculo cabe en tierra mucho peor. Uno de 2800 m daría 13 pueblos, a cambio de excluir más POI y alargar los tiempos; me quedo con 2500.
- **Cierre final de 50 m de radio (100 m de diámetro):** cumple el rango «50-100 m» se lea como radio o como diámetro.
- **Tiempos:** el peor jugador está en el borde opuesto de la zona anterior y tiene que recorrer el desplazamiento del centro más la diferencia de radios, como máximo 1,75 × ΔR. La tabla da la velocidad necesaria para llegar antes del cierre. Siempre es ≤ 4,6 m/s, por debajo del sprint, y el mod pone a 0 el gasto de estamina del sprint (`scripts_server/3_Game/Constants.c:3-5`). Además, fuera de zona el daño es suave en las primeras rondas: 0,1 × k/7 por tick (`6_BattleRoyaleRound.c:93`).
- **Sin prórroga por desplazamiento largo:** ΔR máximo de 700 m, así que el desplazamiento máximo es 525 m, por debajo de los 1500 que activan `f_durationOffset` (`BattleRoyaleZone.c:484-485`).
- **Inicio dinámico** (`0_BattleRoyaleState.c:520-551`): zona 1 si hay más de 60 jugadores, zona 2 si hay más de 30, y si no zona 3. `min_zone_num = 4` no cambia, así que la zona 3 es el inicio más tardío. Con 31-60 jugadores la zona 2 (10,2 km²) da una densidad parecida a la de 120 jugadores en la zona 1. Una partida que empieza en la zona 2 dura unos 27,6 min.
- `round_duration_minutes = 5` no cambia. Sigue fijando los 150 s antes de la primera zona. Lo documento en el comentario de `BattleRoyaleGameData.c:12`.

**Verificación** (salida de `zone_table_check.py`, 8000 generaciones, semilla 4242)
```
### valores nuevos (código por defecto)
zones 7, radii [50.0, 150.0, 350.0, 700.0, 1200.0, 1800.0, 2500.0], timers [135, 180, 225, 270, 300, 330, 240]
final-zone POIs 158 of 306 named locations
generations 8000: give-up (RequestExit) 2, endless loop 0
first zone over the sea: any 5.8%, >5% of its area 0.23%, >10% 0.00%, mean 0.13%
towns that can host spawns in the first zone: p10 8, median 11
full match: 1897 s (31.6 min) from the start match state to the end of the last zone, plus up to ~50 s of damage
### valores del autor (num_zones 6, lista de 3)
generations 8000: give-up (RequestExit) 1785, endless loop 0
first zone over the sea: any 35.5%, >5% of its area 25.74%, >10% 19.49%, mean 4.18%
towns that can host spawns in the first zone: p10 15, median 18
full match: 2434 s (40.6 min) ...
### tablas nuevas con la lista antigua de 3
generations 8000: give-up (RequestExit) 1459, endless loop 0
first zone over the sea: any 28.5%, >5% of its area 22.21%, >10% 17.05%, mean 3.68%
```
El 5,8 % de «any» son círculos con menos del 5 % de mar. Está dentro del error del modelo de costa.

**No verificado:** el ritmo real en partida, las velocidades reales del jugador en el terreno y las probabilidades en el motor; las cifras salen del port, no del juego.

## Ítem 3 — ZON-09 / SRV-45: validación de la configuración

**Cambios**
- Contrato nuevo en la base: `Validate()` y `LogInvalidSetting(...)` (`BattleRoyaleDataBase.c:20-26`). El mensaje es `Error("[DayZ-BattleRoyale] <fichero>: invalid <campo> = <valor> (<regla>), using the default value")`.
- `BattleRoyaleConfig.Load()` llama a `config.Validate()` cuando ya se han cargado el perfil y el override de misión (`BattleRoyaleConfig.c:134`). Al acabar el bucle cruza `num_zones` (general_settings) con las tablas de zone_settings (`:142-148`).
- Tablas de zona (`BattleRoyaleZoneData.c:120-148` y `:151-211`):
  - `num_zones` ≥ 2. Con 1 o 0 hay NULL en `6_BattleRoyaleRound.c:179-182` y `7_BattleRoyaleLastRound.c:81`; esto cubre también SRV-19.
  - Las tres tablas con al menos `num_zones` entradas; las sobrantes solo generan un `Print` informativo.
  - Radios positivos y crecientes; timers positivos.
  - `min_players` ≥ 0 y no decreciente.
  - `min_zone_num` entre 1 y `num_zones - 1`.
  - El primer círculo tiene que caber en el mundo (`GetGame().GetWorld().GetWorldSize()`, `scripts/3_game/global/world.c:82` y `game.c:931`).
  - Cualquier fallo sustituye el bloque entero (`num_zones`, las tres tablas y `min_zone_num`) por los valores por defecto. Así las longitudes siguen siendo coherentes.
- Escalares, cada uno con su valor por defecto:
  - `BattleRoyaleGameData.c:125-172`. Incluye un acoplamiento que encontré: si `(60 * round_duration_minutes) / 2 <= time_until_teleport_unlock`, el estado StartMatch se desactiva antes de desbloquear, detiene el temporizador de desbloqueo y los jugadores se quedan congelados (`5_BattleRoyaleStartMatch.c:22,28,60,65,74-75`).
  - `BattleRoyaleLobbyData.c:114-136`. `autostart_delay` divide en `1_BattleRoyaleDebug.c:78`.
  - `BattleRoyaleSpawnsData.c:69-85`. Con radio 0, el lobby teletransporta a todos en cada tick.
  - `BattleRoyalePOIsData.c:55-78`: quita las entradas de override sin nombre o sin [x, z], con un Error que da el nombre.
- La corrección se hace solo en memoria: el `Save()` del perfil ocurre antes (`BattleRoyaleConfig.c:109-110`). El fichero inválido se queda como está y el Error vuelve a salir en cada arranque hasta que el admin lo arregle.

**Verificación**
- Todos los índices de las tablas en el código: `BattleRoyaleZone.c:146,154,224` y `7_BattleRoyaleLastRound.c:39`. Con longitud ≥ `num_zones` ≥ 2 y `x` en 1..`num_zones`, ningún índice queda fuera de rango.
- El caso P0 de Codex (`num_zones = 8` con tablas de 7) da ahora el Error y los valores por defecto.
- Repasé las reglas de Enforce de la skill: ámbito de método, nada de condiciones ni concatenaciones partidas en varias líneas, `ref` solo en miembros. Un barrido de líneas que acaban en `(`, `+`, `&&` o `||` no encuentra ninguna.
- Lint sin errores nuevos.

**No verificado:** la compilación y la ejecución en el motor. El texto exacto del Error en el RPT. Si `Error()` abre un diálogo en DayZDiag (en retail se registra en el log).

## Ítem 4 — ZON-14: `GetOverrodePosition` ya no recarga

**Cambio:** sin la llamada a `Load()`. El mapa se construye una sola vez con lo que ya cargó `BattleRoyaleConfig` (perfil y después misión), con guarda de entrada nula (`BattleRoyalePOIsData.c:80-112`). Además, `new_position` pasa a ser `ref array<int>` (`:118`), la parte de SRV-42 de confianza baja: un miembro sin `ref` es una referencia débil.

**Verificación:** grep de `.Load()`, `.LoadMission()`, `.Upgrade()` y `.Save()` en todos los `.c`. Solo `BattleRoyaleConfig` carga; `GetOverrodePosition` se usa en `BattleRoyaleZone.c:527` y en `4_BattleRoyalePrepare.c:344`.

**No verificado:** el efecto en el juego de la referencia débil anterior.

## Ítem 5 — ZON-15 / SRV-46: `Upgrade()` del lobby duplicaba objetos

**Cambio:** al migrar desde la versión 1 solo se rellenan las listas que el fichero dejó vacías (`BattleRoyaleLobbyData.c:82-112`). El mismo fallo estaba en `BattleRoyaleGameData.Upgrade()`, que añadía otro atajo 0, y lo corrijo igual (`BattleRoyaleGameData.c:106-122`).

**Verificación:** lectura del flujo. Si el cargador conserva los valores por defecto para un campo ausente, la lista ya tiene los 6 objetos y no se toca. Si lo deja vacío, se rellena una vez. En los dos casos no hay duplicados. `JsonFileLoader.LoadFile` lee sobre la instancia existente (`scripts/3_game/tools/jsonfileloader.c:7-33`).

**No verificado:** qué hace exactamente `JsonSerializer` con un campo ausente; el arreglo funciona en los dos casos.

## Ítem 6 — Paquete de servidor `BattleRoyale_server\`

| Fichero | Contenido |
|---|---|
| `mpmissions/dayzOffline.chernarusplus/db/globals.xml` | Copia byte a byte del vanilla con solo dos líneas cambiadas: `AnimalMaxCount` 200 → 0 (`:3`) y `ZombieMaxCount` 1000 → 150 (`:31`). El vanilla de la misión es idéntico por sha256 al de `P:\DZ\worlds\chernarusplus\ce\db\globals.xml` (`913abd8a…`). |
| `mpmissions/dayzOffline.chernarusplus/cfggameplay.json` | Copia del vanilla 1.30 de la misión con 5 líneas cambiadas (detalle debajo de la tabla). |
| `serverDZ.cfg.example` | Copia del cfg de stock: `maxPlayers` 60 → 120 y se añade `enableCfgGameplayFile = 1`. Esa clave no está en el cfg de stock; la verifiqué en `scripts/3_game/cfggameplayhandler.c:53`. Sin ella el JSON se ignora y se usan los valores legacy (`:62-66`). Ninguna otra clave nueva. |
| `tools/restart_loop.ps1` | Bucle de reinicio con borrado del mundo (detalle debajo de la tabla). Solo escrito, no ejecutado. |
| `tools/zone_table_check.py` | Port offline del generador de zonas, usado en los ítems 1 y 2. Lee las tablas de `BattleRoyaleZoneData.c` o de un `zone_settings.json`. |
| `README.md` | Orden de mods con sus ids de Workshop y claves `.bikey` (sacados de los `meta.cpp` y de las carpetas `keys` locales), qué copiar dónde, activación de cfggameplay, borrado del mundo en cada reinicio, ajustes del BR y cómo migrar desde 0.1.0 (borrar los JSON viejos del perfil). |

**`cfggameplay.json`, las 5 líneas cambiadas**
- `PlayerData.WeaponObstructionData.staticMode` y `dynamicMode` pasan de 1 a 2. El 2 es `EWeaponObstructionMode.ALWAYS`, «obstructs and never lifts» (`scripts/3_game/cfggameplaydatajson.c:375-380,395-396`; se lee en `4_world/entities/firearms/weapon_base.c:1421-1431`). Es el modo que sustituye a `extra_preventweaponraise` (EXT-08).
- Otros tres valores, para que activar el fichero no cambie nada más respecto a como iba el servidor con el fichero desactivado:
  - `staminaWeightLimitThreshold` pasa de 6000 a 6000000. Es el valor del mod (`Constants.c:7`); el JSON pisaría el valor por defecto que toma `GameConstants` (`cfggameplaydatajson.c:134`). Es lo que propone SRV-03.
  - `disablePersonalLight` pasa de false a true. El cfg de stock trae `disablePersonalLight = 1`, pero solo se lee con el JSON desactivado (`cfggameplaydatajson.c:79`).
  - `hitIndicationPostProcessEnabled` pasa de true a false. Es el valor legacy (`cfggameplaydatajson.c:310-313`).
- El resto de campos los comparé con sus valores legacy y coinciden: estamina, shock, movimiento, ahogamiento, temperaturas (`4_world/classes/worlds/chernarusplus.c:42-58`), humedad (`constants.c:961-965`), vehículos y mapa.

**`restart_loop.ps1`**
- Antes de cada arranque borra `mpmissions\<mission>\storage_<instanceId>`. El `instanceId` se lee del cfg; el cfg de stock lo comenta como «storage folders with persistence files», y la carpeta vanilla tiene `storage_-1`, `storage_0` y `storage_1`.
- Después arranca `DayZServer_x64.exe` con `-config`, `-port`, `-profiles`, `-mod`, `-serverMod` y `-dologs -adminlog -freezecheck`, espera a que salga y vuelve a empezar.
- Antes de borrar nada comprueba cinco cosas, y si una falla para sin borrar ni arrancar:
  1. la misión existe y tiene `init.c`;
  2. `template=` del cfg nombra esa misma misión;
  3. el cfg tiene `instanceId`;
  4. no hay ningún servidor de esa carpeta en marcha;
  5. ni la carpeta ni nada dentro es un junction o enlace (LL-393).
- `-WhatIf` hace una pasada en seco.

**Verificación**
- Diff línea a línea contra el vanilla de cada fichero copiado:
  - `globals.xml`: 2 líneas;
  - `cfggameplay.json`: 5 líneas;
  - `serverDZ.cfg.example`: 1 cambiada y 2 añadidas.
- JSON y XML validados con Python (salida al final).
- `restart_loop.ps1` pasado por el parser de PowerShell 5.1.22621: 0 errores, 9 parámetros y 5 funciones. Corregí antes una cadena `"$configPath: ..."` que no parseaba.
- Parámetros de línea de comandos:
  - `serverMod`, `profiles`, `dologs`, `adminlog` y `freezecheck` aparecen como cadenas en la tabla de opciones de `DayZServer_x64.exe`;
  - `-config`, `-profiles`, `-mod`, `-port` y `-freezecheck` están documentados en `DAYZ_INFRA.md:163-169`;
  - `-netlog` no aparece en el binario y no lo uso.

## Gate de lint

`bash tools/lint.sh . build/lint` en la raíz del worktree, con el código final:
```
extra_changefeedbackurl PASS errors 0 warnings 0
extra_defaultfullauto PASS errors 0 warnings 0
extra_limitunconscioustime PASS errors 0 warnings 0
extra_preventplayermodifiers PASS errors 0 warnings 0
extra_preventweaponraise PASS errors 0 warnings 0
extra_spawncarfull PASS errors 0 warnings 0
extra_spawnweaponchambered PASS errors 0 warnings 0
extra_spawnwithammoandmagazine PASS errors 0 warnings 0
extra_spawnwithbattery PASS errors 0 warnings 0
extra_vehicle3ppdefaultfix PASS errors 0 warnings 0
scripts_client FAIL errors 10 warnings 3
scripts_server FAIL errors 2 warnings 12
```
Es la base de common.md. Además comparé los JSON del linter con los de la ejecución sobre el código original: errores y avisos idénticos hallazgo por hallazgo en `scripts_server` y `scripts_client` (los 2 `ES-OVERRIDE-PARAM-NAME-MISMATCH` de `RemovePlayer` y los 10 `ES-LAYOUT-PATH-PBOPREFIX-MISMATCH`).

Validación de JSON y XML (Python, `json.loads` y `xml.etree.ElementTree.parse`):
```
JSON OK  BattleRoyale_server/mpmissions/dayzOffline.chernarusplus/cfggameplay.json | WeaponObstructionData {'staticMode': 2, 'dynamicMode': 2} | staminaWeightLimitThreshold 6000000.0 | disablePersonalLight True | hitIndicationPostProcessEnabled False
XML OK   BattleRoyale_server/mpmissions/dayzOffline.chernarusplus/db/globals.xml | root <variables>, 30 vars, ZombieMaxCount=150 AnimalMaxCount=0
```

## Decisiones tomadas ante ambigüedad (lo conservador)

- **Zonas finales:** amplío `end_avoid_city` para cumplir «primer círculo sin mar» y quitar los arranques fallidos. Es reversible editando `zone_settings.json`, y la herramienta recalcula la lista si cambian las tablas.
- **Borrado del mundo:** el bucle borra solo `storage_<instanceId>`, no todas las `storage_*`. Así no toca el almacenamiento de otras instancias de la misma misión ni copias manuales como `storage_1.bak-*`, que la carpeta vanilla local tiene a decenas.
- **0.1.0:** no hay migración automática de los JSON del perfil. El README dice que hay que borrarlos.
- **cfggameplay.json:** tres valores extra para conservar el comportamiento, justificados arriba.
- **`min_players` no decreciente:** forma parte de «coherente». Las zonas grandes son para más jugadores.

## CROSS-LANE REQUESTS

1. **`4_BattleRoyalePrepare.c:218` (lane de Prepare/spawn):** `a_AvoidCitySpawn.Find(town_entry.Name)` compara el `name=` del config, que en Chernarus va en cirílico («Новоселки»; `dabs_scripts/3_Game/DabsFramework/!Core/NamedLocation.c:21-22`), con nombres de clase. No coincide nunca y `avoid_city_spawn` no tiene efecto. Hay que usar `town_entry.GetName()` (`dabs_scripts/.../ConfigObject.c:12,94-97`, el último token de la ruta, es decir, la clase), como ya hace `:344`. Mientras no se arregle, se puede aparecer en Skalisty o en el lobby si la zona 1 los alcanza.
2. **`BattleRoyaleZone.c` (lane de zona):**
   - La lista `end_avoid_city` por defecto está ajustada a la geometría actual del generador: desplazamiento del 25-75 % hacia el centro, el candidato más lejano de dos (el comentario de `:469` dice «closest», pero `:474-482` se queda con el más lejano) y el cuadrado del mundo. Si ZON-04 o ZON-05 cambian el algoritmo, hay que volver a pasar `BattleRoyale_server/tools/zone_table_check.py`.
   - El `RequestExit` de `:450` sigue haciendo falta quitarlo: en Chernarus mis datos lo bajan del 22 % al 0,025 % de los arranques, pero otro mapa u otras tablas lo reabren.
   - Con `restrict_first_zone` y `end_in_villages` activos, la búsqueda recorre los POI en orden del config (`:262-287`), así que la zona final es siempre el primer POI dentro del polígono. Hoy está desactivado por defecto.
3. **Lane de extras:** retirar `extra_preventweaponraise`, porque sustituye `CheckLiftWeapon` sin `super` (EXT-08). Mientras esté en el build, el modo 2 del `cfggameplay.json` no surte efecto completo.
4. **`BattleRoyaleServerData.c` (otra lane):** la base ahora tiene `Validate()` y `LogInvalidSetting()` (`BattleRoyaleDataBase.c:20-26`) por si quiere validar sus campos. `BattleRoyaleConfig.Load()` ya la llama para todas las configs. Sin override no hace nada.
5. **`5_BattleRoyaleStartMatch.c` (lane de estados, opcional):** mi validación impide la config que deja a los jugadores congelados, pero como defensa en profundidad `Deactivate()` podría desbloquear a quien siga bloqueado (`:74-75` detiene `m_UnlockTimer`).

## LO QUE NO PUDE VERIFICAR

- **Compilación en el motor.** Ni el linter offline ni el parser compilan Enforce; el único gate real es arrancar el módulo, y common.md lo prohíbe. Tampoco hubo ninguna prueba en el juego.
- **Comportamientos de Enforce que asumo:**
  - `array.Copy` sobre un array nuevo (`1_core/proto/enscript.c:499`);
  - la deserialización JSON en un miembro `ref array<int>`;
  - qué hace el cargador con los campos ausentes (los arreglos funcionan en los dos casos);
  - si `Error()` abre un diálogo en DayZDiag.
- **Mapa.** Terreno no visto. Costa aproximada (±200 m). Storozh como isla es una deducción. Los named locations son de la extracción de `P:\DZ` (abril, probablemente 1.29), no del 1.30.
- **Zonas.** Las probabilidades de fallo y de mar, los pueblos por círculo y la duración salen del port a Python y de aritmética, no del juego. Tampoco están medidas la velocidad real de los jugadores ni la duración real de las partidas.
- **Paquete.**
  - El efecto en partida de `AnimalMaxCount = 0` y del tope de 150 infectados frente a los eventos de `events.xml`.
  - El efecto del modo 2 de obstrucción con el extra todavía cargado.
  - Que las claves `.bikey` y los ids de Workshop sigan iguales en el servidor de destino: los saqué de las copias locales.
- **`restart_loop.ps1`.** No se ha ejecutado; solo parseado. La convención `storage_<instanceId>` sale del comentario del cfg de stock y de los nombres de carpeta, no de documentación del motor. `-port` y `-mod` no aparecen como cadenas aisladas en el binario; están documentados en `DAYZ_INFRA.md`.
- **Valores legacy de cfggameplay.json** con el fichero desactivado: los leí en el script, pero no los he medido en un servidor.

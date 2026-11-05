# Auditoría 01: núcleo del servidor y máquina de estados (DayZ-BattleRoyale v0.1.0-Vigrid → DayZ 1.30)

Auditoría de solo lectura. Rutas relativas:
- `EXT/` = `C:\Users\guill\OneDrive\Documentos\DayZ Projects\BattleRoyale_dev\extracted_v0.1.0-vigrid\`
- `V130/` = `C:\Users\guill\OneDrive\Documentos\DayZ Projects\scripts\`
- `V129/` = `C:\Users\guill\OneDrive\Documentos\DayZ Projects\scripts_1.29_backup_20261016\`
- `REF/` = `...\scratchpad\ref\` (CF, Expansion core, COT, VPP extraídos)

Todas las citas `path:line` se han abierto con Read/sed durante la auditoría.

## 0. Resumen

- **No he encontrado ningún error de compilación ni firma rota contra 1.30 en mi área.** Todos los `override` y `modded` comprobados existen en 1.30 con la misma firma (tabla en §1.1). Los 2 FAIL del linter (`RemovePlayer`) son **falsos positivos** (SRV-01).
- Lo grave para 120 jugadores son **bloqueos de la máquina de estados** (el servidor no muere, pero la partida queda atascada para siempre) y **bucles sin tope en el hilo principal**:
  - SRV-10: si alguien se desconecta durante `Prepare` con el menú de spawn activo (es el valor por defecto), hay un índice fuera de rango, el hilo `ProcessPlayers` aborta, `Deactivate()` no llega a llamarse y la partida se queda en Prepare con todos congelados y en godmode.
  - SRV-11: hay varios `while(true)` sin tope en la búsqueda de posiciones de spawn y de zona. Pueden dejar colgado el servidor.
  - SRV-12: si llega un NULL de identidad o de jugador en `HandleUnlock` o en `DisableInput`, el hilo aborta y el resto de jugadores se queda bloqueado.
- **Seguridad**: hay RPC de cliente sin validar.
  - `NextState` no tiene ningún control de permisos con VPP cargado (SRV-20).
  - Spawn: un cliente puede elegir cualquier coordenada del mapa o fijarla para otro jugador (SRV-21).
- El juego en sí no tiene coste por tick O(n²) grave: el scheduler vanilla solo ticka 5 jugadores por frame. Los costes serios son picos puntuales:
  - teletransporte en Prepare (hasta 10 000 `IsSafeForTeleport` en un frame);
  - broadcast del heatmap en SpawnSelection, con un payload O(n²);
  - `UpdateTopPosition`, que envía n RPC idénticos en cada muerte.

## 1. Cambios NECESARIOS para 1.30

### 1.1 Superficie vanilla verificada (sin cambios necesarios)

| Uso en el mod | Cita mod | Firma 1.30 | Estado |
|---|---|---|---|
| `MissionServer.OnInit/OnUpdate/OnEvent` | `EXT/scripts_server/5_Mission/Mission/MissionServer.c:7,16,37` (líneas del fichero real, ver nota) | `V130/5_mission/mission/missionserver.c:85,108,309` | OK |
| `InvokeOnConnect(PlayerBase, PlayerIdentity)` | MissionServer.c (mod) | `V130/.../missionserver.c:435` | OK |
| `EquipCharacter(MenuDefaultCharacterData)` | MissionServer.c (mod) | `V130/.../missionserver.c:511` | OK |
| `PlayerDisconnected(PlayerBase, PlayerIdentity, string)` | MissionServer.c (mod) | `V130/.../missionserver.c:690` | OK |
| `HandleBody(PlayerBase)` | MissionServer.c (mod) | `V130/.../missionserver.c:748` | OK |
| `OnClientRespawnEvent(PlayerIdentity, PlayerBase)` | MissionServer.c (mod) | `V130/.../missionserver.c:611` | OK |
| `OnClientDisconnectedEvent(PlayerIdentity, PlayerBase, int, bool)` | MissionServer.c (mod) | `V130/.../missionserver.c:641` | OK |
| `PlayerBase.OnScheduledTick/EEKilled/EEHitBy/OnUnconsciousStop/OnSyncJuncture/Save/ResetPlayer` | `EXT/scripts_server/4_World/Entities/ManBase/PlayerBase.c:36,45,54,75,85,111,169` | `V130/4_world/entities/manbase/playerbase.c:3007,1214,1255,3819,8478,8057,7936` | OK |
| `CfgGameplayHandler.GetHoldBreathStaminaModifier()` static | `EXT/scripts_server/3_Game/CfgGameplayHandler.c:4` | `V130/3_game/cfggameplayhandler.c:242` (1.29: `:232`) | OK |
| `WorldData.CalcBaseEnvironmentTemperature`, `m_MaxTemps[12]` | `EXT/scripts_server/3_Game/WorldData.c:4` | `V130/3_game/worlddata.c:133,23` | OK |
| Constantes `GameConstants.STAMINA_*` y `PlayerConstants.HEALTH_REGEN_*`, `BLOOD_REGEN_RATE_PER_SEC` | `EXT/scripts_server/3_Game/Constants.c:3-7`, `PlayerConstants.c:4-6` | `V130/3_game/constants.c:705,706,708,746`; `V130/3_game/playerconstants.c:100,149,150` | Existen |
| `Grenade_Base.OnUnpin` (protected), `EEKilled` | `EXT/scripts_server/4_World/Entities/ItemBase/Grenade_Base.c:12,26` | `V130/4_world/entities/itembase/grenade_base.c:188`; EntityAI | OK |
| `LandMineTrap.EEKilled/OnPlacementComplete` | `.../TrapBase/LandMineTrap.c:11,25` | `V130/4_world/entities/itembase/trapbase/trap_landmine.c:204`; `V130/3_game/entities/entityai.c:613` | OK |
| `Land_Radio_PanelPAS.EEInit` | `.../AdvancedCommunication/Land_Radio_PanelPAS.c:4` | `V130/4_world/entities/advancedcommunication/land_radio_panelpas.c:1` | OK, es un override vacío |
| `DisconnectPlayer(identity, uid="")`, `RequestExit`, `GetWorldName()` (wrapper string), `GetDate/SetDate`, `SurfaceIsPond`, `SurfaceGetType`, `IsBoxCollidingGeometry`, `SphereCastBullet`, `CallLaterByName`, `GameScript.Call`, `SendSyncJuncture`, `SetAllowDamage`, `RemoveAllItems`, `SetQuickBarEntityShortcut`, `ServerDropEntity`, `SetHealthMax`, `DecreaseHealthCoef`, `QueueUpPrimarySymptom`, `SYMPTOM_PAIN_LIGHT/HEAVY`, `MuteAllPlayers`, `EnableVoN`, `HumanInputController.SetDisabled/Override*`, `JsonFileLoader<T>.LoadFile/SaveFile/MakeData/JsonMakeData`, `EnScript.GetClassVar`, `GetGame()` | varios | `V130/3_game/global/game.c:399,218,932-939,1197,1180,1365,1048,1055`; `V130/3_game/global/world.c:28,46`; `V130/3_game/global/dayzphysics.c:213`; `V130/2_gamelib/tools.c:63`; `V130/1_core/proto/enscript.c:139,188`; `V130/3_game/dayzplayer.c:1303`; `V130/3_game/entities/object.c:1196,1105,1120`; `V130/4_world/entities/manbase/playerbase.c:1625,2411`; `V130/3_game/entities/man.c:147`; `V130/4_world/classes/playersymptoms/statemanager.c:22-23,534`; `V130/3_game/human.c:22,265,268,274`; `V130/3_game/tools/jsonfileloader.c:7,42,85,162`; `V130/3_game/gameplay.c:636` | Existen |
| APIs del `ResetPlayer` del mod (`DamageSystem.GetDamageZoneMap`, `DamageZoneMap`, `ClearContents`, `IsModifierActive`, `DeactivateAllModifiers`, `ActivateModifier`, `MDF_AREAEXPOSURE/MDF_MASK`, `ResetAllStats`, `PlayerAgentPool.RemoveAllAgents`, `StaminaHandler.SetStamina(float)`, `SendPlayerUnconsciousness`, `UpdateBloodyHandsVisibilityEx`) | `PlayerBase.c:169-240` (mod) | `V130/3_game/damagesystem.c:27,156`; `V130/4_world/classes/playerstomach.c:158`; `V130/4_world/classes/playermodifiers/modifiersmanager.c:207,221,227`; `.../emodifiers.c:53-54`; `.../playerstats/playerstats.c:130`; `.../playeragentpool/playeragentpool.c:218`; `V130/4_world/classes/staminahandler.c:1075`; `V130/4_world/entities/dayzplayersyncjunctures.c:168`; `V130/4_world/plugins/pluginbase/pluginlifespan.c:412` | Existen |
| `ExpansionNotification(string, CF_Localiser, ...)`, `COLOR_EXPANSION_NOTIFICATION_INFO`, `StringLocaliser`, `GetRPCManager().AddRPC/RemoveRPC/SendRPC` | `0_BattleRoyaleState.c:179,191` | `REF/exp_core_scripts/.../ExpansionNotificationSystem.c:176`; `REF/exp_core_scripts/.../ExpansionConstants.c:59`; `REF/cf_scripts/.../CF_Localiser.c:243`; `REF/cf_scripts/.../RPCManager.c:111,189` | Existen |

Nota: `MissionServer.c` del mod son las líneas 553-736 del volcado concatenado; dentro del fichero `EXT/scripts_server/5_Mission/Mission/MissionServer.c` corresponden a las líneas 1-184.

### SRV-01: Los 2 FAIL del linter (`RemovePlayer`) son falsos positivos
- **Severidad**: Baja (no hay que cambiar nada).
- **Evidencia**:
  - Los FAIL están en `EXT/scripts_server/5_Mission/BattleRoyale/Server/States/1_BattleRoyaleDebug.c:168` y `4_BattleRoyalePrepare.c:715`. La cadena de herencia es `BattleRoyaleDebug : BattleRoyaleDebugState : BattleRoyaleState : Timeable` (`0_BattleRoyaleState.c:2,667`), y `Timeable` es una clase propia del mod (`EXT/scripts_server/3_Game/Logic/Timeable.c:2`).
  - El único `RemovePlayer(string UID)` vanilla pertenece a `PlayerListScriptedWidget` (`V130/5_mission/gui/ingamemenu_xbox/playerlistscriptedwidget.c:184`). `grep "class Timeable"` en V130 y en REF da 0 resultados.
- **Propuesta**: ninguna. Hay que anotarlos como excepción en el gate del linter.
- **Esfuerzo**: S. **Confianza**: alta.

### SRV-02: `NamedLocation` (Dabs Framework) no está verificado y es una dependencia implícita
- **Severidad**: Alta si falta DF; si no, Baja.
- **Evidencia**:
  - `4_BattleRoyalePrepare.c:17,164-275,308-406` usa `NamedLocation(string)`, `.Name`, `.Type`, `.Position`, `.GetName()`, `NamedLocation.CITY/CAPITAL`.
  - El comentario de `4_BattleRoyalePrepare.c:168` apunta a DabsFramework.
  - Ni `EXT/scripts_client/config.cpp` (requiredAddons: `DZ_Data`, `DZ_Scripts`, `DayZExpansion_Scripts`) ni el config del servidor (`EXT/scripts_server/config.cpp`) declaran DF. `class NamedLocation` no aparece en REF ni en EXT.
- **Propuesta**: comprobar que la versión 1.30 de Expansion sigue arrastrando DabsFramework. Añadir explícitamente el CfgPatches de DF a `requiredAddons` del cliente (el nombre exacto hay que verificarlo en el PBO de DF) y validar las firmas de `NamedLocation` contra su fuente.
- **Esfuerzo**: S. **Confianza**: media (no he podido abrir DF).

### SRV-03: `GameConstants.STAMINA_WEIGHT_LIMIT_THRESHOLD` se ignora si `cfggameplay.json` está activo
- **Severidad**: Media (comportamiento).
- **Evidencia**:
  - El mod lo pone a 6 000 000 en `EXT/scripts_server/3_Game/Constants.c:7`.
  - Vanilla solo usa la constante como valor por defecto del JSON (`V130/3_game/cfggameplaydatajson.c:134`). El stamina handler lee el getter del JSON (`V130/4_world/classes/staminahandler.c:659`, `V130/3_game/cfggameplayhandler.c:212-214`).
  - Si el servidor tiene `enableCfgGameplayFile=1` con `staminaWeightLimitThreshold` en su JSON, prevalece el valor del fichero. No es nuevo de 1.30: 1.29 también usa el getter (2 hits en `V129/4_world/classes/staminahandler.c`). Pero para un refurbish hay que decidirlo.
  - `Constants.c` es el único fichero del servidor sin `#ifdef SERVER`.
- **Propuesta**: fijar el valor también en el `cfggameplay.json` de la misión (y los modificadores de sprint a 0 si se quiere stamina infinita), o retirarlo del `Constants.c`. Documentar que el cliente también necesita los mismos valores para no desincronizar la predicción de stamina. Hay que verificar si el PBO del servidor se distribuye al cliente; no lo he verificado.
- **Esfuerzo**: S. **Confianza**: alta en el mecanismo, media en el impacto real (depende del cfggameplay del servidor).

### SRV-04: `ResetPlayer` del mod reemplaza el de vanilla sin llamar a `super` y omite reseteos de 1.30
- **Severidad**: Baja.
- **Evidencia**:
  - El vanilla 1.30 (`V130/4_world/entities/manbase/playerbase.c:7936-7998`) está entero dentro de `#ifdef DIAG_DEVELOPER`, así que en retail no hace nada; el override del mod es lo que realmente resetea.
  - El de 1.30 hace además cosas que el mod no hace (`:7962`, `:7977-7984`): `m_DiseaseCount = 0`, `ClearBloodyHandsPenaltyChancePerAgent`, `GetArrowManager().ClearArrows()` y `GetThermalBiasHandler().SetTemporaryResistance(0)`. `ThermalBiasHandler` es nuevo de 1.30: no aparece en `V129/4_world`.
- **Propuesta**: añadir al `ResetPlayer` del mod, en su estilo, `ClearArrows()` (cosmético y físico) y el reseteo de thermal bias, con guardas null como en vanilla.
- **Esfuerzo**: S. **Confianza**: alta.

## 2. Optimizaciones para 100-120 jugadores

Contexto medido en el código: el scheduler vanilla solo ticka `SCHEDULER_PLAYERS_PER_TICK = 5` jugadores por frame (`V130/5_mission/mission/missionserver.c:12,766-784`). Por eso `PlayerBase.OnScheduledTick` → `BattleRoyaleServer.OnPlayerTick` (`EXT/.../PlayerBase.c:36-43`, `BattleRoyaleServer.c:354-404`) se ejecuta 5 veces por frame, no 120. El `ContainsPlayer` O(n) (`0_BattleRoyaleState.c:128-131`) cuesta unas 600 comparaciones por frame: no es un cuello de botella. Efecto secundario: con 120 jugadores a ~30 FPS, cada jugador se ticka cada ~0,8 s, lo que marca la granularidad del daño de zona y del rebote del lobby.

### SRV-30: Teletransporte de Prepare: hasta ~10 000 comprobaciones de colisión en un frame por jugador
- **Severidad**: Alta (pico de frame, rubber-banding).
- **Evidencia**:
  - Para cada jugador, `GetRandomSpawnPosition` (`4_BattleRoyalePrepare.c:318-375`) intenta hasta 200 pueblos × 50 posiciones (`:352`), y cada una hace `IsSafeForTeleport`.
  - `IsSafeForTeleport` (`0_BattleRoyaleState.c:244-328`) hace surface checks, `SphereCastBullet`, `IsBoxCollidingGeometry` y 3 allocs de arrays. Con `check_zone` además ejecuta `GetDynamicStartingZone` con su bucle de trazas (`:520-551`).
  - Todo ocurre sin `Sleep` entre intentos: el `Sleep(100)` va por jugador (`:548,567,606`).
  - Además, todas las llamadas a `BattleRoyaleUtils.Trace("..." + x)` concatenan strings aunque el log esté apagado (`EXT/scripts_client/3_Game/BattleRoyaleUtils.c:15-17` filtra después de construir el argumento).
- **Coste relativo**: el caso típico es de pocos intentos. El caso malo (pueblos llenos o en el borde de la zona) es de miles de raycasts en un frame. Es el mayor pico del mod.
- **Propuesta**:
  - Meter un `Sleep(0)` o un `Sleep(1)` cada N intentos dentro de los bucles internos (el hilo ya es cooperativo).
  - Cachear `GetDynamicStartingZone(i_NumStartingPlayers)` y su zona en un miembro al activar Prepare.
  - Mover `bad_surface_types` y los arrays de `IsSafeForTeleport` a miembros estáticos.
  - Envolver las trazas de bucles calientes en `#ifdef BR_TRACE_ENABLED`, como ya se hace en `:310`.
- **Esfuerzo**: M. **Confianza**: alta en la estructura; el coste en ms no está medido.

### SRV-31: Duración de Prepare lineal con 4 pasadas × 100 ms por jugador (~49 s con 120)
- **Severidad**: Media.
- **Evidencia**: `4_BattleRoyalePrepare.c:512-518` (disable), `:523-528` (items), `:537-568`/`:601-608` (teleport), `:666` (1 s) y `:668-673` (reset). Cada pasada lleva `Sleep(100)` por jugador: 120 × 0,4 s + 1 s ≈ 49 s con la pantalla en negro.
- **Propuesta**: agrupar las tres operaciones baratas (disable input, items, reset) en una sola pasada de lotes de 5-10 jugadores con un `Sleep(100)` por lote, y dejar el teleport con su sleep. Así baja a ~15-20 s.
- **Esfuerzo**: S-M. **Confianza**: alta (aritmética sobre el código).

### SRV-32: Heatmap de SpawnSelection: broadcast O(n²) por selección
- **Severidad**: Media-Alta.
- **Evidencia**: en cada `OnPlayerSpawnSelected` se reconstruye un array con todos los puntos y se envía a todos (`3_BattleRoyaleSpawnSelection.c:423-435`). Con 120 jugadores y repeticiones de clic, el total ronda 120 envíos × 120 clientes × hasta 120 vectores (unos 1,4 KB): del orden de 10 MB en 30 s, más un alloc de array por clic. No hay rate-limit por jugador.
- **Propuesta**: marcar `b_HeatmapDirty` y enviar el heatmap desde un timer en bucle de 1 s (`AddTimer(1.0, this, "FlushHeatMap", NULL, true)`, que ya se limpia en `Deactivate`). Ignorar las selecciones repetidas del mismo jugador en menos de 250 ms.
- **Esfuerzo**: S. **Confianza**: alta.

### SRV-33: `UpdateTopPosition` envía n RPC idénticos en cada cambio de recuento
- **Severidad**: Media.
- **Evidencia**: `0_BattleRoyaleState.c:164-172` envía `SetTopPosition` jugador a jugador con el mismo valor, y se llama desde `OnPlayerCountChanged` (`:138-162`) en cada `AddPlayer`/`RemovePlayer` con el estado activo. Durante una partida de 120 suma ~7 000 RPC. En el lobby pasa lo mismo con cada conexión.
- **Propuesta**: un solo `SendRPC(..., "SetTopPosition", ..., true)` en broadcast (sin identity), manteniendo el bucle solo para `SetBRPosition`. Se pueden fusionar `SetPlayerCount` y `SetTopPosition` en un RPC, pero eso exige tocar el cliente.
- **Esfuerzo**: S. **Confianza**: alta.

### SRV-34: `GetGroupsCount()` (con Carim) se evalúa cada 0,1 s con allocs
- **Severidad**: Baja-Media.
- **Evidencia**: `BattleRoyaleServer.c:176-198` llama a `IsComplete` cada 0,1 s. `IsComplete` de Debug, Round y LastRound llama a `GetGroupsCount()` (`1_BattleRoyaleDebug.c:39`, `6_BattleRoyaleRound.c:252`, `7_BattleRoyaleLastRound.c:132`), que construye un `map` y un `set` de n entradas (`0_BattleRoyaleState.c:336-394`). Son ~10 allocs/s de estructuras de 120.
- **Propuesta**: cachear el recuento de grupos en `OnPlayerCountChanged` (que ya lo calcula, `:147`) y leer la caché en `IsComplete`.
- **Esfuerzo**: S. **Confianza**: alta.

### SRV-35: Pequeños allocs y trazas en caminos frecuentes
- **Severidad**: Baja.
- **Evidencia**:
  - `GetPreviousZone()` hace un `Trace` en cada tick de jugador cuando el estado anterior no es una ronda (`6_BattleRoyaleRound.c:362`), vía `GetActiveZone` (`:368-374`) desde `OnPlayerTick` (`:306`).
  - `BattleRoyaleConfig.Get*Data()` trazan en cada acceso (`BattleRoyaleConfig.c:155-192`) y se llaman en caminos de jugador (p. ej. `0_BattleRoyaleState.c:246,522-523,564,599`).
  - `WorldData.CalcBaseEnvironmentTemperature` recorta `m_MaxTemps` en cada cálculo (`WorldData.c:6-10`).
- **Propuesta**:
  - Cachear la zona anterior en `Activate`.
  - Quitar las trazas de los getters de config.
  - Mover el recorte de `m_MaxTemps` a un `InitWorld` o hacerlo una sola vez con un flag. Ojo: el bucle usa `< 11` y deja diciembre sin recortar (SRV-48).
- **Esfuerzo**: S. **Confianza**: alta.

### SRV-36: `POST_now` bloqueante en el arranque
- **Severidad**: Baja (solo una vez).
- **Evidencia**: `BattleRoyaleServer.c:59-60` → `EXT/scripts_server/3_Game/BattleRoyale/Webhook/Matches/CreateMatchWebhook.c:42` (`ctx.POST_now`). Bloquea el hilo principal durante `OnInit` hasta que responde la API. Los webhooks por evento son asíncronos (`EventWebhook.c:41`, `ctx.POST`).
- **Propuesta**: ninguna para 120 jugadores; solo afecta al arranque. Que otro auditor revise el timeout de la RestApi.
- **Esfuerzo**: S. **Confianza**: media.

## 3. Bugs lógicos y robustez de la máquina de estados

### SRV-10: Una desconexión durante Prepare provoca índice fuera de rango y la partida se queda atascada
- **Severidad**: Alta. Es una excepción que deja la partida colgada, no un crash del proceso.
- **Evidencia**:
  - `pCount` se fija al principio (`4_BattleRoyalePrepare.c:510`). En la rama de spawn selection (activa por defecto, `BattleRoyaleGameData.c:228`) se indexa `GetPlayers()[i]` (`:538`), que es `m_Players` y encoge con cada desconexión.
  - Las desconexiones llaman a `RemovePlayer` (`BattleRoyaleServer.c:328-337` → `4_BattleRoyalePrepare.c:715-724` → `0_BattleRoyaleState.c:111-116`).
  - Con una sola desconexión antes de la tercera pasada, `i = pCount-1` queda fuera de rango, la excepción aborta el hilo `ProcessPlayers` y `Deactivate()` (`:676`) no se ejecuta nunca.
  - Ningún otro camino completa Prepare: no sobreescribe `IsComplete` con condición propia (`:83-87`). Todos quedan en negro (`SetFade`, `:55`), sin input (`:60`) y en godmode (`:696`).
- **Propuesta**: usar `a_PlayerList[i]` (la lista estática que ya existe precisamente para esto, `:57-58`) en `:538`, y acotar el bucle por `a_PlayerList.Count()`. Añadir un watchdog en Prepare: si tras X s el hilo no ha llamado a `Deactivate`, desactivar igualmente.
- **Esfuerzo**: S. **Confianza**: alta en el índice; media-alta en que el OOB aborte la función, que es el comportamiento habitual de Enforce, pero no lo he reproducido.

### SRV-11: Bucles `while(true)` sin tope en el hilo principal pueden colgar el servidor
- **Severidad**: Alta (hang: el proceso deja de responder).
- **Evidencia**:
  - Spawn aleatorio de último recurso (`4_BattleRoyalePrepare.c:383-403`). No tiene `Sleep` ni tope, y comprueba zona: si la zona inicial cae en mar o colisiona entera, el bucle es infinito.
  - `BattleRoyaleZone.GetValidPositionSquare` (`BattleRoyaleZone.c:368-379`).
  - `GetRandomPOI` (`:550-563`). Además hace `s_POI.Get(RandomInt(0, 0))` fuera de rango si no hay POIs, por ejemplo con `end_avoid_*` demasiado amplio o en un mapa sin `CfgWorlds Names`.
  - `GetValidPositionNewCircle`: la rama `continue` de `!IsSafeZoneCenter` (`:457-461`) no consulta `max_try`. Solo la rama «fuera del mundo» llega a `RequestExit` (`:418-451`).
  - El grupo en `TeleportGroup` (`4_BattleRoyalePrepare.c:460-473`) sí tiene tope de 50.
- **Propuesta**: tope en todos (por ejemplo 1000) y fallback determinista: centro de la zona actual para el spawn, o el último candidato válido para la zona. Errores con `BattleRoyaleUtils.Error` en lugar de colgarse. La generación de zonas ocurre en `OnInit` (constructores de `BattleRoyaleRound` → `BattleRoyaleZone.GetZone` → `Init`; `6_BattleRoyaleRound.c:57-73`, `BattleRoyaleZone.c:71`): un hang ahí impide arrancar.
- **Esfuerzo**: S. **Confianza**: alta en el código; la probabilidad depende del mapa y la config.

### SRV-12: NULL de identidad o de jugador en hilos de desbloqueo y bloqueo deja a todos congelados
- **Severidad**: Alta.
- **Evidencia**:
  - `HandleUnlock` (`5_BattleRoyaleStartMatch.c:138-145`) hace `player.GetIdentity().GetName()` sin guarda y luego `player.DisableInput(false)`.
  - `DisableInput` del cliente/compartido hace `GetIdentity().GetPlainId()` sin guarda (`EXT/scripts_client/4_World/Entities/ManBase/PlayerBase.c:44,50`). Se usa también en Prepare (`4_BattleRoyalePrepare.c:159-162,515`).
  - Si un jugador está en el temporizador de logout o su entidad se ha borrado, la excepción aborta el bucle y todos los jugadores posteriores de la lista quedan sin input. En StartMatch no hay reintento.
  - `Unstuck` diferido (`5_BattleRoyaleStartMatch.c:165,173-209`) también desreferencia `player` sin comprobar tras 1-3 s.
- **Propuesta**: comprobar `if (!player || !player.GetIdentity()) continue;` en `HandleUnlock`, `ProcessPlayers`, `Unstuck`, `TeleportPlayer` (`:482`) y `TeleportGroup` (`:457`). En `DisableInput`, guardar la identidad antes de `MuteAllPlayers`.
- **Esfuerzo**: S. **Confianza**: media. No he verificado en qué instante el motor anula `GetIdentity()` durante el logout; el riesgo de NULL pointer es real en cuanto la entidad desaparece.

### SRV-13: Late-joiners: un único `m_Timer` compartido hace que solo se expulse al último
- **Severidad**: Media.
- **Evidencia**:
  - `BattleRoyaleServer.c:278` reutiliza el miembro `m_Timer` (`:14,80`) con `Run(30, ..., "Disconnect", ...)`: cada nueva conexión fuera del lobby reinicia el mismo timer y solo se desconecta al último.
  - Los demás quedan fuera de cualquier estado. En `OnPlayerTick` solo reciben un `SendLogoutTime(player, 0)` una vez (`:381-398`), que informa del logout pero no desconecta: `V130/.../missionserver.c:659,684` lo usa solo como aviso.
  - Además, `Disconnect(identity)` ejecutado 30 s después puede recibir una identidad ya liberada.
- **Propuesta**: un `CallLater` por jugador (`GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(Disconnect, 30000, false, identity)`). En `Disconnect`, comprobar que la identidad sigue existiendo, o mejor, guardar el UID y resolverlo.
- **Esfuerzo**: S. **Confianza**: alta.

### SRV-14: Los admins conectados a mitad de partida reciben igualmente `SendLogoutTime(0)`
- **Severidad**: Media.
- **Evidencia**: la excepción de admins (`BattleRoyaleServer.c:268-275`) devuelve sin añadirlos al estado. Pero `OnPlayerTick` (`:381-398`) envía `SendLogoutTime(player, 0)` a cualquier jugador vivo que no esté en el estado, admins incluidos.
- **Propuesta**: comprobar `admins_steamid64` también en la rama `else` de `OnPlayerTick`, o meter a los admins en un set de espectadores.
- **Esfuerzo**: S. **Confianza**: media. El efecto exacto de `SendLogoutTime` en el cliente no lo he verificado.

### SRV-15: Inconsistencia de la zona inicial: SpawnSelection y StartMatch usan el recuento actual; Prepare y Round usan el de inicio
- **Severidad**: Media.
- **Evidencia**:
  - `i_NumStartingPlayers` se fija en `CountReached.Activate` (`2_BattleRoyaleCountReached.c:224`).
  - SpawnSelection usa `m_Players.Count()` (`3_BattleRoyaleSpawnSelection.c:302`), igual que `ShowFirstZone` (`5_BattleRoyaleStartMatch.c:125`).
  - Prepare usa `i_NumStartingPlayers` (`4_BattleRoyalePrepare.c:316-317`), igual que `IsSafeForTeleport` (`0_BattleRoyaleState.c:251`) y `Round.SkipState` (`6_BattleRoyaleRound.c:280`).
  - Si alguien se va entre CountReached y SpawnSelection y se cruza un umbral de `min_players`, el menú de spawn y el círculo mostrado son de otra zona que la que se juega.
- **Propuesta**: usar siempre `GetDynamicStartingZone(i_NumStartingPlayers)` o, mejor, guardar el índice de zona inicial una sola vez (`static int i_StartingZone`, que ya existe comentado en `0_BattleRoyaleState.c:12`).
- **Esfuerzo**: S. **Confianza**: alta.

### SRV-16: El voto de ready-up usa división entera
- **Severidad**: Media.
- **Evidencia**: `float percent = (ready_count / player_count);` con dos `int` (`1_BattleRoyaleDebug.c:133-148`). El resultado es 0 o 1, así que `ready_up_percent = 0.8` (`BattleRoyaleLobbyData.c:10`) en la práctica exige el 100 %. Además el voto exige `player_count > i_MinPlayers` (`:139`), mientras que el modo sin voto exige `>=` (`:41`).
- **Propuesta**: `float percent = ready_count / (float)player_count;` (o `ready_count * 1.0 / player_count`, en el estilo del autor en `6_BattleRoyaleRound.c:85-86`). Unificar `>=`.
- **Esfuerzo**: S. **Confianza**: alta (semántica de Enforce para int/int).

### SRV-17: `KickWinner` se salta jugadores al borrar mientras itera
- **Severidad**: Baja (Restart apaga el servidor igualmente).
- **Evidencia**: `8_BattleRoyaleWin.c:337-346` hace `RemovePlayer` dentro de `for (k++)` sobre `GetPlayers()`. Con varios ganadores (party), uno de cada dos no recibe `DisconnectPlayer`.
- **Propuesta**: iterar hacia atrás o copiar la lista antes.
- **Esfuerzo**: S. **Confianza**: alta.

### SRV-18: `HandleWinner`: NULL pointer si `CreateObject` falla o el ganador es NULL
- **Severidad**: Baja.
- **Evidencia**: `chicken.SetDirection` sin guarda (`8_BattleRoyaleWin.c:325-329`). `winner.GetIdentity()` con `winner` posiblemente NULL (`:248-249`). `HandleWinner` desreferencia `player_winner.GetIdentity()` (`:296,303,308`).
- **Propuesta**: guardas.
- **Esfuerzo**: S. **Confianza**: alta.

### SRV-19: NULL pointer en el último round cuando no hay zona anterior
- **Severidad**: Baja-Media (depende de la config).
- **Evidencia**: en la rama de última zona, `GetPreviousZone().GetArea()` no tiene guarda (`6_BattleRoyaleRound.c:179-182`). Con `num_zones = 1`, o si la ronda final es la primera activa (zona dinámica inicial = última), `GetPreviousZone()` devuelve NULL (`:351-366`) y se produce un NULL pointer al activar. `LockNewZone` también usa `m_ThisArea` sin guarda (`:395-397`).
- **Propuesta**: aplicar la misma guarda que en `LockNewZone`/`LastRound` (`7_BattleRoyaleLastRound.c:77-78`, que también desreferencia sin guarda en `:81`).
- **Esfuerzo**: S. **Confianza**: alta.

### SRV-40: Desconexión, reconexión y muerte por estado (recorrido R8)

| Estado (índice con spawn selection activa) | Desconexión | Notas / huecos |
|---|---|---|
| Debug (0) | `RemovePlayer` + limpieza de la ready list (`1_BattleRoyaleDebug.c:168-173`). El cuerpo se borra en el radio del lobby (`MissionServer.c` `HandleBody`). | OK |
| CountReached (1) | Solo `RemovePlayer`, sin webhook de puntuación (`BattleRoyaleServer.c:333`). | `i_NumStartingPlayers` no se actualiza (SRV-15). |
| SpawnSelection (2) | `RemovePlayer`; su punto queda en el heatmap. | El índice `> 2` excluye este estado de `OnPlayerDisconnected`/`OnPlayerKilled` de la base (`BattleRoyaleServer.c:333,345`). Sin spawn selection, Prepare pasa a ser el 2 y queda excluido: comportamiento distinto según config (Baja). |
| Prepare (3) | `RemovePlayer` → OOB (SRV-10). | Bloqueo. |
| StartMatch (4) | OK, pero `HandleUnlock` es frágil (SRV-12). Si quedan ≤1, se desactiva antes de desbloquear y el superviviente sigue con input deshabilitado hasta la expulsión de Win (`5_BattleRoyaleStartMatch.c:89-100,71-87`). `m_ShowFirstZone` no se para en `Deactivate` (`:63` frente a `:71-87`). | Baja |
| Rounds / LastRound | `OnPlayerDisconnected` → webhook de puntuación. Inconsciente → kill al último atacante y muerte (`BattleRoyaleServer.c:298-326`). | `EEHitBy` asume `source` no nulo (SRV-41). |
| Win | Ningún webhook; `IsComplete` con 0 jugadores → Restart (`8_BattleRoyaleWin.c:279-285`). | Empate (los dos últimos mueren en el mismo tick): llega a Win con 0 jugadores, aparece `<NO:WINNER>` y Restart. Es correcto, pero sin anuncio ni webhook de empate. |
| Reconexión | Vanilla no llama a `InvokeOnConnect` en `ClientReconnectEvent` (`V130/.../missionserver.c:380-392`): el jugador que vuelve dentro de la ventana de logout conserva su sitio en el estado. Fuera de esa ventana, `Save()` devuelve false (`PlayerBase.c:111-114`) y crea un personaje nuevo → `OnPlayerConnected` → expulsión a los 30 s (con el fallo SRV-13). | Diseño consistente con un BR. |
| Respawn a mitad de partida | `OnClientRespawnEvent` expulsa a quien está fuera del lobby (`MissionServer.c` 689-724 del volcado). | Diseño. |

### SRV-41: `EEHitBy` desreferencia `source` sin guarda
- **Severidad**: Media.
- **Evidencia**:
  - `source.IsPlayer()` (`EXT/scripts_server/4_World/Entities/ManBase/PlayerBase.c:60-63`). La firma vanilla admite `EntityAI source` nulo (`V130/4_world/entities/manbase/playerbase.c:1255`).
  - En vanilla 1.30 la caída usa al propio jugador como source (`V130/4_world/entities/dayzplayerimplementfalldamage.c:155-156`), así que no he encontrado un camino vanilla con NULL. Mods de terceros sí pueden pasar `null` (los vehículos lo hacen consigo mismos: `carscript.c:882`).
  - `GetIdentity().GetName()` en `:71` también puede ser NULL.
- **Propuesta**: `if (!source) { last_unconscious_source = NULL; return; }` tras `super`.
- **Esfuerzo**: S. **Confianza**: media.

### SRV-42: Defaults de POI override mueven Chernogorsk y Novodmitrovsk a la esquina del mapa
- **Severidad**: Media.
- **Evidencia**:
  - Los defaults son `override_poi_positions = { Settlement_Chernogorsk → {100,200}, Settlement_Novodmitrovsk → {300,400} }` (`BattleRoyalePOIsData.c:74-77`).
  - Prepare aplica el override después de filtrar el pueblo por área (`4_BattleRoyalePrepare.c:239` antes de `:344-350`): un jugador puede aparecer en (100, 200).
  - Las zonas usan el override en `InitializePOIs` (`BattleRoyaleZone.c:527-532`).
  - `GetOverrodePosition` llama a `Load()` (solo el perfil) en el primer acceso (`BattleRoyalePOIsData.c:126-128`) y pisa lo cargado desde la misión con `LoadMission`.
  - `new_position` es `array<int>` sin `ref` (`:157`). Con el literal del constructor puede quedar sin referencia fuerte; el NULL pointer en `:137` no está verificado.
- **Propuesta**: default vacío, no llamar a `Load()` dentro del getter y marcar `ref array<int> new_position`.
- **Esfuerzo**: S. **Confianza**: alta en defaults y orden; baja en lo del `ref`.

### SRV-43: El kill de granada o mina usa `EnScript.GetClassVar(..., -1, ...)`
- **Severidad**: Baja.
- **Evidencia**: `0_BattleRoyaleState.c:622-627` con index `-1`. La doc vanilla usa `0` para variables que no son array (`V130/1_core/proto/enscript.c:170-188`). Ya existen getters tipados (`Grenade_Base.c:6`, `LandMineTrap.c:6`).
- **Propuesta**: `Grenade_Base.Cast(source).GetActivatorId()` / `LandMineTrap.Cast(...)`.
- **Esfuerzo**: S. **Confianza**: media. No he verificado qué devuelve con `-1`.

## 4. Seguridad (RPC de cliente)

### SRV-20: RPC `NextState` sin permisos cuando VPP está cargado
- **Severidad**: Alta.
- **Evidencia**:
  - `BattleRoyaleServer.c:21-23` registra `NextState` bajo `#ifdef VPPADMINTOOLS`, y VPP define esa macro (`REF/vpp/VPPAdminTools/Definitions/VPPAdminTools/Definitions.c:1`).
  - El handler (`:536-551`) no comprueba `sender` ni permisos: cualquier cliente que envíe `RPC-DayZBR-Server::NextState` salta el estado actual.
  - CF solo filtra `sender != NULL` (`REF/cf_scripts/.../RPCManager.c:68-75`).
  - El permiso `MenuBattleRoyaleManager` sí se registra (`:44`), pero no se consulta.
  - Fuera de mi área, aunque relacionado: `BRMasterControlsModule` (`EXT/scripts_server/5_Mission/COT/GUI/BRMasterControlsModule.c:11-49`) tampoco comprueba permisos en Next/Pause/Resume/SpawnAirdrop. Que lo confirme el auditor de COT, porque la base JM puede filtrar antes.
- **Propuesta**: en `NextState`, `if (!GetPermissionManager().VerifyPermission(sender.GetPlainId(), "MenuBattleRoyaleManager")) return;`. La API exacta de VPP está por verificar.
- **Esfuerzo**: S. **Confianza**: alta.

### SRV-21: `OnPlayerSpawnSelected`, `PlayerReadyUp` y `PlayerUnstuck` confían en `target` y en la posición del cliente
- **Severidad**: Alta.
- **Evidencia**:
  - El handler de spawn toma `target` en lugar de `sender` (`3_BattleRoyaleSpawnSelection.c:418`): un cliente puede fijar el spawn de otro jugador.
  - La posición no se valida contra la zona inicial ni contra el radio. `Prepare` la usa con `IsSafeForTeleport(..., check_zone=false)` (`4_BattleRoyalePrepare.c:554`, `:423`): un cliente modificado puede aparecer en cualquier punto del mapa.
  - `PlayerReadyUp` y `PlayerUnstuck` también usan `target` (`BattleRoyaleServer.c:443-447,475-479`). Cualquiera puede marcar como listo a otro o teletransportarlo en StartMatch.
  - `RequestEntityHealthUpdate` se registra (`:20`) sin método en la clase. Es inocuo: CF llamará a una función que no existe.
- **Propuesta**:
  - Resolver el jugador desde `sender` (`PlayerBase.Cast(sender.GetPlayer())`) y exigir `target == ese jugador`.
  - Rechazar la posición si queda fuera de la zona inicial más `extra_spawn_radius` o del mapa.
  - Aplicar un rate-limit por jugador.
  - Retirar el `AddRPC` huérfano.
- **Esfuerzo**: S-M. **Confianza**: alta.

## 5. Otras mejoras

### SRV-44: «Vigrid» no es un mapa
- **Severidad**: informativa.
- **Evidencia**: es el nombre del proyecto y de la API (`EXT/scripts_client/2_GameLib/BattleRoyaleConstants.c:45,49`: `github.com/LeMyst/Vigrid-BattleRoyale`, `api.vigrid.ovh`). El mapa sale de `CfgWorlds <world> Names` (`4_BattleRoyalePrepare.c:175-177`, `BattleRoyaleZone.c:505`). Los defaults son de Chernarus (`BattleRoyaleSpawnsData.c:15-24`, `BattleRoyaleZoneData.c:169-170`), con casos especiales para takistanplus y Namalsk (`BattleRoyaleServer.c:513`, `0_BattleRoyaleState.c:267-271`).
- **Propuesta**: para 120 jugadores, revisar `static_sizes`, `min_players` y `num_zones`. Ahora `num_zones = 6` (`BattleRoyaleGameData.c:225`) frente a 7 tamaños, y con 120 jugadores la primera zona es de 3375 m. Revisar también `spawn_point` del lobby (`BattleRoyaleSpawnsData.c:6`) para el mapa elegido.

### SRV-45: Validación de config ausente
- **Severidad**: Baja-Media.
- **Evidencia**: no se valida que `static_sizes`, `static_timers` y `min_players` tengan longitud ≥ `num_zones`:
  - el check es `i > Count()` en vez de `>=` (`BattleRoyaleZone.c:219`);
  - hay índices directos en `:224`, `:146` y `:154`.
- **Propuesta**: validar en `BattleRoyaleZoneData.Load` y abortar con un mensaje claro.

### SRV-46: `LobbyData.Upgrade` duplica items
- **Severidad**: Baja.
- **Evidencia**: el JSON v1 recibe un `Insert` de todos los defaults otra vez (`BattleRoyaleLobbyData.c:84-99`).

### SRV-47: Logs
- **Severidad**: Baja.
- **Evidencia**: el error de UUID se registra a nivel Trace (`BattleRoyaleServer.c:66,72` → `BattleRoyaleUtils.c:10-13`), así que en producción no se ve. El password del servidor se traza (`BattleRoyaleServer.c:58`).
- **Propuesta**: `Error`/`Warn` para el primero y quitar la traza del password.

### SRV-48: `WorldData` recorta solo 11 de los 12 meses
- **Severidad**: Baja.
- **Evidencia**: `WorldData.c:6`.

### SRV-49: `Timeable.AddTimer` no guarda los timers que no son de bucle
- **Severidad**: Baja.
- **Evidencia**: `Timeable.c:18-27`. Si quien llama no guarda la referencia, el timer se libera: la cola vanilla es `array<TimerBase>` débil (`V130/3_game/tools/tools.c:225,362-364`). Hoy todos los llamadores la guardan; es una trampa para código futuro.
- **Propuesta**: documentarlo o guardar todos los timers.

## Qué NO he verificado
- No he compilado: no hay prueba de que el conjunto compile en 1.30. Me baso en la existencia y la firma de cada API (tabla §1.1).
- `NamedLocation` (DabsFramework), `ExpansionMissionModule.CallAirdrop` (Expansion Missions) y la API de partidas de Carim: no estaban en REF.
- El comportamiento runtime de un OOB de array en un hilo `GameScript.Call` (SRV-10), el momento exacto en que `GetIdentity()` pasa a NULL durante el logout (SRV-12) y el efecto de `SendLogoutTime(0)` en el cliente (SRV-13/14).
- Que redeclarar `const` en `modded class GameConstants` (`Constants.c`) siga funcionando en 1.30. Es un patrón histórico; sin compilar no puedo asegurarlo.
- Si el PBO del servidor se carga también en el cliente (afecta a SRV-03).
- Los costes en ms: solo hay estimaciones por conteo de operaciones, sin perfilar.
- Los webhooks, la GUI cliente, COT y los `extra_*`: fuera de mi área, salvo las menciones puntuales.

# Auditoría 03: red y cliente de juego (DayZ-BattleRoyale v0.1.0-Vigrid → DayZ 1.30)

Auditoría de solo lectura. Rutas abreviadas:
- `EXT` = `C:\Users\guill\OneDrive\Documentos\DayZ Projects\BattleRoyale_dev\extracted_v0.1.0-vigrid\`
- `V130` = `C:\Users\guill\OneDrive\Documentos\DayZ Projects\scripts\` · `V129` = `...\scripts_1.29_backup_20261016\`
- `REF` = `C:\Users\guill\AppData\Local\Temp\claude\72e0954b065197d7\151ed9c3-2d0a-403f-9ce8-abb72cfff50c\scratchpad\ref\`

## Resumen

- **Bloqueante-1.30 en mi área: ninguno encontrado.** Todos los overrides de cliente que comprobé siguen casando con 1.30, nombres de parámetros incluidos (tabla en §1). Los 2 FAIL del linter (`RemovePlayer(player)` frente a `UID`) son de estados del servidor y caen fuera de mi área.
- **Lo grave es la seguridad del contrato RPC.** Cuatro handlers de servidor confían en el `target` que manda el cliente, en vez de en `sender`, y tres canales de admin no comprueban permisos. Cualquier cliente puede pasar estados, pausar la partida, marcar listos a los demás o fijarles el spawn.
- Para 100-120 jugadores, la red está bastante bien diseñada: hay pocos broadcasts, la cuenta atrás se interpola en el cliente y las zonas van una vez por ronda. Hay dos excepciones de coste O(N) y O(N²): `SetTopPosition` y `UpdateHeatMap`. En el cliente, `Update` corre cada frame con asignaciones y `SetText`, y puede dar NULL.

| Severidad | Nº |
|---|---|
| Bloqueante-1.30 | 0 |
| Alta | 6 (NET-01..NET-06) |
| Media | 7 |
| Baja | 8 |

---

## 1) Necesario para 1.30

No hay bloqueantes. Comprobé esto, con la firma de 1.30 abierta en cada caso:

| Override del mod | Mod | Vanilla 1.30 | ¿OK? |
|---|---|---|---|
| `DayZGame.OnEvent(EventType eventTypeId, Param params)` | `EXT\scripts_client\3_Game\DayZGame.c:20` | `V130\3_game\dayzgame.c:1479` (1.29: :1444) | sí |
| `DayZGame.ConnectFromCLI()` / `ConnectLaunch()` | `DayZGame.c:8,14` | `dayzgame.c:2827, 2281` | sí (ver NET-05) |
| `VONManagerImplementation.HandleInput(Input inp)` | `VONManager.c:11` | `V130\3_game\vonmanager.c:116`; `UpdateVoiceIcon` es private en :175 (es la misma clase moddeada, así que sirve) | sí, con una degradación (NET-17) |
| `LoadingScreen(DayZGame game)`, `Show()`, `OnUpdate(float timeslice)`, `m_ImageLogoMid/Corner`, `m_ModdedWarning`, `m_ImageBackground`, `m_DayZGame`, `IsLoading()` | `Loading\LoadingScreen.c:8-99` | `dayzgame.c:704-880`; el diff entre 1.29 y 1.30 de la clase solo toca `notification_root` (Xbox/MSStore) | sí |
| `ConnectErrorServerModule.FillErrorDataMap()` + `InsertDialogueErrorProperties` + `EConnectErrorServer.SERVER_LOCKED` | `ConnectErrorServerModule.c:3-5` | `connecterrorservermodule.c:22,48`; `errorhandlermodule.c:233` | sí |
| `DynamicMusicPlayerRegistry.RegisterTracksMenu()` / `RegisterTrackMenu(string,bool,...)` | `DynamicMusicPlayerRegistry.c:4-11` | `dynamicmusicplayerregistry.c:112,171` (idéntico a 1.29) | sí |
| `PPERequesterBank.RegisterRequester`, `PPERequester_GameplayBase.OnStart(Param par = null)`, `SetTargetValueColor`, `PPEGlow.L_23_GLASSES`, `PPOperators.SUBSTRACT` | `PPEManager\*.c` | `pperequesterbank.c:174`; `pperequestplatformsbase.c:197,354,434`; `ppeglow.c:55`; mismo patrón en `pperglasses.c:10` | sí |
| `DayZPlayerImplement.ShowDeadScreen(bool show, float duration)` | `4_World\Entities\DayZPlayerImplement.c:7` | `dayzplayerimplement.c:802` | sí |
| `PluginDiagMenu.RegisterModdedDiagsIDs/RegisterModdedDiags`, `GetModdedDiagID/GetModdedRootMenu`, `PluginDiagMenuClient.BindCallbacks` | `PluginDiagMenu*.c` | `plugindiagmenu.c:664-679`; `plugindiagmenuclient.c:6,36` | sí (solo con DIAG_DEVELOPER) |
| `MissionBase.CreateScriptedMenu(int id)`; id 75 | `MissionBase.c:4`; `Constants.c:1` | `missionbase.c:198`; MENU_* de vanilla llega a 48 (`constants.c:223`) y los de Expansion van de 1000 a 1004; no hay colisión | sí |
| `MissionGameplay.OnInit()` / `OnUpdate(float timeslice)` / `m_UIManager` | `MissionGameplay.c` | `missiongameplay.c:6,102,287` | sí |
| Server `PlayerBase.EEHitBy(...)`, `EEKilled(Object killer)`, `OnScheduledTick(float deltaTime)`, `OnUnconsciousStop(int pCurrentCommandID)`, `OnSyncJuncture(int pJunctureID, ParamsReadContext pCtx)`, `Save()`, `ResetPlayer(bool set_max)` | `EXT\scripts_server\4_World\Entities\ManBase\PlayerBase.c:36-169` | `V130\4_world\entities\manbase\playerbase.c:1255,1214,3007,3819,8478,8057,7936` | firmas OK (ver NET-19) |
| APIs del motor: `MuteAllPlayers`, `EnableVoN`, `HumanInputController.Override*`, `SetDisabled`, `SetMasterAttenuation`, `SetInventorySoftLock`, `QueueAdd/RemoveGlassesEffect`, `PPERequester_BurlapSackEffects`, `SEffectManager.PlaySound`, `InputUtils.GetButtonNameFromInput`, `Game.Chat` | varios | `game.c:1048,1055,1033`; `human.c:22,253-274`; `man.c:41,856`; `playerbase.c:1145,1150`; `pperburlapsack.c:1`; `effectmanager.c:178`; `inpututils.c:17` | sí |
| Expansion: `ExpansionClientSettings.OnRead/OnSave/Init/OnSettingsUpdated` y sus campos; `ExpansionNotification(string,string,...)`; `EXPANSION_MARKER_VIS_*`; `ExpansionArray.GetQuasiRandomElementAvoidRepetition` | `ExpansionClientSettings.c`, `BattleRoyaleRPC.c:292`, `BattleRoyaleClient.c:208-214` | `REF\exp_core_scripts\...\ExpansionClientSettings.c:191,892,1230,1433`; `ExpansionNotificationSystem.c:169`; `ExpansionMarkerData.c:15-16`; `ExpansionArray.c:70` | sí. **No verificado:** `GetExpansionSettings().GetMap().AddServerMarker` y el ctor de `ExpansionServerMarkerData(string)`, porque el módulo Map de Expansion no está en `REF` |

Colisiones de ids, comprobadas:
- Id de RPC de CF: `FRAMEWORK_RPC_ID = 10042` (`REF\cf_scripts\3_Game\CommunityFramework\RPC\RPCManager.c:47`).
- Enum COT del mod: `BattleRoyaleCOTStateMachineRPC` en 11000-11009 (`EXT\scripts_client\2_GameLib\BattleRoyaleEnums.c:2-14`). Los rangos de COT terminan en 10960+ (`REF\cot\scripts\3_Game\CommunityOnlineTools\RPC.c:581`).
- Expansion usa hashes de 1506850293 en adelante (`ExpansionScriptRPC.c:16-17`).
- `ERPCs` de vanilla es secuencial desde 0, con unas 220 entradas (`V130\3_game\enums\erpcs.c`).
- Sync juncture 88 frente a los `SJ_*` de vanilla, 0-26 y 200 (`V130\4_world\entities\dayzplayersyncjunctures.c:6-34`). En las dependencias no aparece ningún `SendSyncJuncture` con id propio.

Conclusión: no hay colisiones.

---

## 2) Optimización para 100-120 jugadores

### NET-07: `SetTopPosition` manda N unicast idénticos en cada cambio de jugadores (O(N²) por partida). Media
- **Evidencia:** `EXT\scripts_server\5_Mission\BattleRoyale\Server\States\0_BattleRoyaleState.c:164-171`. `UpdateTopPosition` recorre a todos y les manda `SetTopPosition` con **el mismo valor** a cada uno. Se llama desde `OnPlayerCountChanged` (:153/:156), que además ya hace un broadcast `SetPlayerCount` (:160). Con 120 jugadores salen unos 7.000 RPC fiables por partida, que no aportan nada.
- **Propuesta:** meter `position` como tercer campo de `SetPlayerCount` (`Param3<int,int,int>`) y quitar `SetTopPosition`. Se cambia el receptor a la vez (`BattleRoyaleRPC.c:63-77, 221-236`).
- **Esfuerzo:** S. **Confianza:** alta.

### NET-08: `Update()` del cliente asigna memoria y reescribe widgets en cada frame. Media
- **Evidencia:** `EXT\scripts_client\5_Mission\BattleRoyale\Client\BattleRoyaleClient.c`. Se llama desde `MissionGameplay.OnUpdate` (`MissionGameplay.c:276`).
  - :166-167: `new BattleRoyalePlayArea(...)` en cada frame en cuanto hay zona actual, sin comprobar si cambió.
  - :125 y :149: `PlayerCountChanged` y `UpdateKillCount` llaman a `SetText`/`Show` cada frame.
  - :76/:104: `GetZoneDistance` se calcula dos veces para la misma zona.
  - `UpdateZoneDistance` llega a `BattleRoyaleHud.SetDistance`, que hace 4 `SetColor`, un `SetText` y un `string.Format` evaluado antes del filtro de log (`EXT\scripts_client\5_Mission\GUI\HUD\BattleRoyaleHud.c:99-122`).
- **Propuesta:** comprobar si los valores cambiaron, como ya se hace con `br_previous_*`, también para recuento, kills y zona actual. Calcular la distancia una vez por frame. Refrescar la UI de distancia a unos 5-10 Hz.
- **Esfuerzo:** S. **Confianza:** alta en el patrón; el coste real en FPS no está medido.

### NET-09: los logs se construyen y filtran en caliente incluso desactivados. Baja
- **Evidencia:** `EXT\scripts_client\3_Game\BattleRoyaleUtils.c:15-18, 91-98`. Cada llamada a `Trace`/`Debug` llama a `CheckLogLevel`, que hace hasta 5 `IsCLIParam`. Los argumentos con `string.Format` se evalúan antes del filtro (`BattleRoyaleRPC.c:277`, `BattleRoyaleHud.c:122` cada frame fuera de zona).
- **Propuesta:** cachear el nivel en un `static int` al arrancar, y envolver los `Format` caros en `if (BattleRoyaleUtils.CheckLogLevel(...))`.
- **Esfuerzo:** S. **Confianza:** alta.

### NET-10: en builds DIAG del servidor, cada log se reenvía por broadcast a todos los clientes. Baja
- **Evidencia:** `BattleRoyaleUtils.c:45-48`, que hace `SendRPC(... "ChatLog" ..., true)` sin identidad. En una prueba DIAG con nivel trace y 120 clientes inunda el chat y la red.
- **Propuesta:** mandarlo solo a admins o detrás de un flag de CLI propio.
- **Esfuerzo:** S. **Confianza:** alta.

---

## 3) Bugs y seguridad

### NET-01: los handlers de servidor confían en el `target` del cliente (suplantación entre jugadores). Alta
- **Evidencia:**
  - `PlayerReadyUp`: `EXT\scripts_server\5_Mission\BattleRoyale\Server\BattleRoyaleServer.c:438-471`, usa `PlayerBase.Cast(target)` (:445) y nunca compara con `sender`.
  - `PlayerUnstuck`: el mismo archivo, :473-496.
  - `OnPlayerSpawnSelected`: `EXT\scripts_server\5_Mission\BattleRoyale\Server\States\3_BattleRoyaleSpawnSelection.c:162-230` (`pbTarget = PlayerBase.Cast(target)` en :173).
  - El cliente pasa su propio jugador como `target` (`BattleRoyaleClient.c:311,318`; `SpawnSelectionMenu.c:401`), pero ese campo lo controla quien envía. CF entrega `target` sin validar (`REF\cf_scripts\3_Game\CommunityFramework\RPC\RPCManager.c:86`).
- **Impacto:**
  - Un cliente modificado puede marcar como listos a todos los del lobby. El recuento de listos decide el arranque por votación (`1_BattleRoyaleDebug.c:78,98-147`), así que puede forzar el inicio.
  - Puede teletransportar a otro jugador con unstuck.
  - Puede fijar el punto de spawn de otro jugador, por ejemplo junto al suyo.
- **Propuesta:** fail-closed. En los tres handlers, obtener el jugador de `sender.GetPlayer()` (`V130\3_game\gameplay.c:375`, `PlayerIdentityBase`) e ignorar `target`, o rechazar si `!sender || pbTarget.GetIdentity() != sender`.
- **Esfuerzo:** S. **Confianza:** alta.

### NET-02: los RPC de admin no comprueban permisos en el servidor (COT y VPP). Alta
- **Evidencia:**
  - COT: `EXT\scripts_server\5_Mission\COT\GUI\BRMasterControlsModule.c:6-49`. `OnRPC` despacha `Next`, `Pause`, `Resume` y `SpawnAirdrop` sin ningún `HasPermission`. El grep de "Permission" en ese fichero da 0. El control positivo encuentra la línea 44 de `BattleRoyaleServer.c`, así que el grep funciona.
  - El dispatcher de CF solo filtra por rango de id (`REF\cf_scripts\3_Game\CommunityFramework\Module\CF_ModuleGameEvent.c:16`).
  - El cliente solo comprueba `HasAccess` en la UI (`EXT\scripts_client\5_Mission\COT\GUI\BRMasterControlsModule.c:15-18`).
  - VPP: `NextState`, en `BattleRoyaleServer.c:22` (registro) y :536-551 (handler), tampoco comprueba el permiso `MenuBattleRoyaleManager`, que solo se registra en :44.
- **Impacto:** cualquier cliente que mande `ScriptRPC` id 11001-11003 o el RPC de CF `NextState` puede pasar de estado, congelar la máquina de estados o llamar airdrops con `EXPANSIONMODMISSIONS`.
- **Propuesta:** primera línea de cada handler:
  - COT: `if (!GetPermissionsManager().HasPermission("BattleRoyale.StateMachine.Update", sender)) return;` (API en `REF\cot\scripts\4_World\CommunityOnlineTools\Classes\Permissions\JMPermissionManager.c:238`).
  - VPP: `if (!sender || !GetPermissionManager().VerifyPermission(sender.GetPlainId(), "MenuBattleRoyaleManager")) return;` (`REF\vpp\...\PermissionManager.c:1008`).
- **Esfuerzo:** S. **Confianza:** alta.

### NET-03: la posición de spawn solo se valida en el cliente. Alta
- **Evidencia:** el cliente comprueba tiempo, límites del mundo, mar y primera zona antes de enviar (`EXT\scripts_client\5_Mission\GUI\SpawnSelectionMenu.c:361-401`). El servidor guarda `data.param1` tal cual (`3_BattleRoyaleSpawnSelection.c:181,192`). Prepare solo busca una posición segura alrededor de ese punto (`4_BattleRoyalePrepare.c:542-562`), sin mirar la primera zona ni los límites.
- **Impacto:** con un cliente modificado se puede aparecer fuera de la primera zona, en cualquier punto del mapa o en el centro probable de las rondas.
- **Propuesta:** repetir en el servidor las mismas comprobaciones: distancia a `v_FirstZoneCenter` ≤ radio + 25, `0 < x,z < GetWorldSize()` y `!SurfaceIsSea`. Si falla, rechazar y no tocar `spawn_pos`.
- **Esfuerzo:** S. **Confianza:** alta.

### NET-04: `OnPlayerSpawnSelected` amplifica cada clic en un broadcast completo y no tiene límite de frecuencia. Alta para 120
- **Evidencia:** `3_BattleRoyaleSpawnSelection.c:178-190`. Cada RPC de un cliente reconstruye el array con todos los spawns y lo manda a **todos** (`UpdateHeatMap`, fiable).
- **Coste:** con 120 jugadores, un clic cuesta 120 envíos × hasta 120 vectores. Un cliente que repita el RPC sin parar mantiene un broadcast O(N²) por mensaje, porque no hay límite por emisor.
- **Propuesta:** limitar por `sender` (por ejemplo 1 cada 500 ms con `map<string,int>`). Marcar el heatmap como sucio y emitirlo con un `Timer` de 1 s solo si cambió.
- **Esfuerzo:** S-M. **Confianza:** alta en el patrón; el ancho de banda real no está medido.

### NET-05: el cliente bloquea `-connect` y el matchmaking depende de forma síncrona de `api.vigrid.ovh`. Alta, operativa
- **Evidencia:**
  - `ConnectFromCLI()` está vacío (`EXT\scripts_client\3_Game\DayZGame.c:8-12`) y `ConnectLaunch()` lleva al menú (:14-18). En vanilla, `ConnectFromCLI` es el que conecta con `-connect` (`V130\3_game\dayzgame.c:2827-2840`; se llama desde :2298 y :2610).
  - El menú principal llama al endpoint de forma bloqueante: `MainMenu.c:155` → `IsMatchMakingAvailable` → `MatchMakingWebhook.isMatchMakingAvailable` → `ctx.GET_now` (`EXT\scripts_client\3_Game\BattleRoyale\Webhook\MatchMakingWebhook.c:66`), con `POST_now` en :28. `GET_now` es "thread blocking" según `V130\3_game\http\restapi.c:106-108`.
  - El endpoint está fijado en `BattleRoyaleConstants.c:30` (`https://api.vigrid.ovh/`).
- **Impacto:**
  - Con DZSA launcher o Steam (`-connect`) no se entra; el jugador acaba en el menú.
  - Si la API de Vigrid no responde, el menú se congela hasta el timeout. El dueño no controla ese servicio.
  - El bloqueo es incoherente: `-join` sí entra (`dayzgame.c:2124`).
  - También impide el arranque automatizado por `-connect` en pruebas (dayz-mcp); no lo comprobé ejecutando.
- **Propuesta:**
  - Restaurar `super.ConnectFromCLI()` y `super.ConnectLaunch()` detrás de un flag (por ejemplo, solo si `BATTLEROYALE_API_ENDPOINT` está configurado).
  - Pasar el matchmaking a `GET`/`POST` asíncronos con `RestCallback` y timeout corto, o desactivarlo por defecto.
  - Hacer configurable el endpoint.
- **Esfuerzo:** M. **Confianza:** alta en el código; la congelación no la he observado, sale de la doc de `GET_now`.

### NET-06: NULL en `Update()` del cliente cuando no hay jugador local. Alta (excepción-NULL por frame)
- **Evidencia:** `BattleRoyaleClient.c:66` obtiene `player` sin comprobarlo. Luego lo usa en :108/:112 (`QueueAdd/RemoveGlassesEffect`), :144 (`DisableInput`) y :224 (`GetZoneDistance` → `player.GetPosition()`). Solo :189 lo comprueba.
- **Cuándo pasa:** cuando hay zona activa o llega `SetInput` mientras `GetGame().GetPlayer()` es NULL: carga inicial, entre muerte y desconexión, o espectador de COT (estos dos últimos no los verifiqué).
- **Impacto:** excepción de script en cada frame. No es un crash del proceso. Además `br_previous_input_state` no se actualiza mientras dure, así que reintenta cada frame.
- **Propuesta:** `if (!player || !gameplay) return;` al principio de `Update`, y comprobar también en `GetZoneDistance`, `FadeIn` y `FadeOut` (:250-267).
- **Esfuerzo:** S. **Confianza:** media.

### NET-11: la llamada repetitiva `OnSecond` nunca se retira y `BattleRoyaleBase` no es `Managed`. Media
- **Evidencia:**
  - `BattleRoyaleClient.c:36` registra `CallLaterByName(this, "OnSecond", 1000, true)` en `CALL_CATEGORY_GUI`, una cola global que sobrevive a la misión.
  - El destructor (:27-30) no llama a `RemoveByName` (API en `V130\2_gamelib\tools.c:69`).
  - `class BattleRoyaleBase` no hereda de `Managed` (`EXT\scripts_client\4_World\BattleRoyale\BattleRoyaleBase.c:1`).
  - `OnSecond` usa `gameplay` sin comprobarlo (:272-281).
- **Cuándo pasa:** tras `LeaveServer` (`DayZPlayerImplement.c:44-50`, que muerto todo jugador hace a los 15 s) la cola puede quedarse con un objeto borrado.
- **Impacto:** puntero colgante o NULL cada segundo en el menú. No he comprobado si el motor descarta la llamada.
- **Propuesta:** `RemoveByName(this, "OnSecond")` en el destructor, `class BattleRoyaleBase : Managed` y una comprobación de `gameplay`.
- **Esfuerzo:** S. **Confianza:** media.

### NET-12: el `Timer` de expulsión de quienes entran a mitad de partida se sobrescribe. Media
- **Evidencia:** `BattleRoyaleServer.c:276` hace `m_Timer.Run(30.0, this, "Disconnect", new Param1<PlayerIdentity>(...))` con un único `m_Timer` (:80). `Timer.Run` reemplaza destino y parámetros (`V130\3_game\tools\tools.c:595-602`, `OnStart` en :340-347).
- **Impacto:** si dos jugadores entran a mitad de partida con menos de 30 s de diferencia, solo se expulsa al segundo. El primero se queda en el mapa sin estar en ningún estado. Es explotable con dos cuentas.
- **Propuesta:** `GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(Disconnect, 30000, false, identity)` por jugador, o un `array<ref Timer>`.
- **Esfuerzo:** S. **Confianza:** alta. Solapa con el auditor de servidor.

### NET-13: un `SendRPC` con identidad NULL se convierte en broadcast (fail-open). Media
- **Evidencia:** CF pasa `sendToIdentity` directo a `g_Game.RPC` (`RPCManager.c:138`), y con NULL se envía a todos. Casos donde ocurre:
  - `ShowWinScreen` a `player_winner.GetIdentity()` (`8_BattleRoyaleWin.c:74`): si el ganador se desconecta en ese instante, **todos** ven la pantalla de victoria.
  - `SetTopPosition` a `player.GetIdentity()` (`0_BattleRoyaleState.c:170`).
- **Propuesta:** un helper `SendTo(identity, ...)` que no envíe si `!identity`.
- **Esfuerzo:** S. **Confianza:** alta.

### NET-14: la sync juncture 88 (teleport) solo se maneja en el servidor. Media
- **Evidencia:** el handler está dentro de `#ifdef SERVER` (`EXT\scripts_server\4_World\Entities\ManBase\PlayerBase.c:1, 85-108`). Se envía desde `4_BattleRoyalePrepare.c:502` y `5_BattleRoyaleStartMatch.c:196`. El cliente dueño recibe la juncture pero vanilla la ignora (`playerbase.c:8478`).
- **Impacto:** la posición del cliente se corrige por sincronización de red, no en el mismo paso de simulación, así que puede haber rubber-band. No lo he comprobado in-game.
- **Propuesta:** mover el `case 88` a la parte común de `PlayerBase`, sin `#ifdef`, para que se aplique en ambos lados. Comprobar el `ctx.Read` (el linter avisa en :93-94) y dar nombre a la constante (`BR_SJ_TELEPORT = 88`).
- **Esfuerzo:** S. **Confianza:** media.

### NET-15: congelar al jugador y fijar el límite de voz son decisiones del cliente. Media
- **Evidencia:**
  - En SpawnSelection el bloqueo de input es solo un RPC `SetInput` (`3_BattleRoyaleSpawnSelection.c:143-154`), aplicado en el cliente (`BattleRoyaleClient.c:142-146` → `PlayerBase.DisableInput`, en `EXT\scripts_client\4_World\Entities\ManBase\PlayerBase.c:34-72`).
  - En Prepare/StartMatch el servidor sí llama a `DisableInput` (`4_BattleRoyalePrepare.c:161`, `5_BattleRoyaleStartMatch.c:143`).
  - El tope de voz (susurro antes del inicio) solo existe en `VONManager.c:11-44`.
- **Impacto:** un cliente modificado puede moverse en la fase de spawn y gritar en el lobby. No verifiqué si el override del `HumanInputController` en el servidor es autoritativo.
- **Propuesta:** llamar a `DisableInput` también en el servidor durante SpawnSelection, y usar `GetGame().EnableVoN`/`SetVoNVolume` en el servidor si existe una API equivalente (no verificado).
- **Esfuerzo:** M. **Confianza:** media.

### NET-16: `PlayerReadyUp` se envía no fiable y unstuck no tiene enfriamiento. Baja
- **Evidencia:** `BattleRoyaleClient.c:311` envía con `guaranteed=false`. Cada pulsación de F2 genera un mensaje al jugador (`5_BattleRoyaleStartMatch.c:158-171`).
- **Propuesta:** `true`, y un enfriamiento de 1-2 s en el cliente.
- **Esfuerzo:** S. **Confianza:** alta.

### NET-18: `RequestEntityHealthUpdate` está registrado pero el método no existe. Baja
- **Evidencia:** se registra en `BattleRoyaleServer.c:20`. El grep de `void RequestEntityHealthUpdate` en todo `EXT` da 0; el mismo grep encuentra los otros tres handlers, así que funciona.
- **Impacto:** si un cliente lo envía, `CallFunctionParams` falla y deja un error de log.
- **Propuesta:** quitar el `AddRPC`.
- **Esfuerzo:** S. **Confianza:** alta.

### NET-19: `ResetPlayer` del servidor no llama a `super` y se pierde la limpieza nueva. Baja
- **Evidencia:** el mod (`scripts_server\...\PlayerBase.c:169-240`) no llama a `super`. La versión de 1.30 limpia además flechas, `ThermalBiasHandler`, `m_DiseaseCount` y la penalización de manos ensangrentadas (`playerbase.c:7936-8000`, aunque dentro de `DIAG_DEVELOPER`).
- **Propuesta:** añadir `ClearArrows` y `SetTemporaryResistance(0)` si se quiere el mismo comportamiento.
- **Esfuerzo:** S. **Confianza:** media.

---

## 4) Otras mejoras

- **NET-17. Baja.** El override de `HandleInput` (`VONManager.c:11-44`) no incluye la rama nueva de 1.30 que muestra el icono de rango cuando no se está hablando (`ShowVoiceNotification`, `vonmanager.c:157-170`). También pierde los guards de party de Xbox y MSStore. Es una degradación cosmética. Propuesta: replicar esa rama.
- **NET-20. Baja.**
  - `#ifndef SERVER` / `#ifdef DIAG_DEVELOPER` sin `#endif` en 14 ficheros del cliente (lint WARN; recuento: `RPC\BattleRoyaleRPC.c`, `VONManager.c`, `DayZPlayerImplement.c` ×2, `PluginDiagMenu*.c` ×2…). El mod ya funcionaba así, por lo que el compilador parece tolerarlo, pero no lo he verificado en 1.30. Conviene cerrarlos por higiene.
  - Los botones COT `AddFakePlayer`, `AddFakeGroup`, `SpawnHorde` y `SpawnChemicals` envían RPC (`EXT\scripts_client\5_Mission\COT\GUI\BRMasterControlsModule.c:77-102`) que el servidor no maneja (`EXT\scripts_server\...\BRMasterControlsModule.c:11-25`). Son botones muertos.
- **NET-21. Baja.** No hay foto del estado al conectar: el cliente depende de recibir cada RPC en su momento (`BattleRoyaleRPC.c:41-56`). Un admin que entra a mitad (`BattleRoyaleServer.c:269-273`) no ve la zona hasta la siguiente ronda. Propuesta: un RPC `SyncState` en `OnPlayerConnected`.
- **NET-22. Baja.** `s_LocalPlayers` y `GetLocalPlayers` del `PlayerBase` del cliente (`EXT\scripts_client\4_World\Entities\ManBase\PlayerBase.c:3-31`) no tienen consumidores; el grep da solo la definición. Es código muerto que hace un `Find` O(n) en cada destrucción de jugador. Se puede borrar.
- **Diseño, solo como nota.** Al morir, el cliente se va del servidor a los 15 s (`DayZPlayerImplement.c:41-50`), así que no hay espectador. Si se quiere espectar, hay que sustituir `LeaveServer` por una cámara de seguimiento con un RPC de lista de vivos.

## Qué NO verifiqué
- No ejecuté nada in-game: rubber-band (NET-14), autoridad del input en el servidor (NET-15) y congelación del menú (NET-05) se deducen de la lectura del código.
- La API de Map de Expansion (`GetMap().AddServerMarker`, ctor de `ExpansionServerMarkerData`): ese módulo no está en `REF`.
- `HttpArguments`/`HttpArgument`: no están en vanilla 1.30; el grep en `V130` da 0. Vienen de una dependencia que no está en `REF`.
- Si los botones por defecto F1/F2/F3 (`EXT\data\Inputs.xml`) chocan con los binds de vanilla 1.30; esos binds están en datos del motor, no en scripts.
- No medí ancho de banda ni FPS (NET-04, NET-07, NET-08).

# server-security

## NET-01 / SRV-21
`BattleRoyaleServer.c:485-508` resuelve el jugador con `sender.GetPlayer()` (`gameplay.c:375`) y rechaza con `Warn` y steamid si `sender` es null, si no hay jugador o si `target` no es ese jugador. `PlayerReadyUp` (`:517`) y `PlayerUnstuck` (`:547`) usan ese resultado. `3_BattleRoyaleSpawnSelection.c:174-198` hace la misma comprobación en `OnPlayerSpawnSelected` (`:243`).
Verificado por lectura del diff. No se ejecutó el RPC en el juego.

## NET-03
`3_BattleRoyaleSpawnSelection.c:71-75` guarda centro, radio del primer círculo y `extra_spawn_radius` (`BattleRoyaleSpawnsData.c:11`, solo lectura). `IsSpawnInsideZone` (`:201-211`) exige XZ dentro de `GetWorld().GetWorldSize()` (`world.c:82`, mismo criterio que `mapnavigationbehaviour.c:190`) y distancia horizontal <= radio + extra. Si no vale, se ignora (`:255`). No había string de aviso al jugador en este handler; el cliente solo hace `Trace` (`SpawnSelectionMenu.c:374-394`).
Verificado por lectura. No se probó una coordenada en el servidor.

## NET-04 / SRV-32
Entre selecciones aceptadas del mismo steamid hay al menos 500 ms (`GetGame().GetTime()`, `game.c:1535`; `3_BattleRoyaleSpawnSelection.c:249`). Aceptar marca `m_HeatMapDirty`. `BroadcastHeatMap` (`:213-225`) manda el mismo `Param1<array<vector>>` a `UpdateHeatMap`, como máximo cada 1000 ms (`CallLater` `tools.c:61`, arranque `:91`, parada `:106`).
Verificado por lectura. No se midió el ancho de banda.

## NET-02 / SRV-20
`BRAdminAuthorizer.IsAllowed` (`BRAdminAuthorizer.c:7`) niega si `sender` es null. Con `VPPADMINTOOLS`, `VerifyPermission(plainId, "BattleRoyale:" + action, "", false)` (`PermissionManager.c:1008`, getter `:1245`). Con `JM_COT` (`cot/scripts/config.cpp:41`), `GetPermissionsManager().HasPermission("BattleRoyale.StateMachine.Update", sender)` (`JMPermissionManager.c:238` y `:940`). Si los dos defines están, basta uno. La lista `admins_steamid64` solo se usa si no hay ninguno de los dos. Cada denegación hace `Warn` con steamid y acción (`:11`, `:40`).
`BRAdminController` (`BRAdminController.c`) hace Next, Pause, Resume y SpawnAirdrop. COT (`BRMasterControlsModule.c:28-57`) y `NextState` (`BattleRoyaleServer.c:614-617`) pasan por el authorizer y luego el controller. Los cuatro permisos VPP se registran en el `Init` existente; registrarlos no los concede.
Verificado por lectura de firmas y del diff. No se comprobó un admin COT ni VPP en ejecución.

## ZON-02
Se quitó el `Trace` de la contraseña. Si `enable_vigrid_api` sigue activo (el default sigue en false, `BattleRoyaleServerData.c:7`) y el UUID no tiene 36 caracteres, se deja `match_uuid` vacío y se sigue (`BattleRoyaleServer.c:60-68`). No hay `RequestExit`. Decisión: `force_match_uuid` ya no cierra el proceso, para que un supervisor externo no entre en bucle. `CreateMatchWebhook.getMatchUUID` sigue siendo `POST_now` si alguien enciende la API.
Verificado: grep de `RequestExit` y de la contraseña en el log de `BattleRoyaleServer.c` (0 coincidencias de `RequestExit`). No se llamó a la API.

## ZON-03
`autolock_url` por defecto es `""` (`BattleRoyaleServerData.c:29`). `AutoLockWebhook.LockServer` (`AutoLockWebhook.c:24-28`) no envía si la URL está vacía. El cuerpo sale de `JsonFileLoader<BRAutoLockBody>.MakeData` (`jsonfileloader.c:85`, `:46-56`). La contraseña RCON por defecto sigue vacía (`BattleRoyaleServerData.c` campo existente). Un `server_settings.json` ya guardado conserva la URL vieja; este cambio no reescribe perfiles.
Verificado por lectura. No se hizo un POST.

## SRV-13 / NET-12
El `m_Timer` único se sustituyó por `m_LateJoinTimers` (`BattleRoyaleServer.c:14`). `ScheduleLateJoinKick` (`:291`) crea un `Timer` por steamid y, si ya había uno, lo para. Los admins de `admins_steamid64` siguen saliendo antes (`:271`). El destructor para los timers vivos.
Verificado por lectura. No se simularon dos conexiones tardías.

## Contrato SetMatchPhase
Tras `AddPlayer` en el lobby, unicast `SetMatchPhase` `Param2(EBRMatchPhase.LOBBY, -1)` (`BattleRoyaleServer.c:285`, `SendRPC` `RPCManager.c:111`). Al final de `Activate` de la selección, broadcast `(EBRMatchPhase.SPAWN_SELECTION, i_SpawnSelectionDuration)` (`3_BattleRoyaleSpawnSelection.c:93`). Enum en `scripts_client/2_GameLib/BattleRoyaleEnums.c:31`.
Verificado por lectura. El cliente de esta lane no registra el RPC; no se comprobó que lo pinte.

## Stats webhook (opcional)
`stats_webhook_url` vacío por defecto (`BattleRoyaleServerData.c:32`). `BRStatsWebhook.Send` usa `RestContext.POST` (`restapi.c:123`) y no `POST_now`. `NotifyStatsWebhook` (`BattleRoyaleServer.c:314`) dispara `match_start` (número de jugadores) al entrar en "Start Match State" y `winner` (último nombre con identidad) al entrar en "Win State". Esos ficheros de estado no son de esta lane. Payload sin contraseñas. Apagado si la URL está vacía.
Verificado por lectura. No se recibió un HTTP.

## Lint
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
Los 2 errores de `scripts_server` siguen siendo `ES-OVERRIDE-PARAM-NAME-MISMATCH` en `RemovePlayer` (`1_BattleRoyaleDebug.c:168`, `4_BattleRoyalePrepare.c:715`). No se añadieron errores. Comando: `bash tools/lint.sh . build/lint` (exit 0).

## CROSS-LANE REQUESTS
Ninguna.

## LO QUE NO PUDE VERIFICAR
- Ningún RPC, permiso COT/VPP, kick ni webhook se ejecutó en DayZ.
- Un perfil ya escrito con `autolock_url` apuntando a api.vigrid.ovh sigue enviando si `use_autolock` está activo; solo cambia el default de la clase.
- `CreateMatchWebhook` sigue bloqueando el hilo con `POST_now` si `enable_vigrid_api` se enciende.
- El nombre del ganador es el último jugador con identidad en el estado Win, leído después de `Activate`, no el string interno de `8_BattleRoyaleWin.c`.

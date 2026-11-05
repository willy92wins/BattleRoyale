# Auditoría 02 — Zona, efectos de área, loot/spawns/POIs/airdrops y webhooks HTTP

Mod: DayZ-BattleRoyale v0.1.0-Vigrid. Auditoría de solo lectura, 2026-11-04.
Abreviaturas de rutas:
- `EXT` = `C:\Users\guill\OneDrive\Documentos\DayZ Projects\BattleRoyale_dev\extracted_v0.1.0-vigrid\`
- `V130` = `C:\Users\guill\OneDrive\Documentos\DayZ Projects\scripts\` (vanilla 1.30)
- `V129` = `C:\Users\guill\OneDrive\Documentos\DayZ Projects\scripts_1.29_backup_20261016\`
- `DF` = `C:\Program Files (x86)\Steam\steamapps\common\DayZ\!Workshop\@Dabs Framework\addons\Scripts.pbo` (leído en binario, sin extraer)

## Resumen ejecutivo

1. **Ninguna API vanilla que use esta área se ha roto en 1.30.** RestApi/RestContext/RestCallback son idénticos en 1.29 y 1.30 (`diff` sin salida). `HttpArgument`/`HttpArguments` no son vanilla: vienen de Dabs Framework, que llega por la cadena Expansion Core → `DF_Scripts`. Las firmas de EffectArea que sobrescribe el mod (`PlaceParticles`, `CreateTrigger`) siguen igual en 1.30, y además ese código está muerto: `BLUE_ZONE` no se define en ningún sitio. No encuentro ningún bloqueante de compilación en esta área.
2. **El backend `api.vigrid.ovh` va cableado en el CLIENTE** (constante de 2_GameLib) y el menú principal lo consulta de forma **síncrona** (`GET_now`/`POST_now`) cada vez que se abre. Además, `ConnectFromCLI` y `ConnectLaunch` están anulados. En el servidor todo el backend queda apagado por defecto (`enable_vigrid_api = false`). La partida, el lock y el matchmaking del servidor **no dependen** del backend salvo que se active. Recomendación: neutralizar el matchmaking del cliente (ZON-01) y dejar desactivados `enable_vigrid_api` y `use_autolock` (ZON-02, ZON-03).
3. **La zona es barata con 120 jugadores.** Se sincroniza por RPC de CF solo en los cambios de estado (2-3 RPC por ronda) y el daño es una distancia 2D por jugador en `OnScheduledTick`. El riesgo real está en la generación de círculos: bucles `while(true)` sin tope y un `RequestExit(0)` que apaga el servidor si no encuentra centro válido (ZON-04, ZON-05).
4. **El loot no se genera por script.** Sale de la CE de la misión y se limpia reiniciando el servidor tras cada partida (`9_BattleRoyaleRestart.c:38`). Los objetos de inicio se reparten en un hilo con `Sleep(100)` por jugador, así que no hay spawn masivo en un frame.
5. **«Vigrid» es en el código el nombre del proyecto/hive (`api.vigrid.ovh`, repo `LeMyst/Vigrid-BattleRoyale`), no un terreno.** El mapa se lee dinámicamente (`GetWorldName()`/`GetWorldSize()`). Hay que confirmar con el dueño qué terreno se usará. Los valores por defecto (spawn del lobby, listas de ciudades, overrides de POI) son de Chernarus (ZON-07).

---

## 1) Necesario para 1.30 / para poder operar

### ZON-01 — Matchmaking del cliente cableado a un backend probablemente muerto, con llamadas HTTP bloqueantes en el menú principal
- **Severidad:** Alta (para operar; no rompe la compilación). **Tipo:** degradación (el cliente se congela) y riesgo de seguridad.
- **Evidencia:**
  - Endpoint fijo en el cliente: `EXT\scripts_client\2_GameLib\BattleRoyaleConstants.c:28` (`BATTLEROYALE_API_ENDPOINT = "https://api.vigrid.ovh/"`). Al estar en el PBO del cliente, el servidor no puede cambiarlo sin reempaquetar.
  - Llamadas síncronas: `EXT\scripts_client\3_Game\BattleRoyale\Webhook\MatchMakingWebhook.c:28` (`POST_now`) y `:66` (`GET_now`). La propia vanilla documenta `GET_now`/`POST_now` como «thread blocking operation!» en `V130\3_game\http\restapi.c:105-108` y `:125-128`. El timeout por defecto es de 10 s de conexión y 10 s de lectura (`V130\3_game\http\restapi.c:30-31`).
  - Se llama en cada `Init` del menú principal: `EXT\scripts_client\5_Mission\Mission\MainMenu.c:155` → `IsMatchMakingAvailable()` (`:180-208`), y otra vez tras cada intento de conexión (`:429`).
  - Conexión directa anulada: `EXT\scripts_client\3_Game\DayZGame.c:8-12` (`ConnectFromCLI` vacío) y `:14-18` (`ConnectLaunch` → `MainMenuLaunch`). Las dos existen en 1.30 (`V130\3_game\dayzgame.c:2827` y `:2281`), así que el override compila.
  - Si la respuesta no es `"true"`, el botón Play se conecta con los parámetros de CLI `-connect/-port/-password` (`MainMenu.c:340-348`), es decir, cae en modo abierto (fail-open) hacia la conexión normal. El selector de servidores está oculto (`MainMenu.c:172`).
  - Riesgo de toma del dominio: si `api.vigrid.ovh` caduca y otro lo registra, controla las respuestas `"true"` y `"ip;port;password"`, y el cliente se conecta solo a donde diga esa respuesta (`MainMenu.c:404-422`). Además, cada cliente envía su lista de mods a ese dominio (`MatchMakingWebhook.c:20-28`).
  - `GetRestApi()` se usa sin comprobar si es NULL (`MatchMakingWebhook.c:26,64`). Ningún script vanilla llama a `CreateRestApi()`; COT sí lo hace (`ref\cot\scripts\4_World\CommunityOnlineTools\Classes\Webhook\JMWebhookModule.c:203`) y VPP comprueba `!GetRestApi()` (`ref\vpp\...\VPPWebhookSender.c:335`).
- **Propuesta:** quitar del cliente el matchmaking Vigrid: `IsMatchMakingAvailable()` devuelve `false` sin hacer HTTP, o se compila bajo un `#ifdef BR_VIGRID_MATCHMAKING` que no se define. Restaurar `ConnectFromCLI`/`ConnectLaunch` a `super` (o dejarlos así si el dueño quiere forzar el menú, pero sin red). Si algún día se quiere matchmaking propio, usar las variantes asíncronas `GET`/`POST` con callback y comprobar `GetRestApi()`.
- **Esfuerzo:** S. **Confianza:** alta en el código. No he comprobado si `api.vigrid.ovh` responde hoy: no he hecho llamadas de red.

### ZON-02 — Backend del servidor (`enable_vigrid_api`): apagado por defecto; si se enciende con el backend muerto, bloquea el arranque o reinicia en bucle
- **Severidad:** Alta si se activa; con la configuración por defecto, Baja. **Tipo:** degradación (hilo principal bloqueado al iniciar) o servidor en bucle de reinicio.
- **Evidencia:**
  - Por defecto está apagado: `EXT\scripts_server\3_Game\Config\BattleRoyaleServerData.c:7`.
  - Con `true`: `BattleRoyaleServer.Init` hace `CreateMatchWebhook.getMatchUUID()` con `POST_now` síncrono (`EXT\scripts_server\3_Game\BattleRoyale\Webhook\Matches\CreateMatchWebhook.c:42`, llamado en `EXT\scripts_server\5_Mission\BattleRoyale\Server\BattleRoyaleServer.c:59-60`). Si el UUID no mide 36 caracteres y `force_match_uuid` está a true, ejecuta `GetGame().RequestExit(0)` (`BattleRoyaleServer.c:64-67`), lo que provoca un reinicio en bucle mientras el backend no exista.
  - Lock, Start, Parties, Score, Win y Event son asíncronos (`ctx.POST(callback, ...)`, p. ej. `...\Servers\LockServerWebhook.c:162`), con 5 reintentos inmediatos sin espera entre ellos (`LockServerCallback.OnError` → `Send` en `:196-203`). Si fallan, el servidor no se bloquea y la partida sigue (fail-open). Ningún estado espera la respuesta.
  - El texto de error al jugador solo sale si `match_uuid == ""` (`BattleRoyaleServer.c:289-290`); con la API apagada se pone `"disabled"` (`:77`).
  - `BattleRoyaleUtils.Trace("Server password: " + ...)` escribe la contraseña del servidor en el log (`BattleRoyaleServer.c:58`), solo con nivel trace (`EXT\scripts_client\3_Game\BattleRoyaleUtils.c:76-98`, `-br-trace`).
- **Propuesta:** dejar `enable_vigrid_api=false` en `server_settings.json` y documentarlo. Si se mantiene la función: pasar `getMatchUUID` a asíncrono, quitar el `RequestExit` (o limitarlo a N intentos con backoff) y borrar la traza con la contraseña. A medio plazo, reemplazar todo por un webhook de Discord o de estadísticas propio, opcional y asíncrono.
- **Esfuerzo:** S (desactivarlo) / M (reescribirlo asíncrono). **Confianza:** alta.

### ZON-03 — Autolock envía la contraseña RCON a una URL que por defecto apunta al dominio del autor
- **Severidad:** Alta (secreto enviado a un tercero) si `use_autolock=true`; desactivado por defecto. **Tipo:** seguridad.
- **Evidencia:** `autolock_url` vale por defecto el dominio del autor (`BattleRoyaleServerData.c:29`). El cuerpo se arma concatenando la contraseña sin escapar (`EXT\scripts_server\3_Game\BattleRoyale\Webhook\Servers\AutoLockWebhook.c:260`), así que una contraseña con `"` rompe el JSON. Se dispara en `EXT\scripts_server\5_Mission\BattleRoyale\Server\States\2_BattleRoyaleCountReached.c:32-35`. Hay 3 reintentos inmediatos (`AutoLockWebhook.c:288-304`).
- **Propuesta:** cambiar el valor por defecto de `autolock_url` a `""`, de modo que la condición en `2_BattleRoyaleCountReached.c:32` ya lo bloquee (fail-closed). Serializar el cuerpo con `JsonFileLoader.MakeData` (como hacen los demás webhooks). Si el dueño quiere bloquear el servidor al empezar la partida, hacerlo en local (`GetGame().ServerConfigGetInt`/BattlEye RCON desde un proceso externo) en lugar de un POST a un tercero.
- **Esfuerzo:** S. **Confianza:** alta.

### Comprobaciones 1.30 sin hallazgo (para que no se repitan)
- RestApi: `V130\3_game\http\restapi.c` es idéntico a `V129\...\restapi.c` (`diff` sin salida). `POST(RestCallback, string, string)` en `:123`, `SetHeader` en `:141`, `GetRestApi` en `:183`, `RestCallback.OnError/OnTimeout/OnSuccess(string,int)` en `:55/:64/:73`.
- `HttpArgument`/`HttpArguments`: no existen en V130 ni en V129 (grep sin resultados). Están definidos en DF (leído en binario) como `typedef Param2<string,string> HttpArgument` y `class HttpArguments: array<ref HttpArgument>` con `ToQuery`. DF llega por `exp_core_scripts\config.cpp:8-11` (`DF_Scripts`). No he podido comprobar que la versión de DF del servidor de destino sea la misma que la del workshop local.
- `JsonFileLoader.MakeData` existe en `V130\3_game\tools\jsonfileloader.c:85`, y `GetModInfos` en `V130\3_game\global\game.c:617`.
- EffectArea: `PlaceParticles` (`V130\4_world\classes\contaminatedarea\effectarea.c:247`) y `CreateTrigger(vector,int)` (`:486`) tienen la misma firma que el override (`EXT\scripts_client\4_World\Classes\ContaminatedArea\BREffectArea.c:59,212`). Entre 1.29 y 1.30 cambió `OnCEUpdate`→`OnCEIterate` y se añadió `OIL_PIT` (diff de effectarea.c), pero el mod no sobrescribe ninguno de los dos. Las APIs de partículas siguen presentes (`V130\3_game\particles\particlemanager\particlemanager.c:52,315,500`).
- PPE: `RegisterRequester`, `PPERequester_GameplayBase` (`V130\3_game\ppemanager\requesters\pperequestplatformsbase.c:434`), `SetTargetValueColor` (`:197`), `PPEGlow.PARAM_COLORIZATIONCOLOR`/`L_23_GLASSES` (`V130\...\ppeglow.c:32,55`) y `PPOperators.SUBSTRACT` (`V130\3_game\ppemanager\ppeconstants.c:58`) existen. También `QueueAddGlassesEffect`/`QueueRemoveGlassesEffect` (`V130\4_world\entities\manbase\playerbase.c:1145,1150`).
- Daño de zona: `DecreaseHealthCoef` (`V130\3_game\entities\object.c:1120`), `QueueUpPrimarySymptom(SymptomIDs.SYMPTOM_PAIN_LIGHT)` (usado igual por vanilla en `V130\4_world\classes\playermodifiers\modifiers\diseases\salmonella.c:105`).
- Mundo: `SurfaceIsSea/Pond` (`V130\3_game\global\game.c:1196-1197`), `SurfaceY` (`:1176`), `SurfaceGetType` (`:1180`), `GetWorldName()` (`:934`), `ConfigGetFloatArray` (`:578`), `GetWorldSize` (`V130\3_game\global\world.c:82`), `float.MAX/LOWEST` (`V130\1_core\proto\enconvert.c:112-113`).

---

## 2) Optimización para 100-120 jugadores

### ZON-06 — Tablas de zona calibradas para unos 50 jugadores y un mapa de 15 km
- **Severidad:** Media (jugabilidad). **Tipo:** degradación de diseño.
- **Evidencia:** `EXT\scripts_server\3_Game\Config\BattleRoyaleZoneData.c:27-29`: `static_sizes = {35,140,562,1125,2250,3375,4500}`, `min_players` con máximo 44. `num_zones = 6` (`EXT\scripts_server\3_Game\Config\BattleRoyaleGameData.c:11`), así que la zona 1 usa el índice `num_zones-1 = 5` (`BattleRoyaleZone.c:71` → `GetBattleRoyalePlayAreas(i_NumRounds - GetZoneNumber())`). Resultado: primer círculo de 3375 m de radio, y el tamaño 4500 no se usa nunca. `GetDynamicStartingZone` elige la zona inicial según `min_players` (`EXT\scripts_server\5_Mission\BattleRoyale\Server\States\0_BattleRoyaleState.c:520-547`). Con 120 jugadores siempre empieza en la zona 1. Los radios son metros absolutos y dependen del tamaño del terreno.
- **Propuesta:** fijar `zone_settings.json` por misión (la carpeta `$mission:` ya se soporta, `BattleRoyaleZoneData.c:39-42`) con `num_zones = 7` y radios escalados a `GetWorldSize()` del terreno final. Hay que mantener `static_sizes`, `static_timers` y `min_players` con la misma longitud (ver ZON-09).
- **Esfuerzo:** S (solo datos). **Confianza:** alta en la mecánica; los números concretos dependen del mapa.

### ZON-08 — PPE de «fuera de zona» reiniciado en cada frame en el cliente
- **Severidad:** Baja. **Tipo:** degradación de rendimiento en el cliente (menor).
- **Evidencia:** `EXT\scripts_client\5_Mission\BattleRoyale\Client\BattleRoyaleClient.c:102-114` llama a `QueueAddGlassesEffect` o `QueueRemoveGlassesEffect` en cada `Update` (`:63`). Vanilla procesa esas colas en `HandleGlassesEffect` (`V130\4_world\entities\manbase\playerbase.c:9930-9952`) y ejecuta `Start()`, que siempre llama a `OnStart` y vuelve a fijar el valor objetivo (`V130\...\pperequestplatformsbase.c:38-42`). El mismo frame puede encolar `Stop()`. Usa la misma capa y el mismo parámetro que las gafas vanilla (`V130\...\pperglasses.c:10,25,36`), de modo que se mezcla con unas gafas de sol (cosmético).
- **Propuesta:** guardar `bool m_OutsideZonePPE` y encolar solo cuando cambia de estado. Opcional: otra capa (p. ej. `L_25_*` si existe una libre) para no mezclarse con las gafas. Esa capa no está verificada.
- **Esfuerzo:** S. **Confianza:** alta.

### Observaciones de coste sin hallazgo
- Daño de zona: `EXT\scripts_server\5_Mission\BattleRoyale\Server\States\6_BattleRoyaleRound.c:304-337`. Calcula una `vector.Distance` 2D por jugador desde `PlayerBase.OnScheduledTick` (`EXT\scripts_server\4_World\Entities\ManBase\PlayerBase.c:35-42`). Cuesta O(N) con constante mínima. Envía un RPC de aviso por jugador fuera de zona cada `zone_damage_tick_seconds = 5` s (`6_BattleRoyaleRound.c:323`); con 120 jugadores fuera, el peor caso son unos 24 RPC/s, asumible.
- Sincronización de la zona: CF `SendRPC` en broadcast solo en Activate y Lock (`6_BattleRoyaleRound.c:136,182,193,395-399`; `7_BattleRoyaleLastRound.c:70-84`; `5_BattleRoyaleStartMatch.c:132`). No hay SyncVars ni envío periódico.
- Reparto de objetos de inicio y teletransporte: `4_BattleRoyalePrepare.c:505-527` corre en un hilo (`GameScript.Call`, `:73`) con `Sleep(100)` por jugador y por pasada. Con 120 jugadores son unos 12 s por pasada y varias pasadas, del orden de 36-48 s en total. No hay pico de frame, pero el otro agente debe comprobar que los temporizadores del estado lo toleran (no verificado aquí).
- `IsInZone` llama dos veces a `Trace` por invocación (`BattleRoyaleZone.c:164-165`) y se usa dentro de los bucles de búsqueda de spawn (`0_BattleRoyaleState.c:251`). Solo hace ruido con `-br-trace`.

---

## 3) Bugs y robustez

### ZON-04 — Bucles infinitos posibles en la generación de zonas (el servidor se congela)
- **Severidad:** Alta. **Tipo:** cuelgue del hilo principal (el servidor deja de responder; el watchdog o el operador lo matan).
- **Evidencia:**
  - `GetValidPositionSquare`: `while(true)` sin tope (`EXT\scripts_server\5_Mission\BattleRoyale\Server\BattleRoyaleZone.c:368-379`).
  - `GetRandomPOI`: `while(true)` sin tope (`:550-563`). Si `s_POI` está vacío (terreno sin `CfgWorlds <world> Names`, o todos filtrados por `end_avoid_*`), `s_POI.Get(Math.RandomInt(0,0))` devuelve NULL y `poi[0]` lanza una excepción por NULL (`:552-556`).
  - `GetValidPositionNewCircle`: `max_try` solo se comprueba en la rama «fuera del mundo» (`:414-455`). La rama `!IsSafeZoneCenter → continue` (`:457-461`) no lo mira, así que si el área es casi toda mar o estanques se queda en un bucle infinito.
- **Propuesta:** limitar los tres bucles con `max_try` y una salida de último recurso (centro del círculo anterior o `CfgWorlds <world> centerPosition`, que el mod ya lee en `SpawnSelectionMenu.c:79`). Comprobar `s_POI.Count() == 0` y caer a `GetValidPositionSquare`.
- **Esfuerzo:** S. **Confianza:** alta en el código. Es un riesgo latente: con terrenos de costa extensa o pocos `Names` la probabilidad sube.

### ZON-05 — `RequestExit(0)` cuando no se encuentra el siguiente círculo: el servidor se apaga a mitad de partida o en el lobby
- **Severidad:** Alta. **Tipo:** cierre del proceso (apagado ordenado, no un crash del motor).
- **Evidencia:** `BattleRoyaleZone.c:418-451`. Al agotar `max_try` calcula una posición de reserva (`:420-446`) y después llama siempre a `GetGame().RequestExit(0)` (`:450`), así que la reserva no se usa. La generación es perezosa: ocurre en la primera llamada a `GetZone()`, desde `GetDynamicStartingZone` (`0_BattleRoyaleState.c:533`), `IsSafeForTeleport` (`:251`) o `ShowFirstZone` (`5_BattleRoyaleStartMatch.c:125`).
- **Propuesta:** quitar el `RequestExit` y devolver la posición de reserva limitada (`new_center` ya se ajusta a `[new_radius, world_size-new_radius]`) más un `Error()`. Generar todos los círculos al arrancar el servidor, antes de abrir el lobby, para que un fallo no corte una partida.
- **Esfuerzo:** S. **Confianza:** alta.

### ZON-09 — Accesos fuera de rango si `num_zones` no casa con las longitudes de `static_sizes`, `static_timers` y `min_players`
- **Severidad:** Media. **Tipo:** excepción de VM (índice fuera de rango) que corta la función de generación y deja `m_PlayAreas` incompleto, con NULL después.
- **Evidencia:** la comprobación tiene un error de uno, `if(i > a_StaticSizes.Count())`, y no corta la ejecución (`BattleRoyaleZone.c:219-222`); justo después lee `a_StaticSizes[i]` (`:224`). `GetZoneTimer` comprueba `x > Count()` pero indexa `[i_NumRounds - x]` (`:141-146`). `GetZoneMinPlayers` no comprueba nada (`:154`).
- **Propuesta:** validar al cargar la configuración que las tres longitudes sean `>= num_zones`, avisar y rellenar con el último valor o recortar `num_zones`.
- **Esfuerzo:** S. **Confianza:** alta.

### ZON-10 — `f_durationOffset` se calcula en un objeto y se aplica en otro: la prórroga por desplazamiento largo no llega a la zona correcta
- **Severidad:** Media. **Tipo:** degradación de jugabilidad.
- **Evidencia:** `f_durationOffset` es un campo de instancia (`BattleRoyaleZone.c:18`). Lo escribe `GetValidPositionNewCircle` (`:484-485`) durante la generación, que corre una sola vez dentro de la instancia que llamó primero a `GetBattleRoyalePlayAreas` (`m_PlayAreas` es estático, `:186-193`), y se va sobrescribiendo en cada iteración. Se suma en `GetZoneTimer` de la instancia que se consulte (`:146`). Las demás zonas ven siempre 0.
- **Propuesta:** guardar el offset en `BattleRoyalePlayArea` (un campo nuevo, `float f_ExtraSeconds`) al generar cada círculo y leerlo en `GetZoneTimer` a través de `GetArea()`.
- **Esfuerzo:** S. **Confianza:** alta.

### ZON-11 — Referencias NULL en rondas sin zona previa
- **Severidad:** Baja (solo con configuraciones extremas). **Tipo:** excepción por NULL.
- **Evidencia:** en `6_BattleRoyaleRound.c:179-182`, en la última zona, se ejecuta `GetPreviousZone().GetArea()` sin comprobación. Es NULL si el estado previo no es `BattleRoyaleRound` (`:351-366`), por ejemplo con `num_zones = 1`. En `7_BattleRoyaleLastRound.c:76-81`, `m_PreviousArea` puede quedar NULL y se usa en `:81`.
- **Propuesta:** comprobar NULL antes de enviar el RPC, igual que se hace en `LockNewZone` (`6_BattleRoyaleRound.c:387-388`).
- **Esfuerzo:** S. **Confianza:** media (depende de la configuración).

### ZON-12 — Un jugador que reconecta o entra tarde no recibe la zona actual
- **Severidad:** Baja (si el modo no permite volver a entrar, no aplica). **Tipo:** degradación del HUD.
- **Evidencia:** los RPC de zona solo se envían en Activate y Lock (lista en §2). No hay reenvío al conectar (grep de `UpdateCurrentPlayArea` sin otros puntos de envío).
- **Propuesta:** en el handler de conexión o reconexión del servidor, mandar `UpdateCurrentPlayArea`, `UpdateFuturePlayArea` y `SetCountdownSeconds` solo a esa identidad. No he verificado si el modo permite reconectar (lo cubre el agente de la máquina de estados).
- **Esfuerzo:** S. **Confianza:** media.

### ZON-13 — Airdrop: posible caída en el mar y dependencia opcional no verificada
- **Severidad:** Baja. **Tipo:** cosmético o degradación.
- **Evidencia:** si los 200 intentos fallan, usa la última posición probada aunque no sea segura (`6_BattleRoyaleRound.c:414-430`). Todo depende de `#ifdef EXPANSIONMODMISSIONS` y `ExpansionMissionModule.s_Instance.CallAirdrop` (`:409,430`). Esa API **no la he podido verificar**: `@DayZ-Expansion-Missions` está vacío en el workshop local y el mod Missions no está en `ref\`. Sin ese mod, el airdrop no hace nada y no avisa.
- **Propuesta:** comprobar la firma de `CallAirdrop` contra la versión de Expansion Missions que se despliegue. Si no se usará Missions, crear el contenedor directamente (`ExpansionAirdropContainer_Basic`, como hace COT en `ref\cot\...\JMEventsAdapter.c:334`, también bajo `EXPANSIONMODMISSIONS`). Añadir una comprobación de seguridad y no lanzar si ningún intento es válido.
- **Esfuerzo:** S. **Confianza:** media.

### ZON-14 — `GetOverrodePosition` vuelve a llamar a `Load()` y pisa con el perfil los valores de la misión
- **Severidad:** Baja. **Tipo:** configuración ignorada.
- **Evidencia:** `EXT\scripts_server\3_Game\Config\BattleRoyalePOIsData.c:59-61` llama a `Load()` (perfil, `:25-37`) después de que ya se hubiera hecho `LoadMission()`. Si existen los dos ficheros, gana el del perfil.
- **Propuesta:** quitar ese `Load()`; la configuración ya está cargada.
- **Esfuerzo:** S. **Confianza:** media (no he revisado el orden de carga en `BattleRoyaleConfig`).

### ZON-15 — `Upgrade()` de LobbyData duplica los objetos del lobby al migrar desde la versión 1
- **Severidad:** Baja. **Tipo:** corrupción menor de la configuración (lista duplicada).
- **Evidencia:** los valores por defecto ya traen 6 objetos (`EXT\scripts_server\3_Game\Config\BattleRoyaleLobbyData.c:25-32`). Si se carga un JSON con `version < 2` sin ese campo, `Upgrade` inserta otros 6 (`:84-98`) y guarda.
- **Propuesta:** en `Upgrade`, hacer `Clear()` antes de insertar, o insertar solo si la lista está vacía.
- **Esfuerzo:** S. **Confianza:** media.

---

## 4) Otras mejoras / limpieza

### ZON-07 — Valores por defecto de Chernarus; «Vigrid» no es un terreno en el código
- **Severidad:** Alta para el despliegue (datos, no código).
- **Evidencia:**
  - Spawn del lobby en `"14829.2 0 14572.3"`, la esquina NE de Chernarus (`EXT\scripts_server\3_Game\Config\BattleRoyaleSpawnsData.c:6`). La misma coordenada aparece en el código muerto de BlueZone (`BattleRoyaleServer.c:139-146`).
  - Ciudades a evitar, todas de Chernarus: `BattleRoyaleZoneData.c:7-8` y `BattleRoyaleSpawnsData.c:15-24`.
  - Overrides de POI de ejemplo **activos por defecto**: `Settlement_Chernogorsk → {100,200}` y `Settlement_Novodmitrovsk → {300,400}` (`BattleRoyalePOIsData.c:7-10`). En Chernarus moverían esos POI a coordenadas absurdas cerca de la esquina SO. En otro terreno no tienen efecto.
  - Superficies a evitar de Namalsk (`6_BattleRoyaleRound.c:446-449`, `0_BattleRoyaleState.c:268-271`) y excepción para `takistanplus` (`BattleRoyaleServer.c:513`).
  - Lo demás se calcula a partir del terreno: `GetWorldSize()` (`BattleRoyaleZone.c:179,234,400`) y `CfgWorlds <world> Names` (`:505`).
  - «Vigrid» aparece solo como nombre de proyecto o hive: `BattleRoyaleConstants.c:8,24,28`, `MainMenu.c:187`.
- **Propuesta:** confirmar con el dueño el terreno real. Crear en `$mission:Vigrid-BattleRoyale\` los ficheros `spawns_settings.json`, `zone_settings.json` y `pois_settings.json` para ese terreno, con `override_poi_positions = []`. Cambiar el valor por defecto del código a una lista vacía. Si el terreno no define `CfgWorlds <world> Names`, ver ZON-04.
- **Esfuerzo:** S (datos). **Confianza:** alta en el código; desconozco qué terreno es.

### ZON-16 — Código muerto: BlueZone/BREffectArea y ExpansionAirdropSettings
- **Severidad:** Baja. **Tipo:** limpieza.
- **Evidencia:** `#ifdef BLUE_ZONE` (`BlueZone.c:1`, `BREffectArea.c:1`, `BattleRoyaleServer.c:139`), y `BLUE_ZONE` no se define en ningún `config.cpp` (el único `defines[]` es `DAYZ_BATTLEROYALE`, `EXT\scripts_client\config.cpp:32-35`; grep sin otros resultados). Si se activara: `BREffectArea.PlaceParticles` no tiene el tope `PARTICLES_MAX` que añade vanilla (`V130\...\effectarea.c:85,425`), y `BlueZone.EEInit` llama a `InitZone()` antes que a `super.EEInit()` (`BlueZone.c:27-29`). No he comprobado si vanilla vuelve a llamar a `InitZone` dentro de `EEInit` (posible doble colocación de partículas). `EXT\scripts_server\3_Game\ExpansionAirdropSettings.c` está todo comentado.
- **Propuesta:** borrar los tres ficheros o moverlos a una carpeta `legacy/` fuera del PBO. Si se quiere un muro visual de zona, rehacerlo partiendo de la `PlaceParticles` de 1.30.
- **Esfuerzo:** S. **Confianza:** alta.

### ZON-17 — `models_shapes` no contiene ningún modelo de zona
- **Severidad:** Baja (informativo). La premisa del brief («zone.p3d config») no se cumple: `EXT\models_shapes\config.cpp:1-13` declara un `CfgPatches` vacío y la carpeta solo tiene `config.bin`/`config.cpp`. La zona es solo datos, marcadores de mapa y PPE.
- **Propuesta:** se puede quitar el PBO si no se piensa usar. **Esfuerzo:** S. **Confianza:** alta.

### ZON-18 — Webhooks: reintentos sin espera y comprobación de fin de fichero
- **Severidad:** Baja. Todos los callbacks reintentan al instante en `OnError` y `OnTimeout` (p. ej. `EventWebhook.c:69-85`). `HttpArguments.ToQuery` de DF no codifica los valores para URL; aquí son UUID, SteamID y enteros, así que no afecta hoy.
- **Lint:** el linter marca «unterminated #ifdef SERVER» en todos los ficheros de webhooks y de zona (`lint\lint_summary.txt`, regla `ES-EMPTY-IFDEF-UNSUPPORTED-PATTERN`, WARN). Es el estilo del autor (`#ifdef SERVER` sin `#endif`) y la versión publicada funcionaba. No he verificado que el preprocesador de 1.30 lo siga aceptando: hay que confirmarlo en el primer arranque y mirar el RPT.

---

## Qué NO verifiqué
- Que `api.vigrid.ovh` esté vivo o muerto (no hice llamadas de red).
- La firma de `ExpansionMissionModule.CallAirdrop` (mod Missions no disponible en local).
- La versión de Dabs Framework del servidor de destino; leí la del workshop local.
- Si el motor crea `RestApi` en el cliente sin que lo haga COT u otro mod (la posible deref NULL de ZON-01).
- La configuración de CE y persistencia de la misión (no está en la extracción): si el loot persiste entre reinicios, si `storage` está desactivado y qué contiene el `types.xml` de BR.
- El tamaño y los `Names` del terreno final.
- `BattleRoyaleMapMarkerZone.c` (dibujo del círculo en el mapa), que queda para el agente de GUI.
- Que el preprocesador de 1.30 acepte `#ifdef` sin cerrar.
- No hubo ninguna prueba in-game: todo lo anterior es análisis estático.

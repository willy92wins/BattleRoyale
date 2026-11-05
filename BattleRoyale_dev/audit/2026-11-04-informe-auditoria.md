# Auditoría del Battle Royale para DayZ 1.30

Mod auditado: DayZ-BattleRoyale v0.1.0-Vigrid (Myst, KeganH; licencia DSPL-SA), la copia de la biblioteca del proyecto, idéntica por sha1 a la del Workshop 2871877303. Objetivo: DayZ 1.30 (ejecutables 1.30.164) y partidas de 100 a 120 jugadores.

## Resumen

El mod está en mejor forma para 1.30 de lo esperado. Seis auditores revisaron contra vanilla 1.30 las firmas que el mod sobrescribe en sus áreas y no hay ninguna rotura de 1.30 demostrada. Lo más cercano es el carrusel de noticias del menú principal, un fallo visual. Lo grave es otra cosa:

1. **Seguridad de red.** El servidor se fía de lo que manda el cliente: un cliente modificado puede marcar listos a todos, teletransportar a otros, elegir spawn fuera de la zona y pasar o pausar la partida sin ser admin.
2. **Dependencia del backend del autor.** El menú principal llama de forma bloqueante a `api.vigrid.ovh` y la conexión normal por `-connect` está anulada. El autolock, si se activa, manda la contraseña RCON a una URL del autor por defecto.
3. **Cuelgues y atascos del servidor.** Bucles `while(true)` sin tope en la generación de zonas y el spawn, un `RequestExit(0)` en el arranque cuando no encuentra círculo, una configuración de zonas que no se valida, y una desconexión durante Prepare que puede dejar a todos congelados en negro.
4. **Escala.** Nada impide 120 jugadores, pero hay picos claros: Prepare tarda unos 49 s y hace hasta ~10.000 comprobaciones de colisión por jugador en un frame, y el heatmap del spawn es O(n²) en red y se redibuja línea a línea cada frame en el cliente.

Una corrección de alcance: «Vigrid» no es un mapa en el código. Es el nombre del proyecto original y de su API. El terreno se lee del juego y todos los valores por defecto son de Chernarus, así que hay que decidir el mapa (ver Decisiones).

**Método y límites.** Análisis estático: seis subagentes por área, con cita `path:line` de cada hallazgo, más el linter offline del Pack. Comprobé a mano, contra el código, UI-01, EXT-04, NET-01, SRV-10, ZON-04, ZON-05 y los `#ifdef` sin `#endif`. Después Codex (`gpt-6.1-sol`, otra familia de modelos) contrastó cada P0 con su propio `path:line`: confirmó la seguridad RPC, los bucles y los datos de mapa, y corrigió varias afirmaciones. Este informe ya incorpora sus correcciones, y comprobé las principales (`ResetPlayer`, `AutoLockWebhook`, la guarda de `num_zones`, `ENABLE_CAROUSEL`). Su revisión completa está en `BattleRoyale_dev\audit\2026-11-04-revision-codex.md`. No se ha compilado con el motor ni se ha probado nada en el juego. Sin fuentes locales quedaron Expansion Navigation, Expansion Missions (airdrop), Vehicle3PP, Dabs Framework y el killfeed ofuscado. Los costes de rendimiento son estimaciones por conteo, no perfiles. Los informes por área, con todas las citas, están en `BattleRoyale_dev\audit\2026-11-04-areas\` (01 servidor, 02 zona/loot/webhooks, 03 red/cliente, 04 UI, 05 extras/configs, 06 COT→VPP).

## Decisiones que necesito de ti

1. **Mapa.** ¿En qué terreno va a correr? Determina las tablas de zona, los POIs, el spawn del lobby, las pantallas de carga y los hints.
2. **Admin en la 1.0.** Recomiendo mantener COT en la 1.0 con los permisos arreglados en servidor y pasar a VPP en la 1.1. La alternativa es sacar la 1.0 sin panel de admin.
3. **VPP en la 1.1: ¿obligatoria u opcional?** Obligatoria es más simple (va en `requiredAddons`). Opcional exige un PBO puente aparte.
4. **Expansion.** El código solo usa Expansion Core y Navigation (mapa y marcadores); Missions es opcional y va con guarda. Recomiendo mantenerla, salvo que quieras quitarla del servidor; quitarla supone rehacer el mapa y los marcadores.

## P0 — Imprescindible para la 1.0

### Seguridad del contrato RPC
- **NET-01 / SRV-21, suplantación entre jugadores.** `PlayerReadyUp`, `PlayerUnstuck` y `OnPlayerSpawnSelected` usan el `target` que manda el cliente y no lo comparan con `sender` (`scripts_server/5_Mission/BattleRoyale/Server/BattleRoyaleServer.c:445,477`, `States/3_BattleRoyaleSpawnSelection.c:173`). Arreglo: sacar el jugador de `sender` y rechazar si no coincide. S.
- **NET-02 / SRV-20, RPC de admin sin permisos.** El `OnRPC` del módulo COT ejecuta Next, Pause, Resume y Airdrop sin comprobar nada (`scripts_server/5_Mission/COT/GUI/BRMasterControlsModule.c:6-49`), y con VPP cargado el `NextState` está abierto a cualquier cliente (`BattleRoyaleServer.c:536-551`). Arreglo: comprobación de permiso en cada handler, denegando por defecto. S.
- **NET-03, zona del spawn validada solo en el cliente.** El servidor guarda la posición elegida sin comprobar zona ni límites (`3_BattleRoyaleSpawnSelection.c:181,192`), y luego Prepare busca sitio con la comprobación de zona desactivada (`4_BattleRoyalePrepare.c:554`). Sí comprueba mar y colisión. El cliente valida la zona en `SpawnSelectionMenu.c:361-401`. Resultado: un cliente modificado puede aparecer fuera del primer círculo. Arreglo: repetir la validación de zona en servidor. S.
- **Admin, escenario concreto (Codex).** Un cliente sin permisos manda el RPC COT 11002 sin payload y la partida queda en pausa (`b_IsPaused`, `0_BattleRoyaleState.c:75,88`). 11001 y 11003 hacen Next y Resume.
- **NET-04, heatmap amplificable.** Cada selección hace broadcast de todos los puntos a todos, sin límite por emisor (`3_BattleRoyaleSpawnSelection.c:178-190`). Arreglo: límite por emisor y envío agrupado cada segundo. S.

### Backend del autor
- **ZON-01 / NET-05 / UI-30, matchmaking del cliente.** Al abrir el menú principal se hace un `GET_now` bloqueante contra `api.vigrid.ovh`, y al buscar partida un `POST_now` que además envía la lista de mods del jugador (`scripts_client/3_Game/BattleRoyale/Webhook/MatchMakingWebhook.c:28,66`; `MainMenu.c:155,348`). Si el backend no responde, el juego se congela hasta el timeout. La conexión automática por `-connect` está anulada (`scripts_client/3_Game/DayZGame.c:8-18`); el botón del menú sí conecta con los argumentos de línea de comandos. Si alguien registra ese dominio, decide a qué servidor se conectan los clientes (`MainMenu.c:404-422`). Arreglo: quitar el matchmaking y restaurar la conexión vanilla. S.
- **ZON-03, autolock.** Viene desactivado, pero si se activa manda la contraseña RCON a `autolock_url`, que por defecto apunta al dominio del autor, y arma el JSON sin escapar (`AutoLockWebhook.c:40`, `BattleRoyaleServerData.c:27-29`). Arreglo: URL vacía por defecto, y no enviar nada sin URL. Esto no sanea configuraciones ya escritas, así que hay que revisar los JSON del perfil al migrar. S.
- **ZON-02.** `enable_vigrid_api` viene apagado, bien. Si se activa con el backend muerto, bloquea el arranque (`CreateMatchWebhook.c:42`) y con `force_match_uuid` sale del servidor (`BattleRoyaleServer.c:59-67`), lo que con un reiniciador externo es un bucle. Además la contraseña del servidor sale en el log con `-br-trace` (`:58`). Arreglo: dejarlo apagado; si se quieren estadísticas, un webhook propio asíncrono (P3).

### Cuelgues y atascos del servidor
- **SRV-10, partida atascada en Prepare.** Con el menú de spawn activo (por defecto), si alguien se desconecta durante Prepare el bucle indexa `GetPlayers()[i]` con un recuento ya viejo (`States/4_BattleRoyalePrepare.c:510,538`). Las otras pasadas ya usan `a_PlayerList` (`:513,521`). El índice fuera de rango está confirmado. Que eso aborte el hilo y deje a todos en negro (sin input y en godmode, porque `Deactivate()` en `:676` no llega) es lo esperable, pero no se ha reproducido en el motor. Arreglo: usar `a_PlayerList` también ahí, más un watchdog del estado. S.
- **SRV-11 / ZON-04, bucles sin tope.** `while(true)` en `BattleRoyaleZone.c:368,550`, en la rama de centro no seguro de `:457-461` y en el spawn de último recurso de `Prepare.c:383-403`. `GetRandomPOI` lee de una lista vacía si no hay POIs (`:552`). La zona se genera en `OnInit`, así que un cuelgue impide arrancar. Arreglo: tope de intentos y reserva explícita. S.
- **ZON-05, el servidor se apaga solo.** Si no encuentra el siguiente círculo, calcula una reserva y aun así llama a `GetGame().RequestExit(0)` (`BattleRoyaleZone.c:450`). Las zonas se generan al construir las rondas en el arranque (`6_BattleRoyaleRound.c:57,72`), así que el servidor muere al iniciar, no a mitad de partida. Codex encontró una entrada que llega a esa rama: radio 35 con margen 140 da desplazamientos de 105 como máximo, que nunca salen del margen. Arreglo: validar que la reserva sirve, usarla y loguear. S.
- **ZON-09 / SRV-45, configuración de zonas sin validar (subido a P0 por Codex).** Con `num_zones=8` y las tablas por defecto de 7 elementos se lee `static_sizes[7]`. La guarda usa `i > Count()` en vez de `>=` y además no corta la ejecución (`BattleRoyaleZone.c:219-224`; `BattleRoyaleZoneData.c:27`). Arreglo: validar la config al cargar y negarse a arrancar con un mensaje claro. S.
- **SRV-19, una sola zona.** Con `num_zones=1`, `GetPreviousZone()` devuelve NULL y `Activate()` lo usa (`6_BattleRoyaleRound.c:179,351`). No es la configuración por defecto, pero entra en la misma validación. S.
- **SRV-12, NULL en los hilos de bloqueo y desbloqueo.** `HandleUnlock` (`5_BattleRoyaleStartMatch.c:138-145`), `DisableInput` (`scripts_client/4_World/Entities/ManBase/PlayerBase.c:44,50`, `GetIdentity()` sin guarda) y `Unstuck`. Si aborta el bucle, el resto se queda sin input. S. Confianza media.
- **NET-06, NULL por frame en el cliente.** `BattleRoyaleClient.Update` usa `player` sin comprobarlo (`BattleRoyaleClient.c:66,108,144,224`): excepción de script en cada frame, no crash. S.

### Datos del mapa
- **ZON-07 / SRV-42.** Spawn del lobby en 14829/14572 (`BattleRoyaleSpawnsData.c:6`), listas de ciudades a evitar de Chernarus y overrides de POI de ejemplo activos que mueven Chernogorsk a {100,200} (`BattleRoyalePOIsData.c:7-10`). Depende de la decisión 1. S-M.

## P1 — Ajustes de compatibilidad y dependencias

Nada de esto es una rotura de 1.30 demostrada, pero conviene resolverlo en la 1.0, antes de la primera compilación contra 1.30.

- **UI-01, carrusel de noticias.** En 1.30 `RefreshNewsCarousel` se llama sin comprobar si existe (`scripts/5_mission/gui/newui/mainmenu/mainmenu.c:148-157`; en 1.29 había guarda). El `MainMenu` del BR no asigna `m_NewsCarouselFrame` y su layout no tiene `carousel_Frame`, así que el carrusel nace sin padre encima del menú del BR. Afecta a la build retail: `ENABLE_CAROUSEL` solo se apaga en `BUILD_EXPERIMENTAL` (`mainmenu.c:1-3`). Es visual, no un crash, y la superposición está pendiente de verla en el juego. Arreglo: añadir un `carousel_Frame` oculto en `main_menu.layout` y asignarlo a `m_NewsCarouselFrame` en `Init`. S.
- **EXT-04, reducecardamage.** Sobrescribe `OnContact` sin `super`, se salta la caché de contactos de vanilla y aplica daño en cada contacto con umbrales calibrados para un coeficiente antiguo. **No es nuevo de 1.30**: en 1.29 ya era así y el coeficiente ya era 0.058 (`carscript.c:198` de 1.29). En 1.30 `OnContact` sube a `Transport` (`scripts/3_game/vehicles/transport.c:318`) y el override sigue siendo válido. Arreglo: sustituirlo por un ajuste de `m_dmgContactCoef` (`carscript.c:224`) que sí haga lo que promete. S.
- **EXT-01, nulledkillfeed.** PBO de terceros de 2021, ofuscado, que hace `modded` de `PlayerBase` y `PluginManager`. No se puede auditar ni arreglar, y su licencia no está clara. No hay prueba de que falle en 1.30: es una decisión de dependencia. Arreglo: retirarlo y usar el killfeed de Expansion o uno propio (P2). S.
- **EXT-02 / EXT-03, `requiredAddons`.** El cliente hace `modded class ExpansionMapMenu` (`scripts_client/5_Mission/GUI/Map/ExpansionMapMenu.c:2`) sin declarar Expansion Navigation, y faltan CF y COT. Dabs Framework sí llega, de forma transitiva, por Expansion Core. Arreglo: declarar las dependencias directas. S.
- **EXT-06, enfermedades nuevas.** `extra_preventplayermodifiers` no cubre HeatStroke, IrritatedEyes ni Silicosis. Su gravedad depende del mapa y de si esas mecánicas aparecen. S.
- **`#ifdef SERVER` sin `#endif`.** Unos 50 ficheros (`8_BattleRoyaleWin.c` es un ejemplo: abre en la línea 1 y acaba en la 129 sin cerrar). El mod se publicó así y funcionaba. No se ha comprobado si el preprocesador de 1.30 lo acepta, pero cerrarlos es una pasada mecánica. S.
- **Descartado: `ResetPlayer` sin `super` (SRV-04 / NET-19).** Todo el cuerpo vanilla está bajo `DIAG_DEVELOPER` (`scripts/4_world/entities/manbase/playerbase.c:7935-8001`), así que en retail no se pierde nada.
- **Lo que sí está bien.** Los overrides de DayZGame, VONManager, LoadingScreen, PluginDiagMenu, MissionBase/MissionGameplay, DayZPlayerImplement, PPE, DynamicMusic, IngameHud, InGameMenu, CreditsMenu, UiHintPanel y de los diez `extra_*` casan con 1.30, nombres de parámetros incluidos. `RestApi` es idéntico en 1.29 y 1.30. Los ids de RPC (11000-11009) y la sync juncture 88 no chocan con vanilla, CF, COT ni Expansion. Los dos FAIL del linter (`RemovePlayer`) son falsos positivos: `BattleRoyaleState` hereda de `Timeable`, que es del mod. Esto no sustituye a una compilación completa con el motor.

## P1 — Rendimiento: carga mínima, por impacto

Lo que más carga el servidor no está en el código del mod, sino en la misión: Chernarus vanilla trae hasta 1000 infectados y 200 animales. Recortarlos es la mejora más grande y no cuesta código. En el mod, los picos están en Prepare y en la selección de spawn; el resto son costes pequeños que se repiten en cada frame o en cada tick.

| # | Qué carga | Dónde | Arreglo | Impacto | Esf. |
|---|---|---|---|---|---|
| 1 | Infectados y animales de la misión | `ZombieMaxCount` 1000 y `AnimalMaxCount` 200 en `mpmissions/dayzOffline.chernarusplus/db/globals.xml:31` y `:3` | Bajar los infectados a lo que pida el diseño (0-200) y los animales a 0; revisar vehículos y volumen de loot en `types.xml` | Muy alto, servidor | S |
| 2 | Persistencia entre partidas | El mod apaga el servidor al acabar cada partida (`9_BattleRoyaleRestart.c:36-38`) y no limpia el mundo | Vaciar el almacenamiento del mundo en el script de reinicio, para que cada partida arranque limpia (a medir) | Medio, arranque | S |
| 3 | Búsqueda del punto de spawn en Prepare | Hasta 200 pueblos × 50 puntos por jugador sin ceder el hilo (`4_BattleRoyalePrepare.c:318-362`); cada punto hace un sphere cast, una colisión de caja y crea 3 arrays (`0_BattleRoyaleState.c:268-305`), y arma un texto de depuración aunque el log esté apagado (`:319-321`) | Precalcular puntos válidos por POI durante el lobby, repartidos entre frames; reutilizar los arrays; quitar ese texto | Alto, pico del servidor | M |
| 4 | Duración de Prepare | Varias pasadas con `Sleep(100)` por jugador (`4_BattleRoyalePrepare.c:517-606`): unos 49 s con 120 | Lotes de 5-10 jugadores por tick | Medio | S |
| 5 | Heatmap de la selección de spawn | Servidor: broadcast de todos los puntos a todos en cada clic (`3_BattleRoyaleSpawnSelection.c:178-190`). Cliente: rejilla con claves de texto y un `DrawLine` por fila de píxel, cada frame (`SpawnSelectionMenu.c:183-311`) | Servidor: envío agrupado cada segundo y límite por emisor. Cliente: recalcular solo al recibir datos | Alto, red y FPS del cliente | M |
| 6 | Etiquetas de jugador en el lobby | El BR activa en PC las etiquetas que vanilla solo usa en PS4 (`IngameHud.c:25-28`; vanilla `ingamehud.c:1214-1217`), y vanilla hace un raycast por cada jugador a menos de 15 m en cada frame (`ingamehud.c:1059-1097`). Con 120 jugadores en un lobby de 50 m de radio (`BattleRoyaleSpawnsData.c:7`) son decenas de raycasts por frame en cada cliente | Etiqueta propia con un solo raycast desde la cámara cada 0,1-0,2 s | Alto, FPS del cliente en el lobby | S |
| 7 | Logs | Cada llamada de log, aunque se descarte, consulta hasta 5 parámetros de arranque y la config del servidor (`BattleRoyaleUtils.c:91-118`), y el texto ya viene construido. Hay 385 llamadas, varias dentro de bucles | Leer el nivel una vez al arrancar y comprobarlo antes de construir el texto | Medio, servidor | S |
| 8 | Curación en el lobby | `Heal()` de cada jugador cada 5 s (`0_BattleRoyaleState.c:771-775`; `PlayerBase.c:116`), que reescribe salud, sangre, shock y stats; el godmode del lobby está comentado (`0_BattleRoyaleState.c:714-746`) | Godmode en el lobby, como ya hace Prepare (`4_BattleRoyalePrepare.c:696`), y curar solo si falta salud | Medio, servidor y red | S |
| 9 | Aviso de daño de zona | Un RPC y una notificación cada 5 s por jugador fuera de zona (`6_BattleRoyaleRound.c:323`, `7_BattleRoyaleLastRound.c:177`) | Indicador en el HUD calculado en el cliente, que ya conoce la zona y su posición: ningún RPC | Medio, red al final de la partida | S |
| 10 | Recuento de grupos (solo con Carim) | `GetGroupsCount` crea un set y un map y recorre a todos cada 0,1 s (`6_BattleRoyaleRound.c:252`, desde `BattleRoyaleServer.c:194-198`), y cada estado tiene un temporizador de 5 s que manda N RPC iguales (`0_BattleRoyaleState.c:33`, `:164-172`) | Recalcular solo cuando cambia un jugador o un grupo, y mandar la posición en un único broadcast | Medio | S |
| 11 | HUD y mapa del cliente en cada frame | Un `BattleRoyalePlayArea` nuevo por frame (`BattleRoyaleClient.c:166-167`), textos del HUD reescritos (`:81-149`), PPE reencolado (`:102-114`), `StringLocaliser` nuevo por frame en la cuenta atrás del spawn (`SpawnSelectionMenu.c:110-128`) y el círculo de zona del mapa redibujado (`BattleRoyaleMapMarkerZone.c:70-120`) | Detectar cambios y refrescar a 5-10 Hz | Bajo-medio, FPS del cliente | S |
| 12 | Modificadores del jugador | 1.30 registra 60 modificadores por jugador (`modifiersmanager.c`) y los revisa cada 3-5 s (`modifierbase.c:17-18`); el autor ya apagó hambre, sed, confort térmico y resfriado | Apagar también los que no pintan en una partida de 30 min: enfermedades, metales pesados, golpe de calor, tormenta de arena, moscas | Bajo-medio, servidor | S |
| 13 | Tamaño de descarga | `gui.pbo` pesa 126 MB por 12 pantallas de carga EDDS sin comprimir | Recomprimir: menos descarga y menos memoria en el cliente | Medio, cliente | S |
| 14 | Terceros que tocan `PlayerBase` | nulledkillfeed está ofuscado (coste por tick desconocido) y reducecardamage aplica daño en cada contacto | Retirarlos (P1 de compatibilidad y P2) | Desconocido | S |
| 15 | Tablas de zona | Calibradas para unos 50 jugadores y 15 km (primer círculo de 3375 m) | Recalibrar para 120 en Chernarus | Jugabilidad, no carga | M |

Hipótesis por medir: con 120 jugadores en 50 m, el lobby es el peor caso de red, porque cada cliente recibe a todos los demás a la máxima frecuencia. Ampliar el radio del lobby o repartir varios puntos de espera podría bajarlo.

Lo que el autor ya hace bien: el guardado de personajes está desactivado (`PlayerBase.c:111-114`), la zona se sincroniza solo al cambiar de fase, el daño de zona es una distancia 2D por jugador, y el scheduler vanilla reparte el tick de jugadores de 5 en 5 por frame (`missionserver.c:12`, `:766-784`).

Cómo lo mediremos: el MCP tiene `server_perf`, que da los FPS medio y mínimo del servidor y el recuento de jugadores, IA, infectados, animales, vehículos e items. Mediremos antes y después de cada bloque en un servidor local con la misión del BR. 120 clientes reales no se pueden simular en local: eso necesita una prueba con jugadores.

## P2 — UI y experiencia

Por valor, según el auditor de UI:
1. **HUD con fase y texto claro** («ZONA SE CIERRA 02:31», «→ 420 m», vivos/total). Hoy solo hay iconos y el reloj no dice qué cuenta (UI-20). Necesita un campo `phase` en el RPC. M.
2. **Killfeed propio**: un RPC por muerte y un pool fijo de 5 filas, que sustituye al PBO ofuscado (UI-21, EXT-01). M.
3. **Panel único de fin de partida** (puesto, kills, tiempo, asesino), localizado y que se desenlace al salir. Sustituye a las frases en inglés hardcodeadas, algunas ofensivas, al «position #-1» posible (`DayZPlayerImplement.c:9-34`) y al `win_screen` que no se desenlaza (`BattleRoyaleClient.c:195-200`) (UI-22). M.
4. **Spawn selection** con motivo de rechazo, marcador propio, zoom a la zona, leyenda y redibujo solo al cambiar (UI-23, UI-10). M.
5. **Localización y marca**: todo al stringtable (ya trae castellano completo, UI-25), URLs propias en lugar de las de LeMyst y vigrid (UI-31, EXT-24) manteniendo la atribución que exige la DSPL-SA, y pantallas de carga y hints con entrada para el mapa elegido (UI-27). S-M.

Además: contraste y tamaños del HUD y del mapa (UI-24), espectador al morir, que hoy no existe (UI-26, L).

## P3 — Otras mejoras

- **Sustituir extras por cfggameplay de 1.30.** `extra_preventweaponraise` lo cubre `WeaponObstructionData` en modo 2, y el extra deja disparar a través de paredes (EXT-08). `extra_limitunconscioustime` fuerza `StartCommand_Move` cada 0,5 s y existe `shockRefillSpeedUnconscious` (EXT-07). `defaultfullauto` puede desincronizar el modo de fuego (EXT-09, confianza baja, probar in-game). Después, consolidar los extras restantes en un único PBO (EXT-25).
- **slowzombies (2019, sin licencia):** válido en 1.30, pero no afecta a los infectados nuevos. Sustituirlo por un parche propio (EXT-19).
- **Robustez de la máquina de estados:** expulsión de quien entra tarde con un único `m_Timer` (SRV-13 / NET-12), voto de ready-up que con la división entera exige el 100 % (SRV-16), `KickWinner` que se salta jugadores (SRV-17), NULL en `HandleWinner` y en rondas sin zona previa (SRV-18, ZON-11), reconexión sin zona actual (ZON-12), prórroga aplicada a la zona equivocada (ZON-10) y `LobbyData.Upgrade` que duplica objetos (SRV-46 / ZON-15).
- **Código muerto:** BlueZone y BREffectArea (`BLUE_ZONE` no se define), `ExpansionAirdropSettings`, `models_shapes` sin modelo de zona, y los botones COT Add Player/Group/Horde/Chemicals sin handler (ZON-16, ZON-17, UI-33).
- **Estadísticas propias (opcional):** webhook asíncrono a un Discord o backend nuestro en lugar del API de Vigrid.

## Licencias y publicación

- El mod es DSPL-SA: se puede adaptar y redistribuir, pero con atribución a los autores, sin uso comercial y con la misma licencia. Eso descarta monetizar el servidor con este código y obliga a publicar el rework también como DSPL-SA.
- reducecardamage es Apache-2.0 (redistribuible con LICENSE y NOTICE), pero se sustituye por EXT-04. slowzombies y nulledkillfeed no tienen licencia visible: se retiran. La música «Apocalypse» se atribuye a Desolation Redux sin licencia verificada (EXT-21).
- Hay cuatro claves ajenas (BattleRoyale, Munghard, daemonforge, napspublic). Al retirar los terceros, se re-firma todo con una clave propia (EXT-18).

## v1.1 — Migración de COT a VPP

Resumen del informe 06:

- **Qué hace hoy el módulo COT:** Next, Pause y Resume de la máquina de estados (`0_BattleRoyaleState.c:55-92`) y Spawn Airdrop (solo con Expansion Missions). Add Player, Add Group, Horde y Chemicals envían RPC sin handler en servidor: código muerto.
- **La integración VPP que ya existe** es un esqueleto: un submenú con un único botón (Next) y un permiso de menú. Sus clases y métodos existen en la VPP actual con firmas compatibles. Le faltan el gate de permisos en servidor, permisos por acción, Pause/Resume/Airdrop, envío por `VSendRPC`, log y aviso al admin, y `DZM_VPPAdminTools` en `requiredAddons`. Sin esa entrada, si VPP compila después del BR, la integración desaparece sin error, porque `VPPADMINTOOLS` es un `#define` de fichero.
- **Patrón VPP:** el nombre del botón es la clase del submenú y su permiso. Los permisos se registran en servidor con `AddPermissionType`, y cada handler hace `if (type != CallType.Server || !sender) return;` y después `VerifyPermission(sender.GetPlainId(), "BattleRoyale:X")`, que ya deniega por defecto. Los roles de COT no se migran solos: hay que dar los permisos `BattleRoyale:*` a mano a un grupo de VPP.
- **Plan recomendado (≈1 día + un ciclo de test in-game):**
  1. Una capa neutral en servidor: `BRAdminController` con las acciones, más un autorizador que usa VPP si está, una lista `admins_steamid64` del BR si no, y deniega por defecto.
  2. Encima, el submenú VPP con 4 botones y RPCs nuevos.
  3. Borrar las carpetas COT, el enum viejo y el handler `NextState`.
  4. Aceptación: un admin con permiso ejecuta las 4 acciones y queda log, y un cliente sin grupo que envía los RPC nuevos y viejos no cambia el estado.
- **Hacer ya en la 1.0 para no rehacer trabajo:** el gate de permisos (P0), sacar la capa neutral para que la 1.1 solo cambie la UI, borrar el código muerto y fijar `requiredAddons`.

## Siguiente fase propuesta

Con tus decisiones, preparo el plan de la 1.0 (P0 y P1 completos) con criterios de aceptación verificables, y lo reviso con Codex antes de implementar. Lo implementa Grok (fallback Opus) con revisión de ChatGPT, según la escalera del pipeline. Las fuentes de trabajo partirían de `BattleRoyale_dev\extracted_v0.1.0-vigrid`, migradas a un repo `BattleRoyale/` + `BattleRoyale_dev/`.

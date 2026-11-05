<!-- Transcrito por el receptor (Claude) desde la salida final de codex exec -m gpt-6.1-sol, 2026-11-04, texto sin editar. -->

**El núcleo de seguridad del informe se sostiene. Su priorización P0 y varias afirmaciones sobre roturas nuevas de 1.30 necesitan correcciones.** Revisión estática y de solo lectura; ningún fichero modificado.

Abreviaturas para las citas:

- `S/` = `scripts_server/5_Mission/BattleRoyale/Server/`; `T/` = `S/States/`.
- `C/` = `scripts_client/5_Mission/BattleRoyale/Client/`.
- `V/` = vanilla 1.30 `scripts/`; `V129/` = raíz vanilla 1.29; `REF/` = dependencias suministradas.
- Las demás rutas son relativas a la raíz del mod.

## 1. Contraste de los P0

El veredicto distingue la evidencia del defecto de su justificación como P0.

| ID | Veredicto | Evidencia propia | Nota |
|---|---|---|---|
| NET-01 / SRV-21 | **CONFIRMADO** | `S/BattleRoyaleServer.c:445`, `:455`, `:463`, `:477`, `:493`; `T/3_BattleRoyaleSpawnSelection.c:173`, `:192` | Ninguno compara la identidad del `target` con `sender`. Permiten actuar sobre otro participante. Escenarios debajo. |
| NET-02 / SRV-20 | **CONFIRMADO** | `scripts_server/5_Mission/COT/GUI/BRMasterControlsModule.c:13`, `:35`, `:72`; `S/BattleRoyaleServer.c:22`, `:544` | COT ejecuta sin permiso. Con el registro VPP activo, `NextState` también queda accesible mediante CF. |
| NET-03 | **CONFIRMADO** | `T/3_BattleRoyaleSpawnSelection.c:192`; `T/4_BattleRoyalePrepare.c:554`, `:423`; `T/0_BattleRoyaleState.c:249` | Guarda coordenadas arbitrarias y la búsqueda posterior desactiva la comprobación de zona. **Sí existe** comprobación posterior de mar y colisión: no garantiza materializar cualquier coordenada recibida. |
| NET-04 | **CONFIRMADO** | `T/3_BattleRoyaleSpawnSelection.c:178`, `:185`, `:189` | Cada mensaje aceptado reconstruye y difunde el heatmap fiable, sin límite por emisor. El mecanismo de amplificación existe; no está demostrado un umbral de saturación. |
| ZON-01 / NET-05 / UI-30 | **PARCIAL** | `scripts_client/3_Game/BattleRoyale/Webhook/MatchMakingWebhook.c:28`, `:66`; `scripts_client/5_Mission/Mission/MainMenu.c:155`, `:348`, `:422`; `scripts_client/3_Game/DayZGame.c:8` | GET al abrir y POST al buscar, ambos bloqueantes. El automatismo CLI está anulado, pero el botón conserva una conexión con los argumentos CLI. La lista de mods se envía al buscar, no en cada apertura. |
| ZON-03 | **CONFIRMADO** | `scripts_server/3_Game/BattleRoyale/Webhook/Servers/AutoLockWebhook.c:40`; `scripts_server/3_Game/Config/BattleRoyaleServerData.c:27`, `:29`; `T/2_BattleRoyaleCountReached.c:32` | Envía RCON sin escapar el JSON, **si se activa y configura autolock**; viene desactivado. La cita `AutoLockWebhook.c:260` del informe no existe: el envío está en `:40`. |
| ZON-02 | **PARCIAL** | `S/BattleRoyaleServer.c:58`, `:64`, `:67`; `scripts_server/3_Game/BattleRoyale/Webhook/Matches/CreateMatchWebhook.c:42` | Bloqueo y salida condicionados confirmados. Un bucle de reinicios requiere un supervisor externo; `RequestExit` no reinicia por sí mismo. |
| SRV-10 | **PARCIAL** | `T/4_BattleRoyalePrepare.c:510`, `:517`, `:538`, `:676`; `S/BattleRoyaleServer.c:336` | Desconectarse entre las pasadas reduce la lista viva y deja índices fuera de rango. No he demostrado con el motor que ese acceso aborte el hilo y produzca exactamente el bloqueo global descrito. |
| SRV-11 / ZON-04 | **CONFIRMADO** | `S/BattleRoyaleZone.c:368`, `:457`, `:550`, `:552`; `T/4_BattleRoyalePrepare.c:383` | Hay búsquedas sin presupuesto global. Lista de POI vacía y dominio sin candidatos válidos son casos sin salida válida. No he reproducido el cuelgue con un terreno/configuración real. |
| ZON-05 | **PARCIAL** | `S/BattleRoyaleZone.c:418`, `:450`; `T/6_BattleRoyaleRound.c:57`, `:72`; `S/BattleRoyaleServer.c:120` | La salida explícita existe. Las zonas se generan al construir las rondas durante el arranque: **«a mitad de partida» no queda sustentado** por esta ruta. |
| SRV-12 | **PARCIAL** | `T/5_BattleRoyaleStartMatch.c:141`, `:183`; `scripts_client/4_World/Entities/ManBase/PlayerBase.c:44`, `:50` | Desreferencias sin guarda confirmadas. Falta demostrar la ventana de destrucción/identidad nula; un fallo de `Unstuck` aislado tampoco implica congelar al resto. |
| NET-06 | **PARCIAL** | `C/BattleRoyaleClient.c:66`, `:144`, `:224` | Si falta el jugador con zona activa o cambio de input pendiente, hay desreferencia nula. La repetición por frame y su ventana real no están reproducidas. |
| UI-01 | **PARCIAL** | `V/5_mission/gui/newui/mainmenu/mainmenu.c:1`, `:156`, `:413`; `V/5_mission/gui/newui/mainmenu/newscarousel.c:49`; `scripts_client/5_Mission/Mission/MainMenu.c:20` | La creación con padre nulo está sustentada cuando `ENABLE_CAROUSEL` está activo; se excluye en `BUILD_EXPERIMENTAL`. Superposición visual pendiente de observar. No justifica P0 por sí sola. |
| EXT-04 | **PARCIAL** | `reducecardamage/scripts/4_World/ReduceCarDamage/ReduceCarDamage.c:22`, `:46`; `V129/4_world/entities/vehicles/carscript.c:198`, `:1466`, `:1483` | El bypass de caché existe, pero **ya existía en 1.29**, cuyo coeficiente también era `0.058`. No está demostrada una regresión nueva ni mayor daño causado por 1.30. |
| EXT-01 | **NO VERIFICABLE** | No hay fuente `nulledkillfeed` en el árbol suministrado. | No puedo confirmar incompatibilidad, seguridad ni imposibilidad de reparación. Su opacidad justifica una decisión de dependencia, no un fallo técnico P0 demostrado. |
| EXT-02 / EXT-03 | **PARCIAL** | `scripts_client/config.cpp:5`; `scripts_client/5_Mission/GUI/Map/ExpansionMapMenu.c:2`; `REF/expansion_scripts/config.cpp:11`; `REF/exp_core_scripts/config.cpp:11` | Navigation no está declarada y su uso es incondicional en cliente. Pero **Dabs sí está requerido transitivamente** por Expansion Core. Falta probar el orden efectivo y completar el grafo de dependencias. |
| SRV-04 / NET-19 | **REFUTADO como P0 retail** | `scripts_server/4_World/Entities/ManBase/PlayerBase.c:169`; `V/4_world/entities/manbase/playerbase.c:7938`, `:8001` | El cuerpo completo de `ResetPlayer` vanilla está bajo `DIAG_DEVELOPER`. Añadir `super` no aporta esa limpieza en retail; cualquier omisión del reset propio debe justificarse por separado. |
| EXT-06 | **CONFIRMADO** | `V/4_world/classes/playermodifiers/modifiersmanager.c:148`; `extra_preventplayermodifiers/Scripts/4_World/Classes/PlayerModifiers/Modifiers/Diseases/CommonCold.c:3` | El extra no intercepta los modificadores nuevos. No he encontrado evidencia de que esta omisión sea P0; depende del terreno, exposición y contrato de gameplay. |
| `#ifdef` sin cierre | **CONFIRMADO** | `T/8_BattleRoyaleWin.c:1`; EOF en `:129`, sin `#endif` | La ausencia es real. Que impida compilar en 1.30 sigue sin verificarse; no debe presentarse como incompatibilidad confirmada. |
| ZON-07 / SRV-42 | **CONFIRMADO** | `scripts_server/3_Game/Config/BattleRoyaleSpawnsData.c:6`; `scripts_server/3_Game/Config/BattleRoyalePOIsData.c:8`; `S/BattleRoyaleZone.c:527` | Los valores y overrides activos existen y se consumen. Su gravedad depende del mapa y de las configuraciones efectivas. |

**Entradas y resultados de seguridad:**

- **Suplantación:** cliente A envía al namespace `RPC-DayZBR-Server` `PlayerReadyUp`, payload `Param1<bool>(true)`, con `target=B` participante del lobby → B queda listo. `PlayerUnstuck`, sin payload y `target=B` durante StartMatch → se programa el desplazamiento de B. `OnPlayerSpawnSelected`, `Param1<vector>(P)`, `target=B` → cambia el spawn de B.
- **Admin:** cliente sin permisos envía RPC COT **11002**, sin payload → `Pause()` pone `b_IsPaused=true`, y `IsComplete()` deja de permitir avanzar (`T/0_BattleRoyaleState.c:75`, `:88`). **11001/11003** ejecutan Next/Resume; **11006** solicita airdrop si Missions está activo. Con el registro VPP compilado, CF `RPC-DayZBR-Server::NextState` también desactiva el estado sin autorización.
- **Spawn:** seleccionar un punto terrestre P fuera del primer círculo, con candidatos físicamente seguros alrededor → Prepare acepta un candidato sin comprobar pertenencia a la zona. No he verificado una coordenada concreta del terreno.
- **Amplificación:** repetir `OnPlayerSpawnSelected` con objetivo conectado y vector válido → un broadcast fiable del mapa completo por mensaje aceptado. **No encuentro prueba de caída del servidor** por esta entrada.

**Escenarios de cuelgue/salida:** una búsqueda cuyos candidatos sean siempre rechazados permanece en los bucles citados. Además, `GetValidPositionNewCircle("0 0 0", 35, 140)` genera desplazamientos de magnitud máxima 105: los centros quedan fuera del margen obligatorio de 140, agota los intentos y solicita salir. Esto demuestra la rama matemáticamente; no demuestra que la configuración desplegada la alcance.

## 2. P0 adicionales no mencionados

**No encontré nuevos P0 con evidencia suficiente.** No elevo desreferencias condicionales ni discrepancias de firmas a crash o pérdida de partida sin demostrar su consecuencia.

## 3. Qué falla en la premisa o priorización

- **«Dos roturas claras de 1.30» es excesivo.** UI-01 depende de la variante de build; EXT-04 describe principalmente un problema preexistente en 1.29. Trasladar `OnContact` a una clase base no invalida el override.
- **P0 mezcla gravedad técnica, configuración y mantenimiento.** Mantendría primero autorización RPC, suplantación, spawn y disponibilidad. Carrusel, enfermedades y limpieza de directivas necesitan categorías distintas.
- **Elevaría ZON-09/SRV-45 a bloqueo de configuración.** `num_zones=8` con las tablas por defecto de siete elementos alcanza `static_sizes[7]`; la guarda usa `i > Count()` y además no detiene la ejecución (`S/BattleRoyaleZone.c:219`, `:224`; `scripts_server/3_Game/Config/BattleRoyaleZoneData.c:27`).
- **SRV-19 merece prioridad superior cuando se admite una sola zona.** Con `num_zones=1`, la única ronda tiene StartMatch como predecesor; `GetPreviousZone()` devuelve NULL y `Activate()` lo desreferencia (`T/6_BattleRoyaleRound.c:179`, `:351`). Es una ruta concreta, aunque no la configuración predeterminada.
- **Las propuestas resumidas requieren completar su contrato.** Añadir el frame del carrusel también exige asignar `m_NewsCarouselFrame`; vaciar el valor predeterminado de autolock no sanea configuraciones existentes; eliminar `RequestExit` exige validar que la reserva sea utilizable.

## 4. LO QUE NO PUDE VERIFICAR

- Compilación y ejecución en motor 1.30; comportamiento exacto de índices fuera de rango y excepciones en hilos.
- Reproducciones de desconexión durante Prepare/Unlock, congelaciones y fallos por frame.
- PBO desplegado, hashes, configuraciones efectivas y variantes de compilación.
- Código del killfeed, Navigation, Missions, Dabs, Vehicle3PP y Carim no suministrados íntegramente.
- Disponibilidad o control actual del dominio, tiempos HTTP, tráfico real, daño de vehículos y capacidad con 100–120 jugadores.
- La afirmación global de compatibilidad de todos los overrides: esta revisión contrastó los hallazgos y rutas relevantes, no realizó una compilación integral.
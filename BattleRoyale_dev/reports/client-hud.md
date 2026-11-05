# Informe de la lane client-hud (BattleRoyale 1.0)

Rama `lane/client-hud`, worktree `C:\Users\guill\Repos\br-wt\client-hud`. Commits: `5ef057d` (entrega inicial) y
el que contiene la sección «Ronda 1 de correcciones» de este fichero
(`git log -1 -- BattleRoyale_dev/reports/client-hud.md`). Rutas de código relativas a `BattleRoyale/addons/`.
Vanilla citado = `C:\Users\guill\OneDrive\Documentos\DayZ Projects\scripts\` (1.30); dependencias = `C:\Users\guill\Repos\br-ref\`.

Resultado: los 8 items hechos. Ningún error de lint nuevo de otro tipo; el recuento de `scripts_client` pasa de 10 a 11
por un `ES-LAYOUT-PATH-PBOPREFIX-MISMATCH` más (el `killfeed.layout` nuevo), el mismo falso positivo de la base. Nada
probado en el juego.

## Items

### 1. Rendimiento #6: etiquetas de jugador en el lobby
- **Cambio:** `scripts_client/5_Mission/GUI/IngameHud.c:36-56` ya no llama a la `RefreshPlayerTags` vanilla. Un
  acumulador lanza `BR_RefreshPlayerTag()` cada 0,15 s (`:7`, `:39-44`). Esa función (`:60-99`) hace **un** raycast
  desde la cámara: mismo rayo de 25 m, mismo `sorted = true` y mismo `ignore` que vanilla. Acepta el primer impacto si
  es un `PlayerBase` distinto del local a ≤15 m de la cámara (`:95`). `RaycastRVParams` y el array de resultados se
  crean una vez y se reutilizan (`:72-82`). El widget `TagFrame` se busca y se redimensiona una sola vez, al aparecer
  `m_PlayerTag` (`:47-55`). Mejora sobre vanilla: si el rayo da en ropa o mochila (proxy, `hierLevel > 0`), se etiqueta
  al `parent` (`:90-92`).
- **APIs:** vanilla `5_mission/gui/ingamehud.c:1059-1097` (`RefreshPlayerTags`: un raycast por jugador),
  `:1099-1139` (`ShowPlayerTag`), `:1214-1217` (solo PS4) y `:116-118` (miembros). `3_game/global/dayzphysics.c:49-92`
  (`RaycastRVParams`), `:97-112` (`RaycastRVResult.obj/parent/hierLevel`) y `:208` (`RaycastRVProxy`).
  `3_game/global/game.c:731-732` (cámara) y `:944` (`GetPlayer`).
- **Verificado:** firmas abiertas en vanilla; lint y `ui_reconcile` (abajo). `TagFrame` sigue en el WARN de base de
  `ui_reconcile` porque es un widget vanilla (`gui/layouts/new_ui/hud/hud_player_tag.layout`).
- **Sin verificar:** coste real por frame y que la etiqueta se vea igual en el juego.

### 2. Rendimiento #5 (cliente): heatmap y cuenta atrás de la selección de spawn
- **Cambio:** en `scripts_client/5_Mission/GUI/SpawnSelectionMenu.c`, `Update` (`:125-153`) ya no dibuja en cada
  frame. Hay dos banderas:
  - `b_HeatMapDirty` se pone a true en `UpdateHeatMap` (`:569`) y en `SetSpawnSize` (`:375`). Con ella,
    `RebuildHeatMap` (`:241-348`) recalcula la rejilla una vez y guarda la lista de cuadrados a pintar. La clave de
    celda es un `int` con `map<int,int>` (`:260`), en lugar del texto `"x,z"` con `Split`.
  - `b_CanvasDirty` se pone a true cuando cambian los datos, el compañero (`:594`) o la primera zona (`:388`), y
    cuando `HasMapViewChanged` (`:189-207`) detecta otra escala, posición del mapa, posición en pantalla o tamaño.
    Solo entonces `RedrawCanvases` (`:209-238`) limpia y repinta los dos canvas.

  La cuenta atrás (`UpdateCountdownText`, `:156-186`) crea el `StringLocaliser` solo cuando cambia el segundo
  mostrado, con la misma aritmética entera que el original, así que el texto es el mismo. Los óvalos usan 64-96
  segmentos en lugar de ≥360 (`:491`, UI-10 punto 4). El relleno de cada cuadrado sigue siendo un `DrawLine` por fila,
  ahora solo al redibujar.
- **APIs:** `3_game/gameplay.c:322-334` (`MapWidget.GetScale/GetMapPos/MapToScreen`), `1_core/proto/enwidgets.c:341-345`
  (`CanvasWidget.DrawLine/Clear`), `:151-154` (`GetScreenPos/GetScreenSize`).
  `cf_scripts/3_Game/CommunityFramework/Utils/CF_Localiser.c:125` (`Set(int,int)`).
- **Verificado:** con un script en Python, la ida y vuelta de la clave entera para celdas entre -4096 y 4095, con la
  clave máxima por debajo de 2^31.
- **Sin verificar:** que el heatmap se vea idéntico en el juego y el coste mientras se arrastra o se hace zoom, que
  sigue redibujando en cada frame de interacción.

### 3. Rendimiento #11 + NET-06: `BattleRoyaleClient.Update`
- **Cambio:** en `scripts_client/5_Mission/BattleRoyale/Client/BattleRoyaleClient.c`, `Update` (`:76-188`) separa dos
  bloques.
  - Primero, `if (!gameplay) return;` (`:79`) y el estado que no necesita jugador: recuentos, kills, inicio, cuenta
    atrás, zonas, pantalla de victoria, fase y killfeed.
  - Después, `if (!player) return;` (`:160`) antes de cualquier uso del jugador. `GetZoneDistance`, `FadeIn` y
    `FadeOut` reciben el jugador por parámetro (`:402`, `:429`, `:438`).
  - `new BattleRoyalePlayArea` solo cuando cambian el centro o el radio que manda el servidor (`:113-120`). Si el
    servidor vacía la zona, se conserva la última, como en el original.
  - `PlayerCountChanged` y `UpdateKillCount` solo si cambia el valor (`:85-97`).
  - La distancia a la zona actual se calcula una vez por frame (`UpdateZoneHud`, `:191-252`).
  - El efecto PPE de fuera de zona se encola solo en la transición dentro/fuera o al cambiar la entidad del jugador
    (`UpdateOutsideZoneEffect`, `:255-275`), y no se encola con el jugador muerto. Antes se encolaba en cada frame, y
    con el jugador muerto la cola crecía sin procesarse.
  - `OnSecond` vuelve a encolarlo si otro sistema paró el requester (`:463-469`).
  - En `scripts_client/5_Mission/GUI/HUD/BattleRoyaleHud.c`, los setters tocan los widgets solo al cambiar:
    - `SetVisible` (`:111-115`).
    - Colores por estado (`:194-196`, `ApplyDistanceColors`).
    - Flecha si gira ≥0,5° (`:199-203`).
    - Texto si cambian los metros enteros (`:210-217`).
  - El `Debug(string.Format)` queda bajo `#ifdef BR_TRACE_ENABLED` (`:167`). Se corrigen las guardas de `SetCount`
    (`:242`) y `SetKillCount` (`:271`) (UI-32).
- **Extra (NET-11, mismo fichero):** el destructor retira la llamada repetitiva `OnSecond` (`:37`) y `OnSecond`
  comprueba `gameplay` (`:450`).
- **APIs:** `4_world/entities/manbase/playerbase.c:1145-1153` (`QueueAdd/RemoveGlassesEffect`), `:9930-9953`
  (`HandleGlassesEffect`), `:3038`, `:3040-3048` y `:3071` (solo se procesa con el jugador vivo).
  `3_game/ppemanager/pperequesterbank.c:111` (`GetRequester(int)`).
  `3_game/ppemanager/requesters/pperequestplatformsbase.c:44-55` (`Stop` sin datos no hace nada; `IsRequesterRunning`).
  `2_gamelib/tools.c:63` y `:69` (`CallLaterByName/RemoveByName`). `1_core/proto/enwidgets.c:125` y `:138`
  (`Show/IsVisible`).
- **Sin verificar:** FPS antes y después; que ningún otro mod pare el requester a mitad (el reenganche cada segundo lo
  cubre, pero no se ha medido).

### 4. UI-13: círculo de zona en el mapa
- **Cambio:** en `scripts_client/5_Mission/GUI/Map/BattleRoyaleMapMarkerZone.c`, `Update` (`:78-169`) repinta el canvas
  solo si cambian:
  - el radio en píxeles (±0,5 px), que recoge el zoom;
  - la posición del mundo;
  - el tamaño del canvas;
  - o un setter (`b_RedrawNeeded`: `:24`, `:300-353`; los setters solo la activan si el valor cambia).

  `Show()` ya no se llama en cada frame (`:85-86`). Tras redimensionar, el canvas se relee para que el centro sea el
  nuevo (`:116-118`). `RenderOval` usa 64-96 segmentos (`:174`). En `scripts_client/5_Mission/GUI/Map/ExpansionMapMenu.c`
  (`:44-45` y `:63-64`), `UpdateZones` solo llama a `SetPosition` si el centro cambió (los tamaños ya los filtran los
  setters).
- **APIs:** `exp_core_scripts/5_Mission/DayZExpansion_Core/ExpansionMapWidgetBase.c:112-119` (el `Update` base recoloca
  el marcador en cada frame), `:174-188` (`SetPosition/GetPosition`) y `:202-215` (`Show`).
  `navigation_scripts/5_Mission/DayZExpansion_Navigation/ExpansionMapMenu.c:1360` (llama a `Update` de cada marcador
  en cada frame).
- **Verificado:** el error de la cuerda frente al círculo, `r(1-cos(π/n))`, queda por debajo de 1 px con n=96 hasta un
  radio de unos 1800 px (cálculo, no medida).
- **Sin verificar:** el aspecto en el juego. Las variantes `Lined` y `Crosshatched` (sin uso) siguen como estaban.

### 5. UI-20: fase en el HUD
- **Cambio:**
  - `scripts_client/3_Game/BattleRoyale/RPC/BattleRoyaleRPC.c`:
    - Registra `SetMatchPhase` (`:22`) con el payload `Param2<int,int>` del contrato (`:307-327`).
    - Guarda la fase y el instante de fin (`match_phase_end_time = GetTime() + s*1000`, o -1).
    - `GetMatchPhaseSecondsLeft()` (`:331-341`) da los segundos que quedan.
    - `Reset()` limpia ambos (`:59-61`).
  - `BattleRoyaleClient.UpdateMatchPhase` (`:297-384`) elige la etiqueta y el temporizador que esa etiqueta describe:

    | Fase | Etiqueta y temporizador |
    |---|---|
    | LOBBY / SPAWN_SELECTION / PREPARE / RESTARTING | Nombre de la fase y su temporizador, si lo hay |
    | STARTING | «LA PARTIDA EMPIEZA EN» con el temporizador de fase; después, «PRIMERA ZONA EN» con la cuenta atrás de zona. Ronda 1: con tiempo desconocido, «LA PARTIDA EMPIEZA» sin temporizador |
    | ROUND | «ZONA SE CIERRA» antes del bloqueo y «FIN DE RONDA EN» después, los dos con la cuenta atrás de zona (`SetCountdownSeconds`, la misma del reloj). El bloqueo se detecta cuando la zona actual es igual a la futura (`IsZoneLocked`, `:278-284`). Si no ha llegado la cuenta atrás (reconexión), «FIN DE RONDA EN» con el temporizador de fase |
    | LAST_ROUND | «LA ZONA DESAPARECE EN» con la cuenta atrás, «SIN ZONA SEGURA» tras el colapso y «ZONA FINAL» sin datos |
    | FINISHED | «PARTIDA TERMINADA» |

  - El HUD (`BattleRoyaleHud.SetMatchPhase`, `:293-313`) solo reescribe el texto si cambian la etiqueta o el segundo.
  - Panel nuevo en `gui/layouts/hud/br_hud.layout:294-325` (`PhasePanel`/`PhaseText`, arriba en el centro, con
    contorno).
  - 12 claves `STR_BR_PHASE_*` en `languagecore/stringtable.csv:49-60`.
- **Contrato:** el payload y el registro casan con `BattleRoyale_dev/contracts/rpc-1.0.md`. La RPC se acepta en
  cualquier orden: solo guarda, y el HUD pinta en el siguiente frame si existe. CF entrega con `Param4<CallType, ...>`
  (`cf_scripts/3_Game/CommunityFramework/RPC/RPCManager.c:87-98`; `AddRPC` en `:171`).
- **Sin verificar:** con el servidor real (la lane del servidor aún no ha hecho commit) y la posición del panel junto
  a la brújula de Expansion u otros HUD.

### 6. UI-21: killfeed propio
- **Cambio:**
  - `BattleRoyaleRPC.KillFeedEvent` (`:354-372`, registro en `:23`, `Param4<string,string,string,float>`) escribe en
    un búfer circular de tamaño fijo (5) con hora de llegada. `GetKillFeedEvent` (`:376-388`) lo lee por número de
    serie.
  - `scripts_client/5_Mission/GUI/HUD/BattleRoyaleKillFeed.c` (nuevo) crea `gui/layouts/hud/killfeed.layout` (nuevo,
    5 filas `KillFeedRow0..4`, ocultas por defecto) como hijo de la raíz del HUD, así que se desenlaza con ella.
  - `Update` (`:49-61`) solo trabaja si llegó un evento o caducó la fila más antigua. `Refresh` (`:63-102`) rellena
    las filas reutilizadas, la más nueva arriba, y oculta cada una a los 6 s (`ROW_LIFETIME_MS`, `:6`). Sin
    asignaciones por frame.
  - Asesino vacío: en la entrega inicial salía «La zona» / «The zone». La ronda 1 lo sustituye por una frase neutra,
    «Ana ha muerto» (ver «Ronda 1 de correcciones», D4).
  - Formato: «%1 eliminó a %2» más `[arma, N m]` cuando hay datos (`:105-125`). Si el arma llega como clave `#STR`,
    se traduce en el cliente (`:110-112`).
  - Se crea en `MissionGameplay.InitBRhud` (`scripts_client/5_Mission/Mission/MissionGameplay.c:106`) y se actualiza
    desde `UpdateKillFeed` (`:182-188`).
- **Verificado:** un port en Python de `KillFeedEvent`, `GetKillFeedEvent`, `Update` y `Refresh` coincide con un
  oráculo de fuerza bruta en 2000 trazas aleatorias de 200 frames (filas visibles = los ≤5 eventos más nuevos con
  menos de 6 s). Esto comprueba el algoritmo, no que el Enforce compile.
- **Sin verificar:** en el juego; el aspecto de las filas, arriba a la izquierda, frente a otros HUD de Expansion.

### 7. Fila #9 (cliente): indicador de daño de zona
- **Cambio:** `BattleRoyaleClient.UpdateZoneHud` (`:241-251`) muestra el indicador si el jugador está vivo y:
  - está fuera de la zona actual (distancia ≥ radio, el mismo criterio que el servidor);
  - o el servidor ha borrado la zona en la última ronda (`IsZoneCollapsed`, `:287-294`).

  El HUD lo muestra u oculta solo al cambiar (`ShowZoneDamage`, `BattleRoyaleHud.c:138-141`). Panel en
  `gui/layouts/hud/br_hud.layout:326-356` (`ZoneDamagePanel`, rojo de Expansion, con contorno). Texto
  `STR_BR_HUD_ZONE_DAMAGE` = «RECIBIENDO DAÑO DE ZONA» / «TAKING ZONE DAMAGE» (`stringtable.csv:61`).
- **Base en el servidor (solo lectura):**
  - Ronda: daño fuera de la zona activa (`scripts_server/.../States/6_BattleRoyaleRound.c:307-318`).
  - La zona activa es la que llega como `UpdateCurrentPlayArea` (`:182`, `:395`).
  - Última ronda: tras `LockFinalZone`, que envía la zona actual `"0 0 0"` (`7_BattleRoyaleLastRound.c:200`), todos
    reciben daño (`:149-150`).
- **Sin verificar:** el cliente no sabe si el servidor tiene `enable_zone_damage = false`. Tampoco cubre a quien
  reconecta tras el colapso de la última ronda: no tiene zona previa.

### 8. UI-22 mínimo: `win_screen` y pantalla de muerte
- **Cambio:**
  - El `win_screen` lo crea y guarda `MissionGameplay.ShowWinScreen` (`MissionGameplay.c:191-201`, llamada desde
    `BattleRoyaleClient.c:145`), y `~MissionGameplay` lo desenlaza (`:41-44`).
  - `gui/layouts/hud/win_screen.layout:2`: la raíz tiene `ignorepointer 1` y el texto pasa a `#STR_BR_YOU_SURVIVED`
    (`:20`).
  - En `scripts_client/4_World/Entities/DayZPlayerImplement.c` se quitan las dos frases ofensivas que señalaba la
    auditoría («Stupid, but bold», «incompetence»). Las 11 restantes pasan a `STR_BR_DEATH_MESSAGE_1..11`
    (`:9-21`), traducidas con `Widget.TranslateString`.
  - La línea de puesto («Has sido eliminado en la posición #N», `STR_BR_DEATH_POSITION`) solo sale si
    `position_top > 0` (`:30-34`), así que ya no puede salir «#-1» ni «#0».
- **APIs:** `1_core/proto/enwidgets.c:112` (`TranslateString`), `:173` (`Unlink`). `3_game/tools/uimanager.c:53`
  (`ScreenFadeIn`), `4_world/entities/dayzplayerimplement.c:802-831` (vanilla `ShowDeadScreen`).
  `1_core/proto/enstring.c:526` (`string.Format`).
- **Sin verificar:** en el juego. La expulsión a los 15 s (`LeaveServer`) sigue como estaba, fuera de alcance.

## Ronda 1 de correcciones

Corrige los defectos de la revisión de Codex (`C:\Users\guill\Repos\br-runs\client-hud\review\codex_review.md`) y los
encargos extra del coordinador. Commit nuevo sobre `5ef057d`, con los mismos ficheros propios.
`scripts_client/5_Mission/GUI/Hints/UiHintPanel.c` ya era de esta lane (`5_Mission/GUI/**`).

1. **D1 (MAJOR): el aviso de daño salía fuera de las fases con daño.**
   - **Cambio:** `BattleRoyaleClient.c:244` solo evalúa el aviso si `IsZoneDamagePhase` (`:286-290`) devuelve
     `ROUND` o `LAST_ROUND`. Ya no sale tras ganar (FINISHED, aunque la zona siga colapsada), ni en lobby, selección de
     spawn, Prepare, STARTING o RESTARTING.
   - **Efecto lateral:** si el servidor nunca envía `SetMatchPhase` (fase -1), el aviso no sale nunca.
   - **Limitación, sin arreglar como pidió el coordinador:** con `enable_zone_damage = false` en el servidor, el aviso
     sigue saliendo fuera de zona durante las rondas. El contrato 1.0 no transmite ese dato.
2. **D2 (MINOR): el colapso dependía de haber recibido una zona antes.**
   - **Cambio:** `IsZoneCollapsed(br_rpc)` (`BattleRoyaleClient.c:292-300`) ya no depende de `m_CurrentPlayArea`. En
     `LAST_ROUND`, una zona actual vacía (`"0 0 0"`, radio 0) cuenta como colapsada aunque el cliente acabe de
     conectar. Fuera de `LAST_ROUND` nunca hay colapso.
   - **Consecuencia aceptada:** un cliente que conecte en `LAST_ROUND` antes del bloqueo final y no reciba la zona
     actual verá «SIN ZONA SEGURA» y el aviso hasta que llegue una zona. Hoy el servidor no la reenvía al conectar
     (NET-21).
3. **D3 (MINOR): STARTING con tiempo desconocido mostraba «PRIMERA ZONA EN 00:00».**
   - **Cambio:** con `SetMatchPhase(STARTING, -1)`, `BattleRoyaleClient.c:326-343` muestra la etiqueta de STARTING sin
     temporizador: «LA PARTIDA EMPIEZA» / «MATCH STARTING».
     - Etiqueta nueva: `PHASE_LABEL_STARTING` (`BattleRoyaleHud.c:18` y `:344-345`).
     - Clave nueva: `STR_BR_PHASE_STARTING` (`stringtable.csv:76`).
   - «PRIMERA ZONA EN» solo sale cuando el temporizador de fase ha llegado a 0 (el desbloqueo) y la cuenta atrás de
     zona es mayor que 0 (`:333`). Ya no puede salir «PRIMERA ZONA EN 00:00».
4. **D4 (MINOR): el asesino vacío se mostraba siempre como la zona.**
   - **Cambio:** sin asesino, `BattleRoyaleKillFeed.c:104-108` escribe una fila neutra, «Ana ha muerto» / «Ana died»
     (`STR_BR_KILLFEED_DIED`, `stringtable.csv:77`), sin arma ni distancia.
   - Se retira la clave `STR_BR_KILLFEED_ZONE`, ya sin uso: un grep en `addons` no da referencias.
5. **Hints (heredado de la revisión de client-menu).**
   - **Cambio:** `scripts_client/5_Mission/GUI/Hints/UiHintPanel.c:1-19` reactiva el override con la firma de vanilla
     1.30, `override protected void LoadContentList()` (vanilla `3_game/gui/hints/uihintpanel.c:85-90`). Hay que
     sobrescribir el cargador porque `m_DataPath` es `protected const` (`:14`).
   - Carga `Vigrid-BattleRoyale/Data/hints.json`; `data/$PBOPREFIX$` es `Vigrid-BattleRoyale\Data`. Usa
     `JsonFileLoader<array<ref HintPage>>.LoadFile` (`3_game/tools/jsonfileloader.c:7`), el mismo cargador y el mismo
     estilo de ruta con que el mod ya lee `LoadingScreens.json` (`scripts_client/3_Game/Loading/LoadingScreenBackground.c:33`).
   - Si falla o la lista sale vacía, llama a `ErrorEx` y vuelve a los hints de vanilla (`super.LoadContentList()`).
   - **Verificado:** `hints.json` casa con `HintPage` (`m_Headline`, `m_Description` y `m_ImagePath`;
     `3_game/gui/hints/hintpage.c:7-9`): es JSON válido con 3 entradas (comprobado con Python). Con `m_ImagePath` vacío,
     vanilla oculta la imagen (`uihintpanel.c:140-156`).
6. **Logs construidos antes del filtro.**
   - **Cambio en `BattleRoyaleHud.c:168`:** antes iba bajo `#ifdef BR_TRACE_ENABLED`; ahora va tras
     `CheckLogLevel(DEBUG)`. Así se mantiene el log de depuración en release cuando el nivel lo pide, que era el
     comportamiento original.
   - **Cambio en `BattleRoyaleRPC.c`:** las 10 llamadas `Trace(string.Format(...))` de los handlers quedan tras
     `if (BattleRoyaleUtils.CheckLogLevel(BattleRoyaleUtils.TRACE))`, en `:79`, `:100`, `:127`, `:179`, `:200`, `:223`,
     `:245`, `:290` (la de `NotificationMessage`, antes `:277`), `:325` y `:373`.
   - **Coste:** en esta rama `CheckLogLevel` (`scripts_client/3_Game/BattleRoyaleUtils.c:91`) aún no está cacheado y
     cuesta hasta 5 `IsCLIParam` por llamada. Tras integrar con client-menu queda cacheado.

Verificado en esta ronda:
- **D1-D3:** un port en Python de `UpdateMatchPhase` y de la condición del aviso pasa los escenarios de la revisión:
  - FINISHED tras el colapso: sin aviso y «PARTIDA TERMINADA».
  - Lobby, selección de spawn, Prepare, STARTING y RESTARTING fuera de una zona conocida: sin aviso.
  - `LAST_ROUND` recién conectado con zona vacía: aviso y «SIN ZONA SEGURA».
  - STARTING con -1: «LA PARTIDA EMPIEZA» sin tiempo.
  - STARTING con 0 y cuenta atrás 90: «PRIMERA ZONA EN 01:30».

  Comprueba la lógica, no que el Enforce compile.
- **`ui_reconcile`:** sobre `addons`, 0 FAIL y los mismos 5 WARN de base (`keys` 76, `strRefs` 42); sobre `gui`,
  0 FAIL y 1 WARN.
- **Gate:** `git show main:tools/gate.sh > build/gate.sh` y `bash build/gate.sh .`:

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
scripts_client FAIL errors 11 warnings 3
scripts_server FAIL errors 2 warnings 12
GATE RED: scripts_client: 11 errors > baseline 10
```

  Salida 1 (RED), como anunció el coordinador: la base del gate para `scripts_client` es 10. Leí el `rule_id` de
  los 11 errores en `build/lint/scripts_client.json`: los 11 son `ES-LAYOUT-PATH-PBOPREFIX-MISMATCH` (el gate tampoco
  informa de reglas inesperadas). El de más es `5_Mission\GUI\HUD\BattleRoyaleKillFeed.c` línea 22
  (`killfeed.layout`). Los 2 de `scripts_server` son `ES-OVERRIDE-PARAM-NAME-MISMATCH`, los de base.

Sin verificar en esta ronda:
- El panel de hints en el juego.
- Si el override, declarado en 5_Mission, llega también a `UiHintPanelLoading`, la subclase que declara 3_Game y que
  usan las pantallas de carga (`3_game/dayzgame.c:130`, `:287` y `:841`). El menú de pausa sí crea `UiHintPanel`
  desde 5_Mission (`5_mission/gui/ingamemenu.c:49`).
- El contenido de `hints.json` (teclas K, Z y P de Expansion), que no es de esta lane.

## Localización
- Hay 29 claves nuevas en `languagecore/stringtable.csv:49-77` (28 en la entrega inicial; la ronda 1 quita
  `STR_BR_KILLFEED_ZONE` y añade `STR_BR_PHASE_STARTING` y `STR_BR_KILLFEED_DIED`), en el mismo formato: 15 columnas entre comillas, CRLF
  y sin salto final.
- Inglés y español están traducidos. `original` y las otras 12 columnas copian el inglés (pendiente de traducción).
- Comprobación de lectura tras escribir: 75 filas, todas de 15 columnas.

## Lint
`bash tools/lint.sh . build/lint` desde la raíz del worktree:

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
scripts_client FAIL errors 11 warnings 3
scripts_server FAIL errors 2 warnings 12
```

Comparé los mensajes de `build/lint/*.json` con los de una pasada de base hecha en esta misma rama antes de tocar
nada (`build/lint-base/`):
- `scripts_client`: los 11 errores contienen el mensaje del falso positivo de ruta de layout
  (`ES-LAYOUT-PATH-PBOPREFIX-MISMATCH`). Los layouts viven en el PBO `gui`, con prefijo `Vigrid-BattleRoyale\GUI`.
  Frente a los 10 de base:
  - Cuatro no cambian y cinco solo cambian de número de línea.
  - El de `win_screen.layout` pasa de `BattleRoyaleClient.c:197` a `MissionGameplay.c:196`.
  - Hay uno nuevo por `Vigrid-BattleRoyale/GUI/layouts/hud/killfeed.layout` (`BattleRoyaleKillFeed.c:22`). Es el mismo
    falso positivo, como permite el brief.
  - No aparece ningún error de otro tipo. Los 3 WARN son los de base: el de `MissionGameplay.c` pasa de la línea 84 a
    la 92.
- `scripts_server`: errores y warnings idénticos a la base, mensaje a mensaje.
- `extra_*`: 0 errores y 0 warnings, como en la base.

`ui_reconcile.py`:
- Sobre `BattleRoyale/addons/gui`: `0 FAIL, 1 WARN` (`UI-STR-NO-STRINGTABLE`: el PBO `gui` no lleva stringtable; las
  claves están en `languagecore`).
- Sobre `BattleRoyale/addons` entero: `0 FAIL, 5 WARN`, los mismos 5 de la base: `TagFrame` y
  `#STR_MP_SESSION_LOCKED`, `#str_..._xbox0` y `#str_modded_version_warning0`, que son vanilla, y `ConfirmButton`
  (UI-23). Recuento: `findRefs` 55→63, `keys` 47→75, `strRefs` 12→41. Todos los `FindAnyWidget` y `#STR` nuevos
  resuelven.

## CROSS-LANE REQUESTS
1. **Lanes del servidor (server-states / server-security):** el cliente usa estas señales. Conviene que no cambien sin
   avisar:
   - En `LockNewZone`, la zona actual y la futura llegan iguales (`6_BattleRoyaleRound.c:395-397`): así se detecta el
     bloqueo.
   - `LockFinalZone` envía la zona actual `"0 0 0"`/0 (`7_BattleRoyaleLastRound.c:200`): así se detecta el colapso.
   - `SetCountdownSeconds` cuenta hasta el bloqueo y después hasta el fin de ronda (`6_BattleRoyaleRound.c:136` y
     `:399`).
2. **Lanes del servidor:**
   - Al reconectar no se reenvía `SetCountdownSeconds` (NET-21). En ROUND, el HUD cae al temporizador de
     `SetMatchPhase`. Si se reenvía, el texto de la fase vuelve a ser exacto.
   - El cliente no sabe si `enable_zone_damage` está desactivado. Si algún servidor lo apaga, haría falta un dato más
     en una RPC futura, porque el contrato 1.0 no lo tiene.
3. **Quien gestione la base del lint:** el falso positivo del `win_screen` cambia de fichero
   (`BattleRoyaleClient.c:197` → `MissionGameplay.c:196`) y hay uno más (`BattleRoyaleKillFeed.c:22`).
4. **Localización:** las 29 claves nuevas tienen el texto inglés en checo, alemán, ruso, polaco, húngaro, italiano,
   francés, chino, japonés, portugués y chino simplificado.

5. **Dueño del PBO `data` (ronda 1):** `data/hints.json` se carga ahora en el menú de pausa. Sus tres textos
   hablan de las teclas K, Z y P de Expansion. Hay que revisar si se quieren hints propios del BR (F1 listo, F2
   desatascarse, zona), y en qué idioma: el JSON no se localiza.
6. **Lanes del servidor (ronda 1):** el aviso de daño y la etiqueta de ronda dependen de que se envíe
   `SetMatchPhase` en ROUND y LAST_ROUND (contrato 1.0). Sin esa RPC, el aviso no sale.

## LO QUE NO PUDE VERIFICAR
- Nada se probó en el juego: compilación real del motor, aspecto de los paneles nuevos (posición, tamaño de letra,
  solape con la brújula o los grupos de Expansion), FPS antes y después, ni el heatmap y el círculo del mapa.
- El linter offline no es el compilador. No comprueba, por ejemplo, el `switch` sobre un `int` con casos `EBRMatchPhase.*`
  ni la conversión de `Math.Ceil/Clamp` (float) a `int`, que sí tienen precedentes en vanilla
  (`3_game/systems/animalcatching/catchingcontextpoissonbase.c:20-23`).
- La integración con las RPC reales del servidor: las lanes del servidor no tenían commit al hacer esto. Se revisó su
  brief, no su código.
- Que un `SetMatchPhase` o un `KillFeedEvent` no pueda llegar antes de `MissionGameplay.OnInit`, que es donde se
  registran los handlers y se hace `Reset()`. Por el flujo de conexión se supone que no, pero no se ha medido.
- Las traducciones distintas de inglés y español.

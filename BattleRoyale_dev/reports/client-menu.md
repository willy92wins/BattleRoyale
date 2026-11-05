# Informe del carril client-menu (BattleRoyale 1.0)

Rama `lane/client-menu`, worktree `C:\Users\guill\Repos\br-wt\client-menu`. Hay dos commits: el inicial (`160a509`) y el de la ronda 1 de correcciones, que trae esta versión del informe; su sha está en el mensaje de entrega (`git log -1 lane/client-menu`).

Resumen del estado tras la ronda 1:
- El menú principal ya no hace ninguna llamada HTTP. `-connect` vuelve a funcionar al arrancar el juego y el botón de jugar abre siempre el navegador de servidores.
- El carrusel de noticias de 1.30 nace dentro de un frame oculto del layout del BR.
- El nivel de log se resuelve una sola vez.
- Las URLs del proyecto original ya no son destino por defecto y la versión pasa a 1.0.0.
- Chernarus tiene su entrada de pantallas de carga, y la pantalla de carga ya no deja un NULL pointer al arrancar el cliente.
- El gate da GATE GREEN.
- No se ha compilado con el motor ni probado en el juego.

Rutas relativas a `BattleRoyale/addons/` salvo que se diga otra cosa. Vanilla = `C:\Users\guill\OneDrive\Documentos\DayZ Projects\scripts\` (1.30).

## Ronda 1 de correcciones (revisión de Codex)

### D1 (MAJOR): Play abre siempre el navegador
- **Problema.** Con `-connect` en la línea de comandos, `Play()` reconectaba siempre al destino del CLI. Como «Choose server» sigue oculto (`MainMenu.c:152`), el navegador quedaba inaccesible.
- **Arreglo.** `scripts_client/5_Mission/Mission/MainMenu.c:171-175`: `Play()` solo llama a `OpenMenuServerBrowser()` (vanilla `mainmenu.c:465-468`). Sale el código de `-connect`, `-port` y `-password` de ese método. Elegí la opción más simple: sin estado ni condición de «primera vez».
- **Por qué no se pierde la conexión por CLI.** La conexión inicial ya la hace vanilla una sola vez por sesión del juego. `OnInitialize` solo entra con `GetLoadState() == DayZLoadState.UNDEFINED` y llama a `ConnectLaunch` si hay `-connect` (`dayzgame.c:2120-2133`); en PC, `ConnectLaunch` llama a `ConnectFromCLI` (`:2281-2299`). Esos dos métodos vanilla vuelven a correr porque el BR ya no los sobrescribe (commit `160a509`). Tras desconectarse, el jugador vuelve al servidor del CLI desde el navegador (historial o conexión directa).
- **Verificación.** Diff, y grep de `GetCLIParam|ConnectFromServerBrowser` en `MainMenu.c`: 0 líneas. `check_overrides.py` sobre `scripts_client`: mismos 73 overrides, 29 MISSING y 2 OBSOLETE que tras el commit inicial, idénticos quitando los números de línea.
- **Estado.** Confirmado en el código. En el juego es plausible.

### Bug del arranque: NULL pointer en `LoadingScreen.UpdateLoadingBackground`
- **Causa.** `LoadingScreen` se construye dentro del constructor de `DayZGame` (`dayzgame.c:1062-1070`), y ahí mismo se llama a `ShowEx` → `Show` (`:845`). En ese momento `g_Game` todavía es NULL, porque se asigna al volver de `new DayZGame` (`3_game/game.c:7`). Por eso `GetGame().GetWorldName()` (antes en `LoadingScreen.c:56`) fallaba.
  - Ojo: tampoco vale `m_DayZGame.GetWorldName()`, porque esa sobrecarga usa `g_Game` por dentro (`global/game.c:934-937`).
  - Las dos líneas previas (`RESOURCES (E): Bad texture name ''` y `ImageWidget::AlphaMaskTexture can't load ''`) no venían de una lista vacía. Las causaba `m_ImageBackground.LoadMaskTexture("")`, la forma en que el autor quita la máscara de revelado de `gui/layouts/loading.layout:52-54`, que se ejecutaba en cada `Show`.
- **Arreglo** (`scripts_client/3_Game/Loading/LoadingScreen.c`):
  - `:35-37`: sale sin hacer nada si `m_DayZGame` o `m_ImageBackground` son NULL.
  - `:63-64`: el nombre del mundo se lee con la sobrecarga proto sobre la instancia, `m_DayZGame.GetWorldName(out)` (`global/game.c:932`), como hace vanilla en `missionbase.c:110`. `GetTickTime` ya se llamaba sobre `m_DayZGame` (`game.c:914`).
  - `:68-89`: el bucle solo corre si `m_Backgrounds` existe.
  - `:97-105`: solo se pide `GetRandomPath()` si la entrada tiene rutas. `ExpansionArray` con lista vacía indexaría `m_Indexes[-1]` (`br-ref/exp_core_scripts/3_Game/DayZExpansion_Core/ExpansionArray.c:70-89`). Si no hay lista, la lista está vacía o la ruta sale vacía, carga `br_loading_1.edds`.
  - `:18-20`: el `LoadMaskTexture("")` pasa del `Show` al constructor, con guarda, y se ejecuta una sola vez.
    - Vanilla nunca recarga la máscara: el único `LoadMaskTexture` del árbol vanilla es el proto (`1_core/proto/enwidgets.c:282`), y `Show` solo toca el progreso (`dayzgame.c:854`).
    - Las dos líneas de RESOURCES pasan a salir una vez por arranque y no en cada carga.
    - No las quité del todo: dejar la máscara devolvería el revelado vanilla sobre los fondos del BR, y eso no puedo evaluarlo sin el juego.
- **¿Cambia la rama `chernarus` → `chernarusplus` al arrancar?** No. Modelé en Python la selección nueva leyendo el JSON real, la de HEAD y la de la base (`344d930`):
  - Con nombre de mundo vacío (lo esperable dentro del constructor, sin mundo cargado), la base y ahora caen en `default`: las 12 imágenes.
  - Solo cambia cuando el mundo ya se llama `ChernarusPlus`: antes `default` (12) y ahora `chernarusplus` (las 6 que el autor asignó a Chernarus).
  - `namalsk` y `sakhal` no cambian, y `Enoch` cae en `default`.
  - Con la lista vacía o nula, o una entrada sin rutas, carga `br_loading_1.edds`.
- **Estado.** Confirmado en el código: el uso de `g_Game` a través de `GetGame()` ya no está en este fichero (grep 0) y las guardas cubren los caminos de arriba. Que el error desaparezca del RPT y qué devuelve `GetWorldName` dentro del constructor es plausible, no comprobado en el motor.

### Punto 3: logs que se construyen antes del filtro
No son de este carril y no los he tocado: están en CROSS-LANE, punto 7.

### Gate de la ronda 1
`tools/gate.sh` solo existe en `main` (commit `b18c5ec`), y no puedo hacer merge ni rebase. Lo ejecuté desde una copia byte a byte: `git show main:tools/gate.sh > build/gate.sh` (mismo sha1 `51d27e9f…` que en `main`). Como `tools/lint.sh` es idéntico en `main` y en esta rama, es lo mismo que `bash tools/gate.sh .` después del merge.

Salida de `bash build/gate.sh .` desde la raíz del worktree:

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
GATE GREEN
```

Código de salida 0. Las reglas de error son solo `ES-LAYOUT-PATH-PBOPREFIX-MISMATCH` (cliente) y `ES-OVERRIDE-PARAM-NAME-MISMATCH` (servidor). Quitando los números de línea, los mensajes de errores y avisos de `scripts_client` y `scripts_server` son idénticos a la pasada de base hecha antes del primer commit (`build/lint-base`): 0 añadidos y 0 quitados.

## ZON-01 / NET-05 / UI-30: fuera el matchmaking, vuelve `-connect`

Qué cambió:
- `scripts_client/5_Mission/Mission/MainMenu.c`:
  - Fuera `IsMatchMakingAvailable` (hacía `GET_now`), `StartMatchMaking` (hacía `POST_now` con la lista de mods y se reprogramaba con `CallLaterByName`), el `Update` que pintaba su estado, `ColorDisable` y los miembros `modList`, `b_MatchMakingAvailable`, `m_MatchMakingState` y `m_MatchMakingTryCount`.
  - `Init` deja la etiqueta en `#main_menu_play` (`:144`).
  - `Play()` (`:171-175`) abre siempre el navegador de servidores (ronda 1, D1).
  - Se quita el override vacío de `OpenMenuServerBrowser`, así que vuelve el vanilla (`mainmenu.c:465-468`).
  - `OnClick` (`:273-278`) manda el botón a `Play()`, como vanilla (`mainmenu.c:235-240`).
  - El botón «choose server» sigue oculto (`BR_OverrideUI`, `:152`): el menú conserva el aspecto del BR.
- `scripts_client/3_Game/DayZGame.c`: quitados los overrides de `ConnectFromCLI` (vacío) y `ConnectLaunch` (mandaba al menú). Vuelven a correr los vanilla, que es lo mismo que llamar a `super`: `OnInitialize` → `ConnectLaunch` con `-connect` (`dayzgame.c:2131-2133`), `ConnectLaunch` → `ConnectFromCLI` en PC (`:2281-2299`) y `ConnectFromCLI` → `Connect()` (`:2827-2840`).
- `scripts_client/3_Game/BattleRoyale/Webhook/MatchMakingWebhook.c`: borrado. Antes de borrarlo, el grep de `MatchMakingWebhook|MM_PlayerInfo|isMatchMakingAvailable|searchGame` en todo `BattleRoyale/` solo encontró el propio fichero.

Cómo lo verifiqué:
- grep de `GET_now|POST_now|GetRestContext|GetRestApi|RestCallback|\.GET(|\.POST(` en `scripts_client/`: 0 líneas. Control positivo: el mismo patrón da 35 líneas en `scripts_server/`, y en HEAD `MainMenu.c` tenía 2 referencias a `MatchMakingWebhook` y el webhook 2 llamadas `GET_now`/`POST_now`.
- Overrides resueltos contra vanilla con `C:\temp\lfq130\tools\check_overrides.py`: mismos 29 MISSING (todos overrides de clases de COT, Expansion y VPP que la herramienta no ve) y 2 OBSOLETE (`FilterDlcs`, `ShowDlcFrame`) que en HEAD; pasa de 77 a 73 overrides comprobados por los cuatro quitados.

Estado: confirmado en el código. En el juego es plausible, no comprobado.

No verificado: el flujo real con el launcher o Steam (`-connect`) y que el navegador se abra desde este menú.

## UI-01: carrusel de noticias de 1.30

Qué cambió:
- `gui/layouts/MainMenu/main_menu.layout:1196-1211`: `PanelWidgetClass carousel_Frame`, hijo directo de `main_menu_root`. Es copia del vanilla (`DayZ Projects\gui\layouts\new_ui\main_menu.layout:1235-1250`) con una sola diferencia: `visible 0`.
- `scripts_client/5_Mission/Mission/MainMenu.c:32-33`: `m_NewsCarouselFrame = layoutRoot.FindAnyWidget("carousel_Frame");`. El miembro está fuera del `#ifdef` en vanilla (`mainmenu.c:50`), así que compila también en Experimental.

Por qué basta:
- `RefreshNewsCarousel` (`mainmenu.c:148-157`) se llama desde `OnShow` (`:412-414`) y desde `CheckWidth` (`:755-759`), y crea el carrusel con `CreateWidgets(..., parent)` (`newscarousel.c:49`). Ahora el padre es un frame oculto de `layoutRoot`: aunque `UpdateCarouselInfo` haga `m_Root.Show(true)` (`newscarousel.c:211`), no se pinta y muere con el menú.
- Lo único que muestra ese frame es `ShowNewsCarousel(true)` (`mainmenu.c:160-165`). Vanilla solo lo llama desde su `Init` (`:126`), que no corre porque el `Init` del BR no llama a `super`, y desde el invoker `GetOnModMenuVisibilityChanged` (`:128`), donde tampoco se registra. El grep de `ShowNewsCarousel|m_NewsCarousel|carousel` en `BattleRoyale/addons` y en `br-ref` no da más llamadas.
- No sobrescribo `RefreshNewsCarousel`: vive bajo `#ifdef ENABLE_CAROUSEL` (`mainmenu.c:1-3`, `:147`).

Cómo lo verifiqué:
- Comparé el bloque línea a línea con el vanilla: solo cambia `visible`.
- Llaves balanceadas: 115/115.
- Script Python: los 28 nombres de `FindAnyWidget` de `MainMenu.c` existen en `main_menu.layout` o `popup_message.layout`, y `carousel_Frame` se declara una sola vez en todos los layouts del BR.

Estado: confirmado en el código y el layout. Que no se vea y que no quede huérfano es plausible: no lo he visto en el juego.

## Rendimiento #7 / NET-09: nivel de log calculado una vez

Qué cambió (`scripts_client/3_Game/BattleRoyaleUtils.c`):
- `:10-14`: `static const int LOG_DISABLED = -1` y los miembros `protected static bool m_LogLevelResolved` y `protected static int m_LogLevel`.
- `:97-110`: `CheckLogLevel` resuelve el umbral en la primera llamada y después solo compara.
- `:112-139`: `ResolveLogLevel` conserva el orden de antes: `-br-trace`, `-br-debug`, `-br-info`, `-br-warn`, `-br-none`, `BRLogLevel` del servidor (`> 0` umbral, `< 0` apagado) y `BATTLEROYALE_LOG_LEVEL`.
- `IsCLIParam` y `ServerConfigGetInt` se leen una vez por proceso, no en cada llamada de log. Firmas: `1_core/proto/ensystem.c:141` y `3_game/global/game.c:1139`.

Cómo lo verifiqué:
- Modelé en Python la función de HEAD y la nueva. Combinaciones: los 5 parámetros de CLI, servidor sí/no, `BRLogLevel` ∈ {-7,-1,0,1,2,3,4,9}, nivel por defecto 0/4 y nivel pedido de -3 a 7. Resultado: 11.264 casos y 0 diferencias.
- Control positivo: un mutante sin la comprobación de `LOG_DISABLED` da 108 diferencias.
- El script está en `build/verify/loglevel_equiv.py`, sin versionar (`/build/` está en `.gitignore`).
- Único caller: `LogServerMessage` (grep de `CheckLogLevel` en `BattleRoyale/addons`).
- En el cliente la resolución solo lee parámetros de CLI, sin `GetGame()`. Es seguro aunque el primer log llegue dentro del constructor de `DayZGame` (el `Trace` del constructor de `LoadingScreen`).

Estado: confirmado como transformación lógica. En el motor es plausible.

No verificado: que `ServerConfigGetInt("BRLogLevel")` ya devuelva el valor de serverDZ.cfg en la primera llamada de log del servidor. El caché congela esa primera lectura. El ahorro no está medido.

## UI-31 / EXT-24: URLs del proyecto original, créditos y S1Q

Qué cambió:
- `scripts_client/2_GameLib/BattleRoyaleConstants.c`:
  - `:23-24`: `GITHUB_URL` (LeMyst) pasa a `BATTLEROYALE_FEEDBACK_URL = ""`. Solo la usaba `extra_changefeedbackurl`.
  - `:27-28`: `BATTLEROYALE_API_ENDPOINT = ""`; antes `https://api.vigrid.ovh/`. El nombre se mantiene porque lo leen 8 webhooks del servidor. Los 7 que se instancian lo hacen dentro de `if (m_ServerData.enable_vigrid_api)`, que viene apagado (`scripts_server/3_Game/Config/BattleRoyaleServerData.c:7`); `PlayersWebhook` no se instancia en ningún sitio (comprobado por cruce de llaves en cada `new …Webhook(`).
  - `:8`: `BATTLEROYALE_VERSION = "1.0.0"`. Se ve en el menú principal y en el de pausa (`DayZGame.GetBattleRoyaleClientVersion`, `scripts_client/3_Game/DayZGame.c:3-6`). El otro lector, `ServerData.IsMatchingVersion` (`scripts_server/4_World/Net/Structures/DatabaseStructs.c:35-41`), no tiene llamadas.
- `extra_changefeedbackurl/Scripts/5_Mission/GUI/InGameMenu.c`:
  - `:3-12`: `Init` oculta `m_FeedbackButton` si la constante está vacía (miembro vanilla `5_mission/gui/ingamemenu.c:18`, `:57`; botón `feedbackbtn` en `gui/layouts/day_z_ingamemenu.layout:419`).
  - `:14-23`: `OpenFeedback` abre la URL configurada; con la constante vacía cae al vanilla (`ingamemenu.c:422-425`).
- Mismo patrón en `extra_changefeedbackurl/.../NewUI/MainMenu/MainMenu.c:4-13` y `MainMenuNewsFeed.c:3-12`. Ahí no hay botón que ocultar: el layout del BR no tiene `feedback_button` y nadie instancia `MainMenuNewsfeed` (grep en vanilla, BR y `br-ref`). Firmas: `mainmenu.c:560` y `mainmenunewsfeed.c:74`.
- `data/credits.json:6-11`: nueva sección «1.0 refurbish» con «DayZ BR community», la primera del departamento. Se conservan, sin cambios, el resto de secciones y el nombre del departamento: Myst, Kegan Hollern (Lystic), desarrolladores, colaboradores, música y testers (comparado como JSON).
- S1Q fuera de las frases del menú (`MainMenu.c:92-108`) y de `data/LoadingMessages.json`.

Cómo lo verifiqué:
- grep de `GITHUB_URL|LeMyst|S1Q` en `BattleRoyale/`: 0 líneas.
- grep de `vigrid\.ovh`: solo `scripts_server/3_Game/Config/BattleRoyaleServerData.c:29` (`autolock_url`, de otro carril). Ese hit sirve de control positivo del propio grep.
- `check_overrides.py` sobre `extra_changefeedbackurl`: 4 overrides y 0 MISSING. Control: un mutante con `InitX` sale MISSING.
- JSON parseados con Python antes y después.

Estado: confirmado en el código. Que el botón desaparezca del menú de pausa y la lista se recoloque es plausible: no lo he visto en el juego.

Decisiones:
- Con la URL vacía, los `OpenFeedback` de la extra caen al vanilla. Solo se oculta el botón del menú de pausa, que es el único que existe en la UI del BR.
- No toqué los prefijos `Vigrid-BattleRoyale` de los PBO ni la carpeta `$profile:Vigrid-BattleRoyale\` (`BattleRoyaleConstants.c:32`). Cambiarlos rompe a todos los carriles y a los perfiles de servidor existentes.
- Tampoco toqué los nombres `vigrid_change_feedback_url` de `extra_changefeedbackurl/config.cpp`.

## mod.cpp

- `BattleRoyale/mod.cpp:8`: `author = "Myst, KeganH, DayZ BR community, check credits"`.
- `BattleRoyale/mod.cpp:9`: `version = "1.0.0"`.
- La columna de comentarios queda alineada como antes.

Estado: confirmado (diff).

## Pantallas de carga y hints de Chernarus

- `data/LoadingScreens.json:20`: la entrada `chernarus` pasa a `chernarusplus`.
  - `scripts_client/3_Game/Loading/LoadingScreen.c:63-65` y `:75-77` comparan el nombre del mundo en minúsculas con `MapName` en minúsculas. Chernarus es `chernarusplus`: vanilla `5_mission/mission/missionbase.c:110-116` lo compara igual.
  - Antes nunca casaba y caía en `default`.
  - Al arrancar el cliente no cambia de rama: ver la ronda 1.
  - Las rutas (`br_loading_1..6.edds`) no cambian. Las 12 existen en `C:\Users\guill\Repos\BattleRoyale\BattleRoyale\addons\gui\textures\loading_screens\` (Glob).
- `data/hints.json:2-16`: tres hints del BR, todos con `m_ImagePath` vacío como en vanilla:
  - F1 para marcarse listo en el lobby.
  - F2 para el unstuck al empezar la partida: solo funciona en el estado StartMatch (`scripts_server/5_Mission/BattleRoyale/Server/BattleRoyaleServer.c:483-493`).
  - Daño fuera de la zona.
  - Las teclas son las de `data/Inputs.xml:23-28`.
  - Se mantienen los tres de Expansion, cuyas teclas sí son las suyas: K en `navigation_scripts/Data/Inputs.xml:56-57`, P y Z en `expansion_scripts/Data/Inputs.xml:16-20`.
  - hints.json no tiene entradas por mapa.
- Hoy ningún script lee hints.json (el override de `UiHintPanel` está comentado; el coordinador lo ha asignado a client-hud como D2) ni `LoadingMessages.json`. Ver cross-lane 3 y 6.

Estado: confirmado en los datos. En la pantalla de carga es plausible.

No verificado: no abrí las imágenes EDDS; conservé el reparto del autor (1-6 para Chernarus).

## Lint (commit inicial)

`bash tools/lint.sh . build/lint` dio exactamente la base de `common.md`:
- `scripts_client` FAIL con 10 errores y 3 avisos.
- `scripts_server` FAIL con 2 errores y 12 avisos.
- Cada `extra_*` PASS con 0.

Además comparé el contenido con una pasada de base sobre este mismo worktree antes de tocarlo (`build/lint-base`). Quitando los números de línea, los mensajes de errores y avisos son idénticos: 0 añadidos y 0 quitados. El gate de la ronda 1 está arriba.

Límite del gate, medido: el lint no detecta un identificador sin definir ni un `override` de un método que no existe. Una copia de `extra_changefeedbackurl` con `BATTLEROYALE_FEEDBACK_URL_UNDEFINED`, y otra con `InitX()` y `OpenFeedback(int)`, dieron las dos PASS con 0 errores. Por eso las firmas están verificadas a mano contra vanilla (citas arriba) y los nombres de override con `check_overrides.py`.

## CROSS-LANE REQUESTS

1. **Servidor, webhooks (ZON-02).** `BATTLEROYALE_API_ENDPOINT` ahora vale `""`. Los 8 webhooks que lo leen deberían no enviar nada con el endpoint vacío, aunque alguien active `enable_vigrid_api`, o retirarse con ZON-02:
   - `scripts_server/3_Game/BattleRoyale/Webhook/EventWebhook.c:40`
   - `Players/WinWebhook.c:38`
   - `Matches/CreateMatchWebhook.c:41`
   - `Matches/PartiesWebhook.c:39`
   - `Matches/PlayersWebhook.c:39`
   - `Matches/ScoreWebhook.c:38`
   - `Matches/StartMatchWebhook.c:38`
   - `Servers/LockServerWebhook.c:44`
2. **Servidor (ZON-03).** `scripts_server/3_Game/Config/BattleRoyaleServerData.c:29`: `autolock_url` sigue apuntando a `https://api.vigrid.ovh/`.
3. **Hints (UI-27, D2 de Codex, asignado a client-hud).** Hay que reactivar el override de `scripts_client/5_Mission/GUI/Hints/UiHintPanel.c:1-8` para que se lea `Vigrid-BattleRoyale/Data/hints.json`, con la firma de 1.30. Vanilla: `3_game/gui/hints/uihintpanel.c:85-90`, y `m_DataPath` es `const` en `:14`, así que no se puede reasignar:

   ```
   override protected void LoadContentList()
   {
       string errorMessage;
       if (!JsonFileLoader<array<ref HintPage>>.LoadFile(path, m_ContentList, errorMessage))
           ErrorEx(errorMessage);
   }
   ```
4. **Créditos (UI-31).** `scripts_client/config.cpp:17`: `credits="Kegan - Modified by Myst"`. Conviene añadir la comunidad, por ejemplo «Kegan - Modified by Myst - 1.0 by DayZ BR community».
5. **Limpieza opcional.**
   - `scripts_client/2_GameLib/BattleRoyaleEnums.c:20`: el enum `BattleRoyaleMatchMakingState` ya no lo usa nadie.
   - `languagecore/stringtable.csv:6-9`: tampoco las filas `STR_BR_MM_MATCHMAKING`, `STR_BR_MM_LOGIN`, `STR_BR_MM_SEARCHING` y `STR_BR_MM_CONNECTING`.
6. **LoadingMessages.json (UI-27).** Ningún script lo carga; solo existe la constante `DAYZBR_LOADING_MESSAGES_PATH` (`BattleRoyaleConstants.c:79`). Hay que cablearlo en `scripts_client/3_Game/Loading/` (ahora de este carril, si se decide hacerlo) o borrarlo.
7. **Logs construidos antes del filtro (punto 3 de la ronda 1).** El mensaje se arma con `string.Format` antes de que `Debug`/`Trace` lo descarten:
   - `scripts_client/5_Mission/GUI/HUD/BattleRoyaleHud.c:122` (`SetDistance`, cada frame fuera de zona; client-hud).
   - `scripts_client/3_Game/BattleRoyale/RPC/BattleRoyaleRPC.c:277` (`NotificationMessage`, por RPC; client-network).
   - Arreglo propuesto: envolverlos en `if (BattleRoyaleUtils.CheckLogLevel(BattleRoyaleUtils.DEBUG))`, o `TRACE` en el segundo. Desde este carril, `CheckLogLevel` cuesta una comparación tras la primera llamada.

Nota de operación, no de código: el logo y el newsfeed que Expansion pinta en el menú de pausa se gobiernan con sus ajustes de servidor `InGameMenuLogoPath` y `UseNewsFeedInGameMenu` (`br-ref/expansion_scripts/5_Mission/DayZExpansion/GUI/InGameMenu.c:149-163`).

## LO QUE NO PUDE VERIFICAR

- **Compilación con el motor.** No lancé DayZ, DayZServer ni DayZDiag (regla del carril), y el lint no es un compilador: lo probé arriba con dos mutantes.
- **Nada probado en el juego:**
  - que el carrusel no se vea y no quede huérfano;
  - que Play abra el navegador;
  - el arranque directo con `-connect` desde el launcher o Steam;
  - el botón de feedback oculto y cómo se recoloca la lista del menú de pausa;
  - la pantalla de carga de Chernarus.
- **Pantalla de carga al arrancar.**
  - Que el NULL pointer y las dos líneas de RESOURCES desaparezcan del RPT: las de RESOURCES deberían salir una vez por arranque.
  - Qué devuelve `GetWorldName` dentro del constructor de `DayZGame`.
  - Que no aparezca un parpadeo o un fondo distinto con la máscara quitada en el constructor y no en `Show`.
- **Primera lectura de `BRLogLevel`.** Que en la primera llamada de log del servidor ya refleje serverDZ.cfg.
- **Imágenes `br_loading_1..6.edds`.** No las abrí.
- **`creditsJson` del CfgMods.** No sé cómo lo usa el motor: el `CreditsLoader` vanilla solo lee `scripts/data/credits.json` (`3_game/gui/credits/creditsloader.c:3`). Mantuve el formato de `JsonDataCredits`, `JsonDataCreditsDepartment` y `JsonDataCreditsSection` (`3_game/gui/credits/jsondatacredits*.c`).
- **hints.json y LoadingMessages.json.** No tienen efecto hasta que se resuelvan los cross-lane 3 y 6.

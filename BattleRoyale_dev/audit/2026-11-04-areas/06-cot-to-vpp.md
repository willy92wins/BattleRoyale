# 06 — Migración admin COT → VPP Admin Tools (BattleRoyale v0.1.0-Vigrid)

Análisis de solo lectura. No se ha modificado ni compilado nada.

Abreviaturas de ruta:
- `BR\` = `C:\Users\guill\OneDrive\Documentos\DayZ Projects\BattleRoyale_dev\extracted_v0.1.0-vigrid\`
- `REF\` = `C:\Users\guill\AppData\Local\Temp\claude\72e0954b065197d7\151ed9c3-2d0a-403f-9ce8-abb72cfff50c\scratchpad\ref\`
- `VPP\` = `REF\vpp\VPPAdminTools\`

## 0. Conclusión

1. **Agujero de seguridad, ya en v0.1.0, por las dos rutas.** Ningún handler de servidor del BR comprueba permisos:
   - Con VPP cargado, cualquier cliente conectado puede mandar `RPC-DayZBR-Server::NextState` y saltarse el estado actual. El handler `BattleRoyaleServer.NextState` (`BR\scripts_server\5_Mission\BattleRoyale\Server\BattleRoyaleServer.c:536-551`) no mira `sender` ni permisos. El cliente lo envía por `GetRPCManager().SendRPC` de CF (`BR\scripts_client\5_Mission\VPPAdminTools\MenuBattleRoyaleManager.c:48`), y esa ruta no pide contraseña: `REF\cf_scripts\3_Game\CommunityFramework\RPC\RPCManager.c:51-108` despacha a `m_RPCActions` sin más control que `sender != NULL`. Además, ese mapa es el mismo que usa el canal con contraseña de VPP (`VPP\3_Game\CommunityFramework\RPC\RPCManager.c:74-95`), así que ni siquiera `VSendRPC` protegería: el permiso tiene que comprobarse dentro del handler.
   - Con COT, el override de servidor `BRMasterControlsModule.OnRPC` (`BR\scripts_server\5_Mission\COT\GUI\BRMasterControlsModule.c:6-26`) ejecuta Next, Pause, Resume y Airdrop sin `JMPermissions.HasRPC`. El permiso `BattleRoyale.StateMachine.Update` se registra (`BR\scripts_client\5_Mission\COT\GUI\BRMasterControlsModule.c:12`) pero nadie lo comprueba. COT no filtra por permiso antes de llamar a `OnRPC`: su base solo aplica rate-limit, y solo si el módulo llama a `super` (`REF\cot\scripts\4_World\CommunityOnlineTools\Classes\Module\JMModuleBase.c:209-221`), cosa que el BR no hace. El gate lo trae el patrón `JMModuleAction`/`ExecAction` (`:166-175`), que el BR no usa.
   - **Esto debe arreglarse ya en v1.0, sea cual sea el admin.**
2. **La integración VPP existente es un esqueleto.** Tiene un submenú con un único botón (Next State), un permiso de menú y el handler sin gate. Le faltan Pause, Resume y Airdrop, permisos por acción, feedback al admin, log y `requiredAddons`. Contra la VPP actual parece compilar: todas las clases y miembros que usa existen con firma compatible (§2). No se ha compilado.
3. **Recomendación para v1.1: cortar COT, quedarse con VPP y añadir una capa neutral** (`BRAdminController` en servidor + autorizador enchufable). Mantener las dos integraciones con `#ifdef` es barato si la capa neutral existe; sin ella, duplica handlers. La capa neutral y el gate de permisos conviene hacerlos ya en v1.0 (§5).

## 1. Inventario: qué ofrece hoy el módulo COT del BR

Registro en COT: `modded class JMModuleConstructor` añade `BRMasterControlsModule` (`BR\scripts_client\5_Mission\COT\JMModuleConstructor.c:1-11`). Todo está bajo `#ifdef JM_COT`, que se define en `REF\cot\scripts\config.cpp:41`.

Módulo cliente (`BR\scripts_client\5_Mission\COT\GUI\BRMasterControlsModule.c`):
- Permisos JM: `BattleRoyale.StateMachine.View` y `BattleRoyale.StateMachine.Update`, registrados en `:11-12`. `HasAccess()` solo comprueba View (`:15-18`), y solo en el cliente, para pintar la ventana.
- Layout `Vigrid-BattleRoyale/GUI/layouts/COT/master_controls.layout` (`:22`; el fichero es `BR\gui\layouts\COT\master_controls.layout`, con scriptclass `BRMasterControlsForm` en `:9`). Título "[BR] State Machine" e icono de texto "BR" (`:25-38`).
- Rango de RPC: `BattleRoyaleCOTStateMachineRPC.INVALID..COUNT` (`:45-53`), es decir 11000-11009 (`BR\scripts_client\2_GameLib\BattleRoyaleEnums.c:2-16`; el `#ifdef JM_COT` está comentado, así que el enum existe siempre).

| Botón (Form) | Visible si | Método cliente → RPC | Handler de servidor | Estado que toca |
|---|---|---|---|---|
| Next State (`BRMasterControlsForm.c:22`) | siempre | `StateMachine_Next` → `rpc.Send(NULL, Next=11001)` (`Module.c:57-61`) | `RPC_Next`→`Server_Next` (`server …Module.c:13-14, 80-92`) | `GetCurrentState().Deactivate()`: para los timers y `b_IsActive=false` (`States\0_BattleRoyaleState.c:55-63`); en el siguiente tick, `Update` transiciona (`BattleRoyaleServer.c:198-224`) |
| Pause (`Form.c:23`) | siempre | `rpc.Send(NULL, Pause=11002)` (`Module.c:63-67`) | `Server_Pause` (`server :66-78`) | `b_IsPaused=true`; `IsComplete()` devuelve false mientras dure (`0_BattleRoyaleState.c:75-92`) |
| Resume (`Form.c:24`) | siempre | `rpc.Send(NULL, Resume=11003)` (`Module.c:69-73`) | `Server_Resume` (`server :52-64`) | `b_IsPaused=false` (`0_BattleRoyaleState.c:80-83`) |
| Spawn Airdrop (`Form.c:28-30`) | `#ifdef EXPANSIONMODMISSIONS` | `rpc.Send(player, SpawnAirdrop=11006)` (`Module.c:87-91`) | `RPC_SpawnAirdrop` (`server :43-49`) | Notificación Expansion a todos + `ExpansionMissionModule.s_Instance.CallAirdrop(senderRPC.GetPlayer().GetPosition())`. **API de Expansion no verificada**: no hay fuentes de Expansion Missions en `REF\`. `GetPlayer()` es null si el admin está muerto o es un espectador, y entonces se lanza una excepción de script |
| Add Player / Add Group (`Form.c:32-37`) | `#ifdef DIAG` | `rpc.Send(player, 11004/11005)` (`Module.c:75-85`) | **ninguno**: el `switch` del servidor no tiene case (`server :11-25`) | nada (código muerto) |
| SpawnHorde / SpawnChemicals (`Form.c:80-88`) | no hay botón | `Module.c:93-103` | **ninguno** | nada (código muerto) |

Notas de COT (solo lo necesario para v1.0, sin auditoría completa de compatibilidad):
- La firma de `OnRPC` del servidor usa `#ifdef CF_BUGFIX_REF`, y CF lo define (`REF\cf_scripts\config.cpp:41`), así que gana la rama sin `ref`, que coincide con `JMModuleBase.OnRPC(PlayerIdentity, Object, int, ParamsReadContext)` (`REF\cot\…\JMModuleBase.c:212`).
- La COT actual ha cambiado de modelo (`JMModuleInfo`, `DescribeModule`, `JMModuleAction`, iconos Lucide), pero conserva los getters legacy que el BR sobrescribe: `GetLayoutRoot`, `GetTitle`, `GetIconName` e `ImageIsIcon` (`JMRenderableModuleBase.c:29,48,78,211`), `GetRPCMin`/`GetRPCMax`/`GetWebhookTitle` (`JMModuleBase.c:38,71,80`), y `GetPermissionsManager()`/`RegisterPermission`/`HasPermission` (`JMPermissionManager.c:940,659,215`). En el form, el `OnInit` legacy sigue llamándose (`JMFormBase.c:350-354`). El `OnShow()` del BR no llama a `super`, así que se pierde `RequestData()` (`JMFormBase.c:388-392`); es inocuo porque el BR no pide datos.
- **`requiredAddons` no lista `JM_COT_Scripts` ni `DZM_VPPAdminTools`** (`BR\scripts_client\config.cpp:5-10`). `modded class JMModuleConstructor` y `modded class VPPAdminHud` dependen de que el orden de carga ponga COT o VPP antes. El orden real sin `requiredAddons` no se ha verificado.

## 2. Estado de la integración VPP ya presente

Ficheros:
- `BR\scripts_client\5_Mission\VPPAdminTools\VPPAdminHud.c:1-13`: `modded class VPPAdminHud`, override de `DefineButtons()` → `InsertButton("MenuBattleRoyaleManager", "BattleRoyale", "set:dayz_gui_vpp image:vpp_icon_obj_editor", …)`. Guardas `#ifdef VPPADMINTOOLS` + `#ifndef SERVER`.
- `BR\scripts_client\5_Mission\VPPAdminTools\MenuBattleRoyaleManager.c:1-52`: `class MenuBattleRoyaleManager: AdminHudSubMenu` con un botón `BtnNextState` → `GetRPCManager().SendRPC(RPC_DAYZBRSERVER_NAMESPACE, "NextState", NULL, true)` (`:46-49`). Mismas guardas.
- `BR\gui\layouts\BattleRoyaleManagerUI\MenuBattleRoyaleManager.layout`: tiene `Header` (`:42`), `BtnClose` (`:85`) y `BtnNextState` (`:29`). Los textos `#STR_BR_COT_NAME` y `#STR_BR_COT_NEXTSTATE` existen (`BR\languagecore\stringtable.csv:2-3`).
- Servidor `BR\scripts_server\5_Mission\BattleRoyale\Server\BattleRoyaleServer.c`: `AddRPC(…,"NextState", this, SingleplayerExecutionType.Server)` bajo `#ifdef VPPADMINTOOLS` (`:21-23`); `GetPermissionManager().AddPermissionType({ "MenuBattleRoyaleManager" })` en `Init()` (`:43-45`); handler `NextState` (`:536-551`).

Comprobación símbolo a símbolo contra la VPP actual (todo abierto con Read o grep sobre `REF\vpp`):

| Símbolo usado por el BR | Existe en VPP | Cita | Observación |
|---|---|---|---|
| define `VPPADMINTOOLS` | sí, como `#define` en un fichero | `VPP\Definitions\VPPAdminTools\Definitions.c:1`; cargado en los 4 módulos (`REF\vpp\assets\config.cpp:69,79,90,100`) | Que un `#define` de fichero se propague a otro mod es lo que la propia VPP documenta (`VPP\4_World\…\ContextMenu\Examples\VPPContextExample.c:8,22`), y exige `requiredAddons` con `DZM_VPPAdminTools`. **El BR no lo pone**: si VPP compila después, la integración desaparece sin error. No verificado in-game |
| CfgPatches `DZM_VPPAdminTools` | sí | `REF\vpp\assets\config.cpp:3` | el `config.cpp` de `VPPAdminTools\` es solo `DZM_VPPAdminToolsScripts` (`VPP\config.cpp:3`) |
| `modded class VPPAdminHud` / `DefineButtons()` | sí | `VPP\5_Mission\VPPAdminTools\GUI\VPPAdminHud.c:3,124` | se llama en el constructor, después de los botones base (`:54-55`) |
| `InsertButton(string,string,string,string)` | sí, `private` | `VPPAdminHud.c:126-129` | accesible desde el `modded class` (el mismo patrón que `VPP\…\GUI\Example.c:4-8`) |
| imagen `vpp_icon_obj_editor` | sí | `VPP\GUI\Textures\dayz_gui_vpp.imageset` (2 hits) | es el mismo icono que Object Builder: confuso |
| `AdminHudSubMenu` y `OnCreate(Widget)`, `OnUpdate(float)`, `OnClick`, `IsSubMenuVisible`, `CreateWidgets`, `M_SUB_WIDGET`, `m_TitlePanel`, `m_closeButton` | sí | `VPP\5_Mission\VPPAdminTools\GUI\SubMenus\AdminHudSubMenu.c:1-8,17,44,114,121,166` | las firmas casan |
| Botón → clase del submenú | por nombre: `ButtonType.ToType()` | `VPP\…\GUI\VPPButton.c:70-84` | `"MenuBattleRoyaleManager"` tiene que ser a la vez nombre de clase y nombre de permiso: casa |
| Visibilidad del botón | el servidor evalúa `VerifyPermission` por cada nombre de botón | `VPPAdminHud.c:57-63`, `PermissionManager.c:166-177` | correcto, fail-closed |
| `GetPermissionManager()` / `AddPermissionType(array<string>)` | sí | `VPP\4_World\…\PermissionManager\PermissionManager.c:1245-1248, 527-541` | solo en servidor (`VPP\4_World\VPPAdminTools\Plugins\PluginManager.c:1-23`). El BR lo llama en `MissionServer.OnInit` (`BR\scripts_server\5_Mission\Mission\MissionServer.c:13`), después de `PluginManagerInit()` (`scripts\5_mission\mission\missionbase.c:21`): el plugin ya existe |
| `GetRPCManager().SendRPC(ns, fn, NULL, true)` | sí (CF) | `REF\cf_scripts\3_Game\CommunityFramework\RPC\RPCManager.c:111` | ruta **sin** contraseña de VPP |
| `AddRPC(..., SingleplayerExecutionType.Server)` | sí (CF) | `RPCManager.c:171`, enum `:8-13` | — |

**Qué falta en la integración VPP:**
1. Gate de permisos en servidor (crítico, §0).
2. Permisos por acción. VPP separa el permiso de menú del de acción (`MenuServerManager` frente a `ServerManager:RestartServer`, `PermissionManager.c:37`; el handler comprueba el de acción, `ServerManager.c:22`).
3. Botones Pause, Resume y Airdrop (y, opcionalmente, mostrar el estado actual).
4. Envío por `VSendRPC` como el resto de VPP (`VPP\…\MenuServerManager\MenuServerManager.c:195`; definición en `VPP\3_Game\CommunityFramework\RPC\RPCManager.c:107/173`). No sustituye al gate, pero añade la contraseña de admin de VPP (`:35-60`).
5. Log y webhook al estilo VPP (`GetSimpleLogger().Log`, `GetWebHooksManager().PostData(AdminActivityMessage, …)`, `ServerManager.c:29-30`; getters en `LogManager.c:215` y `WebHooksManager.c:1539`).
6. Feedback al admin (`GetPermissionManager().NotifyPlayer(id, msg, NotifyTypes.NOTIFY)`, p. ej. `ServerManager.c:34`).
7. `requiredAddons` y orden de carga (§1, última nota).
8. Higiene: el layout reutiliza claves `STR_BR_COT_*`, el icono es el de Object Builder y el `OnUpdate` vacío sobra.

## 3. Mapa COT → VPP

| Concepto | COT (hoy) | VPP (destino) |
|---|---|---|
| Registro de la UI | `modded JMModuleConstructor.RegisterModules` + `JMRenderableModuleBase` + `JMFormBase` | `modded VPPAdminHud.DefineButtons()` → `InsertButton(<NombreClase>, label, icon, tooltip)` (`VPPAdminHud.c:124-129`) + `class <NombreClase>: AdminHudSubMenu` (`AdminHudSubMenu.c:1`), con `OnCreate` que hace `CreateWidgets(layout)` (`:114-119`) y recoge `Header` y `BtnClose` (patrón en `Example.c:20-30`) |
| Permiso de vista | `RegisterPermission("…View")` + `HasAccess()` en cliente | El nombre del botón es el permiso. El servidor lo registra con `AddPermissionType({"MenuBattleRoyaleManager"})` y responde con `VerifyPermission` por botón (`PermissionManager.c:166-177`); el cliente usa la caché `HasPermission` (`VPPAdminHud.c:192`) |
| Permiso de acción | `BattleRoyale.StateMachine.Update` (registrado, nunca comprobado) | Nuevos: `BattleRoyale:NextState`, `BattleRoyale:Pause`, `BattleRoyale:Resume`, `BattleRoyale:SpawnAirdrop`, registrados con `AddPermissionType` en servidor |
| Envío cliente → servidor | `ScriptRPC.Send(NULL, 1100x)` en el rango del módulo | `GetRPCManager().VSendRPC("RPC_BattleRoyaleAdmin", "<Fn>", params, true)` (`RPCManager.c:173`) |
| Recepción en servidor | `OnRPC` + `switch` | `GetRPCManager().AddRPC("RPC_BattleRoyaleAdmin", "<Fn>", this, SingleplayerExecutionType.Server)`; el handler tiene la firma `(CallType, ParamsReadContext, PlayerIdentity, Object)` |
| Comprobar permiso en servidor | `JMPermissions.HasRPC(perm, sender, out admin)` (`REF\cot\…\Permissions\JMPermissions.c:37-50`) | `if (type != CallType.Server \|\| !sender) return; if (!GetPermissionManager().VerifyPermission(sender.GetPlainId(), "BattleRoyale:X")) return;` (patrón en `ServerManager.c:20-22`). `VerifyPermission` es fail-closed: rechaza permisos no registrados (`PermissionManager.c:1010-1018`), usuarios sin grupo (`:1036-1045`) y grupos sin el permiso (`:1092-1098`). Superadmin con `targetID==""` → true (`:1047-1050`) |
| Log y auditoría | `GetCommunityOnlineToolsBase().Log` + webhook (`JMModuleBase.c:184-188`), sin usar en el BR | `GetSimpleLogger().Log(...)` + `GetWebHooksManager().PostData(AdminActivityMessage, new AdminActivityMessage(id, name, text))` (`ServerManager.c:29-30`) |
| Feedback | notificación COT | `GetPermissionManager().NotifyPlayer(id, msg, NotifyTypes.NOTIFY)` |
| Migrar permisos | `JMPermissions` (roles COT) | Grupos de `$profile:VPPAdminTools/Permissions/UserGroups/` (`PermissionManager.c:12`) o superadmins en `SuperAdmins.txt` (`:11`). No hay conversión automática: es manual, añadiendo los `BattleRoyale:*` a un grupo con el Permission Editor de VPP. Ojo: `CreateDefualtUserGroups` solo mete en "Admins" los permisos registrados en ese momento (`:542-546`), así que en un servidor con `UserGroups` ya creado los permisos BR nuevos **no** se añaden solos a ningún grupo: solo los tendrán los superadmins (fail-closed, que es lo correcto) |

| Función BR | Permiso VPP propuesto | RPC (`RPC_BattleRoyaleAdmin::`) | Nota |
|---|---|---|---|
| Next State | `BattleRoyale:NextState` | `NextState` | sustituye al handler sin gate de `BattleRoyaleServer.c:536` |
| Pause | `BattleRoyale:Pause` | `Pause` | — |
| Resume | `BattleRoyale:Resume` | `Resume` | — |
| Spawn Airdrop | `BattleRoyale:SpawnAirdrop` | `SpawnAirdrop` | solo `#ifdef EXPANSIONMODMISSIONS`; comprobar `sender.GetPlayer()` no nulo; API de Expansion por verificar |
| Add Player/Group, Horde, Chemicals | — | — | borrar (no tienen handler) |
| (nuevo, opcional) Ver estado | `MenuBattleRoyaleManager` | `RequestState` → respuesta al cliente | nombre del estado actual + `IsPaused()` |

## 4. Plan v1.1 (sin código)

**Decisión recomendada:** cortar COT en v1.1 y dejar VPP como única UI de admin, apoyada en una capa neutral. Si el dueño quiere seguir aceptando COT, la capa neutral permite mantener `#ifdef JM_COT` con un adaptador fino (unas 40 líneas) sin duplicar la lógica. Aun así, recomiendo cortar: COT ha cambiado de API y es una superficie más que mantener y auditar.

Pasos, en orden:
1. **[DESIGN] Capa neutral en servidor** (si no se hizo ya en v1.0, §5). `BRAdminController` en `scripts_server\5_Mission\BattleRoyale\Admin\` con `bool Execute(BRAdminAction action, PlayerIdentity sender)`, que valida `sender`, pide autorización, aplica y registra. `BRAdminAction` es un enum neutral: NextState, Pause, Resume, SpawnAirdrop. Sustituye a `BattleRoyaleCOTStateMachineRPC`.
2. **[DESIGN] Autorizador enchufable, fail-closed.** `BRAdminAuthorizer.IsAllowed(PlayerIdentity, BRAdminAction)`:
   - `#ifdef VPPADMINTOOLS` → `VerifyPermission(plainId, "BattleRoyale:<Action>")`.
   - Si no hay VPP → lista nativa `admins_steamid64` (`BR\scripts_server\3_Game\Config\BattleRoyaleGameData.c:7-9`).
   - Por defecto → **false**.
   - `AddPermissionType` con los 4 permisos de acción + `MenuBattleRoyaleManager` en `Init()` (sustituye `BattleRoyaleServer.c:43-45`).
3. **Servidor.** Borrar `BattleRoyaleServer.NextState` (`:536-551`) y su `AddRPC` (`:21-23`). Registrar `RPC_BattleRoyaleAdmin::{NextState,Pause,Resume,SpawnAirdrop}`, y que cada uno llame a `BRAdminController.Execute`. Borrar `BR\scripts_server\5_Mission\COT\` entero.
4. **Cliente VPP.** Reescribir `MenuBattleRoyaleManager.c`: 4 botones con `VSendRPC`, el botón Airdrop oculto si no hay `EXPANSIONMODMISSIONS`, y opcionalmente una etiqueta con el estado. Ampliar el layout `MenuBattleRoyaleManager.layout` con `BtnPause`, `BtnResume` y `BtnAirdrop`. Nuevas claves `STR_BR_ADMIN_*` en `languagecore\stringtable.csv`. Elegir otro icono.
5. **Cliente COT.** Borrar `BR\scripts_client\5_Mission\COT\` y `BR\gui\layouts\COT\`, y retirar el enum `BattleRoyaleCOTStateMachineRPC` de `BattleRoyaleEnums.c:1-17`. Si se mantiene COT: adaptador de una sola clase que llama al mismo RPC neutral, con `requiredAddons` condicionado (no se puede: ver riesgo R1).
6. **config.cpp.** Añadir `"DZM_VPPAdminTools"` a `requiredAddons` de `BattleRoyale_Scripts_Client` (`BR\scripts_client\config.cpp:5-10`) **solo si VPP pasa a ser dependencia obligatoria**; de lo contrario, ver R1. No hace falta un define propio.
7. **Documentación para el admin.** Lista de permisos `BattleRoyale:*` y cómo añadirlos a un grupo en el Permission Editor de VPP.

Riesgos:
- **R1. Orden de carga frente a dependencia opcional.** Sin `requiredAddons`, el `#ifdef VPPADMINTOOLS` puede quedar en falso según el orden de `-mod`, y la UI no aparece. Con él, el BR deja de cargar sin VPP. Hay que decidir: o VPP es obligatoria (recomendado si v1.1 es "con VPP"), o se publica un PBO puente `BattleRoyale_VPP` aparte con su propio `requiredAddons`. No se ha verificado cómo ordena DayZ los addons sin `requiredAddons`.
- **R2.** `VerifyPermission` notifica al admin cada rechazo (`sendNotify=true` por defecto, `:1008`). Es aceptable.
- **R3.** Next sobre un estado pausado no avanza hasta Resume (`0_BattleRoyaleState.c:85-92`). Hay que documentarlo o hacer que Next quite la pausa. Lo decide el dueño.
- **R4.** Airdrop depende de la API de Expansion, que no está verificada aquí, y de que `sender.GetPlayer()` no sea nulo.
- **R5.** Los servidores que ya usaban COT pierden su configuración de roles. La migración es manual (§3).

Criterios de aceptación, in-game: servidor dedicado + 2 clientes, con VPP. Cliente A es superadmin, cliente B no tiene grupo.
1. A ve el botón BattleRoyale en la barra de VPP; B no lo ve.
2. A: Next → el `.RPT`/log muestra "Leaving State X / Entering State Y" (`BattleRoyaleServer.c:205,222`). Pause → el estado no avanza aunque se cumpla la condición. Resume → avanza.
3. **Negativo:** B envía los 4 RPC (cliente diag o script de prueba con `GetRPCManager().SendRPC("RPC_BattleRoyaleAdmin","NextState",…)` y también con el nombre viejo `RPC-DayZBR-Server::NextState`) → el estado no cambia y queda la línea de rechazo en el log de VPP (`PermissionManager.c:1038-1039`).
4. Un admin de un grupo VPP sin `BattleRoyale:Pause` pero con `BattleRoyale:NextState` → Next funciona y Pause se rechaza.
5. Servidor sin VPP: el mod carga sin errores de script (si VPP es opcional), y los handlers solo aceptan a `admins_steamid64`.
6. Validación offline con `dayz-script-validator` antes del test, contrastando `errors` con la base.

Esfuerzo estimado:
- Capa neutral + autorizador: unas 2 h.
- Submenú VPP + layout + stringtable: 2-3 h.
- Limpieza de COT + config: 0,5 h.
- Test in-game: 1 ciclo (DZ-R5), unos 30-45 min con dos clientes.
- **Total: 1 día**.

## 5. Qué hacer ya en v1.0 para no rehacerlo

1. **El gate de permisos en servidor, obligatorio en v1.0.** Hoy son explotables los dos caminos (§0). Con COT: en cada `case` del `OnRPC` del servidor, `JMPermissions.HasRPC("BattleRoyale.StateMachine.Update", sender)` (`REF\cot\…\JMPermissions.c:37`). Con VPP: `VerifyPermission` en `NextState`. Si v1.0 sale "sin admin", **retirar el `AddRPC("NextState")`** o meterlo detrás del autorizador.
2. **Extraer `BRAdminController` + `BRAdminAuthorizer` (§4, pasos 1-2) ya en v1.0.** Los handlers de COT pasan a ser llamadas de una línea, y en v1.1 solo cambia el adaptador de UI. Es la pieza que evita rehacer trabajo.
3. **Borrar el código muerto**: AddFakePlayer/Group, SpawnHorde y SpawnChemicals no tienen handler. Renombrar el enum a uno neutral si se toca.
4. **Decidir la política de dependencias** (R1) y fijar `requiredAddons` en consecuencia. Hoy, ni COT ni VPP tienen garantizado el orden de carga.
5. No invertir en la UI de COT en v1.0 más allá de lo que haga falta para compilar contra la COT actual.

## No verificado

- No se ha compilado ni probado in-game nada. "Compila contra VPP" significa solo que existen las clases y miembros con firmas compatibles.
- La API de Expansion Missions (`ExpansionMissionModule.s_Instance.CallAirdrop`): sin fuentes en `REF\`.
- La propagación del `#define VPPADMINTOOLS` a otros mods y el orden de carga sin `requiredAddons`.
- Que la imageset `ccgui_enforce` (botón de cerrar del layout BR) exista en vanilla 1.30: no está en `scripts\` y estaría en los datos.
- Compatibilidad completa del módulo COT con la COT actual (solo se han revisado los miembros sobrescritos).

**GREEN-with-fixes.** Sin P1 identificados. El override es coherente para los toasts; **no demuestra resuelto el bloqueo central**.

- **P2 — El selector de spawn altera el foco de otros overlays; preexistente.** [SpawnSelectionMenu.c:71](/C:/Users/guill/Repos/BattleRoyale/BattleRoyale/addons/scripts_client/5_Mission/GUI/SpawnSelectionMenu.c:71) duplica el bloqueo de `super.OnShow()` y `:81` ejecuta `ResetGameFocus()` global. Si permanece abierto otro diálogo independiente —por ejemplo, el login de VPP, que incrementa el contador en [missionGameplay.c:359](/C:/Users/guill/Repos/br-ref/vpp/VPPAdminTools/5_Mission/VPPAdminTools/missionGameplay.c:359)—, cerrar spawn borra también su bloqueo. **Fix:** eliminar ambos ajustes manuales y conservar el emparejamiento heredado.

- **P3 — Etiqueta central congelada; preexistente.** [IngameHud.c:37](/C:/Users/guill/Repos/BattleRoyale/BattleRoyale/addons/scripts_client/5_Mission/GUI/IngameHud.c:37) deja de actualizarla al comenzar la partida, sin ocultarla. Si había un jugador etiquetado, el nombre permanece. La raíz vanilla carece de `IGNOREPOINTER`, pero mide **1×1 en unidades exactas**: es un capturador residual candidato, insuficiente para atribuirle un bloqueo amplio. **Fix:** ocultar/limpiar la etiqueta al empezar y marcar su raíz.

- **P3 — Se elimina globalmente el cierre por clic; nuevo.** [ExpansionNotificationView.c:13](/C:/Users/guill/Repos/BattleRoyale/BattleRoyale/addons/scripts_client/4_World/GUI/ExpansionNotificationView.c:13) afecta también a BAGUETTE, MARKET, GARAGE y mensajes ajenos a BR. Su [OnMouseButtonUp:167](/C:/Users/guill/Repos/br-ref/exp_core_scripts/4_World/DayZExpansion_Core/Systems/Notifications/ExpansionNotificationView.c:167) deja de poder descartarlos; un aviso largo permanece hasta expirar. **Fix:** acotar el comportamiento a los avisos de BR.

Cobertura del síntoma central:

| Superficie | Resultado estático |
|---|---|
| Fase, daño de zona, victoria, killfeed y minimapa | Todos sus widgets ignoran el puntero; sin adquisición de foco. |
| Localidad, GPS y visión nocturna de Expansion | Jerarquías pasivas. El aviso central de localidad no presenta el defecto del toast. |
| Spawn y mapa | Menús interactivos; foco explícito. El mapa añade y retira su exclusión de entrada. |
| Popups y hints | Pertenecen al menú principal, pausa/carga. No encontré llamadas a `CreatePopup()` ni una ruta que los inserte durante juego libre. |
| Espectador BR | Layout pasivo, sin referencias de creación en esta rama. |
| Tapones y brújula Expansion | Algunos hijos decorativos omiten `ignorepointer`; quedan como candidatos pequeños pendientes de comprobar en motor. |

`4_World`, dependencias y guards son coherentes. `SetFlags` añade el flag; los hijos y contenedores del toast ya lo tienen. No encontré reseteos posteriores durante su ciclo normal. **La recarga Dabs en DIAG sí recrea la raíz y pierde el flag** ([ScriptView.c:145](/C:/Users/guill/Repos/br-ref/dabs_scripts/3_Game/DabsFramework/MVC/ScriptView.c:145)). Una carga fallida puede devolver null, pero `super.ShowNotification()` ya la dereferencia antes del nuevo guard. No se añade trabajo por frame.

**Sin verificar:** compilación y clics reales en esta revisión. Las referencias Expansion contrastadas coinciden con sus PBO, pero el `scripts_client.pbo` actual de `@BattleRoyale-dev` **no contiene el override**. No puedo vincular la prueba Diag declarada a este diff. Antes de cerrar el bug: identificar el artefacto/hash y probar clic, arrastre y rueda sobre inventario/mapa con y sin toast, además de reproducir el bloqueo central.


# Plan: partidas en serie sin reiniciar el servidor (v3, 2026-11-14)

Base: `main` b5a7e88 (1.4.0). Material: council de Astra y Muse (DeepSeek pendiente) y sondas en DayZDiag, en
`C:\Users\guill\Repos\br-runs\serial-matches-2026-11-14\` (`lanes\*\PLAN.md`, `S0_RESULTS.md`).
Decisiones de Guillermo (14-nov): el loot queda fuera (cada owner decide cómo lo repone); limpiar los cuerpos y lo que llevaban;
**los jugadores siguen conectados** y vuelven solos al lobby.

## Qué cambia

- Opción `serial_matches` en `server_settings.json`, apagada por defecto. Apagada, todo es 1.4.0.
- Encendida, al acabar la partida el servidor no se cierra: los que siguen conectados (ganador, espectadores) reciben un
  personaje nuevo en el lobby sin desconectarse, se borran los restos de la partida y empieza el lobby siguiente.
- Los muertos que espectan siguen conectados hasta el final. Con el espectador desactivado salen como hoy a los 15 s y
  vuelven a entrar cuando el lobby está abierto (una sala de espera para ellos sería una fase aparte).
- `serial_restart_every_matches` (0 = nunca): reinicio completo (`RequestExit`, el de hoy) al acabar la partida número K.
- Con `enable_vigrid_api` o `use_autolock` la opción se ignora con un aviso (UUID y bloqueo por proceso, `BattleRoyaleServer.c:55-75`).
- `RestartMission()` descartado: tumba el servidor dedicado (`S0_RESULTS.md`). El loot lo repone el CE como esté configurado
  (medido: un tercio en 8 min tras borrar ~2.300 objetos).

## Servidor

1. **Fin de partida.** El ganador ya no se expulsa a los 15 s (`8_BattleRoyaleWin.c:118-133`): el estado de victoria termina y
   pasa al de reinicio con él dentro (si saliera de los estados, `OnPlayerTick` lo echaría, `BattleRoyaleServer.c:609-626`).
   El estado de reinicio (`9_BattleRoyaleRestart.c:48-51`) pide el cambio de partida en vez de `RequestExit`.
2. **Cambio de partida** (`BRMatchTurnover`, conducido desde `BattleRoyaleServer.Update` fuera del `foreach` de estados):
   - Cancela el trabajo diferido de los estados viejos (`DisableInput`, `BroadcastHeatMap`, `Unstuck`, expulsiones de late join,
     `Start` de espectador pendientes). Si el hilo de `ProcessPlayers` siguiera vivo, aborta a `RequestExit`.
   - Pone a cero lo de la partida: `s_MatchPlayerNames`, zonas (`m_Zones`, `m_PlayAreas`), `temp_disconnecting`.
   - Grafo nuevo con `BuildStates()` extraído de `Init` (`BattleRoyaleServer.c:85-133`), sin repetir RPC, `BlueZone` ni gestor de espectadores; clima nuevo.
   - Devuelve a cada identidad conectada al lobby: personaje nuevo por el camino normal (`CreateCharacter`, `EquipCharacter`,
     `InvokeOnConnect` → `OnPlayerConnected`), borra el cuerpo vivo viejo y el ancla de espectador, y avisa al cliente.
   - Abre el lobby; la espera mínima cuenta desde aquí (`1_BattleRoyaleDebug.c:41,73,105-107` comparaban el tiempo desde el arranque).
   - Borra los restos por lotes con el lobby ya abierto (lejos del lobby, sin tirones).
   - Plazo de 120 s; si no termina, `RequestExit`.
3. **Registro de restos**, sin barrer el mapa (medido: 0,2-0,5 s por barrido de 1,5 km):
   cuerpos (todas las muertes pasan por `OnPlayerKilled`, `BattleRoyaleServer.c:545`), objetos que un jugador deja en el
   suelo (`ItemBase.EEItemLocationChanged`, con tope), anclas de espectador que se quedan sin dueño (hoy no se borran nunca) y gallinas de victoria.
4. **Informe** en el log al abrir cada lobby: partida nº del proceso, segundos del cambio, jugadores devueltos y borrados por tipo.

## Cliente

- RPC `SetStayConnected` al conectar: con la opción y el espectador activos, el muerto no sale a los 15 s (`DayZPlayerImplement.c:65-91`).
- RPC `ReturnToLobby`: deshace la muerte y el espectador (`RestoreAfterDeath`, `ResetSpectating`), cierra la pantalla de victoria,
  cancela la salida pendiente y recrea el estado de partida del cliente (`BattleRoyaleClient` nuevo, que llama a `Reset()`) y el HUD del BR.
  El HUD vanilla lo rehace el propio juego al seleccionar el personaje nuevo (`PlayerBase.OnSelectPlayer` → `ResetGUI`).

## Pruebas (DayZDiag, un cliente)

- Ganador: 3 partidas seguidas en el mismo proceso sin que el cliente se desconecte; lobby, HUD y contador de kills a cero en cada una.
- Muerto: build de prueba en solitario (`Count() < 1`), `player_kill` en ronda, vuelta al lobby con control; el cuerpo viejo desaparece.
- `serial_restart_every_matches = 2`: la segunda partida termina con `RequestExit`.
- Opción apagada: una partida igual que hoy.
- Sin probar aquí (hacen falta dos jugadores): vuelta desde el espectador y borrado del ancla.

Gates: `check_standalone.py .`, `gate.sh`, `build.py` y DayZDiag. Revisión adversarial de Astra sobre el diff antes del merge.

## Riesgos

- Volver del espectador a un personaje es la parte menos probada (cámara del espectador en el cliente); solo con dos jugadores.
- Alguien que entra justo durante el cambio recibe el aviso de partida en curso y reintenta (ventana de un par de ticks).
- Memoria nativa que crezca entre partidas: la cubre el reinicio cada K.
- Objetos metidos en contenedores del mundo no se registran: quedan al CE.

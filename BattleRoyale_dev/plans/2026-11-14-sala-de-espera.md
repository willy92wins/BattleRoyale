# Plan: sala de espera en el lobby (2026-11-14)

Base: `main` c67ef67 (1.5.0). Decisiones de Guillermo (14-nov): entran en la sala de espera quien se conecta con una
partida en curso, los muertos cuando el espectador está desactivado y los espectadores que pulsan F1; funciona siempre,
con partidas en serie y con el reinicio clásico.

## Qué cambia

- Opción `enable_waiting_room` en `general_settings.json`, encendida por defecto. Apagada, todo es 1.5.0.
- Durante una partida, la sala de espera es el lobby: personaje en el lobby, inmune, curado, sin poder salir del radio
  del lobby, fuera de la partida (no cuenta como jugador ni recibe daño de zona). El HUD sigue la partida (fase, killfeed).
- Al acabar la partida: con partidas en serie, los que esperan pasan al lobby siguiente sin personaje nuevo; con el
  reinicio clásico, el reinicio los desconecta como a todos.

## Entradas

| Quién | Hoy | Con la sala de espera |
|---|---|---|
| Se conecta con partida en curso | aviso al preparar la conexión y expulsión a los 30 s (`MissionServer.c` `OnClientPrepareEvent`, `BattleRoyaleServer.c` `OnPlayerConnected`) | personaje creado en el lobby y aviso «partida en curso, esperas en el lobby (~N min)» |
| Muere con el espectador desactivado | sale del servidor a los 15 s (`DayZPlayerImplement.c`) | pasa al lobby tras `spectator_delay_seconds` |
| Muere sin nadie vivo a quien mirar | sale a los 15 s | pasa al lobby |
| Espectador que pulsa F1 (`UADayZBRReadyUp`) | nada | pasa al lobby; el overlay del espectador lo indica |

Las tres entradas usan el mismo camino: un personaje nuevo en el lobby con `MissionServer.BR_CreateLobbyCharacter`
(cliente reseteado con `ReturnToLobby` antes), que termina en `OnPlayerConnected`; durante una partida, en vez de la
expulsión, el jugador entra en la sala de espera (`br_waiting`).

## Servidor

- `PlayerBase.br_waiting`; `OnPlayerTick` de los que esperan: dentro del radio del lobby y curados; nunca el logout
  de «vivo fuera de la partida».
- RPC `SpectatorToLobby` (cliente → servidor): solo de un espectador real, con la opción activa y partida en curso,
  limitado a una petición cada 2 s.
- Cambio de partida en serie: un jugador que espera se añade al lobby nuevo con su personaje (reset de HUD).
- `SetStayConnected` también con la sala de espera activa: el muerto ya no sale a los 15 s.
- Admins: como hoy (pueden entrar durante la partida y moverse libremente).

## Cliente

- RPC `SetWaitingRoom`: el espectador ve «F1: esperar en el lobby» y F1 pide pasar al lobby.

## Pruebas (DayZDiag, un cliente)

- Muerte en ronda con el espectador desactivado (build de prueba en solitario): pasa al lobby, inmune, vuelve al radio
  si sale; con partidas en serie entra en el lobby siguiente sin personaje nuevo.
- Entrar con partida en curso: build de prueba en solitario con la partida corriendo y el cliente reconectando
  (`mode=client`): espera en el lobby, sin expulsión.
- Opción apagada: expulsión de quien entra tarde, como en 1.5.0.
- Sin probar aquí: F1 desde el espectador (hace falta un segundo jugador vivo).

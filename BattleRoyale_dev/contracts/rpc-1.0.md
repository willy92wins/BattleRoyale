# RPC contract for 1.0 (server -> client)

Namespace: `RPC_DAYZBR_NAMESPACE` ("RPC-DayZBR"), sent with CF `GetRPCManager().SendRPC(...)`.
The server lanes send, the client lane registers the handler in `BattleRoyaleRPC.c` and stores the
value for the HUD. Existing RPCs keep their names and payloads.

| RPC | Payload | Sent when | Client use |
|---|---|---|---|
| `SetMatchPhase` | `Param2<int, int>`: phase (`EBRMatchPhase`), seconds left in the phase (`-1` if unknown) | On every state `Activate()` and when a round timer is (re)armed; unicast to a player on connect/reconnect | HUD phase text and timer label |
| `KillFeedEvent` | `Param7<int, string, string, string, int, int, int>`: meta = cause (`EBRKillFeedType`) in bits 0-7 and flags (`BRKillFeedFlags`) in bits 8-15, killer name (empty when the death names nobody), victim name, class name of the weapon, explosive, vehicle or animal (empty if none; the client shows its display name in its own language), distance in whole metres (0 when it does not apply), `PlayerIdentity.GetPlayerId()` of the killer and of the victim (-1 when unknown or nobody; the client compares them with its own to mark its rows) | Once per player death in a running match, broadcast | Killfeed widget, fixed pool of 6 rows: icon of the cause, sentence, weapon and distance |

Rules:
- Broadcast once, never one unicast per player with the same payload.
- The client must accept both RPCs in any order and before the HUD exists (store, then render).
- `EBRMatchPhase` lives in `scripts_client/2_GameLib/BattleRoyaleEnums.c` so both sides compile it.
- `EBRKillFeedType` and `BRKillFeedFlags` live in the same file. The cause is decided by the server (`BattleRoyaleKillFeedClassifier`); enum values are wire values, so new ones go at the end and a client draws an unknown one as a plain death. Until 1.3.0 the payload was `Param4<string, string, string, float>`; client and server must be of the same build.

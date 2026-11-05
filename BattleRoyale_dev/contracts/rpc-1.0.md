# RPC contract for 1.0 (server -> client)

Namespace: `RPC_DAYZBR_NAMESPACE` ("RPC-DayZBR"), sent with CF `GetRPCManager().SendRPC(...)`.
The server lanes send, the client lane registers the handler in `BattleRoyaleRPC.c` and stores the
value for the HUD. Existing RPCs keep their names and payloads.

| RPC | Payload | Sent when | Client use |
|---|---|---|---|
| `SetMatchPhase` | `Param2<int, int>`: phase (`EBRMatchPhase`), seconds left in the phase (`-1` if unknown) | On every state `Activate()` and when a round timer is (re)armed; unicast to a player on connect/reconnect | HUD phase text and timer label |
| `KillFeedEvent` | `Param4<string, string, string, float>`: killer name (empty = zone or environment), victim name, weapon display name (empty if none), distance in metres | Once per player death in a running match, broadcast | Killfeed widget, fixed pool of 5 rows |

Rules:
- Broadcast once, never one unicast per player with the same payload.
- The client must accept both RPCs in any order and before the HUD exists (store, then render).
- `EBRMatchPhase` lives in `scripts_client/2_GameLib/BattleRoyaleEnums.c` so both sides compile it.

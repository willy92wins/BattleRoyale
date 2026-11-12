No demonstrated P1/P2 merge blocker in `e2744b4..851ead3`. Two P3 findings:

- **P3 — Persistent overlay shows a stale alive count.** [BattleRoyaleRPC.c:65](/C:/Users/guill/Repos/br-wt/late-join-notice/BattleRoyale/addons/scripts_client/3_Game/BattleRoyale/RPC/BattleRoyaleRPC.c:65). Repro: A spectates B while B/C/D are alive; C dies. Only C’s watchers receive another `SpectateTarget`; `SetPlayerCount` updates separate fields without refreshing the overlay. A continues seeing “3 alive” until switching targets. Refresh the overlay from the authoritative count update.

- **P3 — Failed anchor creation still commits spectator startup.** [BattleRoyaleSpectators.c:150](/C:/Users/guill/Repos/br-wt/late-join-notice/BattleRoyale/addons/scripts_server/5_Mission/BattleRoyale/Server/BattleRoyaleSpectators.c:150). Fault-injection scenario: return null from `CreatePlayer` while the target is outside the corpse’s streaming bubble. Execution still selects the camera and sends `SpectateTarget`, suppressing automatic departure. Subsequent refresh requests only call `Watch`; they never retry anchor creation. This recreates the unresolved-target condition you measured. Abort or retry before announcing success. No natural spawn failure reproduced here, hence P3.

Lifecycle tracing found no demonstrated duplicate anchor or contamination of match counts, zone damage, winners, stats, or airdrop selection. Cycling reuses the anchor. Vanilla `HandleBody` deletes a normal living, conscious logout body; missing explicit deletion alone does not establish a leak. Remaining anchors at match end are bounded by process shutdown. F2 requires match membership.

The layout prefix and widget names match; cleanup unlinks the overlay. Post-frame placement agrees with COT. **Still unverified in game:** transient possession/HUD timing, anchor visibility and physics, infected reactions, physiology, inventory/actions, and VOIP.

Read-only; `git diff --check` passed. No runtime tests performed.

SOUND-with-fixes


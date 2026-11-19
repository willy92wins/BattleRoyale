# BattleRoyale — bug ledger

Live list of known defects outside the scope of the work in progress. One entry per defect: date, source, scenario, evidence and state.

## BR-001 — The Prepare worker can still be running when a match ends and the turnover exits the server (open)

- **Found:** 2026-11-19, by Sol in its review of the VPP panel plan (`br-runs/vpp-panel-2026-11-18/sol_plan_review_r3.log`). Pre-existing: it does not depend on the panel.
- **Scenario:** with serial matches on, the Prepare worker is still running when its watchdog moves the match on to StartMatch (`States/4_BattleRoyalePrepare.c:104-113`). If every player then leaves, StartMatch completes by itself (`States/5_BattleRoyaleStartMatch.c:123-133`) and the match reaches Restart. If the worker is still alive when the turnover starts, the turnover takes the exit path (`BRMatchTurnover.c:88`, `BattleRoyaleServer.c:606`) and the server shuts down instead of opening the next lobby.
- **Not reproduced** in the game. A cheap reproduction would hold the worker past the watchdog (a lot of players, or an artificial `Sleep`) and disconnect everyone in StartMatch.
- **State:** open. Outside the scope of the panel (R25).

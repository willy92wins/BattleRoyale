//#ifdef JM_COT
enum BattleRoyaleCOTStateMachineRPC
{
    INVALID = 11000, //Do not move this

    Next,
    Pause,
    Resume,
    AddFakePlayer,
    AddFakeGroup,
    SpawnAirdrop,
    SpawnHorde,
    SpawnChemicals,

    COUNT //Do not move this
}
//#endif


enum BattleRoyaleMatchMakingState
{
	INVALID = -1, //Do not move this

	None,
	Searching,
	Connecting,
	Failed,
	Success
}
//! Match phase shown to clients by the SetMatchPhase RPC (see BattleRoyale_dev/contracts/rpc-1.0.md)
enum EBRMatchPhase
{
	LOBBY = 0,
	SPAWN_SELECTION,
	PREPARE,
	STARTING,
	ROUND,
	LAST_ROUND,
	FINISHED,
	RESTARTING
}

//#ifdef JM_COT
enum BattleRoyaleCOTStateMachineRPC
{
    INVALID = 11000, //Do not move this

    Next,
    Pause,
    Resume,
    AddFakePlayer,
    AddFakeGroup,
    SpawnAirdrop, // Reserved: the airdrop was removed; kept so later ids do not move.
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

//! Cause of a death, decided by the server and sent in the low byte of the KillFeedEvent RPC
//! (see BattleRoyale_dev/contracts/rpc-1.0.md). The values travel over the network: add new ones at the end
enum EBRKillFeedType
{
	DIED = 0,       //!< no cause known
	FIREARM,        //!< shot by a player
	MELEE,          //!< hit by a player holding a melee weapon or any other item
	FISTS,          //!< hit by a player with bare hands
	EXPLOSIVE,      //!< grenade, mine or any other explosion; the killer is who set it off, when known
	VEHICLE,        //!< run over; names no killer
	VEHICLE_CRASH,  //!< died inside a crashing vehicle
	FALL,
	ZONE,           //!< damage taken outside the play area
	BLEED_OUT,      //!< blood loss; the killer is the last player who hit the victim, when recent enough
	INFECTED,
	ANIMAL,
	SUICIDE,
	DROWNED,
	FIRE,
	BARBED_WIRE,

	COUNT           //!< not a cause: number of values above
}

//! Bits sent above the type in the KillFeedEvent RPC: meta = type | ( flags << 8 )
class BRKillFeedFlags
{
	static const int HEADSHOT = 1;
}

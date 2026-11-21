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
	RESTARTING,
	AIRDROP
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
	VEHICLE,        //!< run over; the killer is the driver when another player is at the wheel
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

//! Sights a player looks through: its client reports them to the server (SightsState RPC), which passes them on to the
//! players spectating it (SpectateSights RPC, see BattleRoyale_dev/contracts/rpc-1.0.md). The values travel over the
//! network: add new ones at the end
enum EBRSpectateSights
{
	NONE = 0,       //!< no sights: first person from the eyes
	IRONSIGHTS,     //!< the weapon's iron sights, or an optic that stands in for them
	OPTICS,         //!< an optic that does not magnify (ItemOptics.AllowsDOF), looked through on the weapon or in the
	                //!< hands. A magnifying one is reported as NONE: its lens is drawn only on its owner's client

	COUNT           //!< not a mode: number of values above
}

#ifdef SERVER
//! A spectator's anchor stays 3 m under the ground below the watched player (BattleRoyaleSpectators.AnchorPosition).
//! Below a river, a pond or a low shore its head ends up under the water level, and vanilla made it drown: the spectator
//! and the players around heard its drowning breath and pain, and its stamina ran out
modded class DrowningMdfr
{
	override bool ActivateCondition(PlayerBase player)
	{
		if (player.br_spectator_anchor)
			return false;

		return super.ActivateCondition(player);
	}

	override bool DeactivateCondition(PlayerBase player)
	{
		if (player.br_spectator_anchor)
			return true;

		return super.DeactivateCondition(player);
	}
}
#endif

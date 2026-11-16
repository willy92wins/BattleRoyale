//! Actions on another player that hurt or hold that player: force-feeding and force-drinking (spoiled food, human meat,
//! alcohol, disinfectant, tablets), restraints and handcuffs, the head sack, the gag, taking or giving blood, and the
//! defibrillator. A player who cannot take damage (lobby, waiting room) can neither suffer them nor do them to anyone,
//! as with damage. The server rejects them; the client of a player in the lobby does not offer them
modded class ActionBase
{
	override bool Can(PlayerBase player, ActionTarget target, ItemBase item, int condition_mask)
	{
		if (BR_IsBlockedOnPlayer(player, target))
			return false;

		return super.Can(player, target, item, condition_mask);
	}

	//! True for one of the actions above aimed at another player while one of the two is protected.
	//! Called for every action offered to the client on each frame: the cheap checks come first
	protected bool BR_IsBlockedOnPlayer(PlayerBase player, ActionTarget target)
	{
		if (!player || !target)
			return false;

		PlayerBase victim = PlayerBase.Cast(target.GetObject());
		if (!victim || victim == player)
			return false;

		return BR_HarmsTargetPlayer() && BR_IsProtectedPair(player, victim);
	}

	//! The actions that hurt or hold another player, with every vanilla variant inheriting from them
	protected bool BR_HarmsTargetPlayer()
	{
		return IsInherited(ActionForceConsume) || IsInherited(ActionForceConsumeSingle)
			|| IsInherited(ActionRestrainTarget) || IsInherited(ActionHandcuffTarget)
			|| IsInherited(ActionCoverHeadTarget) || IsInherited(ActionGagTarget)
			|| IsInherited(ActionCollectBloodTarget) || IsInherited(ActionCollectSampleTarget)
			|| IsInherited(ActionGiveBloodTarget) || IsInherited(ActionDefibrilateTarget);
	}

#ifdef SERVER
	//! Server, which decides: the lobby, the waiting room and the match preparation make their players unable to take
	//! damage (SetAllowDamage(false))
	protected bool BR_IsProtectedPair(PlayerBase player, PlayerBase victim)
	{
		return !victim.GetAllowDamage() || !player.GetAllowDamage();
	}
#else
	//! Client: the local player is in the lobby, before a match or in the waiting room during one. Everywhere else the
	//! action is offered and the server decides
	protected bool BR_IsProtectedPair(PlayerBase player, PlayerBase victim)
	{
		BattleRoyaleRPC rpc = BattleRoyaleRPC.GetInstance();
		return rpc.waiting || rpc.match_phase == EBRMatchPhase.LOBBY;
	}
#endif
}

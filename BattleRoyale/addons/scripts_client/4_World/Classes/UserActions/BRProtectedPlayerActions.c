//! Actions on another player that hurt or hold that player: force-feeding and force-drinking (spoiled food, human meat,
//! alcohol, disinfectant, tablets), restraints and handcuffs, the head sack, the gag, taking or giving blood, and the
//! defibrillator. In the lobby nobody can do them to anybody, whatever the phase: before a match, during the spawn
//! selection, and in the waiting room while a match runs. The server rejects them at the start, while they run and
//! right before a single-use one takes effect; the client of a player in those phases does not offer them
modded class ActionBase
{
	override bool Can(PlayerBase player, ActionTarget target, ItemBase item, int condition_mask)
	{
		if (BR_IsBlockedOnPlayer(player, target))
			return false;

		return super.Can(player, target, item, condition_mask);
	}

	//! A player who walks into the lobby, or an action that started before, does not keep going
	override bool CanContinue(ActionData action_data)
	{
		if (action_data && BR_IsBlockedOnPlayer(action_data.m_Player, action_data.m_Target))
			return false;

		return super.CanContinue(action_data);
	}

	//! True for one of the actions above aimed at another player in the lobby. The client asks it for every action it
	//! could offer on each frame: the cheap checks come first, the list last
	protected bool BR_IsBlockedOnPlayer(PlayerBase player, ActionTarget target)
	{
		if (!player || !target)
			return false;

		PlayerBase victim = PlayerBase.Cast(target.GetObject());
		if (!victim || victim == player)
			return false;

		return BR_IsLobbyPair(player, victim) && BR_HarmsTargetPlayer();
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
	//! Metres past the lobby radius that still count: the lobby puts back a player who stepped out on its next tick
	protected static const float BR_LOBBY_MARGIN = 5;
	//! The lobby places and puts back its players in a square of ±5 m around the centre (GetLobbySpawnPosition,
	//! BattleRoyaleDebugState.OnPlayerTick): up to 7.1 m away, even with a smaller radius
	protected static const float BR_LOBBY_SPAWN_REACH = 7.1;
	protected static vector s_BR_LobbyCenter;
	//! Squared radius with the margin; negative until the server sets the lobby, so nothing is in it before
	protected static float s_BR_LobbyRadiusSq = -1;

	//! Set once by BattleRoyaleServer.Init with the lobby of spawns_settings.json, which does not change while the
	//! server runs (the settings live in the server scripts, which this shared file cannot see)
	static void BR_SetLobbyArea(vector center, float radius)
	{
		s_BR_LobbyCenter = center;
		float reach = Math.Max(radius, BR_LOBBY_SPAWN_REACH) + BR_LOBBY_MARGIN;
		s_BR_LobbyRadiusSq = reach * reach;
	}

	//! Server, which decides: one of the two stands in the lobby, the circle in which the lobby and the waiting room
	//! keep their players
	protected bool BR_IsLobbyPair(PlayerBase player, PlayerBase victim)
	{
		return BR_IsInLobby(victim.GetPosition()) || BR_IsInLobby(player.GetPosition());
	}

	//! 2D distance, as the lobby measures it
	protected static bool BR_IsInLobby(vector pos)
	{
		float dx = pos[0] - s_BR_LobbyCenter[0];
		float dz = pos[2] - s_BR_LobbyCenter[2];
		return dx * dx + dz * dz <= s_BR_LobbyRadiusSq;
	}
#endif

#ifndef SERVER
	//! Client: the local player is still in the lobby, before the match (lobby, spawn selection, preparation) or in the
	//! waiting room during one. Everywhere else the action is offered and the server decides
	protected bool BR_IsLobbyPair(PlayerBase player, PlayerBase victim)
	{
		BattleRoyaleRPC rpc = BattleRoyaleRPC.GetInstance();
		if (rpc.waiting)
			return true;

		int phase = rpc.match_phase;
		return phase == EBRMatchPhase.LOBBY || phase == EBRMatchPhase.SPAWN_SELECTION || phase == EBRMatchPhase.PREPARE;
	}
#endif
}

modded class AnimatedActionBase
{
	//! A single-use action, such as a forced bite or sip, takes effect on its animation event, which does not ask
	//! CanContinue: the lobby is checked again right before the effect
	override void OnAnimationEvent(ActionData action_data)
	{
		if (action_data && BR_IsBlockedOnPlayer(action_data.m_Player, action_data.m_Target))
			return;

		super.OnAnimationEvent(action_data);
	}
}

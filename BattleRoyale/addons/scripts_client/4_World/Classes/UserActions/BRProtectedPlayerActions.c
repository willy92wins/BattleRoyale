//! Actions on another player that hurt or hold that player: force-feeding and force-drinking (spoiled food, human meat,
//! alcohol, disinfectant, tablets), restraints and handcuffs, the head sack, the gag, taking or giving blood, and the
//! defibrillator. In the lobby nobody can do them to anybody, whatever the phase: before a match, during the spawn
//! selection, and in the waiting room while a match runs. The server rejects them, at the start and while they run;
//! the client of a player in those phases does not offer them
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
	protected static bool s_BR_LobbyRead;
	protected static vector s_BR_LobbyCenter;
	//! Squared radius with the margin; negative, so nothing is in the lobby, when the spawns settings are missing
	protected static float s_BR_LobbyRadiusSq = -1;

	//! Server, which decides: one of the two stands in the lobby, the circle of spawns_settings.json in which the lobby
	//! and the waiting room keep their players
	protected bool BR_IsLobbyPair(PlayerBase player, PlayerBase victim)
	{
		return BR_IsInLobby(victim.GetPosition()) || BR_IsInLobby(player.GetPosition());
	}

	//! 2D distance, as the lobby measures it. The settings do not change while the server runs: read once
	protected static bool BR_IsInLobby(vector pos)
	{
		if (!s_BR_LobbyRead)
		{
			s_BR_LobbyRead = true;
			BattleRoyaleSpawnsData spawns = BattleRoyaleConfig.GetConfig().GetSpawnsData();
			if (spawns)
			{
				s_BR_LobbyCenter = spawns.spawn_point;
				float radius = spawns.radius + BR_LOBBY_MARGIN;
				s_BR_LobbyRadiusSq = radius * radius;
			}
		}

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

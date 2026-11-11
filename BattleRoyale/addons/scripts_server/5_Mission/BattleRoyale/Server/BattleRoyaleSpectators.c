#ifdef SERVER
//! Dead players watch the players still alive until the match ends.
//! The server owns who watches whom: the client only asks for the next or previous player.
class BattleRoyaleSpectators
{
	protected ref map<string, PlayerBase> m_Targets;  //!< spectator identity id -> watched player
	protected ref map<string, float> m_LastRequest;  //!< spectator identity id -> tick time of the last accepted request

	void BattleRoyaleSpectators()
	{
		m_Targets = new map<string, PlayerBase>;
		m_LastRequest = new map<string, float>;
	}

	bool IsEnabled()
	{
		BattleRoyaleGameData gameSettings = BattleRoyaleConfig.GetConfig().GetGameData();
		return gameSettings && gameSettings.enable_spectator;
	}

	//! Schedules spectating for a player who just died in a match
	void OnPlayerDied(PlayerBase killed)
	{
		if (!IsEnabled() || !killed || !killed.GetIdentity())
			return;

		int delay = BattleRoyaleConfig.GetConfig().GetGameData().spectator_delay_seconds;
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(Start, delay * 1000, false, killed.GetIdentity().GetId());
	}

	//! A player left the match (killed or disconnected): whoever watched them moves on
	void RetargetWatchersOf(PlayerBase player)
	{
		if (!player)
			return;

		array<string> watchers = new array<string>;
		foreach (string id, PlayerBase target : m_Targets)
		{
			if (target == player)
				watchers.Insert(id);
		}

		foreach (string watcherId : watchers)
			Watch(watcherId, NextAlive(player, 1));
	}

	void OnIdentityDisconnected(PlayerIdentity identity)
	{
		if (!identity)
			return;

		m_Targets.Remove(identity.GetId());
		m_LastRequest.Remove(identity.GetId());
	}

	//! Asked by the client: next (1) or previous (-1) alive player, or 0 to resend the current one
	void Cycle(PlayerIdentity identity, int direction)
	{
		if (!identity || !m_Targets.Contains(identity.GetId()))
			return;

		// One request every 0.4 s per spectator; the rest are dropped
		string id = identity.GetId();
		float now = GetGame().GetTickTime();
		if (m_LastRequest.Contains(id) && now - m_LastRequest.Get(id) < 0.4)
			return;
		m_LastRequest.Set(id, now);

		PlayerBase current = m_Targets.Get(id);
		if (direction == 0)
		{
			BattleRoyaleState state = BattleRoyaleServer.GetInstance().GetCurrentState();
			if (current && current.IsAlive() && state && state.ContainsPlayer(current))
				Watch(id, current);
			else
				Watch(id, NextAlive(current, 1));
			return;
		}

		if (direction > 0)
			direction = 1;
		else
			direction = -1;

		Watch(id, NextAlive(current, direction));
	}

	protected void Start(string identityId)
	{
		PlayerIdentity identity = FindIdentity(identityId);
		if (!identity)
			return;

		// The player may have reconnected with a living character in the meantime
		PlayerBase own = PlayerBase.Cast(identity.GetPlayer());
		if (own && own.IsAlive())
			return;

		PlayerBase target = NextAlive(null, 1);
		if (!target)
			return;

		GetGame().SelectPlayer(identity, null);
		GetGame().SelectSpectator(identity, "BattleRoyaleSpectatorCamera", HeadPosition(target));
		BattleRoyaleUtils.Info("Spectator started for " + identity.GetName());

		Watch(identityId, target);
	}

	protected void Watch(string identityId, PlayerBase target)
	{
		PlayerIdentity identity = FindIdentity(identityId);
		if (!identity)
		{
			m_Targets.Remove(identityId);
			return;
		}

		if (!target)
		{
			m_Targets.Remove(identityId);
			return;
		}

		m_Targets.Set(identityId, target);

		int low, high;
		target.GetNetworkID(low, high);

		string name = "";
		if (target.GetIdentity())
			name = target.GetIdentity().GetName();

		BattleRoyaleState state = BattleRoyaleServer.GetInstance().GetCurrentState();
		int alive = 0;
		if (state && state.GetPlayers())
			alive = state.GetPlayers().Count();

		GetRPCManager().SendRPC(RPC_DAYZBR_NAMESPACE, "SpectateTarget", new Param5<int, int, vector, string, int>(low, high, HeadPosition(target), name, alive), true, identity);
	}

	//! Alive player after (direction 1) or before (-1) the given one in the match list; any alive player when from is null or gone
	protected PlayerBase NextAlive(PlayerBase from, int direction)
	{
		BattleRoyaleState state = BattleRoyaleServer.GetInstance().GetCurrentState();
		if (!state || !state.GetPlayers())
			return null;

		array<PlayerBase> players = state.GetPlayers();
		int count = players.Count();
		if (count == 0)
			return null;

		int start = players.Find(from);
		if (start == -1)
		{
			if (direction > 0)
				start = -1;
			else
				start = 0;
		}

		for (int step = 1; step <= count; step++)
		{
			int index = (start + direction * step) % count;
			if (index < 0)
				index += count;

			PlayerBase candidate = players[index];
			if (candidate && candidate != from && candidate.IsAlive())
				return candidate;
		}

		// Only the current target is left
		if (from && from.IsAlive() && state.ContainsPlayer(from))
			return from;

		return null;
	}

	protected vector HeadPosition(PlayerBase player)
	{
		return player.GetBonePositionWS(player.GetBoneIndexByName("Head"));
	}

	protected PlayerIdentity FindIdentity(string identityId)
	{
		array<PlayerIdentity> identities = new array<PlayerIdentity>;
		GetGame().GetPlayerIndentities(identities);
		foreach (PlayerIdentity identity : identities)
		{
			if (identity && identity.GetId() == identityId)
				return identity;
		}
		return null;
	}
}
#endif

#ifdef SERVER
//! Dead players watch the players still alive until the match ends.
//! The server owns who watches whom: the client only asks for the next or previous player.
//! The server streams the world to a client around the body that client owns, not around its camera,
//! so each spectator gets an anchor: a new body, immune and without gravity, kept 3 m under the watched
//! player. The corpse stays where the player died, with its loot.
class BattleRoyaleSpectators
{
	protected ref map<string, PlayerBase> m_Targets;  //!< spectator identity id -> watched player
	protected ref map<string, float> m_LastRequest;  //!< spectator identity id -> tick time of the last accepted request
	protected ref map<string, PlayerBase> m_Anchors;  //!< spectator identity id -> anchor body
	protected ref map<string, PlayerIdentity> m_Identities;  //!< spectator identity id -> identity resolved once, reused until disconnect

	void BattleRoyaleSpectators()
	{
		m_Targets = new map<string, PlayerBase>;
		m_LastRequest = new map<string, float>;
		m_Anchors = new map<string, PlayerBase>;
		m_Identities = new map<string, PlayerIdentity>;

		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(UpdateAnchors, 1000, true);
	}

	void ~BattleRoyaleSpectators()
	{
		if (GetGame())
			GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).Remove(UpdateAnchors);
	}

	//! Anchor bodies are not match players: the match logic leaves them alone
	bool IsAnchor(PlayerBase player)
	{
		if (!player)
			return false;

		foreach (string id, PlayerBase anchor : m_Anchors)
		{
			if (anchor == player)
				return true;
		}
		return false;
	}

	protected vector AnchorPosition(PlayerBase target)
	{
		vector pos = target.GetPosition();
		pos[1] = GetGame().SurfaceY(pos[0], pos[2]) - 3.0;
		return pos;
	}

	protected void UpdateAnchors()
	{
		foreach (string id, PlayerBase anchor : m_Anchors)
		{
			PlayerBase target = m_Targets.Get(id);
			if (anchor && target)
				anchor.SetPosition(AnchorPosition(target));
		}
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
		{
			PlayerIdentity watcherIdentity = null;
			if (m_Identities.Contains(watcherId))
				watcherIdentity = m_Identities.Get(watcherId);
			if (!watcherIdentity)
				watcherIdentity = FindIdentity(watcherId);
			if (!watcherIdentity)
			{
				m_Targets.Remove(watcherId);
				continue;
			}
			Watch(watcherIdentity, NextAlive(player, 1));
		}
	}

	void OnIdentityDisconnected(PlayerIdentity identity)
	{
		if (!identity)
			return;

		m_Targets.Remove(identity.GetId());
		m_LastRequest.Remove(identity.GetId());
		m_Anchors.Remove(identity.GetId());
		m_Identities.Remove(identity.GetId());
	}

	//! Asked by the client: next (1) or previous (-1) alive player, or 0 to resend the current one
	void Cycle(PlayerIdentity identity, int direction)
	{
		if (!identity || !m_Targets.Contains(identity.GetId()))
			return;

		// One request every 0.4 s per spectator; the rest are dropped
		string id = identity.GetId();
		m_Identities.Set(id, identity);
		float now = GetGame().GetTickTime();
		if (m_LastRequest.Contains(id) && now - m_LastRequest.Get(id) < 0.4)
			return;
		m_LastRequest.Set(id, now);

		PlayerBase current = m_Targets.Get(id);
		if (direction == 0)
		{
			BattleRoyaleState state = BattleRoyaleServer.GetInstance().GetCurrentState();
			if (current && current.IsAlive() && state && state.ContainsPlayer(current))
				Watch(identity, current);
			else
				Watch(identity, NextAlive(current, 1));
			return;
		}

		if (direction > 0)
			direction = 1;
		else
			direction = -1;

		Watch(identity, NextAlive(current, direction));
	}

	protected void Start(string identityId)
	{
		PlayerIdentity identity = FindIdentity(identityId);
		if (!identity)
			return;
		m_Identities.Set(identityId, identity);

		// The player may have reconnected with a living character in the meantime
		PlayerBase own = PlayerBase.Cast(identity.GetPlayer());
		if (own && own.IsAlive())
			return;

		PlayerBase target = NextAlive(null, 1);
		if (!target)
			return;

		// The identity owns a new anchor body near the target, then spectates from a camera
		// Without an anchor the camera never receives the target: the client leaves after its death as before
		PlayerBase anchor = PlayerBase.Cast(GetGame().CreatePlayer(identity, "SurvivorM_Mirek", AnchorPosition(target), 0, "NONE"));
		if (!anchor)
		{
			BattleRoyaleUtils.Warn("Spectator anchor could not be created for " + identity.GetName());
			return;
		}

		anchor.SetAllowDamage(false);
		dBodyEnableGravity(anchor, false);
		m_Anchors.Set(identityId, anchor);
		GetGame().SelectPlayer(identity, anchor);

		GetGame().SelectPlayer(identity, null);
		GetGame().SelectSpectator(identity, "BattleRoyaleSpectatorCamera", HeadPosition(target));
		BattleRoyaleUtils.Info("Spectator started for " + identity.GetName());

		Watch(identity, target);
	}

	protected void Watch(PlayerIdentity identity, PlayerBase target)
	{
		if (!identity)
			return;

		string identityId = identity.GetId();
		m_Identities.Set(identityId, identity);

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

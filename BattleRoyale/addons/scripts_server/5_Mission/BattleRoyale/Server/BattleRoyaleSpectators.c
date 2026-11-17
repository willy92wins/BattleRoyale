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
	protected int m_SweepTicks;  //!< UpdateAnchors passes since the last sweep for disconnected spectators
	protected ref map<string, int> m_Sights;  //!< player identity id -> sights it last reported (EBRSpectateSights), NONE not kept
	protected ref map<string, float> m_SightsFov;  //!< player identity id -> optic field of view reported with them, radians
	protected ref map<string, bool> m_SightsUnderground;  //!< player identity id -> underground when it reported an optic (its night vision changes there)
	protected ref map<string, bool> m_SentSightsUnderground;  //!< spectator identity id -> underground flag last sent to it
	protected ref map<string, int> m_SentSights;  //!< spectator identity id -> sights last sent to it
	protected ref map<string, float> m_SentSightsFov;  //!< spectator identity id -> field of view last sent with them
	protected ref map<string, float> m_SightsWindowStart;  //!< player identity id -> tick time its current report window opened
	protected ref map<string, int> m_SightsWindowCount;  //!< player identity id -> reports taken in that window
	protected ref map<string, PlayerIdentity> m_SightsThrottled;  //!< player identity id -> player with a report dropped by its window, asked to report again once the window reopens

	//! Sights reports taken from one player per window: twice the most its client sends (one every 0.1 s), so network
	//! jitter never drops a report of a regular client
	protected static const float SIGHTS_WINDOW_SECONDS = 0.5;
	protected static const int SIGHTS_REPORTS_PER_WINDOW = 10;

	void BattleRoyaleSpectators()
	{
		m_Targets = new map<string, PlayerBase>;
		m_LastRequest = new map<string, float>;
		m_Anchors = new map<string, PlayerBase>;
		m_Identities = new map<string, PlayerIdentity>;
		m_Sights = new map<string, int>;
		m_SightsFov = new map<string, float>;
		m_SightsUnderground = new map<string, bool>;
		m_SentSightsUnderground = new map<string, bool>;
		m_SentSights = new map<string, int>;
		m_SentSightsFov = new map<string, float>;
		m_SightsWindowStart = new map<string, float>;
		m_SightsWindowCount = new map<string, int>;
		m_SightsThrottled = new map<string, PlayerIdentity>;

		GetRPCManager().AddRPC(RPC_DAYZBRSERVER_NAMESPACE, "SightsState", this);
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(UpdateAnchors, 1000, true);
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(RelaySights, 100, true);
	}

	void ~BattleRoyaleSpectators()
	{
		if (GetGame())
		{
			GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).Remove(UpdateAnchors);
			GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).Remove(RelaySights);
		}
	}

	//! Anchor bodies are not match players: the match logic leaves them alone.
	//! Called on every scheduled player tick, so it reads a flag kept in step with m_Anchors
	//! (SetAnchor, ClearAnchor) instead of scanning the map
	bool IsAnchor(PlayerBase player)
	{
		if (!player)
			return false;

		return player.br_spectator_anchor;
	}

	//! The only writer of m_Anchors entries: the body the entry held before stops being an anchor
	protected void SetAnchor(string identityId, PlayerBase anchor)
	{
		ClearAnchor(identityId);
		m_Anchors.Set(identityId, anchor);
		if (anchor)
			anchor.br_spectator_anchor = true;
	}

	//! The only remover of m_Anchors entries
	protected void ClearAnchor(string identityId)
	{
		PlayerBase previous = m_Anchors.Get(identityId);
		if (previous)
			previous.br_spectator_anchor = false;
		m_Anchors.Remove(identityId);
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

		// Every 10 passes, drop spectators the disconnect event never reached (it is discarded when the client has no body)
		m_SweepTicks++;
		if (m_SweepTicks >= 10)
		{
			m_SweepTicks = 0;
			SweepGone();
		}
	}

	//! Drops entries whose identity is no longer connected, or whose identity again owns a living character of its own
	protected void SweepGone()
	{
		map<string, PlayerIdentity> connected = new map<string, PlayerIdentity>;
		array<PlayerIdentity> identities = new array<PlayerIdentity>;
		GetGame().GetPlayerIndentities(identities);
		foreach (PlayerIdentity identity : identities)
		{
			if (identity)
				connected.Set(identity.GetId(), identity);
		}

		map<string, bool> candidateIds = new map<string, bool>;
		foreach (string anchorId, PlayerBase anchorBody : m_Anchors)
			candidateIds.Set(anchorId, true);
		foreach (string targetId, PlayerBase watched : m_Targets)
			candidateIds.Set(targetId, true);

		array<string> goneIds = new array<string>;
		foreach (string candidateId, bool candidateFlag : candidateIds)
		{
			PlayerIdentity connectedIdentity = null;
			if (connected.Contains(candidateId))
				connectedIdentity = connected.Get(candidateId);
			if (!connectedIdentity)
			{
				goneIds.Insert(candidateId);
				continue;
			}

			if (HasLivingCharacterOfItsOwn(connectedIdentity, candidateId))
				goneIds.Insert(candidateId);
		}

		foreach (string goneId : goneIds)
			Drop(goneId);

		// Sights reports of players gone without a disconnect event (no body at the time) go with them
		array<string> goneReporters = new array<string>;
		foreach (string reporterId, float windowStart : m_SightsWindowStart)
		{
			if (!connected.Contains(reporterId))
				goneReporters.Insert(reporterId);
		}
		foreach (string sightsId, int sightsMode : m_Sights)
		{
			if (!connected.Contains(sightsId) && goneReporters.Find(sightsId) == -1)
				goneReporters.Insert(sightsId);
		}
		foreach (string goneReporter : goneReporters)
			ForgetReports(goneReporter);
	}

	//! True when the identity again controls a living character that is not its anchor: its spectating entry is stale
	protected bool HasLivingCharacterOfItsOwn(PlayerIdentity identity, string identityId)
	{
		if (!identity)
			return false;

		PlayerBase own = PlayerBase.Cast(identity.GetPlayer());
		PlayerBase anchor = m_Anchors.Get(identityId);
		return own && own.IsAlive() && own != anchor;
	}

	//! Removes one spectator entry and deletes its anchor body; a reconnected identity keeps its own character
	protected void Drop(string identityId)
	{
		PlayerBase anchor = m_Anchors.Get(identityId);
		m_Targets.Remove(identityId);
		m_LastRequest.Remove(identityId);
		m_Identities.Remove(identityId);
		ForgetSent(identityId);
		ClearAnchor(identityId);
		if (anchor)
			GetGame().ObjectDelete(anchor);
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

		// Its sights go with it: a later life is watched from the eyes until its client reports new ones
		if (player.GetIdentity())
			ForgetSights(player.GetIdentity().GetId());

		array<string> watchers = new array<string>;
		foreach (string id, PlayerBase target : m_Targets)
		{
			if (target == player)
				watchers.Insert(id);
		}

		// The next alive player is the same for every watcher of this death: resolve it once
		PlayerBase next = NextAlive(player, 1);
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
			Watch(watcherIdentity, next);
		}
	}

	void OnIdentityDisconnected(PlayerIdentity identity)
	{
		if (!identity)
			return;

		// Serial matches: the anchor of a spectator who left is a leftover of the match; it keeps its anchor flag
		// until it is deleted, so the match logic still leaves it alone
		PlayerBase anchor = m_Anchors.Get(identity.GetId());
		if (anchor && BRMatchDebris.s_Enabled)
		{
			BRMatchDebris.Get().AddObject(anchor);
			m_Anchors.Remove(identity.GetId());
		}

		m_Targets.Remove(identity.GetId());
		m_LastRequest.Remove(identity.GetId());
		ClearAnchor(identity.GetId());
		m_Identities.Remove(identity.GetId());
		ForgetReports(identity.GetId());
		ForgetSent(identity.GetId());
	}

	//! Serial matches: true while this identity watches the match (it has an anchor or a target)
	bool IsSpectator(PlayerIdentity identity)
	{
		if (!identity)
			return false;

		string id = identity.GetId();
		return m_Anchors.Contains(id) || m_Targets.Contains(id);
	}

	//! Serial matches: the identity now controls its new lobby character; its anchor body goes away
	void Release(PlayerIdentity identity)
	{
		if (!identity)
			return;

		string id = identity.GetId();
		PlayerBase anchor = m_Anchors.Get(id);
		m_Anchors.Remove(id);
		m_Targets.Remove(id);
		m_LastRequest.Remove(id);
		m_Identities.Remove(id);
		ForgetSent(id);

		if (anchor)
			GetGame().ObjectDelete(anchor);
	}

	//! Serial matches: spectating scheduled for a death of the finished match must not start in the next lobby
	void CancelPending()
	{
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).Remove(Start);

		// The sights reported in the finished match do not carry over: the next one starts from the eyes
		m_Sights.Clear();
		m_SightsFov.Clear();
		m_SightsUnderground.Clear();
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

		// Nobody left to watch: the waiting room takes the player, when it is on
		PlayerBase target = NextAlive(null, 1);
		if (!target)
		{
			BattleRoyaleServer.GetInstance().MoveToWaitingRoom(identity);
			return;
		}

		// The identity owns a new anchor body near the target, then spectates from a camera
		// Without an anchor the camera never receives the target: the client leaves after its death as before
		PlayerBase anchor = PlayerBase.Cast(GetGame().CreatePlayer(identity, "SurvivorM_Mirek", AnchorPosition(target), 0, "NONE"));
		if (!anchor)
		{
			BattleRoyaleUtils.Warn("Spectator anchor could not be created for " + identity.GetName());
			BattleRoyaleServer.GetInstance().MoveToWaitingRoom(identity);
			return;
		}

		anchor.SetAllowDamage(false);
		dBodyEnableGravity(anchor, false);
		SetAnchor(identityId, anchor);
		GetGame().SelectPlayer(identity, anchor);

		GetGame().SelectPlayer(identity, null);
		GetGame().SelectSpectator(identity, "BattleRoyaleSpectatorCamera", HeadPosition(target));
		BattleRoyaleUtils.Info("Spectator started for " + identity.GetName());

		Watch(identity, target);

		// The spectator's map and walls keep showing the zones, including a closing under way
		BattleRoyaleServer server = BattleRoyaleServer.GetInstance();
		if (server)
			server.SendZoneStateTo(identity);
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

		// The watcher may have reconnected with a living character since its entry was made: drop the stale entry instead of putting that client into spectator mode
		if (HasLivingCharacterOfItsOwn(identity, identityId))
		{
			Drop(identityId);
			return;
		}

		GetRPCManager().SendRPC(RPC_DAYZBR_NAMESPACE, "SpectateTarget", new Param5<int, int, vector, string, int>(low, high, HeadPosition(target), name, alive), true, identity);

		// The watcher looks through the sights the watched player is already looking through
		SendSights(identity, target);
	}

	//! A player reports the sights it looks through whenever they change (its client sends at most ten reports a
	//! second). Only kept here; RelaySights passes them on, so the cost of passing them on follows the number of
	//! watchers and not the number of reports
	void SightsState(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
	{
		if (type != CallType.Server || !sender)
			return;

		// Only a living player looks through sights; a report from anyone else is dropped before it is read
		PlayerBase reporter = PlayerBase.Cast(sender.GetPlayer());
		if (!reporter || !reporter.IsAlive())
			return;

		// SIGHTS_REPORTS_PER_WINDOW reports every SIGHTS_WINDOW_SECONDS at most, twice what a client sends: the rest are
		// dropped before they are read
		string id = sender.GetId();
		float now = GetGame().GetTickTime();
		if (!m_SightsWindowStart.Contains(id) || now - m_SightsWindowStart.Get(id) >= SIGHTS_WINDOW_SECONDS)
		{
			m_SightsWindowStart.Set(id, now);
			m_SightsWindowCount.Set(id, 0);
		}
		// The dropped report may be the player's last change: RelaySights asks for its sights again once the window
		// reopens, so they never stay behind
		int count = m_SightsWindowCount.Get(id);
		if (count >= SIGHTS_REPORTS_PER_WINDOW)
		{
			m_SightsThrottled.Set(id, sender);
			return;
		}
		m_SightsWindowCount.Set(id, count + 1);

		Param3<int, float, bool> data;
		if (!ctx.Read(data))
			return;

		// A field of view only comes with an optic, and within what the optics configs use (0 < fov < 1 rad). Being
		// underground only matters to an optic's night vision, which the spectator's client cannot learn for a remote
		// player (its underground presence stays on its own client)
		int sights = data.param1;
		float fov = data.param2;
		bool underground = data.param3;
		if (sights < EBRSpectateSights.NONE || sights >= EBRSpectateSights.COUNT)
			return;
		if (sights != EBRSpectateSights.OPTICS || !(fov > 0 && fov < 1.0))
			fov = 0;
		if (sights != EBRSpectateSights.OPTICS)
			underground = false;

		if (sights == EBRSpectateSights.NONE)
		{
			ForgetSights(id);
			return;
		}

		m_Sights.Set(id, sights);
		m_SightsFov.Set(id, fov);
		m_SightsUnderground.Set(id, underground);
	}

	//! Ten times a second: each watcher whose watched player looks through other sights than the ones last sent to it
	//! gets them, and a player whose report was dropped is asked for its sights again. The cost follows the number of
	//! watchers and of dropped reports waiting, not the number of reports
	protected void RelaySights()
	{
		// A player whose report was dropped by its window reports again once the window reopens (SightsResend), if it
		// is still alive: a death forgets its sights anyway
		if (m_SightsThrottled.Count() > 0)
		{
			float now = GetGame().GetTickTime();
			array<string> reopened = new array<string>;
			foreach (string throttledId, PlayerIdentity throttled : m_SightsThrottled)
			{
				if (!m_SightsWindowStart.Contains(throttledId) || now - m_SightsWindowStart.Get(throttledId) >= SIGHTS_WINDOW_SECONDS)
					reopened.Insert(throttledId);
			}
			foreach (string reopenedId : reopened)
			{
				PlayerIdentity reporter = m_SightsThrottled.Get(reopenedId);
				m_SightsThrottled.Remove(reopenedId);
				if (!reporter)
					continue;

				PlayerBase reporterPlayer = PlayerBase.Cast(reporter.GetPlayer());
				if (reporterPlayer && reporterPlayer.IsAlive())
					GetRPCManager().SendRPC(RPC_DAYZBR_NAMESPACE, "SightsResend", NULL, true, reporter);
			}
		}

		foreach (string watcherId, PlayerBase watched : m_Targets)
		{
			if (!watched || !m_Identities.Contains(watcherId))
				continue;

			int sights;
			float fov;
			bool underground;
			GetSights(watched, sights, fov, underground);
			if (m_SentSights.Contains(watcherId) && m_SentSights.Get(watcherId) == sights && m_SentSightsFov.Get(watcherId) == fov && m_SentSightsUnderground.Get(watcherId) == underground)
				continue;

			SendSights(m_Identities.Get(watcherId), watched);
		}
	}

	//! The sights the watched player last reported, to one of its watchers; NONE when it reported none. They name the
	//! watched player, so a client never applies them to another one
	protected void SendSights(PlayerIdentity watcher, PlayerBase watched)
	{
		if (!watcher || !watched)
			return;

		int sights;
		float fov;
		bool underground;
		GetSights(watched, sights, fov, underground);

		int low, high;
		watched.GetNetworkID(low, high);

		string watcherId = watcher.GetId();
		m_SentSights.Set(watcherId, sights);
		m_SentSightsFov.Set(watcherId, fov);
		m_SentSightsUnderground.Set(watcherId, underground);
		GetRPCManager().SendRPC(RPC_DAYZBR_NAMESPACE, "SpectateSights", new Param5<int, float, bool, int, int>(sights, fov, underground, low, high), true, watcher);
	}

	protected void GetSights(PlayerBase player, out int sights, out float fov, out bool underground)
	{
		sights = EBRSpectateSights.NONE;
		fov = 0;
		underground = false;
		if (!player || !player.GetIdentity())
			return;

		string id = player.GetIdentity().GetId();
		if (!m_Sights.Contains(id))
			return;

		sights = m_Sights.Get(id);
		fov = m_SightsFov.Get(id);
		underground = m_SightsUnderground.Get(id);
	}

	//! The player's sights end (it left them, died or disconnected): a report waiting to be asked again ends with them
	protected void ForgetSights(string identityId)
	{
		m_Sights.Remove(identityId);
		m_SightsFov.Remove(identityId);
		m_SightsUnderground.Remove(identityId);
		m_SightsThrottled.Remove(identityId);
	}

	//! Everything kept for a player that reported its sights: the sights and its report window
	protected void ForgetReports(string identityId)
	{
		ForgetSights(identityId);
		m_SightsWindowStart.Remove(identityId);
		m_SightsWindowCount.Remove(identityId);
		m_SightsThrottled.Remove(identityId);
	}

	//! A spectator entry that ends: the next one starts with what Watch sends
	protected void ForgetSent(string identityId)
	{
		m_SentSights.Remove(identityId);
		m_SentSightsFov.Remove(identityId);
		m_SentSightsUnderground.Remove(identityId);
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

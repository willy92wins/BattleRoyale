#ifdef SERVER
class BattleRoyaleStartMatch: BattleRoyaleState
{
    protected int i_TimeToUnlock;
    protected bool b_IsGameplay;
    protected int i_FirstRoundDelay;
    protected int i_FirstZoneRevealDelay;
    protected int i_FirstZoneExtra;
    protected bool b_ShowFirstZone;
    protected bool b_ArtillerySound;
    protected bool b_EnablePlaneDeployment;

    protected ref array<PlayerBase> a_PlayerList;
    protected ref BattleRoyalePlayArea m_FirstArea;  //!< first zone once ShowFirstZone sent it
    protected ref BattleRoyalePlaneFlight m_PlaneFlight;
    protected int i_PlaneFirstZoneIndex;
    protected bool b_RevealScheduled;
    protected bool b_FirstZoneRevealed;
    protected int m_PlaneZoningDefers;
    protected float f_ZoningDeferStartClock;

    static const int BR_UNSTUCK_COOLDOWN_MS = 30000;  //!< per-player wait after an unstuck finishes, tuned by the owner
    static const float PLANE_ZONING_DEFER_CAP_S = 180.0;  //!< max seconds StartZoning may wait for airborne players before forcing first round

    protected ref array<ref Timer> m_MessageTimers;
    protected ref Timer m_UnlockTimer;
    protected ref Timer m_ShowFirstZone;
    protected ref Timer m_ZoneStartTimer;
    protected ref Timer m_ZoningDeferTimer;

    void BattleRoyaleStartMatch()
    {
        BattleRoyaleConfig m_Config = BattleRoyaleConfig.GetConfig();
        BattleRoyaleGameData m_GameSettings = m_Config.GetGameData();

        i_FirstRoundDelay = (60 * m_GameSettings.round_duration_minutes) / 2;
        i_FirstZoneRevealDelay = m_GameSettings.first_zone_reveal_delay_seconds;
        i_FirstZoneExtra = m_GameSettings.first_zone_extra_seconds;

        //seconds until unlock
#ifdef DIAG_DEVELOPER
        i_TimeToUnlock = 1;
#else
        i_TimeToUnlock = m_GameSettings.time_until_teleport_unlock;
#endif

        b_ShowFirstZone = m_GameSettings.show_first_zone_at_start;

        b_ArtillerySound = m_GameSettings.artillery_sound;

        b_EnablePlaneDeployment = m_GameSettings.enable_plane_deployment;

        b_IsGameplay = false;
        i_PlaneFirstZoneIndex = 0;
        b_RevealScheduled = false;
        b_FirstZoneRevealed = false;

        a_PlayerList = new array<PlayerBase>;

        m_MessageTimers = new array<ref Timer>;
    }

    override string GetName()
    {
        return "Start Match State";
    }

    override bool CanFreeze()
    {
        return true;
    }

    override bool IsInputLocked()
    {
        return IsActive() && !b_IsGameplay;
    }

    //! Players arrive locked: they take no damage until the unlock, so a client that got free cannot kill anyone
    override void AddPlayer(PlayerBase player)
    {
        if (player && !b_IsGameplay)
            player.SetAllowDamage(false);

        super.AddPlayer(player);
    }

    override int GetAdminPhaseEnum()
    {
        if (b_EnablePlaneDeployment && !b_FirstZoneRevealed)
            return EBRMatchPhase.AIRDROP;
        return EBRMatchPhase.STARTING;
    }

    override bool IsMatchDecided()
    {
        if (b_EnablePlaneDeployment && IsActive() && m_PlaneFlight && m_PlaneFlight.HoldsStartMatch())
            return false;
        return IsActive() && GetPlayers().Count() <= 1;
    }

    override int GetPhaseSecondsLeft()
    {
        if (b_EnablePlaneDeployment)
        {
            if (b_FirstZoneRevealed)
                return 0;
            if (m_PlaneFlight && m_PlaneFlight.HasActiveFlight())
                return Math.Ceil(m_PlaneFlight.GetRemainingFlightSeconds());
            if (m_ShowFirstZone && IsTimerActiveOrPaused(m_ShowFirstZone))
                return Math.Ceil(m_ShowFirstZone.GetRemaining());
            return -1;
        }
        if (m_UnlockTimer && IsTimerActiveOrPaused(m_UnlockTimer))
            return Math.Ceil(m_UnlockTimer.GetRemaining());
        return 0;
    }

    override int GetCountdownSecondsLeft()
    {
        if (m_ZoneStartTimer && IsTimerActiveOrPaused(m_ZoneStartTimer))
            return Math.Ceil(m_ZoneStartTimer.GetRemaining());
        return -1;
    }

    override int GetAdminTimeLeft()
    {
        if (b_EnablePlaneDeployment && !b_FirstZoneRevealed)
            return GetPhaseSecondsLeft();
        return GetCountdownSecondsLeft();
    }

    //! Estimated seconds remaining in StartMatch until the first round begins (waiting-room estimate).
    int GetEstimatedSecondsLeft()
    {
        if (!b_EnablePlaneDeployment)
        {
            int legacyCd = GetCountdownSecondsLeft();
            if (legacyCd > 0)
                return legacyCd;
            return i_FirstRoundDelay;
        }

        int firstZoneWait = i_FirstRoundDelay + i_FirstZoneExtra;
        if (!b_FirstZoneRevealed)
        {
            int preReveal = 0;
            if (m_PlaneFlight && m_PlaneFlight.HasActiveFlight())
                preReveal = Math.Ceil(m_PlaneFlight.GetRemainingFlightSeconds()) + i_FirstZoneRevealDelay;
            else if (m_ShowFirstZone && IsTimerActiveOrPaused(m_ShowFirstZone))
                preReveal = Math.Ceil(m_ShowFirstZone.GetRemaining());
            return preReveal + firstZoneWait;
        }

        int cd = GetCountdownSecondsLeft();
        if (cd > 0)
            return cd;
        return 0;
    }

    override protected void OnFreeze()
    {
        if (b_EnablePlaneDeployment && m_PlaneFlight)
            m_PlaneFlight.FreezeFlight();
    }

    override protected void OnUnfreeze(float frozenSeconds)
    {
        if (b_EnablePlaneDeployment && m_PlaneFlight)
            m_PlaneFlight.UnfreezeFlight(frozenSeconds);
    }

    override protected void SendResumeSync()
    {
        if (b_EnablePlaneDeployment && !b_FirstZoneRevealed)
        {
            if (m_PlaneFlight && m_PlaneFlight.HasActiveFlight())
                m_PlaneFlight.ResendFlightToRiders("resume");
            BroadcastMatchPhase(EBRMatchPhase.AIRDROP, GetPhaseSecondsLeft());
        }
        else
        {
            BroadcastMatchPhase(EBRMatchPhase.STARTING, GetPhaseSecondsLeft());
        }
        int cdSec = GetCountdownSecondsLeft();
        if (cdSec >= 0)
            GetRPCManager().SendRPC(RPC_DAYZBR_NAMESPACE, "SetCountdownSeconds", new Param1<int>(cdSec), true);
    }

    override bool HandleAdminNext()
    {
        if (!b_EnablePlaneDeployment || !IsActive() || b_FirstZoneRevealed)
            return false;

        int ejected = 0;
        if (m_PlaneFlight && m_PlaneFlight.HasActiveFlight())
            ejected = m_PlaneFlight.EjectAllWithParachute("admin_next");

        BattleRoyaleUtils.Info(string.Format("[Plane] Admin Next during airdrop: ejected=%1 (parachutes opened), revealing first zone now", ejected));
        ExecuteFirstZoneReveal("admin_next");
        return true;
    }

    override void Update(float timeslice)
    {
        super.Update(timeslice);
        if (b_EnablePlaneDeployment && IsActive() && m_PlaneFlight)
        {
            m_PlaneFlight.Tick(timeslice);
            if (!IsFrozen())
                CheckPlaneRevealSchedule();
        }
    }

    override void Activate()
    {
        super.Activate();
        RememberMatchPlayers();

        if (b_EnablePlaneDeployment)
        {
            BattleRoyaleGameData gameData = BattleRoyaleConfig.GetConfig().GetGameData();
            int startCount = i_NumStartingPlayers;
            if (startCount <= 0)
                startCount = m_Players.Count();
            i_PlaneFirstZoneIndex = GetDynamicStartingZone(startCount);
            BattleRoyaleZone firstZone = BattleRoyaleZone.GetZone(i_PlaneFirstZoneIndex);
            vector zoneCenter = "0 0 0";
            float zoneRadius = 0.0;
            if (firstZone && firstZone.GetArea())
            {
                zoneCenter = firstZone.GetArea().GetCenter();
                zoneRadius = firstZone.GetArea().GetRadius();
            }

            float worldSize = 0.0;
            if (GetGame() && GetGame().GetWorld())
                worldSize = GetGame().GetWorld().GetWorldSize();

            int seed = Math.RandomInt(1, int.MAX);
            BattleRoyaleFlightPath path = BattleRoyaleFlightPath.FromSeed(seed, worldSize, zoneCenter, zoneRadius, gameData.plane_speed_mps, gameData.plane_min_altitude_m, gameData.plane_max_altitude_m, gameData.plane_edge_margin_m, gameData.plane_path_max_zone_distance);
            int phaseSec = -1;
            if (path && path.m_DurationS > 0.0)
                phaseSec = Math.Ceil(path.m_DurationS);

            BroadcastMatchPhase(EBRMatchPhase.AIRDROP, phaseSec);
            GetRPCManager().SendRPC( RPC_DAYZBR_NAMESPACE, "StartMatch", new Param1<bool>(true), true);

            b_IsGameplay = true;
            b_RevealScheduled = false;
            b_FirstZoneRevealed = false;
            m_FirstArea = null;
            m_PlaneZoningDefers = 0;
            f_ZoningDeferStartClock = 0.0;
            m_PlaneFlight = new BattleRoyalePlaneFlight();
            m_PlaneFlight.StartFlight(path, gameData.parachute_auto_open_agl_m, m_Players);
            MessagePlayersUntranslated( "STR_BR_MATCH_STARTED" );

            CheckPlaneRevealSchedule();
            return;
        }

        BroadcastMatchPhase(EBRMatchPhase.STARTING, i_TimeToUnlock);

        //send start match RPC (this will enable UI such as kill count)
        GetRPCManager().SendRPC( RPC_DAYZBR_NAMESPACE, "StartMatch", new Param1<bool>(true), true); //don't need a param, but id rather keep it just so i know nothing wierd occurs (eventually find out if we can remove it)

        int max_time = i_TimeToUnlock - 1;
        for(int i = max_time; i > 0; i--)
        {
            m_MessageTimers.Insert( AddTimer(i, this, "MessageUnlock", new Param1<int>(i_TimeToUnlock - i), false) );
        }

        m_UnlockTimer = AddTimer(i_TimeToUnlock, this, "UnlockPlayers", NULL, false);

        if (b_ShowFirstZone)
            m_ShowFirstZone = AddTimer(i_TimeToUnlock, this, "ShowFirstZone", NULL, false);

        m_ZoneStartTimer = AddTimer( i_FirstRoundDelay, this, "StartZoning", NULL, false);

        //timer before first zone appears
        GetRPCManager().SendRPC( RPC_DAYZBR_NAMESPACE, "SetCountdownSeconds", new Param1<int>(i_FirstRoundDelay), true);
    }

    override void CancelPendingWork()
    {
        if (GetGame())
        {
            GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).RemoveByName(this, "Unstuck");
            GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).Remove(this.StartZoning);
        }
        if (m_ZoningDeferTimer && m_ZoningDeferTimer.IsRunning())
            m_ZoningDeferTimer.Stop();
        if (m_PlaneFlight && m_PlaneFlight.HasTrackedPlayers())
            m_PlaneFlight.CloseFlight("turnover", true);
    }

    override void Deactivate()
    {
        //! Cancel pending unstucks and deferred zoning callbacks so none fires after this state ends
        if (GetGame())
        {
            GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).RemoveByName(this, "Unstuck");
            GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).Remove(this.StartZoning);
        }

        if (b_EnablePlaneDeployment && m_PlaneFlight)
        {
            BattleRoyaleServer brServer = BattleRoyaleServer.GetInstance();
            bool aborted = brServer && brServer.IsAbortedByAdmin();
            bool matchEnded = aborted || GetPlayers().Count() <= 1;
#ifdef Carim
            if (GetGroupsCount() <= 1)
                matchEnded = true;
#endif
            if (m_PlaneFlight.HasActiveFlight() || (matchEnded && m_PlaneFlight.HasTrackedPlayers()))
            {
                string closeReason = "start_match_next";
                if (aborted)
                    closeReason = "admin_restart";
                else if (matchEnded)
                    closeReason = "start_match_end";
                m_PlaneFlight.CloseFlight(closeReason, matchEnded);
            }
        }

        foreach (PlayerBase pending_player: m_Players)
        {
            if (pending_player)
            {
                pending_player.wait_unstuck = false;
                pending_player.br_unstuck_repeat_notified = false;
                if (b_EnablePlaneDeployment && (pending_player.BR_IsPlaneFrozen() || pending_player.BR_IsPlaneHidden() || pending_player.BR_IsPlaneInRide()))
                {
                    pending_player.BR_SetPlaneInRide(false);
                    pending_player.BR_SetPlaneFrozen(false);
                    pending_player.BR_SetPlaneHidden(false);
                    pending_player.DisableInput(false);
                    if (pending_player.GetIdentity())
                        GetRPCManager().SendRPC(RPC_DAYZBR_NAMESPACE, "SetInput", new Param1<bool>(false), true, pending_player.GetIdentity());
                }
            }
        }

        //deactivate all one-time timers
        if ( m_UnlockTimer && m_UnlockTimer.IsRunning() )
        {
            m_UnlockTimer.Stop();

            // The match ends before the unlock: the players left get their input back
            GetRPCManager().SendRPC( RPC_DAYZBR_NAMESPACE, "SetInput", new Param1<bool>(false), true);
            foreach (PlayerBase locked: m_Players)
            {
                if (locked)
                {
                    locked.DisableInput(false);
                    locked.SetAllowDamage(true);
                }
            }
        }

        if ( m_ShowFirstZone && m_ShowFirstZone.IsRunning() )
            m_ShowFirstZone.Stop();

        if ( m_ZoneStartTimer && m_ZoneStartTimer.IsRunning() )
            m_ZoneStartTimer.Stop();

        if ( m_ZoningDeferTimer && m_ZoningDeferTimer.IsRunning() )
            m_ZoningDeferTimer.Stop();

        for(int i = 0; i < m_MessageTimers.Count(); i++)
        {
            if ( m_MessageTimers[i] && m_MessageTimers[i].IsRunning() )
                m_MessageTimers[i].Stop();
        }

        super.Deactivate();
    }

    override void RemovePlayer(PlayerBase player)
    {
        if (m_PlaneFlight && player)
        {
            m_PlaneFlight.OnPlayerRemoved(player);
            CheckPlaneRevealSchedule();
        }

        super.RemovePlayer(player);
    }

    void HandlePlaneJump(PlayerBase player)
    {
        if (b_EnablePlaneDeployment && IsActive() && !IsFrozen() && m_PlaneFlight)
        {
            if (m_PlaneFlight.HandleJumpRequest(player))
                CheckPlaneRevealSchedule();
        }
    }

    void HandlePlaneResync(PlayerBase player)
    {
        if (b_EnablePlaneDeployment && IsActive() && m_PlaneFlight)
            m_PlaneFlight.ResyncRider(player, "client_req");
    }

    bool HasActivePlaneFlight()
    {
        return b_EnablePlaneDeployment && IsActive() && m_PlaneFlight && m_PlaneFlight.HasActiveFlight();
    }

    bool IsPlayerInPlaneRide(PlayerBase player)
    {
        return m_PlaneFlight && m_PlaneFlight.IsPlayerInRide(player);
    }

    void ClosePlaneFlightForTurnover(string reason)
    {
        if (m_PlaneFlight && m_PlaneFlight.HasTrackedPlayers())
            m_PlaneFlight.CloseFlight(reason, true);
    }

    override bool IsComplete()
    {
        if (b_EnablePlaneDeployment && IsActive() && m_PlaneFlight && m_PlaneFlight.HoldsStartMatch())
            return false;

        if(GetPlayers().Count() <= 1 && IsActive())
        {
            BattleRoyaleUtils.Trace(GetName() + " IsComplete!");
            // TODO: clean call queue?
            // TODO: toggle to debug game
            Deactivate();
        }

        return super.IsComplete();
    }

	void MessageUnlock(int seconds_till)
	{
		LogPanelCallback("MessageUnlock");
		if(seconds_till > 1)
			MessagePlayersUntranslated("STR_BR_STARTING_IN_SECONDS", seconds_till.ToString());
		else
			MessagePlayersUntranslated("STR_BR_STARTING_IN_SECOND", seconds_till.ToString());
	}

    void UnlockPlayers()
    {
        LogPanelCallback("UnlockPlayers");
        a_PlayerList.InsertAll( m_Players );

        //enable player input on clients (we'll do this on server in another thread)
        GetRPCManager().SendRPC( RPC_DAYZBR_NAMESPACE, "SetInput", new Param1<bool>(false), true);

        GetGame().GameScript.Call(this, "HandleUnlock", NULL); //spin up unlocking thread
    }

    void ShowFirstZone()
    {
        LogPanelCallback("ShowFirstZone");
        // Show first circle
        BattleRoyaleUtils.Trace("[BattleRoyaleStartMatch] Show first circle");
        int zoneIndex;
        if (b_EnablePlaneDeployment && i_PlaneFirstZoneIndex > 0)
            zoneIndex = i_PlaneFirstZoneIndex;
        else
            zoneIndex = GetDynamicStartingZone(m_Players.Count());
        BattleRoyaleZone m_Zone = BattleRoyaleZone.GetZone(zoneIndex);
        if (!m_Zone)
            return;
        m_Zone.OnActivate( GetPlayers() ); //hand players over to the zone (for complex zone size/position calculation)
        BattleRoyalePlayArea m_ThisArea = m_Zone.GetArea();
        if (!m_ThisArea)
            return;

        BattleRoyaleUtils.Trace(m_ThisArea.GetCenter());
        BattleRoyaleUtils.Trace(m_ThisArea.GetRadius());

        GetRPCManager().SendRPC( RPC_DAYZBR_NAMESPACE, "UpdateFuturePlayArea", new Param3<vector, float, bool>( m_ThisArea.GetCenter(), m_ThisArea.GetRadius(), false ), true);
        m_FirstArea = m_ThisArea;
    }

    protected void NotifyFirstZoneRevealed()
    {
        if (!m_FirstArea)
            return;

        vector zoneCenter = m_FirstArea.GetCenter();
        zoneCenter[1] = 0;
        float zoneRadius = m_FirstArea.GetRadius();
        array<PlayerBase> players = GetPlayers();
        if (!players)
            return;

        for (int i = 0; i < players.Count(); i++)
        {
            PlayerBase player = players[i];
            if (!player || !player.IsAlive())
                continue;
            vector playerPos = player.GetPosition();
            playerPos[1] = 0;
            float dist = vector.Distance(playerPos, zoneCenter);
            if (dist > zoneRadius)
                MessagePlayerUntranslated(player, "STR_BR_NEW_ZONE_OUTSIDE");
            else
                MessagePlayerUntranslated(player, "STR_BR_NEW_ZONE_INSIDE");
        }
    }

    //! Arms the post-flight reveal delay once the flight has finished (all jumped or end of path).
    protected void CheckPlaneRevealSchedule()
    {
        if (!b_EnablePlaneDeployment || !IsActive() || IsFrozen() || b_RevealScheduled || b_FirstZoneRevealed)
            return;
        if (!m_PlaneFlight || !m_PlaneFlight.IsFlightFinished())
            return;

        b_RevealScheduled = true;
        if (i_FirstZoneRevealDelay <= 0)
        {
            ExecuteFirstZoneReveal("flight_end_immediate");
            return;
        }

        m_ShowFirstZone = AddTimer(i_FirstZoneRevealDelay, this, "RevealFirstZone", NULL, false);
        BroadcastMatchPhase(EBRMatchPhase.AIRDROP, i_FirstZoneRevealDelay);
        BattleRoyaleServer brServer = BattleRoyaleServer.GetInstance();
        if (brServer)
            brServer.MarkAdminStatusDirty();
        BattleRoyaleUtils.Info(string.Format("[Zone] Scheduled first zone reveal in %1 s after flight end", i_FirstZoneRevealDelay));
    }

    void RevealFirstZone()
    {
        LogPanelCallback("RevealFirstZone");
        ExecuteFirstZoneReveal("timer");
    }

    protected void ExecuteFirstZoneReveal(string reason)
    {
        if (!b_EnablePlaneDeployment || !IsActive() || b_FirstZoneRevealed)
            return;

        b_FirstZoneRevealed = true;
        b_RevealScheduled = true;

        if (m_ShowFirstZone && IsTimerActiveOrPaused(m_ShowFirstZone))
        {
            m_ShowFirstZone.Stop();
            RemoveTimer(m_ShowFirstZone);
        }

        if (b_ShowFirstZone)
        {
            ShowFirstZone();
            NotifyFirstZoneRevealed();
        }

        int firstZoneWait = i_FirstRoundDelay + i_FirstZoneExtra;
        if (firstZoneWait < 1)
            firstZoneWait = 1;

        BroadcastMatchPhase(EBRMatchPhase.STARTING, 0);
        m_ZoneStartTimer = AddTimer(firstZoneWait, this, "StartZoning", NULL, false);
        GetRPCManager().SendRPC(RPC_DAYZBR_NAMESPACE, "SetCountdownSeconds", new Param1<int>(firstZoneWait), true);

        BattleRoyaleServer brServer = BattleRoyaleServer.GetInstance();
        if (brServer)
            brServer.MarkAdminStatusDirty();

        int sinceFlightSec = 0;
        if (m_PlaneFlight)
        {
            float rawSince = m_PlaneFlight.GetSecondsSinceFlightFinished();
            if (rawSince > 0.0)
                sinceFlightSec = Math.Round(rawSince);
        }
        BattleRoyaleUtils.Info(string.Format("[Zone] First zone revealed: since_flight_end=%1 s countdown=%2 s reason=%3", sinceFlightSec, firstZoneWait, reason));
    }

    //! Zone state for a client that restarted its mission or joined late: the first zone once it is shown. A paused
    //! state stays the current one after StartZoning deactivates it, so it answers inactive too, as the rounds do
    void SendZoneStateTo(PlayerIdentity identity)
    {
        if (b_EnablePlaneDeployment && m_PlaneFlight && identity)
        {
            PlayerBase reconnected = PlayerBase.Cast(identity.GetPlayer());
            if (reconnected && m_PlaneFlight.IsPlayerInRide(reconnected))
                m_PlaneFlight.ResyncRider(reconnected, "reconnect");
        }

        if (!identity)
            return;

        if (m_FirstArea)
            GetRPCManager().SendRPC( RPC_DAYZBR_NAMESPACE, "UpdateFuturePlayArea", new Param3<vector, float, bool>( m_FirstArea.GetCenter(), m_FirstArea.GetRadius(), false ), true, identity);

        if (b_EnablePlaneDeployment && b_FirstZoneRevealed)
        {
            int cdSec = GetCountdownSecondsLeft();
            if (cdSec > 0)
                GetRPCManager().SendRPC(RPC_DAYZBR_NAMESPACE, "SetCountdownSeconds", new Param1<int>(cdSec), true, identity);
        }
    }

    void HandleUnlock()
    {
        BattleRoyaleUtils.Trace("HandleUnlock");
        for(int i = 0; i < a_PlayerList.Count(); i++)
        {
            PlayerBase player = a_PlayerList[i];
            // Damage comes back for a body without identity too (logging out), in case it reconnects
            if (player)
                player.SetAllowDamage(true);
            if (!player || !player.GetIdentity())
                continue;

            BattleRoyaleUtils.Trace("Unlock " + player.BR_GetPlayerName());

            player.DisableInput(false); //This will re-enable input
			//DayZPlayerSyncJunctures.SendPlayerUnconsciousness( player , false );
        }

        MessagePlayersUntranslated( "STR_BR_MATCH_STARTED" );
        // Unstuck is only served during this state, so the hint stays up only while it works
        int unstuck_window = i_FirstRoundDelay - i_TimeToUnlock;
        if (unstuck_window > 90)
            unstuck_window = 90;
        if (BattleRoyaleConfig.GetConfig().GetGameData().enable_unstuck && unstuck_window > 0)
            MessagePlayersUntranslatedTimed( "STR_BR_UNSTUCK_INFORMATION", unstuck_window );

        b_IsGameplay = true;
    }

    void StartZoning()
    {
        if (b_EnablePlaneDeployment)
        {
            if (!IsActive())
                return;

            int airborneCount = 0;
            if (m_PlaneFlight)
                airborneCount = m_PlaneFlight.GetActiveAirborneCount();

            if (m_PlaneFlight && m_PlaneFlight.HoldsStartMatch())
            {
                if (m_PlaneZoningDefers == 0)
                {
                    f_ZoningDeferStartClock = GetStateClock();
                    BattleRoyaleUtils.Info(string.Format("[Plane] Deferred StartZoning while players are still airborne (airborne=%1 cap=%2 s)", airborneCount, Math.Round(PLANE_ZONING_DEFER_CAP_S)));
                }
                float deferElapsed = GetStateClock() - f_ZoningDeferStartClock;
                if (deferElapsed < PLANE_ZONING_DEFER_CAP_S)
                {
                    m_PlaneZoningDefers = m_PlaneZoningDefers + 1;
                    if (m_ZoningDeferTimer)
                    {
                        m_ZoningDeferTimer.Stop();
                        RemoveTimer(m_ZoningDeferTimer);
                    }
                    m_ZoningDeferTimer = AddTimer(1.0, this, "StartZoning", NULL, false);
                    return;
                }
                BattleRoyaleUtils.Info(string.Format("[Zone] StartZoning airborne cap reached (%1 s); forcing first zone closing", Math.Round(PLANE_ZONING_DEFER_CAP_S)));
            }

            BattleRoyaleUtils.Info(string.Format("[Zone] Starting first zone closing (defers=%1 airborne=%2)", m_PlaneZoningDefers, airborneCount));
        }

        LogPanelCallback("StartZoning");
        Deactivate();
    }

    void DeferredUnstuck( PlayerBase player )
	{
		if( !player )
			return;

		if (b_EnablePlaneDeployment && m_PlaneFlight && m_PlaneFlight.IsUnstuckBlocked(player))
		{
			BattleRoyaleUtils.Debug("[Plane] Unstuck blocked (deferred) for " + player.BR_GetPlayerName());
			return;
		}

		//! Discard silently while this player's previous unstuck is still cooling down
		if( GetGame().GetTime() < player.br_unstuck_ready_at )
			return;

		if( !player.wait_unstuck )
		{
			player.wait_unstuck = true;
			player.br_unstuck_repeat_notified = false;
			MessagePlayerUntranslated( player, "STR_BR_UNSTUCK_TELEPORTATION" );
			BattleRoyaleUtils.Trace( player.BR_GetPlayerName() + " asked for an unstuck teleportation." );
			GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLaterByName(this, "Unstuck", Math.RandomFloat(1, 3) * 1000 , false, new Param1<PlayerBase>( player ));
		}
		else if( !player.br_unstuck_repeat_notified )
		{
			//! Tell the player only once per pending request
			player.br_unstuck_repeat_notified = true;
			MessagePlayerUntranslated( player, "STR_BR_ALREADY_REQUESTED_UNSTUCK" );
		}
	}

	void Unstuck( PlayerBase player )
	{
        if (!player)
            return;

        if (b_EnablePlaneDeployment && m_PlaneFlight && m_PlaneFlight.IsUnstuckBlocked(player))
        {
            player.wait_unstuck = false;
            player.br_unstuck_repeat_notified = false;
            BattleRoyaleUtils.Debug("[Plane] Unstuck blocked (exec) for " + player.BR_GetPlayerName());
            return;
        }

        //! A pending unstuck must not teleport outside this state, for a player who left it or died while waiting
        if (!player.GetIdentity() || !IsActive() || !ContainsPlayer(player) || !player.BR_IsAlive())
        {
            player.wait_unstuck = false;
            player.br_unstuck_repeat_notified = false;
            return;
        }

		bool unstuckSuccess = false;
		for(int search_pos = 0; search_pos < 50; search_pos++)
		{
			float radius, angle, x, z, y;
			vector player_position, playerDir, direction;

			radius = 10.0 + (search_pos * 0.5);
			angle = Math.RandomFloat(0, 360) * Math.DEG2RAD;
			player_position = player.GetPosition();
			x = player_position[0] + ( radius * Math.Cos(angle) );
			z = player_position[2] + ( radius * Math.Sin(angle) );
			y = GetGame().SurfaceY(x, z);

			if( IsSafeForTeleport(x, y, z, false) )
			{
				playerDir = vector.YawToVector( Math.RandomFloat(0, 360) );
				direction = Vector(playerDir[0], 0, playerDir[1]);

				ScriptJunctureData pCtx = new ScriptJunctureData;
				pCtx.Write( Vector(x, y, z) );
				pCtx.Write( direction );
				player.SendSyncJuncture( 88, pCtx );
				player.SetSynchDirty();
				player.wait_unstuck = false;
				player.br_unstuck_repeat_notified = false;
				//! The cooldown starts when the unstuck finishes, not when it was asked for
				player.br_unstuck_ready_at = GetGame().GetTime() + BR_UNSTUCK_COOLDOWN_MS;

				unstuckSuccess = true;
				break;
			}
		}

		if( !unstuckSuccess )
		{
			BattleRoyaleUtils.Warn( player.BR_GetPlayerName() + " unstuck failed at " + player.GetPosition() );
			player.wait_unstuck = false;
			player.br_unstuck_repeat_notified = false;
			//! The cooldown starts when the unstuck finishes, not when it was asked for
			player.br_unstuck_ready_at = GetGame().GetTime() + BR_UNSTUCK_COOLDOWN_MS;
		}
	}

    override void OnPlayerKilled(PlayerBase player, Object source)
    {
        if(!b_IsGameplay)
        {
            BattleRoyaleUtils.Info("Player killed before gameplay!");
            return;
        }

        super.OnPlayerKilled( player, source );
    }
}
#endif

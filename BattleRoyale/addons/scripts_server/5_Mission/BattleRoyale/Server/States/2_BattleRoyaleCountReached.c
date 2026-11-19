#ifdef SERVER
class BattleRoyaleCountReached: BattleRoyaleDebugState
{
    protected int i_TimeToStart;
    protected ref Timer m_StartTimer;

    void BattleRoyaleCountReached()
    {
        BattleRoyaleLobbyData m_DebugSettings = BattleRoyaleConfig.GetConfig().GetDebugData();
		i_TimeToStart = m_DebugSettings.time_to_start_match_seconds;
    }

    override string GetName()
    {
        return "Player Count Reached State";
    }

    override bool CanFreeze()
    {
        return true;
    }

    override bool CanAdjustTime()
    {
        return true;
    }

    override int GetAdminPhaseEnum()
    {
        return EBRMatchPhase.LOBBY;
    }

    override int GetPhaseSecondsLeft()
    {
        if (m_StartTimer && IsTimerActiveOrPaused(m_StartTimer))
            return Math.Ceil(m_StartTimer.GetRemaining());
        return -1;
    }

    override bool AdjustTime(int deltaSeconds, out int appliedDelta, out string reasonKey)
    {
        appliedDelta = 0;
        if (!IsActive() || !m_StartTimer || !IsTimerActiveOrPaused(m_StartTimer))
        {
            reasonKey = "STR_BR_PANEL_ERR_NO_TIMER";
            return false;
        }

        float rem = m_StartTimer.GetRemaining();
        float target = rem + deltaSeconds;
        if (deltaSeconds < 0)
        {
            if (rem <= 30.0)
            {
                reasonKey = "STR_BR_PANEL_ERR_TIME_FLOOR";
                return false;
            }
            if (target < 30.0)
                target = 30.0;
            if (Math.AbsFloat(target - rem) < 1.0)
            {
                reasonKey = "STR_BR_PANEL_ERR_TIME_FLOOR";
                return false;
            }
        }

        appliedDelta = Math.Round(target - rem);
        m_StartTimer.Stop();
        RemoveTimer(m_StartTimer);
        if (IsFrozen())
            m_StartTimer = AddPausedTimer(target, this, "DoStart", NULL, false);
        else
            m_StartTimer = AddTimer(target, this, "DoStart", NULL, false);

        int newSec = Math.Ceil(target);
        if (IsFrozen())
        {
            s_MatchPhaseSeconds = newSec;
            SendFrozenSnapshotTo(null);
        }
        else
        {
            BroadcastMatchPhase(EBRMatchPhase.LOBBY, newSec);
        }

#ifdef DIAG_DEVELOPER
        Print("[BRPanel] AdjustTime state=" + GetName() + " delta=" + appliedDelta.ToString() + " oldRem=" + rem.ToString() + " newRem=" + target.ToString());
#endif
        return true;
    }

    override protected void SendResumeSync()
    {
        BroadcastMatchPhase(EBRMatchPhase.LOBBY, GetPhaseSecondsLeft());
    }

    override void Activate()
    {
    	BattleRoyaleUtils.Debug(string.Format("BattleRoyaleCountReached: Activating with time to start: %1 seconds", i_TimeToStart));
        super.Activate();
        BroadcastMatchPhase(EBRMatchPhase.LOBBY, i_TimeToStart);

    	BattleRoyaleConfig m_Config = BattleRoyaleConfig.GetConfig();
        BattleRoyaleServerData m_ServerData = m_Config.GetServerData();
		if ( m_ServerData.enable_vigrid_api )
		{
			// Lock the server via webhook
			LockServerWebhook serverWebhook = new LockServerWebhook( m_ServerData.webhook_jwt_token );
			serverWebhook.LockServer();
		}

		if ( m_ServerData.use_autolock && m_ServerData.autolock_url != "" && m_ServerData.autolock_ip != "" && m_ServerData.autolock_port > 0 && m_ServerData.autolock_rcon_password != "" )
		{
			AutoLockWebhook autoLockWebhook = new AutoLockWebhook( m_ServerData.autolock_url, m_ServerData.autolock_ip, m_ServerData.autolock_port, m_ServerData.autolock_rcon_password );
			autoLockWebhook.LockServer();
		}

        string second = "seconds";
        if ( i_TimeToStart == 1 )
            second = "second";

        // Track the number of players at start
        i_NumStartingPlayers = m_Players.Count();

        MessagePlayersUntranslated("STR_BR_ANNOUNCEMENT_PLAYERCOUNTREACHED", i_TimeToStart.ToString(), second);
        // Players keep moving during the countdown: a client-only input lock without the
        // matching server lock desyncs movement (rubber banding and sliding in the lobby)
        m_StartTimer = AddTimer(i_TimeToStart, this, "DoStart", NULL, false);
    }

    override void Deactivate()
    {
        if ( m_StartTimer )
        {
            m_StartTimer.Stop();
        }
        
        super.Deactivate();
    }

    void DoStart()
    {
        LogPanelCallback("DoStart");
        Deactivate();
    }
}
#endif

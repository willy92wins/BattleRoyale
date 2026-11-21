#ifdef SERVER
class BattleRoyaleDebug: BattleRoyaleDebugState
{
    protected ref array<PlayerBase> m_ReadyList;

    protected int i_MinPlayers;
    protected int i_TimeBetweenMessages;
    protected bool b_UseVoteSystem;
    protected float f_VoteThreshold;
    protected float f_MinWaitingTime;
    protected float f_AutoStartDelay;
    protected bool b_AutoStartGame;
    protected float i_FirstPlayerTick;
    protected bool b_ForceStart;
    //! Tick time the minimum waiting time counts from: 0 (server start) for the first lobby, the lobby opening after a serial match
    protected float f_WaitOrigin = 0;

    void SetWaitOrigin(float tick_time)
    {
        f_WaitOrigin = tick_time;
    }

    protected float GetWaitingTime()
    {
        return GetStateClock() - f_WaitOrigin;
    }

    void BattleRoyaleDebug()
    {
        m_ReadyList = new array<PlayerBase>();

        BattleRoyaleLobbyData m_DebugSettings = BattleRoyaleConfig.GetConfig().GetDebugData();

		i_MinPlayers = m_DebugSettings.minimum_players;
		i_TimeBetweenMessages = 45;
		b_UseVoteSystem = (m_DebugSettings.use_ready_up == 1);
		f_VoteThreshold = m_DebugSettings.ready_up_percent;
		f_MinWaitingTime = m_DebugSettings.min_waiting_time;
		b_AutoStartGame = m_DebugSettings.autostart_enabled;
		f_AutoStartDelay = m_DebugSettings.autostart_delay;
    }

    override string GetName()
    {
        return "Debug Zone State";
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

    //! Seconds until the lobby ends with the players and votes it has now, -1 when it cannot end yet.
    //! Mirrors IsComplete (no vote system) and CheckReadyState (vote and autostart).
    override int GetAdminTimeLeft()
    {
#ifdef Carim
        //! Carim also needs a second group, and counting groups (here or through IsVoteReady) would change what
        //! IsComplete reads: no estimate
        return -1;
#endif
        float waitRem = f_MinWaitingTime - GetWaitingTime();
        if (waitRem < 0)
            waitRem = 0;

        //! Serial matches: the lobby waits until every player of the finished match is back in it
        BattleRoyaleServer server = BattleRoyaleServer.GetInstance();
        if (server && server.IsReturningPlayers())
            return -1;

        int players = GetPlayers().Count();
        if (!b_UseVoteSystem)
        {
            if (players < i_MinPlayers)
                return -1;
            return Math.Ceil(waitRem);
        }

        if (players <= 1)
            return -1;

        float best = -1;
        if (IsVoteReady())
            best = waitRem;

        int readyCount = GetReadyCount();
        int maxPlayers = GetGame().ServerConfigGetInt("maxPlayers");
        if (b_AutoStartGame && readyCount > 1 && maxPlayers > 0 && f_AutoStartDelay > 0)
        {
            //! Autostart when readyCount >= maxPlayers * (1 - elapsed / delay), checked after the minimum wait
            float autoAt = i_FirstPlayerTick + f_AutoStartDelay * (1.0 - (readyCount * 1.0) / maxPlayers);
            float autoRem = autoAt - GetStateClock();
            if (autoRem < waitRem)
                autoRem = waitRem;
            if (best < 0 || autoRem < best)
                best = autoRem;
        }

        if (best < 0)
            return -1;
        return Math.Ceil(best);
    }

    override int GetPhaseSecondsLeft()
    {
        float rem = f_MinWaitingTime - GetWaitingTime();
        if (rem > 0)
            return Math.Ceil(rem);
        return -1;
    }

    override bool AdjustTime(int deltaSeconds, out int appliedDelta, out string reasonKey)
    {
        appliedDelta = 0;
        if (!IsActive())
        {
            reasonKey = "STR_BR_PANEL_ERR_WRONG_PHASE";
            return false;
        }

        float currentWait = GetWaitingTime();
        float currentRem = f_MinWaitingTime - currentWait;
        if (currentRem < 0)
            currentRem = 0;

        float targetRem = currentRem + deltaSeconds;
        if (deltaSeconds < 0)
        {
            if (currentRem <= 30.0)
            {
                reasonKey = "STR_BR_PANEL_ERR_TIME_FLOOR";
                return false;
            }
            if (targetRem < 30.0)
                targetRem = 30.0;
            if (Math.AbsFloat(targetRem - currentRem) < 1.0)
            {
                reasonKey = "STR_BR_PANEL_ERR_TIME_FLOOR";
                return false;
            }
        }

        appliedDelta = Math.Round(targetRem - currentRem);
        f_MinWaitingTime = currentWait + targetRem;
        int newSec = GetPhaseSecondsLeft();
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
        Print("[BRPanel] AdjustTime state=" + GetName() + " delta=" + appliedDelta.ToString() + " oldRem=" + currentRem.ToString() + " newRem=" + targetRem.ToString());
#endif
        return true;
    }

    override protected void SendResumeSync()
    {
        BroadcastMatchPhase(EBRMatchPhase.LOBBY, GetPhaseSecondsLeft());
    }

    //returns true when this state is complete
    override bool IsComplete()
    {
        // Serial matches: the lobby waits until every player of the finished match is back in it
        BattleRoyaleServer server = BattleRoyaleServer.GetInstance();
        if (server && server.IsReturningPlayers())
            return false;

        if (IsActive() && b_ForceStart)
        {
            if (GetPlayers().Count() >= 1)
                Deactivate();
            else
                b_ForceStart = false;
        }

#ifdef Carim
        if( IsActive() && !b_UseVoteSystem && GetPlayers().Count() >= i_MinPlayers && GetWaitingTime() >= f_MinWaitingTime && GetGroupsCount() > 1 )
#else
        if( IsActive() && !b_UseVoteSystem && GetPlayers().Count() >= i_MinPlayers && GetWaitingTime() >= f_MinWaitingTime )
#endif
        {
            Deactivate();
        }
        
        return super.IsComplete();
    }

    override void AddPlayer(PlayerBase player)
    {
        super.AddPlayer( player );

		// Track the first connected player state clock to determine when to start the game
        if( GetPlayers().Count() == 1 )
        	i_FirstPlayerTick = GetStateClock();
    }

    override void Activate()
    {
        b_ForceStart = false;
        //these loop & will be automatically cleaned up on Deactivation
        AddTimer(i_TimeBetweenMessages, this, "MessageWaiting", NULL, true);

        if( b_UseVoteSystem )
        	AddTimer(2.0, this, "CheckReadyState", NULL, true);

        super.Activate();
        BroadcastMatchPhase(EBRMatchPhase.LOBBY, -1);
    }

    bool ForceStart(out string reasonKey)
    {
        if (!IsActive())
        {
            reasonKey = "STR_BR_PANEL_ERR_WRONG_PHASE";
            return false;
        }

        if (GetPlayers().Count() < 1)
        {
            reasonKey = "STR_BR_PANEL_ERR_NO_PLAYERS";
            return false;
        }

        BattleRoyaleServer server = BattleRoyaleServer.GetInstance();
        if (server && server.IsReturningPlayers())
        {
            reasonKey = "STR_BR_PANEL_ERR_RETURNING";
            return false;
        }

        if (IsFrozen())
            Unfreeze(true, true);

        b_ForceStart = true;
        return true;
    }

    void CheckReadyState()
    {
        LogPanelCallback("CheckReadyState");
        BattleRoyaleServer server = BattleRoyaleServer.GetInstance();
        if (server && server.IsReturningPlayers())
            return;

		if( GetWaitingTime() >= f_MinWaitingTime && GetPlayers().Count() > 1 )
		{
			if( IsActive() && IsVoteReady() )
				Deactivate();

			int t_MaxPlayers = GetGame().ServerConfigGetInt( "maxPlayers" );
			if( b_AutoStartGame && GetReadyCount() > 1 && GetReadyCount() >= ( t_MaxPlayers - ( ( t_MaxPlayers * ( GetStateClock() - i_FirstPlayerTick ) ) / f_AutoStartDelay ) ) )
				Deactivate();
		}
    }

    void MessageWaiting()
    {
        LogPanelCallback("MessageWaiting");
		int waiting_on_count = i_MinPlayers - GetPlayers().Count();

		if( waiting_on_count > 0)
		{
			if( waiting_on_count == 1 )
				MessagePlayersUntranslated("STR_BR_WAITING_FOR_PLAYER");
			else
				MessagePlayersUntranslated("STR_BR_WAITING_FOR_PLAYERS", waiting_on_count.ToString());
		}

		if( b_UseVoteSystem )
		{
			// TODO: Move that to client side
			int ready_count = GetReadyCount();
			if( ready_count == 1 )
				MessagePlayersUntranslated("STR_BR_PLAYER_READY_UP");
			else
				MessagePlayersUntranslated("STR_BR_PLAYERS_READY_UP", ready_count.ToString());

			if( GetWaitingTime() < f_MinWaitingTime )
			{
				int seconds_left = Math.Ceil( f_MinWaitingTime - GetWaitingTime() );

				if( !IsVoteReady() )
					MessagePlayersUntranslated("STR_BR_CANNOT_START_BEFORE_SECOND", seconds_left.ToString());
				else
					MessagePlayersUntranslated("STR_BR_GAME_WILL_AUTO_START_IN", seconds_left.ToString());
			}
		}
    }

    int GetReadyCount()
    {
        int ready_count = 0;
        for(int a = 0; a < m_ReadyList.Count(); a++)
        {
            if(m_ReadyList[a])
                ready_count++;
        }

        return ready_count;
    }

    bool IsVoteReady()
    {
        if(!b_UseVoteSystem)
            return false;

        int ready_count = GetReadyCount();
        int player_count = GetPlayers().Count();

		if( player_count <= 1 ) // need more than 1 player
			return false;

		if( player_count <= i_MinPlayers ) // need more than the minimum player
			return false;

#ifdef Carim
		if( GetGroupsCount() <= 1 ) // need more than one group
			return false;
#endif

        float percent = ready_count / (player_count * 1.0);
        return (percent >= f_VoteThreshold);
    }

    void ReadyUp(PlayerBase player)
    {
        if(m_ReadyList.Find(player) != -1)
        {
            MessagePlayerUntranslated(player, "STR_BR_YOU_ALREADY_READIED_UP");
            return;
        }

        MessagePlayerUntranslated(player, "STR_BR_YOU_READIED_UP");
        m_ReadyList.Insert( player );

        //this is here because we don't want someone mass spamming all players by spamming F1
        int count = GetReadyCount();
        int max = GetPlayers().Count();
        MessagePlayersUntranslated("STR_BR_PLAYER_READIED_UP", count.ToString(), max.ToString());
    }

    override void RemovePlayer(PlayerBase player)
    {
        super.RemovePlayer( player );
        
        m_ReadyList.RemoveItem( player );
    }

    override ref array<PlayerBase> RemoveAllPlayers()
    {
        m_ReadyList.Clear();
        ref array<PlayerBase> players = super.RemoveAllPlayers();
        return players;
    }
}
#endif

#ifdef SERVER
class BattleRoyaleLastRound: BattleRoyaleState
{
    ref BattleRoyaleState m_PreviousState;

    int i_RoundTimeInSeconds;
    bool b_DoZoneDamage;
    int i_DamageTickTime;
    float f_Damage;

    array<int> lock_notif_min;
    array<int> lock_notif_sec;

    bool b_IsZoneLocked;

    //! The final zone shrinks to nothing on its center (zone_settings.json zone_shrink_speed), ending when it collapses today
    protected float f_ShrinkSpeed;
    protected float f_ShrinkDuration;  //!< seconds, 0 when the final zone does not close
    protected float f_ShrinkStartTime;  //!< GetGame().GetTickTime() when the closing started
    protected bool b_Shrinking;  //!< from the start of the closing until the collapse

    protected ref Timer m_FinalZoneLockTimer;
    protected ref Timer m_ShrinkTimer;
    protected ref array<ref Timer> m_MessageTimers;

    void BattleRoyaleLastRound(BattleRoyaleState previous_state)
    {
        m_PreviousState = previous_state;

        BattleRoyaleConfig m_Config = BattleRoyaleConfig.GetConfig();
        BattleRoyaleGameData m_GameSettings = m_Config.GetGameData();

        lock_notif_min =  m_GameSettings.zone_notification_minutes;
        lock_notif_sec =  m_GameSettings.zone_notification_seconds;

        i_DamageTickTime = m_GameSettings.zone_damage_tick_seconds;
        f_Damage = m_GameSettings.zone_damage_delta;
        b_DoZoneDamage = m_GameSettings.enable_zone_damage;
        b_IsZoneLocked = false;
        b_Shrinking = false;
        f_ShrinkDuration = 0;
        f_ShrinkStartTime = 0;

        m_MessageTimers = new array<ref Timer>;

        BattleRoyaleZoneData m_ZoneSettings = m_Config.GetZoneData();
        f_ShrinkSpeed = m_ZoneSettings.zone_shrink_speed;
        if (m_ZoneSettings.shrink_type == 3)
        {
            i_RoundTimeInSeconds = m_ZoneSettings.static_timers[0];
        } else {
            i_RoundTimeInSeconds = 60 * m_GameSettings.round_duration_minutes;
        }
    }

    override void Activate()
    {
        int time_till_lock = (i_RoundTimeInSeconds * 1000) / 2;

        // The final zone closes from its radius to nothing and is gone when it collapses today
        int time_till_shrink = time_till_lock;
        float shrink_seconds = GetShrinkDuration();
        if (shrink_seconds > 0)
        {
            int shrink_ms = Math.Ceil(shrink_seconds * 1000);
            time_till_shrink = time_till_lock - shrink_ms;
            if (time_till_shrink < 0)
            {
                // The closing does not fit before the usual collapse: it starts with the round and the collapse comes
                // just as much later, so the edge still moves at zone_shrink_speed
                time_till_shrink = 0;
                time_till_lock = shrink_ms;
            }
            f_ShrinkDuration = (time_till_lock - time_till_shrink) / 1000.0;
        }

        // With a closing the danger starts when the final zone starts closing: the notices count down to that moment
        string notify_minutes = "NotifyTimeToEndMinutes";
        string notify_seconds = "NotifyTimeToEndSeconds";
        if (f_ShrinkDuration > 0)
        {
            notify_minutes = "NotifyTimeTillShrinkMinutes";
            notify_seconds = "NotifyTimeTillShrinkSeconds";
        }

        int i;
        int min;
        int sec;
        int val;
        //--- notification message timers
        for(i = 0; i < lock_notif_min.Count();i++)
        {
            min = lock_notif_min[i];
            val = time_till_shrink - (min*60*1000);
            if(val > 0)
                m_MessageTimers.Insert( AddTimer(val / 1000.0, this, notify_minutes, new Param1<int>( min ), false) ); //we need to store the object in case it's automatically deconstructed ?
        }
        for(i = 0; i < lock_notif_sec.Count();i++)
        {
            sec = lock_notif_sec[i];
            val = time_till_shrink - (sec*1000);
            if(val > 0)
                m_MessageTimers.Insert( AddTimer(val / 1000.0, this, notify_seconds, new Param1<int>( sec ), false) ); //we need to store the object in case it's automatically deconstructed ?
        }

        //timer before the final zone is gone (ZONE DISAPPEARS IN); during the closing the client counts the same moment down
        GetRPCManager().SendRPC( RPC_DAYZBR_NAMESPACE, "SetCountdownSeconds", new Param1<int>( time_till_lock / 1000 ), true);

        //closing start event; a closing that starts with the round is sent at the end of Activate, after the areas
        if (f_ShrinkDuration > 0 && time_till_shrink > 0)
            m_ShrinkTimer = AddTimer( time_till_shrink / 1000.0, this, "StartShrink", NULL, false);

        //lock zone event
        m_FinalZoneLockTimer = AddTimer( time_till_lock / 1000.0, this, "LockFinalZone", NULL, false);

        // Last round: everyone connected learns now when the server will restart after this match
        BattleRoyaleServer server = BattleRoyaleServer.GetInstance();
        if (server && server.WillRestartAfterMatch())
            MessagePlayersUntranslated("STR_BR_RESTART_AFTER_MATCH");

        //send play area to clients
        ref BattleRoyalePlayArea m_PreviousArea = NULL;
        if(GetPreviousZone())
            m_PreviousArea = GetPreviousZone().GetArea();

        //tell client the current play has not changed (note that if this is the first round, then the current area will be NULL )
        if (m_PreviousArea)
            GetRPCManager().SendRPC( RPC_DAYZBR_NAMESPACE, "UpdateCurrentPlayArea", new Param2<vector, float>( m_PreviousArea.GetCenter(), m_PreviousArea.GetRadius() ), true);
        else
            Error("Last round has no previous zone; current play area was not sent");

        //tell the client the future zone is NULL (no future zone)
        GetRPCManager().SendRPC( RPC_DAYZBR_NAMESPACE, "UpdateFuturePlayArea", new Param3<vector, float, bool>( "0 0 0", 0.0, false ), true);

        // A closing that does not fit before the usual collapse starts now, after the areas sent above
        if (f_ShrinkDuration > 0 && time_till_shrink <= 0)
            StartShrink();

        super.Activate();
        // Seconds until the final zone locks (half the round, later when its closing does not fit before)
        BroadcastMatchPhase(EBRMatchPhase.LAST_ROUND, time_till_lock / 1000);
    }

    override void Deactivate()
    {
        if ( m_ShrinkTimer && m_ShrinkTimer.IsRunning() )
            m_ShrinkTimer.Stop();

        if ( m_FinalZoneLockTimer && m_FinalZoneLockTimer.IsRunning() )
        {
            m_FinalZoneLockTimer.Stop();
        }

        for(int i = 0; i < m_MessageTimers.Count(); i++)
        {
            if ( m_MessageTimers[i] && m_MessageTimers[i].IsRunning() )
            {
                m_MessageTimers[i].Stop();
            }
        }

        //we just deactivated this round (players not yet transfered from previous state)
        super.Deactivate();
    }

    override bool SkipState(BattleRoyaleState _previousState)
    {
        //only one (or less) players remaining, must skip to win state
        if(_previousState.GetPlayers().Count() > 1)
            return false;

#ifdef Carim
        if(_previousState.GetGroupsCount() > 1)
            return false;
#endif

        return true;
    }

	override string GetName()
	{
		return "Last Gameplay State";
	}

	override bool IsComplete() //return true when this state is complete & ready to transfer to the next state
	{
		if(IsActive())
		{
#ifdef Carim
			if(GetPlayers().Count() <= 1 || GetGroupsCount() <= 1)
#else
			if(GetPlayers().Count() <= 1)
#endif
				Deactivate();
		}

		return super.IsComplete();
	}

	override void OnPlayerKilled(PlayerBase player, Object source)
	{
		super.OnPlayerKilled( player, source );
	}

    override void OnPlayerTick(PlayerBase player, float timeslice)
    {
        //determine if the player needs to take damage
        //! The final lock only forces damage when zone damage is enabled, same as the normal rounds
        bool do_damage = b_DoZoneDamage && b_IsZoneLocked;
        if(!do_damage)
        {
            BattleRoyaleZone current_zone = GetPreviousZone();
            if(b_DoZoneDamage && current_zone && current_zone.GetArea())
            {
                float radius = current_zone.GetArea().GetRadius();
                vector center = current_zone.GetArea().GetCenter();

                // While the final zone closes its radius of this instant applies, on the same center
                if (b_Shrinking)
                    radius = GetShrinkRadius(GetGame().GetTickTime());

                vector playerPos = player.GetPosition();

                //distance needs to be done in 2D, not 3D, set Z coord to 0 so this can be done
                playerPos[1] = 0;
                center[1] = 0;
                float distance = vector.Distance(playerPos, center);
                if(distance >= radius)
                {
                    do_damage = true;
                }
            }
        }

        if(do_damage)
        {
            if(player.time_until_damage <= 0)
            {
                //DAMAGE
                player.GetSymptomManager().QueueUpPrimarySymptom(SymptomIDs.SYMPTOM_PAIN_HEAVY);
                player.BR_ZoneDamage( f_Damage );
                player.time_until_damage = i_DamageTickTime; //reset timer
            }
            player.time_until_damage -= timeslice;
        }
        else
        {
            player.time_until_damage = Math.Min(i_DamageTickTime, player.time_until_damage + timeslice);
        }

        super.OnPlayerTick(player, timeslice);
    }

    bool IsLocked()
    {
        return b_IsZoneLocked;
    }

    void LockFinalZone()
    {
        //TODO: this doesn't fucking work | if zone is null, no damage occurs
        GetRPCManager().SendRPC( RPC_DAYZBR_NAMESPACE, "UpdateCurrentPlayArea", new Param2<vector, float>( "0 0 0", 0.0 ), true);
        GetRPCManager().SendRPC( RPC_DAYZBR_NAMESPACE, "UpdateFuturePlayArea", new Param3<vector, float, bool>( "0 0 0", 0.0, false ), true);
        b_IsZoneLocked = true;
        b_Shrinking = false;
    }

    //! Seconds the final zone takes to shrink to nothing at zone_shrink_speed, 0 for no closing
    protected float GetShrinkDuration()
    {
        if (f_ShrinkSpeed <= 0)
            return 0;

        BattleRoyaleZone final_zone = GetPreviousZone();
        if (!final_zone || !final_zone.GetArea() || final_zone.GetArea().GetRadius() <= 0)
            return 0;

        return final_zone.GetArea().GetRadius() / f_ShrinkSpeed;
    }

    //! The final zone starts closing: from now on the damage follows the radius of the instant, and every client draws it
    void StartShrink()
    {
        BattleRoyaleZone final_zone = GetPreviousZone();
        if (b_IsZoneLocked || f_ShrinkDuration <= 0 || !final_zone || !final_zone.GetArea())
            return;

        b_Shrinking = true;
        f_ShrinkStartTime = GetGame().GetTickTime();
        BattleRoyaleUtils.Trace(GetName() + " closing starts, " + f_ShrinkDuration + " seconds");
        SendShrinkTo(null);
    }

    //! Radius of the closing final zone at tick time now
    protected float GetShrinkRadius(float now)
    {
        float radius = GetPreviousZone().GetArea().GetRadius();
        if (f_ShrinkDuration <= 0)
            return radius;

        float s = Math.Clamp((now - f_ShrinkStartTime) / f_ShrinkDuration, 0, 1);
        return radius * (1 - s);
    }

    //! StartZoneShrink: the final zone to radius 0 on its center, total duration and seconds already gone; null identity sends to everyone
    protected void SendShrinkTo(PlayerIdentity identity)
    {
        BattleRoyalePlayArea final_area = GetPreviousZone().GetArea();
        float elapsed = Math.Clamp(GetGame().GetTickTime() - f_ShrinkStartTime, 0, f_ShrinkDuration);
        GetRPCManager().SendRPC( RPC_DAYZBR_NAMESPACE, "StartZoneShrink", new Param6<vector, float, vector, float, float, float>( final_area.GetCenter(), final_area.GetRadius(), final_area.GetCenter(), 0.0, f_ShrinkDuration, elapsed ), true, identity);
    }

    //! Zone state of the last round for a client that restarted its mission or joined late: the final zone, or the
    //! collapse once it happened, no announced zone, and the closing under way with the time already gone
    void SendZoneStateTo(PlayerIdentity identity)
    {
        if (!identity || !IsActive())
            return;

        if (b_IsZoneLocked)
        {
            // Same pair as LockFinalZone
            GetRPCManager().SendRPC( RPC_DAYZBR_NAMESPACE, "UpdateCurrentPlayArea", new Param2<vector, float>( "0 0 0", 0.0 ), true, identity);
            GetRPCManager().SendRPC( RPC_DAYZBR_NAMESPACE, "UpdateFuturePlayArea", new Param3<vector, float, bool>( "0 0 0", 0.0, false ), true, identity);
            return;
        }

        BattleRoyaleZone final_zone = GetPreviousZone();
        if (final_zone && final_zone.GetArea())
            GetRPCManager().SendRPC( RPC_DAYZBR_NAMESPACE, "UpdateCurrentPlayArea", new Param2<vector, float>( final_zone.GetArea().GetCenter(), final_zone.GetArea().GetRadius() ), true, identity);

        GetRPCManager().SendRPC( RPC_DAYZBR_NAMESPACE, "UpdateFuturePlayArea", new Param3<vector, float, bool>( "0 0 0", 0.0, false ), true, identity);

        // After the zone in force, which ends a closing on the client
        if (b_Shrinking)
            SendShrinkTo(identity);
    }

    void NotifyTimeTillShrinkSeconds(int seconds)
    {
        if(seconds > 1)
            MessagePlayersUntranslated("STR_BR_ZONE_STARTS_CLOSING_IN_SECONDS", seconds.ToString());
        else
            MessagePlayersUntranslated("STR_BR_ZONE_STARTS_CLOSING_IN_SECOND", seconds.ToString());
    }

    void NotifyTimeTillShrinkMinutes(int minutes)
    {
        if(minutes > 1)
            MessagePlayersUntranslated("STR_BR_ZONE_STARTS_CLOSING_IN_MINUTES", minutes.ToString());
        else
            MessagePlayersUntranslated("STR_BR_ZONE_STARTS_CLOSING_IN_MINUTE", minutes.ToString());
    }

    BattleRoyaleZone GetPreviousZone()
    {
        BattleRoyaleRound prev_round;
        if(Class.CastTo(prev_round, m_PreviousState))
        {
            return prev_round.GetZone();
        }

        return NULL;
    }

    //TODO: both of these need added to battleroyaleconstants & use string replace to append minutes.ToString()
    void NotifyTimeToEndMinutes(int minutes)
    {
		MessagePlayersUntranslated("STR_BR_ZONE_WILL_DISAPPEAR_MINUTE", minutes.ToString());
    }

    void NotifyTimeToEndSeconds(int seconds)
    {
		MessagePlayersUntranslated("STR_BR_ZONE_WILL_DISAPPEAR_SECOND", seconds.ToString());
    }
}
#endif

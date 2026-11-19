#ifdef SERVER
class BattleRoyaleRound: BattleRoyaleState
{
    ref BattleRoyaleState m_PreviousState;
    ref BattleRoyaleZone m_Zone;
    int i_RoundTimeInSeconds;
    bool b_ZoneLocked;
    bool b_DoZoneDamage;
    int i_DamageTickTime;
    float f_Damage;
    float zone_num;
    int i_NumZones;
    bool b_ArtillerySound;
    array<int> lock_notif_min;
    array<int> lock_notif_sec;

    //! Closing from the previous zone to this one (zone_settings.json zone_shrink_speed); it ends at the lock
    protected float f_ShrinkSpeed;
    protected float f_ShrinkDuration;  //!< seconds, 0 when this round has no closing
    //! GetGame().GetTickTime() at which the closing is due to start, set at the activation like the lock timer: a late
    //! start timer does not move it, so the circle still reaches the new zone when the zone locks
    protected float f_ShrinkStartTime;
    protected bool b_Shrinking;  //!< from the start of the closing until the lock
    protected bool b_ShrinkStopped;  //!< the round ended during its closing: the circle stays at f_ShrinkStopTime
    protected float f_ShrinkStopTime;
    protected bool b_HasCustomShrinkFrom;  //!< set when AdjustTime re-anchors an active shrink to the current circle
    protected vector v_ShrinkFromCenter;
    protected float f_ShrinkFromRadius;

#ifdef Carim
    int i_MaxPartySize;
#endif

    protected ref array<ref Timer> m_MessageTimers;
    protected ref Timer m_NewZoneLockTimer;
    protected ref Timer m_RoundTimeUpTimer;
    protected ref Timer m_ShrinkTimer;

    //If this is NULL, we assume previous state is debug
    //a battle royale round represents a playing state with a play area
    void BattleRoyaleRound(BattleRoyaleState previous_state)
    {
        m_PreviousState = previous_state;

        BattleRoyaleConfig m_Config = BattleRoyaleConfig.GetConfig();
        BattleRoyaleGameData m_GameSettings = m_Config.GetGameData();
        i_RoundTimeInSeconds = 60 * m_GameSettings.round_duration_minutes;

        lock_notif_min = m_GameSettings.zone_notification_minutes;
        lock_notif_sec = m_GameSettings.zone_notification_seconds;

        i_DamageTickTime = m_GameSettings.zone_damage_tick_seconds;
        f_Damage = m_GameSettings.zone_damage_delta;
        i_NumZones = m_GameSettings.num_zones;

        b_ArtillerySound = m_GameSettings.artillery_sound;

        b_DoZoneDamage = m_GameSettings.enable_zone_damage;

        BattleRoyaleZoneData zone_settings = m_Config.GetZoneData();
        f_ShrinkSpeed = 0;
        if (zone_settings)
            f_ShrinkSpeed = zone_settings.zone_shrink_speed;

        m_MessageTimers = new array<ref Timer>;

#ifdef Carim
        i_MaxPartySize = CfgGameplayHandler.GetCarimPartyMaxPartySize();
#endif

        Init();
    }

    void Init()
    {
        BattleRoyaleUtils.Trace(GetName() + " Init!");
        b_ZoneLocked = false;
        b_Shrinking = false;
        b_ShrinkStopped = false;
        f_ShrinkDuration = 0;
        f_ShrinkStartTime = 0;
        f_ShrinkStopTime = 0;
        b_HasCustomShrinkFrom = false;
        v_ShrinkFromCenter = "0 0 0";
        f_ShrinkFromRadius = 0.0;
        m_Zone = new BattleRoyaleZone;

        if(GetPreviousZone())
        {
            int previous_zone_number = Math.Floor(GetPreviousZone().GetZoneNumber());
            m_Zone = m_Zone.GetZone(previous_zone_number + 1);
        } else {
            BattleRoyaleUtils.Trace("No previous zone, default to 1");
            m_Zone = m_Zone.GetZone(1); // Can't add dynamic num zone stuff here
        }

        // Update zone timer
        i_RoundTimeInSeconds = m_Zone.GetZoneTimer();
        BattleRoyaleUtils.Trace("Create round " + GetName());

        if( GetPreviousZone() )
            BattleRoyaleUtils.Trace("- Previous zone number: " + Math.Floor(GetPreviousZone().GetZoneNumber()));

        BattleRoyaleUtils.Trace("- Duration: " + i_RoundTimeInSeconds);

        //dear god i hope i really don't have to keep this, but it should work
        zone_num = m_Zone.GetZoneNumber() * 1.0; //returns 1-max (inclusive)
        float num_zones = i_NumZones * 1.0;
        BattleRoyaleUtils.Trace("- Num zone: " + m_Zone.GetZoneNumber() + "/" + i_NumZones);

        int min_players = m_Zone.GetZoneMinPlayers();
        BattleRoyaleUtils.Trace("- Min players: " + min_players);

        //scale zone damage so it is FULL power in the final zone, and linearly decreases as we decrease zone #
        f_Damage = f_Damage * ( zone_num / num_zones );
        BattleRoyaleUtils.Trace("- Damage scale: " + f_Damage);
    }

    override string GetName()
    {
        return "Gameplay State (" + zone_num + ")";
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
        return EBRMatchPhase.ROUND;
    }

    override int GetAdminZoneNumber()
    {
        if (m_Zone)
            return m_Zone.GetZoneNumber();
        return 0;
    }

    override float GetAdminZoneRadius()
    {
        vector c;
        float r;
        if (GetCurrentCircle(c, r))
            return r;
        return 0.0;
    }

    override bool IsMatchDecided()
    {
        if (!IsActive())
            return false;
#ifdef Carim
        return GetPlayers().Count() <= 1 || GetGroupsCount() <= 1;
#else
        return GetPlayers().Count() <= 1;
#endif
    }

    override int GetPhaseSecondsLeft()
    {
        if (m_RoundTimeUpTimer && IsTimerActiveOrPaused(m_RoundTimeUpTimer))
            return Math.Ceil(m_RoundTimeUpTimer.GetRemaining());
        return -1;
    }

    override int GetCountdownSecondsLeft()
    {
        if (b_ZoneLocked)
            return GetPhaseSecondsLeft();
        if (b_Shrinking && m_NewZoneLockTimer && IsTimerActiveOrPaused(m_NewZoneLockTimer))
            return Math.Ceil(m_NewZoneLockTimer.GetRemaining());
        if (m_ShrinkTimer && IsTimerActiveOrPaused(m_ShrinkTimer))
            return Math.Ceil(m_ShrinkTimer.GetRemaining());
        if (m_NewZoneLockTimer && IsTimerActiveOrPaused(m_NewZoneLockTimer))
            return Math.Ceil(m_NewZoneLockTimer.GetRemaining());
        return GetPhaseSecondsLeft();
    }

    override bool GetCurrentCircle(out vector center, out float radius)
    {
        center = "0 0 0";
        radius = 0.0;
        if (b_ZoneLocked)
        {
            if (GetZone() && GetZone().GetArea())
            {
                center = GetZone().GetArea().GetCenter();
                radius = GetZone().GetArea().GetRadius();
                return true;
            }
            return false;
        }
        if (b_ShrinkStopped)
        {
            GetShrinkCircle(f_ShrinkStopTime, center, radius);
            return true;
        }
        if (b_Shrinking)
        {
            GetShrinkCircle(GetShrinkClock(), center, radius);
            return true;
        }
        BattleRoyaleZone previous_zone = GetPreviousZone();
        if (previous_zone && previous_zone.GetArea() && GetDynamicStartingZone(i_NumStartingPlayers) < zone_num)
        {
            center = previous_zone.GetArea().GetCenter();
            radius = previous_zone.GetArea().GetRadius();
            return true;
        }
        return false;
    }

    override bool AdjustTime(int deltaSeconds, out int appliedDelta, out string reasonKey)
    {
        appliedDelta = 0;
        if (!IsActive() || !m_RoundTimeUpTimer || !IsTimerActiveOrPaused(m_RoundTimeUpTimer))
        {
            reasonKey = "STR_BR_PANEL_ERR_NO_TIMER";
            return false;
        }

        bool frozen = IsFrozen();
        float endRem = m_RoundTimeUpTimer.GetRemaining();
        float lockRem = 0.0;
        float shrinkRem = 0.0;
        bool hasLock = !b_ZoneLocked && m_NewZoneLockTimer && IsTimerActiveOrPaused(m_NewZoneLockTimer);
        bool hasShrink = !b_ZoneLocked && !b_Shrinking && m_ShrinkTimer && IsTimerActiveOrPaused(m_ShrinkTimer);
        float nearestRem = endRem;
        float actualDelta = deltaSeconds;
        float newEndRem = 0.0;
        float newLockRem = 0.0;
        float newShrinkRem = 0.0;
        float noticeTarget = 0.0;
        float nowClock = 0.0;
        float curRadius = 0.0;
        vector curCenter = "0 0 0";
        int postLockSec = 0;
        int countdownSec = 0;

        if (!b_ZoneLocked)
        {
            if (!hasLock)
            {
                reasonKey = "STR_BR_PANEL_ERR_NO_TIMER";
                return false;
            }
            lockRem = m_NewZoneLockTimer.GetRemaining();
            nearestRem = lockRem;
            if (hasShrink)
            {
                shrinkRem = m_ShrinkTimer.GetRemaining();
                nearestRem = shrinkRem;
            }
        }

        if (deltaSeconds < 0)
        {
            if (nearestRem <= 30.0)
            {
                reasonKey = "STR_BR_PANEL_ERR_TIME_FLOOR";
                return false;
            }
            if (nearestRem + actualDelta < 30.0)
                actualDelta = 30.0 - nearestRem;
            if (Math.AbsFloat(actualDelta) < 1.0)
            {
                reasonKey = "STR_BR_PANEL_ERR_TIME_FLOOR";
                return false;
            }
        }

        if (b_ZoneLocked)
        {
            newEndRem = endRem + actualDelta;
            m_RoundTimeUpTimer.Stop();
            RemoveTimer(m_RoundTimeUpTimer);
            m_RoundTimeUpTimer = CreateRoundTimer(newEndRem, "OnRoundTimeUp", NULL, frozen);
            RebuildMessageTimers(false, 0.0, newEndRem, frozen);

            if (frozen)
            {
                s_MatchPhaseSeconds = Math.Ceil(newEndRem);
                SendFrozenSnapshotTo(null);
            }
            else
            {
                BroadcastMatchPhase(EBRMatchPhase.ROUND, Math.Ceil(newEndRem));
                GetRPCManager().SendRPC(RPC_DAYZBR_NAMESPACE, "SetCountdownSeconds", new Param1<int>(Math.Ceil(newEndRem)), true);
            }
        }
        else if (b_Shrinking)
        {
            nowClock = GetStateClock();
            GetShrinkCircle(nowClock, curCenter, curRadius);
            newLockRem = lockRem + actualDelta;
            newEndRem = endRem + actualDelta;
            postLockSec = Math.Round(newEndRem - newLockRem);
            if (postLockSec < 0)
                postLockSec = 0;

            b_HasCustomShrinkFrom = true;
            v_ShrinkFromCenter = curCenter;
            f_ShrinkFromRadius = curRadius;
            f_ShrinkStartTime = nowClock;
            f_ShrinkDuration = newLockRem;

            m_NewZoneLockTimer.Stop();
            RemoveTimer(m_NewZoneLockTimer);
            m_NewZoneLockTimer = CreateRoundTimer(newLockRem, "LockNewZone", new Param1<int>(postLockSec), frozen);

            m_RoundTimeUpTimer.Stop();
            RemoveTimer(m_RoundTimeUpTimer);
            m_RoundTimeUpTimer = CreateRoundTimer(newEndRem, "OnRoundTimeUp", NULL, frozen);

            RebuildMessageTimers(false, 0.0, newEndRem, frozen);

            if (frozen)
            {
                s_MatchPhaseSeconds = Math.Ceil(newEndRem);
                SendFrozenSnapshotTo(null);
            }
            else
            {
                SendShrinkTo(null);
                BroadcastMatchPhase(EBRMatchPhase.ROUND, Math.Ceil(newEndRem));
                GetRPCManager().SendRPC(RPC_DAYZBR_NAMESPACE, "SetCountdownSeconds", new Param1<int>(Math.Ceil(newLockRem)), true);
            }
        }
        else
        {
            newLockRem = lockRem + actualDelta;
            newEndRem = endRem + actualDelta;
            postLockSec = Math.Round(newEndRem - newLockRem);
            if (postLockSec < 0)
                postLockSec = 0;

            noticeTarget = newLockRem;
            if (hasShrink)
            {
                newShrinkRem = shrinkRem + actualDelta;
                f_ShrinkStartTime = GetStateClock() + newShrinkRem;
                m_ShrinkTimer.Stop();
                RemoveTimer(m_ShrinkTimer);
                m_ShrinkTimer = CreateRoundTimer(newShrinkRem, "StartShrink", NULL, frozen);
                noticeTarget = newShrinkRem;
            }

            m_NewZoneLockTimer.Stop();
            RemoveTimer(m_NewZoneLockTimer);
            m_NewZoneLockTimer = CreateRoundTimer(newLockRem, "LockNewZone", new Param1<int>(postLockSec), frozen);

            m_RoundTimeUpTimer.Stop();
            RemoveTimer(m_RoundTimeUpTimer);
            m_RoundTimeUpTimer = CreateRoundTimer(newEndRem, "OnRoundTimeUp", NULL, frozen);

            RebuildMessageTimers(true, noticeTarget, newEndRem, frozen);
            countdownSec = Math.Ceil(noticeTarget);

            if (frozen)
            {
                s_MatchPhaseSeconds = Math.Ceil(newEndRem);
                SendFrozenSnapshotTo(null);
            }
            else
            {
                BroadcastMatchPhase(EBRMatchPhase.ROUND, Math.Ceil(newEndRem));
                GetRPCManager().SendRPC(RPC_DAYZBR_NAMESPACE, "SetCountdownSeconds", new Param1<int>(countdownSec), true);
            }
        }

        appliedDelta = Math.Round(actualDelta);

#ifdef DIAG_DEVELOPER
        Print("[BRPanel] AdjustTime state=" + GetName() + " delta=" + actualDelta.ToString() + " locked=" + b_ZoneLocked.ToString() + " shrinking=" + b_Shrinking.ToString() + " oldShrink=" + shrinkRem.ToString() + " newShrink=" + newShrinkRem.ToString() + " oldLock=" + lockRem.ToString() + " newLock=" + newLockRem.ToString() + " oldEnd=" + endRem.ToString() + " newEnd=" + newEndRem.ToString());
#endif
        return true;
    }

    override protected void SendResumeSync()
    {
        bool shrinkTimerFired = !m_ShrinkTimer || !IsTimerActiveOrPaused(m_ShrinkTimer);
        if (b_Shrinking && !b_ZoneLocked && f_ShrinkDuration > 0 && (GetStateClock() >= f_ShrinkStartTime || shrinkTimerFired))
            SendShrinkTo(null);

        int phaseSec = GetPhaseSecondsLeft();
        if (phaseSec >= 0)
            BroadcastMatchPhase(EBRMatchPhase.ROUND, phaseSec);

        int cdSec = GetCountdownSecondsLeft();
        if (cdSec >= 0)
            GetRPCManager().SendRPC(RPC_DAYZBR_NAMESPACE, "SetCountdownSeconds", new Param1<int>(cdSec), true);
    }

    override void Activate()
    {
        //we just activated this round (players not yet transfered from previous state)
        int time_till_end = i_RoundTimeInSeconds * 1000;
        int time_till_lock = time_till_end * 0.80; // Time before lock, changed from 0.75 to 0.80
        int time_between_lock_and_end = time_till_end - time_till_lock;

        // The zone may change its area with the players, so it gets them before the closing reads its area
        if(GetZone())
            GetZone().OnActivate( GetPlayers() ); //hand players over to the zone (for complex zone size/position calculation)

        // The closing ends at the lock and starts its duration before it
        int time_till_shrink = time_till_lock;
        float shrink_seconds = GetShrinkDuration();
        if (shrink_seconds > 0)
        {
            int shrink_ms = Math.Ceil(shrink_seconds * 1000);
            time_till_shrink = time_till_lock - shrink_ms;
            if (time_till_shrink < 0)
            {
                // The closing does not fit before the usual lock: it starts with the round, and the lock and the end of
                // the round come just as much later, so the edge still moves at zone_shrink_speed
                time_till_shrink = 0;
                time_till_lock = shrink_ms;
                time_till_end = time_till_lock + time_between_lock_and_end;
            }
            f_ShrinkDuration = (time_till_lock - time_till_shrink) / 1000.0;
            f_ShrinkStartTime = GetStateClock() + time_till_shrink / 1000.0;
        }

        BattleRoyaleUtils.Trace(GetName() + " Activate with a duration of " + time_till_end / 1000 + " seconds with a lock at " + time_till_lock / 1000 + " seconds (so " + time_between_lock_and_end / 1000 + " seconds after lock before end), closing from " + time_till_shrink / 1000 + " seconds for " + f_ShrinkDuration + " seconds !");
        BroadcastMatchPhase(EBRMatchPhase.ROUND, time_till_end / 1000);

        int i;
        int min;
        int sec;
        int val;

		if( GetDynamicStartingZone(i_NumStartingPlayers) < zone_num )
		{
			// With a closing the danger starts when the zone starts closing: the notices count down to that moment
			string notify_minutes = "NotifyTimeTillLockMinutes";
			string notify_seconds = "NotifyTimeTillLockSeconds";
			if (f_ShrinkDuration > 0)
			{
				notify_minutes = "NotifyTimeTillShrinkMinutes";
				notify_seconds = "NotifyTimeTillShrinkSeconds";
			}

			//--- notification message timers
			for(i = 0; i < lock_notif_min.Count();i++)
			{
				min = lock_notif_min[i];
				val = time_till_shrink - (min*60*1000);
				if(val > 0)
					m_MessageTimers.Insert( AddTimer(val / 1000.0, this, notify_minutes, new Param1<int>( min ), false) );
			}

			for(i = 0; i < lock_notif_sec.Count();i++)
			{
				sec = lock_notif_sec[i];
				val = time_till_shrink - (sec*1000);
				if(val > 0)
					m_MessageTimers.Insert( AddTimer(val / 1000.0, this, notify_seconds, new Param1<int>( sec ), false) );
			}
        }

        //timer before the zone starts closing (without a closing: before it locks)
        GetRPCManager().SendRPC( RPC_DAYZBR_NAMESPACE, "SetCountdownSeconds", new Param1<int>( time_till_shrink / 1000 ), true);

        //closing start event; a closing that starts with the round is sent at the end of Activate, after the areas
        if (f_ShrinkDuration > 0 && time_till_shrink > 0)
            m_ShrinkTimer = AddTimer(time_till_shrink / 1000.0, this, "StartShrink", NULL, false);

        //lock zone event
        m_NewZoneLockTimer = AddTimer(time_till_lock / 1000.0, this, "LockNewZone", new Param1<int>( time_between_lock_and_end / 1000 ), false);

        if (m_Zone.GetZoneNumber() < i_NumZones)  // Not the last zone
        {
            //--- notification message timers
            for(i = 0; i < lock_notif_min.Count();i++)
            {
                min = lock_notif_min[i];
                val = time_till_end - (min*60*1000);
                if(val > 0)
                    m_MessageTimers.Insert( AddTimer(val / 1000.0, this, "NotifyTimeTillNewZoneMinutes", new Param1<int>( min ), false) );
            }

            for(i = 0; i < lock_notif_sec.Count();i++)
            {
                sec = lock_notif_sec[i];
                val = time_till_end - (sec*1000);
                if(val > 0)
                    m_MessageTimers.Insert( AddTimer(val / 1000.0, this, "NotifyTimeTillNewZoneSeconds", new Param1<int>( sec ), false) );
            }
        } else {  // The last zone, no new zone
            //--- notification message timers
            for(i = 0; i < lock_notif_min.Count();i++)
            {
                min = lock_notif_min[i];
                val = time_till_end - (min*60*1000);
                if(val > 0)
                    m_MessageTimers.Insert( AddTimer(val / 1000.0, this, "NotifyTimeTillNoZoneMinutes", new Param1<int>( min ), false) );
            }

            for(i = 0; i < lock_notif_sec.Count();i++)
            {
                sec = lock_notif_sec[i];
                val = time_till_end - (sec*1000);
                if(val > 0)
                    m_MessageTimers.Insert( AddTimer(val / 1000.0, this, "NotifyTimeTillNoZoneSeconds", new Param1<int>( sec ), false) );
            }
        }

        //tell the clients the zone in force: the previous zone, the circle the damage uses until the lock. A round skipped
        //before its lock (admin Next) left the clients on its old zone or in the middle of its closing; this ends that
        //closing on the circle the damage uses, before this round announces its zone or starts its own closing.
        //The first round played has no zone in force on the clients and sends none, except the last normal round, which
        //always sent it
        bool is_last_zone = m_Zone.GetZoneNumber() >= i_NumZones;
        BattleRoyalePlayArea previous_area = NULL;
        BattleRoyaleZone previous_zone = GetPreviousZone();
        if (previous_zone)
            previous_area = previous_zone.GetArea();

        if (previous_area && (is_last_zone || GetDynamicStartingZone(i_NumStartingPlayers) < zone_num))
            GetRPCManager().SendRPC( RPC_DAYZBR_NAMESPACE, "UpdateCurrentPlayArea", new Param2<vector, float>( previous_area.GetCenter(), previous_area.GetRadius() ), true);
        else if (is_last_zone)
            Error("Round has no previous zone; current play area was not sent");

        ref BattleRoyalePlayArea m_ThisArea = NULL;
        if(GetZone())
            m_ThisArea = GetZone().GetArea();

        //tell the client the next play area and play artillery sound
        GetRPCManager().SendRPC( RPC_DAYZBR_NAMESPACE, "UpdateFuturePlayArea", new Param3<vector, float, bool>( m_ThisArea.GetCenter(), m_ThisArea.GetRadius(), b_ArtillerySound ), true);

        //end state event
        m_RoundTimeUpTimer = AddTimer( time_till_end / 1000.0, this, "OnRoundTimeUp", NULL, false);

        //message players saying the new zone has appeared & notify them if they're outside the play area (hopefully this won't lag the server)
        for(i = 0; i < GetPlayers().Count(); i++)
        {
            PlayerBase player = GetPlayers()[i];
            if(player)
            {
                vector playerPos = player.GetPosition();
                playerPos[1] = 0;
                vector next_pos = m_ThisArea.GetCenter();
                next_pos[1] = 0;
                float dist = vector.Distance(playerPos, next_pos);
                if(dist > m_ThisArea.GetRadius())
                {
                    MessagePlayerUntranslated(player, "STR_BR_NEW_ZONE_OUTSIDE");
                }
                else if (m_Zone.GetZoneNumber() != 1)
                {
                    MessagePlayerUntranslated(player, "STR_BR_NEW_ZONE_INSIDE");
                }
            }
        }

        // A closing that does not fit before the usual lock starts now, after the areas sent above
        if (f_ShrinkDuration > 0 && time_till_shrink <= 0)
            StartShrink();

        super.Activate();
    }

    override void Deactivate()
    {
        if ( m_ShrinkTimer )
            m_ShrinkTimer.Stop();

        if ( m_NewZoneLockTimer )
            m_NewZoneLockTimer.Stop();

        if ( m_RoundTimeUpTimer )
            m_RoundTimeUpTimer.Stop();

        for(int i = 0; i < m_MessageTimers.Count(); i++)
        {
            if ( m_MessageTimers[i] )
                m_MessageTimers[i].Stop();
        }

        // A closing cut short before the lock (admin Next, or the match ends) stops at the circle of this instant, on the
        // clients and for the damage until the next state takes over; a next round then sends the zone in force
        if (b_Shrinking && !b_ZoneLocked && !b_ShrinkStopped)
        {
            b_ShrinkStopped = true;
            f_ShrinkStopTime = GetStateClock();
            vector stop_center;
            float stop_radius;
            GetShrinkCircle(f_ShrinkStopTime, stop_center, stop_radius);
            GetRPCManager().SendRPC( RPC_DAYZBR_NAMESPACE, "UpdateCurrentPlayArea", new Param2<vector, float>( stop_center, stop_radius ), true);
        }

        //we just deactivated this round (players not yet transfered from previous state)
        super.Deactivate();
    }

	override bool IsComplete() //return true when this state is complete & ready to transfer to the next state
	{
		if(!IsActive())
			return super.IsComplete();

#ifdef Carim
		bool playersLessThanTwo = GetPlayers().Count() <= 1;
		bool groupsLessThanTwo = GetGroupsCount() <= 1;

		if(playersLessThanTwo || groupsLessThanTwo)
		{
			string reason;
			if(playersLessThanTwo && groupsLessThanTwo)
				reason = "(Players/Groups)";
			else if(playersLessThanTwo)
				reason = "(Players)";
			else
				reason = "(Groups)";

			BattleRoyaleUtils.Trace(GetName() + " IsComplete " + reason + "!");
			Deactivate();
		}
#else
		if(GetPlayers().Count() <= 1)
		{
			BattleRoyaleUtils.Trace(GetName() + " IsComplete (Players)!");
			Deactivate();
		}
#endif

		return super.IsComplete();
	}

    override bool SkipState(BattleRoyaleState _previousState)
    {
        if( GetDynamicStartingZone(i_NumStartingPlayers) > zone_num )
        {
            BattleRoyaleUtils.Trace("[State Machine] Skipping State `" + GetDynamicStartingZone(i_NumStartingPlayers) + "` > `" + zone_num + "`");
            return true;
        }

        //only one (or less) players remaining, must skip to win state
        // TODO: toggle to debug game
        if(_previousState.GetPlayers().Count() <= 1)
            return true;

#ifdef Carim
        if(_previousState.GetGroupsCount() <= 1)
            return true;
#endif

        return false;
    }

    override void OnPlayerKilled(PlayerBase player, Object source)
    {
        super.OnPlayerKilled( player, source );
    }

    override void OnPlayerTick(PlayerBase player, float timeslice)
    {
        BattleRoyaleZone current_zone = GetActiveZone();
        if(current_zone && b_DoZoneDamage)
        {
            float radius = current_zone.GetArea().GetRadius();
            vector center = current_zone.GetArea().GetCenter();

            // While the zone closes the circle of this instant applies: a player the edge passed over is outside from now on
            if (b_Shrinking && !b_ZoneLocked)
                GetShrinkCircle(GetShrinkClock(), center, radius);

            vector playerPos = player.GetPosition();

            //distance needs to be done in 2D, not 3D, set Z coord to 0 so this can be done
            playerPos[1] = 0;
            center[1] = 0;
            float distance = vector.Distance(playerPos, center);
            if(distance >= radius)
            {
                if(player.time_until_damage <= 0)
                {
                    //DAMAGE
				    player.GetSymptomManager().QueueUpPrimarySymptom(SymptomIDs.SYMPTOM_PAIN_LIGHT);
                    //TODO: determine if this last health tick will kill the player
                    player.BR_ZoneDamage( f_Damage ); //TODO: delta this by the # of zones that have ticked (more zones = more damage)
                    player.time_until_damage = i_DamageTickTime; //reset timer
                }
                player.time_until_damage -= timeslice;
            }
            else
            {
                //if the player leaves the zone damage area, their damage ticktime will slowly incremement until it reaches the max value (5)
                //this way you can't just keep jumping in and out of the zone to edgeplay
                player.time_until_damage = Math.Min(i_DamageTickTime, player.time_until_damage + timeslice);
            }
        }

        // Unlimited stamina
        //player.GetStatStamina().Set(CfgGameplayHandler.GetStaminaMax());

        super.OnPlayerTick(player, timeslice);
    }

    //handle zoning stuff
    BattleRoyaleZone GetZone()
    {
        return m_Zone;
    }

    //! Zone of the previous round; null in the first round played, which follows the start match state.
    //! OnPlayerTick reaches this every tick until the first zone locks, so that expected case is not logged
    BattleRoyaleZone GetPreviousZone()
    {
        BattleRoyaleRound prev_round;
        if(Class.CastTo(prev_round, m_PreviousState))
        {
            return prev_round.GetZone();
//            if( m_Zone.GetZoneNumber() > m_PreviousState.i_StartingZone )
//                return prev_round.GetZone();
//            else
//                BattleRoyaleUtils.Trace(m_Zone.GetZoneNumber() + " <= " + m_PreviousState.i_StartingZone);
        }

        return NULL;
    }

    BattleRoyaleZone GetActiveZone() //returns NULL if first zone & not locked
    {
        if(b_ZoneLocked)
            return GetZone();
        else
            return GetPreviousZone();
    }

    bool IsLocked()
    {
        return b_ZoneLocked;
    }

    //! Seconds the closing from the previous zone to this one lasts, 0 for none: off in the settings, or the first round
    //! played, whose zone appears at the lock because the clients were never shown the zone before it.
    //! The edge point that moves the most covers the radius difference plus the shift of the center at zone_shrink_speed
    protected float GetShrinkDuration()
    {
        if (f_ShrinkSpeed <= 0)
            return 0;

        if (GetDynamicStartingZone(i_NumStartingPlayers) >= zone_num)
            return 0;

        BattleRoyaleZone previous_zone = GetPreviousZone();
        if (!previous_zone || !previous_zone.GetArea() || !GetZone() || !GetZone().GetArea())
            return 0;

        vector from_center = previous_zone.GetArea().GetCenter();
        vector to_center = GetZone().GetArea().GetCenter();
        float dx = to_center[0] - from_center[0];
        float dz = to_center[2] - from_center[2];
        float travel = previous_zone.GetArea().GetRadius() - GetZone().GetArea().GetRadius() + Math.Sqrt(dx * dx + dz * dz);
        if (travel <= 0)
            return 0;

        return travel / f_ShrinkSpeed;
    }

    protected Timer CreateRoundTimer(float duration, string fnName, Param params, bool paused)
    {
        if (paused)
            return AddPausedTimer(duration, this, fnName, params, false);
        return AddTimer(duration, this, fnName, params, false);
    }

    protected void RebuildMessageTimers(bool includePreShrink, float preShrinkRem, float endRem, bool paused)
    {
        int i;
        for (i = 0; i < m_MessageTimers.Count(); i++)
        {
            if (m_MessageTimers[i])
            {
                m_MessageTimers[i].Stop();
                RemoveTimer(m_MessageTimers[i]);
            }
        }
        m_MessageTimers.Clear();

        int min;
        int sec;
        float val;
        if (includePreShrink && GetDynamicStartingZone(i_NumStartingPlayers) < zone_num)
        {
            string notify_minutes = "NotifyTimeTillLockMinutes";
            string notify_seconds = "NotifyTimeTillLockSeconds";
            if (f_ShrinkDuration > 0)
            {
                notify_minutes = "NotifyTimeTillShrinkMinutes";
                notify_seconds = "NotifyTimeTillShrinkSeconds";
            }

            for (i = 0; i < lock_notif_min.Count(); i++)
            {
                min = lock_notif_min[i];
                val = preShrinkRem - (min * 60.0);
                if (val > 0)
                    m_MessageTimers.Insert(CreateRoundTimer(val, notify_minutes, new Param1<int>(min), paused));
            }
            for (i = 0; i < lock_notif_sec.Count(); i++)
            {
                sec = lock_notif_sec[i];
                val = preShrinkRem - sec;
                if (val > 0)
                    m_MessageTimers.Insert(CreateRoundTimer(val, notify_seconds, new Param1<int>(sec), paused));
            }
        }

        string endMinutesFn = "NotifyTimeTillNewZoneMinutes";
        string endSecondsFn = "NotifyTimeTillNewZoneSeconds";
        if (m_Zone.GetZoneNumber() >= i_NumZones)
        {
            endMinutesFn = "NotifyTimeTillNoZoneMinutes";
            endSecondsFn = "NotifyTimeTillNoZoneSeconds";
        }

        for (i = 0; i < lock_notif_min.Count(); i++)
        {
            min = lock_notif_min[i];
            val = endRem - (min * 60.0);
            if (val > 0)
                m_MessageTimers.Insert(CreateRoundTimer(val, endMinutesFn, new Param1<int>(min), paused));
        }
        for (i = 0; i < lock_notif_sec.Count(); i++)
        {
            sec = lock_notif_sec[i];
            val = endRem - sec;
            if (val > 0)
                m_MessageTimers.Insert(CreateRoundTimer(val, endSecondsFn, new Param1<int>(sec), paused));
        }
    }

    protected void GetShrinkFromArea(out vector from_center, out float from_radius)
    {
        if (b_HasCustomShrinkFrom)
        {
            from_center = v_ShrinkFromCenter;
            from_radius = f_ShrinkFromRadius;
            return;
        }
        BattleRoyalePlayArea from_area = GetPreviousZone().GetArea();
        from_center = from_area.GetCenter();
        from_radius = from_area.GetRadius();
    }

    //! The zone starts closing: from now on the damage follows the circle of the instant, and every client draws it
    void StartShrink()
    {
        LogPanelCallback("StartShrink");
        BattleRoyaleZone previous_zone = GetPreviousZone();
        if (b_ZoneLocked || f_ShrinkDuration <= 0 || !previous_zone || !previous_zone.GetArea() || !GetZone() || !GetZone().GetArea())
            return;

        // f_ShrinkStartTime stays the planned start: a late timer gives the clients the seconds really gone
        b_Shrinking = true;
        float late_seconds = GetStateClock() - f_ShrinkStartTime;
        BattleRoyaleUtils.Trace(GetName() + " closing starts, " + f_ShrinkDuration + " seconds, " + late_seconds + " seconds after its planned start");
        SendShrinkTo(null);
    }

    //! State clock of the closing circle in force: now, or the instant the round ended during its closing
    protected float GetShrinkClock()
    {
        if (b_ShrinkStopped)
            return f_ShrinkStopTime;

        return GetStateClock();
    }

    //! Circle of the closing at state clock now, from its planned start; the lerp factor stays within [0, 1]
    protected void GetShrinkCircle(float now, out vector center, out float radius)
    {
        vector from_center;
        float from_radius;
        GetShrinkFromArea(from_center, from_radius);
        BattleRoyalePlayArea to_area = GetZone().GetArea();

        float s = 1;
        if (f_ShrinkDuration > 0)
            s = Math.Clamp((now - f_ShrinkStartTime) / f_ShrinkDuration, 0, 1);

        vector to_center = to_area.GetCenter();
        center = from_center + (to_center - from_center) * s;
        radius = from_radius + (to_area.GetRadius() - from_radius) * s;
    }

    //! StartZoneShrink: start circle, end circle, total duration and seconds already gone since the planned start (more
    //! than 0 when the start timer came late); null identity sends to everyone
    protected void SendShrinkTo(PlayerIdentity identity)
    {
        vector from_center;
        float from_radius;
        GetShrinkFromArea(from_center, from_radius);
        BattleRoyalePlayArea to_area = GetZone().GetArea();
        float elapsed = Math.Clamp(GetStateClock() - f_ShrinkStartTime, 0, f_ShrinkDuration);
        GetRPCManager().SendRPC( RPC_DAYZBR_NAMESPACE, "StartZoneShrink", new Param6<vector, float, vector, float, float, float>( from_center, from_radius, to_area.GetCenter(), to_area.GetRadius(), f_ShrinkDuration, elapsed ), true, identity);
    }

    //! Zone state of this round for a client that restarted its mission or joined late: the zone in force, the announced
    //! one and the closing under way with the time already gone. A round already deactivated stays the current state
    //! until the transition (paused, or waiting for the next check) and its damage still applies, so it still sends
    void SendZoneStateTo(PlayerIdentity identity)
    {
        if (!identity)
            return;

        BattleRoyaleZone previous_zone = GetPreviousZone();
        BattleRoyaleZone this_zone = GetZone();
        if (!this_zone || !this_zone.GetArea())
            return;

        BattleRoyalePlayArea this_area = this_zone.GetArea();
        if (b_ZoneLocked)
        {
            // Same pair as LockNewZone
            GetRPCManager().SendRPC( RPC_DAYZBR_NAMESPACE, "UpdateCurrentPlayArea", new Param2<vector, float>( this_area.GetCenter(), this_area.GetRadius() ), true, identity);
            GetRPCManager().SendRPC( RPC_DAYZBR_NAMESPACE, "UpdateFuturePlayArea", new Param3<vector, float, bool>( this_area.GetCenter(), this_area.GetRadius(), false ), true, identity);
            return;
        }

        if (b_ShrinkStopped)
        {
            // Same circle as Deactivate, the one the damage uses: the stop instant is fixed, so no StartZoneShrink, which
            // would run the closing again on the client's clock
            vector stop_center;
            float stop_radius;
            GetShrinkCircle(f_ShrinkStopTime, stop_center, stop_radius);
            GetRPCManager().SendRPC( RPC_DAYZBR_NAMESPACE, "UpdateCurrentPlayArea", new Param2<vector, float>( stop_center, stop_radius ), true, identity);
            GetRPCManager().SendRPC( RPC_DAYZBR_NAMESPACE, "UpdateFuturePlayArea", new Param3<vector, float, bool>( this_area.GetCenter(), this_area.GetRadius(), false ), true, identity);
            return;
        }

        // Before the lock the zone in force is the previous one; the first round played has none on the clients
        if (previous_zone && previous_zone.GetArea() && GetDynamicStartingZone(i_NumStartingPlayers) < zone_num)
        {
            BattleRoyalePlayArea previous_area = previous_zone.GetArea();
            GetRPCManager().SendRPC( RPC_DAYZBR_NAMESPACE, "UpdateCurrentPlayArea", new Param2<vector, float>( previous_area.GetCenter(), previous_area.GetRadius() ), true, identity);
        }

        // No artillery sound: the zone was announced before this client arrived
        GetRPCManager().SendRPC( RPC_DAYZBR_NAMESPACE, "UpdateFuturePlayArea", new Param3<vector, float, bool>( this_area.GetCenter(), this_area.GetRadius(), false ), true, identity);

        // After the zone in force, which ends a closing on the client
        if (b_Shrinking)
        {
            if (IsFrozen())
            {
                vector frozen_center;
                float frozen_radius;
                GetShrinkCircle(GetShrinkClock(), frozen_center, frozen_radius);
                GetRPCManager().SendRPC( RPC_DAYZBR_NAMESPACE, "UpdateCurrentPlayArea", new Param2<vector, float>( frozen_center, frozen_radius ), true, identity);
            }
            else
            {
                SendShrinkTo(identity);
            }
        }
    }

    void LockNewZone(int seconds)
    {
        LogPanelCallback("LockNewZone");
        b_ZoneLocked = true;
        b_Shrinking = false;

        int postLockSec = seconds;
        if (m_RoundTimeUpTimer && IsTimerActiveOrPaused(m_RoundTimeUpTimer))
            postLockSec = Math.Ceil(m_RoundTimeUpTimer.GetRemaining());

        //send play area to clients
        ref BattleRoyalePlayArea m_PreviousArea = NULL;
        if(GetPreviousZone())
            m_PreviousArea = GetPreviousZone().GetArea();

        ref BattleRoyalePlayArea m_ThisArea = NULL;
        if(GetZone())
            m_ThisArea = GetZone().GetArea();

        //tell the client the current area is now this area
        GetRPCManager().SendRPC( RPC_DAYZBR_NAMESPACE, "UpdateCurrentPlayArea", new Param2<vector, float>( m_ThisArea.GetCenter(), m_ThisArea.GetRadius() ), true);
        //tell the client we don't know the next play area
        GetRPCManager().SendRPC( RPC_DAYZBR_NAMESPACE, "UpdateFuturePlayArea", new Param3<vector, float, bool>( m_ThisArea.GetCenter(), m_ThisArea.GetRadius(), b_ArtillerySound ), true);
        //tell the client how much time until the next zone appears
        GetRPCManager().SendRPC( RPC_DAYZBR_NAMESPACE, "SetCountdownSeconds", new Param1<int>( postLockSec ) , true);
        BroadcastMatchPhase(EBRMatchPhase.ROUND, postLockSec);
    }

    void OnRoundTimeUp()
    {
        LogPanelCallback("OnRoundTimeUp");
        Deactivate();
    }

	// Round messaging
	void NotifyTimeTillLockSeconds(int seconds)
	{
		LogPanelCallback("NotifyTimeTillLockSeconds");
		if(seconds > 1)
			MessagePlayersUntranslated("STR_BR_CURRENT_ZONE_WILL_LOCK_IN_SECONDS", seconds.ToString());
		else
			MessagePlayersUntranslated("STR_BR_CURRENT_ZONE_WILL_LOCK_IN_SECOND", seconds.ToString());
	}

	void NotifyTimeTillLockMinutes(int minutes)
	{
		LogPanelCallback("NotifyTimeTillLockMinutes");
		if(minutes > 1)
			MessagePlayersUntranslated("STR_BR_CURRENT_ZONE_WILL_LOCK_IN_MINUTES", minutes.ToString());
		else
			MessagePlayersUntranslated("STR_BR_CURRENT_ZONE_WILL_LOCK_IN_MINUTE", minutes.ToString());
	}

	void NotifyTimeTillShrinkSeconds(int seconds)
	{
		LogPanelCallback("NotifyTimeTillShrinkSeconds");
		if(seconds > 1)
			MessagePlayersUntranslated("STR_BR_ZONE_STARTS_CLOSING_IN_SECONDS", seconds.ToString());
		else
			MessagePlayersUntranslated("STR_BR_ZONE_STARTS_CLOSING_IN_SECOND", seconds.ToString());
	}

	void NotifyTimeTillShrinkMinutes(int minutes)
	{
		LogPanelCallback("NotifyTimeTillShrinkMinutes");
		if(minutes > 1)
			MessagePlayersUntranslated("STR_BR_ZONE_STARTS_CLOSING_IN_MINUTES", minutes.ToString());
		else
			MessagePlayersUntranslated("STR_BR_ZONE_STARTS_CLOSING_IN_MINUTE", minutes.ToString());
	}

	void NotifyTimeTillNewZoneSeconds(int seconds)
	{
		LogPanelCallback("NotifyTimeTillNewZoneSeconds");
		if(seconds > 1)
			MessagePlayersUntranslated("STR_BR_A_NEW_ZONE_WILL_APPEAR_IN_SECONDS", seconds.ToString());
		else
			MessagePlayersUntranslated("STR_BR_A_NEW_ZONE_WILL_APPEAR_IN_SECOND", seconds.ToString());
	}

	void NotifyTimeTillNewZoneMinutes(int minutes)
	{
		LogPanelCallback("NotifyTimeTillNewZoneMinutes");
		if(minutes > 1)
			MessagePlayersUntranslated("STR_BR_A_NEW_ZONE_WILL_APPEAR_IN_MINUTES", minutes.ToString());
		else
			MessagePlayersUntranslated("STR_BR_A_NEW_ZONE_WILL_APPEAR_IN_MINUTE", minutes.ToString());
	}

	void NotifyTimeTillNoZoneSeconds(int seconds)
	{
		LogPanelCallback("NotifyTimeTillNoZoneSeconds");
		if(seconds > 1)
			MessagePlayersUntranslated("STR_BR_THE_LAST_ZONE_WILL_APPEAR_IN_SECONDS", seconds.ToString());
		else
			MessagePlayersUntranslated("STR_BR_THE_LAST_ZONE_WILL_APPEAR_IN_SECOND", seconds.ToString());
	}

	void NotifyTimeTillNoZoneMinutes(int minutes)
	{
		LogPanelCallback("NotifyTimeTillNoZoneMinutes");
		if(minutes > 1)
			MessagePlayersUntranslated("STR_BR_THE_LAST_ZONE_WILL_APPEAR_IN_MINUTES", minutes.ToString());
		else
			MessagePlayersUntranslated("STR_BR_THE_LAST_ZONE_WILL_APPEAR_IN_MINUTE", minutes.ToString());
	}
}
#endif

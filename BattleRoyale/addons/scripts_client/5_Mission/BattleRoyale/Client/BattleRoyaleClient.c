#ifndef SERVER
class BattleRoyaleClient: BattleRoyaleBase
{
    protected ref BattleRoyalePlayArea m_CurrentPlayArea;
    protected ref BattleRoyalePlayArea m_FuturePlayArea;

    protected int i_Kills; //TODO: this needs to be done differently (most likely)
    protected bool b_MatchStarted;
    protected int i_SecondsRemaining;

    protected bool b_IsReady;

    protected ref BattleRoyaleZoneWall m_ZoneWall;  //!< red, on the zone in force
    protected ref BattleRoyaleZoneWall m_OpenZoneWall;  //!< yellow, on the announced zone until it locks

    //! Outside zone effect last queued: -1 unknown, 0 removed, 1 added; reset when the local player entity changes
    protected int i_OutsideZoneEffectState;
    protected PlayerBase m_OutsideZoneEffectPlayer;

    void BattleRoyaleClient()
    {
        BattleRoyaleUtils.Trace("BattleRoyaleClient::BattleRoyaleClient");

        b_IsReady = false;
        b_MatchStarted = false;
        i_Kills = 0;
        i_SecondsRemaining = 0;
        i_OutsideZoneEffectState = -1;
        m_ZoneWall = new BattleRoyaleZoneWall( BattleRoyaleZoneWall.MODEL_LOCKED );
        m_OpenZoneWall = new BattleRoyaleZoneWall( BattleRoyaleZoneWall.MODEL_OPEN );

        Init();
    }

    void ~BattleRoyaleClient()
    {
    	BattleRoyaleUtils.Trace("BattleRoyaleClient::~BattleRoyaleClient");

		if ( GetGame() )
			GetGame().GetCallQueue(CALL_CATEGORY_GUI).RemoveByName( this, "OnSecond" );
    }

    void Init()
    {
        BattleRoyaleUtils.Trace("BattleRoyaleClient::Init");

		GetGame().GetCallQueue(CALL_CATEGORY_GUI).CallLaterByName( this, "OnSecond", 1000, true );

		BattleRoyaleRPC br_rpc = BattleRoyaleRPC.GetInstance();
		br_rpc.Reset();

		BattleRoyaleUtils.Trace("BattleRoyaleClient::Init - Done");
    }

    //--- note: these return NULL of there is no area referenced for next or current area
    BattleRoyalePlayArea GetPlayArea()
    {
        return m_CurrentPlayArea;
    }

    BattleRoyalePlayArea GetNextArea()
    {
        return m_FuturePlayArea;
    }

	// To track changes
    bool br_previous_fade_state = false;
    bool br_previous_input_state = false;
    vector br_previous_current_play_area_center;
    float br_previous_current_play_area_radius;
    vector br_previous_future_play_area_center;
    float br_previous_future_play_area_radius;
    bool br_previous_win_screen = false;
    int br_previous_countdown = 0;
    int br_previous_nb_players = -1;
    int br_previous_nb_groups = -1;
    int br_previous_kills = -1;

    override void Update(float delta)
    {
        MissionGameplay gameplay = MissionGameplay.Cast( GetGame().GetMission() );
		if ( !gameplay )
			return;

		BattleRoyaleRPC br_rpc = BattleRoyaleRPC.GetInstance();

		// Update player and group remaining count
		// A value is only marked as shown once the HUD exists: one received before it would be lost
		if ( br_previous_nb_players != br_rpc.nb_players || br_previous_nb_groups != br_rpc.nb_groups )
		{
			if ( gameplay.UpdatePlayerCount( br_rpc.nb_players, br_rpc.nb_groups ) )
			{
				br_previous_nb_players = br_rpc.nb_players;
				br_previous_nb_groups = br_rpc.nb_groups;
			}
		}

		// Update player kill count
		if ( br_previous_kills != br_rpc.player_kills )
		{
			if ( gameplay.UpdateKillCount( br_rpc.player_kills ) )
				br_previous_kills = br_rpc.player_kills;
		}

		// Update match start state
		if( br_rpc.match_started && !b_MatchStarted )
		{
			OnMatchStarted();
		}

		// Update countdown timer and zone distance
		if ( br_previous_countdown != br_rpc.countdown_seconds )
		{
			i_SecondsRemaining = br_rpc.countdown_seconds;
			gameplay.UpdateCountdownTimer( i_SecondsRemaining );
			br_previous_countdown = br_rpc.countdown_seconds;
		}

		// Update current play area, only when the server sent a new one (a cleared area keeps the last one)
		if ( br_previous_current_play_area_center != br_rpc.current_play_area_center || br_previous_current_play_area_radius != br_rpc.current_play_area_radius )
		{
			if ( br_rpc.current_play_area_center != "0 0 0" && br_rpc.current_play_area_radius != 0.0 )
				m_CurrentPlayArea = new BattleRoyalePlayArea( br_rpc.current_play_area_center, br_rpc.current_play_area_radius );

			br_previous_current_play_area_center = br_rpc.current_play_area_center;
			br_previous_current_play_area_radius = br_rpc.current_play_area_radius;
		}

		// Update future play area
		if ( br_previous_future_play_area_center != br_rpc.future_play_area_center || br_previous_future_play_area_radius != br_rpc.future_play_area_radius )
		{
			if ( br_rpc.future_play_area_center && br_rpc.future_play_area_radius )
			{
				m_FuturePlayArea = new BattleRoyalePlayArea( br_rpc.future_play_area_center, br_rpc.future_play_area_radius );

				if ( br_rpc.b_ArtillerySound )
				{
					ref EffectSound m_ArtySound = SEffectManager.PlaySound("Artillery_Distant_SoundSet", m_FuturePlayArea.GetCenter(), 0.1, 0.1);
					m_ArtySound.SetAutodestroy(true);
				}
			}
			br_previous_future_play_area_center = br_rpc.future_play_area_center;
			br_previous_future_play_area_radius = br_rpc.future_play_area_radius;
		}

		// Show the winner screen
		if( br_rpc.winner_screen && !br_previous_win_screen )
		{
			gameplay.ShowWinScreen();
			br_previous_win_screen = true;
		}

		// Match phase and kill feed, also shown without a local player
		UpdateMatchPhase( gameplay, br_rpc );
		gameplay.UpdateKillFeed();

		// Walls along the zone edges while the zones are in play: red on the zone in force, yellow on the announced
		// zone until it locks. Both are gone once the final zone collapsed and when the match is over, because the
		// client keeps the last areas after the server clears them
		BattleRoyalePlayArea locked_area = null;
		BattleRoyalePlayArea open_area = null;
		if ( IsZonePlayPhase( br_rpc ) && !IsZoneCollapsed( br_rpc ) )
		{
			locked_area = m_CurrentPlayArea;
			if ( !IsZoneLocked() )
				open_area = m_FuturePlayArea;
		}
		vector wall_camera_pos = GetGame().GetCurrentCameraPosition();
		m_ZoneWall.Update( delta, wall_camera_pos, locked_area );
		m_OpenZoneWall.Update( delta, wall_camera_pos, open_area );

#ifdef BR_MINIMAP
        vector camera_pos = GetGame().GetCurrentCameraPosition();
        gameplay.UpdateMiniMap( camera_pos );
#endif

		// Everything below needs the local player. Without one there is no distance to show, and hiding it also
		// turns the phase text back to white
		PlayerBase player = PlayerBase.Cast( GetGame().GetPlayer() );
		if ( !player )
		{
			gameplay.HideDistance();
			return;
		}

		// Fade in/out effect
		if( br_previous_fade_state != br_rpc.fade_state )
		{
			if( br_rpc.fade_state )
			{
				FadeIn( player );
			}
			else
			{
				FadeOut( player );
			}
			br_previous_fade_state = br_rpc.fade_state;
		}

		// Input enable/disable
		if( br_previous_input_state != br_rpc.input_state )
		{
			player.DisableInput( br_rpc.input_state );
			br_previous_input_state = br_rpc.input_state;
		}

		// Set top position
		player.position_top = br_rpc.top_position;

		UpdateZoneHud( gameplay, player, br_rpc );
    }

    //! Distance to the zone, outside zone visual effect and zone damage indicator
    protected void UpdateZoneHud(MissionGameplay gameplay, PlayerBase player, BattleRoyaleRPC br_rpc)
    {
		float distExt;
		float distInt;
		float angle;
		bool isInsideZone = true;

		// First check if player is outside current play area; after the match the last area is no longer in play
		if (m_CurrentPlayArea && IsZonePlayPhase(br_rpc))
		{
			// Check if player is inside current play area and get distExt, distInt and angle
			isInsideZone = GetZoneDistance(player, m_CurrentPlayArea, distExt, distInt, angle);

			// Once the final zone is gone there is no safe area left to point at
			if (IsZoneCollapsed(br_rpc))
			{
				gameplay.HideDistance();
			}
			// If outside current play area, show distance to it
			else if (!isInsideZone)
			{
				gameplay.UpdateZoneDistance(isInsideZone, distExt, distInt, angle);
			}
			// Player is inside current play area, check if future play area exists
			else if (m_FuturePlayArea)
			{
				float futureDistExt;
				float futureDistInt;
				float futureAngle;
				bool isInsideFutureZone = GetZoneDistance(player, m_FuturePlayArea, futureDistExt, futureDistInt, futureAngle);
				gameplay.UpdateZoneDistance(isInsideFutureZone, futureDistExt, futureDistInt, futureAngle);
			}
			// Player inside current area but no future area
			else
			{
				gameplay.UpdateZoneDistance(true, 0, distInt, angle);
			}
		}
		// No current play area
		else
		{
			gameplay.HideDistance();
		}

		bool alive = player.IsAlive();

		// Visual effect when outside of the current zone, queued only on the inside/outside transition
		if ( alive )
		{
			bool outside_effect = false;
			if ( m_CurrentPlayArea && distExt > 0 )
				outside_effect = true;

			UpdateOutsideZoneEffect( player, outside_effect );
		}

		// During the rounds the server damages players outside the current zone, and every player once the final zone is gone
		bool taking_damage = false;
		if ( alive && IsZoneDamagePhase( br_rpc ) )
		{
			if ( IsZoneCollapsed( br_rpc ) )
				taking_damage = true;
			else if ( m_CurrentPlayArea && !isInsideZone )
				taking_damage = true;
		}
		gameplay.UpdateZoneDamage( taking_damage );
    }

    //! Queues the outside zone effect only when its state or the local player entity changes
    protected void UpdateOutsideZoneEffect(PlayerBase player, bool outside)
    {
		int state = 0;
		if ( outside )
			state = 1;

		if ( player == m_OutsideZoneEffectPlayer && state == i_OutsideZoneEffectState )
			return;

		m_OutsideZoneEffectPlayer = player;
		i_OutsideZoneEffectState = state;

		if ( outside )
		{
			player.QueueAddGlassesEffect(PPERequesterBank.REQ_BATTLEROYALE);
		}
		else
		{
			player.QueueRemoveGlassesEffect(PPERequesterBank.REQ_BATTLEROYALE);
		}
    }

    //! After the lock the server sends the same area as current and future zone
    protected bool IsZoneLocked()
    {
		if ( !m_CurrentPlayArea || !m_FuturePlayArea )
			return false;

		return m_CurrentPlayArea.GetCenter() == m_FuturePlayArea.GetCenter() && m_CurrentPlayArea.GetRadius() == m_FuturePlayArea.GetRadius();
    }

    //! Zone damage only exists while a round runs
    protected bool IsZoneDamagePhase(BattleRoyaleRPC br_rpc)
    {
		return br_rpc.match_phase == EBRMatchPhase.ROUND || br_rpc.match_phase == EBRMatchPhase.LAST_ROUND;
    }

    //! The zones are in play from the start of the match, when the first zone is shown, until the match is over
    protected bool IsZonePlayPhase(BattleRoyaleRPC br_rpc)
    {
		return br_rpc.match_phase == EBRMatchPhase.STARTING || IsZoneDamagePhase( br_rpc );
    }

    //! The last round clears the current area when the final zone locks; from then on every player takes damage.
    //! An empty current area counts as collapsed only once the server has sent one; before that the area is unknown
    protected bool IsZoneCollapsed(BattleRoyaleRPC br_rpc)
    {
		if ( br_rpc.match_phase != EBRMatchPhase.LAST_ROUND )
			return false;

		return br_rpc.current_play_area_received && br_rpc.current_play_area_center == "0 0 0" && br_rpc.current_play_area_radius == 0.0;
    }

    //! Picks the phase label and the timer it describes; the HUD only redraws when one of them changes
    protected void UpdateMatchPhase(MissionGameplay gameplay, BattleRoyaleRPC br_rpc)
    {
		int phase_seconds = br_rpc.GetMatchPhaseSecondsLeft();
		int label = BattleRoyaleHud.PHASE_LABEL_NONE;
		int seconds = -1;

		switch ( br_rpc.match_phase )
		{
			case EBRMatchPhase.LOBBY:
				label = BattleRoyaleHud.PHASE_LABEL_LOBBY;
				seconds = phase_seconds;
				break;

			case EBRMatchPhase.SPAWN_SELECTION:
				label = BattleRoyaleHud.PHASE_LABEL_SPAWN_SELECTION;
				seconds = phase_seconds;
				break;

			case EBRMatchPhase.PREPARE:
				label = BattleRoyaleHud.PHASE_LABEL_PREPARING;
				seconds = phase_seconds;
				break;

			case EBRMatchPhase.STARTING:
				// The phase timer runs until players are unlocked, then the countdown runs until the first zone
				if ( phase_seconds > 0 )
				{
					label = BattleRoyaleHud.PHASE_LABEL_MATCH_STARTS_IN;
					seconds = phase_seconds;
				}
				else if ( phase_seconds == 0 && i_SecondsRemaining > 0 )
				{
					label = BattleRoyaleHud.PHASE_LABEL_FIRST_ZONE_IN;
					seconds = i_SecondsRemaining;
				}
				else
				{
					// Unknown time (-1), or no countdown to show yet
					label = BattleRoyaleHud.PHASE_LABEL_STARTING;
				}
				break;

			case EBRMatchPhase.ROUND:
				// The countdown runs until the zone locks, then until the end of the round
				if ( IsZoneLocked() )
				{
					label = BattleRoyaleHud.PHASE_LABEL_ROUND_ENDS_IN;
				}
				else
				{
					label = BattleRoyaleHud.PHASE_LABEL_ZONE_CLOSING;
				}

				if ( i_SecondsRemaining > 0 )
				{
					seconds = i_SecondsRemaining;
				}
				else if ( phase_seconds > 0 )
				{
					// No countdown received (e.g. after a reconnect): the phase timer runs until the end of the round
					label = BattleRoyaleHud.PHASE_LABEL_ROUND_ENDS_IN;
					seconds = phase_seconds;
				}
				break;

			case EBRMatchPhase.LAST_ROUND:
				if ( IsZoneCollapsed( br_rpc ) )
				{
					label = BattleRoyaleHud.PHASE_LABEL_NO_SAFE_ZONE;
				}
				else if ( i_SecondsRemaining > 0 )
				{
					label = BattleRoyaleHud.PHASE_LABEL_ZONE_DISAPPEARS_IN;
					seconds = i_SecondsRemaining;
				}
				else
				{
					label = BattleRoyaleHud.PHASE_LABEL_FINAL_ZONE;
				}
				break;

			case EBRMatchPhase.FINISHED:
				label = BattleRoyaleHud.PHASE_LABEL_MATCH_OVER;
				break;

			case EBRMatchPhase.RESTARTING:
				label = BattleRoyaleHud.PHASE_LABEL_RESTARTING;
				seconds = phase_seconds;
				break;
		}

		gameplay.UpdateMatchPhase( label, seconds );
    }

    protected bool GetZoneDistance(PlayerBase player, BattleRoyalePlayArea play_area, out float distExt, out float distInt, out float angle)
    {
        vector center = play_area.GetCenter();
        vector playerpos = player.GetPosition();

        //2d distance check
        center[1] = 0;
        playerpos[1] = 0;
        float distance_from_center = vector.Distance(center, playerpos);
        distExt = distance_from_center - play_area.GetRadius();
        distInt = Math.AbsFloat(distance_from_center);
        vector playerdir = vector.Direction(playerpos, center);
		angle = Math.NormalizeAngle(360 - ( GetGame().GetCurrentCameraDirection().VectorToAngles()[0] - playerdir.VectorToAngles()[0] ) );

        return distExt < 0;
    }

    protected void PlayerCountChanged(int nb_players, int nb_groups)
    {
        MissionGameplay gameplay = MissionGameplay.Cast( GetGame().GetMission() );
        if (gameplay)
        {
            //BattleRoyaleUtils.Trace(string.Format("PlayerCountChanged: %1 %2", nb_players, nb_groups));
            gameplay.UpdatePlayerCount( nb_players, nb_groups );
        }
    }

    protected void FadeIn(PlayerBase player)
    {
        BattleRoyaleUtils.Trace("BattleRoyale: FADE IN!");

        PPERequesterBank.GetRequester(PPERequester_BurlapSackEffects).Start();
        player.SetInventorySoftLock(true);
        player.SetMasterAttenuation("BurlapSackAttenuation");
    }

    protected void FadeOut(PlayerBase player)
    {
        BattleRoyaleUtils.Trace("BattleRoyale: FADE OUT!");

        PPERequesterBank.GetRequester(PPERequester_BurlapSackEffects).Stop();
        player.SetInventorySoftLock(false);
        player.SetMasterAttenuation("");
    }

    protected void OnSecond()
    {
        MissionGameplay gameplay = MissionGameplay.Cast( GetGame().GetMission() );
        if (!gameplay)
            return;

        if(i_SecondsRemaining > 0)
        {
            i_SecondsRemaining--;
            gameplay.UpdateCountdownTimer(i_SecondsRemaining);
        }

        // If another system stopped the outside zone effect, queue it again on the next frame
        if ( i_OutsideZoneEffectState == 1 )
        {
            PPERequesterBase outside_zone_requester = PPERequesterBank.GetRequester(PPERequesterBank.REQ_BATTLEROYALE);
            if ( outside_zone_requester && !outside_zone_requester.IsRequesterRunning() )
                i_OutsideZoneEffectState = -1;
        }
    }

    protected void AddPlayerKilled(int increase)
    {
        i_Kills += increase;
        MissionGameplay gameplay = MissionGameplay.Cast( GetGame().GetMission() );
        gameplay.UpdateKillCount(i_Kills);
    }

    protected void OnMatchStarted()
    {
        if(b_MatchStarted)
        {
            Error("Match started already but received another RPC?");
        }
        VONManager.GetInstance().SetMaxVolume( VoiceLevelShout );
        //VONManager.GetInstance().EnableVoice( true );
        b_MatchStarted = true;
    }

    void ReadyUp()
    {
        //if(b_IsReady)
        //    return; //already ready!

        b_IsReady = true; //this only runs once

        PlayerBase player = PlayerBase.Cast( GetGame().GetPlayer() );
        ref Param1<bool> ready_state = new Param1<bool>( true );  //perhaps this can be made togglable?
        GetRPCManager().SendRPC( RPC_DAYZBRSERVER_NAMESPACE, "PlayerReadyUp", ready_state, false , NULL, player);

    }

    void Unstuck()
    {
        PlayerBase player = PlayerBase.Cast( GetGame().GetPlayer() );
        GetRPCManager().SendRPC( RPC_DAYZBRSERVER_NAMESPACE, "PlayerUnstuck", NULL, true , NULL, player);

    }

    void StartMatch(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
    {
        if ( type == CallType.Client )
        {
            OnMatchStarted();
        }
    }

    override void OnPlayerTick(PlayerBase player, float timeslice)
    {
        //unused
    }

    override void OnPlayerKilled(PlayerBase killed, Object killer)
    {
        //unused
    }
}
#endif

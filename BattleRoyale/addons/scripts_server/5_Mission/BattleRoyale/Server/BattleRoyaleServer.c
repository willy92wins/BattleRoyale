#ifdef SERVER
class BattleRoyaleServer: BattleRoyaleBase
{
	protected static BattleRoyaleServer m_Instance;
    ref array<ref BattleRoyaleState> m_States;
    int i_CurrentStateIndex;

    int i_NumRounds;

    bool b_EnableSpawnSelectionMenu;

    string match_uuid;

    protected ref map<string, ref Timer> m_LateJoinTimers;

    protected ref BattleRoyaleSpectators m_Spectators;

    void BattleRoyaleServer()
    {
        GetRPCManager().AddRPC( RPC_DAYZBRSERVER_NAMESPACE, "PlayerReadyUp", this);
        GetRPCManager().AddRPC( RPC_DAYZBRSERVER_NAMESPACE, "PlayerUnstuck", this);
        GetRPCManager().AddRPC( RPC_DAYZBRSERVER_NAMESPACE, "SpectateCycle", this);
#ifdef VPPADMINTOOLS
        GetRPCManager().AddRPC( RPC_DAYZBRSERVER_NAMESPACE, "NextState", this, SingleplayerExecutionType.Server);
#endif

        Init();
    }

    void ~BattleRoyaleServer()
    {
        if (m_LateJoinTimers)
        {
            foreach (string steamid, Timer kickTimer : m_LateJoinTimers)
            {
                if (kickTimer && kickTimer.IsRunning())
                    kickTimer.Stop();
            }
            m_LateJoinTimers.Clear();
        }
    }

    void Init()
    {
		m_Instance = this;

        BattleRoyaleUtils.Trace("BattleRoyaleServer() Init()");
#ifdef VPPADMINTOOLS
        GetPermissionManager().AddPermissionType({ "MenuBattleRoyaleManager", "BattleRoyale:Next", "BattleRoyale:Pause", "BattleRoyale:Resume" });
#endif

        BattleRoyaleConfig config_data = BattleRoyaleConfig.GetConfig();
        BattleRoyaleServerData m_ServerData = config_data.GetServerData();

		if ( m_ServerData.enable_vigrid_api )
		{
			LockServerWebhook serverWebhook = new LockServerWebhook( m_ServerData.webhook_jwt_token );
			serverWebhook.UnlockServer();
		}

		if ( m_ServerData.enable_vigrid_api )
		{
			BattleRoyaleUtils.Trace("Requesting match uuid from the Vigrid API");
			CreateMatchWebhook createMatchWebhook = new CreateMatchWebhook( m_ServerData.webhook_jwt_token, m_ServerData.server_password );
			match_uuid = createMatchWebhook.getMatchUUID();

			if ( match_uuid.Length() != 36 )
			{
				match_uuid = "";
				BattleRoyaleUtils.Warn("Failed to get match uuid. Continuing without one.");
			}
			BattleRoyaleUtils.Trace("Match UUID: " + match_uuid);
        } else {
        	match_uuid = "disabled";  // Set a string to disable the ingame error message if no uuid is set
        }

        m_LateJoinTimers = new map<string, ref Timer>;
        m_Spectators = new BattleRoyaleSpectators;

        //load config (this may error because GetBattleRoyale would return false)
        BattleRoyaleGameData m_GameData = config_data.GetGameData();
        i_NumRounds = m_GameData.num_zones;
        b_EnableSpawnSelectionMenu = m_GameData.enable_spawn_selection_menu;

        //--- initialize all states (in order from start to finish)
        m_States = new array<ref BattleRoyaleState>;

        // (1) DEBUG ZONE
        BattleRoyaleDebug debug_state = new BattleRoyaleDebug;
        m_States.Insert(debug_state); //insert debug state

        // (2) PLAYER COUNT REACHED COUNTDOWN
        BattleRoyaleCountReached count_reached = new BattleRoyaleCountReached;
        m_States.Insert(count_reached);

        // (3) SPAWN SELECTION MENU
        if (b_EnableSpawnSelectionMenu)
		{
			BattleRoyaleSpawnSelection spawn_selection = new BattleRoyaleSpawnSelection;
			m_States.Insert(spawn_selection);
		}

        // (4) PREPARE CLIENTS & TELEPORT
        BattleRoyalePrepare prepare_clients = new BattleRoyalePrepare;
        m_States.Insert(prepare_clients);

        // (5) UNLOCK CLIENTS AND START MATCH WOO
        BattleRoyaleStartMatch start_match = new BattleRoyaleStartMatch;
        m_States.Insert(start_match);

		// (6) ROUNDS
        int num_states = m_States.Count();
        for(int i = 0; i < i_NumRounds; i++)
        {
            BattleRoyaleUtils.Trace("Add Round " + i);
            int prev_state_ind = i + num_states - 1;
            BattleRoyaleState previous_state = m_States[prev_state_ind];
            BattleRoyaleRound round = new BattleRoyaleRound(previous_state);
            m_States.Insert(round);
        }

        // (7) LAST ROUND
        BattleRoyaleLastRound last_round = new BattleRoyaleLastRound(m_States[m_States.Count() - 1]);
        m_States.Insert(last_round);

        // (8) WINNING PLAYER/TEAM
        m_States.Insert(new BattleRoyaleWin);

        // (9) RESTART SERVER
        m_States.Insert(new BattleRoyaleRestart);

        i_CurrentStateIndex = 0;  // start at the first state
        GetCurrentState().Activate();  // activate the first state

        RandomizeServerEnvironment();

#ifdef BLUE_ZONE
        BattleRoyaleUtils.Trace("Instance BlueZone Server");
        vector blue_zone_pos = "14829.2 73 14572.3";
        blue_zone_pos[1] = GetGame().SurfaceY(blue_zone_pos[0], blue_zone_pos[2]) + 10;

        BattleRoyaleUtils.Trace(blue_zone_pos);

        GetGame().CreateObjectEx( "BlueZone", blue_zone_pos, ECE_NOLIFETIME );
#endif
    }

	static BattleRoyaleServer GetInstance()
	{
		return m_Instance;
	}

    override bool IsDebug()
    {
        BattleRoyaleState m_CurrentState = GetCurrentState();
        BattleRoyaleDebug m_Debug;

        if(Class.CastTo(m_Debug, m_CurrentState))
        {
            return true;
        }

        //not debug state, check if match is actually running!
        BattleRoyalePrepare m_Prep;

        if(Class.CastTo(m_Prep, m_CurrentState))
        {
            //we are in prep state! - consider this a debug state!
            return true;
        }
        return false;
    }

	const float CHECK_IS_COMPLETE = 0.1;  //seconds
	float m_TimeSinceLastTick = CHECK_IS_COMPLETE + 1;

    override void Update(float delta)
    {
        float timeslice = delta; //Legacy

        foreach(BattleRoyaleState state: m_States)
        {
            if(state)
                state.Update(timeslice);
            else
                Error("BAD STATE IN m_States!");
        }

		m_TimeSinceLastTick += delta;

        //--- transition states
        if (m_TimeSinceLastTick > CHECK_IS_COMPLETE)
        {
        	m_TimeSinceLastTick = 0;

			if (GetCurrentState().IsComplete()) //current state is complete
			{
				int next_index = GetNextStateIndex();
				if(next_index > 0)
				{
					BattleRoyaleState next_state = GetState(next_index);

					BattleRoyaleUtils.Trace("[State Machine] Leaving State `" + GetCurrentState().GetName() + "`");
					if(GetCurrentState().IsActive())
						GetCurrentState().Deactivate(); //deactivate old state

					ref array<PlayerBase> players = GetCurrentState().RemoveAllPlayers(); //remove players from old state
					for(int i = 0; i < players.Count(); i++) //can't use foreach because it doesn't play nice with null entries
					{
						if(players[i])
						{
							next_state.AddPlayer(players[i]); //add players to new state
						}
						else
						{
							Error("null player in RemoveAllPlayers result!");
						}
					}
					i_CurrentStateIndex = next_index;//move us to the next state
					BattleRoyaleUtils.Trace("[State Machine] Entering State `" + GetCurrentState().GetName() + "`");
					GetCurrentState().Activate(); //activate new state
					NotifyStatsWebhook(GetCurrentState());
				}
				else
				{
					Error("NEXT STATE IS NULL!");
				}
			}
        }
    }

    void OnPlayerConnected(PlayerBase player)
    {
        //Teleport player into debug zone
        BattleRoyaleUtils.Trace("Player " + player.BR_GetPlayerName() + " connected!"); //lets find out if respawning players end up here

        //Copy PlainID (steamid) to PlayerBase to avoid the disparition of PlayerIdentity (OnPlayerDisconnected)
        player.player_steamid = player.GetIdentity().GetPlainId();

        //Dirty way to sync server settings with the client | this should be converted into a generic "sync settings" function
        BattleRoyaleConfig config_data = BattleRoyaleConfig.GetConfig();

        // Characters created during a lobby state already stand in the lobby (OnClientPrepareEvent).
        // The rest (created during a match, or before a restart reached the lobby) are moved there.
        if ( !IsInLobbyArea( player.GetPosition() ) )
        {
            BattleRoyaleUtils.Trace("Player " + player.BR_GetPlayerName() + " created outside the lobby, teleporting");
            player.SetPosition( GetLobbySpawnPosition() );
        }

		float dir = Math.RandomFloat(0, 360);
		vector playerDir = vector.YawToVector(dir);
		player.SetDirection( Vector(playerDir[0], 0, playerDir[1]) );

        BattleRoyaleDebugState m_DebugStateObj;

        if(!Class.CastTo(m_DebugStateObj, GetCurrentState()))
        {
			//BAD VERY BAD!
			//This gives the player 15 seconds to finish his setup before we boot him. There may still be a chance it crashes.
			//Ideally the player should notify us when he is "ready" to be disconnected (I have no idea when that would be)

			//NOTE: calling this will immediately crash the server (as the player hasn't fully established his connection yet) GetGame().DisconnectPlayer(player.GetIdentity());

			if ( IsLateJoinAdmin( player.GetIdentity() ) )
			{
				BattleRoyaleUtils.Info("Admin " + player.BR_GetPlayerName() + " has connected during non-debug state, allowing connection.");
				BattleRoyaleState.SendMatchPhaseTo(player.GetIdentity());
				return; //allow admins to connect during non-debug state
			}

			Error("PLAYER CONNECTED DURING NON-DEBUG ZONE STATE!");
			// Backstop for a client that missed the notice sent during OnClientPrepareEvent
			GetRPCManager().SendRPC(RPC_DAYZBR_NAMESPACE, "MatchInProgress", new Param1<int>(GetMatchMinutesLeft()), true, player.GetIdentity());
			ScheduleLateJoinKick(player.GetIdentity());

            return;
        }

        // only add player if they connect during debug
        if( player.GetIdentity() )
            player.owner_id = player.GetIdentity().GetPlainId(); //cache their id (for connection loss)

        GetCurrentState().AddPlayer(player);

        // Lobby or lobby countdown, with the seconds left when the countdown already runs
        BattleRoyaleState.SendMatchPhaseTo(player.GetIdentity());

        if( match_uuid == "" )
        	GetCurrentState().MessagePlayerUntranslated( player, "STR_BR_MM_ERROR_REGISTERING_MATCH");
    }

    //! Random point on the ground within 5 m of the lobby center
    vector GetLobbySpawnPosition()
    {
        BattleRoyaleDebug lobby = BattleRoyaleDebug.Cast( GetState(0) );
        vector center = lobby.GetCenter();

        vector spawn_pos = "0 0 0";
        spawn_pos[0] = Math.RandomFloatInclusive((center[0] - 5), (center[0] + 5));
        spawn_pos[2] = Math.RandomFloatInclusive((center[2] - 5), (center[2] + 5));
        spawn_pos[1] = GetGame().SurfaceY(spawn_pos[0], spawn_pos[2]);
        return spawn_pos;
    }

    //! True when pos lies within the lobby radius (2D distance, as the lobby states check it) and not under the terrain
    bool IsInLobbyArea(vector pos)
    {
        BattleRoyaleDebug lobby = BattleRoyaleDebug.Cast( GetState(0) );
        if (!lobby)
            return false;

        vector center = lobby.GetCenter();
        float distance = vector.Distance( Vector( pos[0], 0, pos[2] ), Vector( center[0], 0, center[2] ) );
        if (distance > lobby.GetRadius())
            return false;

        return pos[1] >= GetGame().SurfaceY( pos[0], pos[2] ) - 0.5;
    }

    //! True while a match runs: players who connect now cannot join it
    bool IsMatchInProgress()
    {
        BattleRoyaleDebugState debugState;
        return !Class.CastTo(debugState, GetCurrentState());
    }

    //! Admins listed in general_settings.json; the only check available while a client prepares
    bool IsListedAdmin(PlayerIdentity identity)
    {
        if (!identity)
            return false;

        BattleRoyaleGameData gameSettings = BattleRoyaleConfig.GetConfig().GetGameData();
        return gameSettings && gameSettings.admins_steamid64 && gameSettings.admins_steamid64.Find(identity.GetPlainId()) != -1;
    }

    //! Admins may connect during a match; admin tool permissions are only known once the player is connected
    bool IsLateJoinAdmin(PlayerIdentity identity)
    {
        if (IsListedAdmin(identity))
            return true;

        return BRAdminAuthorizer.IsOperator(identity);
    }

    //! Estimate, in whole minutes, of how long the current match still lasts: the zone timers, ignoring
    //! pauses, zone travel offsets and a last round that only ends when one player or group is left
    int GetMatchMinutesLeft()
    {
        BattleRoyaleConfig config = BattleRoyaleConfig.GetConfig();
        BattleRoyaleGameData gameSettings = config.GetGameData();
        BattleRoyaleZoneData zoneSettings = config.GetZoneData();
        if (!gameSettings || !zoneSettings)
            return 0;

        int numZones = gameSettings.num_zones;
        int roundSeconds = 60 * gameSettings.round_duration_minutes;
        bool staticTimers = zoneSettings.shrink_type == 3 && zoneSettings.static_timers && zoneSettings.static_timers.Count() >= numZones;

        // Rounds still to play after the current state; static timer index 0 is the final zone
        int laterRoundsCount = numZones;
        bool withLastRound = true;
        int seconds = 0;

        BattleRoyaleState state = GetCurrentState();
        int phaseLeft = BattleRoyaleState.GetMatchPhaseSecondsLeft();

        BattleRoyaleRound round;
        BattleRoyaleLastRound lastRound;
        if (Class.CastTo(round, state))
        {
            if (phaseLeft > 0)
                seconds += phaseLeft;
            else
                seconds += round.i_RoundTimeInSeconds;

            laterRoundsCount = numZones - Math.Floor(round.zone_num);
        }
        else if (Class.CastTo(lastRound, state))
        {
            seconds += lastRound.i_RoundTimeInSeconds;
            laterRoundsCount = 0;
            withLastRound = false;
        }
        else if (state && (state.GetName() == "Win State" || state.GetName() == "Restart State"))
        {
            seconds += 60;
            laterRoundsCount = 0;
            withLastRound = false;
        }
        else
        {
            // Spawn selection, preparation or start: the first zone waits half a round before it starts
            seconds += gameSettings.spawn_selection_duration + gameSettings.spawn_selection_extra_time + 120 + roundSeconds / 2;
        }

        for (int i = 0; i < laterRoundsCount; i++)
        {
            if (staticTimers)
                seconds += zoneSettings.static_timers[i];
            else
                seconds += roundSeconds;
        }

        // The last round after every zone round runs the final zone timer again
        if (withLastRound)
        {
            if (staticTimers)
                seconds += zoneSettings.static_timers[0];
            else
                seconds += roundSeconds;
        }

        int minutes = (seconds + 59) / 60;
        if (minutes < 1)
            minutes = 1;
        return minutes;
    }

    void ScheduleLateJoinKick(PlayerIdentity identity)
    {
        if (!identity)
            return;

        string steamid = identity.GetPlainId();
        if (m_LateJoinTimers && m_LateJoinTimers.Contains(steamid))
        {
            Timer existing = m_LateJoinTimers.Get(steamid);
            if (existing && existing.IsRunning())
                existing.Stop();
            m_LateJoinTimers.Remove(steamid);
        }

        Timer kickTimer = new Timer;
        kickTimer.Run(30.0, this, "Disconnect", new Param1<PlayerIdentity>(identity), false);
        m_LateJoinTimers.Set(steamid, kickTimer);
    }

    void Disconnect(PlayerIdentity identity)
    {
        // The map is the only strong ref to the Timer that is running this callback.
        // Drop it on the next call-queue turn, after Timer.OnTimer finishes.
        if (identity)
            GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(this.RemoveLateJoinTimer, 0, false, identity.GetPlainId());

        GetGame().DisconnectPlayer( identity ); //can't directly call disconnectplayer with timer, so we use this method
    }

    void RemoveLateJoinTimer(string steamid)
    {
        if (!m_LateJoinTimers || !m_LateJoinTimers.Contains(steamid))
            return;

        Timer kickTimer = m_LateJoinTimers.Get(steamid);
        if (kickTimer && kickTimer.IsRunning())
            return;

        m_LateJoinTimers.Remove(steamid);
    }

    protected void NotifyStatsWebhook(BattleRoyaleState state)
    {
        if (!state)
            return;

        BattleRoyaleServerData serverData = BattleRoyaleConfig.GetConfig().GetServerData();
        if (!serverData || serverData.stats_webhook_url == "")
            return;

        string stateName = state.GetName();
        if (stateName == "Start Match State")
        {
            int players = 0;
            if (state.GetPlayers())
                players = state.GetPlayers().Count();
            BRStatsWebhook.Send(serverData.stats_webhook_url, "match_start", players, "");
        }
        else if (stateName == "Win State")
        {
            string winnerName = "";
            ref array<PlayerBase> winners = state.GetPlayers();
            if (winners)
            {
                for (int i = 0; i < winners.Count(); i++)
                {
                    PlayerBase winner = winners[i];
                    if (winner && winner.GetIdentity())
                        winnerName = winner.GetIdentity().GetName();
                }
            }
            BRStatsWebhook.Send(serverData.stats_webhook_url, "winner", 0, winnerName);
        }
    }

    void OnPlayerDisconnect(PlayerBase player, PlayerIdentity identity)
    {
		if ( GetCurrentState().ContainsPlayer(player) )
		{
		    if ( player.IsUnconscious() )
            {
                // We add a kill to the last damage source
                if ( player.last_unconscious_source )
                {
                    if ( player.last_unconscious_source.IsInherited( PlayerBase ) )
                    {
                        BattleRoyaleUtils.Info("Player " + player.BR_GetPlayerName() + " disconnected while unconscious, adding kill to last damage source.");

                        PlayerBase killer = PlayerBase.Cast( player.last_unconscious_source );
                        GetCurrentState().OnPlayerKilled( player, killer );
                    } else {
                        BattleRoyaleUtils.Info("Player " + player.BR_GetPlayerName() + " disconnected while unconscious, but the last damage source is not a player.");
                    }
                } else {
                    BattleRoyaleUtils.Info("Player " + player.BR_GetPlayerName() + " disconnected while unconscious, but there is no last damage source.");
                }

                // If the player is alive, we kill him
                BattleRoyaleUtils.Info("Player " + player.BR_GetPlayerName() + " disconnected while unconscious, killing him.");
                player.SetHealth("GlobalHealth", "Health", 0);
                player.SetHealth("GlobalHealth", "Blood", 0);
            }
		}
    }

    void OnPlayerDisconnected(PlayerBase player, PlayerIdentity identity)
    {
        if(GetCurrentState().ContainsPlayer(player))
        {
            //if we are in a round, then we need to call OnPlayerDisconnected (since it's not a state based function we must cast)
            if(i_CurrentStateIndex > 2 && i_CurrentStateIndex < m_States.Count() - 2 )
                GetCurrentState().OnPlayerDisconnected(player);

            GetCurrentState().RemovePlayer(player);
        }

        // Also when the state already removed the player (killed while unconscious on disconnect)
        if (m_Spectators)
        {
            m_Spectators.RetargetWatchersOf(player);
            m_Spectators.OnIdentityDisconnected(identity);
        }
    }

    override void OnPlayerKilled(PlayerBase killed, Object killer)
    {
        if(GetCurrentState().ContainsPlayer(killed))
        {
            //if we are in a round, then we need to call onplayerkilled (since it's not a state based function we must cast)
            if(i_CurrentStateIndex > 2 && i_CurrentStateIndex < m_States.Count() - 2 )
                GetCurrentState().OnPlayerKilled(killed, killer);

            //remove player from the state (this would take place in on-disconnect, but some players would choose not to disconnect)
            GetCurrentState().RemovePlayer(killed);

            // Dead players of a running match watch the ones still alive
            if (m_Spectators && i_CurrentStateIndex > 2 && i_CurrentStateIndex < m_States.Count() - 2)
                m_Spectators.OnPlayerDied(killed);
        }

        // Whoever watched the dead player moves on, even if the state already removed them
        if (m_Spectators)
            m_Spectators.RetargetWatchersOf(killed);
    }

    //! Spectator asks for the next (1) or previous (-1) alive player
    void SpectateCycle(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
    {
        Param1<int> data;
        if (!ctx.Read(data))
            return;

        if (type == CallType.Server && m_Spectators)
            m_Spectators.Cycle(sender, data.param1);
    }

    ref array<PlayerBase> temp_disconnecting;
    override void OnPlayerTick(PlayerBase player, float timeslice)
    {
        // Spectator anchor bodies are not match players
        if (m_Spectators && m_Spectators.IsAnchor(player))
            return;

        if(!temp_disconnecting)
        {
            temp_disconnecting = new array<PlayerBase>();
        }

        if(GetCurrentState().ContainsPlayer(player))
        {
            GetCurrentState().OnPlayerTick(player, timeslice);

            //--- check if they have entered an invalid position
            vector pos = player.GetPosition();
            float bigNum = 1000000;
            //when invalid, height gets fucked, lets check that (others are NaN & may cause errors)
            if(pos[1] > bigNum || pos[1] < (-1 * bigNum))
            {
                if(temp_disconnecting.Find(player) == -1)
                {
                    temp_disconnecting.Insert(player);
                    //GetGame().DisconnectPlayer( player.GetIdentity() );
                    GetGame().SendLogoutTime(player, 0);
                    //BattleRoyaleUtils.Trace(pos);
                    //Error("PLAYER FOUND IN INVALID POSITION!");
                }
            }
        }
        else
        {
            //current state does not contain player, wtf is going on
            int life_state = player.GetPlayerState();
            if(life_state == EPlayerStates.ALIVE)
            {
                if(player && player.GetIdentity())
                {
					if ( temp_disconnecting.Find(player) == -1 )
					{
						//this ensures we only call disconnect on this player once
						temp_disconnecting.Insert(player);

						GetGame().SendLogoutTime(player, 0);
						//GetGame().DisconnectPlayer( player.GetIdentity() );
						//Error("GetCurrentState() DOES NOT CONTAIN PLAYER TICKING!");
					}
                }

            }
            //any other case here, the player is dead & therefore shouldn't count towards any state
        }

    }

    BattleRoyaleState GetState(int index)
    {
        if(index < 0 || index >= m_States.Count())
            return NULL;

        return m_States[index];
    }

    BattleRoyaleState GetCurrentState()
    {
        return GetState(i_CurrentStateIndex);
    }

    int GetNextStateIndex()
    {
        if(m_States.Count() <= (i_CurrentStateIndex + 1))
            return -1;

        for(int i = i_CurrentStateIndex + 1; i < m_States.Count(); i++)
        {
            BattleRoyaleState state = m_States[i];
            if( !state.SkipState(GetState(i_CurrentStateIndex)) )
            {
                return i;
            } else {
                BattleRoyaleUtils.Trace("[State Machine] Skipping State `" + state.GetName() + "`");
            }
        }

        return -1;
    }

    protected PlayerBase ResolveSenderPlayer(PlayerIdentity sender, Object target)
    {
        if (!sender)
        {
            BattleRoyaleUtils.Warn("RPC rejected: null sender");
            return null;
        }

        string steamid = sender.GetPlainId();
        PlayerBase player = PlayerBase.Cast(sender.GetPlayer());
        if (!player)
        {
            BattleRoyaleUtils.Warn("RPC rejected: sender has no player, steamid=" + steamid);
            return null;
        }

        if (target != player)
        {
            BattleRoyaleUtils.Warn("RPC rejected: target is not the sender, steamid=" + steamid);
            return null;
        }

        return player;
    }

    void PlayerReadyUp(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
    {
        Param1< bool > data;
        if( !ctx.Read( data ) ) return;

        if(type == CallType.Server)
        {
            PlayerBase targetBase = ResolveSenderPlayer(sender, target);
            if (!targetBase)
                return;

            BattleRoyaleDebug m_DebugStateObj;
            if(!Class.CastTo(m_DebugStateObj, GetCurrentState())) //this ensures we can only ready up during the debug state
                return;

            if(!m_DebugStateObj.ContainsPlayer(targetBase))
            {
                Error("Debug state does not contain player requesting ready up!");
                return;
            }

            if(data.param1)
            {
                m_DebugStateObj.ReadyUp(targetBase);
            }
            else
            {
                //perhaps allow readyup to be toggled?
            }

        }
    }

    void PlayerUnstuck(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
    {
		if(type == CallType.Server)
		{
			if (!BattleRoyaleConfig.GetConfig().GetGameData().enable_unstuck)
				return;

			PlayerBase targetBase = ResolveSenderPlayer(sender, target);
			if (!targetBase)
				return;

			BattleRoyaleStartMatch m_StartMatchStateObj;
			if(!Class.CastTo(m_StartMatchStateObj, GetCurrentState()))
				return;

            if(!m_StartMatchStateObj.ContainsPlayer(targetBase))
            {
                Error("StartMatch state does not contain player requesting unstuck!");
                return;
            }

			m_StartMatchStateObj.DeferredUnstuck( targetBase );
			targetBase.SetSynchDirty();
		}
    }

    //TODO: This will need a rework!
    void RandomizeServerEnvironment()
    {
        BattleRoyaleUtils.Trace("BattleRoyale: Randomizing Environment!");
        //NOTE: this is all legacy, we should find a better way to do this
        int year = 2018;
        int month = Math.RandomIntInclusive(3, 9); //march to september
        int day = 24;
        int hour = Math.RandomIntInclusive(8,16); //7am to 4pm
        int minute = 0;
        GetGame().GetWorld().SetDate(year, month, day, hour, minute);

		string world_name = GetGame().GetWorldName();
		world_name.ToLower();

		if (world_name != "takistanplus")
		{
			//Set Random Weather
			Weather weather = GetGame().GetWeather();

			weather.GetOvercast().SetLimits( 0.0 , 1.0 );
			weather.GetRain().SetLimits( 0.0 , 1.0 );
			weather.GetFog().SetLimits( 0.0 , 0.25 );

			weather.GetOvercast().SetForecastChangeLimits( 0.5, 0.8 );
			weather.GetRain().SetForecastChangeLimits( 0.1, 0.3 );
			weather.GetFog().SetForecastChangeLimits( 0.05, 0.10 );

			weather.GetOvercast().SetForecastTimeLimits( 3600 , 3600 );
			weather.GetRain().SetForecastTimeLimits( 300 , 300 );
			weather.GetFog().SetForecastTimeLimits( 3600 , 3600 );

			weather.GetOvercast().Set( Math.RandomFloatInclusive(0.0, 0.3), 0, 0);
			weather.GetRain().Set( Math.RandomFloatInclusive(0.0, 0.2), 0, 0);
			weather.GetFog().Set( Math.RandomFloatInclusive(0.0, 0.1), 0, 0);
        }
    }

    void NextState(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
    {
        BattleRoyaleUtils.Trace("BattleRoyaleManager NextState");
#ifdef SERVER
        if (type != CallType.Server)
            return;

        if (!BRAdminAuthorizer.IsAllowed(sender, "Next"))
            return;

        BRAdminController.Next();
#endif
    }
}
#endif

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
        GetRPCManager().AddRPC( RPC_DAYZBRSERVER_NAMESPACE, "SpectatorToLobby", this);
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
        m_CharacterTypes = new map<string, string>;

        // The actions that hurt or hold another player are blocked in the lobby (BRProtectedPlayerActions.c)
        BattleRoyaleSpawnsData spawnsData = config_data.GetSpawnsData();
        if (spawnsData)
            ActionBase.BR_SetLobbyArea(spawnsData.spawn_point, spawnsData.radius);

        BRMatchDebris.s_Enabled = m_ServerData.UsesSerialMatches();
        if (BRMatchDebris.s_Enabled)
            BattleRoyaleUtils.Info("[Serial] Serial matches on: the server stays up between matches (full restart every " + m_ServerData.serial_restart_every_matches + " matches, 0 = never)");

        m_States = BuildStates();

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

    //! The states of one match, in order from start to finish; Init builds the first match, a serial match turnover the next ones
    protected ref array<ref BattleRoyaleState> BuildStates()
    {
        //load config (this may error because GetBattleRoyale would return false)
        BattleRoyaleGameData m_GameData = BattleRoyaleConfig.GetConfig().GetGameData();
        i_NumRounds = m_GameData.num_zones;
        b_EnableSpawnSelectionMenu = m_GameData.enable_spawn_selection_menu;

        ref array<ref BattleRoyaleState> states = new array<ref BattleRoyaleState>;

        // (1) DEBUG ZONE
        BattleRoyaleDebug debug_state = new BattleRoyaleDebug;
        states.Insert(debug_state); //insert debug state

        // (2) PLAYER COUNT REACHED COUNTDOWN
        BattleRoyaleCountReached count_reached = new BattleRoyaleCountReached;
        states.Insert(count_reached);

        // (3) SPAWN SELECTION MENU
        if (b_EnableSpawnSelectionMenu)
		{
			BattleRoyaleSpawnSelection spawn_selection = new BattleRoyaleSpawnSelection;
			states.Insert(spawn_selection);
		}

        // (4) PREPARE CLIENTS & TELEPORT
        BattleRoyalePrepare prepare_clients = new BattleRoyalePrepare;
        states.Insert(prepare_clients);

        // (5) UNLOCK CLIENTS AND START MATCH WOO
        BattleRoyaleStartMatch start_match = new BattleRoyaleStartMatch;
        states.Insert(start_match);

		// (6) ROUNDS
        int num_states = states.Count();
        for(int i = 0; i < i_NumRounds; i++)
        {
            BattleRoyaleUtils.Trace("Add Round " + i);
            int prev_state_ind = i + num_states - 1;
            BattleRoyaleState previous_state = states[prev_state_ind];
            BattleRoyaleRound round = new BattleRoyaleRound(previous_state);
            states.Insert(round);
        }

        // (7) LAST ROUND
        BattleRoyaleLastRound last_round = new BattleRoyaleLastRound(states[states.Count() - 1]);
        states.Insert(last_round);

        // (8) WINNING PLAYER/TEAM
        states.Insert(new BattleRoyaleWin);

        // (9) RESTART SERVER
        states.Insert(new BattleRoyaleRestart);

        return states;
    }

	static BattleRoyaleServer GetInstance()
	{
		return m_Instance;
	}

    //--- Serial matches (server_settings.json serial_matches)

    //! Matches ended in this server run
    protected static int s_MatchesEnded = 0;
    protected ref BRMatchTurnover m_Turnover;
    //! A match ended while the previous turnover was still cleaning: served when that turnover ends
    protected bool m_TurnoverPending;
    //! The current turnover aborted and the server is exiting: a pending request is dropped with it
    protected bool m_TurnoverAborted;
    //! identity id -> character class of the last body, so a spectator returns to the lobby with the same look
    protected ref map<string, string> m_CharacterTypes;

    //! Called by the restart state when a match is over: true when the server stays up and the next match follows,
    //! false when it must exit as without serial matches (setting off, ignored, or the scheduled full restart)
    bool RequestTurnover()
    {
        if (m_Turnover)
        {
            // A match cannot end before the lobby opens, so this is the clean phase outliving the next
            // match: keep the request instead of dropping it, Update serves it when this turnover ends
            m_TurnoverPending = true;
            return true;
        }

        BattleRoyaleServerData serverData = BattleRoyaleConfig.GetConfig().GetServerData();
        if (!serverData || !serverData.UsesSerialMatches())
            return false;

        s_MatchesEnded++;
        if (serverData.serial_restart_every_matches > 0 && s_MatchesEnded >= serverData.serial_restart_every_matches)
        {
            BattleRoyaleUtils.Info("[Serial] Match " + s_MatchesEnded + " ended: scheduled full restart");
            return false;
        }

        m_Turnover = new BRMatchTurnover(this, s_MatchesEnded);
        return true;
    }

    //! True when the server exits after the match in progress: always without serial matches, and on the match that
    //! reaches serial_restart_every_matches with them. Players are warned in the last round and at the end
    bool WillRestartAfterMatch()
    {
        if (!BRMatchDebris.s_Enabled)
            return true;

        BattleRoyaleServerData serverData = BattleRoyaleConfig.GetConfig().GetServerData();
        return serverData && serverData.serial_restart_every_matches > 0 && s_MatchesEnded + 1 >= serverData.serial_restart_every_matches;
    }

    bool IsTurnoverRunning()
    {
        return m_Turnover != null;
    }

    //! Turnover step 1: nothing of the finished match may run once the next one starts
    void Turnover_CancelOldMatch()
    {
        foreach (BattleRoyaleState state : m_States)
        {
            if (state)
                state.CancelPendingWork();
        }

        if (m_LateJoinTimers)
        {
            foreach (string steamid, Timer kickTimer : m_LateJoinTimers)
            {
                if (kickTimer && kickTimer.IsRunning())
                    kickTimer.Stop();
            }
            m_LateJoinTimers.Clear();
        }
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).Remove(this.RemoveLateJoinTimer);
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).Remove(this.MoveToWaitingRoomById);

        if (m_Spectators)
            m_Spectators.CancelPending();

        if (temp_disconnecting)
            temp_disconnecting.Clear();

        if (m_LastLobbyRequest)
            m_LastLobbyRequest.Clear();
    }

    //! True while the preparation thread of the finished match still runs: the next match must not start under it
    bool Turnover_IsPrepareRunning()
    {
        foreach (BattleRoyaleState state : m_States)
        {
            BattleRoyalePrepare prepare;
            if (Class.CastTo(prepare, state) && prepare.IsProcessing())
                return true;
        }
        return false;
    }

    //! Turnover step 2: new states, zones and match data, then the lobby opens.
    //! The states of the finished match are released, not deactivated: their timers stop with them
    void Turnover_BuildNextMatch()
    {
        BattleRoyaleState.ResetMatchStatics();
        BattleRoyaleZone.ResetMatch();

        m_States = BuildStates();
        i_CurrentStateIndex = 0;
        m_TimeSinceLastTick = CHECK_IS_COMPLETE + 1;

        BattleRoyaleDebug lobby = BattleRoyaleDebug.Cast( GetState(0) );
        if (lobby)
            lobby.SetWaitOrigin( GetGame().GetTickTime() );

        GetCurrentState().Activate();
        RandomizeServerEnvironment();
    }

    //! Results of Turnover_ReturnToLobby
    static const int RETURN_SKIPPED = 0;   //!< nothing to do for this identity
    static const int RETURN_DONE = 1;      //!< new lobby character, in the lobby state
    static const int RETURN_FAILED = -1;   //!< the identity is still connected without a lobby character

    //! Turnover step 3: a connected player gets a new character in the lobby. Skipped for an identity still connecting
    //! (the normal connection puts it in the lobby) or already in the lobby; failed when no lobby character came out of it
    int Turnover_ReturnToLobby(PlayerIdentity identity)
    {
        if (!identity)
            return RETURN_SKIPPED;

        PlayerBase previous = PlayerBase.Cast( identity.GetPlayer() );
        bool spectating = m_Spectators && m_Spectators.IsSpectator(identity);
        if (!previous && !spectating)
            return RETURN_SKIPPED;

        // Already a lobby player (an admin who joined the lobby early, for one): nothing to replace
        if (previous && previous.IsAlive() && previous.br_in_current_state && GetCurrentState().ContainsPlayer(previous))
            return RETURN_SKIPPED;

        // A player of the waiting room already stands in the lobby with a lobby character: it joins the new lobby as it is
        if (previous && previous.IsAlive() && previous.br_waiting)
        {
            if (!JoinLobbyFromWaitingRoom(previous, identity))
                return RETURN_FAILED;
            return RETURN_DONE;
        }

        PlayerBase fresh = GiveLobbyCharacter(identity, previous);
        if (!fresh)
            return RETURN_FAILED;

        if (!fresh.br_in_current_state || !GetCurrentState().ContainsPlayer(fresh))
        {
            BattleRoyaleUtils.Warn("[Serial] " + identity.GetName() + " got a lobby character outside the lobby state");
            return RETURN_FAILED;
        }

        return RETURN_DONE;
    }

    //! A connected player gets a new character in the lobby, without leaving the server: the client resets its match
    //! state first (ReturnToLobby), so the phase and player count sent while the character connects land on the fresh
    //! state. The character connects through OnPlayerConnected: into the lobby state between matches, into the waiting
    //! room during one. The spectator anchor and a living previous body go away; a dead body is a recorded leftover
    protected PlayerBase GiveLobbyCharacter(PlayerIdentity identity, PlayerBase previous)
    {
        MissionServer mission = MissionServer.Cast( GetGame().GetMission() );
        if (!mission || !identity)
            return null;

        string character_type = "SurvivorM_Mirek";
        if (previous)
            character_type = previous.GetType();
        else if (m_CharacterTypes.Contains(identity.GetId()))
            character_type = m_CharacterTypes.Get(identity.GetId());

        GetRPCManager().SendRPC( RPC_DAYZBR_NAMESPACE, "ReturnToLobby", NULL, true, identity );

        PlayerBase fresh = mission.BR_CreateLobbyCharacter(identity, character_type);
        if (!fresh)
            return null;

        if (m_Spectators)
            m_Spectators.Release(identity);

        if (previous && previous.IsAlive() && previous != fresh)
            GetGame().ObjectDelete(previous);

        return fresh;
    }

    //--- Waiting room (general_settings.json enable_waiting_room): during a match, players who joined late, died with
    //--- nobody to watch or left spectating wait in the lobby, outside the match states, for the next match

    //! identity id -> tick time of the last accepted SpectatorToLobby request
    protected ref map<string, float> m_LastLobbyRequest;
    //! True while MoveToWaitingRoom creates the lobby character: OnPlayerConnected then puts an admin in the waiting room too
    protected bool m_ForceWaitingRoom;

    bool IsWaitingRoomEnabled()
    {
        BattleRoyaleGameData gameData = BattleRoyaleConfig.GetConfig().GetGameData();
        return gameData && gameData.enable_waiting_room;
    }

    //! Called by OnPlayerConnected for a player who connects, or gets a lobby character, while a match runs
    protected void EnterWaitingRoom(PlayerBase player)
    {
        player.br_waiting = true;
        player.owner_id = player.GetIdentity().GetPlainId();
        player.SetAllowDamage(false);
        player.Heal();
        if ( !IsInLobbyArea( player.GetPosition() ) )
            player.SetPosition( GetLobbySpawnPosition() );

        BattleRoyaleUtils.Info("Waiting room: " + player.BR_GetPlayerName() + " waits in the lobby for the next match");
        // The client reset its HUD when this character was made: it learns that it waits (no zone warnings) and the
        // player count, broadcast only when it changes
        ResyncWaitingPlayer(player);
        // With a restart after this match the player will have to join again: the notice says so
        string notice = "STR_BR_WAITING_ROOM";
        if (WillRestartAfterMatch())
            notice = "STR_BR_WAITING_ROOM_RESTART";
        // Enforce cannot call ToString on a call result: the minutes go through a local first
        int minutesLeft = GetMatchMinutesLeft();
        GetCurrentState().MessagePlayerUntranslated(player, notice, minutesLeft.ToString());
    }

    //! Resends phase, waiting flag and player count to a waiting client whose mission restarted (reconnect):
    //! no notice, teleport or heal, the body never left the lobby
    void ResyncWaitingPlayer(PlayerBase player)
    {
        if (!player || !player.GetIdentity())
            return;

        BattleRoyaleState.SendMatchPhaseTo(player.GetIdentity());
        GetRPCManager().SendRPC( RPC_DAYZBR_NAMESPACE, "SetWaiting", new Param1<bool>( true ), true, player.GetIdentity() );
        GetCurrentState().SendPlayerCountTo(player.GetIdentity());
        // The map of a waiting player still shows the zones of the match
        SendZoneStateTo(player.GetIdentity());
    }

    //! A player of the waiting room joins the lobby state with its lobby character, after its client reset its match
    //! state. False when the lobby state did not take it
    protected bool JoinLobbyFromWaitingRoom(PlayerBase player, PlayerIdentity identity)
    {
        player.br_waiting = false;
        GetRPCManager().SendRPC( RPC_DAYZBR_NAMESPACE, "ReturnToLobby", NULL, true, identity );
        player.owner_id = identity.GetPlainId();
        GetCurrentState().AddPlayer(player);
        BattleRoyaleState.SendMatchPhaseTo(identity);
        return player.br_in_current_state && GetCurrentState().ContainsPlayer(player);
    }

    //! A waiting player stays inside the lobby radius, immune and healed, like a lobby player
    protected void TickWaitingPlayer(PlayerBase player, float timeslice)
    {
        if (!player.IsAlive())
            return;

        // The match is over and the turnover did not take this player (it reconnected after it, for one): it joins
        // the lobby now instead of waiting through the next match
        if (!IsMatchInProgress() && !m_Turnover && player.GetIdentity())
        {
            BattleRoyaleUtils.Info("Waiting room: " + player.BR_GetPlayerName() + " joins the lobby after the match");
            JoinLobbyFromWaitingRoom(player, player.GetIdentity());
            return;
        }

        if ( !IsInLobbyArea( player.GetPosition() ) )
            player.SetPosition( GetLobbySpawnPosition() );

        if (player.time_until_heal <= 0)
        {
            BattleRoyaleGameData gameData = BattleRoyaleConfig.GetConfig().GetGameData();
            player.time_until_heal = DAYZBR_DEBUG_HEAL_TICK;
            if (gameData)
                player.time_until_heal = gameData.debug_heal_tick_seconds;
            if (BattleRoyaleState.VitalsNeedHeal(player))
                player.Heal();
        }
        player.time_until_heal -= timeslice;
    }

    //! Moves a connected player out of the running match into the waiting room, with a new lobby character.
    //! Does nothing between matches (the lobby takes the player) or with the setting off
    bool MoveToWaitingRoom(PlayerIdentity identity)
    {
        if (!identity || !IsWaitingRoomEnabled() || !IsMatchInProgress() || m_Turnover)
            return false;

        PlayerBase previous = PlayerBase.Cast( identity.GetPlayer() );
        if (previous && previous.IsAlive() && (previous.br_waiting || previous.br_in_current_state))
            return false;

        m_ForceWaitingRoom = true;
        PlayerBase fresh = GiveLobbyCharacter(identity, previous);
        m_ForceWaitingRoom = false;
        return fresh && fresh.br_waiting;
    }

    //! CallLater target: a dead player with nobody to watch, by identity id
    void MoveToWaitingRoomById(string identity_id)
    {
        array<PlayerIdentity> identities = new array<PlayerIdentity>;
        GetGame().GetPlayerIndentities(identities);
        foreach (PlayerIdentity identity : identities)
        {
            if (identity && identity.GetId() == identity_id)
            {
                MoveToWaitingRoom(identity);
                return;
            }
        }
    }

    //! Asked by a spectator's client (the ready key): leave spectating and wait in the lobby
    void SpectatorToLobby(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
    {
        if (type != CallType.Server || !sender || !m_Spectators || !m_Spectators.IsSpectator(sender))
            return;

        // One request every 2 s per identity; the rest are dropped
        string id = sender.GetId();
        float now = GetGame().GetTickTime();
        if (!m_LastLobbyRequest)
            m_LastLobbyRequest = new map<string, float>;
        if (m_LastLobbyRequest.Contains(id) && now - m_LastLobbyRequest.Get(id) < 2.0)
            return;
        m_LastLobbyRequest.Set(id, now);

        MoveToWaitingRoom(sender);
    }

    //! True while connected players are still being given their lobby characters: the lobby must not start a match yet
    bool IsReturningPlayers()
    {
        return m_Turnover && m_Turnover.IsReturning();
    }

    //! Turnover: sends the player count held back while a batch returned, after that batch's ReturnToLobby resets
    void Turnover_BroadcastPlayerCount()
    {
        BattleRoyaleState state = GetCurrentState();
        if (state)
            state.BroadcastPlayerCount();
    }

    //! Turnover: every player of the finished match is back in the lobby; the next match may now start and record its own deaths
    void Turnover_ReturnsDone()
    {
        if (m_CharacterTypes)
            m_CharacterTypes.Clear();
    }

    //! Turnover step 4: the finished match is gone; one line for the server owner
    void Turnover_Finished(int match_number, float seconds, int returned, BRMatchDebrisBatch debris)
    {
        string deleted = "nothing recorded";
        if (debris)
            deleted = debris.m_DeletedBodies.ToString() + " bodies, " + debris.m_DeletedItems.ToString() + " dropped items, " + debris.m_DeletedObjects.ToString() + " other objects";

        BattleRoyaleUtils.Info("[Serial] Match " + match_number + " done, next lobby open: turnover took " + seconds.ToString() + " s, " + returned + " players back in the lobby, deleted " + deleted);
    }

    //! The turnover could not finish: exit as without serial matches, the restart loop starts a clean server
    void Turnover_Abort(string reason)
    {
        // The server exits anyway, so a turnover request pending behind this one is dropped with it
        m_TurnoverAborted = true;
        Error("[Serial] Turnover aborted (" + reason + "), restarting the server");
        GetGame().RequestExit(0);
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

    //--- The lobby is kept free of infected: they cannot hurt its protected players, but they chase and hit them

    //! Seconds between two sweeps of the lobby for infected
    static const float LOBBY_INFECTED_SWEEP_SECONDS = 5;
    //! Metres past the lobby radius also swept, so an infected running in goes before it reaches the players
    static const float LOBBY_INFECTED_MARGIN = 30;
    protected float m_LobbyInfectedSweepTime;

    //! Removes the living infected within the lobby radius and its margin. While the lobby holds players: between
    //! matches, during the spawn selection and the preparation that moves them to the match, and later while a player
    //! of the waiting room is there, so a lobby that lies inside the play area keeps the infected of the match the rest
    //! of the time
    protected void SweepLobbyInfected()
    {
        BattleRoyaleDebug lobby = BattleRoyaleDebug.Cast( GetState(0) );
        if (!lobby)
            return;

        vector center = lobby.GetCenterXZ();
        float radius = lobby.GetRadius() + LOBBY_INFECTED_MARGIN;
        float ground = GetGame().SurfaceY(center[0], center[2]);
        vector boxMin = Vector(center[0] - radius, ground - 200, center[2] - radius);
        vector boxMax = Vector(center[0] + radius, ground + 200, center[2] + radius);

        array<EntityAI> entities = new array<EntityAI>;
        DayZPlayerUtils.SceneGetEntitiesInBox(boxMin, boxMax, entities, QueryFlags.DYNAMIC);

        bool lobbyInUse = !IsMatchInProgress();
        BattleRoyaleState current = GetCurrentState();
        if (BattleRoyaleSpawnSelection.Cast(current) || BattleRoyalePrepare.Cast(current))
            lobbyInUse = true;

        array<ZombieBase> infected = new array<ZombieBase>;
        foreach (EntityAI entity : entities)
        {
            PlayerBase waiting = PlayerBase.Cast(entity);
            if (waiting)
            {
                if (waiting.br_waiting && waiting.IsAlive())
                    lobbyInUse = true;
                continue;
            }

            ZombieBase zombie = ZombieBase.Cast(entity);
            if (!zombie || !zombie.IsAlive())
                continue;

            vector pos = zombie.GetPosition();
            if (vector.Distance(Vector(pos[0], 0, pos[2]), Vector(center[0], 0, center[2])) <= radius)
                infected.Insert(zombie);
        }

        if (!lobbyInUse || infected.Count() == 0)
            return;

        foreach (ZombieBase removed : infected)
            GetGame().ObjectDelete(removed);

        if (BattleRoyaleUtils.CheckLogLevel(BattleRoyaleUtils.DEBUG))
            BattleRoyaleUtils.Debug("Lobby: removed " + infected.Count() + " infected");
    }

    override void Update(float delta)
    {
        float timeslice = delta; //Legacy

        // A serial match turnover replaces m_States, so it runs before the states are iterated.
        // Released here, after Step returned, never from inside it
        if (m_Turnover && !m_Turnover.Step())
        {
            m_Turnover = null;

            // A match ended while this turnover was still cleaning: serve its request now, through the
            // same counting and scheduled-restart decision as a request with no turnover running
            if (m_TurnoverPending)
            {
                m_TurnoverPending = false;
                if (!m_TurnoverAborted && !RequestTurnover())
                    GetGame().RequestExit(0);
            }
            m_TurnoverAborted = false;
        }

        foreach(BattleRoyaleState state: m_States)
        {
            if(state)
                state.Update(timeslice);
            else
                Error("BAD STATE IN m_States!");
        }

        m_LobbyInfectedSweepTime += delta;
        if (m_LobbyInfectedSweepTime >= LOBBY_INFECTED_SWEEP_SECONDS)
        {
            m_LobbyInfectedSweepTime = 0;
            SweepLobbyInfected();
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

    //! The client flags a fresh client mission starts without: stay connected after death, and the waiting room
    //! exists. Sent on connect and again on reconnect, with the same conditions in both cases
    void SendClientFlags(PlayerIdentity identity)
    {
        if (!identity)
            return;

        // A dead player stays connected when the server takes care of it: spectating until a serial match ends, or the
        // waiting room. The client must then not leave 15 s after its death
        BattleRoyaleGameData stayGameData = BattleRoyaleConfig.GetConfig().GetGameData();
        bool waitingRoom = IsWaitingRoomEnabled();
        if (waitingRoom || (BRMatchDebris.s_Enabled && stayGameData && stayGameData.enable_spectator))
            GetRPCManager().SendRPC( RPC_DAYZBR_NAMESPACE, "SetStayConnected", new Param1<bool>( true ), true, identity );
        if (waitingRoom)
            GetRPCManager().SendRPC( RPC_DAYZBR_NAMESPACE, "SetWaitingRoom", new Param1<bool>( true ), true, identity );
    }

    //! A reconnected client restarted its mission and lost every server-sent flag and state, whatever the player
    //! was doing: resend the client flags to everyone, then the waiting resync or the plain phase, count and zones
    void OnPlayerReconnected(PlayerBase player)
    {
        if (!player || !player.GetIdentity())
            return;

        SendClientFlags(player.GetIdentity());
        if (player.IsAlive() && player.br_waiting)
        {
            ResyncWaitingPlayer(player);
        }
        else
        {
            BattleRoyaleState.SendMatchPhaseTo(player.GetIdentity());
            GetCurrentState().SendPlayerCountTo(player.GetIdentity());
            SendZoneStateTo(player.GetIdentity());
        }
    }

    //! Zones of the running match for one client that did not get them as they came (connect, reconnect, waiting room,
    //! spectating): the zone in force, the announced one and the closing under way with the time already gone.
    //! Nothing between matches, when no zone is in play
    void SendZoneStateTo(PlayerIdentity identity)
    {
        if (!identity)
            return;

        BattleRoyaleState state = GetCurrentState();
        BattleRoyaleRound round;
        BattleRoyaleLastRound last_round;
        BattleRoyaleStartMatch start_match;
        if (Class.CastTo(round, state))
            round.SendZoneStateTo(identity);
        else if (Class.CastTo(last_round, state))
            last_round.SendZoneStateTo(identity);
        else if (Class.CastTo(start_match, state))
            start_match.SendZoneStateTo(identity);
    }

    void OnPlayerConnected(PlayerBase player)
    {
        //Teleport player into debug zone
        BattleRoyaleUtils.Trace("Player " + player.BR_GetPlayerName() + " connected!"); //lets find out if respawning players end up here

        //Copy PlainID (steamid) to PlayerBase to avoid the disparition of PlayerIdentity (OnPlayerDisconnected)
        player.player_steamid = player.GetIdentity().GetPlainId();

        SendClientFlags(player.GetIdentity());

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

			// Waiting room: the player waits in the lobby for the next match. An admin who joins late keeps the free
			// connection below, unless the server is moving that admin out of the match (death, spectating)
			bool lateAdmin = IsLateJoinAdmin( player.GetIdentity() );
			// Consumed here: a script error later in this connection cannot leave it set for the next player
			bool forced = m_ForceWaitingRoom;
			m_ForceWaitingRoom = false;
			if ( IsWaitingRoomEnabled() && ( forced || !lateAdmin ) )
			{
				EnterWaitingRoom(player);
				return;
			}

			if ( lateAdmin )
			{
				BattleRoyaleUtils.Info("Admin " + player.BR_GetPlayerName() + " has connected during non-debug state, allowing connection.");
				BattleRoyaleState.SendMatchPhaseTo(player.GetIdentity());
				GetCurrentState().SendPlayerCountTo(player.GetIdentity());
				SendZoneStateTo(player.GetIdentity());
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

        vector center = lobby.GetCenterXZ();
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

    //! Stops and drops a pending late-join kick, keyed as ScheduleLateJoinKick keys it
    protected void CancelLateJoinKick(string steamid)
    {
        if (steamid == "" || !m_LateJoinTimers || !m_LateJoinTimers.Contains(steamid))
            return;

        Timer kickTimer = m_LateJoinTimers.Get(steamid);
        if (kickTimer && kickTimer.IsRunning())
            kickTimer.Stop();
        // The map is the only strong ref to the Timer: if DisconnectPlayer fired this from inside that timer's own
        // callback, removing it now would free the Timer while it runs, so drop it on the next call-queue turn
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(this.RemoveLateJoinTimer, 0, false, steamid);
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
        if (!identity)
            return;

        // The map is the only strong ref to the Timer that is running this callback.
        // Drop it on the next call-queue turn, after Timer.OnTimer finishes.
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
        // The identity may already be deleted here, so the steamid cached on connect is the fallback key
        if (player)
            CancelLateJoinKick(player.player_steamid);
        if (identity)
        {
            CancelLateJoinKick(identity.GetPlainId());
            if (m_LastLobbyRequest)
                m_LastLobbyRequest.Remove(identity.GetId());
        }

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
        // Serial matches: the body and its inventory are removed when the match is over
        if (killed && BRMatchDebris.s_Enabled)
            BRMatchDebris.Get().AddBody(killed);

        // The last character type is remembered with or without serial matches, so a spectator returns as itself
        if (killed && killed.GetIdentity() && m_CharacterTypes)
            m_CharacterTypes.Set(killed.GetIdentity().GetId(), killed.GetType());

        if(GetCurrentState().ContainsPlayer(killed))
        {
            //if we are in a round, then we need to call onplayerkilled (since it's not a state based function we must cast)
            if(i_CurrentStateIndex > 2 && i_CurrentStateIndex < m_States.Count() - 2 )
                GetCurrentState().OnPlayerKilled(killed, killer);

            //remove player from the state (this would take place in on-disconnect, but some players would choose not to disconnect)
            GetCurrentState().RemovePlayer(killed);

            // Dead players of a running match watch the ones still alive; with spectating off they wait in the lobby
            if (m_Spectators && i_CurrentStateIndex > 2 && i_CurrentStateIndex < m_States.Count() - 2)
            {
                if (m_Spectators.IsEnabled())
                    m_Spectators.OnPlayerDied(killed);
                else if (IsWaitingRoomEnabled() && killed.GetIdentity())
                    GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(this.MoveToWaitingRoomById, BattleRoyaleConfig.GetConfig().GetGameData().spectator_delay_seconds * 1000, false, killed.GetIdentity().GetId());
            }
        }
        // A suicide gets past the immunity of the waiting room: the player waits again with a new lobby character
        else if (killed && killed.br_waiting && IsWaitingRoomEnabled() && killed.GetIdentity())
        {
            GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(this.MoveToWaitingRoomById, BattleRoyaleConfig.GetConfig().GetGameData().spectator_delay_seconds * 1000, false, killed.GetIdentity().GetId());
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

    //! The anchor body of a spectator, null when the identity has none
    PlayerBase GetSpectatorAnchor(PlayerIdentity identity)
    {
        if (!m_Spectators)
            return null;

        return m_Spectators.GetAnchor(identity);
    }

    ref array<PlayerBase> temp_disconnecting;
    override void OnPlayerTick(PlayerBase player, float timeslice)
    {
        // Spectator anchor bodies are not match players
        if (m_Spectators && m_Spectators.IsAnchor(player))
            return;

        // Waiting room players are outside the match: kept in the lobby, never logged out as strays
        if (player.br_waiting)
        {
            TickWaitingPlayer(player, timeslice);
            return;
        }

        if(!temp_disconnecting)
        {
            temp_disconnecting = new array<PlayerBase>();
        }

        if(player.br_in_current_state)
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

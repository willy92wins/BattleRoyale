#ifndef SERVER
class BattleRoyaleRPC
{
	void BattleRoyaleRPC()
	{
		BattleRoyaleUtils.Trace("BattleRoyaleClient::Init");

		GetRPCManager().AddRPC( RPC_DAYZBR_NAMESPACE, "SetPlayerCount", this );
		GetRPCManager().AddRPC( RPC_DAYZBR_NAMESPACE, "SetFade", this );
		GetRPCManager().AddRPC( RPC_DAYZBR_NAMESPACE, "SetInput", this );
		GetRPCManager().AddRPC( RPC_DAYZBR_NAMESPACE, "AddPlayerKill", this );
		GetRPCManager().AddRPC( RPC_DAYZBR_NAMESPACE, "StartMatch", this );
		GetRPCManager().AddRPC( RPC_DAYZBR_NAMESPACE, "SetCountdownSeconds", this );
		GetRPCManager().AddRPC( RPC_DAYZBR_NAMESPACE, "UpdateCurrentPlayArea", this );
		GetRPCManager().AddRPC( RPC_DAYZBR_NAMESPACE, "UpdateFuturePlayArea", this );
		GetRPCManager().AddRPC( RPC_DAYZBR_NAMESPACE, "SetTopPosition", this );
		GetRPCManager().AddRPC( RPC_DAYZBR_NAMESPACE, "ShowWinScreen", this );

		GetRPCManager().AddRPC( RPC_DAYZBR_NAMESPACE, "NotificationMessage", this );

		GetRPCManager().AddRPC( RPC_DAYZBR_NAMESPACE, "SetMatchPhase", this );
		GetRPCManager().AddRPC( RPC_DAYZBR_NAMESPACE, "KillFeedEvent", this );
		GetRPCManager().AddRPC( RPC_DAYZBR_NAMESPACE, "MatchInProgress", this );
		GetRPCManager().AddRPC( RPC_DAYZBR_NAMESPACE, "SpectateTarget", this );

		BattleRoyaleUtils.Trace("BattleRoyaleClient::Init - Done");
	}

	void ~BattleRoyaleRPC()
	{

	}

	// Spectator state of a dead player, read by BattleRoyaleSpectatorCamera; the network id is 0/0 while there is no target
	static bool s_Spectating = false;
	static int s_SpectateTargetLow = 0;
	static int s_SpectateTargetHigh = 0;
	static vector s_SpectateTargetHint = "0 0 0";
	static string s_SpectateTargetName = "";

	// Overlay that tells a spectator apart from a living player in first person
	static ref Widget s_SpectatorOverlay;

	static void ResetSpectating()
	{
		s_Spectating = false;
		s_SpectateTargetLow = 0;
		s_SpectateTargetHigh = 0;

		if ( s_SpectatorOverlay )
		{
			s_SpectatorOverlay.Unlink();
			s_SpectatorOverlay = null;
		}
	}

	static void UpdateSpectatorOverlay(string name, int alive)
	{
		if ( !s_SpectatorOverlay )
			s_SpectatorOverlay = GetGame().GetWorkspace().CreateWidgets( "Vigrid-BattleRoyale/Scripts/Client/layouts/br_spectator.layout" );
		if ( !s_SpectatorOverlay )
			return;

		TextWidget title = TextWidget.Cast( s_SpectatorOverlay.FindAnyWidget( "SpectatorTitle" ) );
		if ( title )
			title.SetText( Widget.TranslateString( "#STR_BR_SPECTATE_TITLE" ) + " - " + string.Format( Widget.TranslateString( "#STR_BR_SPECTATE_WATCHING" ), name, alive ) );

		TextWidget help = TextWidget.Cast( s_SpectatorOverlay.FindAnyWidget( "SpectatorHelp" ) );
		if ( help )
		{
			string viewKey = InputUtils.GetButtonNameFromInput( "UAPersonView", EInputDeviceType.MOUSE_AND_KEYBOARD );
			help.SetText( string.Format( Widget.TranslateString( "#STR_BR_SPECTATE_HELP" ), viewKey ) );
		}

		s_SpectatorOverlay.Show( true );
	}

	// Pending main menu message for a late join, shown once by MainMenu after AbortMission
	static string s_PendingMainMenuMessage = "";
	static bool s_MatchInProgressAbortScheduled = false;

	static string GetPendingMainMenuMessage()
	{
		return s_PendingMainMenuMessage;
	}

	static void ClearPendingMainMenuMessage()
	{
		s_PendingMainMenuMessage = "";
		s_MatchInProgressAbortScheduled = false;

		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).Remove(LeaveToMainMenu);
	}

	private static ref BattleRoyaleRPC m_Instance;
	static BattleRoyaleRPC GetInstance()
	{
		if ( m_Instance == NULL )
		{
			m_Instance = new BattleRoyaleRPC();
		}

		return m_Instance;
	}

	void Reset()
	{
		nb_players = 0;
		nb_groups = 0;
		fade_state = false;
		input_state = false;
		player_kills = 0;
		match_started = false;
		countdown_seconds = 0;
		current_play_area_center = "0 0 0";
		current_play_area_radius = 0.0;
		current_play_area_received = false;
		future_play_area_center = "0 0 0";
		future_play_area_radius = 0.0;
		top_position = 0;
		winner_screen = false;
		match_phase = -1;
		match_phase_end_time = -1;
		kill_feed_serial = 0;
	}

	// Set the number of players and groups

	int nb_players = 0;
	int nb_groups = 0;

	void SetPlayerCount(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
	{
		Param2<int, int> data;
		if( !ctx.Read( data ) )
		{
			Error("FAILED TO READ SETPLAYERCOUNT RPC");
			return;
		}
		if ( type == CallType.Client )
		{
			if ( BattleRoyaleUtils.CheckLogLevel( BattleRoyaleUtils.TRACE ) )
				BattleRoyaleUtils.Trace(string.Format("SetPlayerCount: %1 %2", data.param1, data.param2));
			nb_players = data.param1;
			nb_groups = data.param2;

			// The overlay shows the players alive, which change without a new spectate target
			if ( s_Spectating && s_SpectatorOverlay )
				UpdateSpectatorOverlay( s_SpectateTargetName, nb_players );
		}
	}

	// Set the fade state

	bool fade_state = false;

	void SetFade(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
	{
		Param1<bool> data;
		if( !ctx.Read( data ) )
		{
			Error("FAILED TO READ SETFADE RPC");
			return;
		}
		if ( type == CallType.Client )
		{
			if ( BattleRoyaleUtils.CheckLogLevel( BattleRoyaleUtils.TRACE ) )
				BattleRoyaleUtils.Trace(string.Format("SetFade: %1", data.param1));
			if ( data.param1 )
			{
				fade_state = true;
			}
			else
			{
				fade_state = false;
			}
		}
	}

	// Set the input state

	bool input_state = false;

	void SetInput(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
	{
		Param1<bool> data;
		if( !ctx.Read( data ) )
		{
			Error("FAILED TO READ SETINPUT RPC");
			return;
		}
		if ( type == CallType.Client )
		{
			if ( BattleRoyaleUtils.CheckLogLevel( BattleRoyaleUtils.TRACE ) )
				BattleRoyaleUtils.Trace(string.Format("SetInput: %1", data.param1));
			input_state = data.param1;
		}
	}

	// Add a player kill

	int player_kills = 0;

	void AddPlayerKill(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
	{
		Param1<int> data;
		if( !ctx.Read( data ) )
		{
			Error("FAILED TO READ ADDPLAYERKILL RPC");
			return;
		}
		if ( type == CallType.Client )
		{
			BattleRoyaleUtils.Trace("AddPlayerKill");
			player_kills += 1;
		}
	}

	// Start the match

	bool match_started = false;

	void StartMatch(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
	{
		if ( type == CallType.Client )
		{
			BattleRoyaleUtils.Trace("StartMatch");
			match_started = true;
		}
	}

	// Set the countdown seconds

	int countdown_seconds = 0;

	void SetCountdownSeconds(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
	{
		Param1<int> data;
		if( !ctx.Read( data ) )
		{
			Error("FAILED TO READ SETCOUNTDOWNSECONDS RPC");
			return;
		}
		if ( type == CallType.Client )
		{
			if ( BattleRoyaleUtils.CheckLogLevel( BattleRoyaleUtils.TRACE ) )
				BattleRoyaleUtils.Trace(string.Format("SetCountdownSeconds: %1", data.param1));
			countdown_seconds = data.param1;
		}
	}

	// Update the current play area

	vector current_play_area_center = "0 0 0";
	float current_play_area_radius = 0.0;
	bool current_play_area_received = false;

	void UpdateCurrentPlayArea(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
	{
		Param2<vector, float> data;
		if( !ctx.Read( data ) )
		{
			Error("FAILED TO READ UPDATECURRENTPLAYAREA RPC");
			return;
		}
		if ( type == CallType.Client )
		{
			if ( BattleRoyaleUtils.CheckLogLevel( BattleRoyaleUtils.TRACE ) )
				BattleRoyaleUtils.Trace(string.Format("UpdateCurrentPlayArea: %1 %2", data.param1, data.param2));
			current_play_area_center = data.param1;
			current_play_area_radius = data.param2;
			current_play_area_received = true;
		}
	}

	// Update the future play area

	vector future_play_area_center = "0 0 0";
	float future_play_area_radius = 0.0;
	bool b_ArtillerySound = false;

	void UpdateFuturePlayArea(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
	{
		Param3<vector, float, bool> data;
		if( !ctx.Read( data ) )
		{
			Error("FAILED TO READ UPDATEFUTUREPLAYAREA RPC");
			return;
		}
		if ( type == CallType.Client )
		{
			if ( BattleRoyaleUtils.CheckLogLevel( BattleRoyaleUtils.TRACE ) )
				BattleRoyaleUtils.Trace(string.Format("UpdateFuturePlayArea: %1 %2 %3", data.param1, data.param2, data.param3));
			future_play_area_center = data.param1;
			future_play_area_radius = data.param2;
			b_ArtillerySound = data.param3;
		}
	}

	// Set the top position

	int top_position = 0;

	void SetTopPosition(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
	{
		Param1<int> data;
		if( !ctx.Read( data ) )
		{
			Error("FAILED TO READ SETTOPPOSITION RPC");
			return;
		}
		if ( type == CallType.Client )
		{
			if ( BattleRoyaleUtils.CheckLogLevel( BattleRoyaleUtils.TRACE ) )
				BattleRoyaleUtils.Trace(string.Format("SetTopPosition: %1", data.param1));
			top_position = data.param1;
		}
	}

	// Show the win screen

	bool winner_screen = false;

	void ShowWinScreen(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
	{
		if ( type == CallType.Client )
		{
			BattleRoyaleUtils.Trace("ShowWinScreen");
			winner_screen = true;
		}
	}

	void NotificationMessage(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
	{
		Param7<string, float, string, string, string, string, string> data;
		if( !ctx.Read( data ) )
		{
			Error("FAILED TO READ NOTIFICATIONMESSAGE RPC");
			return;
		}
		if ( type == CallType.Client )
		{
			if ( BattleRoyaleUtils.CheckLogLevel( BattleRoyaleUtils.TRACE ) )
				BattleRoyaleUtils.Trace(string.Format("NotificationMessage: %1 %2 %3 %4 %5 %6 %7", data.param1, data.param2, data.param3, data.param4, data.param5, data.param6, data.param7));
			StringLocaliser message = new StringLocaliser(data.param1, data.param3, data.param4, data.param5, data.param6, data.param7);
			string translated_message = message.Format();

			// Special case
			// Finish the translation client side to get the correct key
			if (translated_message.Contains("READY_KEY"))
			{
				translated_message.Replace("READY_KEY", InputUtils.GetButtonNameFromInput("UADayZBRReadyUp", EInputDeviceType.MOUSE_AND_KEYBOARD));
			}
			if (translated_message.Contains("UNSTUCK_KEY"))
			{
				translated_message.Replace("UNSTUCK_KEY", InputUtils.GetButtonNameFromInput("UADayZBRUnstuck", EInputDeviceType.MOUSE_AND_KEYBOARD));
			}

			// Created locally; the text is already translated
			StringLocaliser title = new StringLocaliser("");
			title.Set(-1, DAYZBR_MSG_TITLE, false);
			StringLocaliser text = new StringLocaliser("");
			text.Set(-1, translated_message, false);
			NotificationSystem.Create(title, text, DAYZBR_MSG_IMAGE, COLOR_BR_INFO, data.param2);
		}
	}

	// Set the match phase (EBRMatchPhase) and the time left in it

	int match_phase = -1;
	int match_phase_end_time = -1; //!< GetGame().GetTime() at which the phase timer reaches zero, -1 if the server sent no timer

	void SetMatchPhase(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
	{
		Param2<int, int> data;
		if( !ctx.Read( data ) )
		{
			Error("FAILED TO READ SETMATCHPHASE RPC");
			return;
		}
		if ( type == CallType.Client )
		{
			if ( BattleRoyaleUtils.CheckLogLevel( BattleRoyaleUtils.TRACE ) )
				BattleRoyaleUtils.Trace(string.Format("SetMatchPhase: %1 %2", data.param1, data.param2));
			match_phase = data.param1;
			if ( data.param2 >= 0 )
			{
				match_phase_end_time = GetGame().GetTime() + ( data.param2 * 1000 );
			}
			else
			{
				match_phase_end_time = -1;
			}
		}
	}

	//! Seconds left in the current match phase, -1 if unknown
	int GetMatchPhaseSecondsLeft()
	{
		if ( match_phase_end_time < 0 )
			return -1;

		float seconds_left = ( match_phase_end_time - GetGame().GetTime() ) / 1000.0;
		if ( seconds_left <= 0 )
			return 0;

		return Math.Ceil( seconds_left );
	}

	// Kill feed, stored in a fixed ring buffer until the HUD reads it

	static const int KILL_FEED_SIZE = 8; //!< at least the number of rows of BattleRoyaleKillFeed

	int kill_feed_serial = 0; //!< number of KillFeedEvent received since Reset
	protected int m_KillFeedType[KILL_FEED_SIZE];      //!< EBRKillFeedType
	protected int m_KillFeedFlags[KILL_FEED_SIZE];     //!< BRKillFeedFlags
	protected string m_KillFeedKiller[KILL_FEED_SIZE]; //!< empty when the death names nobody
	protected string m_KillFeedVictim[KILL_FEED_SIZE];
	protected string m_KillFeedItem[KILL_FEED_SIZE];   //!< name of the weapon, explosive, vehicle or animal in the client's language
	protected int m_KillFeedDistance[KILL_FEED_SIZE];  //!< metres, 0 when it does not apply
	protected int m_KillFeedKillerId[KILL_FEED_SIZE];  //!< PlayerIdentity.GetPlayerId() of the killer, -1 when unknown or nobody
	protected int m_KillFeedVictimId[KILL_FEED_SIZE];  //!< same for the victim
	protected int m_KillFeedTime[KILL_FEED_SIZE];      //!< GetGame().GetTime() at reception

	void KillFeedEvent(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
	{
		// meta (type | flags << 8), killer name, victim name, item class name, distance in metres,
		// player id of the killer, player id of the victim
		Param7<int, string, string, string, int, int, int> data;
		if( !ctx.Read( data ) )
		{
			Error("FAILED TO READ KILLFEEDEVENT RPC");
			return;
		}
		if ( type == CallType.Client )
		{
			if ( BattleRoyaleUtils.CheckLogLevel( BattleRoyaleUtils.TRACE ) )
				BattleRoyaleUtils.Trace(string.Format("KillFeedEvent: %1 %2 %3 %4 %5 %6 %7", data.param1, data.param2, data.param3, data.param4, data.param5, data.param6, data.param7));

			// A type this client does not know (newer server) is drawn as a plain death
			int kill_type = data.param1 & 0xFF;
			if ( kill_type >= EBRKillFeedType.COUNT )
				kill_type = EBRKillFeedType.DIED;

			int slot = kill_feed_serial % KILL_FEED_SIZE;
			m_KillFeedType[slot] = kill_type;
			m_KillFeedFlags[slot] = ( data.param1 >> 8 ) & 0xFF;
			m_KillFeedKiller[slot] = data.param2;
			m_KillFeedVictim[slot] = data.param3;
			m_KillFeedItem[slot] = GetKillFeedItemName( data.param4 );
			m_KillFeedDistance[slot] = data.param5;
			m_KillFeedKillerId[slot] = data.param6;
			m_KillFeedVictimId[slot] = data.param7;
			m_KillFeedTime[slot] = GetGame().GetTime();
			kill_feed_serial++;
		}
	}

	//! Display name of an item, vehicle or creature class in the client's language; the class name itself when the config has none
	protected string GetKillFeedItemName(string class_name)
	{
		if ( class_name == "" )
			return "";

		string name = "";
		if ( GetGame().ConfigGetText( "CfgWeapons " + class_name + " displayName", name ) && name != "" )
			return name;
		if ( GetGame().ConfigGetText( "CfgVehicles " + class_name + " displayName", name ) && name != "" )
			return name;
		if ( GetGame().ConfigGetText( "CfgMagazines " + class_name + " displayName", name ) && name != "" )
			return name;

		return class_name;
	}

	//! Reads the kill feed event number 'serial' (0 based); false if it was never received or was already overwritten
	bool GetKillFeedEvent(int serial, out int kill_type, out int flags, out string killer, out string victim, out string item, out int distance, out int killer_id, out int victim_id, out int time)
	{
		if ( serial < 0 || serial >= kill_feed_serial || serial < kill_feed_serial - KILL_FEED_SIZE )
			return false;

		int slot = serial % KILL_FEED_SIZE;
		kill_type = m_KillFeedType[slot];
		flags = m_KillFeedFlags[slot];
		killer = m_KillFeedKiller[slot];
		victim = m_KillFeedVictim[slot];
		item = m_KillFeedItem[slot];
		distance = m_KillFeedDistance[slot];
		killer_id = m_KillFeedKillerId[slot];
		victim_id = m_KillFeedVictimId[slot];
		time = m_KillFeedTime[slot];
		return true;
	}

	// The server switched this dead player to spectating, or changed the watched player
	void SpectateTarget(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
	{
		Param5<int, int, vector, string, int> data;
		if( !ctx.Read( data ) )
		{
			Error("FAILED TO READ SPECTATETARGET RPC");
			return;
		}
		if ( type != CallType.Client )
			return;

		bool first = !s_Spectating;
		s_Spectating = true;
		s_SpectateTargetLow = data.param1;
		s_SpectateTargetHigh = data.param2;
		s_SpectateTargetHint = data.param3;
		s_SpectateTargetName = data.param4;

		if ( first )
			RestoreAfterDeath();

		UpdateSpectatorOverlay( data.param4, data.param5 );
	}

	//! Undoes what the vanilla death simulation did on this client (DayZPlayerImplement.SimulateDeath):
	//! darkening and dead screen, locked controls with the cursor shown, and muted sound
	static void RestoreAfterDeath()
	{
		Man player = GetGame().GetPlayer();
		if ( player )
			player.StopDeathDarkeningEffect();

		GetGame().GetUIManager().ScreenFadeOut( 1.0 );

		// The dead player's stamina bar and quickbar have nothing to show while watching someone else
		if ( GetGame().GetMission() && GetGame().GetMission().GetHud() )
			GetGame().GetMission().GetHud().Show( false );

		Input input = GetGame().GetInput();
		if ( !input.HasGameFocus( INPUT_DEVICE_MOUSE ) && !GetGame().GetUIManager().GetMenu() )
		{
			input.ChangeGameFocus( -1, INPUT_DEVICE_MOUSE );
			input.ChangeGameFocus( -1, INPUT_DEVICE_KEYBOARD );
			input.ChangeGameFocus( -1, INPUT_DEVICE_GAMEPAD );
			GetGame().GetUIManager().ShowUICursor( false );
		}

		DayZGame game = DayZGame.Cast( GetGame() );
		if ( game )
		{
			game.GetSoundScene().SetSoundVolume( game.m_volume_sound, 1 );
			game.GetSoundScene().SetSpeechExVolume( game.m_volume_speechEX, 1 );
			game.GetSoundScene().SetMusicVolume( game.m_volume_music, 1 );
			game.GetSoundScene().SetVOIPVolume( game.m_volume_VOIP, 1 );
			game.GetSoundScene().SetRadioVolume( game.m_volume_radio, 1 );
		}
	}

	// Match already in progress: leave to the main menu and show the reason there
	void MatchInProgress(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
	{
		Param1<int> data;
		if( !ctx.Read( data ) )
		{
			Error("FAILED TO READ MATCHINPROGRESS RPC");
			return;
		}
		if ( type == CallType.Client )
			HandleMatchInProgress( data.param1 );
	}

	//! Shared by the CF RPC and the engine RPC sent during the connection, whichever arrives first
	static void HandleMatchInProgress(int minutes_left)
	{
		if ( s_MatchInProgressAbortScheduled )
			return;
		s_MatchInProgressAbortScheduled = true;

		if ( minutes_left > 0 )
			s_PendingMainMenuMessage = string.Format( Widget.TranslateString( "#STR_BR_MATCH_IN_PROGRESS" ), minutes_left );
		else
			s_PendingMainMenuMessage = Widget.TranslateString( "#STR_BR_MATCH_IN_PROGRESS_NO_TIME" );

		// The system queue also ticks while the client is still loading; the GUI queue does not
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(LeaveToMainMenu, 1000, false);
	}

	static void LeaveToMainMenu()
	{
		// Cleared by the main menu: the rejected session is already over, never leave a later one
		if ( !s_MatchInProgressAbortScheduled )
			return;

		DayZGame game = DayZGame.Cast( GetGame() );
		if ( !game )
			return;

		Mission mission = game.GetMission();
		if ( game.GetGameState() == DayZGameState.IN_GAME && mission && mission.IsMissionGameplay() )
		{
			mission.AbortMission();
			return;
		}

		// Still connecting or loading: same exit as the vanilla login screen
		game.SetGameState( DayZGameState.MAIN_MENU );
		game.SetLoadState( DayZLoadState.MAIN_MENU_START );
		game.GetCallQueue( CALL_CATEGORY_SYSTEM ).Call( game.DisconnectSessionForce );
	}
}
#endif

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

		BattleRoyaleUtils.Trace("BattleRoyaleClient::Init - Done");
	}

	void ~BattleRoyaleRPC()
	{

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

		if ( m_Instance )
			GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).Remove(m_Instance.AbortToMainMenu);
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

			ExpansionNotification(DAYZBR_MSG_TITLE, translated_message, DAYZBR_MSG_IMAGE, COLOR_EXPANSION_NOTIFICATION_INFO, data.param2).Create();
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

	static const int KILL_FEED_SIZE = 5;

	int kill_feed_serial = 0; //!< number of KillFeedEvent received since Reset
	protected string m_KillFeedKiller[KILL_FEED_SIZE];
	protected string m_KillFeedVictim[KILL_FEED_SIZE];
	protected string m_KillFeedWeapon[KILL_FEED_SIZE];
	protected float m_KillFeedDistance[KILL_FEED_SIZE];
	protected int m_KillFeedTime[KILL_FEED_SIZE];

	void KillFeedEvent(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
	{
		Param4<string, string, string, float> data;
		if( !ctx.Read( data ) )
		{
			Error("FAILED TO READ KILLFEEDEVENT RPC");
			return;
		}
		if ( type == CallType.Client )
		{
			if ( BattleRoyaleUtils.CheckLogLevel( BattleRoyaleUtils.TRACE ) )
				BattleRoyaleUtils.Trace(string.Format("KillFeedEvent: %1 %2 %3 %4", data.param1, data.param2, data.param3, data.param4));
			int slot = kill_feed_serial % KILL_FEED_SIZE;
			m_KillFeedKiller[slot] = data.param1;
			m_KillFeedVictim[slot] = data.param2;
			m_KillFeedWeapon[slot] = data.param3;
			m_KillFeedDistance[slot] = data.param4;
			m_KillFeedTime[slot] = GetGame().GetTime();
			kill_feed_serial++;
		}
	}

	//! Reads the kill feed event number 'serial' (0 based); false if it was never received or was already overwritten
	bool GetKillFeedEvent(int serial, out string killer, out string victim, out string weapon, out float distance, out int time)
	{
		if ( serial < 0 || serial >= kill_feed_serial || serial < kill_feed_serial - KILL_FEED_SIZE )
			return false;

		int slot = serial % KILL_FEED_SIZE;
		killer = m_KillFeedKiller[slot];
		victim = m_KillFeedVictim[slot];
		weapon = m_KillFeedWeapon[slot];
		distance = m_KillFeedDistance[slot];
		time = m_KillFeedTime[slot];
		return true;
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
		{
			if ( s_MatchInProgressAbortScheduled )
				return;
			s_MatchInProgressAbortScheduled = true;
			s_PendingMainMenuMessage = "#STR_BR_MATCH_IN_PROGRESS";
			// The system queue also ticks while the client is still loading; the GUI queue does not
			GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(AbortToMainMenu, 1000, false);
		}
	}

	void AbortToMainMenu()
	{
		// Cleared by the main menu: the rejected session is already over, never abort a later one
		if ( !s_MatchInProgressAbortScheduled )
			return;

		if ( GetGame() && GetGame().GetMission() )
			GetGame().GetMission().AbortMission();
	}
}
#endif

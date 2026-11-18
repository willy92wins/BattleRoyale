#ifdef SERVER
class BattleRoyaleServerData: BattleRoyaleDataBase
{
	int version = 1;  // Config version

	// Enable Vigrid API support
	bool enable_vigrid_api = false;

	// Server password (to pass it to the Vigrid API)
	// Must be the same as the one in the serverDZ.cfg
	string server_password = "changeme";

	// JWT Token for the Vigrid API webhook
    string webhook_jwt_token = "changeme";

    // Force getting a match UUID from the webhook, otherwise restart the server
    bool force_match_uuid = false;

    // If the server should warn the players if no UUID is received
    bool warning_no_uuid = false;

	// Autolock the server when a match starts
	// Gonna make a POST request to the autolock URL at the start of the match
	// Alternative to using the Vigrid API to lock the server
	// Autolock URL MUST BE in the format http(s)://<autolock_url>/autolock/<autolock_ip>/<autolock_port> and the payload must be a JSON object with the rcon_password field
	// Example: { "rcon_password": "mypassword" }
	bool use_autolock = false;

	string autolock_url = "";  // Autolock API URL. Empty disables the request even if use_autolock is true.

	// Optional match stats. Empty disables the webhook. Never includes the server or RCON password.
	string stats_webhook_url = "";
	string autolock_ip = "";  // Server RCon IP (mandatory if use_autolock is true)
	int autolock_port = 2305;  // Server RCon Port (mandatory if use_autolock is true)
	string autolock_rcon_password = "";  // Server RCon Password (mandatory if use_autolock is true)

	// Serial matches: when a match ends the server stays up, removes the bodies and dropped items of the match and opens
	// the next lobby. Connected players (winner, spectators) get a new character in the lobby without reconnecting.
	// Ignored while enable_vigrid_api or use_autolock is on (their match id and lock belong to one server run).
	// Loot is left to the Central Economy as the server is set up.
	bool serial_matches = false;
	// With serial_matches: full restart, as without serial matches, when this many matches have ended in one run (0 = never).
	// The restart loop then wipes the world, which brings the loot back
	int serial_restart_every_matches = 6;

	// Message posted to a Discord webhook when a match ends. Empty URL disables it. Keep the URL private: whoever has it
	// can post to the channel. The mod never sends it to the players nor writes it to the logs.
	// Placeholders: {winner} (the winner's name; the names of a winning team, comma separated), {kills} (the winner's
	// kills in the match, a team's added together), {players} (players at the match start) and {duration} (minutes from
	// the match start to the win). Names and mentions in the message ping nobody; Discord refuses over 2000 characters.
	string match_end_webhook_url = "";
	string match_end_message = "{winner} won the match with {kills} kills ({players} players, {duration} min)";
	// Posted instead when nobody wins: the last players died together or left. Empty: nothing is posted then
	string match_end_message_no_winner = "The match ended without a winner ({players} players, {duration} min)";

	// Leaderboard points: by final position and by kill, kept in leaderboard.json next to this file
	int leaderboard_points_first = 10;     // 1st place
	int leaderboard_points_step = 2;       // each later place gets this much less, never below 0
	int leaderboard_points_per_kill = 1;   // each kill the HUD counts in the match
	string leaderboard_wipe = "monthly";   // "monthly" (1st of the month), "weekly" (Monday) or "yearly" (1 January)
	string leaderboard_webhook_url = "";   // Discord webhook for the top 30 after a match that changed it; empty = off

    override string GetProfilePath()
    {
        return BATTLEROYALE_SETTINGS_FOLDER + "server_settings.json";
    }

	override void Validate()
	{
		if (serial_restart_every_matches < 0)
		{
			LogInvalidSetting("server_settings.json", "serial_restart_every_matches", serial_restart_every_matches.ToString(), "must be 0 or more");
			serial_restart_every_matches = 6;
		}

		if (leaderboard_points_first < 0 || leaderboard_points_first > 1000)
		{
			LogInvalidSetting("server_settings.json", "leaderboard_points_first", leaderboard_points_first.ToString(), "must be between 0 and 1000");
			leaderboard_points_first = 10;
		}

		if (leaderboard_points_step < 0 || leaderboard_points_step > 1000)
		{
			LogInvalidSetting("server_settings.json", "leaderboard_points_step", leaderboard_points_step.ToString(), "must be between 0 and 1000");
			leaderboard_points_step = 2;
		}

		if (leaderboard_points_per_kill < 0 || leaderboard_points_per_kill > 1000)
		{
			LogInvalidSetting("server_settings.json", "leaderboard_points_per_kill", leaderboard_points_per_kill.ToString(), "must be between 0 and 1000");
			leaderboard_points_per_kill = 1;
		}

		// The webhook URL is never a logged value, so it is not validated here
		if (leaderboard_wipe != "monthly" && leaderboard_wipe != "weekly" && leaderboard_wipe != "yearly")
		{
			LogInvalidSetting("server_settings.json", "leaderboard_wipe", leaderboard_wipe, "must be monthly, weekly or yearly");
			leaderboard_wipe = "monthly";
		}
	}

	//! True when a match end keeps the server up; logs once per call why the setting is ignored
	bool UsesSerialMatches()
	{
		if (!serial_matches)
			return false;

		if (enable_vigrid_api || use_autolock)
		{
			BattleRoyaleUtils.Warn("serial_matches is ignored while enable_vigrid_api or use_autolock is on: the server restarts after the match");
			return false;
		}

		return true;
	}

	override void Load()
	{
		string errorMessage;
		// Load from profile folder
		if (FileExist(GetProfilePath()))
		{
			if (!JsonFileLoader<BattleRoyaleServerData>.LoadFile(GetProfilePath(), this, errorMessage))
				ErrorEx(errorMessage);
		}

		// Run the upgrade function here to avoid overrides from mission folder
		Upgrade();

		// No load from mission folder for this one
	}

	override void Save()
	{
		string errorMessage;
		if (!JsonFileLoader<BattleRoyaleServerData>.SaveFile(GetProfilePath(), this, errorMessage))
			ErrorEx(errorMessage);
	}
};
#endif

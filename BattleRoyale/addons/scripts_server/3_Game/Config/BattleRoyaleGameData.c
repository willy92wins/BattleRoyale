#ifdef SERVER
class BattleRoyaleGameData: BattleRoyaleDataBase
{
	int version = 3;  // Config version

	// Allowed admins - Are immune to kick and can go outside the play area
	ref array<string> admins_steamid64 = {
		"123456789123456789" // Dummy SteamID64
	};

    int num_zones = 7;  // number of zones, static_sizes/static_timers/min_players in zone_settings.json need at least this many entries
    int round_duration_minutes = 5;  // round length in minutes, also sets the delay before the first zone: (60 * round_duration_minutes) / 2 seconds

	bool enable_spawn_selection_menu = true;  // show spawn selection menu (0 = no, 1 = yes)
	int spawn_selection_duration = 30;  // spawn selection duration in seconds
	int spawn_selection_extra_time = 2;  // extra time between spawn selection and next state in seconds
	float spawn_selection_radius = 50;  // radius where the player can spawn
	bool show_spawn_heatmap = true;  // show spawn heatmap (0 = no, 1 = yes)

    int time_until_teleport_unlock = 10;  // seconds before unlock after teleporting & preparing, must stay below the first zone delay
    bool enable_unstuck = true;  // the unstuck key (F2) teleports a player stuck at the start of the match (0 = off, 1 = on)
    bool enable_spectator = true;  // dead players watch the players still alive until the match ends (0 = off, 1 = on)
    int spectator_delay_seconds = 5;  // seconds between a death and the start of spectating (0 to 10)
    // Waiting room: during a match, players who join late, players who die with spectating off (or with nobody left to watch)
    // and spectators who press the ready key wait in the lobby for the next match instead of leaving the server (0 = off, 1 = on)
    bool enable_waiting_room = true;

    ref array<int> zone_notification_minutes = { 1, 2 };  // minutes when notification about the zone shrinking will be displayed
    ref array<int> zone_notification_seconds = { 30, 10 };  // seconds when notification about the zone shrinking will be displayed, when under the minute

    int debug_heal_tick_seconds = 5;  // seconds between debug (lobby) heal ticks

    // Zone damage settings
    int zone_damage_tick_seconds = 5;  // seconds between zone damage ticks
    float zone_damage_delta = 0.1;  // damage per tick
    bool enable_zone_damage = true;  // enable zone damage

    bool hide_players_endgame = false;  // Hide the number of players left in the endgame

    bool show_first_zone_at_start = true;  // Show the first zone at the start of the game

    bool artillery_sound = true;  // Play the artillery sound when the zone shrinks

	// Plane deployment. Read by the flight-path generator; no match state reads these yet.
	bool enable_plane_deployment = true;
	float plane_speed_mps = 100;  // metres per second, 40..200
	float plane_min_altitude_m = 800;  // AMSL, 400..2000, must stay below plane_max_altitude_m
	float plane_max_altitude_m = 1100;  // AMSL, 400..2500
	float plane_edge_margin_m = 600;  // inset from the world border, 100..3000
	float plane_path_max_zone_distance = 1.0;  // max distance from the first-zone centre, in multiples of that radius; 0 = no limit (0..3)
	float parachute_auto_open_agl_m = 150;  // 30..500 and below plane_min_altitude_m - 100
	int first_zone_reveal_delay_seconds = 60;  // seconds after the plane finishes its run (0..600)
	int first_zone_extra_seconds = 180;  // added to the first-zone countdown at reveal (0..1800)

	// Airdrop settings: ignored since 1.3.0 (the airdrop was removed); kept so older JSON files load
    bool airdrop_enabled = true;  // Enable airdrops
    int airdrop_ignore_last_zones = 3;  // Number of last zones to ignore for airdrops

	// Clothing the player starts with
    ref array<string> player_starting_clothes = {
        "TrackSuitJacket_Red",
        "TrackSuitPants_Red",
        "JoggingShoes_Red"
    };

	// Items the player starts with
    ref array<string> player_starting_items = {
        "HuntingKnife",
        "BandageDressing",
        "Compass",
        "Battery9V",
        "Battery9V"
    };

    // Items that can be quickly accessed in the inventory hotbar
    // The index corresponds to the item in the player_starting_items array (0 = first item, 1 = second item, etc.)
    ref array<int> player_starting_items_shortcut = {
		0  // HuntingKnife
	};

    override string GetProfilePath()
    {
        return BATTLEROYALE_SETTINGS_FOLDER + "general_settings.json";
    }

    override string GetMissionPath()
    {
        return BATTLEROYALE_SETTINGS_MISSION_FOLDER + "general_settings.json";
    }

	override void Load()
	{
		string errorMessage;
		// Load from profile folder
		if (FileExist(GetProfilePath()))
		{
			if (!JsonFileLoader<BattleRoyaleGameData>.LoadFile(GetProfilePath(), this, errorMessage))
				ErrorEx(errorMessage);
		}

		// Run the upgrade function here to avoid overrides from mission folder
		Upgrade();
	}

	override void LoadMission()
	{
		string errorMessage;
		// Override from mission folder
		if (FileExist(GetMissionPath()))
		{
			if (!JsonFileLoader<BattleRoyaleGameData>.LoadFile(GetMissionPath(), this, errorMessage))
				ErrorEx(errorMessage);
		}
	}

	override void Save()
	{
		string errorMessage;
		if (!JsonFileLoader<BattleRoyaleGameData>.SaveFile(GetProfilePath(), this, errorMessage))
			ErrorEx(errorMessage);
	}

	override void Upgrade()
	{
		// Each step is its own if. A v1 file becomes v2 and then falls through into v3
		// on the same load. else-if would leave that file at version 2.
		if (version == 1)
		{
			// Set default shortcut for the first item (HuntingKnife) if not set
			// A version 1 file has no shortcut list, so the declared default may already be there
			if (!player_starting_items_shortcut)
				player_starting_items_shortcut = new array<int>;

			if (player_starting_items_shortcut.Count() == 0)
				player_starting_items_shortcut.Insert(0);

			version = 2;
			Save();  // Save the upgraded config
		}

		if (version == 2)
		{
			// New keys are absent in v2 files; JsonFileLoader leaves the declared defaults in place
			version = 3;
			Save();
		}
	}

	// num_zones is checked together with the zone tables in BattleRoyaleZoneData.ValidateZoneTables
	override void Validate()
	{
		BattleRoyaleGameData defaults = new BattleRoyaleGameData;

		// The client leaves 15 s after its death unless spectating has started by then
		if (spectator_delay_seconds < 0 || spectator_delay_seconds > 10)
		{
			LogInvalidSetting("general_settings.json", "spectator_delay_seconds", spectator_delay_seconds.ToString(), "must be between 0 and 10");
			spectator_delay_seconds = defaults.spectator_delay_seconds;
		}

		if (time_until_teleport_unlock < 0)
		{
			LogInvalidSetting("general_settings.json", "time_until_teleport_unlock", time_until_teleport_unlock.ToString(), "must not be negative");
			time_until_teleport_unlock = defaults.time_until_teleport_unlock;
		}

		// The first zone starts (60 * round_duration_minutes) / 2 seconds after the start match state, which then stops the unlock timer
		if (round_duration_minutes < 1 || (60 * round_duration_minutes) / 2 <= time_until_teleport_unlock)
		{
			LogInvalidSetting("general_settings.json", "round_duration_minutes / time_until_teleport_unlock", round_duration_minutes.ToString() + " / " + time_until_teleport_unlock.ToString(), "round_duration_minutes >= 1 and (60 * round_duration_minutes) / 2 > time_until_teleport_unlock");
			round_duration_minutes = defaults.round_duration_minutes;
			time_until_teleport_unlock = defaults.time_until_teleport_unlock;
		}

		if (spawn_selection_duration < 1)
		{
			LogInvalidSetting("general_settings.json", "spawn_selection_duration", spawn_selection_duration.ToString(), "must be positive");
			spawn_selection_duration = defaults.spawn_selection_duration;
		}

		if (spawn_selection_extra_time < 0)
		{
			LogInvalidSetting("general_settings.json", "spawn_selection_extra_time", spawn_selection_extra_time.ToString(), "must not be negative");
			spawn_selection_extra_time = defaults.spawn_selection_extra_time;
		}

		if (spawn_selection_radius <= 0)
		{
			LogInvalidSetting("general_settings.json", "spawn_selection_radius", spawn_selection_radius.ToString(), "must be positive");
			spawn_selection_radius = defaults.spawn_selection_radius;
		}

		if (debug_heal_tick_seconds < 1)
		{
			LogInvalidSetting("general_settings.json", "debug_heal_tick_seconds", debug_heal_tick_seconds.ToString(), "must be positive");
			debug_heal_tick_seconds = defaults.debug_heal_tick_seconds;
		}

		if (zone_damage_tick_seconds < 1)
		{
			LogInvalidSetting("general_settings.json", "zone_damage_tick_seconds", zone_damage_tick_seconds.ToString(), "must be positive");
			zone_damage_tick_seconds = defaults.zone_damage_tick_seconds;
		}

		if (plane_speed_mps < 40 || plane_speed_mps > 200)
		{
			LogInvalidSetting("general_settings.json", "plane_speed_mps", plane_speed_mps.ToString(), "must be between 40 and 200");
			plane_speed_mps = defaults.plane_speed_mps;
		}

		if (plane_min_altitude_m < 400 || plane_min_altitude_m > 2000)
		{
			LogInvalidSetting("general_settings.json", "plane_min_altitude_m", plane_min_altitude_m.ToString(), "must be between 400 and 2000");
			plane_min_altitude_m = defaults.plane_min_altitude_m;
		}

		if (plane_max_altitude_m < 400 || plane_max_altitude_m > 2500)
		{
			LogInvalidSetting("general_settings.json", "plane_max_altitude_m", plane_max_altitude_m.ToString(), "must be between 400 and 2500");
			plane_max_altitude_m = defaults.plane_max_altitude_m;
		}

		// One log resets both. Range checks above already ran, so a pair that is merely inverted logs once.
		if (plane_min_altitude_m >= plane_max_altitude_m)
		{
			LogInvalidSetting("general_settings.json", "plane_min_altitude_m / plane_max_altitude_m", plane_min_altitude_m.ToString() + " / " + plane_max_altitude_m.ToString(), "plane_min_altitude_m must be lower than plane_max_altitude_m");
			plane_min_altitude_m = defaults.plane_min_altitude_m;
			plane_max_altitude_m = defaults.plane_max_altitude_m;
		}

		if (plane_edge_margin_m < 100 || plane_edge_margin_m > 3000)
		{
			LogInvalidSetting("general_settings.json", "plane_edge_margin_m", plane_edge_margin_m.ToString(), "must be between 100 and 3000");
			plane_edge_margin_m = defaults.plane_edge_margin_m;
		}

		if (plane_path_max_zone_distance < 0 || plane_path_max_zone_distance > 3)
		{
			LogInvalidSetting("general_settings.json", "plane_path_max_zone_distance", plane_path_max_zone_distance.ToString(), "must be between 0 and 3");
			plane_path_max_zone_distance = defaults.plane_path_max_zone_distance;
		}

		if (parachute_auto_open_agl_m < 30 || parachute_auto_open_agl_m > 500 || parachute_auto_open_agl_m >= plane_min_altitude_m - 100)
		{
			LogInvalidSetting("general_settings.json", "parachute_auto_open_agl_m", parachute_auto_open_agl_m.ToString(), "must be between 30 and 500 and lower than plane_min_altitude_m - 100");
			parachute_auto_open_agl_m = defaults.parachute_auto_open_agl_m;
		}

		if (first_zone_reveal_delay_seconds < 0 || first_zone_reveal_delay_seconds > 600)
		{
			LogInvalidSetting("general_settings.json", "first_zone_reveal_delay_seconds", first_zone_reveal_delay_seconds.ToString(), "must be between 0 and 600");
			first_zone_reveal_delay_seconds = defaults.first_zone_reveal_delay_seconds;
		}

		if (first_zone_extra_seconds < 0 || first_zone_extra_seconds > 1800)
		{
			LogInvalidSetting("general_settings.json", "first_zone_extra_seconds", first_zone_extra_seconds.ToString(), "must be between 0 and 1800");
			first_zone_extra_seconds = defaults.first_zone_extra_seconds;
		}
	}
};
#endif

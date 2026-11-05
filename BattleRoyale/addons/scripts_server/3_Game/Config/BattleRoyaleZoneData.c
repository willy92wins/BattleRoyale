#ifdef SERVER
class BattleRoyaleZoneData: BattleRoyaleDataBase
{
	int version = 1;  // Config version

    bool end_in_villages = true;  // The final zone will end in a village/city/town
    ref array<string> end_avoid_type = {"DeerStand", "FeedShack", "Marine"};  // Types to avoid in the final zone (Only if end_in_villages is true)

    // Cities to avoid in the final zone, by CfgWorlds Names class name (Only if end_in_villages is true)
    // Chernarus list for the default zone tables below; check it again after changing static_sizes or the map
    ref array<string> end_avoid_city = {
        // Lobby (Camp Shkolnik) and islands (Skalisty, Storozh prison)
        "Camp_Shkolnik", "Ruin_Storozh", "Ruin_Voron", "Settlement_Skalisty",
        // Too close to the map edge: the larger zones cannot be generated around them
        "Camp_Metalurg", "Camp_Pobeda", "Hill_BolshoyKotel", "Hill_Simurg",
        "Hill_Sokol", "Hill_SvyatoyRoman", "Hill_Tumannyverkh", "Hill_Volchiypik",
        "LocalOffice_Novo", "LocalOffice_Sinys", "LocalOffice_Svetlo", "Local_Chernyeskaly",
        "Local_Grozovyedoly", "Local_Grozovypereval", "Local_Krutyeskaly", "Local_MB_Kamensk",
        "Local_MB_Tisy", "Local_Polonina", "Local_Rostoki", "Local_Skalka",
        "Local_TriKresta", "Local_Turovskiypereval", "Local_Vidy", "RailroadStation_Dobroye",
        "RailroadStation_Novo", "RailroadStation_Svetlo", "Settlement_BelayaPolyana", "Settlement_Berezhki",
        "Settlement_Dobroye", "Settlement_Galkino", "Settlement_Kamensk", "Settlement_Karmanovka",
        "Settlement_Krasnoye", "Settlement_Nagornoye", "Settlement_Novodmitrovsk", "Settlement_SKVSChBiathlonArena",
        "Settlement_StaryYar", "Settlement_Svetloyarsk", "Settlement_SvyatoyRomanSkiResort", "Settlement_Tisy",
        "Settlement_Turovo", "Settlement_Zabolotye", "Settlement_Zvir", "ViewPoint_1",
        "ViewPoint_11", "ViewPoint_12", "ViewPoint_13", "ViewPoint_15",
        "ViewPoint_16", "ViewPoint_17", "ViewPoint_18", "ViewPoint_19",
        "ViewPoint_2", "ViewPoint_20", "ViewPoint_21", "ViewPoint_22",
        "ViewPoint_23", "ViewPoint_24", "ViewPoint_26", "ViewPoint_27",
        "ViewPoint_28", "ViewPoint_29", "ViewPoint_3", "ViewPoint_30",
        "ViewPoint_31", "ViewPoint_32", "ViewPoint_33", "ViewPoint_34",
        "ViewPoint_35", "ViewPoint_36", "ViewPoint_4", "ViewPoint_5",
        "ViewPoint_6", "ViewPoint_7",
        // On or near the coast: the first zone would cover the sea
        "Hill_Dobry", "Hill_Mayak", "Hill_Olsha", "Hill_Ostrog",
        "Hill_PikKozlova", "Hill_Viselitsa", "LocalOffice_Berezino", "LocalOffice_Cherno",
        "LocalOffice_Elektro", "LocalOffice_Kamys", "LocalOffice_Komar", "LocalOffice_Solnich",
        "Local_AF_Balota", "Local_Drakon", "Local_Grubyeskaly", "Local_MB_Pavlovo",
        "Local_Maryanka", "Local_MysGolova", "Local_MysKrutoy", "Local_Otmel",
        "Local_Tridoliny", "Marine_Rify", "RailroadStation_Balota", "RailroadStation_Berezino",
        "RailroadStation_Cherno", "RailroadStation_Elektro", "RailroadStation_Kamenka", "RailroadStation_Kamyshovo",
        "RailroadStation_Komarovo", "RailroadStation_Solnich", "RailroadStation_Solnich2", "Settlement_Balota",
        "Settlement_Berezino", "Settlement_Chernogorsk", "Settlement_Elektrozavodsk", "Settlement_Kamenka",
        "Settlement_Kamyshovo", "Settlement_Khelm", "Settlement_Komarovo", "Settlement_Nizhneye",
        "Settlement_Olsha", "Settlement_Solnechny", "Settlement_Tulga", "Settlement_Vysotovo",
        "ViewPoint_41", "ViewPoint_45", "ViewPoint_46", "ViewPoint_47",
        "ViewPoint_48", "ViewPoint_49", "ViewPoint_50", "ViewPoint_8"
    };

    // First zone polygon restriction
    bool restrict_first_zone = false;  // Whether to restrict the first zone to a polygon
    ref array<string> first_zone_polygon = {}; // Array of vectors defining polygon for first zone

	// Dynamic zones
    bool use_dynamic_zones = true;  // Use dynamic zones based on player count
    int min_zone_num = 4;  // Minimum number of zones to have

    int shrink_type = 3;  // 1 = Exp, 2 = Lin, 3 = Static, 4 = Const
    // TODO: I think Exp, Lin and Const are not used anymore
    // TODO: That is because the logic changed at one point and now the zone is always static

    // Exponential (NOT USED ANYMORE)
    float shrink_base = 2.718281828459; // ~ e
    float shrink_exponent = 3.0;

	// Static
	// Index 0 is the final zone and index num_zones - 1 the first one; each table needs num_zones entries (general_settings.json)
	// Chernarus, 100-120 players: first zone of 2500 m radius, final zone of 50 m, a full match lasts about 32 minutes
    ref array<float> static_sizes = { 50, 150, 350, 700, 1200, 1800, 2500 };  // zone radius in meters
    ref array<int> static_timers = { 135, 180, 225, 270, 300, 330, 240 };  // zone duration in seconds, the zone locks at 80%; the first zone also waits (60 * round_duration_minutes) / 2 seconds before it starts
    ref array<int> min_players = { 10, 10, 10, 10, 10, 30, 60 };  // dynamic zones: the match starts in the first zone (last index first) whose value is below the player count, at the latest in zone num_zones - min_zone_num

    // Constant (NOT USED ANYMORE)
    float constant_scale = 0.65;

    override string GetProfilePath()
    {
        return BATTLEROYALE_SETTINGS_FOLDER + "zone_settings.json";
    }

    override string GetMissionPath()
    {
        return BATTLEROYALE_SETTINGS_MISSION_FOLDER + "zone_settings.json";
    }

	override void Load()
	{
		string errorMessage;
		// Load from profile folder
		if (FileExist(GetProfilePath()))
		{
			if (!JsonFileLoader<BattleRoyaleZoneData>.LoadFile(GetProfilePath(), this, errorMessage))
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
			if (!JsonFileLoader<BattleRoyaleZoneData>.LoadFile(GetMissionPath(), this, errorMessage))
				ErrorEx(errorMessage);
		}
	}

	override void Save()
	{
		string errorMessage;
		if (!JsonFileLoader<BattleRoyaleZoneData>.SaveFile(GetProfilePath(), this, errorMessage))
			ErrorEx(errorMessage);
	}

	// BattleRoyaleZone and the round states index the tables directly with num_zones from general_settings.json
	// Called by BattleRoyaleConfig once every file is loaded; unusable tables are replaced as one block with the defaults
	void ValidateZoneTables(BattleRoyaleGameData game_data)
	{
		string error = GetZoneTablesError(game_data.num_zones);
		if (error == "")
		{
			if (static_sizes.Count() > game_data.num_zones || static_timers.Count() > game_data.num_zones || min_players.Count() > game_data.num_zones)
				Print("[DayZ-BattleRoyale] zone_settings.json: tables are longer than num_zones = " + game_data.num_zones + ", the extra entries are not used");

			return;
		}

		Error("[DayZ-BattleRoyale] zone_settings.json / general_settings.json: " + error + ", using the default num_zones, static_sizes, static_timers, min_players and min_zone_num");

		BattleRoyaleGameData default_game = new BattleRoyaleGameData;
		BattleRoyaleZoneData default_zone = new BattleRoyaleZoneData;

		game_data.num_zones = default_game.num_zones;

		static_sizes = new array<float>;
		static_sizes.Copy(default_zone.static_sizes);

		static_timers = new array<int>;
		static_timers.Copy(default_zone.static_timers);

		min_players = new array<int>;
		min_players.Copy(default_zone.min_players);

		min_zone_num = default_zone.min_zone_num;
	}

	// Returns the first problem found, or an empty string when the tables can be used
	protected string GetZoneTablesError(int zone_count)
	{
		// With a single zone the last round has no previous zone, and the round states dereference it
		if (zone_count < 2)
			return "num_zones = " + zone_count + " (minimum 2)";

		int sizes_count = 0;
		if (static_sizes)
			sizes_count = static_sizes.Count();

		if (sizes_count < zone_count)
			return "static_sizes has " + sizes_count + " entries for num_zones = " + zone_count;

		int timers_count = 0;
		if (static_timers)
			timers_count = static_timers.Count();

		if (timers_count < zone_count)
			return "static_timers has " + timers_count + " entries for num_zones = " + zone_count;

		int players_count = 0;
		if (min_players)
			players_count = min_players.Count();

		if (players_count < zone_count)
			return "min_players has " + players_count + " entries for num_zones = " + zone_count;

		int i;
		for (i = 0; i < zone_count; i++)
		{
			if (static_sizes[i] <= 0)
				return "static_sizes[" + i + "] = " + static_sizes[i] + " (must be positive)";

			// Each zone is generated around the next smaller one
			if (i > 0 && static_sizes[i] <= static_sizes[i - 1])
				return "static_sizes[" + i + "] = " + static_sizes[i] + " (must be larger than static_sizes[" + (i - 1) + "] = " + static_sizes[i - 1] + ")";

			if (static_timers[i] <= 0)
				return "static_timers[" + i + "] = " + static_timers[i] + " (must be positive)";

			if (min_players[i] < 0)
				return "min_players[" + i + "] = " + min_players[i] + " (must not be negative)";

			// Larger zones are meant for more players
			if (i > 0 && min_players[i] < min_players[i - 1])
				return "min_players[" + i + "] = " + min_players[i] + " (must not be lower than min_players[" + (i - 1) + "] = " + min_players[i - 1] + ")";
		}

		if (min_zone_num < 1 || min_zone_num >= zone_count)
			return "min_zone_num = " + min_zone_num + " (must be between 1 and num_zones - 1 = " + (zone_count - 1) + ")";

		// The first zone has to fit inside the world
		if (GetGame() && GetGame().GetWorld())
		{
			int world_size = GetGame().GetWorld().GetWorldSize();
			if (world_size > 0 && static_sizes[zone_count - 1] * 2 >= world_size)
				return "static_sizes[" + (zone_count - 1) + "] = " + static_sizes[zone_count - 1] + " (the first zone does not fit a world of " + world_size + " m)";
		}

		return "";
	}
};
#endif

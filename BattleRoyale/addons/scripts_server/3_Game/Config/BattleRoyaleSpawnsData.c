#ifdef SERVER
class BattleRoyaleSpawnsData: BattleRoyaleDataBase
{
	int version = 1;  // Config version

    vector spawn_point = "14829.2 0 14572.3";  // Lobby spawn point, change it related to the map (Chernarus: Camp Shkolnik, north-east corner, outside every zone)
    float radius = 50;  // Lobby spawn point radius

    bool spawn_in_villages = true;  // Spawn players and groups in villages
    bool spawn_in_first_zone = true;  // Spawn players and groups in the first zone, otherwise they will spawn all around the map
    int extra_spawn_radius = 250;  // Extra radius around the first zone for spawning, allow player to spawn outside the first zone

	// Cities to avoid for spawning, by CfgWorlds Names class name
	// Most of the time, you want to avoid cities because they have no loot
	// Chernarus: Camp_Shkolnik is the lobby, Ruin_Voron and Settlement_Skalisty are on Skalisty island, Novoselki/Dubovo/Vysotovo are City entries next to Chernogorsk
    ref array<string> avoid_city_spawn = {
        "Camp_Shkolnik",
        "Hill_Zelenayagora",
        "Settlement_Kumyrna",
        "Ruin_Voron",
        "Settlement_Skalisty",
        "Settlement_Novoselki",
        "Settlement_Dubovo",
        "Settlement_Vysotovo"
    };

    override string GetProfilePath()
    {
        return BATTLEROYALE_SETTINGS_FOLDER + "spawns_settings.json";
    }

    override string GetMissionPath()
    {
        return BATTLEROYALE_SETTINGS_MISSION_FOLDER + "spawns_settings.json";
    }

	override void Load()
	{
		string errorMessage;
		// Load from profile folder
		if (FileExist(GetProfilePath()))
		{
			if (!JsonFileLoader<BattleRoyaleSpawnsData>.LoadFile(GetProfilePath(), this, errorMessage))
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
			if (!JsonFileLoader<BattleRoyaleSpawnsData>.LoadFile(GetMissionPath(), this, errorMessage))
				ErrorEx(errorMessage);
		}
	}

	override void Save()
	{
		string errorMessage;
		if (!JsonFileLoader<BattleRoyaleSpawnsData>.SaveFile(GetProfilePath(), this, errorMessage))
			ErrorEx(errorMessage);
	}

	override void Validate()
	{
		BattleRoyaleSpawnsData defaults = new BattleRoyaleSpawnsData;

		// The lobby state teleports back every player farther than this radius
		if (radius <= 0)
		{
			LogInvalidSetting("spawns_settings.json", "radius", radius.ToString(), "must be positive");
			radius = defaults.radius;
		}

		if (extra_spawn_radius < 0)
		{
			LogInvalidSetting("spawns_settings.json", "extra_spawn_radius", extra_spawn_radius.ToString(), "must not be negative");
			extra_spawn_radius = defaults.extra_spawn_radius;
		}
	}
};
#endif

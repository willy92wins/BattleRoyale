#ifdef SERVER
class BattleRoyalePOIsData: BattleRoyaleDataBase
{
	int version = 1;  // Config version

	// Allow to override the position of the POIs, by CfgWorlds Names class name
	// Example entry: { "poi_name": "Settlement_Chernogorsk", "new_position": [ 6780, 2320 ] } ([ x, z ] in meters)
	ref array<ref BattleRoyaleOverridePOIPosition> override_poi_positions = {};

	[NonSerialized()]
	ref map<string, vector> m_OverrideSpawnPositions;

    override string GetProfilePath()
    {
        return BATTLEROYALE_SETTINGS_FOLDER + "pois_settings.json";
    }

    override string GetMissionPath()
    {
        return BATTLEROYALE_SETTINGS_MISSION_FOLDER + "pois_settings.json";
    }

	override void Load()
	{
		string errorMessage;
		// Load from profile folder
		if (FileExist(GetProfilePath()))
		{
			if (!JsonFileLoader<BattleRoyalePOIsData>.LoadFile(GetProfilePath(), this, errorMessage))
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
			if (!JsonFileLoader<BattleRoyalePOIsData>.LoadFile(GetMissionPath(), this, errorMessage))
				ErrorEx(errorMessage);
		}
	}

	override void Save()
	{
		string errorMessage;
		if (!JsonFileLoader<BattleRoyalePOIsData>.SaveFile(GetProfilePath(), this, errorMessage))
			ErrorEx(errorMessage);
	}

	override void Validate()
	{
		if (!override_poi_positions)
		{
			override_poi_positions = new array<ref BattleRoyaleOverridePOIPosition>;
			return;
		}

		string entry_name;
		BattleRoyaleOverridePOIPosition entry;
		for (int i = override_poi_positions.Count() - 1; i >= 0; i--)
		{
			entry = override_poi_positions[i];
			if (entry && entry.poi_name != "" && entry.new_position && entry.new_position.Count() >= 2)
				continue;

			entry_name = "(empty)";
			if (entry)
				entry_name = "\"" + entry.poi_name + "\"";

			Error("[DayZ-BattleRoyale] pois_settings.json: override_poi_positions entry " + entry_name + " needs a poi_name and a [ x, z ] new_position, entry ignored");
			override_poi_positions.RemoveOrdered(i);
		}
	}

	vector GetOverrodePosition(string poi_name)
	{
		// Built once from the values BattleRoyaleConfig already loaded (profile, then mission override)
		if( !m_OverrideSpawnPositions )
		{
			BattleRoyaleUtils.Trace("Load m_OverrideSpawnPositions!");
			m_OverrideSpawnPositions = new map<string, vector>();

			if( override_poi_positions )
			{
				foreach(BattleRoyaleOverridePOIPosition position: override_poi_positions)
				{
					if( !position || !position.new_position || position.new_position.Count() < 2 )
						continue;

					BattleRoyaleUtils.Trace(position.poi_name + " " + position.new_position);
					vector temp_pos;
					temp_pos[0] = position.new_position[0];
					temp_pos[2] = position.new_position[1];
					temp_pos[1] = GetGame().SurfaceY( temp_pos[0], temp_pos[2] );
					m_OverrideSpawnPositions.Set( position.poi_name, temp_pos );
				}
			}
		}

		if( m_OverrideSpawnPositions.Contains( poi_name ) )
		{
			BattleRoyaleUtils.Trace("Asked for " + poi_name + " position!");
			return m_OverrideSpawnPositions.Get( poi_name );
		}

		return "0 0 0";
	}
};

class BattleRoyaleOverridePOIPosition
{
    string poi_name;
    ref array<int> new_position;  // [ x, z ]

    void BattleRoyaleOverridePOIPosition(string in_poi_name, array<int> in_new_position)
	{
		this.poi_name = in_poi_name;
		this.new_position = in_new_position;
	}
};
#endif

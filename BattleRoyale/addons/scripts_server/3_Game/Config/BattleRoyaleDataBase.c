#ifdef SERVER
class BattleRoyaleDataBase
{
	// Path to the config file from profile folder
	string GetProfilePath()
	{
		return "";
	}

	// Path to the config file from mission folder
	string GetMissionPath()
	{
		return "";
	}

	void Load() {}  // Load the config from the file
	void LoadMission() {}  // Load the config from the mission file
	void Save() {}  // Save the config to the file
	void Upgrade() {}  // Upgrade the config to the latest version if needed
	void Validate() {}  // Replace unusable values with defaults, called once profile and mission files are loaded

	// Logs a rejected setting; the caller restores the default value
	protected void LogInvalidSetting(string file_name, string field, string value, string rule)
	{
		Error("[DayZ-BattleRoyale] " + file_name + ": invalid " + field + " = " + value + " (" + rule + "), using the default value");
	}
};
#endif

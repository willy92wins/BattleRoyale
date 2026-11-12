class LoadingScreenBackground
{
	string MapName;
	ref array<string> Path;

	//! Index returned last time, so the next call picks another one
	[NonSerialized()]
	int m_LastIndex = -1;

	void LoadingScreenBackground(string map_name, array<string> texture_path)
	{
		MapName = map_name;
		Path = texture_path;
	}

	//! Random entry of Path, other than the previous one when there are two or more; empty when Path is empty
	string GetRandomPath()
	{
		if (!Path || Path.Count() == 0)
			return "";

		int count = Path.Count();
		int index = Math.RandomInt(0, count);
		if (count > 1 && index == m_LastIndex)
			index = (index + 1 + Math.RandomInt(0, count - 1)) % count;

		m_LastIndex = index;
		return Path[index];
	}
};

class LoadingScreenBackgrounds
{
	static ref array<ref LoadingScreenBackground> s_Backgrounds = new array<ref LoadingScreenBackground>;

	static array<ref LoadingScreenBackground> Get()
	{
		if (!s_Backgrounds.Count())
		{
    		string errorMessage;
			if (!JsonFileLoader<array<ref LoadingScreenBackground>>.LoadFile( DAYZBR_LOADING_SCREENS_PATH, s_Backgrounds, errorMessage))
				ErrorEx(errorMessage);
		}

		return s_Backgrounds;
	}
}

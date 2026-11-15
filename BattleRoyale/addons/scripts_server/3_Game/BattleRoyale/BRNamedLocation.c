#ifdef SERVER
// Named location of the world config: CfgWorlds <world> Names <class>
class BRNamedLocation: Managed
{
	static const string CAPITAL = "Capital";
	static const string CITY = "City";

	// Raw "name" entry: localization keys are not translated
	string Name;
	// Raw "type" entry, e.g. Capital, City, Village
	string Type;
	// "position" entry as x, surface height, z
	vector Position;

	protected string m_ClassName;

	void BRNamedLocation(string path)
	{
		// The class name is the last token of the config path
		array<string> path_tokens = new array<string>();
		path.Split(" ", path_tokens);
		if (path_tokens.Count() > 0)
			m_ClassName = path_tokens[path_tokens.Count() - 1];

		if (!GetGame())
			return;

		// Each entry is only read when it exists, otherwise its field keeps the default value
		string name_entry = path + " name";
		if (GetGame().ConfigIsExisting(name_entry))
		{
			string raw_name;
			if (GetGame().ConfigGetTextRaw(name_entry, raw_name))
				Name = raw_name;
		}

		string position_entry = path + " position";
		if (GetGame().ConfigIsExisting(position_entry))
		{
			// position[] = {x, z}: the first two components hold x and z
			vector config_position = GetGame().ConfigGetVector(position_entry);
			float position_x = config_position[0];
			float position_z = config_position[1];
			Position = Vector(position_x, GetGame().SurfaceY(position_x, position_z), position_z);
		}

		string type_entry = path + " type";
		if (GetGame().ConfigIsExisting(type_entry))
		{
			string raw_type;
			if (GetGame().ConfigGetTextRaw(type_entry, raw_type))
				Type = raw_type;
		}
	}

	// Config class name of the location, e.g. Settlement_Novoselki
	string GetName()
	{
		return m_ClassName;
	}
}
#endif

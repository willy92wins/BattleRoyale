#ifdef SERVER
//! One player's standing in the current period; id is the plain id (Steam id)
class BRLeaderboardEntry
{
	string id;
	string name;
	int points;
	int kills;
	int wins;
	int matches;
}

//! leaderboard.json as a whole: the standings of one period and the top 30 last posted to Discord
class BRLeaderboardFile
{
	int version = 1;
	string period;
	string last_posted;
	ref array<ref BRLeaderboardEntry> players = new array<ref BRLeaderboardEntry>;
}

//! Records that Discord took the leaderboard post, so the same top 30 is not posted twice
class BRLeaderboardWebhookCallback: BRDiscordCallback
{
	protected string m_Text;

	void BRLeaderboardWebhookCallback(string text)
	{
		m_Label = "Leaderboard webhook";
		m_Text = text;
	}

	override void OnSent()
	{
		BRLeaderboard.Get().MarkPosted(m_Text);
	}
}

//! The leaderboard of server_settings.json: points by final position and by kill, wiped by period
//! (leaderboard_wipe). Single-threaded server use only: matches apply their results when they end
class BRLeaderboard
{
	static const int TOP_COUNT = 30;

	protected static ref BRLeaderboard s_Instance;

	protected ref BRLeaderboardFile m_File = new BRLeaderboardFile;
	protected ref map<string, BRLeaderboardEntry> m_ById = new map<string, BRLeaderboardEntry>;

	//! True when an unreadable file could not be preserved: scores stay in memory and nothing is written this run
	protected bool m_SaveDisabled;
	//! True only when leaderboard.json is a known good copy: it loaded whole at startup or the last
	//! save read it back whole. Only then may it be copied to .bak before being replaced
	protected bool m_MainKnownValid;

	//! Cache of the first TOP_COUNT standings, rebuilt whenever the standings change: client requests read it as it is
	protected ref array<string> m_TopNames = new array<string>;
	protected ref array<int> m_TopPoints = new array<int>;
	protected ref array<int> m_TopKills = new array<int>;
	protected ref array<int> m_TopWins = new array<int>;

	static BRLeaderboard Get()
	{
		if (!s_Instance)
			s_Instance = new BRLeaderboard;

		return s_Instance;
	}

	static string FilePath()
	{
		return BATTLEROYALE_SETTINGS_FOLDER + "leaderboard.json";
	}

	static string NewFilePath()
	{
		return FilePath() + ".new";
	}

	static string BakFilePath()
	{
		return FilePath() + ".bak";
	}

	//! First free name for a preserved copy: baseWithoutExtension + ".json", then "_2", "_3" and so on
	protected string FirstFreeJsonPath(string baseWithoutExtension)
	{
		string freePath = baseWithoutExtension + ".json";
		if (!FileExist(freePath))
			return freePath;

		int suffix = 2;
		string candidatePath = baseWithoutExtension + "_" + suffix.ToString() + ".json";
		while (FileExist(candidatePath))
		{
			suffix = suffix + 1;
			candidatePath = baseWithoutExtension + "_" + suffix.ToString() + ".json";
		}

		return candidatePath;
	}

	//! Copies sourcePath to the first free name of baseWithoutExtension; true only when the copy is there
	protected bool PreserveFile(string sourcePath, string baseWithoutExtension, out string preservedPath)
	{
		preservedPath = FirstFreeJsonPath(baseWithoutExtension);
		if (!CopyFile(sourcePath, preservedPath))
			return false;

		return FileExist(preservedPath);
	}

	//! Turns saving off for this run, logged once: matches still score in memory, no file is written
	protected void DisableSaving()
	{
		if (m_SaveDisabled)
			return;

		m_SaveDisabled = true;
		Error("[Leaderboard] leaderboard.json could not be read and could not be preserved; saving is disabled for this run, scores are kept in memory only and the file is left for the administrator");
	}

	protected int PlayerCount(BRLeaderboardFile file)
	{
		if (!file || !file.players)
			return 0;

		return file.players.Count();
	}

	//! Loads path into a fresh object; true only when the file reads whole into it
	protected bool TryLoad(string path, out BRLeaderboardFile loaded)
	{
		loaded = new BRLeaderboardFile;
		string tryLoadError;
		return JsonFileLoader<BRLeaderboardFile>.LoadFile(path, loaded, tryLoadError);
	}

	//! A file is valid when it reads whole and, when expected is given, its period and player count match it.
	//! No write or copy is trusted without this read-back
	protected bool Valid(string path, BRLeaderboardFile expected)
	{
		BRLeaderboardFile probe = new BRLeaderboardFile;
		string probeError;
		if (!JsonFileLoader<BRLeaderboardFile>.LoadFile(path, probe, probeError))
			return false;

		if (expected)
		{
			if (probe.period != expected.period)
				return false;
			if (PlayerCount(probe) != PlayerCount(expected))
				return false;
		}

		return true;
	}

	//! Reads leaderboard.json, then leaderboard.json.new, then leaderboard.json.bak, and uses the first
	//! one that reads whole. A board recovered from .new or .bak is copied onto leaderboard.json (and read
	//! back) after the unreadable main file, if any, has been copied aside, so the recovered board lives
	//! in two places before any later save. Only when none reads is every unreadable file copied aside
	//! and the board starts empty; when a copy aside fails, saving stays off for this run
	void Load()
	{
		m_File = new BRLeaderboardFile;
		m_ById.Clear();
		m_MainKnownValid = false;

		array<string> loadCandidates = new array<string>;
		loadCandidates.Insert(FilePath());
		loadCandidates.Insert(NewFilePath());
		loadCandidates.Insert(BakFilePath());

		string loadedPath = "";
		BRLeaderboardFile loadedFile;
		for (int c = 0; c < loadCandidates.Count(); c++)
		{
			string candidatePath = loadCandidates[c];
			if (!FileExist(candidatePath))
				continue;

			BRLeaderboardFile candidateFile;
			if (TryLoad(candidatePath, candidateFile))
			{
				loadedFile = candidateFile;
				loadedPath = candidatePath;
				break;
			}
		}

		if (loadedPath != "")
		{
			m_File = loadedFile;
			if (!m_File.players)
				m_File.players = new array<ref BRLeaderboardEntry>;

			if (loadedPath == FilePath())
			{
				m_MainKnownValid = true;
			}
			else
			{
				BattleRoyaleUtils.Info("[Leaderboard] Loaded " + loadedPath + " instead of leaderboard.json");
				bool recoveryOk = true;
				if (FileExist(FilePath()))
				{
					string brokenPath;
					if (PreserveFile(FilePath(), BATTLEROYALE_SETTINGS_FOLDER + "leaderboard.broken", brokenPath))
						Error("[Leaderboard] leaderboard.json could not be read, copied to " + brokenPath);
					else
						recoveryOk = false;
				}

				if (recoveryOk)
				{
					if (!CopyFile(loadedPath, FilePath()) || !Valid(FilePath(), m_File))
						recoveryOk = false;
					else
						m_MainKnownValid = true;
				}

				if (!recoveryOk)
					DisableSaving();
			}
		}
		else
		{
			bool anyExisting = false;
			bool allPreserved = true;
			for (int p = 0; p < loadCandidates.Count(); p++)
			{
				string preserveCandidate = loadCandidates[p];
				if (!FileExist(preserveCandidate))
					continue;

				anyExisting = true;
				string preservedCandidatePath;
				if (!PreserveFile(preserveCandidate, BATTLEROYALE_SETTINGS_FOLDER + "leaderboard.broken", preservedCandidatePath))
					allPreserved = false;
			}

			if (anyExisting && allPreserved)
				Error("[Leaderboard] No leaderboard file could be read; every unreadable file was copied to a leaderboard.broken file and the leaderboard starts empty");
			if (!allPreserved)
				DisableSaving();
		}

		if (!m_File.players)
			m_File.players = new array<ref BRLeaderboardEntry>;

		for (int i = 0; i < m_File.players.Count(); i++)
		{
			BRLeaderboardEntry entry = m_File.players[i];
			if (entry && entry.id != "")
				m_ById.Set(entry.id, entry);
		}

		SortPlayers();
		RebuildTop();
	}

	//! Recoverable save, not an atomic one: every write and copy is read back before the next step.
	//! The new standings go to leaderboard.json.new and are read back against memory; only a main file
	//! already known to be good is copied to leaderboard.json.bak (and read back), so a bad main file
	//! never overwrites the backup; then .new is copied over the main file and read back against memory.
	//! A shutdown mid-save leaves a file Load() can still read: the old main file, the verified .new or the .bak
	protected void Save()
	{
		if (m_SaveDisabled)
			return;

		string newPath = NewFilePath();
		string errorMessage;
		if (!JsonFileLoader<BRLeaderboardFile>.SaveFile(newPath, m_File, errorMessage))
		{
			ErrorEx(errorMessage);
			return;
		}

		if (!Valid(newPath, m_File))
		{
			Error("[Leaderboard] leaderboard.json.new could not be read back; leaderboard.json was left unchanged");
			return;
		}

		if (m_MainKnownValid)
		{
			if (!CopyFile(FilePath(), BakFilePath()) || !Valid(BakFilePath(), null))
			{
				Error("[Leaderboard] leaderboard.json could not be copied to leaderboard.json.bak or the copy could not be read back; leaderboard.json was left unchanged and the new standings are in leaderboard.json.new");
				return;
			}
		}

		if (!CopyFile(newPath, FilePath()) || !Valid(FilePath(), m_File))
		{
			Error("[Leaderboard] leaderboard.json.new could not be copied over leaderboard.json or the result could not be read back; the verified leaderboard.json.new will be used on load");
			m_MainKnownValid = false;
			return;
		}

		m_MainKnownValid = true;
	}

	//! Adds a standings amount without overflowing: the result stops at int.MAX. Amounts are never negative (the point
	//! settings are 0 to 1000), and a comparison against int.MIN is unreliable in Enforce, so a negative one adds nothing
	protected static int SaturateAdd(int current, int added)
	{
		if (added <= 0)
			return current;

		if (current > int.MAX - added)
			return int.MAX;

		return current + added;
	}

	//! Key of the period that contains today (server local date): monthly-YYYY-MM, yearly-YYYY or
	//! weekly-YYYY-MM-DD with the Monday of this week
	static string PeriodKey(string wipe)
	{
		int year;
		int month;
		int day;
		GetYearMonthDay(year, month, day);

		if (wipe == "yearly")
			return "yearly-" + year.ToString();

		if (wipe == "weekly")
		{
			int days = DaysFromCivil(year, month, day);
			int weekday = (days + 4) % 7;         // 0 = Sunday; 1970-01-01 (day 0) was a Thursday
			int sinceMonday = (weekday + 6) % 7;  // days since this week's Monday
			int mondayYear;
			int mondayMonth;
			int mondayDay;
			CivilFromDays(days - sinceMonday, mondayYear, mondayMonth, mondayDay);
			return "weekly-" + mondayYear.ToString() + "-" + mondayMonth.ToStringLen(2) + "-" + mondayDay.ToStringLen(2);
		}

		return "monthly-" + year.ToString() + "-" + month.ToStringLen(2);
	}

	//! The date part of a period key, as the panel and Discord show it
	static string PeriodLabel(string key)
	{
		int dash = key.IndexOf("-");
		if (dash == -1)
			return key;

		return key.Substring(dash + 1, key.Length() - dash - 1);
	}

	//! Days from 1970-01-01 to a civil date (Howard Hinnant's days_from_civil, integer arithmetic)
	protected static int DaysFromCivil(int year, int month, int day)
	{
		int y = year;
		if (month <= 2)
			y = y - 1;

		int era = (y - 399) / 400;
		if (y >= 0)
			era = y / 400;

		int yoe = y - era * 400;
		int mp = month - 3;
		if (month <= 2)
			mp = month + 9;

		int doy = (153 * mp + 2) / 5 + day - 1;
		int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
		return era * 146097 + doe - 719468;
	}

	//! Civil date of a day count from 1970-01-01 (Howard Hinnant's civil_from_days, integer arithmetic)
	protected static void CivilFromDays(int days, out int year, out int month, out int day)
	{
		int z = days + 719468;
		int era = (z - 146096) / 146097;
		if (z >= 0)
			era = z / 146097;

		int doe = z - era * 146097;
		int yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
		int y = yoe + era * 400;
		int doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
		int mp = (5 * doy + 2) / 153;
		day = doy - (153 * mp + 2) / 5 + 1;
		month = mp - 9;
		if (mp < 10)
			month = mp + 3;

		year = y;
		if (month <= 2)
			year = y + 1;
	}

	//! Starts today's period when the last one ended. True when the board already is in today's period
	//! or rotated into it; false when the rotation failed and nothing was emptied, so the caller must not
	//! score a new match into the old period. To rotate, the standings in memory are written straight to
	//! the period archive (first free name) and read back against memory before anything is emptied, so
	//! a failed save since the load cannot make the archive an older copy. Changing leaderboard_wipe
	//! changes the key, so it archives and starts over too
	bool EnsurePeriod(string wipe)
	{
		string key = PeriodKey(wipe);
		if (m_File.period == key)
			return true;

		if (m_File.period != "" && m_File.players && m_File.players.Count() > 0)
		{
			if (m_SaveDisabled)
				return false;

			string archivePath = FirstFreeJsonPath(BATTLEROYALE_SETTINGS_FOLDER + "leaderboard_" + m_File.period);
			string archiveError;
			if (!JsonFileLoader<BRLeaderboardFile>.SaveFile(archivePath, m_File, archiveError))
			{
				ErrorEx(archiveError);
				Error("[Leaderboard] Period " + m_File.period + " could not be archived; keeping its standings and retrying at the next period check");
				return false;
			}

			if (!Valid(archivePath, m_File))
			{
				Error("[Leaderboard] Period " + m_File.period + " archive " + archivePath + " could not be read back; keeping its standings and retrying at the next period check");
				return false;
			}

			BattleRoyaleUtils.Info("[Leaderboard] Period " + m_File.period + " archived as " + archivePath);
		}

		m_File.period = key;
		m_File.last_posted = "";
		if (m_File.players)
			m_File.players.Clear();
		else
			m_File.players = new array<ref BRLeaderboardEntry>;

		m_ById.Clear();
		RebuildTop();
		Save();
		return true;
	}

	//! Adds one finished match: every participant gains its points and kills, the winner of position 1 a win.
	//! The four arrays run in parallel, one entry per participant
	void ApplyMatch(array<string> ids, array<string> names, array<int> points, array<int> kills, array<bool> won)
	{
		if (!ids || !names || !points || !kills || !won)
			return;

		int count = ids.Count();
		if (names.Count() < count)
			count = names.Count();
		if (points.Count() < count)
			count = points.Count();
		if (kills.Count() < count)
			count = kills.Count();
		if (won.Count() < count)
			count = won.Count();

		for (int i = 0; i < count; i++)
		{
			string playerId = ids[i];
			if (playerId == "")
				continue;

			BRLeaderboardEntry entry;
			if (!m_ById.Find(playerId, entry) || !entry)
			{
				entry = new BRLeaderboardEntry;
				entry.id = playerId;
				m_File.players.Insert(entry);
				m_ById.Set(playerId, entry);
			}

			entry.name = names[i];
			entry.points = SaturateAdd(entry.points, points[i]);
			entry.kills = SaturateAdd(entry.kills, kills[i]);
			if (won[i])
				entry.wins = SaturateAdd(entry.wins, 1);
			entry.matches = SaturateAdd(entry.matches, 1);
		}

		SortPlayers();
		RebuildTop();
		Save();
	}

	//! Points first, then wins, then kills, then name: the order the panel and Discord show
	protected void SortPlayers()
	{
		if (!m_File.players)
			return;

		for (int i = 1; i < m_File.players.Count(); i++)
		{
			BRLeaderboardEntry moving = m_File.players[i];
			if (!moving)
				continue;

			int j = i - 1;
			while (j >= 0 && ComesBefore(moving, m_File.players[j]))
			{
				m_File.players[j + 1] = m_File.players[j];
				j = j - 1;
			}
			m_File.players[j + 1] = moving;
		}
	}

	protected bool ComesBefore(BRLeaderboardEntry a, BRLeaderboardEntry b)
	{
		if (!b)
			return true;
		if (a.points != b.points)
			return a.points > b.points;
		if (a.wins != b.wins)
			return a.wins > b.wins;
		if (a.kills != b.kills)
			return a.kills > b.kills;

		return a.name < b.name;
	}

	protected void RebuildTop()
	{
		m_TopNames.Clear();
		m_TopPoints.Clear();
		m_TopKills.Clear();
		m_TopWins.Clear();

		if (!m_File.players)
			return;

		int count = m_File.players.Count();
		if (count > TOP_COUNT)
			count = TOP_COUNT;

		for (int i = 0; i < count; i++)
		{
			BRLeaderboardEntry entry = m_File.players[i];
			if (!entry)
				continue;

			m_TopNames.Insert(entry.name);
			m_TopPoints.Insert(entry.points);
			m_TopKills.Insert(entry.kills);
			m_TopWins.Insert(entry.wins);
		}
	}

	array<string> GetTopNames()
	{
		return m_TopNames;
	}

	array<int> GetTopPoints()
	{
		return m_TopPoints;
	}

	array<int> GetTopKills()
	{
		return m_TopKills;
	}

	array<int> GetTopWins()
	{
		return m_TopWins;
	}

	string GetPeriod()
	{
		return m_File.period;
	}

	string GetLastPosted()
	{
		return m_File.last_posted;
	}

	//! The top standings as a Discord message for the period of wipe, or "" while nobody has played.
	//! Names are cut to 16 characters and lose their backticks, so 30 lines stay under Discord's 2000
	string BuildTopText(int count, string wipe)
	{
		if (!m_File.players || m_File.players.Count() == 0)
			return "";

		string text = "**Leaderboard** · " + wipe + " · " + PeriodLabel(m_File.period);
		text = text + "\n```\n#  Player            Pts  Kills  Wins\n";

		int shown = m_File.players.Count();
		if (shown > count)
			shown = count;

		for (int i = 0; i < shown; i++)
		{
			BRLeaderboardEntry entry = m_File.players[i];
			if (!entry)
				continue;

			string playerName = entry.name;
			if (playerName.LengthUtf8() > 16)
				playerName = playerName.SubstringUtf8(0, 16);
			playerName.Replace("`", "'");

			int rankNumber = i + 1;
			string rank = rankNumber.ToString();
			string pts = entry.points.ToString();
			string entryKills = entry.kills.ToString();
			string entryWins = entry.wins.ToString();
			text = text + PadLeft(rank, 2) + " " + PadRight(playerName, 16) + "  " + PadLeft(pts, 3) + "  " + PadLeft(entryKills, 5) + "  " + PadLeft(entryWins, 4) + "\n";
		}

		text = text + "```";
		return text;
	}

	//! Remembers the posted top, so a match that leaves it unchanged posts nothing
	void MarkPosted(string text)
	{
		m_File.last_posted = text;
		Save();
	}

	protected string PadRight(string value, int width)
	{
		string padded = value;
		while (padded.LengthUtf8() < width)
			padded = padded + " ";

		return padded;
	}

	protected string PadLeft(string value, int width)
	{
		string padded = value;
		while (padded.Length() < width)
			padded = " " + padded;

		return padded;
	}
}
#endif

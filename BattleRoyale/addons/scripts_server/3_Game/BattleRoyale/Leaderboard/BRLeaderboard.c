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

	//! One coherent reading of the server local clock: the date is read, then the time, then
	//! the date again, and the whole capture repeats when the day changed in between, so the
	//! returned date and time always belong to the same day, even around midnight
	static void CaptureNow(out int year, out int month, out int day, out int hour, out int minute, out int second)
	{
		int checkYear;
		int checkMonth;
		int checkDay;
		GetYearMonthDay(year, month, day);
		GetHourMinuteSecond(hour, minute, second);
		GetYearMonthDay(checkYear, checkMonth, checkDay);
		while (checkYear != year || checkMonth != month || checkDay != day)
		{
			year = checkYear;
			month = checkMonth;
			day = checkDay;
			GetHourMinuteSecond(hour, minute, second);
			GetYearMonthDay(checkYear, checkMonth, checkDay);
		}
	}

	//! Key of the period that contains today (server local date): monthly-YYYY-MM, yearly-YYYY or
	//! weekly-YYYY-MM-DD with the Monday of this week
	static string PeriodKey(string wipe)
	{
		int year;
		int month;
		int day;
		int hour;
		int minute;
		int second;
		CaptureNow(year, month, day, hour, minute, second);
		return PeriodKeyForDate(wipe, year, month, day);
	}

	//! Key of the period that contains the given civil date, for wipe: monthly-YYYY-MM,
	//! yearly-YYYY or weekly-YYYY-MM-DD with the Monday of that week. Any other wipe value
	//! is monthly, as in PeriodKey
	static string PeriodKeyForDate(string wipe, int year, int month, int day)
	{
		if (wipe == "yearly")
			return "yearly-" + year.ToString();

		if (wipe == "weekly")
		{
			int startYear;
			int startMonth;
			int startDay;
			int endYear;
			int endMonth;
			int endDay;
			PeriodBoundsForDate(wipe, year, month, day, startYear, startMonth, startDay, endYear, endMonth, endDay);
			return "weekly-" + startYear.ToString() + "-" + startMonth.ToStringLen(2) + "-" + startDay.ToStringLen(2);
		}

		return "monthly-" + year.ToString() + "-" + month.ToStringLen(2);
	}

	//! First and last day of the period named by key, as YYYY-MM-DD, and the seconds from the
	//! captured now (year to second, from CaptureNow) until 00:00 on the day after that last
	//! day; never negative, so a period already past counts 0. The key is the one stored in
	//! the leaderboard, which stays on the previous period when rotation failed, so the
	//! season and the counter always name the same period as the standings themselves.
	//! The count assumes 86400 seconds per day: DayZ has no time zone database, so it is
	//! approximate on daylight saving change days (aproximado: ±1 h los días de cambio de hora)
	static void SeasonForPeriod(string key, int year, int month, int day, int hour, int minute, int second, out string start, out string end, out int secondsLeft)
	{
		int startYear;
		int startMonth;
		int startDay;
		int endYear;
		int endMonth;
		int endDay;
		start = "";
		end = "";
		secondsLeft = 0;
		if (!PeriodBoundsForKey(key, startYear, startMonth, startDay, endYear, endMonth, endDay))
			return;

		start = FormatDate(startYear, startMonth, startDay);
		end = FormatDate(endYear, endMonth, endDay);
		int daysLeft = DaysFromCivil(endYear, endMonth, endDay) - DaysFromCivil(year, month, day);
		secondsLeft = (daysLeft + 1) * 86400 - (hour * 3600 + minute * 60 + second);
		if (secondsLeft < 0)
			secondsLeft = 0;
	}

	//! Civil dates of the first and last day of the period named by a period key, from the
	//! key itself (monthly-YYYY-MM, yearly-YYYY or weekly-YYYY-MM-DD with its Monday), with
	//! the same day arithmetic as PeriodKeyForDate (DaysFromCivil/CivilFromDays). False when
	//! the key names no period of those shapes
	protected static bool PeriodBoundsForKey(string key, out int startYear, out int startMonth, out int startDay, out int endYear, out int endMonth, out int endDay)
	{
		startYear = 0;
		startMonth = 0;
		startDay = 0;
		endYear = 0;
		endMonth = 0;
		endDay = 0;
		if (key == "")
			return false;

		int keyYear;
		int keyMonth;
		int keyDay;
		int checkYear;
		int checkMonth;
		int checkDay;
		string yearText;
		string monthText;
		string dayText;
		if (key.Length() == 11 && key.Substring(0, 7) == "yearly-")
		{
			yearText = key.Substring(7, 4);
			keyYear = yearText.ToInt();
			if (keyYear <= 0)
				return false;

			startYear = keyYear;
			startMonth = 1;
			startDay = 1;
			endYear = keyYear;
			endMonth = 12;
			endDay = 31;
			return true;
		}

		if (key.Length() == 15 && key.Substring(0, 8) == "monthly-")
		{
			yearText = key.Substring(8, 4);
			keyYear = yearText.ToInt();
			monthText = key.Substring(13, 2);
			keyMonth = monthText.ToInt();
			if (keyYear <= 0 || keyMonth < 1 || keyMonth > 12)
				return false;

			startYear = keyYear;
			startMonth = keyMonth;
			startDay = 1;
			int nextYear = keyYear;
			int nextMonth = keyMonth + 1;
			if (nextMonth > 12)
			{
				nextMonth = 1;
				nextYear = keyYear + 1;
			}
			CivilFromDays(DaysFromCivil(nextYear, nextMonth, 1) - 1, endYear, endMonth, endDay);
			return true;
		}

		if (key.Length() == 17 && key.Substring(0, 7) == "weekly-")
		{
			yearText = key.Substring(7, 4);
			keyYear = yearText.ToInt();
			monthText = key.Substring(12, 2);
			keyMonth = monthText.ToInt();
			dayText = key.Substring(15, 2);
			keyDay = dayText.ToInt();
			if (keyYear <= 0 || keyMonth < 1 || keyMonth > 12 || keyDay < 1 || keyDay > 31)
				return false;

			CivilFromDays(DaysFromCivil(keyYear, keyMonth, keyDay), checkYear, checkMonth, checkDay);
			if (checkYear != keyYear || checkMonth != keyMonth || checkDay != keyDay)
				return false;

			startYear = keyYear;
			startMonth = keyMonth;
			startDay = keyDay;
			CivilFromDays(DaysFromCivil(keyYear, keyMonth, keyDay) + 6, endYear, endMonth, endDay);
			return true;
		}

		return false;
	}

	//! Civil dates of the first and last day of the period that contains the given civil
	//! date for wipe, with the same day arithmetic as PeriodKeyForDate
	//! (DaysFromCivil/CivilFromDays)
	protected static void PeriodBoundsForDate(string wipe, int year, int month, int day, out int startYear, out int startMonth, out int startDay, out int endYear, out int endMonth, out int endDay)
	{
		if (wipe == "yearly")
		{
			startYear = year;
			startMonth = 1;
			startDay = 1;
			endYear = year;
			endMonth = 12;
			endDay = 31;
			return;
		}

		if (wipe == "weekly")
		{
			int days = DaysFromCivil(year, month, day);
			int weekday = (days + 4) % 7;         // 0 = Sunday; 1970-01-01 (day 0) was a Thursday
			int sinceMonday = (weekday + 6) % 7;  // days since this week's Monday
			CivilFromDays(days - sinceMonday, startYear, startMonth, startDay);
			CivilFromDays(days - sinceMonday + 6, endYear, endMonth, endDay);
			return;
		}

		startYear = year;
		startMonth = month;
		startDay = 1;
		int nextYear = year;
		int nextMonth = month + 1;
		if (nextMonth > 12)
		{
			nextMonth = 1;
			nextYear = year + 1;
		}
		CivilFromDays(DaysFromCivil(nextYear, nextMonth, 1) - 1, endYear, endMonth, endDay);
	}

	//! A civil date as YYYY-MM-DD
	protected static string FormatDate(int year, int month, int day)
	{
		return year.ToString() + "-" + month.ToStringLen(2) + "-" + day.ToStringLen(2);
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
		int year;
		int month;
		int day;
		int hour;
		int minute;
		int second;
		CaptureNow(year, month, day, hour, minute, second);
		return EnsurePeriodForDate(wipe, year, month, day);
	}

	//! EnsurePeriod for the period that contains the given civil date (from one CaptureNow
	//! reading, when the caller already holds one and the answer must share it)
	bool EnsurePeriodForDate(string wipe, int year, int month, int day)
	{
		string key = PeriodKeyForDate(wipe, year, month, day);
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

	//! Standing of plainId in the full standings: rank from 1 with its points, kills and wins, or
	//! rank 0 and zeroes when it has no entry this period. m_File.players is already sorted, so
	//! this sorts nothing: one linear scan, O(players)
	void FindStanding(string plainId, out int rank, out int points, out int kills, out int wins)
	{
		rank = 0;
		points = 0;
		kills = 0;
		wins = 0;
		if (!m_File.players)
			return;

		for (int i = 0; i < m_File.players.Count(); i++)
		{
			BRLeaderboardEntry entry = m_File.players[i];
			if (entry && entry.id == plainId)
			{
				rank = i + 1;
				points = entry.points;
				kills = entry.kills;
				wins = entry.wins;
				return;
			}
		}
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

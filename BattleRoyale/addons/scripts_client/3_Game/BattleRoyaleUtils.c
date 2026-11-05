class BattleRoyaleUtils: Managed
{
    // Move to enum ?
    static const int NONE = 0;
    static const int WARN = 1;
    static const int INFO = 2;
    static const int DEBUG = 3;
    static const int TRACE = 4;

    //! Threshold that rejects every level: -br-none or a negative BRLogLevel
    static const int LOG_DISABLED = -1;

    protected static bool m_LogLevelResolved;
    protected static int m_LogLevel;

    static void LogMessage(string message)
    {
        BattleRoyaleUtils.Trace("[DayZ-BattleRoyale] " + message);
    }

    static void LogServerMessage(int level, string message)
    {
        if(CheckLogLevel(level))
        {
        	int hour;
        	int minute;
        	int second;

        	GetHourMinuteSecond(hour, minute, second);

			string msg;
        	if (level == NONE) {
        		msg = hour.ToStringLen(2) + ":" + minute.ToStringLen(2) + ":" + second.ToStringLen(2) + " [DayZ-BattleRoyale] " + message;
        		Error2("", string.ToString(msg));
        	} else {
        		msg = hour.ToStringLen(2) + ":" + minute.ToStringLen(2) + ":" + second.ToStringLen(2) + " [DayZ-BattleRoyale][" + level + "] " + message;
	        	PrintFormat("%1:%2:%3 [DayZ-BattleRoyale][%4] %5", hour.ToStringLen(2), minute.ToStringLen(2), second.ToStringLen(2), level, message);
            }
            // Logs go to the script log only: they are not echoed to the in-game chat
        }
    }

    static void Error(string message)
    {
        LogServerMessage(NONE, message);
    }

    static void Warn(string message)
    {
        LogServerMessage(WARN, message);
    }

    static void Info(string message)
    {
        LogServerMessage(INFO, message);
    }

    static void Debug(string message)
    {
        LogServerMessage(DEBUG, message);
    }

    static void Trace(string message)
    {
        LogServerMessage(TRACE, message);
    }

    static void Trace(vector v)
	{
		LogServerMessage(TRACE, v.ToString());
	}

	static void Trace(float f)
	{
		LogServerMessage(TRACE, f.ToString());
	}

	static bool CheckLogLevel(int level)
	{
		// Launch parameters and server config do not change at runtime: resolve the threshold once
		if (!m_LogLevelResolved)
		{
			m_LogLevel = ResolveLogLevel();
			m_LogLevelResolved = true;
		}

		if (m_LogLevel == LOG_DISABLED)
			return false;

		return (m_LogLevel >= level);
	}

	protected static int ResolveLogLevel()
	{
		// Check command line params
		if (IsCLIParam("br-trace")) return TRACE;
		if (IsCLIParam("br-debug")) return DEBUG;
		if (IsCLIParam("br-info")) return INFO;
		if (IsCLIParam("br-warn")) return WARN;
		if (IsCLIParam("br-none")) return LOG_DISABLED;  // Disable all logging

#ifdef SERVER
		// Check server config
		int config_br_log_level = GetGame().ServerConfigGetInt("BRLogLevel");
		if (config_br_log_level > 0)
		{
			return config_br_log_level;
		}
		// config_br_log_level == 0 can mean that the config value is not set
		// In that case we use the default log level defined by BATTLEROYALE_LOG_LEVEL
		// Check negative value to disable all logging
		if (config_br_log_level < 0)
		{
			return LOG_DISABLED;  // Disable all logging
		}
#endif

		// Default log level
		return BATTLEROYALE_LOG_LEVEL;
	}

    static string GetDateTime(string dateTimeFormat = "%1.%2.%3 %4:%5:%6")
    {
        int year = 0;
        int month = 0;
        int day = 0;
        int hour = 0;
        int minute = 0;
        int second = 0;

        GetYearMonthDay(year, month, day);
        GetHourMinuteSecond(hour, minute, second);
        return string.Format(dateTimeFormat, day.ToStringLen(2), month.ToStringLen(2), year.ToStringLen(4), hour.ToStringLen(2), minute.ToStringLen(2), second.ToStringLen(2));
    }

    static string GetTime(bool withSeconds = false)
    {
        string dateTimeFormat = "";
        int hour = 0;
        int minute = 0;
        int second = 0;
        GetHourMinuteSecond(hour, minute, second);

        if (withSeconds)
        {
            dateTimeFormat = "%1:%2:%3";
            return string.Format(dateTimeFormat, hour.ToStringLen(2), minute.ToStringLen(2), second.ToStringLen(2));
        }
        else
        {
            dateTimeFormat = "%1:%2";
            return string.Format(dateTimeFormat, hour.ToStringLen(2), minute.ToStringLen(2));
        }
    }
}

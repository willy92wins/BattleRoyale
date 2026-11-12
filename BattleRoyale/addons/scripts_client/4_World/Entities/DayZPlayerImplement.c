#ifndef SERVER
#ifndef NO_GUI
modded class DayZPlayerImplement
{
	int position_top = -1;

	//! Bumped when a mission starts or finishes; a pending leave from an older session does nothing
	protected static int s_BR_SessionSerial;
	//! Player whose LeaveServer call is queued, if any
	protected static DayZPlayerImplement s_BR_PendingLeave;
	protected int m_BR_LeaveSession = -1;

	//! Called by the mission when it starts and when it finishes
	static void BR_OnSessionChanged()
	{
		s_BR_SessionSerial++;
		BR_CancelLeaveServer();
	}

	static void BR_CancelLeaveServer()
	{
		if (s_BR_PendingLeave)
			GetGame().GetCallQueue(CALL_CATEGORY_GUI).Remove(s_BR_PendingLeave.LeaveServer);

		s_BR_PendingLeave = null;
	}

	override void ShowDeadScreen(bool show, float duration)
	{
        array<string> funny_strings = {
			"#STR_BR_DEATH_MESSAGE_1",
			"#STR_BR_DEATH_MESSAGE_2",
			"#STR_BR_DEATH_MESSAGE_3",
			"#STR_BR_DEATH_MESSAGE_4",
			"#STR_BR_DEATH_MESSAGE_5",
			"#STR_BR_DEATH_MESSAGE_6",
			"#STR_BR_DEATH_MESSAGE_7",
			"#STR_BR_DEATH_MESSAGE_8",
			"#STR_BR_DEATH_MESSAGE_9",
			"#STR_BR_DEATH_MESSAGE_10",
			"#STR_BR_DEATH_MESSAGE_11",
        };

		if (show && IsPlayerSelected())
		{
			string message = "";
			if (!GetGame().GetMission().IsPlayerRespawning())
			{
				message = Widget.TranslateString(funny_strings.GetRandomElement());

				// The position is only known once the server sent it
				if (position_top > 0)
				{
					message = string.Format(Widget.TranslateString("#STR_BR_DEATH_POSITION"), position_top) + "\r\n" + message;
				}
			}

			GetGame().GetUIManager().ScreenFadeIn(duration, message, FadeColors.DARK_RED, FadeColors.WHITE);
		}
		else
		{
			super.ShowDeadScreen(show, duration);
		}

		// 15s timer before leaving server, bound to the current session
		if (show)
		{
			BR_CancelLeaveServer();
			m_BR_LeaveSession = s_BR_SessionSerial;
			s_BR_PendingLeave = this;
			GetGame().GetCallQueue(CALL_CATEGORY_GUI).CallLater(LeaveServer, 15000, false);
		}
	}

	void LeaveServer()
	{
		if (s_BR_PendingLeave == this)
			s_BR_PendingLeave = null;

		// The session that scheduled the leave has ended; a new one must not be dropped
		if (m_BR_LeaveSession != s_BR_SessionSerial || !GetGame().GetMission())
			return;

		// Spectating dead players stay until the match ends
		if (BattleRoyaleRPC.s_Spectating)
			return;

		GetGame().GetMission().AbortMission();
	}
}
#endif
#endif

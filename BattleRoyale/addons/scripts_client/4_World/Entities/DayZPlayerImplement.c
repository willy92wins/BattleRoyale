#ifndef SERVER
#ifndef NO_GUI
modded class DayZPlayerImplement
{
	int position_top = -1;

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

		// 15s timer before leaving server
		if (show)
		{
			GetGame().GetCallQueue(CALL_CATEGORY_GUI).CallLater(LeaveServer, 15000, false);
		}
	}

	void LeaveServer()
	{
		GetGame().GetMission().AbortMission();
	}
}
#endif
#endif

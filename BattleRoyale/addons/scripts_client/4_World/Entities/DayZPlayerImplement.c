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

		// Spectating dead players stay until the match ends; with serial matches the server brings them back to the lobby
		if (BattleRoyaleRPC.s_Spectating || BattleRoyaleRPC.s_StayConnected)
			return;

		GetGame().GetMission().AbortMission();
	}

	//! Sights last reported to the server (EBRSpectateSights), the optic field of view reported with them, and whether the
	//! player was underground then (only with an optic: its night vision changes underground)
	protected int m_BR_ReportedSights = EBRSpectateSights.NONE;
	protected float m_BR_ReportedSightsFov = 0;
	protected bool m_BR_ReportedUnderground = false;
	//! Seconds before the next report may leave
	protected float m_BR_SightsReportCooldown = 0;

	override void CommandHandler(float pDt, int pCurrentCommandID, bool pCurrentCommandFinished)
	{
		super.CommandHandler(pDt, pCurrentCommandID, pCurrentCommandFinished);

		BR_ReportSights(pDt);
	}

	//! The local player reports the sights it looks through to the server whenever they change, so that the players
	//! spectating it look through them too. One report every 0.1 s at most: a magnification stepped quickly sends the
	//! last step, because the change is still there on the next tick
	protected void BR_ReportSights(float pDt)
	{
		if (GetGame().GetPlayer() != this)
			return;

		if (m_BR_SightsReportCooldown > 0)
			m_BR_SightsReportCooldown -= pDt;

		int sights = EBRSpectateSights.NONE;
		float fov = 0;
		bool underground = false;
		if (IsInOptics())
		{
			sights = EBRSpectateSights.OPTICS;
			EntityAI inHands = GetEntityInHands();
			ItemOptics optic = ItemOptics.Cast(inHands);
			Weapon_Base weapon = Weapon_Base.Cast(inHands);
			if (weapon)
				optic = weapon.GetAttachedOptics();
			if (optic)
				fov = optic.GetCurrentStepFOV();

			// Only this client knows it: the underground presence of a player is not sent to the others
			PlayerBase player = PlayerBase.Cast(this);
			underground = player && player.GetUndergroundPresence() != EUndergroundPresence.NONE;
		}
		else if (IsInIronsights())
		{
			sights = EBRSpectateSights.IRONSIGHTS;
		}

		// The server asks again when it dropped a report (SightsResend): the current sights leave even unchanged
		if (sights == m_BR_ReportedSights && fov == m_BR_ReportedSightsFov && underground == m_BR_ReportedUnderground && !BattleRoyaleRPC.s_SightsResend)
			return;

		if (m_BR_SightsReportCooldown > 0)
			return;

		m_BR_ReportedSights = sights;
		m_BR_ReportedSightsFov = fov;
		m_BR_ReportedUnderground = underground;
		m_BR_SightsReportCooldown = 0.1;
		BattleRoyaleRPC.s_SightsResend = false;
		GetRPCManager().SendRPC(RPC_DAYZBRSERVER_NAMESPACE, "SightsState", new Param3<int, float, bool>(sights, fov, underground), true);
	}
}
#endif
#endif

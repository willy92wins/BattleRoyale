#ifdef SERVER
class BRAdminController
{
	protected static BattleRoyaleServer GetServer()
	{
		BattleRoyaleServer server;
		if (Class.CastTo(server, GetBR()))
			return server;

		Error("Failed to cast GetBR() to BattleRoyaleServer");
		return null;
	}

	static void Next()
	{
		BattleRoyaleServer server = GetServer();
		if (!server)
			return;

		BattleRoyaleUtils.Trace("[DayZBR] State machine skipping");
		server.GetCurrentState().Deactivate();
	}

	static void Pause()
	{
		BattleRoyaleServer server = GetServer();
		if (!server)
			return;

		BattleRoyaleUtils.Trace("[DayZBR] State machine pausing");
		server.GetCurrentState().Pause();
	}

	static void Resume()
	{
		BattleRoyaleServer server = GetServer();
		if (!server)
			return;

		BattleRoyaleUtils.Trace("[DayZBR] State machine resuming");
		server.GetCurrentState().Resume();
	}

	static void SpawnAirdrop(PlayerIdentity sender)
	{
		if (!sender)
			return;

		Man man = sender.GetPlayer();
		if (!man)
		{
			BattleRoyaleUtils.Warn("SpawnAirdrop denied: sender has no player, steamid=" + sender.GetPlainId());
			return;
		}

#ifdef EXPANSIONMODMISSIONS
		ExpansionNotification(new StringLocaliser(DAYZBR_MSG_TITLE), new StringLocaliser("Airdrop sent."), DAYZBR_MSG_IMAGE, COLOR_EXPANSION_NOTIFICATION_INFO, DAYZBR_MSG_TIME).Create();
		ExpansionMissionModule.s_Instance.CallAirdrop(man.GetPosition());
#endif
	}
}
#endif

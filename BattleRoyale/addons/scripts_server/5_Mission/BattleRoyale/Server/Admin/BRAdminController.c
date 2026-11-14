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

		//! Restart is the last state and its shutdown timer is the only exit: skipping it would stop the timer and leave the server stuck, so Next is ignored there
		BattleRoyaleRestart restart_state;
		if (Class.CastTo(restart_state, server.GetCurrentState()))
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
}
#endif

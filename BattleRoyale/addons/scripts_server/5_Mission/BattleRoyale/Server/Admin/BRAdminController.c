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
		string reasonKey;
		TryNext(reasonKey);
	}

	static void Pause()
	{
		string reasonKey;
		TryFreeze(reasonKey);
	}

	static void Resume()
	{
		string reasonKey;
		TryResume(reasonKey);
	}

	static bool TryNext(out string reasonKey)
	{
		BattleRoyaleServer server = GetServer();
		if (!server)
		{
			reasonKey = "STR_BR_PANEL_ERR_FAILED";
			return false;
		}

		if (server.IsPrepareWorkerRunning())
		{
			reasonKey = "STR_BR_PANEL_ERR_PREPARE_RUNNING";
			return false;
		}

		BattleRoyaleState current = server.GetCurrentState();
		if (!current || !current.IsActive())
		{
			reasonKey = "STR_BR_PANEL_ERR_WRONG_PHASE";
			return false;
		}

		//! Prepare runs a script thread with Sleep and Restart is the exit timer: Next is refused in both
		BattleRoyalePrepare prepare_state;
		BattleRoyaleRestart restart_state;
		if (Class.CastTo(prepare_state, current) || Class.CastTo(restart_state, current))
		{
			reasonKey = "STR_BR_PANEL_ERR_WRONG_PHASE";
			return false;
		}

		BattleRoyaleUtils.Trace("[DayZBR] State machine skipping");
		if (current.IsFrozen())
			current.Unfreeze(true, true);

		current.Deactivate();
		server.MarkAdminStatusDirty();
		return true;
	}

	static bool TryFreeze(out string reasonKey)
	{
		BattleRoyaleServer server = GetServer();
		if (!server)
		{
			reasonKey = "STR_BR_PANEL_ERR_FAILED";
			return false;
		}

		BattleRoyaleState current = server.GetCurrentState();
		if (!current || !current.IsActive() || !current.CanFreeze())
		{
			reasonKey = "STR_BR_PANEL_ERR_WRONG_PHASE";
			return false;
		}

		if (current.IsFrozen())
		{
			reasonKey = "STR_BR_PANEL_ERR_ALREADY_FROZEN";
			return false;
		}

		BattleRoyaleUtils.Trace("[DayZBR] State machine freezing");
		if (!current.Freeze())
		{
			reasonKey = "STR_BR_PANEL_ERR_WRONG_PHASE";
			return false;
		}

		server.MarkAdminStatusDirty();
		return true;
	}

	static bool TryResume(out string reasonKey)
	{
		BattleRoyaleServer server = GetServer();
		if (!server)
		{
			reasonKey = "STR_BR_PANEL_ERR_FAILED";
			return false;
		}

		BattleRoyaleState current = server.GetCurrentState();
		if (!current || !current.IsFrozen())
		{
			reasonKey = "STR_BR_PANEL_ERR_NOT_FROZEN";
			return false;
		}

		BattleRoyaleUtils.Trace("[DayZBR] State machine resuming");
		if (!current.Unfreeze(false, false))
		{
			reasonKey = "STR_BR_PANEL_ERR_NOT_FROZEN";
			return false;
		}

		server.MarkAdminStatusDirty();
		return true;
	}

	static bool TryForceStart(out string reasonKey)
	{
		BattleRoyaleServer server = GetServer();
		if (!server)
		{
			reasonKey = "STR_BR_PANEL_ERR_FAILED";
			return false;
		}

		BattleRoyaleDebug debugState;
		if (!Class.CastTo(debugState, server.GetCurrentState()))
		{
			reasonKey = "STR_BR_PANEL_ERR_WRONG_PHASE";
			return false;
		}

		if (!debugState.ForceStart(reasonKey))
			return false;

		BattleRoyaleUtils.Trace("[DayZBR] Admin force start");
		server.MarkAdminStatusDirty();
		return true;
	}

	static bool TryAdjustTime(int deltaSeconds, out int appliedDelta, out string reasonKey)
	{
		appliedDelta = 0;
		if (deltaSeconds != -300 && deltaSeconds != -60 && deltaSeconds != 60 && deltaSeconds != 300)
		{
			reasonKey = "STR_BR_PANEL_ERR_INVALID_DELTA";
			return false;
		}

		BattleRoyaleServer server = GetServer();
		if (!server)
		{
			reasonKey = "STR_BR_PANEL_ERR_FAILED";
			return false;
		}

		BattleRoyaleState current = server.GetCurrentState();
		if (!current || !current.IsActive() || !current.CanAdjustTime())
		{
			reasonKey = "STR_BR_PANEL_ERR_WRONG_PHASE";
			return false;
		}

		if (!current.AdjustTime(deltaSeconds, appliedDelta, reasonKey))
			return false;

		server.MarkAdminStatusDirty();
		return true;
	}

	static bool TryRestart(out string reasonKey)
	{
		BattleRoyaleServer server = GetServer();
		if (!server)
		{
			reasonKey = "STR_BR_PANEL_ERR_FAILED";
			return false;
		}

		return server.AbortMatch(reasonKey);
	}
}
#endif

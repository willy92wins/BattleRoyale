#ifdef SERVER
class BRAdminAuthorizer
{
	protected static const int DENY_LOG_INTERVAL_MS = 10000;
	protected static ref map<string, int> s_DenyLogMs;

	// Deny by default. VPP and COT are consulted only when that mod is compiled in.
	// When both are present, either grant is enough. The steamid list is used only
	// when neither admin mod is compiled.
	static bool IsAllowed(PlayerIdentity sender, string action)
	{
		if (HasGrant(sender, action))
			return true;

		string steamid = "null";
		if (sender)
			steamid = sender.GetPlainId();

		if (ShouldLogDeny(steamid + "|" + action))
			BattleRoyaleUtils.Warn("Admin RPC denied: steamid=" + steamid + " action=" + action);

		return false;
	}

	// True when COT or VPP would allow any Battle Royale admin action.
	// Does not log. The steamid list is not consulted here.
	static bool IsOperator(PlayerIdentity sender)
	{
		if (!sender)
			return false;

		if (HasFrameworkGrant(sender, "Next"))
			return true;
		if (HasFrameworkGrant(sender, "Pause"))
			return true;
		if (HasFrameworkGrant(sender, "Resume"))
			return true;
		if (HasFrameworkGrant(sender, "SpawnAirdrop"))
			return true;

		return false;
	}

	protected static bool HasGrant(PlayerIdentity sender, string action)
	{
		if (!sender)
			return false;

		if (HasFrameworkGrant(sender, action))
			return true;

		bool framework = false;
#ifdef VPPADMINTOOLS
		framework = true;
#endif
#ifdef JM_COT
		framework = true;
#endif
		if (framework)
			return false;

		BattleRoyaleGameData gameData = BattleRoyaleConfig.GetConfig().GetGameData();
		if (!gameData || !gameData.admins_steamid64)
			return false;

		return gameData.admins_steamid64.Find(sender.GetPlainId()) != -1;
	}

	protected static bool HasFrameworkGrant(PlayerIdentity sender, string action)
	{
		if (!sender)
			return false;

		bool allowed = false;

#ifdef VPPADMINTOOLS
		PermissionManager vpp = GetPermissionManager();
		if (vpp && vpp.VerifyPermission(sender.GetPlainId(), "BattleRoyale:" + action, "", false))
			allowed = true;
#endif

#ifdef JM_COT
		if (GetPermissionsManager().HasPermission("BattleRoyale.StateMachine.Update", sender))
			allowed = true;
#endif

		return allowed;
	}

	protected static bool ShouldLogDeny(string key)
	{
		if (!s_DenyLogMs)
			s_DenyLogMs = new map<string, int>();

		int nowMs = GetGame().GetTime();
		if (s_DenyLogMs.Contains(key))
		{
			int elapsed = nowMs - s_DenyLogMs.Get(key);
			if (elapsed >= 0 && elapsed < DENY_LOG_INTERVAL_MS)
				return false;
		}

		s_DenyLogMs.Set(key, nowMs);
		return true;
	}
}
#endif

#ifdef SERVER
//! Instance hook overridden by admin framework compatibility mods so virtual dispatch works
//! regardless of script compile order between the core and the framework.
class BRAdminFrameworkHook
{
	bool HasGrant(PlayerIdentity sender, string action)
	{
		return false;
	}

	bool HasStatusGrant(PlayerIdentity sender)
	{
		return false;
	}

	bool OwnsAdminList()
	{
		return false;
	}

	void NotifyDeny(PlayerIdentity sender, string action)
	{
	}
}

class BRAdminAuthorizer
{
	protected static const int DENY_LOG_INTERVAL_MS = 10000;
	protected static ref map<string, int> s_DenyLogMs;
	protected static ref BRAdminFrameworkHook s_FrameworkHook;

	protected static BRAdminFrameworkHook GetFrameworkHook()
	{
		if (!s_FrameworkHook)
			s_FrameworkHook = new BRAdminFrameworkHook();

		return s_FrameworkHook;
	}

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

		NotifyFrameworkDeny(sender, action);

		return false;
	}

	//! Separate authorization policy for live status viewing and subscription:
	//! VPP raw grant MenuBattleRoyaleManager, or COT BattleRoyale.StateMachine.View.
	static bool CanViewStatus(PlayerIdentity sender, bool logDeny = false)
	{
		if (HasStatusGrant(sender))
			return true;

		if (logDeny)
		{
			string steamid = "null";
			if (sender)
				steamid = sender.GetPlainId();

			if (ShouldLogDeny(steamid + "|Status"))
				BattleRoyaleUtils.Warn("Admin status RPC denied: steamid=" + steamid);
		}

		return false;
	}

	// True when COT or VPP would allow any Battle Royale admin mutation.
	// Does not log. The steamid list and status-only view grant are not consulted here.
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
		if (HasFrameworkGrant(sender, "ForceStart"))
			return true;
		if (HasFrameworkGrant(sender, "AdjustTime"))
			return true;
		if (HasFrameworkGrant(sender, "Restart"))
			return true;

		return false;
	}

	//! Status is granted only by an admin framework view permission, never by the steamid list.
	protected static bool HasStatusGrant(PlayerIdentity sender)
	{
		if (!sender)
			return false;

		return HasFrameworkStatusGrant(sender);
	}

	protected static bool HasFrameworkStatusGrant(PlayerIdentity sender)
	{
		if (!sender)
			return false;

		bool allowed = false;

#ifdef VPPADMINTOOLS
		PermissionManager vpp = GetPermissionManager();
		if (vpp && vpp.VerifyPermission(sender.GetPlainId(), "MenuBattleRoyaleManager", "", false))
			allowed = true;
#endif

#ifdef JM_COT
		if (GetPermissionsManager().HasPermission("BattleRoyale.StateMachine.View", sender))
			allowed = true;
#endif

		if (!allowed)
		{
			BRAdminFrameworkHook hook = GetFrameworkHook();
			if (hook && hook.HasStatusGrant(sender))
				allowed = true;
		}

		return allowed;
	}

	protected static void NotifyFrameworkDeny(PlayerIdentity sender, string action)
	{
		if (!sender)
			return;

#ifdef VPPADMINTOOLS
		PermissionManager vpp = GetPermissionManager();
		if (vpp && vpp.HasUserGroup(sender.GetPlainId()))
			vpp.NotifyPlayer(sender.GetPlainId(), "#VSTR_ERROR_NO_PERM BattleRoyale:" + action, NotifyTypes.PERMISSION_REJECT);
#else
		BRAdminFrameworkHook hook = GetFrameworkHook();
		if (hook)
			hook.NotifyDeny(sender, action);
#endif
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
		BRAdminFrameworkHook hook = GetFrameworkHook();
		if (hook && hook.OwnsAdminList())
			framework = true;
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

		if (!allowed)
		{
			BRAdminFrameworkHook hook = GetFrameworkHook();
			if (hook && hook.HasGrant(sender, action))
				allowed = true;
		}

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

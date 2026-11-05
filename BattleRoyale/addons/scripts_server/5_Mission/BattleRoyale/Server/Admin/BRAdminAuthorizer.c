#ifdef SERVER
class BRAdminAuthorizer
{
	// Deny by default. VPP and COT are consulted only when that mod is compiled in.
	// When both are present, either grant is enough. The steamid list is used only
	// when neither admin mod is compiled.
	static bool IsAllowed(PlayerIdentity sender, string action)
	{
		if (!sender)
		{
			BattleRoyaleUtils.Warn("Admin RPC denied: null sender, action=" + action);
			return false;
		}

		string steamid = sender.GetPlainId();
		bool checked = false;
		bool allowed = false;

#ifdef VPPADMINTOOLS
		checked = true;
		PermissionManager vpp = GetPermissionManager();
		if (vpp && vpp.VerifyPermission(steamid, "BattleRoyale:" + action, "", false))
			allowed = true;
#endif

#ifdef JM_COT
		checked = true;
		if (GetPermissionsManager().HasPermission("BattleRoyale.StateMachine.Update", sender))
			allowed = true;
#endif

		if (!checked)
		{
			BattleRoyaleGameData gameData = BattleRoyaleConfig.GetConfig().GetGameData();
			if (gameData && gameData.admins_steamid64 && gameData.admins_steamid64.Find(steamid) != -1)
				allowed = true;
		}

		if (!allowed)
			BattleRoyaleUtils.Warn("Admin RPC denied: steamid=" + steamid + " action=" + action);

		return allowed;
	}
}
#endif

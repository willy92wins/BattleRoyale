#ifdef VPPADMINTOOLS
#ifdef SERVER
//! VPP registration owned by the compatibility mod.
//! VPPADMINTOOLS comes from a #define in VPP's own scripts, so it is only visible to scripts compiled after VPP.
//! The core does not require VPP, and with BattleRoyale ahead of VPP on -mod= its VPP blocks compile out.
//! This mod requires VPP and the core server scripts, so it always compiles after both.
modded class BattleRoyaleServer
{
    override void Init()
    {
        super.Init();

        //! A permission already registered by the core is only logged as a duplicate by VPP
        GetPermissionManager().AddPermissionType({ "MenuBattleRoyaleManager", "BattleRoyale:Next", "BattleRoyale:Pause", "BattleRoyale:Resume", "BattleRoyale:ForceStart", "BattleRoyale:AdjustTime", "BattleRoyale:Restart" });

        //! A separate RPC name, so it never collides with the core NextState registration
        GetRPCManager().AddRPC( RPC_DAYZBRSERVER_NAMESPACE, "VPPNextState", this, SingleplayerExecutionType.Server);
    }

    //! Next state from the VPP menu. The modded BRAdminAuthorizer below consults VPP and throttles the deny log.
    void VPPNextState(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
    {
        if (type != CallType.Server || !sender)
            return;

        if (!BRAdminAuthorizer.IsAllowed(sender, "Next"))
            return;

        BattleRoyaleUtils.Trace("BattleRoyaleManager VPPNextState");
        BRAdminController.Next();
    }
};

//! The core authorizer consults VPP only when it compiled after VPP. This mod always does, so VPP is
//! always consulted and, as in the core when VPP is compiled in, the BattleRoyale steamid list is not.
modded class BRAdminAuthorizer
{
    override protected static bool HasGrant(PlayerIdentity sender, string action)
    {
        if (!sender)
            return false;

        return HasFrameworkGrant(sender, action);
    }

    override protected static bool HasFrameworkGrant(PlayerIdentity sender, string action)
    {
        if (super.HasFrameworkGrant(sender, action))
            return true;

        if (!sender)
            return false;

        PermissionManager vpp = GetPermissionManager();
        return vpp && vpp.VerifyPermission(sender.GetPlainId(), "BattleRoyale:" + action, "", false);
    }

    override protected static bool HasStatusGrant(PlayerIdentity sender)
    {
        if (!sender)
            return false;

        return HasFrameworkStatusGrant(sender);
    }

    override protected static bool HasFrameworkStatusGrant(PlayerIdentity sender)
    {
        if (super.HasFrameworkStatusGrant(sender))
            return true;

        if (!sender)
            return false;

        PermissionManager vpp = GetPermissionManager();
        return vpp && vpp.VerifyPermission(sender.GetPlainId(), "MenuBattleRoyaleManager", "", false);
    }

    override protected static void NotifyFrameworkDeny(PlayerIdentity sender, string action)
    {
        if (!sender)
            return;

        PermissionManager vpp = GetPermissionManager();
        if (vpp && vpp.HasUserGroup(sender.GetPlainId()))
            vpp.NotifyPlayer(sender.GetPlainId(), "#VSTR_ERROR_NO_PERM BattleRoyale:" + action, NotifyTypes.PERMISSION_REJECT);
    }
};
#endif
#endif

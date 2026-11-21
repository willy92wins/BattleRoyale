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

    //! Next state from the VPP menu. The modded BRAdminFrameworkHook below consults VPP and throttles the deny log.
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

//! The core authorizer consults VPP directly only when it compiled after VPP. This mod always does, so
//! overriding the instance hook ensures VPP is always consulted and the BattleRoyale steamid list is not.
modded class BRAdminFrameworkHook
{
    override bool HasGrant(PlayerIdentity sender, string action)
    {
        if (super.HasGrant(sender, action))
            return true;

        if (!sender)
            return false;

        PermissionManager vpp = GetPermissionManager();
        return vpp && vpp.VerifyPermission(sender.GetPlainId(), "BattleRoyale:" + action, "", false);
    }

    override bool HasStatusGrant(PlayerIdentity sender)
    {
        if (super.HasStatusGrant(sender))
            return true;

        if (!sender)
            return false;

        PermissionManager vpp = GetPermissionManager();
        return vpp && vpp.VerifyPermission(sender.GetPlainId(), "MenuBattleRoyaleManager", "", false);
    }

    override bool OwnsAdminList()
    {
        return true;
    }

    override void NotifyDeny(PlayerIdentity sender, string action)
    {
        super.NotifyDeny(sender, action);

        if (!sender)
            return;

        PermissionManager vpp = GetPermissionManager();
        if (vpp && vpp.HasUserGroup(sender.GetPlainId()))
            vpp.NotifyPlayer(sender.GetPlainId(), "#VSTR_ERROR_NO_PERM BattleRoyale:" + action, NotifyTypes.PERMISSION_REJECT);
    }
};
#endif
#endif

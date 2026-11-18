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
        GetPermissionManager().AddPermissionType({ "MenuBattleRoyaleManager", "BattleRoyale:Next", "BattleRoyale:Pause", "BattleRoyale:Resume" });

        //! A separate RPC name, so it never collides with the core NextState registration
        GetRPCManager().AddRPC( RPC_DAYZBRSERVER_NAMESPACE, "VPPNextState", this, SingleplayerExecutionType.Server);
    }

    //! Next state from the VPP menu. VPP decides the permission, whatever the core authorizer was compiled with.
    void VPPNextState(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
    {
        if (type != CallType.Server || !sender)
            return;

        if (!GetPermissionManager().VerifyPermission(sender.GetPlainId(), "BattleRoyale:Next"))
        {
            BattleRoyaleUtils.Warn("VPP NextState denied: steamid=" + sender.GetPlainId());
            return;
        }

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
};
#endif
#endif

#ifdef SERVER
#ifdef JM_COT
modded class BRMasterControlsModule
{
#ifdef CF_BUGFIX_REF
    override void OnRPC( PlayerIdentity sender, Object target, int rpc_type, ParamsReadContext ctx )
#else
    override void OnRPC( PlayerIdentity sender, Object target, int rpc_type, ref ParamsReadContext ctx )
#endif
    {
        switch ( rpc_type )
        {
            case BattleRoyaleCOTStateMachineRPC.Next:
                RPC_Next( ctx, sender, target );
            break;
            case BattleRoyaleCOTStateMachineRPC.Pause:
                RPC_Pause( ctx, sender, target );
            break;
            case BattleRoyaleCOTStateMachineRPC.Resume:
                RPC_Resume( ctx, sender, target );
            break;
        }
    }

    private void RPC_Next( ParamsReadContext ctx, PlayerIdentity senderRPC, Object target )
    {
        if (!BRAdminAuthorizer.IsAllowed(senderRPC, "Next"))
            return;

        BRAdminController.Next();
    }

    private void RPC_Pause( ParamsReadContext ctx, PlayerIdentity senderRPC, Object target )
    {
        if (!BRAdminAuthorizer.IsAllowed(senderRPC, "Pause"))
            return;

        BRAdminController.Pause();
    }

    private void RPC_Resume( ParamsReadContext ctx, PlayerIdentity senderRPC, Object target )
    {
        if (!BRAdminAuthorizer.IsAllowed(senderRPC, "Resume"))
            return;

        BRAdminController.Resume();
    }
}
#endif // JM_COT
#endif

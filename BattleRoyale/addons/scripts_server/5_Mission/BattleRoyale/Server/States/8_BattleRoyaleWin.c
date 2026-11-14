#ifdef SERVER
class BattleRoyaleWin: BattleRoyaleState
{
    int i_SecondsTillKick;
    bool complete;

    protected ref Timer m_KickTimer;

    void BattleRoyaleWin()
    {
        i_SecondsTillKick = 15; //TODO: config this
        complete = false;
    }

    //TODO: state functionality for winners!
    //TODO: Add party support
	override void Activate()
	{
		super.Activate();
        BroadcastMatchPhase(EBRMatchPhase.FINISHED, -1);

		string winner_name = "<NO:WINNER>";
		if(GetPlayers().Count() > 0)
		{
			for ( int k = 0; k < GetPlayers().Count(); k++ )
			{
				PlayerBase winner = GetPlayers()[k];
				PlayerIdentity identity = winner.GetIdentity();
				if(identity)
				{
					winner_name = identity.GetName();
					BattleRoyaleUtils.Trace("[Win State] Winner!");
					BattleRoyaleUtils.Trace(identity.GetName());
					BattleRoyaleUtils.Trace(identity.GetFullName());
					BattleRoyaleUtils.Trace(identity.GetId());
					BattleRoyaleUtils.Trace(identity.GetPlainId());
				}
				HandleWinner(winner);
			}
		}

		m_KickTimer = AddTimer(i_SecondsTillKick, this, "KickWinner", NULL, false);
	}

    override string GetName()
    {
        return "Win State";
    }

    override void Deactivate()
    {
        if ( m_KickTimer && m_KickTimer.IsRunning() )
            m_KickTimer.Stop();
        
        super.Deactivate();
    }

    override bool IsComplete()
    {
        if(GetPlayers().Count() == 0 && IsActive())
            Deactivate();
        
        return super.IsComplete(); //go to restart state when player disconnects
    }

    void HandleWinner(PlayerBase player_winner)
    {
        if (!player_winner || !player_winner.GetIdentity())
        {
            Error("HandleWinner called without a winner");
            return;
        }

        BattleRoyaleConfig m_Config = BattleRoyaleConfig.GetConfig();
        BattleRoyaleServerData m_ServerData = m_Config.GetServerData();

        // Send notification
        MessagePlayerUntranslated(player_winner, "STR_BR_ANNOUNCEMENT_WINNER");

        // Show win screen
		GetRPCManager().SendRPC( RPC_DAYZBR_NAMESPACE, "ShowWinScreen", NULL, true, player_winner.GetIdentity() );

		if ( m_ServerData.enable_vigrid_api )
		{
			// Send win webhook
			BattleRoyaleServer br_instance = BattleRoyaleServer.GetInstance();
			WinWebhook winWebhook = new WinWebhook( m_ServerData.webhook_jwt_token );
			winWebhook.Send( br_instance.match_uuid, player_winner.GetIdentity().GetPlainId() );

			// Send score webhook
			BattleRoyaleUtils.Trace("ScoreWebhook: Sending winner score");
			ScoreWebhook scoreWebhook = new ScoreWebhook( m_ServerData.webhook_jwt_token );
			scoreWebhook.Send( br_instance.match_uuid, player_winner.GetIdentity().GetPlainId(), player_winner.GetBRPosition() );
		}

        // Fireworks around the winner: launchers in a ring, lit one after another so the shots keep coming
        // through the win screen (each one has a 3 s fuse, then a shot every 3 s). A shot does 1 damage within 1-2 m
        // of its launcher (CfgAmmo Fireworks_Ammo), and the launchers stand FIREWORKS_DISTANCE metres away
        vector winner_position = player_winner.GetPosition();
        for (int j = 0; j < FIREWORKS_COUNT; j++)
        {
            float angle = (j * 360.0 / FIREWORKS_COUNT) * Math.DEG2RAD;
            vector position;
            position[0] = winner_position[0] + ( FIREWORKS_DISTANCE * Math.Cos(angle) );
            position[2] = winner_position[2] + ( FIREWORKS_DISTANCE * Math.Sin(angle) );
            position[1] = GetGame().SurfaceY(position[0], position[2]);

            FireworksLauncher launcher = FireworksLauncher.Cast( GetGame().CreateObjectEx( "FireworksLauncher", position, ECE_PLACE_ON_SURFACE ) );
            if (!launcher)
                continue;

            // Serial matches: the launchers leave with the match
            BRMatchDebris.Get().AddObject(launcher);
            GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(this.LightFireworks, j * FIREWORKS_STAGGER_MS, false, launcher);
        }
    }

    protected static const int FIREWORKS_COUNT = 4;
    protected static const float FIREWORKS_DISTANCE = 8.0;
    protected static const int FIREWORKS_STAGGER_MS = 750;

    void LightFireworks(FireworksLauncher launcher)
    {
        if (launcher)
            launcher.OnIgnitedThis(null);
    }

    override void CancelPendingWork()
    {
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).Remove(this.LightFireworks);
    }

    void KickWinner()
    {
        // Serial matches: the winner stays connected and goes back to the lobby with everyone else.
        // The players move on to the restart state with this state; outside every state they would be logged out
        if (BRMatchDebris.s_Enabled)
        {
            Deactivate();
            return;
        }

		for ( int k = GetPlayers().Count() - 1; k >= 0; k-- )
		{
			PlayerBase winner = GetPlayers()[k];
			if(winner && winner.GetIdentity() )
			{
				PlayerIdentity identity = winner.GetIdentity();
				RemovePlayer(winner); //disconnect does not trigger RemovePlayer !
				GetGame().DisconnectPlayer( identity );
			}
		}

        Deactivate();
    }
}
#endif

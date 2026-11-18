#ifdef SERVER
class BattleRoyaleRestart: BattleRoyaleState
{
    protected ref Timer m_ShutdownTimer;

    //! Seconds between entering this state and the server exit; the win screen before it already lasts its own time
    protected static const int SHUTDOWN_DELAY_SECONDS = 3;
    //! With the Vigrid API on, score webhooks may still be in flight (10 s HTTP timeout and retries), so the exit waits longer
    protected static const int SHUTDOWN_DELAY_SECONDS_WITH_API = 10;
    //! Longest extra wait for the Discord webhooks, a second at a time: the default REST connection timeout (restapi.c)
    protected static const int WEBHOOK_WAIT_SECONDS = 10;

    protected int m_WebhookWaitedSeconds;

    override void Activate()
    {
        super.Activate();
        m_WebhookWaitedSeconds = 0;

        int delay = SHUTDOWN_DELAY_SECONDS;
        BattleRoyaleServerData server_data = BattleRoyaleConfig.GetConfig().GetServerData();
        if (server_data && server_data.enable_vigrid_api)
            delay = SHUTDOWN_DELAY_SECONDS_WITH_API;

        //! RESTARTING only when the server will actually exit after this match; in serial matches without a restart due the clients keep "match over" until they are back in the lobby
        BattleRoyaleServer server = BattleRoyaleServer.GetInstance();
        if (server && !server.WillRestartAfterMatch())
            BroadcastMatchPhase(EBRMatchPhase.FINISHED, -1);
        else
            BroadcastMatchPhase(EBRMatchPhase.RESTARTING, delay);

        BattleRoyaleUtils.Trace("[Restart State] Restarting!");

        m_ShutdownTimer = AddTimer(delay, this, "Shutdown", NULL, false);
    }

    override string GetName()
    {
        return "Restart State";
    }

    override void Deactivate()
    {
        Error("RESTART STATE WAS DEACTIVATED!");
        if ( m_ShutdownTimer && m_ShutdownTimer.IsRunning() )
        {
            m_ShutdownTimer.Stop();
        }
        
        super.Deactivate();
    }

    override bool IsComplete()
    {
        return false;
    }

    override void CancelPendingWork()
    {
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).Remove(this.Shutdown);
    }

    void Shutdown()
    {
        // The match end message can still be on its way: without a winner the win state ends at once, and a slow
        // connection takes longer than SHUTDOWN_DELAY_SECONDS. Called again from the call queue, as the timer that runs
        // this is the only reference to itself
        if (BRDiscordWebhook.IsPending() && m_WebhookWaitedSeconds < WEBHOOK_WAIT_SECONDS)
        {
            if (m_WebhookWaitedSeconds == 0)
                BattleRoyaleUtils.Info("[Restart State] Waiting for the Discord webhooks");

            m_WebhookWaitedSeconds++;
            GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(this.Shutdown, 1000, false);
            return;
        }

        // Serial matches: the next match follows in this server run, unless the scheduled full restart is due
        BattleRoyaleServer server = BattleRoyaleServer.GetInstance();
        if (server && server.RequestTurnover())
            return;

        GetGame().RequestExit(0);
    }
}
#endif

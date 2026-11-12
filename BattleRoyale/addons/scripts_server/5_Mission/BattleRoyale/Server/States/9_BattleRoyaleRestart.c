#ifdef SERVER
class BattleRoyaleRestart: BattleRoyaleState
{
    protected ref Timer m_ShutdownTimer;

    //! Seconds between entering this state and the server exit; the win screen before it already lasts its own time
    protected static const int SHUTDOWN_DELAY_SECONDS = 3;
    //! With the Vigrid API on, score webhooks may still be in flight (10 s HTTP timeout and retries), so the exit waits longer
    protected static const int SHUTDOWN_DELAY_SECONDS_WITH_API = 10;

    override void Activate()
    {
        super.Activate();

        int delay = SHUTDOWN_DELAY_SECONDS;
        BattleRoyaleServerData server_data = BattleRoyaleConfig.GetConfig().GetServerData();
        if (server_data && server_data.enable_vigrid_api)
            delay = SHUTDOWN_DELAY_SECONDS_WITH_API;

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

    void Shutdown()
    {
        GetGame().RequestExit(0);
    }
}
#endif

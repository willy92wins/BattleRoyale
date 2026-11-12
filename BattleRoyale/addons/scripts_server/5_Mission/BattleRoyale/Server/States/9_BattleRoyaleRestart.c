#ifdef SERVER
class BattleRoyaleRestart: BattleRoyaleState
{
    protected ref Timer m_ShutdownTimer;

    //! Seconds between entering this state and the server exit; the win screen before it already lasts its own time
    protected static const int SHUTDOWN_DELAY_SECONDS = 3;

    override void Activate()
    {
        super.Activate();
        BroadcastMatchPhase(EBRMatchPhase.RESTARTING, SHUTDOWN_DELAY_SECONDS);

        BattleRoyaleUtils.Trace("[Restart State] Restarting!");

        m_ShutdownTimer = AddTimer(SHUTDOWN_DELAY_SECONDS, this, "Shutdown", NULL, false);
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

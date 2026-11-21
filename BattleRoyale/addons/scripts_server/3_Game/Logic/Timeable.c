#ifdef SERVER
class Timeable
{
    protected ref array<ref Timer> m_Timers;
    protected ref array<ref Timer> m_PausedTimers;

    void Timeable()
    {
        m_Timers = new array<ref Timer>;
        m_PausedTimers = new array<ref Timer>;
    }

    void ~Timeable()
    {
        StopTimers();

        if (m_PausedTimers)
            m_PausedTimers.Clear();
        if (m_Timers)
            m_Timers.Clear();
    }

    ref Timer AddTimer(float duration, Managed obj, string fn_name, Param params = NULL, bool loop = false)
    {
        ref Timer m_Timer = new Timer;
        m_Timer.Run( duration, obj, fn_name, params, loop );
        m_Timers.Insert( m_Timer );

        return m_Timer;
    }

    Timer AddPausedTimer(float duration, Managed obj, string fn_name, Param params = NULL, bool loop = false)
    {
        Timer pausedTimer = AddTimer(duration, obj, fn_name, params, loop);
        if (pausedTimer)
        {
            pausedTimer.Pause();
            m_PausedTimers.Insert(pausedTimer);
        }
        return pausedTimer;
    }

    bool RemoveTimer(Timer m_Timer)
    {
        if (m_PausedTimers)
        {
            int pausedIdx = m_PausedTimers.Find(m_Timer);
            if (pausedIdx != -1)
                m_PausedTimers.Remove(pausedIdx);
        }

        int index = m_Timers.Find(m_Timer);
        if(index == -1)
            return false;

        m_Timers.Remove(index);
        return true;
    }

    bool IsTimerPaused(Timer m_Timer)
    {
        if (!m_Timer || !m_PausedTimers)
            return false;
        return m_PausedTimers.Find(m_Timer) != -1;
    }

    bool IsTimerActiveOrPaused(Timer m_Timer)
    {
        if (!m_Timer)
            return false;
        return m_Timer.IsRunning() || IsTimerPaused(m_Timer);
    }

    void PauseTimers()
    {
        if (!m_PausedTimers)
            m_PausedTimers = new array<ref Timer>;
        m_PausedTimers.Clear();

        if (!m_Timers)
            return;

        for (int i = 0; i < m_Timers.Count(); i++)
        {
            Timer t = m_Timers[i];
            if (t && t.IsRunning())
            {
                t.Pause();
                m_PausedTimers.Insert(t);
            }
        }
    }

    void ContinueTimers()
    {
        if (!m_PausedTimers)
            return;

        for (int i = 0; i < m_PausedTimers.Count(); i++)
        {
            Timer t = m_PausedTimers[i];
            if (t)
                t.Continue();
        }
        m_PausedTimers.Clear();
    }

    void StopTimers()
    {
        if (m_Timers)
        {
            for (int i = 0; i < m_Timers.Count(); i++)
            {
                if (m_Timers[i])
                    m_Timers[i].Stop();
            }
            m_Timers.Clear();
        }

        if (m_PausedTimers)
            m_PausedTimers.Clear();
    }
};
#endif
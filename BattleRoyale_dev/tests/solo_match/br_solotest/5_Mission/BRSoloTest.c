#ifdef SERVER
// Local tests only, never shipped: one player plays the rounds of a match.
// Start, round and last round normally end the match as soon as one player is left;
// here they end on their own timers, or when no player is left.

modded class BattleRoyaleStartMatch
{
    override bool IsComplete()
    {
        if (IsPaused())
            return false;

        return !IsActive();
    }
}

modded class BattleRoyaleRound
{
    override bool IsComplete()
    {
        if (IsActive() && GetPlayers().Count() == 0)
            Deactivate();

        if (IsPaused())
            return false;

        return !IsActive();
    }

    override bool SkipState(BattleRoyaleState _previousState)
    {
        return GetDynamicStartingZone(i_NumStartingPlayers) > zone_num;
    }
}

modded class BattleRoyaleLastRound
{
    override bool IsComplete()
    {
        if (IsActive() && GetPlayers().Count() == 0)
            Deactivate();

        if (IsPaused())
            return false;

        return !IsActive();
    }

    override bool SkipState(BattleRoyaleState _previousState)
    {
        return false;
    }
}
#endif

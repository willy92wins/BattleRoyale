#ifdef SERVER
//! Serial matches: what happens between the end of a match and the next lobby, one bounded step per server update.
//! 1. cancel the finished match's pending work, 2. build the next match and open its lobby, 3. give every connected
//! player a new character in the lobby, a few per update (the lobby cannot start a match meanwhile), 4. delete the
//! recorded leftovers, a few per update. Anything that fails or takes longer than the deadline ends in the usual exit.
class BRMatchTurnover
{
	static const float DEADLINE_SECONDS = 120;
	static const int RETURNS_PER_UPDATE = 8;
	static const int DELETES_PER_UPDATE = 60;
	//! Tries per identity before the turnover gives up and the server restarts
	static const int RETURN_ATTEMPTS = 3;

	protected const int PHASE_PREPARE = 0;
	protected const int PHASE_RETURN = 1;
	protected const int PHASE_CLEAN = 2;

	protected BattleRoyaleServer m_Server;
	protected int m_MatchNumber;
	protected int m_Phase;
	protected float m_StartedAt;
	//! Identities still to return, and how many times each was tried (same index)
	protected ref array<PlayerIdentity> m_Pending;
	protected ref array<int> m_Attempts;
	protected int m_Returned;
	protected ref BRMatchDebrisBatch m_Debris;

	void BRMatchTurnover(BattleRoyaleServer server, int match_number)
	{
		m_Server = server;
		m_MatchNumber = match_number;
		m_Phase = PHASE_PREPARE;
		m_StartedAt = GetGame().GetTickTime();
		m_Pending = new array<PlayerIdentity>;
		m_Attempts = new array<int>;
		BattleRoyaleUtils.Info("[Serial] Match " + match_number + " over: preparing the next lobby");
	}

	//! True until every connected player has a lobby character
	bool IsReturning()
	{
		return m_Phase <= PHASE_RETURN;
	}

	//! One step; false once the turnover is over (finished or aborted)
	bool Step()
	{
		if (!m_Server)
			return false;

		if (GetGame().GetTickTime() - m_StartedAt > DEADLINE_SECONDS)
		{
			m_Server.Turnover_Abort("took more than " + DEADLINE_SECONDS + " s");
			return false;
		}

		switch (m_Phase)
		{
			case PHASE_PREPARE:
				return Prepare();
			case PHASE_RETURN:
				return ReturnSome();
			case PHASE_CLEAN:
				return CleanSome();
		}

		return false;
	}

	protected bool Prepare()
	{
		if (m_Server.Turnover_IsPrepareRunning())
		{
			m_Server.Turnover_Abort("the preparation thread of the finished match is still running");
			return false;
		}

		m_Server.Turnover_CancelOldMatch();

		// The record of the finished match; the next match records into a new one from now on
		m_Debris = BRMatchDebris.Get().TakeAll();

		m_Server.Turnover_BuildNextMatch();

		array<PlayerIdentity> identities = new array<PlayerIdentity>;
		GetGame().GetPlayerIndentities(identities);

		// Living players first, in this same update: alive and outside every state, the state machine would log them out
		foreach (PlayerIdentity identity : identities)
		{
			if (!identity)
				continue;

			PlayerBase player = PlayerBase.Cast( identity.GetPlayer() );
			if (player && player.IsAlive())
			{
				if (!Return(identity, 0))
					return false;
			}
			else
			{
				m_Pending.Insert(identity);
				m_Attempts.Insert(0);
			}
		}

		m_Phase = PHASE_RETURN;
		return true;
	}

	protected bool ReturnSome()
	{
		int budget = RETURNS_PER_UPDATE;
		while (budget > 0 && m_Pending.Count() > 0)
		{
			PlayerIdentity identity = m_Pending[0];
			int attempts = m_Attempts[0];
			m_Pending.Remove(0);
			m_Attempts.Remove(0);
			if (!identity)
				continue;

			budget--;
			if (!Return(identity, attempts))
				return false;
		}

		if (m_Pending.Count() == 0)
			m_Phase = PHASE_CLEAN;

		return true;
	}

	//! Returns one identity; a failure is tried again on a later update, and aborts the turnover once out of tries
	protected bool Return(PlayerIdentity identity, int attempts)
	{
		int result = m_Server.Turnover_ReturnToLobby(identity);
		if (result == BattleRoyaleServer.RETURN_DONE)
		{
			m_Returned++;
			return true;
		}

		if (result == BattleRoyaleServer.RETURN_SKIPPED)
			return true;

		attempts++;
		if (attempts >= RETURN_ATTEMPTS)
		{
			m_Server.Turnover_Abort("no lobby character for " + identity.GetName() + " after " + attempts + " tries");
			return false;
		}

		m_Pending.Insert(identity);
		m_Attempts.Insert(attempts);
		return true;
	}

	protected bool CleanSome()
	{
		if (m_Debris && !m_Debris.IsDone())
		{
			m_Debris.DeleteSome(DELETES_PER_UPDATE);
			return true;
		}

		m_Server.Turnover_Finished(m_MatchNumber, GetGame().GetTickTime() - m_StartedAt, m_Returned, m_Debris);
		return false;
	}
}
#endif

//! Silent mode, switched by the player with its key (UADayZBRToggleSilentMode): the voices of the other players (the
//! VOIP volume of the sound scene) are muted and the notification sound does not play. Notifications still show and
//! the player's own microphone is not touched.
//! The choice lasts the whole game session (serial matches, back to the lobby, another server). The volume is given
//! back whenever the mission ends, and muted again by the next one, so the mode never leaves the player's VOIP at 0.
//! Not guarded by SERVER: NotificationUI, compiled without that guard, reads IsActive; only the client mission switches it
class BattleRoyaleSilentMode
{
	protected static const float FADE_TIME = 0.2;

	protected static bool s_Active;             //!< the player switched the mode on
	protected static bool s_Muted;              //!< this mode muted the VOIP of the sound scene and owes it back
	protected static float s_SavedVOIPVolume;   //!< VOIP volume before this mode muted it

	static bool IsActive()
	{
		return s_Active;
	}

	//! Key press: switches the mode on or off and returns the new state
	static bool Toggle()
	{
		s_Active = !s_Active;
		if (s_Active)
			Apply();
		else
			Restore(FADE_TIME);

		return s_Active;
	}

	//! Mutes the voices while the mode is on: when it is switched on, when a mission starts, and after something gave
	//! the VOIP volume back (end of the death, respawn). The volume is saved only by the first mute, never the muted one
	static void Apply()
	{
		if (!s_Active)
			return;

		AbstractSoundScene scene = GetGame().GetSoundScene();
		if (!scene)
			return;

		if (!s_Muted)
		{
			s_SavedVOIPVolume = scene.GetVOIPVolume();
			// The vanilla death muted the VOIP (DayZPlayerImplement.SimulateDeath): give back the one of the options
			if (s_SavedVOIPVolume <= 0)
			{
				DayZGame game = DayZGame.Cast(GetGame());
				if (game)
					s_SavedVOIPVolume = game.m_volume_VOIP;
			}
			s_Muted = true;
		}

		scene.SetVOIPVolume(0, FADE_TIME);
	}

	//! Gives back the VOIP volume saved by Apply, if this mode muted it; the mode itself stays as it is
	static void Restore(float fade_time)
	{
		if (!s_Muted)
			return;

		s_Muted = false;

		AbstractSoundScene scene = GetGame().GetSoundScene();
		if (scene)
			scene.SetVOIPVolume(s_SavedVOIPVolume, fade_time);
	}
}

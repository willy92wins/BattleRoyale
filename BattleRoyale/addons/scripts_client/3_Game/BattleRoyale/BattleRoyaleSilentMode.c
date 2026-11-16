//! Silent mode, switched by the player with its key (UADayZBRToggleSilentMode): the voices of the other players (the
//! VOIP volume of the sound scene) are muted and the notification sound does not play. Notifications still show and
//! the player's own microphone is not touched.
//! The choice lasts the whole game session (serial matches, back to the lobby, another server). The volume is given
//! back whenever the mission ends, and muted again by the next one, so the mode never leaves the player's VOIP at 0.
//! The volume given back is the one the player chose, kept apart from the mute: the one of the options when the game
//! started, then every value the options menu gave the sound scene since (BeginOptions, EndOptions).
//! Not guarded by SERVER: NotificationUI, compiled without that guard, reads IsActive; only the client mission switches it
class BattleRoyaleSilentMode
{
	protected static const float FADE_TIME = 0.2;

	protected static bool s_Active;             //!< the player switched the mode on
	protected static bool s_Muted;              //!< this mode muted the VOIP of the sound scene and owes it back
	protected static bool s_PlayerVOIPKnown;    //!< s_PlayerVOIPVolume holds the choice of the player
	protected static float s_PlayerVOIPVolume;  //!< VOIP volume the player chose, in the scale of the sound scene
	protected static int s_OptionsDepth;        //!< options calls running, one inside another (BeginOptions)
	protected static bool s_OptionsVOIPChanged; //!< the VOIP option of the open options menu was written

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
	//! the VOIP volume back (end of the death, respawn). The volume of the player is not read here, it is kept apart
	static void Apply()
	{
		if (!s_Active)
			return;

		AbstractSoundScene scene = GetGame().GetSoundScene();
		if (!scene)
			return;

		s_Muted = true;
		scene.SetVOIPVolume(0, FADE_TIME);
	}

	//! Gives back the VOIP volume the player chose, if this mode muted it; the mode itself stays as it is
	static void Restore(float fade_time)
	{
		if (!s_Muted)
			return;

		s_Muted = false;

		AbstractSoundScene scene = GetGame().GetSoundScene();
		if (scene)
			scene.SetVOIPVolume(GetPlayerVOIPVolume(), fade_time);
	}

	//! VOIP volume the player chose. Until the options menu gives one, the one vanilla read from the sound scene once
	//! the options of the player were loaded, when the game started (DayZGame.DeferredInit). A 0 chosen stays 0
	protected static float GetPlayerVOIPVolume()
	{
		if (!s_PlayerVOIPKnown)
		{
			DayZGame game = DayZGame.Cast(GetGame());
			if (game)
			{
				s_PlayerVOIPVolume = game.m_volume_VOIP;
				s_PlayerVOIPKnown = true;
			}
		}

		return s_PlayerVOIPVolume;
	}

	//! The options menu is about to create, write, test, apply or revert its options. While the mode mutes the voices,
	//! the sound scene gets the volume of the player back at once, so the options never see the 0 of this mode (it
	//! can not reach the file Apply saves) and what they set on the scene can be read in EndOptions.
	//! new_menu: a new options menu starts, which forgets the VOIP writes of the one before
	static void BeginOptions(bool new_menu)
	{
		if (new_menu)
		{
			s_OptionsDepth = 0;
			s_OptionsVOIPChanged = false;
		}

		s_OptionsDepth++;
		if (s_OptionsDepth > 1 || !s_Muted)
			return;

		AbstractSoundScene scene = GetGame().GetSoundScene();
		if (scene)
			scene.SetVOIPVolume(GetPlayerVOIPVolume(), 0);
	}

	//! The VOIP slider of the options menu wrote its option: moved, reverted or set to its default
	static void OnOptionsVOIPChanged()
	{
		s_OptionsVOIPChanged = true;
	}

	//! The options call started by BeginOptions is over. Once the VOIP option of this menu was written, the volume the
	//! options left on the sound scene is the choice of the player; a 0 counts only when options holds its VOIP at the
	//! minimum, any other 0 is a mute that is not the player's (the vanilla death). The mode then mutes again at once
	static void EndOptions(GameOptions options)
	{
		if (s_OptionsDepth > 0)
			s_OptionsDepth--;

		if (s_OptionsDepth > 0)
			return;

		AbstractSoundScene scene = GetGame().GetSoundScene();
		if (!scene)
			return;

		if (s_OptionsVOIPChanged)
		{
			float volume = scene.GetVOIPVolume();
			if (volume > 0 || IsVOIPOptionSilent(options))
			{
				s_PlayerVOIPVolume = volume;
				s_PlayerVOIPKnown = true;
			}
		}

		if (s_Muted)
			scene.SetVOIPVolume(0, 0);
	}

	//! The VOIP option of options is at its minimum, compared in the scale of the option itself
	protected static bool IsVOIPOptionSilent(GameOptions options)
	{
		if (!options)
			return false;

		NumericOptionsAccess voip = NumericOptionsAccess.Cast(options.GetOptionByType(OptionAccessType.AT_OPTIONS_VON_SLIDER));
		if (!voip)
			return false;

		if (voip.ReadValue() <= voip.GetMin())
			return true;

		return false;
	}
}

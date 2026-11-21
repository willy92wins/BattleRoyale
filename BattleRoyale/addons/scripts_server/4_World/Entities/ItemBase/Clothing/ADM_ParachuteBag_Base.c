#ifdef SERVER
//! Server-side tuning helper for Admiral's Parachute bags so BattleRoyale enforces speed and
//! acceleration multipliers from general_settings.json without relying on an external JSON file.
modded class ADM_ParachuteBag_Base
{
	void BR_SetParachuteTuning(float fallSpeedMulti, float forwardSpeedMulti, float accelMulti)
	{
		bool changed = false;
		if (fallSpeedMulti > 0.0)
		{
			m_SpeedMultiY = fallSpeedMulti;
			changed = true;
		}
		if (forwardSpeedMulti > 0.0)
		{
			m_SpeedMultiZ = forwardSpeedMulti;
			changed = true;
		}
		if (accelMulti > 0.0)
		{
			m_AccelMultiY = accelMulti;
			m_AccelMultiZ = accelMulti;
			changed = true;
		}
		if (changed)
			SetSynchDirty();
	}
}
#endif

#ifndef SERVER
//! Keeps the player's view lined up with where its character aims while a firearm is raised.
//! Vanilla moves the view's up/down angle with the mouse on its own (UpdateUDAngleUnlocked) and only takes the
//! character's aim back after freelook or tracking. The character's aim is limited by its body and animations, the view
//! only by the look limits, so after a while they point at different heights: the player shoots where the view points,
//! while every other client, spectators included, shows the weapon aiming higher or lower. While a firearm is raised, a
//! gap above BR_AIM_SYNC_START degrees is closed smoothly; freelook, tracking and lowered or empty hands are left as they
//! are, and vertical mouse input or a weapon lifted against an obstacle pause a correction
modded class DayZPlayerCameraBase
{
	//! Degrees between the view and the character's aim that start a correction, and that end it
	protected static const float BR_AIM_SYNC_START = 0.5;
	protected static const float BR_AIM_SYNC_STOP = 0.05;
	//! Smoothing time of the correction, in seconds
	protected static const float BR_AIM_SYNC_TIME = 0.1;

	protected bool m_BR_AimSyncing;
	protected float m_BR_AimSyncVelocity[1];

	//! True while a correction is under way
	bool BR_IsAimSyncing()
	{
		return m_BR_AimSyncing;
	}

	//! Vanilla hands the angles over to the next camera (GetBaseAngles); a correction under way carries on there too
	override void OnActivate(DayZPlayerCamera pPrevCamera, DayZPlayerCameraResult pPrevCameraResult)
	{
		super.OnActivate(pPrevCamera, pPrevCameraResult);

		DayZPlayerCameraBase previous = DayZPlayerCameraBase.Cast(pPrevCamera);
		m_BR_AimSyncing = previous && previous.BR_IsAimSyncing();
		m_BR_AimSyncVelocity[0] = 0;
	}

	override float UpdateUDAngleUnlocked(out float pAngle, out float pAngleAdd, float pMin, float pMax, float pDt)
	{
		float angle = super.UpdateUDAngleUnlocked(pAngle, pAngleAdd, pMin, pMax, pDt);
		if (!BR_IsAimSyncAllowed(pAngleAdd))
		{
			m_BR_AimSyncing = false;
			m_BR_AimSyncVelocity[0] = 0;
			return angle;
		}

		// Only in a frame where the mouse does not move the aim up or down: the view takes that move at once and the
		// character in its own update, so a gap measured meanwhile would hold the view back. A weapon lifted against an
		// obstacle is left to vanilla too. Both only pause a correction under way
		DayZPlayerImplement player = DayZPlayerImplement.Cast(m_pPlayer);
		if (m_pInput.GetAimDelta(pDt)[1] != 0 || player.IsLiftWeapon())
		{
			m_BR_AimSyncVelocity[0] = 0;
			return angle;
		}

		float target = m_CommandWeapons.GetBaseAimingAngleUD();
		if (!m_BR_AimSyncing)
		{
			if (Math.AbsFloat(pAngle - target) <= BR_AIM_SYNC_START)
				return angle;

			m_BR_AimSyncing = true;
			m_BR_AimSyncVelocity[0] = 0;
		}

		pAngle = Math.SmoothCD(pAngle, target, m_BR_AimSyncVelocity, BR_AIM_SYNC_TIME, 1000, pDt);
		if (Math.AbsFloat(pAngle - target) < BR_AIM_SYNC_STOP)
		{
			pAngle = target;
			m_BR_AimSyncing = false;
			m_BR_AimSyncVelocity[0] = 0;
		}

		// Within the look limits, as vanilla keeps the view
		float lookDown, lookUp, lookLeft, lookRight;
		m_pPlayer.GetLookLimits(lookDown, lookUp, lookLeft, lookRight);
		pAngle = Limit(pAngle, lookDown, lookUp);
		return pAngle + pAngleAdd;
	}

	//! A raised firearm, no freelook, no tracking and no freelook angle still returning (vanilla takes the aim back then)
	protected bool BR_IsAimSyncAllowed(float pAngleAdd)
	{
		if (!m_CommandWeapons || !m_pInput || Math.AbsFloat(pAngleAdd) > 0.001)
			return false;

		if (m_pInput.CameraIsFreeLook() || m_pInput.CameraIsTracking())
			return false;

		DayZPlayerImplement player = DayZPlayerImplement.Cast(m_pPlayer);
		if (!player || !player.IsAlive() || !player.IsRaised())
			return false;

		return Weapon_Base.Cast(player.GetHumanInventory().GetEntityInHands()) != null;
	}
}
#endif

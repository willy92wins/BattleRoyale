//! Measured tilt constants and exponential smoothing helper for parachute flight.
class BattleRoyaleParachuteTilt
{
	static const float MAX_PITCH_DEG = 25.0;         //!< nose down with W
	static const float MAX_SPRINT_PITCH_DEG = 35.0;  //!< nose down with Shift+W
	static const float MAX_ROLL_DEG = 20.0;
	static const float MAX_ROLL_INPUT_DEG = 12.0;
	//! Roll gain per deg/s of yaw rate (~20 deg at 90 deg/s).
	static const float ROLL_PER_YAW_RATE = 0.2222222;
	static const float TILT_TAU_SEC = 0.35;

	static float Smooth(float cur, float target, float dt, float tau)
	{
		if (dt <= 0.0)
			return cur;
		if (tau <= 0.001)
			return target;
		float factor = 1.0 - Math.Pow(Math.EULER, -dt / tau);
		return cur + ((target - cur) * factor);
	}
}

//! Plane-deployment descent targets, set before Admiral's SetJSONMultiplier scales them: Y by
//! parachute_fall_speed_multiplier, Z by parachute_forward_speed_multiplier. The m/s in the
//! comments are for the defaults (16 and 8).
class BattleRoyaleParachuteTuning
{
	static const float NEUTRAL_Y = -0.5;        //!< 8 m/s down, as Admiral's base
	static const float DIVE_Y = -1.875;         //!< W: 30 m/s down
	static const float DIVE_Z = 1.25;           //!< W: 10 m/s forward
	static const float SPRINT_DIVE_Y = -3.125;  //!< Shift+W: 50 m/s down (Admiral's clamp)
	static const float SPRINT_DIVE_Z = 1.75;    //!< Shift+W: 14 m/s forward
	//! Vertical speed change per second is AccelMultiY times this (4 x 3 = 12 m/s each second)
	static const float VERTICAL_ACCEL_GAIN = 3.0;
	//! Landing damage with plane deployment: only a dive hurts (m/s down at touchdown)
	static const float DIVE_DAMAGE_FALL = 15.0;
	static const float HARD_DIVE_DAMAGE_FALL = 35.0;
}

//! BattleRoyale overrides for Admiral's parachute command:
//! - S key acts as an air brake (neutral descent, zero backward travel) instead of reversing.
//! - W dives (mostly down, some forward), Shift+W dives harder; vertical speed changes faster.
//! - The filtered velocity is handed to the player for the speedometer and dive landing damage.
//! - Smooth pitch (W/S) and roll (A/D + yaw rate) body tilt, leveled on landing or deactivation.
//! Inactive when legacy_spawn is 1 (all methods fall through to super).
modded class HumanCommandScript_Parachute
{
	protected float m_BRTiltPitch;
	protected float m_BRTiltRoll;
	protected float m_BRPrevYaw;
	protected bool m_BRHasYaw;
	protected bool m_BRTiltApplied;

	protected void BR_ApplyWorldTilt(float yaw, float pitch, float roll)
	{
		vector mat[3];
		float outRot[4];
		Math3D.YawPitchRollMatrix(Vector(yaw, pitch, roll), mat);
		Math3D.MatrixToQuat(mat, outRot);
		PostPhys_SetRotation(outRot);
	}

	override void ParachuteXYZ(vector Dir)
	{
		super.ParachuteXYZ(Dir);
		if (!m_pPlayer || !m_pPlayer.BR_IsPlaneDeploymentActive())
			return;
		if (!m_pPlayer.IsWearingChute())
			return;

		if (Dir[2] >= 0.0)
		{
			float diveY = BattleRoyaleParachuteTuning.DIVE_Y;
			float diveZ = BattleRoyaleParachuteTuning.DIVE_Z;
			if (speed == 3)
			{
				diveY = BattleRoyaleParachuteTuning.SPRINT_DIVE_Y;
				diveZ = BattleRoyaleParachuteTuning.SPRINT_DIVE_Z;
			}
			constantY = BattleRoyaleParachuteTuning.NEUTRAL_Y + (diveY - BattleRoyaleParachuteTuning.NEUTRAL_Y) * Dir[2];
			constantZ = diveZ * Dir[2];
			return;
		}

		// S brakes to the neutral descent, Shift included
		constantZ = 0.0;
		constantY = BattleRoyaleParachuteTuning.NEUTRAL_Y;
	}

	override void SetJSONMultiplier()
	{
		super.SetJSONMultiplier();
		if (m_pPlayer && m_pPlayer.BR_IsPlaneDeploymentActive() && m_pPlayer.IsWearingChute())
			constantAY = constantAY * BattleRoyaleParachuteTuning.VERTICAL_ACCEL_GAIN;
	}

	override void PrePhysUpdate(float pDt)
	{
		super.PrePhysUpdate(pDt);
		if (m_pPlayer && m_pPlayer.BR_IsPlaneDeploymentActive())
			m_pPlayer.BR_SetChuteVelocity(Vector(filterX, filterY, filterZ));
	}

	override void OnActivate()
	{
		super.OnActivate();
		m_BRTiltPitch = 0.0;
		m_BRTiltRoll = 0.0;
		m_BRPrevYaw = 0.0;
		m_BRHasYaw = false;
		m_BRTiltApplied = false;
	}

	override bool PostPhysUpdate(float pDt)
	{
		bool result = super.PostPhysUpdate(pDt);

		if (!m_pPlayer || !m_pPlayer.BR_IsPlaneDeploymentActive())
			return result;

		bool closed = !m_pPlayer.IsParachuteOpen();
		float curRot[4];
		vector curAng;

		if (!result || m_bNeedFinish || closed)
		{
			m_BRTiltPitch = 0.0;
			m_BRTiltRoll = 0.0;
			m_BRHasYaw = false;
			if (m_BRTiltApplied)
			{
				PostPhys_GetRotation(curRot);
				curAng = Math3D.QuatToAngles(curRot);
				BR_ApplyWorldTilt(curAng[0], 0.0, 0.0);
				m_BRTiltApplied = false;
			}
			return result;
		}

		PostPhys_GetRotation(curRot);
		curAng = Math3D.QuatToAngles(curRot);
		float yaw = curAng[0];

		float yawRate = 0.0;
		if (m_BRHasYaw && pDt > 0.0001)
		{
			float dYaw = Math.DiffAngle(yaw, m_BRPrevYaw);
			yawRate = dYaw / pDt;
		}
		m_BRPrevYaw = yaw;
		m_BRHasYaw = true;

		float inFwd = 0.0;
		float inRight = 0.0;
		if (speed > 0.001)
		{
			inFwd = locDir[2];
			inRight = locDir[0];
		}

		// W yields locDir[2] > 0; negative pitch in YawPitchRollMatrix pitches nose down.
		float maxPitch = BattleRoyaleParachuteTilt.MAX_PITCH_DEG;
		if (speed == 3)
			maxPitch = BattleRoyaleParachuteTilt.MAX_SPRINT_PITCH_DEG;
		float targetPitch = -inFwd * maxPitch;
		targetPitch = Math.Clamp(targetPitch, -maxPitch, maxPitch);

		// D yields locDir[0] > 0 and right yaw rotation yields yawRate > 0; positive roll banks right.
		float targetRoll = (inRight * BattleRoyaleParachuteTilt.MAX_ROLL_INPUT_DEG) + (yawRate * BattleRoyaleParachuteTilt.ROLL_PER_YAW_RATE);
		targetRoll = Math.Clamp(targetRoll, -BattleRoyaleParachuteTilt.MAX_ROLL_DEG, BattleRoyaleParachuteTilt.MAX_ROLL_DEG);

		m_BRTiltPitch = BattleRoyaleParachuteTilt.Smooth(m_BRTiltPitch, targetPitch, pDt, BattleRoyaleParachuteTilt.TILT_TAU_SEC);
		m_BRTiltRoll = BattleRoyaleParachuteTilt.Smooth(m_BRTiltRoll, targetRoll, pDt, BattleRoyaleParachuteTilt.TILT_TAU_SEC);

		BR_ApplyWorldTilt(yaw, m_BRTiltPitch, m_BRTiltRoll);
		m_BRTiltApplied = true;

		return result;
	}

	override void OnDeactivate()
	{
		super.OnDeactivate();
		if (m_pPlayer)
			m_pPlayer.BR_ClearChuteVelocity();
		bool wasApplied = m_BRTiltApplied;
		m_BRTiltPitch = 0.0;
		m_BRTiltRoll = 0.0;
		m_BRHasYaw = false;
		m_BRTiltApplied = false;
		if (wasApplied && m_pPlayer)
		{
			vector ori = m_pPlayer.GetOrientation();
			if (ori[1] != 0.0 || ori[2] != 0.0)
				m_pPlayer.SetOrientation(Vector(ori[0], 0.0, 0.0));
		}
	}
}

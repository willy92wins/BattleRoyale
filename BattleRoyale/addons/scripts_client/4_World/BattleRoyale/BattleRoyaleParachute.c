//! Measured tilt constants and exponential smoothing helper for parachute flight.
class BattleRoyaleParachuteTilt
{
	static const float MAX_PITCH_DEG = 15.0;
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

//! BattleRoyale overrides for Admiral's parachute command:
//! - S key acts as an air brake (neutral descent, zero backward travel) instead of reversing.
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
		if (Dir[2] >= 0.0)
			return;
		if (!m_pPlayer.IsWearingChute())
			return;

		float neutralY = -0.5;
		if (speed == 3)
			neutralY = -2.0;
		constantZ = 0.0;
		constantY = neutralY;
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
		float targetPitch = -inFwd * BattleRoyaleParachuteTilt.MAX_PITCH_DEG;
		targetPitch = Math.Clamp(targetPitch, -BattleRoyaleParachuteTilt.MAX_PITCH_DEG, BattleRoyaleParachuteTilt.MAX_PITCH_DEG);

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

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
	static const float NEUTRAL_Y = -0.315;      //!< 5.05 m/s down
	static const float DIVE_Y = -0.39375;       //!< W: 6.3 m/s down
	static const float DIVE_Z = 5.6;            //!< W: 44.8 m/s forward
	static const float SPRINT_DIVE_Y = -0.65625; //!< Shift+W: 10.5 m/s down
	static const float SPRINT_DIVE_Z = 9.6;     //!< Shift+W: 76.8 m/s forward
	//! Forward clamp in m/s, the same as Admiral's own
	static const float MAX_FORWARD_SPEED = 80.0;
	//! Vertical speed change per second is AccelMultiY times this (4 x 3 = 12 m/s each second)
	static const float VERTICAL_ACCEL_GAIN = 3.0;
	//! While W is held the dive builds up slower (4 x 2 = 8 m/s each second), which carries the glide further
	static const float DIVE_VERTICAL_ACCEL_GAIN = 2.0;
	//! Forward speed change per second is AccelMultiZ times this (4 x 2 = 8 m/s each second)
	static const float FORWARD_ACCEL_GAIN = 2.0;
	//! Forward speed bleeds off slower without S (4 x 1 = 4 m/s each second)
	static const float FORWARD_COAST_ACCEL_GAIN = 1.0;
	//! Looking down past this angle turns forward speed into descent
	static const float DIVE_PITCH_START_DEG = 10.0;
	//! Look-down angle past the start where the transfer stops growing
	static const float DIVE_PITCH_MAX_DEG = 60.0;
	//! Share of the current forward speed added to the descent, times the sine of the dive angle
	static const float DIVE_TRANSFER = 1.0;
	//! Landing damage with plane deployment: only a dive hurts (m/s down at touchdown)
	//! A W glide (6.3 m/s) and an idle descent (5.05 m/s) stay under these
	static const float DIVE_DAMAGE_FALL = 7.55;
	static const float HARD_DIVE_DAMAGE_FALL = 10.1;  //!< a full Shift+W dive lands at 10.5 m/s
}

//! BattleRoyale overrides for Admiral's parachute command:
//! - S key acts as an air brake (neutral descent, zero backward travel) instead of reversing.
//! - W glides forward with a slightly faster descent, Shift+W faster still; vertical speed changes faster.
//! - Looking down turns forward speed into descent; forward speed bleeds off slowly without S.
//! - The filtered velocity is handed to the player for the speedometer and dive landing damage.
//! - Smooth pitch (W/S) and roll (A/D + yaw rate) body tilt, leveled on landing or deactivation.
//! Inactive when ParachuteDeploy is 0 (all methods fall through to super).
modded class HumanCommandScript_Parachute
{
	protected float m_BRTiltPitch;
	protected float m_BRTiltRoll;
	protected float m_BRPrevYaw;
	protected bool m_BRHasYaw;
	protected bool m_BRTiltApplied;
	protected float m_BRDiveLogTimer;

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

	//! Base aiming angle in degrees, negative below the horizon; 0 without a weapons modifier
	protected float BR_GetLookPitch()
	{
		HumanCommandWeapons hcw = m_pPlayer.GetCommandModifier_Weapons();
		if (!hcw)
			return 0.0;
		return hcw.GetBaseAimingAngleUD();
	}

	//! Looking down rotates the target velocity: the forward target shrinks by the cosine of the
	//! dive angle and the current forward speed times its sine is added to the descent.
	protected void BR_ApplyDivePitch()
	{
		float dive = -BR_GetLookPitch() - BattleRoyaleParachuteTuning.DIVE_PITCH_START_DEG;
		if (dive <= 0.0)
			return;
		dive = Math.Min(dive, BattleRoyaleParachuteTuning.DIVE_PITCH_MAX_DEG);
		float rad = dive * Math.DEG2RAD;
		float forward = Math.Max(filterZ, 0.0);
		constantY = constantY - (forward * Math.Sin(rad) * BattleRoyaleParachuteTuning.DIVE_TRANSFER);
		constantZ = constantZ * Math.Cos(rad);
	}

	override void SetJSONMultiplier()
	{
		super.SetJSONMultiplier();
		if (m_pPlayer && m_pPlayer.BR_IsPlaneDeploymentActive() && m_pPlayer.IsWearingChute())
		{
			BR_ApplyDivePitch();

			float verticalGain = BattleRoyaleParachuteTuning.VERTICAL_ACCEL_GAIN;
			if (locDir[2] > 0.0)
				verticalGain = BattleRoyaleParachuteTuning.DIVE_VERTICAL_ACCEL_GAIN;
			constantAY = constantAY * verticalGain;

			// S keeps the quick brake; anything else slowing the glide coasts down
			float forwardGain = BattleRoyaleParachuteTuning.FORWARD_ACCEL_GAIN;
			if (locDir[2] >= 0.0 && constantZ < filterZ)
				forwardGain = BattleRoyaleParachuteTuning.FORWARD_COAST_ACCEL_GAIN;
			constantAZ = constantAZ * forwardGain;
		}
	}

	//! Same smoothing as Admiral's, with the forward clamp raised for the plane glide
	override void Filters(float pDt)
	{
		if (!m_pPlayer || !m_pPlayer.BR_IsPlaneDeploymentActive() || !m_pPlayer.IsWearingChute())
		{
			super.Filters(pDt);
			return;
		}

		filterX = Math.SmoothCD(filterX, constantX, filterXV, 0.3, constantAX, pDt);
		filterY = Math.SmoothCD(filterY, constantY, filterYV, 0.3, constantAY, pDt);
		filterZ = Math.SmoothCD(filterZ, constantZ, filterZV, 0.3, constantAZ, pDt);

		filterX = Math.Clamp(filterX, -80, 80);
		filterY = Math.Clamp(filterY, -50, 50);
		filterZ = Math.Clamp(filterZ, -BattleRoyaleParachuteTuning.MAX_FORWARD_SPEED, BattleRoyaleParachuteTuning.MAX_FORWARD_SPEED);
	}

	override void PrePhysUpdate(float pDt)
	{
		super.PrePhysUpdate(pDt);
		if (m_pPlayer && m_pPlayer.BR_IsPlaneDeploymentActive())
		{
			m_pPlayer.BR_SetChuteVelocity(Vector(filterX, filterY, filterZ));
			m_BRDiveLogTimer = m_BRDiveLogTimer + pDt;
			if (m_BRDiveLogTimer >= 1.0)
			{
				m_BRDiveLogTimer = 0.0;
				BattleRoyaleUtils.Trace("[Parachute] Glide pitch=" + BR_GetLookPitch().ToString() + " fwd=" + filterZ.ToString() + " vy=" + filterY.ToString());
			}
		}
	}

	override void OnActivate()
	{
		super.OnActivate();
		m_BRTiltPitch = 0.0;
		m_BRTiltRoll = 0.0;
		m_BRPrevYaw = 0.0;
		m_BRHasYaw = false;
		m_BRTiltApplied = false;
		m_BRDiveLogTimer = 0.0;
	}

	override bool PostPhysUpdate(float pDt)
	{
		bool result = super.PostPhysUpdate(pDt);

		if (!m_pPlayer || !m_pPlayer.BR_IsPlaneDeploymentActive())
			return result;

		bool closed = !m_pPlayer.IsParachuteOpen();
		float curRot[4];
		vector curAng;

		#ifndef SERVER
		// The server's landing ruins or removes the chute bag; Admiral's landing check needs a worn
		// chute, so a client that has not seen its own touchdown yet would keep the command running.
		if (result && !m_bNeedFinish && (closed || !m_pPlayer.IsWearingChute()))
			m_bNeedFinish = true;
		#endif

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

//! Admiral's handler starts a parachute command whenever the open flag is set and no script
//! command runs. The flag is cleared by the server, and on the client it stays set for a few
//! frames after the command ends on landing, so the client could start extra parachute
//! commands on the ground. A player who has landed, or no longer wears a device, never starts a new one.
modded class DayZPlayerImplementParachute
{
	override bool HandleParachute(float pDt, int pCurrentCommandID, bool pCurrentCommandFinished)
	{
		if (pCurrentCommandID != DayZPlayerConstants.COMMANDID_SCRIPT)
		{
			PlayerBase player = PlayerBase.Cast(dp_player);
			if (player && player.IsParachuteOpen())
			{
				if (player.PlayerHasLanded())
					return false;
				// The flag can outlive the chute bag on the client; without a worn device there is nothing to fly
				if (!player.IsWearingChute() && !player.IsWearingJetPack() && !player.IsWearingParaMotor())
					return false;
			}
		}
		return super.HandleParachute(pDt, pCurrentCommandID, pCurrentCommandFinished);
	}
}

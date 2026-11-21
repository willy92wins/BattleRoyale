//! Shared client-side drop plane pose and camera state in 4_World so the player camera
//! and the 5_Mission client controller evaluate the exact same 3D pose from a single
//! time function: start + (end - start) * ((t - t0) / duration).
class BattleRoyalePlaneState
{
	static const float MODEL_YAW = 180.0;

	//! Measured 1PP inside Land_Wreck_C130J_Cargo looking aft at the open cargo ramp.
	static const float CAM1_RIGHT = 0.0;
	static const float CAM1_UP = -3.4;
	static const float CAM1_FWD = 2.0;
	static const float CAM1_YAW = 180.0;
	static const float CAM1_PITCH = -8.0;

	//! Measured 3PP exterior view behind and above the C-130.
	static const float CAM3_RIGHT = 0.0;
	static const float CAM3_UP = 15.0;
	static const float CAM3_FWD = -60.0;
	static const float CAM3_YAW = 0.0;
	static const float CAM3_PITCH = -10.0;

	protected static bool s_Active;
	protected static bool s_HasFlight;
	protected static bool s_View3P;
	protected static vector s_Start;
	protected static vector s_End;
	protected static vector s_HorizDir;
	protected static float s_HeadingDeg;
	protected static float s_Duration;
	protected static int s_T0Ms;
	protected static bool s_Frozen;
	protected static float s_FrozenElapsed;
	protected static Object s_PlaneObj;

	static void Start(vector startPos, vector endPos, float durationSec, int t0Ms, Object planeObj)
	{
		s_Start = startPos;
		s_End = endPos;
		s_Duration = durationSec;
		s_T0Ms = t0Ms;
		s_Frozen = false;
		s_FrozenElapsed = 0.0;
		s_PlaneObj = planeObj;

		vector delta = endPos - startPos;
		vector horiz = Vector(delta[0], 0.0, delta[2]);
		float hLen = horiz.Length();
		if (hLen < 0.001)
			s_HorizDir = Vector(1.0, 0.0, 0.0);
		else
			s_HorizDir = horiz * (1.0 / hLen);

		vector ang = s_HorizDir.VectorToAngles();
		s_HeadingDeg = ang[0];

		s_View3P = false;
		s_HasFlight = true;
		s_Active = true;
	}

	static bool MatchesRoute(vector startPos, vector endPos, float durationSec)
	{
		if (vector.DistanceSq(s_Start, startPos) > 0.01)
			return false;
		if (vector.DistanceSq(s_End, endPos) > 0.01)
			return false;
		return Math.AbsFloat(s_Duration - durationSec) <= 0.01;
	}

	//! Freezes or unfreezes the client flight clock; elapsedSec >= 0 overrides the captured elapsed time.
	static void SetFrozen(bool frozen, float elapsedSec = -1.0)
	{
		if (frozen)
		{
			if (elapsedSec >= 0.0)
			{
				s_FrozenElapsed = elapsedSec;
			}
			else if (!s_Frozen)
			{
				float cur = 0.0;
				if (GetGame())
					cur = (GetGame().GetTime() - s_T0Ms) * 0.001;
				if (cur < 0.0)
					cur = 0.0;
				s_FrozenElapsed = cur;
			}
			s_Frozen = true;
			return;
		}

		if (GetGame())
		{
			if (elapsedSec >= 0.0)
				s_T0Ms = GetGame().GetTime() - Math.Round(elapsedSec * 1000.0);
			else if (s_Frozen)
				s_T0Ms = GetGame().GetTime() - Math.Round(s_FrozenElapsed * 1000.0);
		}
		s_Frozen = false;
	}

	//! Resynchronizes the client flight clock to a server-reported elapsed time and frozen state.
	static void SyncElapsed(float elapsedSec, bool frozen)
	{
		if (elapsedSec < 0.0)
			elapsedSec = 0.0;
		if (GetGame())
			s_T0Ms = GetGame().GetTime() - Math.Round(elapsedSec * 1000.0);
		s_FrozenElapsed = elapsedSec;
		s_Frozen = frozen;
	}

	static void LeavePlane()
	{
		s_Active = false;
		s_View3P = false;
	}

	static void StopAll()
	{
		s_Active = false;
		s_HasFlight = false;
		s_View3P = false;
		s_Frozen = false;
		s_FrozenElapsed = 0.0;
		s_PlaneObj = null;
	}

	static bool IsActive()
	{
		return s_Active;
	}

	//! Seconds elapsed since flight start on the current client clock (clamped >= 0).
	static float GetElapsedSec()
	{
		float elapsed = 0.0;
		if (s_Frozen)
			elapsed = s_FrozenElapsed;
		else if (GetGame())
			elapsed = (GetGame().GetTime() - s_T0Ms) * 0.001;
		if (elapsed < 0.0)
			elapsed = 0.0;
		return elapsed;
	}

	//! True while the flight path and plane marker should be drawn on the map and minimap:
	//! always while aboard, or while coasting before reaching the end of the flight line.
	static bool IsFlightVisible()
	{
		if (!s_HasFlight || s_Duration <= 0.001)
			return false;
		if (s_Active)
			return true;
		return GetElapsedSec() <= s_Duration;
	}

	//! Returns the flight start and end world positions while the flight is visible.
	static bool GetFlightRoute(out vector startPos, out vector endPos)
	{
		if (!IsFlightVisible())
			return false;
		startPos = s_Start;
		endPos = s_End;
		return true;
	}

	static bool IsView3P()
	{
		return s_View3P;
	}

	static bool ToggleView3P()
	{
		s_View3P = !s_View3P;
		return s_View3P;
	}

	//! Evaluates the continuous 3D pose (including altitude interpolation) at the current client clock.
	static void ComputePose(out vector pos, out vector horizDir, out float headingDeg)
	{
		float elapsed = GetElapsedSec();

		float frac = 0.0;
		if (s_Duration > 0.001)
			frac = elapsed / s_Duration;

		pos = s_Start + ((s_End - s_Start) * frac);
		horizDir = s_HorizDir;
		if (horizDir.Length() < 0.001)
			horizDir = Vector(1.0, 0.0, 0.0);
		headingDeg = s_HeadingDeg;
	}

	//! Moves the local plane object to the exact pose computed for this frame.
	static void SyncPlaneObject(vector pos, float headingDeg)
	{
		if (!s_PlaneObj)
			return;

		s_PlaneObj.SetPosition(pos);
		s_PlaneObj.SetOrientation(Vector(headingDeg + MODEL_YAW, 0.0, 0.0));
	}
}

//! Base player camera for the drop plane, positioned in world space via m_OwnerTM / m_bOwnerTMOverride
//! so it tracks the interpolated plane pose rather than the 1 Hz stepped player body.
class BattleRoyalePlaneCameraBase extends DayZPlayerCameraBase
{
	void BattleRoyalePlaneCameraBase(DayZPlayer pPlayer, HumanInputController pInput)
	{
	}

	override bool CanFreeLook()
	{
		return false;
	}

	protected void GetCamParams(out float right, out float up, out float fwd, out float yaw, out float pitch)
	{
		right = BattleRoyalePlaneState.CAM1_RIGHT;
		up = BattleRoyalePlaneState.CAM1_UP;
		fwd = BattleRoyalePlaneState.CAM1_FWD;
		yaw = BattleRoyalePlaneState.CAM1_YAW;
		pitch = BattleRoyalePlaneState.CAM1_PITCH;
	}

	protected void ApplyViewSpecific(inout DayZPlayerCameraResult pOutResult)
	{
		pOutResult.m_fInsideCamera = 1.0;
		pOutResult.m_fNearPlane = 0.04;
	}

	override void OnUpdate(float pDt, out DayZPlayerCameraResult pOutResult)
	{
		super.OnUpdate(pDt, pOutResult);

		vector planePos;
		vector planeDir;
		float headingDeg;
		BattleRoyalePlaneState.ComputePose(planePos, planeDir, headingDeg);
		BattleRoyalePlaneState.SyncPlaneObject(planePos, headingDeg);

		Math3D.DirectionAndUpMatrix(planeDir, "0 1 0", pOutResult.m_OwnerTM);
		pOutResult.m_OwnerTM[3] = planePos;
		pOutResult.m_bOwnerTMOverride = true;

		float camRight;
		float camUp;
		float camFwd;
		float camYaw;
		float camPitch;
		GetCamParams(camRight, camUp, camFwd, camYaw, camPitch);
		float clampedPitch = Math.Clamp(camPitch, -89.0, 89.0);
		vector lookDirLS = Vector(camYaw, clampedPitch, 0.0).AnglesToVector();
		Math3D.DirectionAndUpMatrix(lookDirLS, "0 1 0", pOutResult.m_CameraTM);
		pOutResult.m_CameraTM[3] = Vector(camRight, camUp, camFwd);

		m_CurrentCameraYaw = camYaw;
		m_CurrentCameraPitch = clampedPitch;
		m_CurrentCameraRoll = 0.0;

		pOutResult.m_fPositionModelSpace = 1.0;
		pOutResult.m_fUseHeading = 0.0;
		pOutResult.m_iDirectBone = -1;
		pOutResult.m_iDirectBoneMode = 0;
		pOutResult.m_fDistance = 0.0;
		pOutResult.m_fPredictCollisionRadius = 0.0;
		pOutResult.m_CollisionIgnoreEntity = m_pPlayer;
		pOutResult.m_fIgnoreParentRoll = 1.0;
		pOutResult.m_fIgnoreParentPitch = 1.0;
		pOutResult.m_fIgnoreParentYaw = 0.0;
		pOutResult.m_bUpdateEveryFrame = true;
		pOutResult.m_bUpdateWhenBlendOut = false;
		pOutResult.m_fShootFromCamera = 0.0;

		ApplyViewSpecific(pOutResult);
	}
}

class BattleRoyalePlaneCamera1P extends BattleRoyalePlaneCameraBase
{
	void BattleRoyalePlaneCamera1P(DayZPlayer pPlayer, HumanInputController pInput)
	{
	}

	protected override void GetCamParams(out float right, out float up, out float fwd, out float yaw, out float pitch)
	{
		right = BattleRoyalePlaneState.CAM1_RIGHT;
		up = BattleRoyalePlaneState.CAM1_UP;
		fwd = BattleRoyalePlaneState.CAM1_FWD;
		yaw = BattleRoyalePlaneState.CAM1_YAW;
		pitch = BattleRoyalePlaneState.CAM1_PITCH;
	}

	protected override void ApplyViewSpecific(inout DayZPlayerCameraResult pOutResult)
	{
		pOutResult.m_fInsideCamera = 1.0;
		pOutResult.m_fNearPlane = 0.04;
	}

	override string GetCameraName()
	{
		return "BattleRoyalePlaneCamera1P";
	}
}

class BattleRoyalePlaneCamera3P extends BattleRoyalePlaneCameraBase
{
	void BattleRoyalePlaneCamera3P(DayZPlayer pPlayer, HumanInputController pInput)
	{
	}

	protected override void GetCamParams(out float right, out float up, out float fwd, out float yaw, out float pitch)
	{
		right = BattleRoyalePlaneState.CAM3_RIGHT;
		up = BattleRoyalePlaneState.CAM3_UP;
		fwd = BattleRoyalePlaneState.CAM3_FWD;
		yaw = BattleRoyalePlaneState.CAM3_YAW;
		pitch = BattleRoyalePlaneState.CAM3_PITCH;
	}

	protected override void ApplyViewSpecific(inout DayZPlayerCameraResult pOutResult)
	{
		pOutResult.m_fInsideCamera = 0.0;
		pOutResult.m_fNearPlane = 0.1;
	}

	override string GetCameraName()
	{
		return "BattleRoyalePlaneCamera3P";
	}
}

//! Registers the drop plane 1P/3P cameras inside vanilla's unused ID gap 15..29
//! (vanilla uses 1..14 and 30..32 in dayzplayercameras.c).
//! IDs 26 and 27 avoid 15 (LFTurret), 17..18 (LFSkateboard), 18..19 (reserved/spike),
//! and high IDs (>= 120) which crash RegisterCameraCreator during DayZPlayerTypeCreate.
modded class DayZPlayerCameras
{
	static const int DAYZCAMERA_BR_PLANE_1P = 26;
	static const int DAYZCAMERA_BR_PLANE_3P = 27;

	override static void RegisterCameras(DayZPlayerType pType)
	{
		super.RegisterCameras(pType);
		pType.RegisterCameraCreator(DAYZCAMERA_BR_PLANE_1P, BattleRoyalePlaneCamera1P);
		pType.RegisterCameraCreator(DAYZCAMERA_BR_PLANE_3P, BattleRoyalePlaneCamera3P);
		RegisterTransitionTime(DAYZCAMERA_BR_PLANE_1P, DAYZCAMERA_BR_PLANE_3P, 0.0, true);
		RegisterTransitionTime(DAYZCAMERA_1ST, DAYZCAMERA_BR_PLANE_1P, 0.0, true);
		RegisterTransitionTime(DAYZCAMERA_1ST, DAYZCAMERA_BR_PLANE_3P, 0.0, true);
		RegisterTransitionTime(DAYZCAMERA_3RD_ERC, DAYZCAMERA_BR_PLANE_1P, 0.0, true);
		RegisterTransitionTime(DAYZCAMERA_3RD_ERC, DAYZCAMERA_BR_PLANE_3P, 0.0, true);
		RegisterTransitionTime(DAYZCAMERA_3RD_JUMP, DAYZCAMERA_BR_PLANE_1P, 0.0, true);
		RegisterTransitionTime(DAYZCAMERA_3RD_JUMP, DAYZCAMERA_BR_PLANE_3P, 0.0, true);
	}
}

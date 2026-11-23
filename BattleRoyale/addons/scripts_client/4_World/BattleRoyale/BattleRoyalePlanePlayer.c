//! PlayerBase state and overrides for plane deployment and parachute descent.
//! All plane/airlock behavior is gated by m_BRPlaneDeployment (synced from server when
//! ParachuteDeploy is 1); with plane deployment off, every override delegates directly to super.
modded class PlayerBase
{
	protected bool m_BRPlaneDeployment;
	protected bool m_BRPlaneFrozen;
	protected bool m_BRPlaneHidden;
	protected bool m_BRPlaneHiddenApplied;
	protected bool m_BRPlaneInRide;
	protected int m_BRPlaneJumpGen;
	protected int m_BRPlaneJumpSeen;
	protected bool m_BRPlaneJumpReady;
	protected bool m_BRPlaneRaiseLocked;
	protected bool m_BRPlaneFallPending;
	protected int m_BRPlaneFallTries;
	//! Server-only descent latches for safety auto-open (never net-synced).
	protected bool m_BRChuteWasOpen;
	protected bool m_BRAutoOpenSkipLogged;
	protected ref HumanMovementState m_BRPlaneMoveState;
	//! Filtered parachute velocity from the descent command (local, both sides, never net-synced)
	protected vector m_BRChuteVelocity;
	protected bool m_BRChuteVelocityValid;

	override void Init()
	{
		super.Init();
		m_BRPlaneMoveState = new HumanMovementState;
		RegisterNetSyncVariableBool("m_BRPlaneDeployment");
		RegisterNetSyncVariableBool("m_BRPlaneHidden");
		RegisterNetSyncVariableBool("m_BRPlaneInRide");
		RegisterNetSyncVariableInt("m_BRPlaneJumpGen");
	}

	void BR_SetPlaneDeploymentActive(bool active)
	{
		if (m_BRPlaneDeployment != active)
		{
			m_BRPlaneDeployment = active;
			SetSynchDirty();
		}
	}

	bool BR_IsPlaneDeploymentActive()
	{
		return m_BRPlaneDeployment;
	}

	void BR_SetChuteVelocity(vector velocity)
	{
		m_BRChuteVelocity = velocity;
		m_BRChuteVelocityValid = true;
	}

	void BR_ClearChuteVelocity()
	{
		m_BRChuteVelocity = vector.Zero;
		m_BRChuteVelocityValid = false;
	}

	//! The speedometer reads the command's filtered velocity: GetVelocity jumps because the command moves the body by translation
	override float PlayerCurrentSpeed()
	{
		if (m_BRPlaneDeployment && m_BRChuteVelocityValid)
			return m_BRChuteVelocity.Length() * 3.6;
		return super.PlayerCurrentSpeed();
	}

	//! With plane deployment only a dive hurts on landing; a neutral or braked descent lands safely
	override void PlayerLandDamage()
	{
		if (!m_BRPlaneDeployment || !m_BRChuteVelocityValid)
		{
			super.PlayerLandDamage();
			return;
		}

		if (!LandingSpeedDamage())
			return;

		float fall = -m_BRChuteVelocity[1];
		#ifdef SERVER
		if (fall >= BattleRoyaleParachuteTuning.HARD_DIVE_DAMAGE_FALL)
		{
			DecreaseHealth("", "Health", 90);
			DecreaseHealth("", "Shock", 90);
		}
		else if (fall >= BattleRoyaleParachuteTuning.DIVE_DAMAGE_FALL)
		{
			DecreaseHealth("", "Health", 50);
			DecreaseHealth("", "Shock", 50);
		}
		BattleRoyaleUtils.Trace("[Parachute] Landing fall=" + fall.ToString() + " m/s");
		#endif
	}

	void BR_SetPlaneInRide(bool inRide)
	{
		if (m_BRPlaneInRide != inRide)
		{
			m_BRPlaneInRide = inRide;
			SetSynchDirty();
		}
	}

	bool BR_IsPlaneInRide()
	{
		return m_BRPlaneInRide;
	}

	bool BR_IsPlaneFrozen()
	{
		return m_BRPlaneFrozen;
	}

	bool BR_IsPlaneHidden()
	{
		return m_BRPlaneHidden;
	}

	void BR_SetChuteWasOpen(bool wasOpen)
	{
		m_BRChuteWasOpen = wasOpen;
	}

	bool BR_WasChuteOpen()
	{
		return m_BRChuteWasOpen;
	}

	void BR_SetAutoOpenSkipLogged(bool logged)
	{
		m_BRAutoOpenSkipLogged = logged;
	}

	bool BR_WasAutoOpenSkipLogged()
	{
		return m_BRAutoOpenSkipLogged;
	}

	void BR_ResetAutoOpenDescent()
	{
		m_BRChuteWasOpen = false;
		m_BRAutoOpenSkipLogged = false;
	}

	//! Blocks Admiral's Space (IsJumpClimb) and low-FPS (<20) automatic cord pull when
	//! plane deployment is active; manual opening goes through the validated F RPC.
	override bool ChuteCordPulled()
	{
		if (m_BRPlaneDeployment)
			return false;
		return super.ChuteCordPulled();
	}

	//! Hides or unhides the player body and all attached clothing/gear on state transitions only,
	//! avoiding redundant false writes that would unhide clothes hidden by vanilla optics.
	void BR_SetPlaneHidden(bool hidden)
	{
		if (m_BRPlaneHidden != hidden)
		{
			m_BRPlaneHidden = hidden;
			SetSynchDirty();
		}
		BR_ApplyPlaneHidden();
	}

	void BR_ApplyPlaneHidden()
	{
		if (m_BRPlaneHidden == m_BRPlaneHiddenApplied)
			return;
		m_BRPlaneHiddenApplied = m_BRPlaneHidden;
		SetInvisible(m_BRPlaneHiddenApplied);
		SetInvisibleRecursive(m_BRPlaneHiddenApplied, this, null);
	}

	//! Monotonic counter bumped by the server on plane exit so the owning client applies
	//! downward fall velocity and starts COMMANDID_FALL cleanly.
	void BR_BumpPlaneJumpGen()
	{
		m_BRPlaneJumpGen = m_BRPlaneJumpGen + 1;
		SetSynchDirty();
	}

	void BR_ApplyPlaneJumpVelocity()
	{
		PhysicsEnableGravity(true);
		SetVelocity(this, Vector(0.0, -5.0, 0.0));
	}

	//! Freezes or unfreezes a boarded body on the server: invulnerable, gravity off, input disabled.
	void BR_SetPlaneFrozen(bool frozen)
	{
		m_BRPlaneFrozen = frozen;
		if (!frozen)
			BR_SetPlaneHidden(false);
		SetAllowDamage(!frozen);
		PhysicsEnableGravity(!frozen);
		HumanInputController hic = GetInputController();
		if (!hic)
			return;
		HumanInputControllerOverrideType mode = HumanInputControllerOverrideType.DISABLED;
		if (frozen)
			mode = HumanInputControllerOverrideType.ENABLED;
		hic.OverrideMovementSpeed(mode, 0);
		hic.OverrideMovementAngle(mode, 0);
		hic.OverrideRaise(mode, false);
		hic.SetDisabled(frozen);
	}

	//! Ground elevation using sea level (0.0) over ocean where SurfaceY returns the seabed.
	float BR_GroundY(float x, float z)
	{
		if (!GetGame())
			return 0.0;
		if (GetGame().SurfaceIsSea(x, z))
			return 0.0;
		return GetGame().SurfaceY(x, z);
	}

	float BR_AGL()
	{
		vector pos = GetPosition();
		return pos[1] - BR_GroundY(pos[0], pos[2]);
	}

	int BR_GetMovementCommandID()
	{
		if (!m_BRPlaneMoveState)
			m_BRPlaneMoveState = new HumanMovementState;
		GetMovementState(m_BRPlaneMoveState);
		return m_BRPlaneMoveState.m_CommandTypeId;
	}

	//! True while falling, under an open parachute above ground, or descending > 2 m AGL.
	//! Swimming and standing on the surface (AGL < 1.5 m with |vy| < 1 m/s) are not air.
	bool BR_InAir()
	{
		if (IsSwimming())
			return false;
		if (BR_GetMovementCommandID() == DayZPlayerConstants.COMMANDID_FALL)
			return true;

		vector pos = GetPosition();
		float agl = pos[1] - BR_GroundY(pos[0], pos[2]);
		vector vel = GetVelocity(this);
		if (agl < 1.5 && Math.AbsFloat(vel[1]) < 1.0)
			return false;
		if (IsParachuteOpen())
			return true;
		if (agl > 2.0 && vel[1] < -1.0)
			return true;
		return false;
	}

	//! True when plane deployment is active and the player is aboard the plane or airborne.
	bool BR_IsAirlocked()
	{
		if (!m_BRPlaneDeployment)
			return false;
		if (m_BRPlaneInRide)
			return true;
		return BR_InAir();
	}

	override bool CanManipulateInventory()
	{
		if (BR_IsAirlocked())
			return false;
		return super.CanManipulateInventory();
	}

	override bool CanReceiveItemIntoHands(EntityAI item_to_hands)
	{
		if (BR_IsAirlocked())
			return false;
		return super.CanReceiveItemIntoHands(item_to_hands);
	}

	override bool CanReleaseFromHands(EntityAI handheld)
	{
		if (BR_IsAirlocked())
			return false;
		return super.CanReleaseFromHands(handheld);
	}

	override bool CanReceiveItemIntoCargo(EntityAI item)
	{
		if (BR_IsAirlocked())
			return false;
		return super.CanReceiveItemIntoCargo(item);
	}

	override bool CanReleaseCargo(EntityAI cargo)
	{
		if (BR_IsAirlocked())
			return false;
		return super.CanReleaseCargo(cargo);
	}

	//! Keeps weapons lowered and ADS dropped while airlocked, and starts COMMANDID_FALL on
	//! the owning client right after jumping from the plane so client and server stay in sync.
	override void CommandHandler(float pDt, int pCurrentCommandID, bool pCurrentCommandFinished)
	{
		HumanInputController hic = GetInputController();
		if (hic)
		{
			if (BR_IsAirlocked())
			{
				hic.OverrideRaise(HumanInputControllerOverrideType.ENABLED, false);
				if (hic.WeaponADS())
					hic.ResetADS();
				m_BRPlaneRaiseLocked = true;
			}
			else if (m_BRPlaneRaiseLocked)
			{
				m_BRPlaneRaiseLocked = false;
				if (!m_BRPlaneFrozen)
					hic.OverrideRaise(HumanInputControllerOverrideType.DISABLED, false);
			}
		}

		super.CommandHandler(pDt, pCurrentCommandID, pCurrentCommandFinished);

		if (m_BRPlaneFallPending && GetGame() && GetGame().IsClient() && PlayerBase.Cast(GetGame().GetPlayer()) == this)
		{
			m_BRPlaneFallTries = m_BRPlaneFallTries + 1;
			int curCmd = GetCurrentCommandID();
			bool inFall = (curCmd == DayZPlayerConstants.COMMANDID_FALL) || (GetCommand_Fall() != null);
			if (inFall || m_BRPlaneFallTries > 30)
			{
				m_BRPlaneFallPending = false;
			}
			else if (curCmd == DayZPlayerConstants.COMMANDID_MOVE)
			{
				vector curPos = GetPosition();
				StartCommand_Fall(-5.0);
				SetFallYDiff(curPos[1]);
			}
		}
	}

	//! Selects our drop-plane camera on the owning client while aboard the plane.
	override int CameraHandler(int pCameraMode)
	{
		if (GetGame() && GetGame().IsClient() && BattleRoyalePlaneState.IsActive())
		{
			if (PlayerBase.Cast(GetGame().GetPlayer()) == this)
			{
				if (BattleRoyalePlaneState.IsView3P())
					return DayZPlayerCameras.DAYZCAMERA_BR_PLANE_3P;
				return DayZPlayerCameras.DAYZCAMERA_BR_PLANE_1P;
			}
		}
		return super.CameraHandler(pCameraMode);
	}

	override void OnVariablesSynchronized()
	{
		super.OnVariablesSynchronized();
		BR_ApplyPlaneHidden();
		if (!GetGame() || !GetGame().IsClient())
			return;
		if (PlayerBase.Cast(GetGame().GetPlayer()) != this)
			return;
		if (!m_BRPlaneJumpReady)
		{
			m_BRPlaneJumpReady = true;
			if (m_BRPlaneJumpGen == 0)
			{
				m_BRPlaneJumpSeen = 0;
				return;
			}
		}
		if (m_BRPlaneJumpGen == m_BRPlaneJumpSeen)
			return;
		m_BRPlaneJumpSeen = m_BRPlaneJumpGen;
		BR_ApplyPlaneJumpVelocity();
		m_BRPlaneFallPending = true;
		m_BRPlaneFallTries = 0;
	}
}

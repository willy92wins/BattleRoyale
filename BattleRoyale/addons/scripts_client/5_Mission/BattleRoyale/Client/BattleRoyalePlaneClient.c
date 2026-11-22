#ifndef SERVER
//! Client-side drop plane controller driven from BattleRoyaleClient.Update.
//! Spawns and moves the local Land_Wreck_C130J_Cargo mesh every frame, handles V (1P/3P)
//! and UADayZBRJump (F by default: jump from plane / open parachute in free fall),
//! and requests a phase resync if the server reports the player aboard without a local plane.
class BattleRoyalePlaneClient
{
	static const string PLANE_MODEL_PRIMARY = "Land_Wreck_C130J_Cargo";
	static const float PLANE_COAST_SECONDS = 15.0;
	static const int PARACHUTE_OPEN_DELAY_MS = 1000;
	static const float RESYNC_COOLDOWN_SECONDS = 2.0;

	protected Object m_PlaneObj;
	protected bool m_PlaneActive;
	protected bool m_PlaneCoasting;
	protected float m_PlaneCoastRem;
	protected bool m_LocalExitedRide;
	protected int m_JumpTimeMs;
	protected bool m_DeferredOpenReq;
	protected float m_ResyncCooldown;
	protected int m_LastFlightSerial;
	protected bool m_LastFrozen;

	void BattleRoyalePlaneClient()
	{
		BattleRoyaleRPC br_rpc = BattleRoyaleRPC.GetInstance();
		if (br_rpc)
			m_LastFlightSerial = br_rpc.plane_flight_serial;
	}

	void ~BattleRoyalePlaneClient()
	{
		Cleanup();
	}

	void SyncSerialFromRPC(BattleRoyaleRPC br_rpc)
	{
		if (br_rpc)
			m_LastFlightSerial = br_rpc.plane_flight_serial;
	}

	bool IsPlayerInPlane(PlayerBase player)
	{
		if (m_PlaneActive)
			return true;
		if (player && player.BR_IsPlaneInRide() && !m_LocalExitedRide)
			return true;
		return false;
	}

	//! True while the local drop plane is active and its interpolated position lies over the sea.
	bool IsPlaneOverSea()
	{
		if (!GetGame() || !m_PlaneActive)
			return false;
		vector planePos;
		vector planeDir;
		float headingDeg;
		BattleRoyalePlaneState.ComputePose(planePos, planeDir, headingDeg);
		return GetGame().SurfaceIsSea(planePos[0], planePos[2]);
	}

	bool IsRecentlyJumped()
	{
		if (m_JumpTimeMs <= 0 || !GetGame())
			return false;
		return (GetGame().GetTime() - m_JumpTimeMs) < PARACHUTE_OPEN_DELAY_MS;
	}

	void Cleanup()
	{
		DeletePlane();
		m_LocalExitedRide = false;
		m_JumpTimeMs = 0;
		m_ResyncCooldown = 0.0;
		m_LastFrozen = false;
	}

	protected void DeletePlane()
	{
		m_PlaneActive = false;
		m_PlaneCoasting = false;
		m_PlaneCoastRem = 0.0;
		m_DeferredOpenReq = false;
		m_LastFrozen = false;
		BattleRoyalePlaneState.StopAll();
		if (m_PlaneObj)
		{
			if (GetGame())
				GetGame().ObjectDelete(m_PlaneObj);
			m_PlaneObj = null;
		}
	}

	protected Object CreatePlaneObject(vector pos)
	{
		if (!GetGame())
			return null;
		Object obj = GetGame().CreateObject(PLANE_MODEL_PRIMARY, pos, true, false, false);
		if (obj)
			return obj;
		BattleRoyaleUtils.Warn("[Plane] Failed to create local plane object " + PLANE_MODEL_PRIMARY);
		return null;
	}

	protected void UpdatePlanePose()
	{
		if (!GetGame() || !m_PlaneObj)
			return;
		vector planePos;
		vector planeDir;
		float headingDeg;
		BattleRoyalePlaneState.ComputePose(planePos, planeDir, headingDeg);
		BattleRoyalePlaneState.SyncPlaneObject(planePos, headingDeg);
	}

	protected void OnStartPlaneFlight(vector startPos, vector endPos, float durationSec, float elapsedSec, bool isFrozen)
	{
		if (!GetGame())
			return;
		if (durationSec <= 0.001)
		{
			BattleRoyaleUtils.Warn("[Plane] Ignored StartPlaneFlight with non-positive duration");
			return;
		}
		if (elapsedSec < 0.0)
			elapsedSec = 0.0;

		if (m_PlaneActive)
		{
			if (BattleRoyalePlaneState.MatchesRoute(startPos, endPos, durationSec))
			{
				BattleRoyalePlaneState.SyncElapsed(elapsedSec, isFrozen);
				m_LastFrozen = isFrozen;
				UpdatePlanePose();
				if (BattleRoyaleUtils.CheckLogLevel(BattleRoyaleUtils.DEBUG))
					BattleRoyaleUtils.Debug(string.Format("[Plane] Resynced flight clock on client: elapsed=%1 s frozen=%2", elapsedSec, isFrozen));
				return;
			}
			BattleRoyaleUtils.Debug("[Plane] Ignored duplicate StartPlaneFlight while already active");
			return;
		}

		DeletePlane();
		m_LocalExitedRide = false;
		m_DeferredOpenReq = false;

		int elapsedMs = Math.Round(elapsedSec * 1000.0);
		int t0Ms = GetGame().GetTime() - elapsedMs;
		float frac = elapsedSec / durationSec;
		vector initialPos = startPos + ((endPos - startPos) * frac);

		m_PlaneObj = CreatePlaneObject(initialPos);
		if (!m_PlaneObj)
			return;

		BattleRoyalePlaneState.Start(startPos, endPos, durationSec, t0Ms, m_PlaneObj);
		if (isFrozen)
			BattleRoyalePlaneState.SetFrozen(true, elapsedSec);
		m_LastFrozen = isFrozen;
		m_PlaneActive = true;
		m_PlaneCoasting = false;
		UpdatePlanePose();

		if (BattleRoyaleUtils.CheckLogLevel(BattleRoyaleUtils.INFO))
			BattleRoyaleUtils.Info(string.Format("[Plane] Flight started on client: duration=%1 s elapsed=%2 s", durationSec, elapsedSec));
	}

	protected void OnEndPlaneFlight(bool keepCoasting)
	{
		bool hadPlane = m_PlaneActive;
		m_PlaneActive = false;
		m_LocalExitedRide = true;
		m_DeferredOpenReq = false;
		BattleRoyalePlaneState.LeavePlane();
		if (GetGame())
			m_JumpTimeMs = GetGame().GetTime();

		if (keepCoasting && hadPlane && m_PlaneObj)
		{
			m_PlaneCoasting = true;
			m_PlaneCoastRem = PLANE_COAST_SECONDS;
		}
		else
		{
			DeletePlane();
		}

		if (BattleRoyaleUtils.CheckLogLevel(BattleRoyaleUtils.DEBUG))
			BattleRoyaleUtils.Debug(string.Format("[Plane] Flight ended on client: coast=%1", keepCoasting));
	}

	protected void TickPlane(float dt)
	{
		if (!m_PlaneObj || !GetGame())
		{
			DeletePlane();
			return;
		}
		UpdatePlanePose();
		if (!m_PlaneActive && m_PlaneCoasting && !m_LastFrozen)
		{
			m_PlaneCoastRem = m_PlaneCoastRem - dt;
			if (m_PlaneCoastRem <= 0.0)
				DeletePlane();
		}
	}

	protected void TickInput(PlayerBase player, bool isFrozen)
	{
		if (!GetGame() || !player)
			return;

		bool inServerRide = m_PlaneActive;
		if (player.BR_IsPlaneInRide() && !m_LocalExitedRide)
			inServerRide = true;

		// Flush a deferred parachute open request once 1 s has elapsed after jumping.
		if (m_DeferredOpenReq)
		{
			if (inServerRide || player.IsParachuteOpen() || (!player.BR_InAir() && player.BR_AGL() < 1.5))
			{
				m_DeferredOpenReq = false;
			}
			else if (m_JumpTimeMs == 0 || (GetGame().GetTime() - m_JumpTimeMs) >= PARACHUTE_OPEN_DELAY_MS)
			{
				m_DeferredOpenReq = false;
				GetRPCManager().SendRPC(RPC_DAYZBRSERVER_NAMESPACE, "RequestOpenParachute", NULL, true, NULL, player);
				BattleRoyaleUtils.Debug("[Plane] Sent deferred RequestOpenParachute");
			}
		}

		if (!inServerRide && !player.BR_IsPlaneDeploymentActive())
			return;

		UIManager ui = GetGame().GetUIManager();
		if (ui && (ui.GetMenu() || ui.IsDialogVisible()))
			return;

		UAInputAPI uapi = GetUApi();
		if (!uapi)
			return;

		if (m_PlaneActive)
		{
			UAInput viewInput = uapi.GetInputByName("UAPersonView");
			if (viewInput && viewInput.LocalPress())
			{
				bool is3p = BattleRoyalePlaneState.ToggleView3P();
				if (BattleRoyaleUtils.CheckLogLevel(BattleRoyaleUtils.DEBUG))
				{
					string viewTag = "1P";
					if (is3p)
						viewTag = "3P";
					BattleRoyaleUtils.Debug("[Plane] Camera view toggled to " + viewTag);
				}
			}
		}

		UAInput jumpInput = uapi.GetInputByName("UADayZBRJump");
		if (!jumpInput || !jumpInput.LocalPress())
			return;

		if (inServerRide)
		{
			if (isFrozen)
				return;
			GetRPCManager().SendRPC(RPC_DAYZBRSERVER_NAMESPACE, "RequestPlaneJump", NULL, true, NULL, player);
			BattleRoyaleUtils.Debug("[Plane] Sent RequestPlaneJump");
			return;
		}

		if (!player.IsParachuteOpen())
		{
			int nowMs = GetGame().GetTime();
			bool fallingNow = player.BR_InAir();
			if (!fallingNow && m_JumpTimeMs > 0 && (nowMs - m_JumpTimeMs) < PARACHUTE_OPEN_DELAY_MS && player.BR_AGL() > 5.0)
				fallingNow = true;
			if (fallingNow)
			{
				if (m_JumpTimeMs == 0 || (nowMs - m_JumpTimeMs) >= PARACHUTE_OPEN_DELAY_MS)
				{
					m_DeferredOpenReq = false;
					GetRPCManager().SendRPC(RPC_DAYZBRSERVER_NAMESPACE, "RequestOpenParachute", NULL, true, NULL, player);
					BattleRoyaleUtils.Debug("[Plane] Sent RequestOpenParachute");
				}
				else if (!m_DeferredOpenReq)
				{
					m_DeferredOpenReq = true;
					BattleRoyaleUtils.Debug("[Plane] Deferred RequestOpenParachute (<1s after jump)");
				}
			}
		}
	}

	void Update(float dt, BattleRoyaleRPC br_rpc)
	{
		if (!GetGame())
			return;

		if (m_ResyncCooldown > 0.0)
			m_ResyncCooldown = m_ResyncCooldown - dt;

		bool isFrozen = false;
		if (br_rpc)
			isFrozen = br_rpc.match_frozen;

		if (br_rpc && m_LastFlightSerial != br_rpc.plane_flight_serial)
		{
			m_LastFlightSerial = br_rpc.plane_flight_serial;
			if (br_rpc.plane_flight_active)
				OnStartPlaneFlight(br_rpc.plane_flight_start, br_rpc.plane_flight_end, br_rpc.plane_flight_duration, br_rpc.plane_flight_elapsed, isFrozen);
			else
				OnEndPlaneFlight(br_rpc.plane_flight_end_coast);
		}

		if (isFrozen != m_LastFrozen)
		{
			m_LastFrozen = isFrozen;
			if (m_PlaneActive || m_PlaneCoasting)
			{
				BattleRoyalePlaneState.SetFrozen(isFrozen, -1.0);
				UpdatePlanePose();
			}
		}

		PlayerBase player = PlayerBase.Cast(GetGame().GetPlayer());
		if (player)
		{
			if (player.BR_IsPlaneInRide())
			{
				if (!m_PlaneActive && !m_LocalExitedRide && m_ResyncCooldown <= 0.0)
				{
					m_ResyncCooldown = RESYNC_COOLDOWN_SECONDS;
					GetRPCManager().SendRPC(RPC_DAYZBRSERVER_NAMESPACE, "RequestPlaneResync", NULL, true, NULL, player);
					BattleRoyaleUtils.Debug("[Plane] Sent RequestPlaneResync");
				}
			}
			else if (m_LocalExitedRide)
			{
				m_LocalExitedRide = false;
			}
		}

		if (m_PlaneActive || m_PlaneCoasting)
			TickPlane(dt);

		TickInput(player, isFrozen);
	}
}
#endif

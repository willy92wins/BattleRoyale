#ifdef SERVER
//! Server-authoritative drop plane flight, 1 Hz rider transport, jump/ejection,
//! parachute open validation, safety auto-open, and airborne landing tracker.
class BattleRoyalePlaneFlight
{
	static const string PARACHUTE_BAG_CLASS = "ADM_ParachuteBag_Fabric";
	static const float RIDE_STEP_HZ = 1.0;
	static const float EXIT_BACK_M = 25.0;
	static const float EXIT_DOWN_M = 6.0;
	static const float EXIT_LATERAL_JITTER_M = 15.0;
	static const float EJECT_FAN_STEP_M = 6.0;
	static const int EJECT_FAN_ROW_WIDTH = 7;
	static const float AUTO_OPEN_MIN_AGL_M = 5.0;
	static const float AUTO_OPEN_MIN_FALL_VY = -5.0;
	static const float LANDING_AGL_M = 1.5;
	static const float LANDING_MAX_ABS_VY = 1.0;

	protected bool m_FlightActive;
	protected bool m_FlightFinished;
	protected int m_FlightFinishedMs;
	protected vector m_Start;
	protected vector m_End;
	protected vector m_HorizDir;
	protected vector m_RightDir;
	protected float m_Duration;
	protected int m_StartMs;
	protected float m_RideAcc;
	protected float m_AutoOpenAlt;
	protected bool m_Paused;
	protected float m_PausedElapsedSec;

	protected ref array<PlayerBase> m_Riders;
	protected ref array<PlayerBase> m_Airborne;
	protected ref array<PlayerBase> m_AirborneSeen;
	//! COT anti-cheat movement grace for riders and airborne players, renewed every second
	static const float ANTICHEAT_GRACE_SEC = 5.0;
	protected float m_AntiCheatGraceTimer;

	void BattleRoyalePlaneFlight()
	{
		m_Riders = new array<PlayerBase>;
		m_Airborne = new array<PlayerBase>;
		m_AirborneSeen = new array<PlayerBase>;
		m_AutoOpenAlt = 150.0;
	}

	//! Empty string if player wears an intact Admiral's parachute bag in Back; otherwise a refusal reason.
	static string GetOpenRefusal(PlayerBase player)
	{
		if (!player)
			return "no_player";
		EntityAI back = player.FindAttachmentBySlotName("Back");
		if (!back)
			return "no_bag";
		ADM_ParachuteBag_Base bag = ADM_ParachuteBag_Base.Cast(back);
		if (!bag || !bag.IsParachute())
			return "not_parachute";
		if (bag.IsRuined())
			return "ruined";
		return "";
	}

	static bool HasValidParachuteBag(PlayerBase player)
	{
		return GetOpenRefusal(player) == "";
	}

	//! Applies BattleRoyale's configured parachute speed and acceleration multipliers to a bag.
	static void ApplyParachuteTuning(ADM_ParachuteBag_Base bag, BattleRoyaleGameData gameData)
	{
		if (!bag || !gameData || gameData.ParachuteDeploy == 0)
			return;
		if (gameData.parachute_fall_speed_multiplier <= 0.0 && gameData.parachute_forward_speed_multiplier <= 0.0 && gameData.parachute_accel_multiplier <= 0.0)
			return;
		bag.BR_SetParachuteTuning(gameData.parachute_fall_speed_multiplier, gameData.parachute_forward_speed_multiplier, gameData.parachute_accel_multiplier);
	}

	//! Validates and serves C->S RequestOpenParachute for any airborne player with a valid bag.
	static bool HandleOpenParachuteRequest(PlayerBase player)
	{
		if (!player || !player.IsAlive())
			return false;
		if (player.IsParachuteOpen())
		{
			BattleRoyaleUtils.Debug("[Plane] Open refused (already_open) for " + player.BR_GetPlayerName());
			return false;
		}
		if (player.BR_IsPlaneFrozen() || player.BR_IsPlaneInRide())
		{
			BattleRoyaleUtils.Debug("[Plane] Open refused (in_plane) for " + player.BR_GetPlayerName());
			return false;
		}
		if (!player.BR_InAir())
		{
			BattleRoyaleUtils.Debug("[Plane] Open refused (not_in_air) for " + player.BR_GetPlayerName());
			return false;
		}
		string refusal = GetOpenRefusal(player);
		if (refusal != "")
		{
			BattleRoyaleUtils.Debug("[Plane] Open refused (" + refusal + ") for " + player.BR_GetPlayerName());
			return false;
		}

		player.SetParachuteOpen(true);
		player.BR_SetChuteWasOpen(true);
		int aglM = Math.Round(player.BR_AGL());
		BattleRoyaleUtils.Info(string.Format("[Plane] Parachute opened (manual): player=%1 agl=%2 m", player.BR_GetPlayerName(), aglM));
		return true;
	}

	protected void RestoreBRInput(PlayerBase player)
	{
		if (!player)
			return;
		player.DisableInput(false);
		PlayerIdentity pid = player.GetIdentity();
		if (pid)
			GetRPCManager().SendRPC(RPC_DAYZBR_NAMESPACE, "SetInput", new Param1<bool>(false), true, pid);
	}

	protected void SendStartPlaneFlight(PlayerBase player, float elapsedSec)
	{
		if (!player)
			return;
		PlayerIdentity pid = player.GetIdentity();
		if (!pid)
			return;
		GetRPCManager().SendRPC(RPC_DAYZBR_NAMESPACE, "StartPlaneFlight", new Param4<vector, vector, float, float>(m_Start, m_End, m_Duration, elapsedSec), true, pid);
	}

	protected void SendEndPlaneFlight(PlayerBase player, bool keepCoasting)
	{
		if (!player)
			return;
		PlayerIdentity pid = player.GetIdentity();
		if (!pid)
			return;
		GetRPCManager().SendRPC(RPC_DAYZBR_NAMESPACE, "EndPlaneFlight", new Param1<bool>(keepCoasting), true, pid);
	}

	void StartFlight(BattleRoyaleFlightPath path, float autoOpenAltM, array<PlayerBase> matchPlayers)
	{
		m_Riders.Clear();
		m_Airborne.Clear();
		m_AirborneSeen.Clear();
		m_FlightActive = false;
		m_FlightFinished = false;
		m_FlightFinishedMs = 0;
		m_RideAcc = 0.0;
		m_AutoOpenAlt = autoOpenAltM;
		m_Paused = false;
		m_PausedElapsedSec = 0.0;

		if (!path || path.m_DurationS <= 0.001)
		{
			BattleRoyaleUtils.Warn("[Plane] StartFlight called with invalid flight path");
			MarkFlightFinished("invalid_path");
			return;
		}

		m_Start = path.m_Start;
		m_End = path.m_End;
		m_Duration = path.m_DurationS;

		vector delta = m_End - m_Start;
		vector horiz = Vector(delta[0], 0.0, delta[2]);
		float hLen = horiz.Length();
		if (hLen < 0.001)
			m_HorizDir = Vector(1.0, 0.0, 0.0);
		else
			m_HorizDir = horiz * (1.0 / hLen);
		m_RightDir = Vector(m_HorizDir[2], 0.0, -m_HorizDir[0]);

		m_StartMs = 0;
		if (GetGame())
			m_StartMs = GetGame().GetTime();

		BattleRoyaleGameData gameData = BattleRoyaleConfig.GetConfig().GetGameData();
		int skipped = 0;
		if (matchPlayers)
		{
			for (int i = 0; i < matchPlayers.Count(); i++)
			{
				PlayerBase p = matchPlayers.Get(i);
				if (!p || !p.IsAlive())
					continue;
				p.BR_SetPlaneDeploymentActive(true);
				if (!HasValidParachuteBag(p))
				{
					skipped = skipped + 1;
					BattleRoyaleUtils.Warn("[Plane] Skipped boarding " + p.BR_GetPlayerName() + " reason=" + GetOpenRefusal(p));
					p.SetAllowDamage(true);
					RestoreBRInput(p);
					continue;
				}
				ADM_ParachuteBag_Base bag = ADM_ParachuteBag_Base.Cast(p.FindAttachmentBySlotName("Back"));
				if (bag)
					ApplyParachuteTuning(bag, gameData);
				if (m_Riders.Find(p) != -1)
					continue;
				m_Riders.Insert(p);
				p.BR_SetPlaneInRide(true);
				p.BR_ResetAutoOpenDescent();
				p.BR_SetPlaneFrozen(true);
				p.BR_SetPlaneHidden(true);
				SetVelocity(p, vector.Zero);
				GrantAntiCheatMovement(p);
				p.SetPosition(m_Start);
				SendStartPlaneFlight(p, 0.0);
			}
		}

		m_FlightActive = m_Riders.Count() > 0;
		BattleRoyaleUtils.Info(string.Format("[Plane] Boarded %1 players (skipped=%2) start=%3 end=%4 duration=%5 s", m_Riders.Count(), skipped, m_Start.ToString(), m_End.ToString(), Math.Round(m_Duration)));
		if (!m_FlightActive)
			MarkFlightFinished("no_riders");
	}

	bool HasActiveFlight()
	{
		return m_FlightActive && m_Riders && m_Riders.Count() > 0;
	}

	bool HasTrackedPlayers()
	{
		if (m_FlightActive && m_Riders && m_Riders.Count() > 0)
			return true;
		if (m_Airborne && m_Airborne.Count() > 0)
			return true;
		return false;
	}

	int GetActiveAirborneCount()
	{
		if (!m_Airborne)
			return 0;
		int count = 0;
		for (int i = 0; i < m_Airborne.Count(); i++)
		{
			PlayerBase a = m_Airborne.Get(i);
			if (a && a.IsAlive() && a.GetIdentity())
				count = count + 1;
		}
		return count;
	}

	bool IsFlightFinished()
	{
		return m_FlightFinished;
	}

	float GetSecondsSinceFlightFinished()
	{
		if (!m_FlightFinished || !GetGame())
			return -1.0;
		return (GetGame().GetTime() - m_FlightFinishedMs) * 0.001;
	}

	float GetElapsedSec()
	{
		if (m_Paused)
			return m_PausedElapsedSec;
		float elapsed = 0.0;
		if (GetGame())
			elapsed = (GetGame().GetTime() - m_StartMs) * 0.001;
		if (elapsed < 0.0)
			elapsed = 0.0;
		if (elapsed > m_Duration)
			elapsed = m_Duration;
		return elapsed;
	}

	float GetRemainingFlightSeconds()
	{
		if (!m_FlightActive)
			return 0.0;
		float rem = m_Duration - GetElapsedSec();
		if (rem < 0.0)
			rem = 0.0;
		return rem;
	}

	vector GetPlanePositionAt(float elapsedSec)
	{
		float clamped = elapsedSec;
		if (clamped < 0.0)
			clamped = 0.0;
		if (clamped > m_Duration)
			clamped = m_Duration;
		float frac = 0.0;
		if (m_Duration > 0.001)
			frac = clamped / m_Duration;
		return m_Start + ((m_End - m_Start) * frac);
	}

	//! Captures the frozen flight elapsed time and resends it to every boarded rider.
	void FreezeFlight()
	{
		if (m_Paused)
			return;
		m_PausedElapsedSec = GetElapsedSec();
		m_Paused = true;
		if (m_FlightActive && m_Riders && m_Riders.Count() > 0)
		{
			for (int i = 0; i < m_Riders.Count(); i++)
			{
				PlayerBase rider = m_Riders.Get(i);
				if (rider && rider.IsAlive() && rider.GetIdentity())
					SendStartPlaneFlight(rider, m_PausedElapsedSec);
			}
			BattleRoyaleUtils.Info(string.Format("[Plane] Flight paused: elapsed=%1 s riders=%2", Math.Round(m_PausedElapsedSec), m_Riders.Count()));
		}
	}

	void UnfreezeFlight(float frozenSeconds)
	{
		if (m_FlightActive && GetGame())
		{
			if (m_Paused)
				m_StartMs = GetGame().GetTime() - Math.Round(m_PausedElapsedSec * 1000.0);
			else if (frozenSeconds > 0.0)
				m_StartMs = m_StartMs + Math.Round(frozenSeconds * 1000.0);
		}
		if (m_FlightFinished && m_FlightFinishedMs > 0 && frozenSeconds > 0.0)
			m_FlightFinishedMs = m_FlightFinishedMs + Math.Round(frozenSeconds * 1000.0);
		m_Paused = false;
	}

	void ShiftClockForFreeze(float frozenSeconds)
	{
		UnfreezeFlight(frozenSeconds);
	}

	void ResendFlightToRiders(string reason)
	{
		if (!m_FlightActive || !m_Riders || m_Riders.Count() == 0)
			return;
		float elapsed = GetElapsedSec();
		for (int i = 0; i < m_Riders.Count(); i++)
		{
			PlayerBase rider = m_Riders.Get(i);
			if (rider && rider.IsAlive() && rider.GetIdentity())
				SendStartPlaneFlight(rider, elapsed);
		}
		BattleRoyaleUtils.Info(string.Format("[Plane] Flight resumed (%1): elapsed=%2 s riders=%3", reason, Math.Round(elapsed), m_Riders.Count()));
	}

	bool IsPlayerInRide(PlayerBase player)
	{
		if (!m_FlightActive || !player || !m_Riders)
			return false;
		return m_Riders.Find(player) != -1;
	}

	bool IsPlayerAirborne(PlayerBase player)
	{
		if (!player || !m_Airborne)
			return false;
		return m_Airborne.Find(player) != -1;
	}

	bool IsUnstuckBlocked(PlayerBase player)
	{
		if (!player)
			return false;
		if (IsPlayerInRide(player))
			return true;
		if (IsPlayerAirborne(player))
			return true;
		if (player.BR_InAir())
			return true;
		return false;
	}

	//! True while any connected living player is still aboard the plane or descending.
	bool HoldsStartMatch()
	{
		int i;
		if (m_FlightActive && m_Riders)
		{
			for (i = 0; i < m_Riders.Count(); i++)
			{
				PlayerBase r = m_Riders.Get(i);
				if (r && r.IsAlive() && r.GetIdentity())
					return true;
			}
		}
		if (m_Airborne)
		{
			for (i = 0; i < m_Airborne.Count(); i++)
			{
				PlayerBase a = m_Airborne.Get(i);
				if (a && a.IsAlive() && a.GetIdentity())
					return true;
			}
		}
		return false;
	}

	protected void TrackAirborne(PlayerBase player)
	{
		if (!player || !m_Airborne)
			return;
		if (m_Airborne.Find(player) == -1)
			m_Airborne.Insert(player);
	}

	protected void UntrackAirborne(PlayerBase player)
	{
		if (!player)
			return;
		if (m_Airborne)
		{
			int idx = m_Airborne.Find(player);
			if (idx != -1)
				m_Airborne.Remove(idx);
		}
		if (m_AirborneSeen)
		{
			int seenIdx = m_AirborneSeen.Find(player);
			if (seenIdx != -1)
				m_AirborneSeen.Remove(seenIdx);
		}
	}

	protected void MarkFlightFinished(string reason)
	{
		m_FlightActive = false;
		if (!m_FlightFinished)
		{
			m_FlightFinished = true;
			if (GetGame())
				m_FlightFinishedMs = GetGame().GetTime();
			BattleRoyaleUtils.Info(string.Format("[Plane] Flight ended: reason=%1 airborne=%2", reason, GetActiveAirborneCount()));
		}
	}

	protected void PruneRiders()
	{
		if (!m_Riders)
			return;
		for (int i = m_Riders.Count() - 1; i >= 0; i--)
		{
			PlayerBase r = m_Riders.Get(i);
			if (!r || !r.IsAlive())
			{
				if (r)
				{
					r.BR_SetPlaneInRide(false);
					r.BR_SetPlaneFrozen(false);
					r.BR_SetPlaneHidden(false);
					SendEndPlaneFlight(r, false);
				}
				m_Riders.Remove(i);
			}
		}
	}

	protected vector ClampExitPosition(PlayerBase player, vector pos)
	{
		if (GetGame() && GetGame().GetWorld())
		{
			float worldSize = GetGame().GetWorld().GetWorldSize();
			if (worldSize > 20.0)
			{
				pos[0] = Math.Clamp(pos[0], 10.0, worldSize - 10.0);
				pos[2] = Math.Clamp(pos[2], 10.0, worldSize - 10.0);
			}
		}
		if (player)
		{
			float minY = player.BR_GroundY(pos[0], pos[2]) + 5.0;
			if (pos[1] < minY)
				pos[1] = minY;
		}
		return pos;
	}

	protected void ExecuteExit(PlayerBase player, float elapsedSec, float backOffsetM, float lateralOffsetM, string source)
	{
		if (!player || !m_Riders)
			return;
		int rideIdx = m_Riders.Find(player);
		if (rideIdx == -1)
			return;

		m_Riders.Remove(rideIdx);
		player.BR_SetPlaneInRide(false);

		vector planePos = GetPlanePositionAt(elapsedSec);
		vector exitPos = planePos - (m_HorizDir * backOffsetM) + (m_RightDir * lateralOffsetM);
		exitPos[1] = planePos[1] - EXIT_DOWN_M;
		exitPos = ClampExitPosition(player, exitPos);

		player.SetPosition(exitPos);
		SetVelocity(player, vector.Zero);
		SendEndPlaneFlight(player, true);

		player.BR_ResetAutoOpenDescent();
		player.BR_SetPlaneFrozen(false);
		RestoreBRInput(player);
		player.BR_ApplyPlaneJumpVelocity();
		player.BR_BumpPlaneJumpGen();
		TrackAirborne(player);

		if (source == "manual")
			BattleRoyaleUtils.Info(string.Format("[Plane] Player jumped: player=%1 pos=%2 lateral=%3 m", player.BR_GetPlayerName(), exitPos.ToString(), Math.Round(lateralOffsetM)));
		else
			BattleRoyaleUtils.Info(string.Format("[Plane] Player ejected (%1): player=%2 pos=%3 lateral=%4 m", source, player.BR_GetPlayerName(), exitPos.ToString(), Math.Round(lateralOffsetM)));

		if (m_Riders.Count() == 0 && m_FlightActive)
		{
			string finishReason = "all_jumped";
			if (source != "manual")
				finishReason = source;
			MarkFlightFinished(finishReason);
		}
	}

	protected void EjectFan(array<PlayerBase> remaining, float elapsedSec, string source)
	{
		if (!remaining)
			return;
		int total = remaining.Count();
		for (int i = 0; i < total; i++)
		{
			PlayerBase remPlayer = remaining.Get(i);
			if (!remPlayer)
				continue;
			int row = i / EJECT_FAN_ROW_WIDTH;
			int col = i % EJECT_FAN_ROW_WIDTH;
			int rowStart = row * EJECT_FAN_ROW_WIDTH;
			int colsInRow = total - rowStart;
			if (colsInRow > EJECT_FAN_ROW_WIDTH)
				colsInRow = EJECT_FAN_ROW_WIDTH;
			float lateral = (col - ((colsInRow - 1) * 0.5)) * EJECT_FAN_STEP_M;
			float back = EXIT_BACK_M + (row * EJECT_FAN_STEP_M);
			ExecuteExit(remPlayer, elapsedSec, back, lateral, source);
		}
	}

	protected void EjectAllAtEnd(float endElapsed)
	{
		array<PlayerBase> remaining = new array<PlayerBase>;
		for (int i = 0; i < m_Riders.Count(); i++)
			remaining.Insert(m_Riders.Get(i));

		BattleRoyaleUtils.Info(string.Format("[Plane] End of flight line: ejecting %1 remaining players", remaining.Count()));
		EjectFan(remaining, endElapsed, "end_of_path");
		m_Riders.Clear();
		MarkFlightFinished("end_of_path");
	}

	//! Ejects all remaining riders in a fan and opens their parachutes immediately (admin Next during flight).
	int EjectAllWithParachute(string source)
	{
		if (!m_FlightActive || !m_Riders || m_Riders.Count() == 0)
		{
			if (m_FlightActive)
				MarkFlightFinished(source);
			return 0;
		}

		array<PlayerBase> remaining = new array<PlayerBase>;
		for (int i = 0; i < m_Riders.Count(); i++)
			remaining.Insert(m_Riders.Get(i));

		int count = remaining.Count();
		EjectFan(remaining, GetElapsedSec(), source);
		for (int j = 0; j < count; j++)
		{
			PlayerBase p = remaining.Get(j);
			if (!p || !p.IsAlive())
				continue;
			if (!p.IsParachuteOpen() && GetOpenRefusal(p) == "")
			{
				p.SetParachuteOpen(true);
				p.BR_SetChuteWasOpen(true);
				BattleRoyaleUtils.Info(string.Format("[Plane] Parachute opened (%1): player=%2 agl=%3 m", source, p.BR_GetPlayerName(), Math.Round(p.BR_AGL())));
			}
		}

		m_Riders.Clear();
		MarkFlightFinished(source);
		return count;
	}

	bool HandleJumpRequest(PlayerBase player)
	{
		if (m_Paused)
		{
			if (player)
				BattleRoyaleUtils.Debug("[Plane] Jump refused (paused) for " + player.BR_GetPlayerName());
			return false;
		}
		if (!player || !player.IsAlive() || !IsPlayerInRide(player))
		{
			if (player)
				BattleRoyaleUtils.Debug("[Plane] Jump refused (not_in_ride) for " + player.BR_GetPlayerName());
			return false;
		}
		float elapsed = GetElapsedSec();
		vector planePos = GetPlanePositionAt(elapsed);
		if (GetGame() && GetGame().SurfaceIsSea(planePos[0], planePos[2]))
		{
			BattleRoyaleUtils.Debug("[Plane] Jump refused (over_sea) for " + player.BR_GetPlayerName());
			return false;
		}
		float lateral = Math.RandomFloatInclusive(-EXIT_LATERAL_JITTER_M, EXIT_LATERAL_JITTER_M);
		ExecuteExit(player, elapsed, EXIT_BACK_M, lateral, "manual");
		return true;
	}

	void ResyncRider(PlayerBase player, string reason)
	{
		if (!player || !player.GetIdentity() || !IsPlayerInRide(player))
			return;
		float elapsed = GetElapsedSec();
		player.BR_SetPlaneDeploymentActive(true);
		player.BR_SetPlaneInRide(true);
		player.BR_SetPlaneHidden(true);
		GetRPCManager().SendRPC(RPC_DAYZBR_NAMESPACE, "SetInput", new Param1<bool>(true), true, player.GetIdentity());
		SendStartPlaneFlight(player, elapsed);
		if (BattleRoyaleUtils.CheckLogLevel(BattleRoyaleUtils.DEBUG))
			BattleRoyaleUtils.Debug(string.Format("[Plane] Resynced rider %1 reason=%2 elapsed=%3 s", player.BR_GetPlayerName(), reason, elapsed));
	}

	void OnPlayerRemoved(PlayerBase player)
	{
		if (!player)
			return;
		if (m_Riders)
		{
			int rideIdx = m_Riders.Find(player);
			if (rideIdx != -1)
			{
				m_Riders.Remove(rideIdx);
				player.BR_SetPlaneInRide(false);
				player.BR_SetPlaneFrozen(false);
				SendEndPlaneFlight(player, false);
				if (m_Riders.Count() == 0 && m_FlightActive)
					MarkFlightFinished("riders_removed");
			}
		}
		bool wasAirborne = IsPlayerAirborne(player);
		UntrackAirborne(player);
		if (wasAirborne && BattleRoyaleUtils.CheckLogLevel(BattleRoyaleUtils.DEBUG))
			BattleRoyaleUtils.Debug(string.Format("[Plane] Removed airborne player (death/disconnect): player=%1 remaining_airborne=%2", player.BR_GetPlayerName(), GetActiveAirborneCount()));
	}

	//! Restores a boarded or airborne player to normal ground state for match end, restart or serial turnover.
	protected void ReturnPlayerToGround(PlayerBase player, vector refPos)
	{
		if (!player)
			return;

		if (player.IsParachuteOpen())
		{
			player.SetParachuteOpen(false);
			player.DeleteCanopy();
		}
		player.BR_ResetAutoOpenDescent();
		player.BR_SetPlaneInRide(false);
		player.BR_SetPlaneFrozen(false);
		player.BR_SetPlaneHidden(false);
		player.BR_SetPlaneDeploymentActive(false);

		if (player.IsAlive())
		{
			float gx = refPos[0];
			float gz = refPos[2];
			float gy = player.BR_GroundY(gx, gz);
			vector groundPos = Vector(gx, gy, gz);
			vector dirG = player.GetDirection();
			dirG[1] = 0.0;

			player.SetDynamicPhysicsLifeTime(0.001);
			dBodyActive(player, ActiveState.INACTIVE);
			player.SetPosition(groundPos);
			dBodyActive(player, ActiveState.ACTIVE);
			player.PhysicsEnableGravity(true);
			SetVelocity(player, vector.Zero);

			ScriptJunctureData pCtx = new ScriptJunctureData;
			pCtx.Write(groundPos);
			pCtx.Write(dirG);
			player.SendSyncJuncture(88, pCtx);
		}

		RestoreBRInput(player);
		SendEndPlaneFlight(player, false);
	}

	//! Defensive normalizer for serial turnover: ensures any player still boarded, hidden, frozen or airborne
	//! is returned to normal state before the lobby character swap, and tells its client to end the flight.
	static bool NormalizePlayerForTurnover(PlayerBase player, string reason)
	{
		if (!player)
			return false;

		bool needsReset = player.BR_IsPlaneInRide() || player.BR_IsPlaneFrozen() || player.BR_IsPlaneHidden() || player.IsParachuteOpen();
		if (!needsReset && player.BR_IsPlaneDeploymentActive() && player.BR_InAir())
			needsReset = true;

		if (!needsReset)
		{
			if (player.BR_IsPlaneDeploymentActive())
				player.BR_SetPlaneDeploymentActive(false);
			return false;
		}

		if (player.IsParachuteOpen())
		{
			player.SetParachuteOpen(false);
			player.DeleteCanopy();
		}
		player.BR_ResetAutoOpenDescent();
		player.BR_SetPlaneInRide(false);
		player.BR_SetPlaneFrozen(false);
		player.BR_SetPlaneHidden(false);
		player.BR_SetPlaneDeploymentActive(false);
		player.DisableInput(false);

		if (player.IsAlive() && player.BR_InAir())
		{
			vector pos = player.GetPosition();
			float gy = player.BR_GroundY(pos[0], pos[2]);
			vector groundPos = Vector(pos[0], gy, pos[2]);
			player.SetDynamicPhysicsLifeTime(0.001);
			dBodyActive(player, ActiveState.INACTIVE);
			player.SetPosition(groundPos);
			dBodyActive(player, ActiveState.ACTIVE);
			player.PhysicsEnableGravity(true);
			SetVelocity(player, vector.Zero);
		}

		PlayerIdentity pid = player.GetIdentity();
		if (pid)
		{
			GetRPCManager().SendRPC(RPC_DAYZBR_NAMESPACE, "SetInput", new Param1<bool>(false), true, pid);
			GetRPCManager().SendRPC(RPC_DAYZBR_NAMESPACE, "EndPlaneFlight", new Param1<bool>(false), true, pid);
		}

		BattleRoyaleUtils.Info(string.Format("[Plane] Normalized player for %1: player=%2", reason, player.BR_GetPlayerName()));
		return true;
	}

	//! Closes any active flight when StartMatch deactivates or the match ends/turns over.
	//! toGround=true returns remaining riders and airborne players to terrain with damage, gravity and input restored;
	//! toGround=false ejects remaining riders in a fan to continue the match.
	void CloseFlight(string reason, bool toGround)
	{
		int riderCount = 0;
		if (m_Riders)
			riderCount = m_Riders.Count();
		int airCount = 0;
		if (m_Airborne)
			airCount = m_Airborne.Count();

		if (!toGround)
		{
			if (!m_FlightActive || riderCount == 0)
			{
				if (m_FlightActive)
					MarkFlightFinished(reason);
				return;
			}

			array<PlayerBase> remaining = new array<PlayerBase>;
			for (int i = 0; i < riderCount; i++)
				remaining.Insert(m_Riders.Get(i));

			BattleRoyaleUtils.Info(string.Format("[Plane] Closing flight: reason=%1 to_ground=0 riders=%2", reason, remaining.Count()));
			EjectFan(remaining, GetElapsedSec(), reason);
			m_Riders.Clear();
			m_Paused = false;
			MarkFlightFinished(reason);
			return;
		}

		if (!m_FlightActive && riderCount == 0 && airCount == 0)
			return;

		array<PlayerBase> ridersToGround = new array<PlayerBase>;
		for (int ri = 0; ri < riderCount; ri++)
			ridersToGround.Insert(m_Riders.Get(ri));

		array<PlayerBase> airToGround = new array<PlayerBase>;
		for (int ai = 0; ai < airCount; ai++)
			airToGround.Insert(m_Airborne.Get(ai));

		float elapsedG = GetElapsedSec();
		vector planePosG = GetPlanePositionAt(elapsedG);
		if (m_Riders)
			m_Riders.Clear();
		if (m_Airborne)
			m_Airborne.Clear();
		if (m_AirborneSeen)
			m_AirborneSeen.Clear();
		m_FlightActive = false;
		m_Paused = false;

		BattleRoyaleUtils.Info(string.Format("[Plane] Closing flight: reason=%1 to_ground=1 riders=%2 airborne=%3", reason, ridersToGround.Count(), airToGround.Count()));

		for (int j = 0; j < ridersToGround.Count(); j++)
			ReturnPlayerToGround(ridersToGround.Get(j), planePosG);

		for (int k = 0; k < airToGround.Count(); k++)
		{
			PlayerBase airPlayer = airToGround.Get(k);
			if (airPlayer)
				ReturnPlayerToGround(airPlayer, airPlayer.GetPosition());
		}

		GetRPCManager().SendRPC(RPC_DAYZBR_NAMESPACE, "EndPlaneFlight", new Param1<bool>(false), true);
		MarkFlightFinished(reason);
	}

	protected void TickRide(float dt)
	{
		PruneRiders();
		if (!m_Riders || m_Riders.Count() == 0)
		{
			MarkFlightFinished("all_jumped");
			return;
		}

		if (GetGame())
		{
			float rawElapsed = (GetGame().GetTime() - m_StartMs) * 0.001;
			if (rawElapsed >= m_Duration)
			{
				EjectAllAtEnd(m_Duration);
				return;
			}
		}

		m_RideAcc = m_RideAcc + dt;
		float stepT = 1.0 / RIDE_STEP_HZ;
		int guard = 0;
		while (m_FlightActive && m_RideAcc >= stepT && guard < 4)
		{
			guard = guard + 1;
			m_RideAcc = m_RideAcc - stepT;
			PruneRiders();
			if (m_Riders.Count() == 0)
			{
				MarkFlightFinished("all_jumped");
				return;
			}
			float curElapsed = GetElapsedSec();
			if (curElapsed >= m_Duration)
			{
				EjectAllAtEnd(m_Duration);
				return;
			}
			vector nextPos = GetPlanePositionAt(curElapsed);
			for (int i = 0; i < m_Riders.Count(); i++)
			{
				PlayerBase rider = m_Riders.Get(i);
				if (!rider)
					continue;
				rider.SetPosition(nextPos);
				SetVelocity(rider, vector.Zero);
			}
			if (BattleRoyaleUtils.CheckLogLevel(BattleRoyaleUtils.TRACE))
				BattleRoyaleUtils.Trace(string.Format("[Plane] Ride step pos=%1 riders=%2", nextPos.ToString(), m_Riders.Count()));
		}
	}

	//! Ticks only airborne players for landing detection and safety auto-open.
	protected void TickAirborneAndAutoOpen()
	{
		if (!m_Airborne || m_Airborne.Count() == 0)
			return;

		for (int i = m_Airborne.Count() - 1; i >= 0; i--)
		{
			PlayerBase p = m_Airborne.Get(i);
			if (!p || !p.IsAlive())
			{
				m_Airborne.Remove(i);
				if (p)
					UntrackAirborne(p);
				continue;
			}
			if (IsPlayerInRide(p) || p.BR_IsPlaneFrozen())
				continue;

			int cmdId = p.BR_GetMovementCommandID();
			bool chuteOpen = p.IsParachuteOpen();
			if (chuteOpen)
				p.BR_SetChuteWasOpen(true);

			if ((cmdId == DayZPlayerConstants.COMMANDID_FALL || chuteOpen) && m_AirborneSeen.Find(p) == -1)
				m_AirborneSeen.Insert(p);

			bool swim = p.IsSwimming();
			float agl = p.BR_AGL();
			vector vel = GetVelocity(p);
			bool airborneSeen = m_AirborneSeen.Find(p) != -1;
			bool onGround = false;

			if (!swim && cmdId != DayZPlayerConstants.COMMANDID_FALL && !chuteOpen)
			{
				if ((agl < LANDING_AGL_M || (cmdId == DayZPlayerConstants.COMMANDID_MOVE && airborneSeen)) && Math.AbsFloat(vel[1]) < LANDING_MAX_ABS_VY)
					onGround = true;
			}

			if (swim || onGround)
			{
				m_Airborne.Remove(i);
				int seenIdx = m_AirborneSeen.Find(p);
				if (seenIdx != -1)
					m_AirborneSeen.Remove(seenIdx);
				p.BR_ResetAutoOpenDescent();
				BattleRoyaleUtils.Info(string.Format("[Plane] Player landed: player=%1 agl=%2 m swim=%3 remaining_airborne=%4", p.BR_GetPlayerName(), Math.Round(agl), swim, m_Airborne.Count()));
				continue;
			}

			// Safety auto-open below configured AGL, never below 5 m, never re-opening.
			if (!chuteOpen && p.BR_InAir() && vel[1] < AUTO_OPEN_MIN_FALL_VY && agl < m_AutoOpenAlt)
			{
				if (GetOpenRefusal(p) == "")
				{
					if (agl < AUTO_OPEN_MIN_AGL_M)
					{
						if (!p.BR_WasAutoOpenSkipLogged())
						{
							p.BR_SetAutoOpenSkipLogged(true);
							BattleRoyaleUtils.Debug(string.Format("[Plane] Auto-open skipped (min_agl): player=%1 agl=%2 m", p.BR_GetPlayerName(), Math.Round(agl)));
						}
					}
					else if (p.BR_WasChuteOpen())
					{
						if (!p.BR_WasAutoOpenSkipLogged())
						{
							p.BR_SetAutoOpenSkipLogged(true);
							BattleRoyaleUtils.Debug(string.Format("[Plane] Auto-open skipped (was_open): player=%1 agl=%2 m", p.BR_GetPlayerName(), Math.Round(agl)));
						}
					}
					else
					{
						p.SetParachuteOpen(true);
						p.BR_SetChuteWasOpen(true);
						BattleRoyaleUtils.Info(string.Format("[Plane] Parachute opened (auto): player=%1 agl=%2 m vy=%3", p.BR_GetPlayerName(), Math.Round(agl), Math.Round(vel[1])));
					}
				}
			}
		}
	}

	//! The plane moves riders at flight speed and the parachute glide is faster than a
	//! runner, so COT's speed and teleport rules would flag every player in the drop.
	protected void GrantAntiCheatMovement(PlayerBase player)
	{
#ifdef COT_2026_1020
		if (player)
			player.COT_GrantAction(JMAntiCheatSanction.MOVEMENT, ANTICHEAT_GRACE_SEC);
#endif
	}

	protected void TickAntiCheatGrace(float dt)
	{
		m_AntiCheatGraceTimer = m_AntiCheatGraceTimer - dt;
		if (m_AntiCheatGraceTimer > 0.0)
			return;
		m_AntiCheatGraceTimer = 1.0;

		int i;
		if (m_FlightActive && m_Riders)
		{
			for (i = 0; i < m_Riders.Count(); i++)
				GrantAntiCheatMovement(m_Riders.Get(i));
		}
		if (m_Airborne)
		{
			for (i = 0; i < m_Airborne.Count(); i++)
				GrantAntiCheatMovement(m_Airborne.Get(i));
		}
	}

	void Tick(float dt)
	{
		TickAntiCheatGrace(dt);
		if (m_FlightActive && !m_Paused)
			TickRide(dt);
		TickAirborneAndAutoOpen();
	}
}
#endif

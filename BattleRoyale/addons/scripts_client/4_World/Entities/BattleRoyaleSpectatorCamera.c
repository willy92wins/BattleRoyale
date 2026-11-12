//! Camera of a dead player who watches the players still alive (created by the server with SelectSpectator).
//! First person sits at the watched player's eyes and looks along the weapon, or the body when no weapon is held;
//! third person orbits behind the player with the mouse. The vanilla person view key switches between them.
class BattleRoyaleSpectatorCamera: Camera
{
	static bool s_ThirdPerson;

	protected float m_UpdateAccumulator;
	protected float m_MissingTarget;
	protected float m_FrameTimeSlice;
	protected float m_OrbitYaw;
	protected float m_OrbitPitch = -15;
	protected int m_HeadBoneIndex = -1;
	protected string m_HeadBoneTargetType;
	protected Human m_HeadBoneTarget;

	void BattleRoyaleSpectatorCamera()
	{
		SetEventMask(EntityEvent.FRAME | EntityEvent.POSTFRAME);
	}

	override void EOnFrame(IEntity other, float timeSlice)
	{
		m_FrameTimeSlice = timeSlice;
	}

	// Placed after the frame update, as the player camera system writes the view during the frame
	override void EOnPostFrame(IEntity other, int extra)
	{
#ifndef SERVER
		float timeSlice = m_FrameTimeSlice;
		// The engine creates this camera for the spectator but does not switch the view to it
		if (Camera.GetCurrentCamera() != this)
		{
			if (!BattleRoyaleRPC.s_Spectating)
				return;

			SetActive(true);
			if (Camera.GetCurrentCamera() != this)
				return;
		}

		HandleInput(timeSlice);

		// Target sent by the server (SpectateTarget RPC)
		Human target;
		if (BattleRoyaleRPC.s_SpectateTargetLow != 0 || BattleRoyaleRPC.s_SpectateTargetHigh != 0)
			Class.CastTo(target, GetGame().GetObjectByNetworkId(BattleRoyaleRPC.s_SpectateTargetLow, BattleRoyaleRPC.s_SpectateTargetHigh));

		if (target)
		{
			m_MissingTarget = 0;
			if (s_ThirdPerson)
				FollowThirdPerson(target);
			else
				FollowFirstPerson(target);
		}
		else
		{
			// Not streamed in yet: wait at the position the server sent, and ask for a fresh one
			// when the target stays out of reach (moved far away, or died without a new target yet)
			SetPosition(BattleRoyaleRPC.s_SpectateTargetHint);
			m_MissingTarget += timeSlice;
			if (m_MissingTarget > 3)
			{
				m_MissingTarget = 0;
				GetRPCManager().SendRPC(RPC_DAYZBRSERVER_NAMESPACE, "SpectateCycle", new Param1<int>(0), true);
			}
		}

		// The server streams the world around this position
		m_UpdateAccumulator += timeSlice;
		if (m_UpdateAccumulator > 0.5)
		{
			GetGame().UpdateSpectatorPosition(GetPosition());
			m_UpdateAccumulator = 0;
		}
#endif
	}

#ifndef SERVER
	protected void HandleInput(float timeSlice)
	{
		if (GetUApi().GetInputByID(UAPersonView).LocalPress())
			s_ThirdPerson = !s_ThirdPerson;

		if (GetUApi().GetInputByID(UAFire).LocalPress())
			GetRPCManager().SendRPC(RPC_DAYZBRSERVER_NAMESPACE, "SpectateCycle", new Param1<int>(1), true);
		else if (GetUApi().GetInputByID(UATempRaiseWeapon).LocalPress())
			GetRPCManager().SendRPC(RPC_DAYZBRSERVER_NAMESPACE, "SpectateCycle", new Param1<int>(-1), true);

		if (s_ThirdPerson)
		{
			float yawDiff = GetUApi().GetInputByID(UAAimLeft).LocalValue() - GetUApi().GetInputByID(UAAimRight).LocalValue();
			float pitchDiff = GetUApi().GetInputByID(UAAimDown).LocalValue() - GetUApi().GetInputByID(UAAimUp).LocalValue();
			m_OrbitYaw = Math.NormalizeAngle(m_OrbitYaw - Math.RAD2DEG * yawDiff * timeSlice);
			m_OrbitPitch = Math.Clamp(m_OrbitPitch - Math.RAD2DEG * pitchDiff * timeSlice, -70, 40);
		}
	}

	protected void FollowFirstPerson(Human target)
	{
		// All player skeletons share the bone layout, so the by-name lookup is only needed once per target class
		if (m_HeadBoneIndex == -1)
		{
			m_HeadBoneIndex = target.GetBoneIndexByName("Head");
			m_HeadBoneTargetType = target.GetType();
			m_HeadBoneTarget = target;
		}
		else if (target != m_HeadBoneTarget)
		{
			if (m_HeadBoneTargetType != target.GetType())
			{
				m_HeadBoneIndex = target.GetBoneIndexByName("Head");
				m_HeadBoneTargetType = target.GetType();
			}
			m_HeadBoneTarget = target;
		}

		vector head[4];
		target.GetBoneTransformWS(m_HeadBoneIndex, head);

		vector body[4];
		target.GetTransform(body);
		vector direction = body[2];

		// Look along the weapon when one is held, it follows the aim up and down
		Weapon_Base weapon;
		if (Class.CastTo(weapon, target.GetHumanInventory().GetEntityInHands()))
		{
			vector weaponTransform[4];
			weapon.GetTransform(weaponTransform);
			vector barrelStart = weapon.GetSelectionPositionLS("konec hlavne").Multiply4(weaponTransform);
			vector barrelEnd = weapon.GetSelectionPositionLS("usti hlavne").Multiply4(weaponTransform);
			if (vector.DistanceSq(barrelStart, barrelEnd) > 0.0001)
				direction = (barrelEnd - barrelStart).Normalized();
		}

		SetPosition(head[3] + direction * 0.15 + "0 0.05 0");
		SetOrientation(direction.VectorToAngles());

		// Third person starts behind the player when switching
		m_OrbitYaw = direction.VectorToAngles()[0];
	}

	protected void FollowThirdPerson(Human target)
	{
		vector pivot = target.GetPosition() + "0 1.6 0";
		vector angles = Vector(m_OrbitYaw, m_OrbitPitch, 0);
		vector forward = angles.AnglesToVector();
		vector desired = pivot - forward * 3.0;

		// Keep the camera in front of walls and terrain between it and the player
		vector hitPosition;
		vector hitNormal;
		float hitFraction;
		Object hitObject;
		PhxInteractionLayers layers = PhxInteractionLayers.BUILDING | PhxInteractionLayers.VEHICLE | PhxInteractionLayers.TERRAIN | PhxInteractionLayers.ITEM_LARGE;
		if (DayZPhysics.RayCastBullet(pivot, desired, layers, target, hitObject, hitPosition, hitNormal, hitFraction))
			desired = hitPosition + forward * 0.2;

		SetPosition(desired);
		SetOrientation(angles);
	}
#endif
}

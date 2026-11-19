//! Camera of a dead player who watches the players still alive (created by the server with SelectSpectator).
//! First person recreates what the watched player sees: from its eyes where it aims, and through the iron sights or the
//! optic that does not magnify it looks through, which its own client reports (SpectateSights RPC). Behind a magnifying
//! optic it stays at the eyes: the optic's lens is drawn only on its owner's client.
//! Its head and what it wears on it are hidden on this client meanwhile, as the vanilla first person camera hides the
//! player's own. Third person orbits behind the player with the mouse. The vanilla person view key switches between them.
class BattleRoyaleSpectatorCamera: Camera
{
	//! Each spectate starts in third person (the server makes a new camera for it); the vanilla person view key
	//! switches to first person and back
	protected bool m_ThirdPerson = true;

	//! Eyes in the head bone's frame (x up, y forward), where the vanilla first person camera sits (DayZPlayerCamera1stPerson)
	protected static const float EYE_UP = 0.04;
	protected static const float EYE_FORWARD = 0.04;
	//! Near planes of the vanilla cameras: first person and iron sights, the least for an optic, and the default
	protected static const float NEAR_PLANE_EYES = 0.04;
	protected static const float NEAR_PLANE_OPTICS = 0.06;
	protected static const float NEAR_PLANE_ORBIT = 0.07;
	//! Seconds between two refreshes of the hidden head, so that a helmet put on meanwhile is hidden too
	protected static const float HEAD_REFRESH_SECONDS = 1.0;
	//! Smoothing times, in seconds, of the first person view: from the eyes its angles follow the watched player's aim,
	//! which reaches this client in steps, the eyes follow the head without its bobbing, and the view moves over between
	//! the eyes and the sights
	protected static const float VIEW_SMOOTH_TIME = 0.15;
	protected static const float EYES_SMOOTH_TIME = 0.2;
	//! Most the smoothed eyes may stay from the head's, in metres: about the head's bobbing, so that a quick crouch, a fall
	//! to the ground or a vehicle tilting does not leave the view above cover or out of the cabin
	protected static const float EYES_MAX_LAG = 0.06;
	protected static const float SIGHTS_SMOOTH_TIME = 0.1;

	protected float m_UpdateAccumulator;
	protected float m_MissingTarget;
	protected float m_FrameTimeSlice;
	protected float m_OrbitYaw;
	protected float m_OrbitPitch = -15;
	//! Watched player the orbit is around: a new one starts the orbit behind it
	protected Human m_OrbitTarget;
	protected int m_HeadBoneIndex = -1;
	protected string m_HeadBoneTargetType;
	protected Human m_HeadBoneTarget;

	//! First person view: the watched player (a new one is taken at once), what it looks through (EBRSpectateSights),
	//! the angles from the eyes smoothed towards its aim, and the view as shown
	protected Human m_ViewTarget;
	protected int m_ViewSights;
	protected float m_ViewYaw;
	protected float m_ViewPitch;
	protected float m_ViewYawVelocity[1];
	protected float m_ViewPitchVelocity[1];
	protected vector m_ShownOffset;
	protected float m_ShownYaw;
	protected float m_ShownPitch;
	protected float m_ShownFOV;
	//! Gap between the view shown and the new one when the watched player switches between the eyes and the sights,
	//! closed as the weight goes from 1 to 0. Through the sights the view is otherwise their own, with no delay
	protected vector m_SwitchOffset;
	protected float m_SwitchYaw;
	protected float m_SwitchPitch;
	protected float m_SwitchFOV;
	protected float m_SwitchWeight;
	protected float m_SwitchWeightVelocity[1];
	//! Eyes relative to the watched player's position, smoothed so that the view does not bob with the head
	protected vector m_EyesOffset;
	protected float m_EyesVelocityX[1];
	protected float m_EyesVelocityY[1];
	protected float m_EyesVelocityZ[1];

	//! Optic of the watched player shown as its owner sees it, and every entity of the watched player's clothing hidden
	//! for it, until LeaveOptics
	protected ItemOptics m_ShownOptic;
	protected ref array<EntityAI> m_HiddenClothingItems = new array<EntityAI>;
	//! Watched player whose head is hidden, every entity hidden for it, and the seconds since they were hidden, until ShowHead
	protected Human m_HiddenHeadTarget;
	protected ref array<EntityAI> m_HiddenHeadItems = new array<EntityAI>;
	protected float m_HiddenHeadAge;
	//! The newest spectator camera on this client. The engine makes one for every spectate (SelectSpectator) and never
	//! deletes the ones before, which would otherwise take the view back every frame
	protected static BattleRoyaleSpectatorCamera s_Newest;

	void BattleRoyaleSpectatorCamera()
	{
		SetEventMask(EntityEvent.FRAME | EntityEvent.POSTFRAME);
		s_Newest = this;
	}

	void ~BattleRoyaleSpectatorCamera()
	{
#ifndef SERVER
		LeaveOptics();
		ShowHead();
#endif
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
		// A camera from an earlier spectate gives back what it changed on this client and leaves the view to the newest
		if (s_Newest != this)
		{
			LeaveOptics();
			ShowHead();
			return;
		}

		// Back in the lobby: what this camera changed on this client goes back, also when another camera is the current
		// one already
		if (!BattleRoyaleRPC.s_Spectating)
		{
			LeaveOptics();
			ShowHead();
		}

		// The engine creates this camera for the spectator but does not switch the view to it
		if (Camera.GetCurrentCamera() != this)
		{
			if (!BattleRoyaleRPC.s_Spectating)
				return;

			SetActive(true);
			if (Camera.GetCurrentCamera() != this)
				return;
		}

		// Serial matches: the server gave this client a new lobby character. The view goes back to the player only once
		// that character is here, so the client is never left without an active camera in between
		if (!BattleRoyaleRPC.s_Spectating)
		{
			Man local_player = GetGame().GetPlayer();
			if (local_player && local_player.IsAlive())
				SetActive(false);
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
			if (m_ThirdPerson)
			{
				m_ViewTarget = null;
				FollowThirdPerson(target);
			}
			else
				FollowFirstPerson(target, timeSlice);
		}
		else
		{
			m_ViewTarget = null;
			LeaveOptics();
			ShowHead();
			SetFOV(GetDayZGame().GetUserFOV());
			SetNearPlane(NEAR_PLANE_ORBIT);

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
			m_ThirdPerson = !m_ThirdPerson;

		if (GetUApi().GetInputByID(UAFire).LocalPress())
			GetRPCManager().SendRPC(RPC_DAYZBRSERVER_NAMESPACE, "SpectateCycle", new Param1<int>(1), true);
		else if (GetUApi().GetInputByID(UATempRaiseWeapon).LocalPress())
			GetRPCManager().SendRPC(RPC_DAYZBRSERVER_NAMESPACE, "SpectateCycle", new Param1<int>(-1), true);

		if (m_ThirdPerson)
		{
			float yawDiff = GetUApi().GetInputByID(UAAimLeft).LocalValue() - GetUApi().GetInputByID(UAAimRight).LocalValue();
			float pitchDiff = GetUApi().GetInputByID(UAAimDown).LocalValue() - GetUApi().GetInputByID(UAAimUp).LocalValue();
			m_OrbitYaw = Math.NormalizeAngle(m_OrbitYaw - Math.RAD2DEG * yawDiff * timeSlice);
			m_OrbitPitch = Math.Clamp(m_OrbitPitch - Math.RAD2DEG * pitchDiff * timeSlice, -70, 40);
		}
	}

	protected void FollowFirstPerson(Human target, float timeSlice)
	{
		HideHead(target, timeSlice);

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
		vector eyes = StableEyes(target, head[3] + head[0] * EYE_UP + head[1] * EYE_FORWARD, timeSlice);

		vector position;
		vector angles;
		float fov;
		float nearPlane;
		int sights = BattleRoyaleRPC.s_SpectateSights;
		if (!FollowSights(target, position, angles, fov, nearPlane))
		{
			sights = EBRSpectateSights.NONE;
			LeaveOptics();

			// As the vanilla first person camera does (DayZPlayerCamera1stPerson), the head bone gives the position and
			// the view looks where the player aims
			position = eyes;
			angles = AimAngles(target, head[1]);
			fov = GetDayZGame().GetUserFOV();
			nearPlane = NEAR_PLANE_EYES;
		}

		SmoothView(target, sights, position - eyes, angles, fov, timeSlice);
		SetPosition(eyes + m_ShownOffset);
		SetOrientation(Vector(m_ShownYaw, m_ShownPitch, 0));
		SetFOV(m_ShownFOV);
		SetNearPlane(nearPlane);

		// Third person starts behind the player when switching
		m_OrbitYaw = m_ShownYaw;
		m_OrbitTarget = target;
	}

	//! The eyes without the head's bobbing: their place relative to the watched player's position is smoothed. A new
	//! watched player is taken at once (SmoothView changes m_ViewTarget after this)
	protected vector StableEyes(Human target, vector eyes, float timeSlice)
	{
		vector origin = target.GetPosition();
		vector offset = eyes - origin;
		if (target != m_ViewTarget)
		{
			m_EyesOffset = offset;
			m_EyesVelocityX[0] = 0;
			m_EyesVelocityY[0] = 0;
			m_EyesVelocityZ[0] = 0;
			return eyes;
		}

		float x = Math.SmoothCD(m_EyesOffset[0], offset[0], m_EyesVelocityX, EYES_SMOOTH_TIME, 1000, timeSlice);
		float y = Math.SmoothCD(m_EyesOffset[1], offset[1], m_EyesVelocityY, EYES_SMOOTH_TIME, 1000, timeSlice);
		float z = Math.SmoothCD(m_EyesOffset[2], offset[2], m_EyesVelocityZ, EYES_SMOOTH_TIME, 1000, timeSlice);
		m_EyesOffset = Vector(x, y, z);

		vector lag = m_EyesOffset - offset;
		if (lag.Length() > EYES_MAX_LAG)
			m_EyesOffset = offset + lag.Normalized() * EYES_MAX_LAG;
		return origin + m_EyesOffset;
	}

	//! Works out the view shown from the one wanted (its place relative to the eyes, angles and field of view): from the
	//! eyes the angles are smoothed, through the sights the view is theirs, and a switch between the two moves over. A
	//! new watched player is taken at once
	protected void SmoothView(Human target, int sights, vector offset, vector angles, float fov, float timeSlice)
	{
		float yaw = SignedAngle(angles[0]);
		float pitch = SignedAngle(angles[1]);
		if (target != m_ViewTarget)
		{
			m_ViewTarget = target;
			m_ViewSights = sights;
			m_ViewYaw = yaw;
			m_ViewPitch = pitch;
			m_ViewYawVelocity[0] = 0;
			m_ViewPitchVelocity[0] = 0;
			m_SwitchWeight = 0;
			m_SwitchWeightVelocity[0] = 0;
		}
		else if (sights == EBRSpectateSights.NONE)
		{
			// The yaw goes the short way round
			float yawGoal = m_ViewYaw + SignedAngle(yaw - m_ViewYaw);
			m_ViewYaw = SignedAngle(Math.SmoothCD(m_ViewYaw, yawGoal, m_ViewYawVelocity, VIEW_SMOOTH_TIME, 10000, timeSlice));
			m_ViewPitch = Math.SmoothCD(m_ViewPitch, pitch, m_ViewPitchVelocity, VIEW_SMOOTH_TIME, 10000, timeSlice);
		}
		else
		{
			// Back at the eyes the smoothing starts from the sights' angles
			m_ViewYaw = yaw;
			m_ViewPitch = pitch;
			m_ViewYawVelocity[0] = 0;
			m_ViewPitchVelocity[0] = 0;
		}

		if (sights != m_ViewSights)
		{
			m_ViewSights = sights;
			m_SwitchOffset = m_ShownOffset - offset;
			m_SwitchYaw = SignedAngle(m_ShownYaw - m_ViewYaw);
			m_SwitchPitch = m_ShownPitch - m_ViewPitch;
			m_SwitchFOV = m_ShownFOV - fov;
			m_SwitchWeight = 1;
			m_SwitchWeightVelocity[0] = 0;
		}
		else if (m_SwitchWeight > 0)
		{
			m_SwitchWeight = Math.SmoothCD(m_SwitchWeight, 0, m_SwitchWeightVelocity, SIGHTS_SMOOTH_TIME, 1000, timeSlice);
			if (m_SwitchWeight < 0.001)
				m_SwitchWeight = 0;
		}

		m_ShownOffset = offset + m_SwitchOffset * m_SwitchWeight;
		m_ShownYaw = SignedAngle(m_ViewYaw + m_SwitchYaw * m_SwitchWeight);
		m_ShownPitch = m_ViewPitch + m_SwitchPitch * m_SwitchWeight;
		m_ShownFOV = fov + m_SwitchFOV * m_SwitchWeight;
	}

	//! The angle in degrees between -180 and 180
	protected static float SignedAngle(float angle)
	{
		angle = Math.NormalizeAngle(angle);
		if (angle > 180)
			angle -= 360;
		return angle;
	}

	//! Where the watched player looks, as yaw and pitch in degrees: its heading plus its base aiming angles, which carry
	//! no sway (HumanCommandWeapons.GetBaseAimingAngleLR/UD, composed as Weapon_Base does). Those are where its view
	//! points only while it raises its hands (the view is kept on them, DayZPlayerCameraBase); with them lowered its
	//! view moves on its own and only its head shows the height, so the pitch is the head's. Along the head without them
	protected vector AimAngles(Human target, vector headForward)
	{
		vector headAngles = headForward.VectorToAngles();
		HumanCommandWeapons weapons = target.GetCommandModifier_Weapons();
		if (!weapons)
			return headAngles;

		float yaw = target.GetOrientation()[0] + weapons.GetBaseAimingAngleLR();
		HumanMovementState movement = new HumanMovementState();
		target.GetMovementState(movement);
		if (!movement.IsRaised())
			return Vector(yaw, headAngles[1], 0);

		return Vector(yaw, weapons.GetBaseAimingAngleUD(), 0);
	}

	//! Through the sights the watched player looks through: the camera sits at their eye point, as the vanilla iron sights
	//! and optics cameras do (DayZPlayerCameraIronsights.GetCurrentSightInfo, DayZPlayerCameraOptics), with their field of
	//! view and near plane, given back for the view. False when the player looks through none of them
	protected bool FollowSights(Human target, out vector position, out vector angles, out float fov, out float nearPlane)
	{
		int sights = BattleRoyaleRPC.s_SpectateSights;
		if (sights == EBRSpectateSights.NONE)
			return false;

		EntityAI inHands = target.GetHumanInventory().GetEntityInHands();
		Weapon_Base weapon = Weapon_Base.Cast(inHands);
		ItemOptics optic = ItemOptics.Cast(inHands);
		if (weapon)
			optic = weapon.GetAttachedOptics();

		EntityAI sight = null;
		vector eyePoint;
		vector eyeDirection;
		// A magnifying optic stays at the eyes. Its owner reports it as none; this covers an optic swapped since the report
		if (sights == EBRSpectateSights.OPTICS && optic && optic.AllowsDOF())
		{
			optic.UseWeaponIronsightsOverride(false);
			optic.GetCameraPoint(eyePoint, eyeDirection);
			sight = optic;
		}
		else if (sights == EBRSpectateSights.IRONSIGHTS && weapon)
		{
			// An optic that stands in for the iron sights (a reflex sight) is looked through instead
			if (optic && optic.HasWeaponIronsightsOverride())
			{
				optic.UseWeaponIronsightsOverride(true);
				optic.GetCameraPoint(eyePoint, eyeDirection);
				sight = optic;
			}
			else
			{
				weapon.GetCameraPoint(weapon.GetCurrentMuzzle(), eyePoint, eyeDirection);
				sight = weapon;
			}
		}

		if (!sight)
		{
			LeaveOptics();
			return false;
		}

		// The eye point and its direction are in the sight's model space
		vector sightTransform[4];
		sight.GetTransform(sightTransform);
		vector eyePosition = eyePoint.Multiply4(sightTransform);
		vector ahead = eyePoint + eyeDirection;
		vector direction = (ahead.Multiply4(sightTransform) - eyePosition).Normalized();
		position = eyePosition;
		angles = direction.VectorToAngles();

		if (sight == optic && sights == EBRSpectateSights.OPTICS)
		{
			ShowOptic(target, optic);
			fov = optic.GetCurrentStepFOV();
			nearPlane = Math.Max(optic.GetNearPlaneValue(), NEAR_PLANE_OPTICS);
		}
		else
		{
			LeaveOptics();
			fov = GameConstants.DZPLAYER_CAMERA_FOV_IRONSIGHTS;
			nearPlane = NEAR_PLANE_EYES;
		}
		return true;
	}

	//! Shows an optic that does not magnify as its owner sees it, as the vanilla optics camera does for one
	//! (DayZPlayerCameraOptics.OnActivate): its own hidden parts and the clothing that would cover the view. Undone by
	//! LeaveOptics
	protected void ShowOptic(Human target, ItemOptics optic)
	{
		if (m_ShownOptic == optic)
			return;

		LeaveOptics();
		m_ShownOptic = optic;
		optic.OnOpticEnter();
		HideClothing(target);
	}

	protected void LeaveOptics()
	{
		if (!m_ShownOptic && m_HiddenClothingItems.Count() == 0)
			return;

		ShowHidden(m_HiddenClothingItems);
		if (m_ShownOptic)
		{
			m_ShownOptic.OnOpticExit();
			m_ShownOptic.UpdateSelectionVisibility();
		}
		m_ShownOptic = null;
	}

	//! Hides the watched player's head and what it wears on it (headgear, mask, eyewear, with their attachments) on this
	//! client, as the vanilla first person camera hides the player's own; refreshed every HEAD_REFRESH_SECONDS
	protected void HideHead(Human target, float timeSlice)
	{
		m_HiddenHeadAge += timeSlice;
		if (m_HiddenHeadTarget == target && m_HiddenHeadAge < HEAD_REFRESH_SECONDS)
			return;

		ShowHead();
		m_HiddenHeadTarget = target;
		m_HiddenHeadAge = 0;

		GameInventory inventory = target.GetInventory();
		if (!inventory)
			return;

		HideRecursive(inventory.FindPlaceholderForSlot(InventorySlots.GetSlotIdFromString("Head")), m_HiddenHeadItems);
		HideRecursive(inventory.FindAttachment(InventorySlots.HEADGEAR), m_HiddenHeadItems);
		HideRecursive(inventory.FindAttachment(InventorySlots.MASK), m_HiddenHeadItems);
		HideRecursive(inventory.FindAttachment(InventorySlots.EYEWEAR), m_HiddenHeadItems);
	}

	//! Hides what the watched player carries on its back, shoulder and belt on this client, as the vanilla optics camera
	//! hides the player's own behind an optic that does not magnify (PlayerBase.HideClothing); until LeaveOptics
	protected void HideClothing(Human target)
	{
		GameInventory inventory = target.GetInventory();
		if (!inventory)
			return;

		HideRecursive(inventory.FindAttachment(InventorySlots.BACK), m_HiddenClothingItems);
		HideRecursive(inventory.FindAttachment(InventorySlots.SHOULDER), m_HiddenClothingItems);
		HideRecursive(inventory.FindAttachment(InventorySlots.MELEE), m_HiddenClothingItems);
	}

	protected void HideRecursive(EntityAI item, array<EntityAI> hidden)
	{
		if (!item)
			return;

		item.SetInvisible(true);
		hidden.Insert(item);

		GameInventory inventory = item.GetInventory();
		if (!inventory)
			return;

		int count = inventory.AttachmentCount();
		for (int i = 0; i < count; i++)
			HideRecursive(inventory.GetAttachmentFromIndex(i), hidden);
	}

	//! Shows again every entity HideHead hid, even when it has left the head since
	protected void ShowHead()
	{
		ShowHidden(m_HiddenHeadItems);
		m_HiddenHeadTarget = null;
	}

	//! Shows again every entity of the list, even when it has moved since, and empties it
	protected void ShowHidden(array<EntityAI> hidden)
	{
		foreach (EntityAI item : hidden)
		{
			if (item)
				item.SetInvisible(false);
		}
		hidden.Clear();
	}

	protected void FollowThirdPerson(Human target)
	{
		LeaveOptics();
		ShowHead();
		SetFOV(GetDayZGame().GetUserFOV());
		SetNearPlane(NEAR_PLANE_ORBIT);

		// A new watched player: the orbit starts behind it, where it is heading
		if (target != m_OrbitTarget)
		{
			m_OrbitTarget = target;
			m_OrbitYaw = target.GetOrientation()[0];
		}

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

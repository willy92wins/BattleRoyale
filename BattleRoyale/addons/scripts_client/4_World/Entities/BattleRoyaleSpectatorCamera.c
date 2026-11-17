//! Camera of a dead player who watches the players still alive (created by the server with SelectSpectator).
//! First person recreates what the watched player sees: from its eyes along its head, along the barrel of a weapon it
//! raises, and through the iron sights or the optic it looks through, which its own client reports (SpectateSights RPC).
//! Its head and what it wears on it are hidden on this client meanwhile, as the vanilla first person camera hides the
//! player's own. Third person orbits behind the player with the mouse. The vanilla person view key switches between them.
class BattleRoyaleSpectatorCamera: Camera
{
	static bool s_ThirdPerson;

	//! Eyes in the head bone's frame (x up, y forward), where the vanilla first person camera sits (DayZPlayerCamera1stPerson)
	protected static const float EYE_UP = 0.04;
	protected static const float EYE_FORWARD = 0.04;
	//! Near planes of the vanilla cameras: first person and iron sights, the least for an optic, and the default
	protected static const float NEAR_PLANE_EYES = 0.04;
	protected static const float NEAR_PLANE_OPTICS = 0.06;
	protected static const float NEAR_PLANE_ORBIT = 0.07;
	//! Sandstorm intensity above which an optic's night vision takes its sandstorm variant
	//! (DayZPlayerCameraBase.SANDSTORM_NVG_THRESHOLD)
	protected static const float SANDSTORM_NIGHT_VISION_THRESHOLD = 0.15;
	//! Seconds between two refreshes of the hidden head, so that a helmet put on meanwhile is hidden too
	protected static const float HEAD_REFRESH_SECONDS = 1.0;

	protected float m_UpdateAccumulator;
	protected float m_MissingTarget;
	protected float m_FrameTimeSlice;
	protected float m_OrbitYaw;
	protected float m_OrbitPitch = -15;
	protected int m_HeadBoneIndex = -1;
	protected string m_HeadBoneTargetType;
	protected Human m_HeadBoneTarget;
	protected ref HumanMovementState m_TargetMovement = new HumanMovementState();

	//! Optic of the watched player shown as its owner sees it, the weapon whose barrel that hides, and every entity of the
	//! watched player's clothing hidden for it, until LeaveOptics
	protected ItemOptics m_ShownOptic;
	protected Weapon_Base m_HiddenBarrelWeapon;
	protected ref array<EntityAI> m_HiddenClothingItems = new array<EntityAI>;
	//! Effects this camera started on this client: the optic's mask and lens, and its night vision mode (-1 for none).
	//! They belong to the client, not to the optic, so they stop even when the optic or the weapon is gone
	protected bool m_OpticEffects;
	protected int m_OpticNightVision = -1;
	//! Watched player whose head is hidden, every entity hidden for it, and the seconds since they were hidden, until ShowHead
	protected Human m_HiddenHeadTarget;
	protected ref array<EntityAI> m_HiddenHeadItems = new array<EntityAI>;
	protected float m_HiddenHeadAge;

	void BattleRoyaleSpectatorCamera()
	{
		SetEventMask(EntityEvent.FRAME | EntityEvent.POSTFRAME);
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
			if (s_ThirdPerson)
				FollowThirdPerson(target);
			else
				FollowFirstPerson(target, timeSlice);
		}
		else
		{
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

	protected void FollowFirstPerson(Human target, float timeSlice)
	{
		HideHead(target, timeSlice);

		if (FollowSights(target))
			return;

		LeaveOptics();

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

		// The eyes look where the head looks. A raised weapon aims along its barrel, which follows the aim up and down; a
		// lowered one points at the ground or across the body, so the view stays with the head then
		vector direction = head[1].Normalized();
		target.GetMovementState(m_TargetMovement);
		Weapon_Base weapon;
		if (m_TargetMovement.IsRaised() && Class.CastTo(weapon, target.GetHumanInventory().GetEntityInHands()))
		{
			vector weaponTransform[4];
			weapon.GetTransform(weaponTransform);
			vector barrelStart = weapon.GetSelectionPositionLS("konec hlavne").Multiply4(weaponTransform);
			vector barrelEnd = weapon.GetSelectionPositionLS("usti hlavne").Multiply4(weaponTransform);
			if (vector.DistanceSq(barrelStart, barrelEnd) > 0.0001)
				direction = (barrelEnd - barrelStart).Normalized();
		}

		SetPosition(head[3] + head[0] * EYE_UP + head[1] * EYE_FORWARD);
		SetOrientation(direction.VectorToAngles());
		SetFOV(GetDayZGame().GetUserFOV());
		SetNearPlane(NEAR_PLANE_EYES);

		// Third person starts behind the player when switching
		m_OrbitYaw = direction.VectorToAngles()[0];
	}

	//! Through the sights the watched player looks through: the camera sits at their eye point, as the vanilla iron sights
	//! and optics cameras do (DayZPlayerCameraIronsights.GetCurrentSightInfo, DayZPlayerCameraOptics), with their field of
	//! view and near plane, and an optic shows its lens and mask. False when the player looks through none of them
	protected bool FollowSights(Human target)
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
		if (sights == EBRSpectateSights.OPTICS && optic)
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
		SetPosition(eyePosition);
		SetOrientation(direction.VectorToAngles());
		m_OrbitYaw = direction.VectorToAngles()[0];

		if (sight == optic && sights == EBRSpectateSights.OPTICS)
		{
			// The magnification its owner chose; the optic's own on this client when the report has none
			float fov = BattleRoyaleRPC.s_SpectateSightsFov;
			if (fov <= 0)
				fov = optic.GetCurrentStepFOV();

			ShowOptic(target, optic, weapon, fov);
			SetFOV(fov);
			SetNearPlane(Math.Max(optic.GetNearPlaneValue(), NEAR_PLANE_OPTICS));
		}
		else
		{
			LeaveOptics();
			SetFOV(GameConstants.DZPLAYER_CAMERA_FOV_IRONSIGHTS);
			SetNearPlane(NEAR_PLANE_EYES);
		}
		return true;
	}

	//! Shows the optic as its owner sees it, as the vanilla optics camera does (DayZPlayerCameraOptics.SetCameraPP): its own
	//! hidden parts, the clothing that would cover the view, its night vision and, for a magnifying optic, the barrel that
	//! would cover the view and the lens and mask effects. Undone by LeaveOptics
	protected void ShowOptic(Human target, ItemOptics optic, Weapon_Base weapon, float fov)
	{
		if (m_ShownOptic != optic)
		{
			LeaveOptics();
			m_ShownOptic = optic;
			optic.OnOpticEnter();
			HideClothing(target, fov);

			// A sight that allows depth of field (a reflex sight) is not a magnifying optic: no lens, mask or hidden barrel
			if (!optic.AllowsDOF())
			{
				if (weapon)
				{
					weapon.HideWeaponBarrel(true);
					m_HiddenBarrelWeapon = weapon;
				}

				PPERequester_CameraADS requester = PPERequester_CameraADS.Cast(PPERequesterBank.GetRequester(PPERequester_CameraADS));
				if (requester)
				{
					array<float> mask = new array<float>;
					array<float> lens = new array<float>;
					if (optic.GetOpticsPPMask() && optic.GetOpticsPPMask().Count() == 4)
						mask.Copy(optic.GetOpticsPPMask());
					if (optic.GetOpticsPPLens() && optic.GetOpticsPPLens().Count() == 4)
						lens.Copy(optic.GetOpticsPPLens());
					requester.SetValuesOptics(mask, lens, optic.GetOpticsPPBlur());
					m_OpticEffects = true;
				}
			}
		}

		// Night vision follows the optic's mode and power, and the watched player's surroundings, for as long as it is
		// looked through
		int nightVision = -1;
		if (optic.IsNVOptic())
			nightVision = OpticNightVisionMode(ResolveOpticNightVisionType(optic.GetCurrentNVType(), PlayerBase.Cast(target)));
		SetOpticNightVision(nightVision);
	}

	//! An optic's night vision type upgraded for the watched player's surroundings, underground or in a sandstorm, as
	//! DayZPlayerCameraBase.ResolveNVType does for the optic's owner. Being underground comes from the owner's report
	//! (SpectateSights): the underground presence of a remote player stays on its own client
	protected static int ResolveOpticNightVisionType(int rawType, PlayerBase player)
	{
		bool night = rawType == NVTypes.NV_OPTICS_ON || rawType == NVTypes.NV_OPTICS_KAZUAR_NIGHT || rawType == NVTypes.NV_OPTICS_STARLIGHT_NIGHT;
		bool day = rawType == NVTypes.NV_OPTICS_KAZUAR_DAY || rawType == NVTypes.NV_OPTICS_STARLIGHT_DAY;
		if ((!night && !day) || !player)
			return rawType;

		bool underground = BattleRoyaleRPC.s_SpectateSightsUnderground;
		bool sandstorm = false;
		Weather weather = GetGame().GetWeather();
		if (weather && weather.GetSandstorm() && weather.GetSandstorm().IsActive())
			sandstorm = weather.GetSandstorm().GetIntensity(player.GetPosition()) > SANDSTORM_NIGHT_VISION_THRESHOLD;

		if (underground && sandstorm)
		{
			if (night)
				return NVTypes.NV_OPTICS_UNDERGROUND_SANDSTORM;
			return NVTypes.NV_OPTICS_UNDERGROUND_SANDSTORM_DAY;
		}
		if (underground)
		{
			if (night)
				return NVTypes.NV_OPTICS_UNDERGROUND;
			return NVTypes.NV_OPTICS_UNDERGROUND_DAY;
		}
		if (sandstorm)
		{
			if (night)
				return NVTypes.NV_OPTICS_SANDSTORM_NIGHT;
			return NVTypes.NV_OPTICS_SANDSTORM_DAY;
		}
		return rawType;
	}

	//! The camera night vision mode (PPERequester_CameraNV) of an optic's night vision type, as
	//! DayZPlayerCameraBase.SetNVPostprocess picks it; -1 for none
	protected static int OpticNightVisionMode(int nvType)
	{
		switch (nvType)
		{
			case NVTypes.NV_OPTICS_ON:
			case NVTypes.NV_OPTICS_KAZUAR_NIGHT:
			case NVTypes.NV_OPTICS_STARLIGHT_NIGHT:
				return PPERequester_CameraNV.NV_DEFAULT_OPTICS;
			case NVTypes.NV_OPTICS_KAZUAR_DAY:
			case NVTypes.NV_OPTICS_STARLIGHT_DAY:
			case NVTypes.NV_OPTICS_SANDSTORM_DAY:
				return PPERequester_CameraNV.NV_DAYTIME_OPTICS;
			case NVTypes.NV_OPTICS_SANDSTORM_NIGHT:
				return PPERequester_CameraNV.NV_SANDSTORM_OPTICS;
			case NVTypes.NV_OPTICS_UNDERGROUND:
				return PPERequester_CameraNV.NV_UNDERGROUND_OPTICS;
			case NVTypes.NV_OPTICS_UNDERGROUND_DAY:
				return PPERequester_CameraNV.NV_UNDERGROUND_DAYTIME_OPTICS;
			case NVTypes.NV_OPTICS_UNDERGROUND_SANDSTORM:
				return PPERequester_CameraNV.NV_UNDERGROUND_SANDSTORM_OPTICS;
			case NVTypes.NV_OPTICS_UNDERGROUND_SANDSTORM_DAY:
				return PPERequester_CameraNV.NV_UNDERGROUND_SANDSTORM_DAYTIME_OPTICS;
			case NVTypes.NV_OPTICS_OFF:
				return PPERequester_CameraNV.NV_NO_BATTERY;
		}
		return -1;
	}

	protected void SetOpticNightVision(int mode)
	{
		if (mode == m_OpticNightVision)
			return;

		m_OpticNightVision = mode;
		PPERequesterBase requester = PPERequesterBank.GetRequester(PPERequester_CameraNV);
		if (!requester)
			return;

		if (mode == -1)
			requester.Stop();
		else
			requester.Start(new Param1<int>(mode));
	}

	protected void LeaveOptics()
	{
		if (!m_ShownOptic && !m_HiddenBarrelWeapon && !m_OpticEffects && m_OpticNightVision == -1 && m_HiddenClothingItems.Count() == 0)
			return;

		ShowHidden(m_HiddenClothingItems);

		if (m_ShownOptic)
		{
			// A night vision optic hides its pilot cover on entering too; its visibility update shows it again
			m_ShownOptic.OnOpticExit();
			m_ShownOptic.UpdateSelectionVisibility();
		}

		// Shown again whatever optic the weapon holds now: HideWeaponBarrel(false) needs a magnifying one still attached
		if (m_HiddenBarrelWeapon)
			m_HiddenBarrelWeapon.BR_ShowWeaponBarrel();

		// The effects belong to this client, not to the optic: they stop even when the optic or the weapon is gone
		if (m_OpticEffects)
		{
			PPERequesterBase requester = PPERequesterBank.GetRequester(PPERequester_CameraADS);
			if (requester)
				requester.Stop();
		}
		SetOpticNightVision(-1);

		m_ShownOptic = null;
		m_HiddenBarrelWeapon = null;
		m_OpticEffects = false;
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

	//! Hides what the watched player carries on its back, shoulder and belt and, behind a magnifying optic, its top and vest
	//! on this client, as the vanilla optics camera hides the player's own (PlayerBase.HideClothing); until LeaveOptics
	protected void HideClothing(Human target, float fov)
	{
		GameInventory inventory = target.GetInventory();
		if (!inventory)
			return;

		HideRecursive(inventory.FindAttachment(InventorySlots.BACK), m_HiddenClothingItems);
		HideRecursive(inventory.FindAttachment(InventorySlots.SHOULDER), m_HiddenClothingItems);
		HideRecursive(inventory.FindAttachment(InventorySlots.MELEE), m_HiddenClothingItems);
		if (fov < GameConstants.DZPLAYER_CAMERA_FOV_IRONSIGHTS)
		{
			HideRecursive(inventory.FindAttachment(InventorySlots.BODY), m_HiddenClothingItems);
			HideRecursive(inventory.FindAttachment(InventorySlots.VEST), m_HiddenClothingItems);
		}
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

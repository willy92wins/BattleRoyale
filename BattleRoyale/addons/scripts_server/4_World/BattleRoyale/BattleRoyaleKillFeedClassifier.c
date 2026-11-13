#ifdef SERVER
//! One death as the kill feed shows it; filled by BattleRoyaleKillFeedClassifier.Classify
class BattleRoyaleKillFeedEntry
{
	int m_Type = EBRKillFeedType.DIED;  //!< EBRKillFeedType
	int m_Flags;                        //!< BRKillFeedFlags
	string m_KillerName;                //!< empty when the death names nobody
	string m_ItemClass;                 //!< class name of the weapon, explosive, vehicle or animal; empty when none
	int m_Distance;                     //!< whole metres between killer and victim; 0 when it does not apply
	string m_ActivatorId;               //!< plain id of who set off the explosive; the caller turns it into a name
}

//! Decides the cause of a player's death from the kill source, the damage that was being applied and the
//! state of the victim. It only reads; the caller sends the result
class BattleRoyaleKillFeedClassifier
{
	//! A damage older than this at the time of death did not cause it
	static const int LETHAL_DAMAGE_WINDOW_MS = 1000;
	//! Same, for the damage tick taken outside the play area
	static const int ZONE_DAMAGE_WINDOW_MS = 1000;

	static const string AMMO_FALL_PREFIX = "FallDamage";  //!< DayZPlayerImplementFallDamage.FALL_DAMAGE_AMMO_*
	static const string AMMO_FIRE = "FireDamage";         //!< area damage of fireplaces and burning objects
	static const string AMMO_BARBED_WIRE = "BarbedWireHit";  //!< area damage of BarbedWire
	static const string AMMO_TRANSPORT = "TransportHit";  //!< EntityAI: body hit by a moving vehicle

	//! source is what EEKilled named as killer: null or the victim for a death without one.
	//! bleed_out is true when the death is a blood loss credited to the last player who hit the victim
	static void Classify(PlayerBase victim, Object source, bool bleed_out, BattleRoyaleKillFeedEntry entry)
	{
		if (!victim)
			return;

		int now = GetGame().GetTime();

		// Damage that was being applied when the victim died, if any
		bool lethal_damage = victim.br_last_damage_time > 0 && now - victim.br_last_damage_time <= LETHAL_DAMAGE_WINDOW_MS;
		string ammo = "";
		string zone = "";
		int damage_type = -1;
		if (lethal_damage)
		{
			ammo = victim.br_last_damage_ammo;
			zone = victim.br_last_damage_zone;
			damage_type = victim.br_last_damage_type;
		}

		EntityAI source_ai = EntityAI.Cast(source);

		// Player holding the source, or the source itself when it is a player (bare hands)
		PlayerBase holder;
		if (source_ai)
			holder = PlayerBase.Cast(source_ai.GetHierarchyParent());
		if (!holder)
			holder = PlayerBase.Cast(source);

		if (victim.CommitedSuicide())
		{
			entry.m_Type = EBRKillFeedType.SUICIDE;
			if (source_ai && source_ai != victim && holder == victim)
				entry.m_ItemClass = source_ai.GetType();
			return;
		}

		if (bleed_out)
		{
			entry.m_Type = EBRKillFeedType.BLEED_OUT;
			entry.m_KillerName = victim.br_last_player_hit_name;
			entry.m_ItemClass = victim.br_last_player_hit_weapon;
			entry.m_Distance = Math.Round(victim.br_last_player_hit_distance);
			return;
		}

		Grenade_Base grenade = Grenade_Base.Cast(source);
		LandMineTrap mine = LandMineTrap.Cast(source);
		if (grenade || mine)
		{
			entry.m_Type = EBRKillFeedType.EXPLOSIVE;
			entry.m_ItemClass = source.GetType();
			if (grenade)
				entry.m_ActivatorId = grenade.GetActivatorId();
			else
				entry.m_ActivatorId = mine.GetActivatorId();
			return;
		}

		if (!source || source == victim || holder == victim)
		{
			entry.m_Type = ClassifyWithoutKiller(victim, ammo, damage_type, now);
			if (entry.m_Type == EBRKillFeedType.VEHICLE_CRASH)
				entry.m_ItemClass = GetVehicleClass(victim);
			return;
		}

		Transport transport = Transport.Cast(source);
		if (transport)
		{
			entry.m_ItemClass = source.GetType();
			if (transport.CrewMemberIndex(victim) >= 0)
			{
				entry.m_Type = EBRKillFeedType.VEHICLE_CRASH;
				return;
			}

			entry.m_Type = EBRKillFeedType.VEHICLE;
			PlayerBase driver = PlayerBase.Cast(transport.CrewDriver());
			if (driver && driver != victim && driver.GetIdentity())
				entry.m_KillerName = driver.GetIdentity().GetName();
			return;
		}

		if (holder && holder.GetIdentity())
		{
			entry.m_KillerName = holder.GetIdentity().GetName();

			if (source == holder && !lethal_damage)
			{
				// Not a hit: the victim left the game unconscious and the server names who knocked it out.
				// The feed shows the last hit that player landed
				entry.m_Type = EBRKillFeedType.FISTS;
				if (victim.br_last_player_hit_by == holder)
				{
					entry.m_Type = victim.br_last_player_hit_type;
					entry.m_ItemClass = victim.br_last_player_hit_weapon;
					entry.m_Distance = Math.Round(victim.br_last_player_hit_distance);
				}
				return;
			}

			if (source == holder)
			{
				entry.m_Type = EBRKillFeedType.FISTS;
				return;
			}

			entry.m_ItemClass = source.GetType();
			if (source.IsWeapon() && damage_type != DamageType.CLOSE_COMBAT)
			{
				entry.m_Type = EBRKillFeedType.FIREARM;
				entry.m_Distance = Math.Round(vector.Distance(victim.GetPosition(), holder.GetPosition()));
				if (zone == "Brain" || zone == "Head")
					entry.m_Flags = entry.m_Flags | BRKillFeedFlags.HEADSHOT;
			}
			else
			{
				// Any other item in the hands, or a firearm used to strike
				entry.m_Type = EBRKillFeedType.MELEE;
			}
			return;
		}

		if (source_ai && source_ai.IsZombie())
		{
			entry.m_Type = EBRKillFeedType.INFECTED;
			return;
		}

		if (source_ai && source_ai.IsAnimal())
		{
			entry.m_Type = EBRKillFeedType.ANIMAL;
			entry.m_ItemClass = source.GetType();
			return;
		}

		// Any other object: a fireplace, a gas canister, a placed explosive...
		entry.m_Type = ClassifyByDamage(ammo, damage_type);
		if (ExplosivesBase.Cast(source))
			entry.m_Type = EBRKillFeedType.EXPLOSIVE;
		if (entry.m_Type == EBRKillFeedType.EXPLOSIVE)
			entry.m_ItemClass = source.GetType();
	}

	//! Cause named by the damage alone; DIED when it names none
	protected static int ClassifyByDamage(string ammo, int damage_type)
	{
		if (ammo.IndexOf(AMMO_FALL_PREFIX) == 0)
			return EBRKillFeedType.FALL;

		if (ammo == AMMO_FIRE)
			return EBRKillFeedType.FIRE;

		if (ammo == AMMO_BARBED_WIRE)
			return EBRKillFeedType.BARBED_WIRE;

		if (ammo == AMMO_TRANSPORT)
			return EBRKillFeedType.VEHICLE;

		if (damage_type == DamageType.EXPLOSION)
			return EBRKillFeedType.EXPLOSIVE;

		return EBRKillFeedType.DIED;
	}

	//! Cause of a death that names no killer, from the most to the least specific evidence
	protected static int ClassifyWithoutKiller(PlayerBase victim, string ammo, int damage_type, int now)
	{
		// A zone tick is not a damage hit, so the later of the two is the one that killed
		bool zone_tick = victim.br_zone_damage_time > 0 && now - victim.br_zone_damage_time <= ZONE_DAMAGE_WINDOW_MS;
		if (zone_tick && victim.br_zone_damage_time >= victim.br_last_damage_time)
			return EBRKillFeedType.ZONE;

		int by_damage = ClassifyByDamage(ammo, damage_type);
		if (by_damage != EBRKillFeedType.DIED)
			return by_damage;

		if (zone_tick)
			return EBRKillFeedType.ZONE;

		if (victim.br_bled_out)
			return EBRKillFeedType.BLEED_OUT;

		// A crash kills the crew by setting its health, without a damage hit (CarScript.DamageCrew),
		// so a death with no other cause while seated is read as one
		if (victim.GetCommand_Vehicle())
			return EBRKillFeedType.VEHICLE_CRASH;

		if (victim.GetDrowningWaterLevelCheck())
			return EBRKillFeedType.DROWNED;

		return EBRKillFeedType.DIED;
	}

	//! Class name of the vehicle the player sits in, empty when on foot
	protected static string GetVehicleClass(PlayerBase player)
	{
		HumanCommandVehicle command = player.GetCommand_Vehicle();
		if (!command)
			return "";

		Transport transport = command.GetTransport();
		if (!transport)
			return "";

		return transport.GetType();
	}
}
#endif

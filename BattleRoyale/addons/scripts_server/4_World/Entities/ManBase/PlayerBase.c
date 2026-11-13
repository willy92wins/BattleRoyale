#ifdef SERVER
modded class PlayerBase
{
    float time_until_heal = 0;
    float time_until_damage = 0;
    float time_until_move = 0;

    bool allow_fade = false;

    float time_between_net_sync = 1000;
    float time_since_last_net_sync = 0;
    bool force_result = true;

    bool wait_unstuck = false;

    string owner_id = "";

	int br_position = -1;
	string player_steamid = "";
	bool br_in_current_state = false;

	vector spawn_pos = vector.Zero;

	PlayerBase last_unconscious_source;
	float m_UnconsciousStartTime;

	// Last hit taken from another player, kept to credit that player with a later bleed out
	PlayerBase br_last_player_hit_by;
	string br_last_player_hit_name;  //!< empty until another player hits this one
	int br_last_player_hit_time;  //!< GetGame().GetTime() of that hit, in ms
	string br_last_player_hit_weapon;  //!< class name of the weapon, empty for fists
	int br_last_player_hit_type;  //!< EBRKillFeedType of that hit: FIREARM, MELEE or FISTS
	float br_last_player_hit_distance;  //!< metres between the two players at the hit, 0 in melee
	bool br_bled_out;  //!< set when this player dies: its blood was under the fatal level, so the death was blood loss

	// Damage being applied, written right before the engine applies it and forgotten once the player has
	// survived it, so that at death it is the damage that killed. The kill feed reads it to tell a fall,
	// a fire or an explosion apart
	int br_last_damage_time;  //!< GetGame().GetTime() of that damage, in ms; 0 when there is none
	int br_last_damage_type;  //!< DamageType
	string br_last_damage_ammo;  //!< ammo name, such as "FallDamageHealth" or "TransportHit"
	string br_last_damage_zone;  //!< damage zone, such as "Brain" or "Torso"
	int br_zone_damage_time;  //!< GetGame().GetTime() of the damage tick outside the play area that killed this player; 0 when none did

	//! Damage tick taken outside the play area. The stamp stays only when the tick killed, so that the
	//! kill feed names the zone for that death and for no other
	void BR_ZoneDamage( float damage_coef )
	{
		br_zone_damage_time = GetGame().GetTime();
		DecreaseHealthCoef( damage_coef );
		if ( BR_IsAlive() )
			br_zone_damage_time = 0;
	}

	//! True while the player is neither destroyed nor out of health. Either sign alone means dead: which
	//! of the two the engine updates first when a damage kills is not documented
	bool BR_IsAlive()
	{
		return IsAlive() && GetHealth( "", "" ) > 0;
	}

	void SetBRPosition( int position )
	{
		br_position = position;
	}

	int GetBRPosition()
	{
		return br_position;
	}

    override void OnScheduledTick(float deltaTime)
    {
        super.OnScheduledTick(deltaTime);

        BattleRoyaleBase m_BR = GetBR();
        if(m_BR)
            m_BR.OnPlayerTick(this, deltaTime);
    }

    override void EEKilled( Object killer )
    {
        // Vanilla kills a player through blood loss when the blood falls under BLOOD_THRESHOLD_FATAL
        // (BleedingCheckMdfr). An open wound alone does not make the death a bleed out
        br_bled_out = GetHealth( "", "Blood" ) < PlayerConstants.BLOOD_THRESHOLD_FATAL;

        super.EEKilled( killer );

        BattleRoyaleBase m_BR = GetBR();
        if(m_BR)
            m_BR.OnPlayerKilled(this, killer);
    }

    override bool EEOnDamageCalculated(TotalDamageResult damageResult, int damageType, EntityAI source, int component, string dmgZone, string ammo, vector modelPos, float speedCoef)
    {
        bool apply = super.EEOnDamageCalculated(damageResult, damageType, source, component, dmgZone, ammo, modelPos, speedCoef);
        if (apply)
        {
            br_last_damage_time = GetGame().GetTime();
            br_last_damage_type = damageType;
            br_last_damage_ammo = ammo;
            br_last_damage_zone = dmgZone;
        }

        return apply;
    }

    override void EEHitBy(TotalDamageResult damageResult, int damageType, EntityAI source, int component, string dmgZone, string ammo, vector modelPos, float speedCoef)
	{
		super.EEHitBy(damageResult, damageType, source, component, dmgZone, ammo, modelPos, speedCoef);

		// Survived: that damage will not be the cause of a later death
		if ( BR_IsAlive() )
			br_last_damage_time = 0;

		if (!source)
		{
			last_unconscious_source = NULL;
			return;
		}

		PlayerBase playerSource;

		if ( source.IsPlayer() )  // Fists
			playerSource = PlayerBase.Cast( source );
		else
			playerSource = PlayerBase.Cast( source.GetHierarchyParent() );

		// Test if the source is a player
		if ( playerSource )
		{
			last_unconscious_source = playerSource;

			if ( playerSource != this && playerSource.GetIdentity() )
			{
				br_last_player_hit_by = playerSource;
				string hit_name = playerSource.BR_GetPlayerName();
				if ( hit_name == "" )
					hit_name = playerSource.GetIdentity().GetName();
				br_last_player_hit_name = hit_name;
				br_last_player_hit_time = GetGame().GetTime();
				br_last_player_hit_weapon = "";
				br_last_player_hit_type = EBRKillFeedType.FISTS;
				br_last_player_hit_distance = 0;
				if ( source != playerSource )
				{
					br_last_player_hit_weapon = source.GetType();
					br_last_player_hit_type = EBRKillFeedType.MELEE;
				}
				// A firearm used to strike stays a melee hit
				if ( source.IsWeapon() && damageType != DamageType.CLOSE_COMBAT )
				{
					br_last_player_hit_type = EBRKillFeedType.FIREARM;
					br_last_player_hit_distance = vector.Distance( GetPosition(), playerSource.GetPosition() );
				}
			}
		} else {
			last_unconscious_source = NULL;
			string victim_name = "unknown";
			if (GetIdentity())
				victim_name = GetIdentity().GetName();
			BattleRoyaleUtils.Info("Player " + victim_name + " was hit by an unknown source.");
		}
	}

	override void OnUnconsciousStop(int pCurrentCommandID)
	{
		super.OnUnconsciousStop(pCurrentCommandID);

		if (last_unconscious_source)
		{
			last_unconscious_source = NULL;
		}
	}

	override void OnSyncJuncture( int pJunctureID, ParamsReadContext pCtx )
	{
		super.OnSyncJuncture( pJunctureID, pCtx );

		switch( pJunctureID )
		{
			case 88:
				vector position, direction;
				pCtx.Read( position );
				pCtx.Read( direction );
				if ( position )
				{
					SetDynamicPhysicsLifeTime( 0.001 );
					dBodyActive( this, ActiveState.INACTIVE );
					SetPosition( position );
					dBodyActive( this, ActiveState.ACTIVE );
				}
				if ( direction )
				{
					SetDirection( direction );
				}
				break;
		}
	}

    //Temp fix for disabling character saving
    override bool Save()
    {
        return false;
    }

    void Heal()
    {
        //NOTE: this heal function was done by legodev, not sure it's performance, we'll have to see

        SetHealth("", "Health", GetMaxHealth("", "Health"));
        SetHealth("", "Blood", GetMaxHealth("", "Blood"));
        SetHealth("", "Shock", GetMaxHealth("", "Shock"));

        // GetStatStomachVolume + GetStatStomachWater > 1000 == STUFFED!

        //SetBleedingBits(0);
        if ( m_BleedingManagerServer )
            m_BleedingManagerServer.RemoveAllSources();

        //--- legacy function (need to access m_PlayerStomach [PlayerStomach] and try from there)
        //GetStatStomachVolume().Set(250);
        //GetStatStomachWater().Set(250);

        // for bone regen: water = 2500 and energy = 4000 so 5000 should be ok
        GetStatWater().Set(4500);
        GetStatEnergy().Set(4500);
        // is get max an good idea?
        // player.GetStatWater().Set(player.GetStatWater().GetMax());
        // player.GetStatEnergy().Set(player.GetStatEnergy().GetMax());


        // default body temperature is  37.4 -> HYPOTHERMIC_TEMPERATURE_TRESHOLD = 35.8
        //player.GetStatTemperature().Set(37.4);

        // BURNING_TRESHOLD = 199 -> 100 should be fine
        //GetStatHeatComfort().Set(100); //no temperature flashing
        //GetStatHeatBuffer().Set(25); //give players a + by default

        // seems unused
        // player.GetStatHeatIsolation().Set(100);

        // we don't want shaking -> limit is 0.008
        GetStatTremor().Set(GetStatTremor().GetMin());

        // wet if > 0.2
        GetStatWet().Set(0);

        // unknown effect, don't alter yet
        // player.GetStatStomachEnergy().Set(100);
        // player.GetStatDiet().Set(100);

        // think max stamina does not break the game
        GetStatStamina().Set(GetStatStamina().GetMax());

        // required for repairing and stuff, so no need to change for godmode
        //player.GetStatSpecialty().Set(100);
    }

    override void ResetPlayer(bool set_max)
    {
        //clear stomach content
        GetStomach().ClearContents();

        // bleeding sources
        if ( m_BleedingManagerServer )
            m_BleedingManagerServer.RemoveAllSources();

        // Modifiers
        bool hasAreaExposureModifier, hasMaksModifier;
        if (GetModifiersManager())
        {
            hasAreaExposureModifier = GetModifiersManager().IsModifierActive(eModifiers.MDF_AREAEXPOSURE);
            hasMaksModifier = GetModifiersManager().IsModifierActive(eModifiers.MDF_MASK);
            GetModifiersManager().DeactivateAllModifiers();

            if (hasAreaExposureModifier)
                GetModifiersManager().ActivateModifier(eModifiers.MDF_AREAEXPOSURE);

            if (hasMaksModifier)
                GetModifiersManager().ActivateModifier(eModifiers.MDF_MASK);
        }

        // Stats
        if (GetPlayerStats())
        {
            int bloodType = GetStatBloodType().Get();
            GetPlayerStats().ResetAllStats();
            GetStatBloodType().Set(bloodType);
        }

        if (m_StaminaHandler)
            m_StaminaHandler.SetStamina(GameConstants.STAMINA_MAX);

        // Agents
        if (m_AgentPool)
            m_AgentPool.RemoveAllAgents();

        // Damage System
        DamageZoneMap zones = new DamageZoneMap();
        DamageSystem.GetDamageZoneMap(this, zones);
        SetHealth("", "Health", GetMaxHealth("","Health"));
        SetHealth("", "Shock", GetMaxHealth("","Shock"));
        SetHealth("", "Blood", GetMaxHealth("","Blood"));

        for (int i = 0; i < zones.Count(); i++)
        {
            string zone = zones.GetKey(i);
            SetHealth(zone, "Health", GetMaxHealth(zone,"Health"));
            SetHealth(zone, "Shock", GetMaxHealth(zone,"Shock"));
            SetHealth(zone, "Blood", GetMaxHealth(zone,"Blood"));
        }

        // uncon
        if (IsUnconscious())
            DayZPlayerSyncJunctures.SendPlayerUnconsciousness(this, false);

        // set max
        if (set_max)
        {
            GetStatWater().Set(GetStatWater().GetMax());
            GetStatEnergy().Set(GetStatEnergy().GetMax());
        }

        // fix up inventory
        FixAllInventoryItems();

        //remove bloody hands
        PluginLifespan moduleLifespan = PluginLifespan.Cast(GetPlugin(PluginLifespan));
        moduleLifespan.UpdateBloodyHandsVisibilityEx(this, eBloodyHandsTypes.CLEAN);
    }

    void SetSpawnPos(vector pos)
	{
		BattleRoyaleUtils.Trace("SetSpawnPos: " + pos);
		spawn_pos = pos;
	}

	vector GetSpawnPos()
	{
		return spawn_pos;
	}
};
#endif

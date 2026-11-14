#ifdef SERVER
modded class ItemBase
{
	protected static bool m_BRSpawnAmmoConfigResolved;
	protected static bool m_BRDisableSpawnWithAmmo;
	protected static int m_BRMinSpawnAmmo;
	protected static int m_BRMaxSpawnAmmo;

	//! Config does not change at runtime: cache it by class instead of reading it for every CE weapon
	protected static ref map<string, int> m_BRMagazineCapacityCache;
	protected static ref map<string, ref array<string>> m_BRChamberableFromCache;
	//! 1 when the ammo class has an economy profile with nominal above zero, 0 when it does not
	protected static ref map<string, int> m_BRAmmoNominalCache;

	// Thanks to @incinatus for this
	override void EEOnCECreate()
	{
		super.EEOnCECreate();

		// Server config does not change at runtime: read the spawn settings once
		if( !m_BRSpawnAmmoConfigResolved )
		{
			m_BRDisableSpawnWithAmmo = GetGame().ServerConfigGetInt( "BRDisableSpawnWithAmmo" ) == 1;
			m_BRMinSpawnAmmo = GetGame().ServerConfigGetInt( "BRMinSpawnAmmo" );
			m_BRMaxSpawnAmmo = GetGame().ServerConfigGetInt( "BRMaxSpawnAmmo" );
			m_BRSpawnAmmoConfigResolved = true;
		}

		bool config_disable_spawn_with_ammo = m_BRDisableSpawnWithAmmo;
		if( IsWeapon() && !config_disable_spawn_with_ammo )
		{
//			if( Flaregun.Cast(this) ) return;  // Skip flaregun

			int min_spawn = 1;
			int max_spawn = 2;
			int config_min_spawn = m_BRMinSpawnAmmo;
			int config_max_spawn = m_BRMaxSpawnAmmo;

			// Override min/max spawn if set in server config
			// If min is set below 0, set it to 0
			// If max is set below min, set it to min
			// If max is set to 0 or below, ignore it and use default
			if( config_min_spawn > 0 )
			{
				min_spawn = config_min_spawn;
			}
			if( config_min_spawn < 0 )
			{
				min_spawn = 0;
			}

			if( config_max_spawn > 0 )
			{
				max_spawn = config_max_spawn;
			}

			// Ensure max is not lower than min
			if( max_spawn < min_spawn )
			{
				max_spawn = min_spawn;
			}

			Weapon weapon = Weapon.Cast(this);
			string magazineType = weapon.GetRandomMagazineTypeName(0);
			// Try to spawn a magazine of the weapon's compatible magazines
			if( magazineType != string.Empty )
			{
				if( !m_BRMagazineCapacityCache )
					m_BRMagazineCapacityCache = new map<string, int>();
				int spawnCount = Math.RandomIntInclusive(min_spawn, max_spawn);
				int validMagazinesSpawned = 0;
				int maxAttempts = spawnCount * 10; // Set a reasonable limit to prevent infinite loops
				int attempts = 0;

				// Keep trying until we've spawned enough valid magazines or hit the attempt limit
				while (validMagazinesSpawned < spawnCount && attempts < maxAttempts)
				{
					attempts++;
					magazineType = weapon.GetRandomMagazineTypeName(0);

					// Skip high capacity magazines (check if > 100 rounds)
					int maxMagCapacity;
					if( !m_BRMagazineCapacityCache.Find(magazineType, maxMagCapacity) )
					{
						maxMagCapacity = GetGame().ConfigGetInt("CfgMagazines " + magazineType + " count");
						m_BRMagazineCapacityCache.Insert(magazineType, maxMagCapacity);
					}
					if(maxMagCapacity > 100)
					{
						continue; // Try another magazine type without counting it as spawned
					}

					EntityAI spawnedMag = EntityAI.Cast(GetGame().CreateObjectEx(magazineType, GetPosition(), ECE_CREATEPHYSICS|ECE_UPDATEPATHGRAPH));
					if (!spawnedMag)
						continue;

					validMagazinesSpawned++;
				}
			}
			else  // No compatible magazines, try to spawn chamberable ammo piles instead
			{
				int ammoSpawnCount = Math.RandomIntInclusive(min_spawn, max_spawn);
				if( !m_BRChamberableFromCache )
					m_BRChamberableFromCache = new map<string, ref array<string>>();
				if( !m_BRAmmoNominalCache )
					m_BRAmmoNominalCache = new map<string, int>();

				//! chamberableFrom is fixed per weapon class, so read it once and share the list
				string weaponType = GetType();
				array<string> ammoTypes;
				if( m_BRChamberableFromCache.Contains(weaponType) )
				{
					ammoTypes = m_BRChamberableFromCache.Get(weaponType);
				}
				else
				{
					ammoTypes = new array<string>;
					ConfigGetTextArray("chamberableFrom", ammoTypes);
					m_BRChamberableFromCache.Insert(weaponType, ammoTypes);
				}
				if( ammoTypes.Count() > 0 )
				{
					for( int j = 0; j < ammoSpawnCount; j++ )
					{
						string ammoType = ammoTypes.GetRandomElement();

						//! A class already known to have no economy profile would only be created to be deleted
						int ammoNominal;
						bool ammoNominalKnown = m_BRAmmoNominalCache.Find(ammoType, ammoNominal);
						if( ammoNominalKnown && ammoNominal == 0 )
							continue;

						EntityAI ammoPile = EntityAI.Cast(GetGame().CreateObjectEx( ammoType, GetPosition(), ECE_CREATEPHYSICS|ECE_UPDATEPATHGRAPH ));
						if (!ammoPile)
							continue;

						if( !ammoNominalKnown )
						{
							ammoNominal = 1;
							if( ammoPile.GetEconomyProfile() == NULL || ammoPile.GetEconomyProfile().GetNominal() == 0 )
								ammoNominal = 0;
							m_BRAmmoNominalCache.Insert(ammoType, ammoNominal);

							if( ammoNominal == 0 )
							{
								// Ammo pile has no economy profile, delete it
								// We don't try to spawn one more to avoid infinite loop
								ammoPile.Delete();
							}
						}
					}
//					string ammoType = weapon.GetRandomChamberableAmmoTypeName(0);
//
//					string ammoTypeName;
//					GetGame().ConfigGetText( string.Format("CfgAmmo %1 spawnPileType", ammoType), ammoTypeName );  // Get the ammo pile type
//					// GetEconomyProfile
//					if( ammoTypeName != string.Empty )
//					{
//						EntityAI ammoPile = GetGame().CreateObjectEx( ammoTypeName, GetPosition(), ECE_CREATEPHYSICS|ECE_UPDATEPATHGRAPH );
//						if( ammoPile.GetEconomyProfile() == NULL || ammoPile.GetEconomyProfile().GetNominal() == 0 )
//						{
//							// Ammo pile has no economy profile, delete it
//							// We don't try to spawn one more to avoid infinite loop
//							ammoPile.Delete();
//						} else {
//							Print("Ammo type name: " + ammoTypeName);
//							Print(ammoPile.GetEconomyProfile().GetNominal());
//						}
//					}
				}
			}
		}
	}
}
#endif

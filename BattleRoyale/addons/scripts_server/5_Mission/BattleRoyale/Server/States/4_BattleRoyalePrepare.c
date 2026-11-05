#ifdef SERVER
class BattleRoyalePrepare: BattleRoyaleState
{
    protected ref array<PlayerBase> a_PlayerList;

    protected ref array<string> a_StartingClothes;
    protected ref array<string> a_StartingItems;
    protected ref array<string> a_AvoidCitySpawn;
    protected bool b_EnableSpawnSelectionMenu;

	private BattleRoyaleConfig m_Config;
    private BattleRoyaleGameData m_GameSettings;
    private BattleRoyaleSpawnsData m_SpawnsSettings;
    private BattleRoyaleServerData m_ServerData;
    private BattleRoyalePOIsData m_POIsSettings;

    private ref array<ref NamedLocation> villages;
    private int i_VillagesIndex;

    private string last_village_spawn = "";

    ref map<string, vector> m_OverrideSpawnPositions;
    protected ref map<string, ref array<vector>> m_SafeVillagePoints;
    protected bool m_PlayersProcessed;
    protected int m_PlayerBatch;
    protected ref Timer m_ProcessWatchdog;

    void BattleRoyalePrepare()
    {
    	m_Config = BattleRoyaleConfig.GetConfig();

        m_GameSettings = m_Config.GetGameData();
        m_SpawnsSettings = m_Config.GetSpawnsData();
        m_ServerData = m_Config.GetServerData();
        m_POIsSettings = m_Config.GetPOIsData();

		a_StartingClothes = m_GameSettings.player_starting_clothes;
		a_StartingItems = m_GameSettings.player_starting_items;
		a_AvoidCitySpawn = m_SpawnsSettings.avoid_city_spawn;
		b_EnableSpawnSelectionMenu = m_GameSettings.enable_spawn_selection_menu;

        a_PlayerList = new array<PlayerBase>();
        m_SafeVillagePoints = new map<string, ref array<vector>>();

        i_VillagesIndex = 0;
    }

    override void Activate()
    {
        super.Activate();
        RememberMatchPlayers();
        BroadcastMatchPhase(EBRMatchPhase.PREPARE, -1);

		if ( m_ServerData.enable_vigrid_api )
		{
			StartMatchWebhook matchWebhook = new StartMatchWebhook( m_ServerData.webhook_jwt_token );
			BattleRoyaleServer br_instance = BattleRoyaleServer.GetInstance();
			matchWebhook.startMatch( br_instance.match_uuid );
		}

        //TODO: spawn & setup drop plane
        GetRPCManager().SendRPC( RPC_DAYZBR_NAMESPACE, "SetFade", new Param1<bool>(true), true); //fade out screen

        //we process on a static list so when players possibly disconnect during this phase we don't error out or skip any clients
        a_PlayerList.InsertAll( m_Players ); //this gave an error when using m_Players

        GetRPCManager().SendRPC( RPC_DAYZBR_NAMESPACE, "SetInput", new Param1<bool>(true), true); //disable user input on all clients (we'll do this on the server in another thread)

        BattleRoyaleUtils.Trace("Reset date time to random");
        int year, month, day, hour, minute;
        GetGame().GetWorld().GetDate(year, month, day, hour, minute);
        GetGame().GetWorld().SetDate(year, month, day, Math.RandomIntInclusive(6, 12), 0);

		// Resend hide Spawn Selection UI RPC in case some players didn't get it
		if (b_EnableSpawnSelectionMenu)
		{
			GetRPCManager().SendRPC( RPC_DAYZBR_NAMESPACE, "HideSpawnSelection", NULL, true);
		}

        m_PlayersProcessed = false;
        m_ProcessWatchdog = AddTimer(120.0, this, "ProcessPlayersWatchdog", NULL, false);
        GetGame().GameScript.Call(this, "ProcessPlayers", NULL); //Spin up a new thread to process giving players items and teleporting them
    }

    protected void StopProcessWatchdog()
    {
        if (m_ProcessWatchdog && m_ProcessWatchdog.IsRunning())
            m_ProcessWatchdog.Stop();
        m_ProcessWatchdog = null;
    }

    void ProcessPlayersWatchdog()
    {
        if (m_PlayersProcessed)
            return;

        Error("ProcessPlayers did not finish within 120s; forcing Deactivate");
        m_PlayersProcessed = true;
        if (IsActive())
            Deactivate();
    }

    protected void SleepPlayerBatch()
    {
        m_PlayerBatch++;
        if (m_PlayerBatch >= 8)
        {
            Sleep(100);
            m_PlayerBatch = 0;
        }
    }

    protected void FlushPlayerBatch()
    {
        if (m_PlayerBatch > 0)
        {
            Sleep(100);
            m_PlayerBatch = 0;
        }
    }

    override void Deactivate()
    {
        StopProcessWatchdog();
        super.Deactivate();

        GetRPCManager().SendRPC( RPC_DAYZBR_NAMESPACE, "SetFade", new Param1<bool>(false), true); //fade out screen
    }

    override bool IsComplete()
    {
        //BattleRoyaleUtils.Trace(GetName() + " IsComplete!");
        return super.IsComplete();
    }

    override string GetName()
    {
        return "Prepare State";
    }

    protected bool DeleteAllItems(PlayerBase process_player)
    {
        if ( process_player == NULL )
            return false;

        if (process_player.GetInventory().CountInventory() > 0)
        {
            process_player.RemoveAllItems();
        }

        return true;
    }

    protected bool AddStartItems(PlayerBase process_player)
    {
        DeleteAllItems(process_player);

		array<int> player_starting_items_shortcut = m_GameSettings.player_starting_items_shortcut;

        int cCount = a_StartingClothes.Count();
        bool item_spawned = false;
        EntityAI new_item;
        for (int i = 0; i < cCount; i++)
        {
            EntityAI clothes = process_player.GetInventory().CreateAttachment(a_StartingClothes[i]);
            if(!item_spawned && clothes && clothes.GetInventory() && clothes.GetInventory().GetCargo())
            {
                int iCount = a_StartingItems.Count();
                for (int j = 0; j < iCount; j++)
                {
                    new_item = clothes.GetInventory().CreateEntityInCargo(a_StartingItems[j]);
                    if( new_item )
					{
						// set health to the maximum
						new_item.SetHealthMax();

						// If item is in the shortcut list, set it to the hotbar
						int shortcut_index = player_starting_items_shortcut.Find(j);
						if(shortcut_index != -1)
						{
							process_player.SetQuickBarEntityShortcut(new_item, shortcut_index);
						}
					}
                }
                item_spawned = true;
            }
        }

        return true;
    }

    protected void GiveStartingItems(PlayerBase process_player)
    {
        //drop the item out of the player's hands before we delete it
        if(process_player.GetItemInHands())
        {
            ItemBase item_inhands = process_player.GetItemInHands();
            process_player.ServerDropEntity( item_inhands );
            //GetGame().GameScript.Call(GetGame(), "ObjectDelete", item_inhands);
            GetGame().ObjectDelete( item_inhands );
        }
        DeleteAllItems(process_player);
        AddStartItems(process_player);
    }

    protected void DisablePlayerInput(PlayerBase process_player)
    {
        process_player.DisableInput(true);
    }

    protected NamedLocation GetRandomVillage(BattleRoyalePlayArea area = NULL, bool use_radius = false)
    {
		BattleRoyaleUtils.Trace("GetRandomVillage()");

        // https://github.com/InclementDab/DayZ-Dabs-Framework/blob/production/DabsFramework/Scripts/3_Game/DabsFramework/!Core/NamedLocation.c
        if(!villages)
        {
            villages = new array<ref NamedLocation>;

            //ref array<ref NamedLocation> temp_villages = NamedLocation.GetMapTowns(TownFlags.CAPITAL | TownFlags.CITY | TownFlags.VILLAGE | TownFlags.CAMP);

            string world_name = "";
            GetGame().GetWorldName(world_name);
            string cfg = "CfgWorlds " + world_name + " Names";
            BattleRoyaleUtils.Trace(cfg);
            for (int i = 0; i < GetGame().ConfigGetChildrenCount(cfg); i++) {
				string city;
				GetGame().ConfigGetChildName(cfg, i, city);
//				vector city_position;
				// TODO: Override city position from config file

//				vector override_position = m_POIsSettings.GetOverrodePosition( city );
//
//				if( override_position != "0 0 0" )
//				{
//					city_position = override_position;
//					BattleRoyaleUtils.Trace("Override " + city + " position!");
//				}
//				else
//				{
//					TFloatArray float_array = {};
//					GetGame().ConfigGetFloatArray(string.Format("%1 %2 position", cfg, city), float_array);
//					city_position[0] = float_array[0]; city_position[2] = float_array[1];
//					city_position[1] = GetGame().SurfaceY(city_position[0], city_position[2]);
//				}

				string town_type = GetGame().ConfigGetTextOut(string.Format("%1 %2 type", cfg, city));
				if(town_type != "Capital" && town_type != "City" && town_type != "Village")
					continue;

				BattleRoyaleUtils.Trace("cfg "+city+" "+GetGame().ConfigGetTextOut(string.Format("%1 %2 name", cfg, city))+" "+GetGame().ConfigGetTextOut(string.Format("%1 %2 type", cfg, city)));

				NamedLocation town_entry = new NamedLocation(string.Format("%1 %2", cfg, city));
//				town_entry.Entry = city;
//				town_entry.Type = GetGame().ConfigGetTextOut(string.Format("%1 %2 type", cfg, city));
//				town_entry.Name = GetGame().ConfigGetTextOut(string.Format("%1 %2 name", cfg, city));
//				town_entry.Position = city_position;

				BattleRoyaleUtils.Trace("- " + i + ". " + town_entry.Name + " (" + town_entry.Type + ")");

				if(town_entry.Name == "" || town_entry.Type == "") // useless ?
					continue;

				// Check if city Entry is not in the avoid spawn list
				if(a_AvoidCitySpawn.Find(town_entry.GetName()) != -1)
					continue;

				if(m_SpawnsSettings.spawn_in_first_zone && area != NULL)
				{
					float village_pad;

					if(use_radius)
					{
						// TODO: Move this to configuration file
						if (town_entry.Type == NamedLocation.CITY)
							village_pad = 300.0;
						else if (town_entry.Type == NamedLocation.CAPITAL)
							village_pad = 500.0;
						else
							village_pad = 150.0;
					}
					else
						village_pad = 0.0;

					// Check if city area is in the area, otherwise skip it
					if(!area.IsAreaOverlap(new BattleRoyalePlayArea(town_entry.Position, village_pad), m_SpawnsSettings.extra_spawn_radius))
						continue;
				}

				int pond = 1;
				if(town_entry.Type == NamedLocation.CAPITAL)
					pond = 5;
				else if(town_entry.Type == NamedLocation.CITY)
					pond = 3;

				for(int p = 0; p < pond; p++)
					villages.Insert(town_entry);
			}

			// Add weighting


			// Randomize order of villages
			villages.ShuffleArray();

			BattleRoyaleUtils.Trace("Final village list:");
			foreach(NamedLocation village: villages)
			{
				BattleRoyaleUtils.Trace("- " + village.Name + " (" + village.Type + ")");
			}
        }

        if(villages.Count() > 0)
        {
            if(i_VillagesIndex >= villages.Count())
                i_VillagesIndex = 0;

            return villages[i_VillagesIndex++];
        }
        else
            return NULL;
    }

    protected vector GetRandomVillagePosition(NamedLocation village)
    {
        float village_x = village.Position[0];
        float village_z = village.Position[2];
        float village_pad;

        if (village.Type == NamedLocation.CITY)
            village_pad = 300.0;
        else if (village.Type == NamedLocation.CAPITAL)
            village_pad = 500.0;
        else
            village_pad = 100.0;

        float radius, angle, x, z, y;
        // Use Math.Pow with power > 1 to concentrate points toward the center
        // Higher values (2, 3, 4) will concentrate points more heavily near center
        radius = village_pad * Math.Pow(Math.RandomFloat(0, 1), 2); // Using power of 2
		angle = Math.RandomFloat(0, 360) * Math.DEG2RAD;
        x = village_x + ( radius * Math.Cos(angle) );
        z = village_z + ( radius * Math.Sin(angle) );
        y = GetGame().SurfaceY(x, z);

        // Get the distance between the center of the village and the random point
        float distance = Math.Sqrt(Math.Pow(x - village_x, 2) + Math.Pow(z - village_z, 2));
        BattleRoyaleUtils.Trace("Distance from village center: " + distance + " (from an initial radius of " + village_pad + ")");

        BattleRoyaleUtils.Trace("Trying to spawn player to " + village.Name + " (" + village.Type + ") with a radius of " + village_pad);

        return Vector(x, y, z);
    }

    protected ref Param2<vector, NamedLocation> GetRandomSpawnPosition()
    {
        vector random_pos = "0 0 0";
        NamedLocation village;

        if (m_SpawnsSettings.spawn_in_villages)
        {
            vector village_pos = "0 0 0";
            BattleRoyaleUtils.Trace("Spawn in zone " + GetDynamicStartingZone(i_NumStartingPlayers));
            ref BattleRoyalePlayArea spawn_area = BattleRoyaleZone.GetZone(GetDynamicStartingZone(i_NumStartingPlayers)).GetArea();
            int spawn_checks = 0;
            for(int village_spawn_try = 1; village_spawn_try <= 40 && spawn_checks < 600; village_spawn_try++)
            {
                BattleRoyaleUtils.Trace("Try to spawn in village " + village_spawn_try);

                village = NULL;
                if(m_SpawnsSettings.spawn_in_first_zone)
                    village = GetRandomVillage(spawn_area, true);
                else
                    village = GetRandomVillage();

                if (village != NULL && village.Name != "")
                {
                    BattleRoyaleUtils.Trace("Found village " + village.Name);
                    village_pos = "0 0 0";
                    vector search_for_village = "0 0 0";

                    if( village.Name == last_village_spawn )
                    {
                    	BattleRoyaleUtils.Trace("Same village as last spawn, we skip it");
                    	continue;
					}

                    ref array<vector> cached_points = m_SafeVillagePoints.Get(village.Name);
                    int cached_index = -1;
                    if (cached_points && cached_points.Count() > 0)
                    {
                        cached_index = Math.RandomInt(0, cached_points.Count());
                        village_pos = cached_points.Get(cached_index);
                        BattleRoyaleUtils.Trace("Reusing cached village position " + village_pos);
                    }
                    else
                    {

					// Override village position from config file using the technical name (e.g. "Settlement_Novoselki")
					vector override_position = BattleRoyaleConfig.GetConfig().GetPOIsData().GetOverrodePosition( village.GetName() );
					if( override_position != "0 0 0" )
					{
						BattleRoyaleUtils.Trace("Override position " + override_position);
						village.Position[0] = override_position[0];
						village.Position[2] = override_position[2];
					}

                    for(int search_pos = 1; search_pos <= 15 && spawn_checks < 600; search_pos++)
                    {
                        spawn_checks++;
                        BattleRoyaleUtils.Trace("Try to find a position in village " + search_pos);
                        search_for_village = GetRandomVillagePosition(village);

                        if(!IsAcceptedSpawn(search_for_village))
                            continue;

                        if (!cached_points)
                        {
                            cached_points = new array<vector>();
                            m_SafeVillagePoints.Insert(village.Name, cached_points);
                        }
                        cached_points.Insert(search_for_village);
                        village_pos = search_for_village;
                        break;
                    }
                    }

                    if( village_pos == "0 0 0" || !IsAcceptedSpawn(village_pos) )
                    {
                        if (cached_points && cached_index >= 0 && cached_index < cached_points.Count())
                            cached_points.Remove(cached_index);
                        continue;
                    }

                    BattleRoyaleUtils.Trace("Found village position " + village_pos);
                    random_pos = village_pos; // Found a valid village position
                    if( village.Type == NamedLocation.CAPITAL || village.Type == NamedLocation.CITY )
                    	last_village_spawn = village.Name; // Save last village spawn to avoid it next time
                    break;
                } else {
                    BattleRoyaleUtils.Trace("Another fucked up village!");
                }
            }
        }

        // If at this step we always have a zero vector, try to find a random one
        if( random_pos == "0 0 0" )
        {
        	BattleRoyaleUtils.Trace("Trying to spawn at a random position");
            bool found_random = false;
            float edge_pad = 0.1;
            int world_size = GetGame().GetWorld().GetWorldSize();
            for (int random_spawn_try = 1; random_spawn_try <= 40; random_spawn_try++)
            {
                BattleRoyaleUtils.Trace("Try to spawn at random position " + random_spawn_try);

                float x = Math.RandomFloatInclusive((world_size * edge_pad), world_size - (world_size * edge_pad));
                float z = Math.RandomFloatInclusive((world_size * edge_pad), world_size - (world_size * edge_pad));
                float y = GetGame().SurfaceY(x, z);

                random_pos[0] = x;
                random_pos[1] = y;
                random_pos[2] = z;

                if(!IsAcceptedSpawn(random_pos))
                    continue;

                found_random = true;
                break;
            }

            if (!found_random)
            {
                random_pos = GetValidatedReserve();
                if (random_pos == vector.Zero)
                    Error("GetRandomSpawnPosition found no safe point and the zone-center reserve failed");
                else
                    Error("GetRandomSpawnPosition exhausted 40 attempts; using validated zone-center reserve " + random_pos.ToString());
            }
        }

        return new Param2<vector, NamedLocation>(random_pos, village);
    }

    protected vector GetRandomSafePosition(vector position = "0 0 0", float radius = 100.0)
	{
		BattleRoyaleUtils.Trace("GetRandomSafePosition() with position " + position + " and radius " + radius);

		vector random_pos = "0 0 0";
		int max_spawn_try = 50;
		for(int i = 0; i < max_spawn_try; i++)
		{
			BattleRoyaleUtils.Trace("Try to spawn at random position " + i);

			random_pos[0] = position[0] + Math.RandomFloatInclusive(-radius-i, radius+i);
			random_pos[2] = position[2] + Math.RandomFloatInclusive(-radius-i, radius+i);
			random_pos[1] = GetGame().SurfaceY(random_pos[0], random_pos[2]);

			if(!IsAcceptedSpawn(random_pos))
			{
				BattleRoyaleUtils.Trace("Position " + random_pos + " is not safe, we try again");
				random_pos = "0 0 0";  // Reset random_pos to zero vector
				continue;
			}

			break;
		}

		return random_pos;
	}

    protected bool IsOnMap(vector pos)
    {
        int world_size = GetGame().GetWorld().GetWorldSize();
        return pos[0] >= 0 && pos[2] >= 0 && pos[0] <= world_size && pos[2] <= world_size;
    }

    protected bool IsInsideExpandedFirstZone(vector pos)
    {
        BattleRoyalePlayArea area = BattleRoyaleZone.GetZone(GetDynamicStartingZone(i_NumStartingPlayers)).GetArea();
        if (!area)
            return false;

        vector center = area.GetCenter();
        float limit = area.GetRadius() + m_SpawnsSettings.extra_spawn_radius;
        float dx = pos[0] - center[0];
        float dz = pos[2] - center[2];
        return (dx * dx) + (dz * dz) <= (limit * limit);
    }

    // Final spawn: on the map, inside the first circle plus extra_spawn_radius, and clear of collision.
    protected bool IsAcceptedSpawn(vector pos)
    {
        if (pos == vector.Zero)
            return false;
        if (!IsOnMap(pos))
            return false;
        if (!IsInsideExpandedFirstZone(pos))
            return false;
        return IsSafeForTeleport(pos[0], pos[1], pos[2], false);
    }

    protected vector GetValidatedReserve()
    {
        BattleRoyalePlayArea area = BattleRoyaleZone.GetZone(GetDynamicStartingZone(i_NumStartingPlayers)).GetArea();
        if (!area)
            return vector.Zero;

        vector reserve = area.GetCenter();
        reserve[1] = GetGame().SurfaceY(reserve[0], reserve[2]);
        if (IsAcceptedSpawn(reserve))
            return reserve;

        for (int i = 0; i < 16; i++)
        {
            float radius = 5.0 + (i * 2.0);
            float angle = Math.RandomFloat(0, 360) * Math.DEG2RAD;
            vector candidate = reserve;
            candidate[0] = reserve[0] + (radius * Math.Cos(angle));
            candidate[2] = reserve[2] + (radius * Math.Sin(angle));
            candidate[1] = GetGame().SurfaceY(candidate[0], candidate[2]);
            if (IsAcceptedSpawn(candidate))
                return candidate;
        }

        return vector.Zero;
    }

    protected bool IsInsideFirstZone(vector pos)
    {
        BattleRoyalePlayArea area = BattleRoyaleZone.GetZone(GetDynamicStartingZone(i_NumStartingPlayers)).GetArea();
        if (!area)
            return false;

        vector center = area.GetCenter();
        float limit = area.GetRadius();
        float dx = pos[0] - center[0];
        float dz = pos[2] - center[2];
        return (dx * dx) + (dz * dz) <= (limit * limit);
    }

    // Growing rings inside the first zone. Collision is preferred; the last point only has to be on the map and inside the zone.
    protected vector GetLastResortSpawn()
    {
        BattleRoyalePlayArea area = BattleRoyaleZone.GetZone(GetDynamicStartingZone(i_NumStartingPlayers)).GetArea();
        if (!area)
            return vector.Zero;

        vector center = area.GetCenter();
        center[1] = GetGame().SurfaceY(center[0], center[2]);
        float zone_radius = area.GetRadius();
        vector loose = vector.Zero;

        if (IsOnMap(center) && IsInsideFirstZone(center))
            loose = center;
        if (IsAcceptedSpawn(center))
            return center;

        int rings = 12;
        if (zone_radius < 12.0)
            rings = 1;

        for (int ring = 1; ring <= rings; ring++)
        {
            float radius = zone_radius * (ring / (rings * 1.0)) * 0.95;
            for (int step = 0; step < 8; step++)
            {
                float angle = (step * 45.0) * Math.DEG2RAD;
                vector candidate = center;
                candidate[0] = center[0] + (radius * Math.Cos(angle));
                candidate[2] = center[2] + (radius * Math.Sin(angle));
                candidate[1] = GetGame().SurfaceY(candidate[0], candidate[2]);
                if (!IsOnMap(candidate) || !IsInsideFirstZone(candidate))
                    continue;
                if (loose == vector.Zero)
                    loose = candidate;
                if (IsAcceptedSpawn(candidate))
                    return candidate;
            }
        }

        return loose;
    }

    protected vector ResolveFinalSpawn(vector position)
    {
        if (IsAcceptedSpawn(position))
            return position;

        vector reserve = GetValidatedReserve();
        if (IsAcceptedSpawn(reserve))
        {
            Error("Spawn candidate rejected; using validated reserve " + reserve.ToString());
            return reserve;
        }

        vector last_resort = GetLastResortSpawn();
        if (last_resort != vector.Zero)
        {
            Error("Spawn search failed; using zone reserve that skips collision " + last_resort.ToString());
            return last_resort;
        }

        return vector.Zero;
    }

    protected bool Teleport(PlayerBase process_player)
    {
        ref Param2<vector, NamedLocation> random_pos = GetRandomSpawnPosition();
        vector position = ResolveFinalSpawn(random_pos.param1);
        NamedLocation village = random_pos.param2;
        if (position == vector.Zero)
        {
            string steamid = "unknown";
            if (process_player && process_player.GetIdentity())
                steamid = process_player.GetIdentity().GetPlainId();
            Error("Prepare could not place player " + steamid + " inside the first zone");
            return false;
        }

        return TeleportPlayer(process_player, position, village);
    }

    protected void TeleportGroup(set<PlayerBase> group)
    {
        ref Param2<vector, NamedLocation> random_pos = GetRandomSpawnPosition();
        vector position = random_pos.param1;
        NamedLocation village = random_pos.param2;

        int tmpNbPlayers = group.Count();
        for(int i = 0; i < tmpNbPlayers; i++)
        {
            PlayerBase player = group.Get(i);
            if(player && player.GetIdentity())
            {
                BattleRoyaleUtils.Trace("Teleport player " + player.GetIdentity().GetName() + " to position " + position);

                vector accepted = vector.Zero;
                for (int spawn_try = 1; spawn_try <= 50; spawn_try++)
                {
                    BattleRoyaleUtils.Trace("Try Group " + spawn_try);
                    float x = position[0] + Math.RandomFloatInclusive(-5.0, 5.0);
                    float z = position[2] + Math.RandomFloatInclusive(-5.0, 5.0);
                    float y = GetGame().SurfaceY(x, z);
                    vector candidate = Vector(x, y, z);
                    if (IsAcceptedSpawn(candidate))
                    {
                        accepted = candidate;
                        break;
                    }
                }

                accepted = ResolveFinalSpawn(accepted);
                if (!TeleportPlayer(player, accepted, village))
                {
                    Error("Prepare could not place grouped player " + player.GetIdentity().GetPlainId() + " inside the first zone");
                }
            }
            SleepPlayerBatch();
        }
    }

    protected bool TeleportPlayer(PlayerBase player, vector position, NamedLocation village = NULL)
    {
        if (!player || !player.GetIdentity())
            return false;

        if (!IsAcceptedSpawn(position))
        {
            if (!IsOnMap(position) || !IsInsideFirstZone(position))
            {
                Error("TeleportPlayer refused a position outside the first zone or the map for " + player.GetIdentity().GetPlainId());
                return false;
            }
            Error("TeleportPlayer last resort ignores collision for " + player.GetIdentity().GetPlainId() + " at " + position.ToString());
        }

        BattleRoyaleUtils.Trace("Spawn player " + player.GetIdentity().GetName() + " at " + position);

        //TODO: make sure the retarded game engine doesn't keep the player in a swimming state ????
        //TODO: force stand up

		vector direction;

        if (village)
        {
            direction = vector.Direction(player.GetPosition(), village.Position);
        } else {
            //random direction
            float dir = Math.RandomFloat(0, 360); //non-inclusive, 360==0
            vector playerDir = vector.YawToVector(dir);
            direction = Vector(playerDir[0], 0, playerDir[1]);
        }

		ScriptJunctureData pCtx = new ScriptJunctureData;
		pCtx.Write( position );
		pCtx.Write( direction );
		player.SendSyncJuncture( 88, pCtx );
        return true;
    }

    void ProcessPlayers()
    {
        BattleRoyaleUtils.Trace("Starting to process players...");
        int i;
        PlayerBase process_player;
        int pCount = a_PlayerList.Count();

        m_PlayerBatch = 0;
        for (i = 0; i < pCount; i++) {
            process_player = a_PlayerList[i];
            if (process_player)
            	DisablePlayerInput(process_player);
            SleepPlayerBatch();
        }
        FlushPlayerBatch();
        BattleRoyaleUtils.Trace("Players are disabled");

        a_PlayerList.ShuffleArray();

        m_PlayerBatch = 0;
        for (i = 0; i < pCount; i++) {
            process_player = a_PlayerList[i];
            if (process_player) GiveStartingItems(process_player);
            SleepPlayerBatch();
        }
        FlushPlayerBatch();
        BattleRoyaleUtils.Trace("Gave starting items");

        BattleRoyaleServer br_instance = BattleRoyaleServer.GetInstance();
        array<ref map<string, string>> parties_list = new array<ref map<string, string>> ();

		if (b_EnableSpawnSelectionMenu)
		{
			BattleRoyaleUtils.Trace("Spawn selection menu enabled");
            m_PlayerBatch = 0;
			for (i = 0; i < pCount; i++) {
				process_player = a_PlayerList[i];
				if (process_player && process_player.GetIdentity())
				{
					float f_SpawnSelectionRadius = m_GameSettings.spawn_selection_radius;
					vector position = process_player.GetSpawnPos();

					if( position == vector.Zero )
					{
						BattleRoyaleUtils.Trace("Player " + process_player.GetIdentity().GetName() + " didn't select a spawn point, we randomly teleport them");
						Teleport(process_player);
                        SleepPlayerBatch();
						continue;
					}

					BattleRoyaleUtils.Trace("Try to spawn player " + process_player.GetIdentity().GetName() + " at " + position + " with a radius of " + f_SpawnSelectionRadius);

					vector random_position = ResolveFinalSpawn(GetRandomSafePosition(position, f_SpawnSelectionRadius));

					if ( random_position == vector.Zero )
					{
						BattleRoyaleUtils.Trace("No safe position found, we randomly teleport the player");
						Teleport(process_player);
					}
					else
					{
						BattleRoyaleUtils.Trace("Found a safe position " + random_position);
						TeleportPlayer(process_player, random_position);
					}
				}
                SleepPlayerBatch();
			}
            FlushPlayerBatch();
		}
		else
		{
			BattleRoyaleUtils.Trace("Spawn selection menu disabled");
			// Check if mod party is present
#ifdef Carim
			BattleRoyaleUtils.Trace("Mod party present");

			// ref array ref set, what in the seven fucks is this ?
			ref array<ref set<PlayerBase>> teleport_groups = GetGroups();

			// Teleport groups
			int pGroupCount = teleport_groups.Count();
			ref set<PlayerBase> group;

			teleport_groups.ShuffleArray();
			BattleRoyaleUtils.Trace("Groups: " + pGroupCount);
            m_PlayerBatch = 0;
			for (i = 0; i < pGroupCount; i++) {
				BattleRoyaleUtils.Trace("Teleport group " + i);
				group = teleport_groups.Get(i);
				if (!group)
                    continue;
                if (group.Count() > 1)
					TeleportGroup(group);
                else if (group.Count() > 0)
                {
                    process_player = group.Get(0);
                    if (process_player)
                        Teleport(process_player);
                    SleepPlayerBatch();
                }
			}
            FlushPlayerBatch();
#else
			BattleRoyaleUtils.Trace("Mod party absent");
            m_PlayerBatch = 0;
			for (i = 0; i < pCount; i++) {
				process_player = a_PlayerList[i];
				if (process_player)
				{
					Teleport(process_player);
				}
                SleepPlayerBatch();
			}
            FlushPlayerBatch();
#endif
		}
        BattleRoyaleUtils.Trace("Teleported players");

		if ( m_ServerData.enable_vigrid_api )
		{
#ifdef Carim
			teleport_groups = GetGroups();

			// Teleport groups
			pGroupCount = teleport_groups.Count();

			// Send parties list to API server
			for (i = 0; i < pGroupCount; i++) {
				group = teleport_groups.Get(i);
                if (!group)
                    continue;
				map<string, string> party = new map<string, string>();
				int tmpNbPlayers = group.Count();
				for(int j = 0; j < tmpNbPlayers; j++)
				{
					process_player = group.Get(j);
					if ( process_player && process_player.GetIdentity() )
					{
						BattleRoyaleUtils.Trace( process_player.GetIdentity().GetPlainId() );
						CF_StringStream string_stream = CF_StringStream( process_player.GetIdentity().GetPlainName() );
						CF_Base16Stream base16_stream = CF_Base16Stream();
						string_stream.CopyTo( base16_stream );
						party.Insert( process_player.GetIdentity().GetPlainId(), base16_stream.Encode() );
					}
				}
				parties_list.Insert( party );
			}
#else
			for (i = 0; i < pCount; i++) {
				process_player = a_PlayerList[i];
				if (process_player && process_player.GetIdentity())
				{
					map<string, string> party = new map<string, string>();
					// Encode player ID to Base16
					CF_StringStream string_stream = CF_StringStream( process_player.GetIdentity().GetPlainName() );
					CF_Base16Stream base16_stream = CF_Base16Stream();
					string_stream.CopyTo( base16_stream );
					party.Insert( process_player.GetIdentity().GetPlainId(), base16_stream.Encode() )
					parties_list.Insert( party );
				}
			}
#endif

#ifdef BR_TRACE_ENABLED
			Print( parties_list );
#endif

			PartiesWebhook partiesWebhook = new PartiesWebhook( m_ServerData.webhook_jwt_token );
			partiesWebhook.postParties( br_instance.match_uuid, parties_list );
			BattleRoyaleUtils.Trace("Parties list sent");
		}

        // plz fix this
        Sleep(1000);

        m_PlayerBatch = 0;
        for (i = 0; i < pCount; i++) {
            process_player = a_PlayerList[i];
            if (process_player) process_player.ResetPlayer(true);
            SleepPlayerBatch();
        }
        FlushPlayerBatch();
        BattleRoyaleUtils.Trace("Healed players");

        m_PlayersProcessed = true;
        StopProcessWatchdog();
        if (IsActive())
            Deactivate();
    }

    override void OnPlayerTick(PlayerBase player, float timeslice)
    {
        super.OnPlayerTick(player, timeslice);

        if(player.time_until_heal <= 0)
        {
            player.time_until_heal = 5;
            player.Heal();
        }

        player.time_until_heal -= timeslice;
    }

    override void AddPlayer(PlayerBase player)
    {
        if(player)
        {
            player.SetAllowDamage(false); //all players in this state are god mode
            player.Heal();
        }

        super.AddPlayer(player);
    }

    override ref array<PlayerBase> RemoveAllPlayers()
    {
        ref array<PlayerBase> players = super.RemoveAllPlayers();
        foreach(PlayerBase player: players)
        {
            player.SetAllowDamage(true); //leaving debug state = disable god mode
            player.Heal();
        }

        return players;
    }

    override void RemovePlayer(PlayerBase player)
    {
        if(player)
        {
            player.SetAllowDamage(true); //leaving debug state = disable god mode
            player.Heal();
        }

        super.RemovePlayer(player);
    }
}
#endif

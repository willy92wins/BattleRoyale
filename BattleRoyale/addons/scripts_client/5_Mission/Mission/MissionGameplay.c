#ifndef SERVER
modded class MissionGameplay
{
	protected Widget m_BattleRoyaleHudRootWidget;
	protected ref BattleRoyaleHud m_BattleRoyaleHud;
	protected ref BattleRoyaleKillFeed m_BattleRoyaleKillFeed;
	protected ref BattleRoyaleMiniMap m_BattleRoyaleMiniMap;
	protected Widget m_BattleRoyaleWinScreen;

	void MissionGameplay()
	{
		Print("MissionGameplay::MissionGameplay");
		m_BattleRoyaleHudRootWidget = null;
		m_BattleRoyaleWinScreen = null;
		m_BattleRoyale = null;

		// Add RPCs
		GetRPCManager().AddRPC( RPC_DAYZBR_NAMESPACE, "ShowSpawnSelection", this );
		GetRPCManager().AddRPC( RPC_DAYZBR_NAMESPACE, "HideSpawnSelection", this );
		GetRPCManager().AddRPC( RPC_DAYZBR_NAMESPACE, "ShowSpawnPoint", this );
		GetRPCManager().AddRPC( RPC_DAYZBR_NAMESPACE, "UpdateHeatMap", this );
		GetRPCManager().AddRPC( RPC_DAYZBR_NAMESPACE, "ReturnToLobby", this );
	}

	void ~MissionGameplay()
	{
		if (m_BattleRoyaleHudRootWidget)
		{
			m_BattleRoyaleHudRootWidget.Unlink();
		}

		if (m_BattleRoyaleWinScreen)
		{
			m_BattleRoyaleWinScreen.Unlink();
		}

		m_BattleRoyale = null;

		// Remove RPCs
		GetRPCManager().RemoveRPC( RPC_DAYZBR_NAMESPACE, "ShowSpawnSelection" );
		GetRPCManager().RemoveRPC( RPC_DAYZBR_NAMESPACE, "HideSpawnSelection" );
		GetRPCManager().RemoveRPC( RPC_DAYZBR_NAMESPACE, "ShowSpawnPoint" );
		GetRPCManager().RemoveRPC( RPC_DAYZBR_NAMESPACE, "UpdateHeatMap" );
		GetRPCManager().RemoveRPC( RPC_DAYZBR_NAMESPACE, "ReturnToLobby" );
	}

	override void OnInit()
	{
		BattleRoyaleUtils.Trace("MissionGameplay::OnInit");
		super.OnInit();

#ifndef NO_GUI
		DayZPlayerImplement.BR_OnSessionChanged();
#endif

		m_BattleRoyale = new BattleRoyaleClient;

		InitBRhud();
	}

	override void OnMissionFinish()
	{
#ifndef NO_GUI
		DayZPlayerImplement.BR_OnSessionChanged();
#endif
		// Set again by the next server that runs serial matches or the waiting room
		BattleRoyaleRPC.s_StayConnected = false;
		BattleRoyaleRPC.s_WaitingRoom = false;

		super.OnMissionFinish();
	}

	override BattleRoyaleClient GetBattleRoyale()
	{
		if ( !m_BattleRoyale )
		{
			m_BattleRoyale = new BattleRoyaleClient;
		}

		return m_BattleRoyale;
	}

	//! Serial matches: the server gave this client a new character in the next lobby. Everything the finished match left
	//! on the client goes: the death (dead screen, locked controls, muted sound), spectating, the win screen, the pending
	//! leave, the screen effects, and the match state and HUD of the BattleRoyale client, which are built again.
	//! The vanilla HUD is reset by the game itself when the new character is selected (PlayerBase.OnSelectPlayer)
	void ReturnToLobby(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
	{
		if ( type != CallType.Client )
			return;

		BattleRoyaleUtils.Info("Serial match: back in the lobby");

#ifndef NO_GUI
		DayZPlayerImplement.BR_OnSessionChanged();
#endif
		BattleRoyaleRPC.ResetSpectating();
		BattleRoyaleRPC.RestoreAfterDeath();
		if ( GetHud() )
			GetHud().Show( true );

		PPERequesterBase outside_zone = PPERequesterBank.GetRequester( PPERequesterBank.REQ_BATTLEROYALE );
		if ( outside_zone )
			outside_zone.Stop();
		PPERequesterBank.GetRequester( PPERequester_BurlapSackEffects ).Stop();

		if ( m_BattleRoyaleWinScreen )
		{
			m_BattleRoyaleWinScreen.Unlink();
			m_BattleRoyaleWinScreen = null;
		}

		// A new client object resets the shared match state (BattleRoyaleRPC.Reset) and its zone walls
		m_BattleRoyale = new BattleRoyaleClient;

		if ( m_BattleRoyaleHudRootWidget )
		{
			m_BattleRoyaleHudRootWidget.Unlink();
			m_BattleRoyaleHudRootWidget = null;
		}
		m_BattleRoyaleHud = null;
		m_BattleRoyaleKillFeed = null;
		m_BattleRoyaleMiniMap = null;
		InitBRhud();
	}

	void InitBRhud()
	{
		BattleRoyaleUtils.Trace("Initializing BattleRoyale HUD");
		if(!m_BattleRoyaleHudRootWidget)
		{
			m_BattleRoyaleHudRootWidget = GetGame().GetWorkspace().CreateWidgets("Vigrid-BattleRoyale/GUI/layouts/hud/br_hud.layout");

			if (!m_BattleRoyaleHudRootWidget)
			{
				Error("Failed to create the BattleRoyale HUD!");
				return;
			}

			m_BattleRoyaleHud = new BattleRoyaleHud( m_BattleRoyaleHudRootWidget );
			m_BattleRoyaleHud.ShowHud( true );

			// Kill feed rows live under the HUD root and are unlinked with it
			m_BattleRoyaleKillFeed = new BattleRoyaleKillFeed( m_BattleRoyaleHudRootWidget );

			// The minimap panel is part of the HUD layout
			m_BattleRoyaleMiniMap = new BattleRoyaleMiniMap( m_BattleRoyaleHudRootWidget );
			BattleRoyaleUtils.Trace("HUD Initialized");
		}
	}

	//! Returns false while the HUD does not exist yet, so the caller can try again on the next update
	bool UpdateKillCount(int count)
	{
		if (!m_BattleRoyaleHud)
			return false;

		m_BattleRoyaleHud.ShowKillCount( true );
		m_BattleRoyaleHud.SetKillCount( count );
		return true;
	}

	void UpdateCountdownTimer(int seconds)
	{
		if (!m_BattleRoyaleHud)
			return;

		m_BattleRoyaleHud.SetCountdown( seconds );
	}

	//! Returns false while the HUD does not exist yet, so the caller can try again on the next update
	bool UpdatePlayerCount(int nb_players, int nb_groups)
	{
		if (!m_BattleRoyaleHud)
			return false;

		//BattleRoyaleUtils.Trace(string.Format("UpdatePlayerCount: %1 %2", nb_players, nb_groups));
		m_BattleRoyaleHud.ShowCount( true );
		m_BattleRoyaleHud.SetCount( nb_players, nb_groups );
		return true;
	}

	void UpdateZoneDistance(bool isInsideZone, float distExt, float distInt, float angle)
	{
		if (!m_BattleRoyaleHud)
			return;

		m_BattleRoyaleHud.ShowDistance(true);
		//m_BattleRoyaleHud.ShowDistance( distance > 0 );
		m_BattleRoyaleHud.SetDistance( isInsideZone, distExt, distInt, angle );
	}

	void HideDistance()
	{
		if (!m_BattleRoyaleHud)
			return;

		m_BattleRoyaleHud.ShowDistance( false );
	}

	//! label is one of BattleRoyaleHud.PHASE_LABEL_*, seconds is -1 when there is no timer to show
	void UpdateMatchPhase(int label, int seconds)
	{
		if (!m_BattleRoyaleHud)
			return;

		m_BattleRoyaleHud.SetMatchPhase( label, seconds );
	}

	void UpdateZoneDamage(bool taking_damage)
	{
		if (!m_BattleRoyaleHud)
			return;

		m_BattleRoyaleHud.ShowZoneDamage( taking_damage );
	}

	void UpdateKillFeed()
	{
		if (!m_BattleRoyaleKillFeed)
			return;

		m_BattleRoyaleKillFeed.Update();
	}

	//! Called once per frame by the client with the zones to outline: locked_area red, open_area yellow
	void UpdateMiniMap(float timeslice, BattleRoyalePlayArea locked_area, BattleRoyalePlayArea open_area)
	{
		if (!m_BattleRoyaleMiniMap)
			return;

		m_BattleRoyaleMiniMap.Update( timeslice, locked_area, open_area );
	}

	//! Created once; unlinked by the destructor when the mission ends
	void ShowWinScreen()
	{
		if (m_BattleRoyaleWinScreen || !GetGame().GetWorkspace())
			return;

		m_BattleRoyaleWinScreen = GetGame().GetWorkspace().CreateWidgets("Vigrid-BattleRoyale/GUI/layouts/hud/win_screen.layout");
		if (m_BattleRoyaleWinScreen)
		{
			m_BattleRoyaleWinScreen.Show( true );
		}
	}


	override void OnUpdate( float timeslice )
	{
		super.OnUpdate( timeslice ); //no more using fade out because it causes way to much compatibility issues, instead we'll use widgets

		m_BattleRoyale.Update( timeslice ); //send tick to br client

		if (GetUApi() && !m_UIManager.IsMenuOpen(MENU_CHAT_INPUT)) {
			if (GetUApi().GetInputByID(UADayZBRReadyUp).LocalPress()) {
				BattleRoyaleClient.Cast( m_BattleRoyale ).ReadyUp();
			}
			if (GetUApi().GetInputByID(UADayZBRUnstuck).LocalPress()) {
				BattleRoyaleClient.Cast( m_BattleRoyale ).Unstuck();
			}
			// The minimap keys act only in gameplay: no menu, inventory or settings open
			if (m_BattleRoyaleMiniMap && !m_UIManager.GetMenu()) {
				if (GetUApi().GetInputByID(UADayZBRToggleMiniMap).LocalPress()) {
					m_BattleRoyaleMiniMap.ToggleVisible();
				}
				if (GetUApi().GetInputByID(UADayZBRMiniMapZoom).LocalPress()) {
					m_BattleRoyaleMiniMap.CycleZoom();
				}
			}
		}
	}

	void ShowSpawnSelection(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
	{
		Param4<int, float, vector, float> data;
		if( !ctx.Read( data ) )
		{
			Error("FAILED TO READ SHOWSPAWNSELECTION RPC");
			return;
		}
		if ( type == CallType.Client )
		{
			BattleRoyaleUtils.Trace(string.Format("ShowSpawnSelection: %1 %2 %3 %4", data.param1, data.param2, data.param3, data.param4));
			//ANVICaptchaMenu.Cast(GetUIManager().EnterScriptedMenu(MENU_ANVI_CAPTCHA, GetUIManager().GetMenu()));
			SpawnSelectionMenu m = SpawnSelectionMenu.Cast(GetUIManager().EnterScriptedMenu(MENU_SPAWN_SELECTION, GetUIManager().GetMenu()));
			m.SetInitialCountdown(data.param1);
			m.SetSpawnSize(data.param2);
			m.SetFirstZone(data.param3, data.param4);
		}
	}

	void HideSpawnSelection(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
	{
		if ( type == CallType.Client )
		{
			BattleRoyaleUtils.Trace("HideSpawnSelection");
			SpawnSelectionMenu m = SpawnSelectionMenu.Cast(GetUIManager().FindMenu(MENU_SPAWN_SELECTION));
			if (m)
			{
				m.Close();
			}
		}
	}

	void ShowSpawnPoint(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
	{
		Param3<PlayerBase, vector, int> data;
		if( !ctx.Read( data ) )
		{
			Error("FAILED TO READ SHOWSPAWNPOINT RPC");
			return;
		}
		if ( type == CallType.Client )
		{
			BattleRoyaleUtils.Trace(string.Format("ShowSpawnPoint: %1 %2 %3", data.param1, data.param2, data.param3));
			SpawnSelectionMenu m = SpawnSelectionMenu.Cast(GetUIManager().FindMenu(MENU_SPAWN_SELECTION));
			if (m)
			{
				m.SetTeammateSpawnPoint(data.param1, data.param2, data.param3);
			}
		}
	}

	void UpdateHeatMap(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
	{
		Param1<array<vector>> data;
		if( !ctx.Read( data ) )
		{
			Error("FAILED TO READ UPDATEHEATMAP RPC");
			return;
		}
		if ( type == CallType.Client )
		{
			BattleRoyaleUtils.Trace("UpdateHeatMap");
			SpawnSelectionMenu m = SpawnSelectionMenu.Cast(GetUIManager().FindMenu(MENU_SPAWN_SELECTION));
			if (m)
			{
				m.UpdateHeatMap(data.param1);
			}
		}
	}
}
#endif

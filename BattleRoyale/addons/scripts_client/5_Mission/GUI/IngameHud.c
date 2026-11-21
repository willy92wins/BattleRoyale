#ifndef SERVER
modded class IngameHud
{
	protected bool b_BrVisibilityOverride;

	// Lobby player tags: a single camera raycast every BR_TAG_REFRESH_INTERVAL seconds
	protected static const float BR_TAG_REFRESH_INTERVAL = 0.15;
	protected static const float BR_TAG_MAX_DISTANCE = 15.0;	//!< same range as vanilla RefreshPlayerTags
	protected static const float BR_TAG_RAY_LENGTH = 25.0;		//!< same ray as vanilla RefreshPlayerTags

	protected float m_BrTagRefreshTimer;
	protected ref RaycastRVParams m_BrTagRayParams;
	protected ref array<ref RaycastRVResult> m_BrTagRayResults;
	protected Widget m_BrTagRoot;	//!< m_PlayerTag the cached frame was looked up in
	protected Widget m_BrTagFrame;

	void IngameHud()
	{
		b_BrVisibilityOverride = false;
		m_BrTagRefreshTimer = 0;
	}

	//! Prevents duplicate ParachuteGUI / FreeFall widgets if Init is ever called more than once on this HUD
	override void Init( Widget hud_panel_widget )
	{
		if ( m_VehicleHudMap && m_VehicleHudMap.Contains( "VehicleTypeParachute" ) )
		{
			ParachuteGUI oldPara = ParachuteGUI.Cast( m_VehicleHudMap.Get( "VehicleTypeParachute" ) );
			if ( oldPara )
				oldPara.BR_UnlinkPanel();
			m_VehicleHudMap.Remove( "VehicleTypeParachute" );
		}
		if ( m_FreeFall )
		{
			m_FreeFall.Unlink();
			m_FreeFall = null;
		}

		super.Init( hud_panel_widget );
	}

	protected bool BR_IsMenuBlockingParachuteHud()
	{
		if ( m_HudVisibility )
		{
			if ( m_HudVisibility.IsContextFlagActive( EHudContextFlags.MENU_OPEN ) )
				return true;
			if ( m_HudVisibility.IsContextFlagActive( EHudContextFlags.INVENTORY_OPEN ) )
				return true;
		}
		if ( GetGame() && GetGame().GetUIManager() )
		{
			UIManager ui = GetGame().GetUIManager();
			if ( ui.GetMenu() || ui.IsDialogVisible() )
				return true;
		}
		return false;
	}

	protected void BR_SyncParachuteHudVisibility()
	{
		if ( !m_VehicleHudMap )
			return;

		VehicleHudBase paraHud = m_VehicleHudMap.Get( "VehicleTypeParachute" );
		if ( !paraHud || m_ActiveVehicleHUD != paraHud )
			return;

		bool blocked = BR_IsMenuBlockingParachuteHud();
		bool disabled = false;
		if ( m_HudVisibility )
		{
			if ( m_HudVisibility.IsContextFlagActive( EHudContextFlags.VEHICLE_DISABLE ) )
				disabled = true;
			if ( m_HudVisibility.IsContextFlagActive( EHudContextFlags.UNCONSCIOUS ) )
				disabled = true;
		}

		if ( blocked || disabled )
		{
			paraHud.HidePanel();
		}
		else
		{
			if ( m_HudVisibility && !m_HudVisibility.IsContextFlagActive( EHudContextFlags.DRIVER ) )
				ShowVehicleHud( true );
			paraHud.ShowPanel();
		}
	}

	override void ShowVehicleInfo()
	{
		super.ShowVehicleInfo();
		BR_SyncParachuteHudVisibility();
	}

	//! D1/D2 fix: vanilla RefreshVehicleHud clears EHudContextFlags.VEHICLE without a HumanCommandVehicle
	//! and Admiral's override immediately sets it back after super, toggling left-HUD visibility twice per
	//! frame. Handle VEHICLE once while the parachute is open, enforce menu hiding, and hide ParachuteGUI
	//! as a safety net when the parachute closes on landing.
	override void RefreshVehicleHud( float timeslice )
	{
		if ( !GetGame() )
			return;

		PlayerBase parapb = PlayerBase.Cast( GetGame().GetPlayer() );
		VehicleHudBase paraHud = null;
		if ( m_VehicleHudMap )
			paraHud = m_VehicleHudMap.Get( "VehicleTypeParachute" );

		if ( parapb && parapb.IsParachuteOpen() )
		{
			if ( m_HudVisibility && !m_HudVisibility.IsContextFlagActive( EHudContextFlags.VEHICLE ) )
				m_HudVisibility.SetContextFlag( EHudContextFlags.VEHICLE, true );

			if ( paraHud && m_ActiveVehicleHUD != paraHud )
				ShowVehicleInfo();

			if ( m_ActiveVehicleHUD )
				m_ActiveVehicleHUD.RefreshVehicleHud( timeslice );

			BR_SyncParachuteHudVisibility();
			return;
		}

		if ( paraHud && m_ActiveVehicleHUD == paraHud )
			HideVehicleInfo();

		super.RefreshVehicleHud( timeslice );
	}

	override void Show( bool show )
	{
		if(b_BrVisibilityOverride)
			return;

		super.Show( show );
	}

	override void Update( float timeslice )
	{
		super.Update( timeslice );

		// Show player tags only if the match is not started
		BattleRoyaleRPC br_rpc = BattleRoyaleRPC.GetInstance();
		if( !br_rpc.match_started )
		{
			m_BrTagRefreshTimer -= timeslice;
			if ( m_BrTagRefreshTimer <= 0 )
			{
				m_BrTagRefreshTimer = BR_TAG_REFRESH_INTERVAL;
				BR_RefreshPlayerTag();
			}

			ShowPlayerTag(timeslice);

			// The tag layout is created by ShowPlayerTag; its frame is resized once
			if ( m_PlayerTag && m_PlayerTag != m_BrTagRoot )
			{
				m_BrTagRoot = m_PlayerTag;
				m_BrTagFrame = m_PlayerTag.FindAnyWidget( "TagFrame" );
				if ( m_BrTagFrame )
					m_BrTagFrame.SetSize( 300, 25 );
				m_PlayerTag.SetFlags( WidgetFlags.IGNOREPOINTER );
			}
		}
		else if ( m_PlayerTag && m_PlayerTag.IsVisible() )
		{
			// Tags stop updating when the match starts: hide the last one instead of leaving it frozen on screen
			m_PlayerTag.Show( false );
		}
	}

	//! Replaces vanilla RefreshPlayerTags, which casts the same camera ray once per player in range
	protected void BR_RefreshPlayerTag()
	{
		Man local_player = GetGame().GetPlayer();
		if ( !local_player )
		{
			m_CurrentTaggedPlayer = null;
			return;
		}

		vector head_pos = GetGame().GetCurrentCameraPosition();
		vector end_pos = head_pos + GetGame().GetCurrentCameraDirection() * BR_TAG_RAY_LENGTH;

		if ( !m_BrTagRayParams )
		{
			m_BrTagRayParams = new RaycastRVParams( head_pos, end_pos, local_player, 0 );
			m_BrTagRayParams.sorted = true;
			m_BrTagRayResults = new array<ref RaycastRVResult>;
		}

		m_BrTagRayParams.begPos = head_pos;
		m_BrTagRayParams.endPos = end_pos;
		m_BrTagRayParams.ignore = local_player;
		m_BrTagRayResults.Clear();

		PlayerBase tagged = null;
		if ( DayZPhysics.RaycastRVProxy( m_BrTagRayParams, m_BrTagRayResults ) && m_BrTagRayResults.Count() > 0 )
		{
			RaycastRVResult hit = m_BrTagRayResults.Get( 0 );
			tagged = PlayerBase.Cast( hit.obj );

			// A hit on worn gear is a proxy; its parent is the player wearing it
			if ( !tagged && hit.hierLevel > 0 )
				tagged = PlayerBase.Cast( hit.parent );
		}

		if ( tagged && ( tagged == local_player || vector.Distance( head_pos, tagged.GetPosition() ) > BR_TAG_MAX_DISTANCE ) )
			tagged = null;

		m_CurrentTaggedPlayer = tagged;
	}
}
#endif

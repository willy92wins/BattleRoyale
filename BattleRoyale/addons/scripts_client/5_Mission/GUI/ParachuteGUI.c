#ifndef SERVER
//! Fixes D2 on Admiral's ParachuteGUI: keeps the parachute panel hidden while a menu or dialog
//! is open and clears m_CurrentPlayer when hidden.
modded class ParachuteGUI
{
	void BR_UnlinkPanel()
	{
		if ( m_VehiclePanel )
		{
			m_VehiclePanel.Unlink();
			m_VehiclePanel = null;
		}
		m_CurrentPlayer = null;
	}

	//! Admiral's ShowVehicleInfo calls m_VehiclePanel.Show(true) directly, bypassing
	//! IngameHudVisibility's MENU_OPEN rule. Immediately hide the panel if a menu or dialog
	//! is already open when the parachute deploys (e.g. safety auto-open while viewing the map).
	override void ShowVehicleInfo( PlayerBase player )
	{
		super.ShowVehicleInfo( player );
		if ( GetGame() && GetGame().GetUIManager() )
		{
			UIManager ui = GetGame().GetUIManager();
			if ( ui.GetMenu() || ui.IsDialogVisible() )
				HidePanel();
		}
	}

	//! Admiral's ParachuteGUI does not clear m_CurrentPlayer on HideVehicleInfo.
	override void HideVehicleInfo()
	{
		super.HideVehicleInfo();
		m_CurrentPlayer = null;
		HidePanel();
	}
}
#endif

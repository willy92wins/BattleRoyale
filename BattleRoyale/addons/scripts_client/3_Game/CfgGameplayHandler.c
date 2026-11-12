//! Every player can open the 2D map with the map key and sees their own position on it, without a map,
//! compass or GPS item. Applied on both sides, so it holds whatever the mission cfggameplay.json says.
modded class CfgGameplayHandler
{
	override static bool GetMapIgnoreMapOwnership()
	{
		return true;
	}

	override static bool GetMapIgnoreNavItemsOwnership()
	{
		return true;
	}

	override static bool GetMapDisplayPlayerPosition()
	{
		return true;
	}

	override static bool GetUse3DMap()
	{
		return false;
	}
}

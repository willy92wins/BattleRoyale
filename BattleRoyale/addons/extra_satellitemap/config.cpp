class CfgPatches
{
	class vigrid_satellite_map
	{
		units[]={};
		weapons[]={};
		requiredVersion=0.1;
		requiredAddons[]=
		{
			"DZ_Data",
			"DZ_Worlds_Chernarusplus_World"
		};
	};
};
class CfgWorlds
{
	class CAWorld;
	class ChernarusPlus: CAWorld
	{
		// The 2D map draws its tiles from userMapPath. The terrain's own satellite tiles use the same 32x32 grid and
		// s_XXX_YYY_lco names as the vanilla user map, so the map shows the satellite image with no new assets
		userMapPath="DZ\worlds\chernarusplus\data\layers";
	};
};

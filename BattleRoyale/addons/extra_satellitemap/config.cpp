class CfgPatches
{
	class vigrid_satellite_map
	{
		units[]={};
		weapons[]={};
		requiredVersion=0.1;
		requiredAddons[]=
		{
			"DZ_Data"
		};
	};
};
class MapDefaults;
class RscMapControl: MapDefaults
{
	// The 2D map and the minimap show the terrain's satellite image up to scale 4 (vanilla: 0, no satellite); the minimap zooms out to scale 2
	maxSatelliteAlpha=1;
	alphaFadeStartScale=4;
	alphaFadeEndScale=4;
	// No paper map tiles over the satellite image (vanilla: 1 up to scale 0.34)
	maxUserMapAlpha=0;
	// Forests drawn without the green tint over the photo (vanilla alpha 0.5); roads, names and grid stay
	colorForest[]={0.36000001,0.77999997,0.079999998,0};
};

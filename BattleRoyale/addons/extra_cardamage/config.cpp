class CfgPatches
{
	class vigrid_car_damage
	{
		requiredAddons[]=
		{
			"DZ_Data",
			"DZ_Scripts",
			"DZ_Vehicles_Wheeled"
		};
	};
};
class CfgMods
{
	class vigrid_car_damage
	{
		name="Vigrid Car Damage";
		type="mod";
		dependencies[]=
		{
			"World"
		};
		class defs
		{
			class worldScriptModule
			{
				value="";
				files[]=
				{
					"Vigrid-BattleRoyale/Extra/CarDamage/Scripts/4_World"
				};
			};
		};
	};
};

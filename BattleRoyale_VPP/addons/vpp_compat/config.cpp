class CfgPatches
{
	class BattleRoyale_VPP_Compat
	{
		units[]={};
		weapons[]={};
		requiredVersion=0.1;
		requiredAddons[]=
		{
			"DZM_VPPAdminTools",
			"DZM_VPPAdminToolsScripts",
			"BattleRoyale_Scripts_Client"
		};
	};
};
class CfgMods
{
	class BattleRoyale_VPP_Compat
	{
		name="DayZ-BattleRoyale VPP compatibility";
		type="mod";
		dependencies[]=
		{
			"Mission"
		};
		class defs
		{
			class missionScriptModule
			{
				value="";
				files[]=
				{
					"Vigrid-BattleRoyale/Compat/VPP/5_Mission"
				};
			};
		};
	};
};

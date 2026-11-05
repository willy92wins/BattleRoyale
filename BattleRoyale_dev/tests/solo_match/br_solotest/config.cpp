class CfgPatches
{
	class BattleRoyale_SoloTest
	{
		units[]={};
		weapons[]={};
		requiredVersion=0.1;
		requiredAddons[]=
		{
			"BattleRoyale_Scripts_Server"
		};
	};
};
class CfgMods
{
	class BattleRoyale_SoloTest
	{
		name="BattleRoyale solo match test (local tests only)";
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
					"Vigrid-BattleRoyale/Tests/SoloTest/5_Mission"
				};
			};
		};
	};
};

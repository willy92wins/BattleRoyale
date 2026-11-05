class CfgPatches
{
	class BattleRoyale_LoadTest
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
	class BattleRoyale_LoadTest
	{
		name="BattleRoyale load test (local tests only)";
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
					"Vigrid-BattleRoyale/Tests/LoadTest/5_Mission"
				};
			};
		};
	};
};

class CfgPatches
{
	class BattleRoyale_Data
	{
		units[]={};
		weapons[]={};
		requiredVersion=0.1;
		requiredAddons[]=
		{
			"DZ_Sounds_Effects"
		};
	};
};
class CfgSoundShaders
{
	class baseCharacter_SoundShader;
	class BR_NotificationBeep_SoundShader: baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"Vigrid-BattleRoyale\Data\sounds\br_notification_beep",
				1
			}
		};
		volume=1;
	};
};
class CfgSoundSets
{
	class baseCharacter_SoundSet;
	class BR_NotificationBeep_SoundSet: baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"BR_NotificationBeep_SoundShader"
		};
		spatial=0;
	};
};

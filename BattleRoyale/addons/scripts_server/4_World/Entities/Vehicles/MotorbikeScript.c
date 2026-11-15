#ifdef SERVER
modded class MotorbikeScript
{
	//! A crash hurts the crew by setting its health, without a damage hit; the seated players are
	//! stamped so that the kill feed names the crash for a death it caused (PlayerBase.BR_CrashDamageStart)
	override protected void DamageCrew(float dmg)
	{
		array<PlayerBase> crew = PlayerBase.BR_CrashDamageStart(this);
		super.DamageCrew(dmg);
		PlayerBase.BR_CrashDamageEnd(crew);
	}
}
#endif

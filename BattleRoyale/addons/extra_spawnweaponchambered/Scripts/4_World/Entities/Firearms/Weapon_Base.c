#ifdef SERVER
modded class Weapon_Base
{
	override void EEOnCECreate()
	{
		super.EEOnCECreate();

		// Chamber a round on economy spawn only. Do not attach a magazine:
		// SpawnWithAmmoAndMagazine already creates the loose ones.
		FillChamber( "", WeaponWithAmmoFlags.CHAMBER );
	}
}
#endif

#ifndef SERVER
modded class Weapon_Base
{
	//! Shows the barrel again whatever optic is attached now. HideWeaponBarrel(false) only does it while a magnifying optic
	//! is still attached, so a spectator that hid the barrel behind an optic taken off since would leave it hidden
	void BR_ShowWeaponBarrel()
	{
		if (m_weaponHideBarrelIdx != -1)
			SetSimpleHiddenSelectionState(m_weaponHideBarrelIdx, true);
	}
}
#endif

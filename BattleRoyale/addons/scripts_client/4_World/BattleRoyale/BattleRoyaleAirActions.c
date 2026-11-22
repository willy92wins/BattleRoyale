//! Blocks world actions, item dropping and weapon firing while the player is aboard
//! the drop plane or airborne during a plane-deployment match.
//! Loads after ADM_Parachute_Mod so our airlock wins over Admiral's overrides in the air.
//! When legacy_spawn is 1 (plane deployment off), BR_IsAirlocked() is always false and super runs unchanged.

modded class ActionManagerBase
{
	override bool ActionPossibilityCheck(int pCurrentCommandID)
	{
		PlayerBase player = PlayerBase.Cast(m_Player);
		if (player && player.BR_IsAirlocked())
			return false;
		return super.ActionPossibilityCheck(pCurrentCommandID);
	}
}

modded class ActionDropItem
{
	override bool ActionCondition(PlayerBase player, ActionTarget target, ItemBase item)
	{
		if (player && player.BR_IsAirlocked())
			return false;
		return super.ActionCondition(player, target, item);
	}
}

modded class WeaponManager
{
	override bool CanFire(Weapon_Base wpn)
	{
		if (m_player && m_player.BR_IsAirlocked())
			return false;
		return super.CanFire(wpn);
	}
}

#ifdef SERVER
modded class ItemBase
{
	//! Serial matches: an item a player puts on the ground (dropped, swapped, thrown, or taken off a body) is a leftover
	//! of the match. One cheap check per inventory move, and only while the setting is on
	override void EEItemLocationChanged(notnull InventoryLocation oldLoc, notnull InventoryLocation newLoc)
	{
		super.EEItemLocationChanged(oldLoc, newLoc);

		if (!BRMatchDebris.s_Enabled || newLoc.GetType() != InventoryLocationType.GROUND)
			return;

		EntityAI from = oldLoc.GetParent();
		if (from && from.GetHierarchyRootPlayer())
			BRMatchDebris.Get().AddItem(this);
	}
}
#endif

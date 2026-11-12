#ifndef SERVER
#ifdef EXPANSIONUI
//! Notifications are display only: the root of each Expansion notification layout takes mouse clicks
//! (click to dismiss), so a notification over the inventory or the map swallows the clicks meant for what is under it
modded class ExpansionNotificationView
{
	override void ShowNotification()
	{
		super.ShowNotification();

		Widget root = GetLayoutRoot();
		if (root)
			root.SetFlags(WidgetFlags.IGNOREPOINTER);
	}
};
#endif
#endif

//! Notifications are display only: the root of each notification element takes mouse clicks
//! (click to dismiss), so a notification over the inventory or the map swallows the clicks meant for what is under it.
//! Battle royale notifications also get their colour on the icon.
modded class NotificationUI
{
	override void AddNotification(NotificationRuntimeData data)
	{
		super.AddNotification(data);

		if (m_Root)
			m_Root.SetFlags(WidgetFlags.IGNOREPOINTER);
		if (m_Spacer)
			m_Spacer.SetFlags(WidgetFlags.IGNOREPOINTER);

		Widget notification;
		if (!m_Notifications.Find(data, notification) || !notification)
			return;

		notification.SetFlags(WidgetFlags.IGNOREPOINTER);

		if (data.GetTitleText() == DAYZBR_MSG_TITLE)
		{
			Widget icon = notification.FindAnyWidget("Image");
			if (icon)
				icon.SetColor(data.GetColor());
		}
	}
}

//! A new notification is always shown at once, as with the previous notification system.
//! Vanilla defers notifications past MAX_NOTIFICATIONS and starts their timer only when they show, so a
//! deferred "zone closes in 10 seconds" could appear after the zone closed. Here the oldest visible
//! notification is removed instead, the same way vanilla Update removes an expired one.
modded class NotificationSystem
{
	override protected void AddNotif(NotificationRuntimeData data)
	{
		while (m_TimeArray.Count() >= MAX_NOTIFICATIONS)
		{
			NotificationRuntimeData oldest = m_TimeArray.Get(0);
			m_OnNotificationRemoved.Invoke(oldest);
			m_TimeArray.Remove(0);
		}

		super.AddNotif(data);
	}
}

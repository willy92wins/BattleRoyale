//! Deferred notifications are shown in the order they arrived.
//! Vanilla Update moves the LAST element of m_DeferredArray on screen when one expires, so a burst
//! past MAX_NOTIFICATIONS would show newest first; inserting at the front keeps the oldest last.
modded class NotificationSystem
{
	override protected void AddNotif(NotificationRuntimeData data)
	{
		if (m_TimeArray.Count() < MAX_NOTIFICATIONS)
		{
			super.AddNotif(data);
			return;
		}

		m_DeferredArray.InsertAt(data, 0);
	}
}

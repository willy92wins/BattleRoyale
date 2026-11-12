//! Notifications are display only: the root of each notification element takes mouse clicks
//! (click to dismiss), so a notification over the inventory or the map swallows the clicks meant for what is under it.
//! Battle royale notifications also get their colour on the icon and a short vanilla sound.
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

			BR_PlayNotificationSound();
		}
	}

	//! Tick time of the last notification sound; notifications closer than BR_MSG_SOUND_GAP play one sound
	protected static float s_BR_LastSoundTime = -1000;
	protected static const float BR_MSG_SOUND_GAP = 1.0;

	//! Plays the notification sound in gameplay only (a player exists), at most once per BR_MSG_SOUND_GAP seconds
	protected void BR_PlayNotificationSound()
	{
		if (!GetGame().GetPlayer())
			return;

		float now = GetGame().GetTickTime();
		if (now - s_BR_LastSoundTime < BR_MSG_SOUND_GAP)
			return;
		s_BR_LastSoundTime = now;

		EffectSound sound = SEffectManager.PlaySound(DAYZBR_MSG_SOUND, GetGame().GetCurrentCameraPosition());
		if (sound)
			sound.SetAutodestroy(true);
	}
}

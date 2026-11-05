modded class MainMenuNewsfeed
{
	override void OpenFeedback()
	{
		if (BATTLEROYALE_FEEDBACK_URL == "")
		{
			super.OpenFeedback();
			return;
		}

		GetGame().OpenURL( BATTLEROYALE_FEEDBACK_URL );
	}
}
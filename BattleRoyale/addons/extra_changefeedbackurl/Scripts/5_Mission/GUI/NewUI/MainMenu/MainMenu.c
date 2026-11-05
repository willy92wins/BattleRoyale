modded class MainMenu
{
	// The BR main menu layout has no feedback button, so there is nothing to hide here
	override protected void OpenFeedback()
	{
		if (BATTLEROYALE_FEEDBACK_URL == "")
		{
			super.OpenFeedback();
			return;
		}

		GetGame().OpenURL( BATTLEROYALE_FEEDBACK_URL );
	}
}
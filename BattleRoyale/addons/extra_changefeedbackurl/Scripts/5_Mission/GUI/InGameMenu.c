modded class InGameMenu
{
	override Widget Init()
	{
		Widget root = super.Init();

		// Without a feedback link the button has nowhere to go
		if (m_FeedbackButton)
			m_FeedbackButton.Show(BATTLEROYALE_FEEDBACK_URL != "");

		return root;
	}

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
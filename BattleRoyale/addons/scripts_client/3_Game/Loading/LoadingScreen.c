#ifndef SERVER
modded class LoadingScreen
{
    protected autoptr array< ref LoadingScreenBackground > m_Backgrounds;
	static float f_LoadingTime = -1;
	static float f_LoadingTimeStamp = -1;

    void LoadingScreen(DayZGame game)
    {
        BattleRoyaleUtils.Trace("Loading screens DayZ-BR");

		m_Backgrounds = LoadingScreenBackgrounds.Get();

        m_ImageLogoMid.Show( false );  // Hide the DayZ logo
        m_ImageLogoCorner.Show( false );  // Hide the Bohemia Interactive logo
        m_ModdedWarning.Show( false );  // Hide the modded warning

		// Remove the reveal mask of loading.layout once; LoadMaskTexture("") logs a resource error each time it runs
		if (m_ImageBackground)
			m_ImageBackground.LoadMaskTexture("");
    }

    override void Show()
    {
        super.Show();

        m_ImageLogoMid.Show( false );  // Hide the DayZ logo
        m_ImageLogoCorner.Show( false );  // Hide the Bohemia Interactive logo
        
	    UpdateLoadingBackground();
    }

	void UpdateLoadingBackground()
	{
		// The first Show runs inside the DayZGame constructor, before g_Game is set: only m_DayZGame is valid
		if (!m_DayZGame || !m_ImageBackground)
			return;

		float loadingTime = f_LoadingTime;
		float tickTime = m_DayZGame.GetTickTime();

		if (f_LoadingTimeStamp < 0)
		{
			f_LoadingTime = 0;
		}
		else
		{
			f_LoadingTime += tickTime - f_LoadingTimeStamp;
		}

		f_LoadingTimeStamp = tickTime;

		//! Show each loading message and screen at least five seconds
		if (loadingTime > -1 && f_LoadingTime < 5)
			return;

		f_LoadingTime = 0;

		// Get current world name; an unknown or empty name falls back to the default entry
		LoadingScreenBackground backgrounds = null;
		LoadingScreenBackground defaultBackgrounds = null;

		string worldNameLower;
		m_DayZGame.GetWorldName(worldNameLower);
		worldNameLower.ToLower();

		// Find background for current map or default
		if (m_Backgrounds)
		{
			for (int i = 0; i < m_Backgrounds.Count(); i++)
			{
				LoadingScreenBackground current = m_Backgrounds[i];
				if (current)
				{
					string mapNameLower = current.MapName;
					mapNameLower.ToLower();
					if (mapNameLower == worldNameLower)
					{
						backgrounds = current;
						break;
					}

					if (mapNameLower == "default")
					{
						defaultBackgrounds = current;
					}
				}
			}
		}

		// If no matching background found, use default
		if (!backgrounds)
		{
			backgrounds = defaultBackgrounds;
		}

		string backgroundPath;
		if (backgrounds && backgrounds.Path && backgrounds.Path.Count() > 0)
			backgroundPath = backgrounds.GetRandomPath();

		// No list, an empty list or an empty entry: use the first BR background
		if (backgroundPath == "")
			backgroundPath = "Vigrid-BattleRoyale/GUI/textures/loading_screens/br_loading_1.edds";

		m_ImageBackground.LoadImageFile(0, backgroundPath);
	}

    override void OnUpdate(float timeslice)
    {
        super.OnUpdate(timeslice);

		// Switch to another background image
        if (IsLoading())
            UpdateLoadingBackground();
    }
}
#endif

#ifndef SERVER
modded class MainMenu
{
	protected ImageWidget m_Logo;
	protected TextWidget m_PlayButtonLabel;

	protected Widget m_PopupMessage;
	protected RichTextWidget m_PopupText;
	protected ButtonWidget m_PopupButton;
	protected ButtonWidget m_PopupButton_2;
	protected ref PopupButtonCallback popup_onClick;
	protected ref PopupButtonCallback popup_onClick2;

	override Widget Init()
	{
		layoutRoot = GetGame().GetWorkspace().CreateWidgets("Vigrid-BattleRoyale/GUI/layouts/MainMenu/main_menu.layout");

		m_Play						= layoutRoot.FindAnyWidget("play");
		m_PlayButtonLabel			= TextWidget.Cast(m_Play.FindAnyWidget("play_label"));
		m_ChooseServer				= layoutRoot.FindAnyWidget("choose_server");
		m_CustomizeCharacter		= layoutRoot.FindAnyWidget("customize_character");
		m_PlayVideo					= layoutRoot.FindAnyWidget("play_video");
		m_Tutorials					= layoutRoot.FindAnyWidget("tutorials");
		m_TutorialButton			= layoutRoot.FindAnyWidget("tutorial_button");
		m_MessageButton				= layoutRoot.FindAnyWidget("message_button");
		m_SettingsButton			= layoutRoot.FindAnyWidget("settings_button");
		m_Exit						= layoutRoot.FindAnyWidget("exit_button");
		m_PrevCharacter				= layoutRoot.FindAnyWidget("prev_character");
		m_NextCharacter				= layoutRoot.FindAnyWidget("next_character");

		m_DlcFrame 					= layoutRoot.FindAnyWidget("dlc_Frame");
		// Hidden parent for the vanilla news carousel, created by OnShow and CheckWidth since 1.30
		m_NewsCarouselFrame			= layoutRoot.FindAnyWidget("carousel_Frame");
		m_Version					= TextWidget.Cast(layoutRoot.FindAnyWidget("version"));
		m_ModdedWarning				= TextWidget.Cast(layoutRoot.FindAnyWidget("ModdedWarning"));
		m_CharacterRotationFrame	= layoutRoot.FindAnyWidget("character_rotation_frame");

		m_LastPlayedTooltip			= layoutRoot.FindAnyWidget("last_server_info");
		m_LastPlayedTooltip.Show(false);
		m_LastPlayedTooltipLabel	= m_LastPlayedTooltip.FindAnyWidget("last_server_info_label");
		m_LastPlayedTooltipName 	= TextWidget.Cast(m_LastPlayedTooltip.FindAnyWidget("last_server_info_name"));
		m_LastPlayedTooltipIP		= TextWidget.Cast(m_LastPlayedTooltip.FindAnyWidget("last_server_info_ip"));
		m_LastPlayedTooltipPort		= TextWidget.Cast(m_LastPlayedTooltip.FindAnyWidget("last_server_info_port"));

		m_LastPlayedTooltipTimer	= new WidgetFadeTimer();

		m_Stats						= new MainMenuStats(layoutRoot.FindAnyWidget("character_stats_root"));

		m_Mission					= MissionMainMenu.Cast(GetGame().GetMission());

		m_LastFocusedButton 		= m_Play;

		m_ScenePC					= m_Mission.GetIntroScenePC();

		if (m_ScenePC)
		{
			m_ScenePC.ResetIntroCamera();
		}

		m_PlayVideo.Show(false);

		m_PlayerName				= TextWidget.Cast(layoutRoot.FindAnyWidget("character_name_text"));

		// Set Version
		string version;
		GetGame().GetVersion(version);
		m_Version.SetText("#main_menu_version" + " " + version);

		GetGame().GetUIManager().ScreenFadeOut(0);

		SetFocus(null);

		Refresh();

		LoadMods();
	//		PopulateDlcFrame();

		GetDayZGame().GetBacklit().MainMenu_OnShow();
		GetGame().GetMission().GetOnModMenuVisibilityChanged().Insert(ShowDlcFrame);

		g_Game.SetLoadState(DayZLoadState.MAIN_MENU_CONTROLLER_SELECT);

		m_Logo = ImageWidget.Cast( layoutRoot.FindAnyWidget( "dayz_logo" ) );
		if(!m_Logo.LoadImageFile( 0, BATTLEROYALE_LOGO_IMAGE ))
			Error("Failed to load imageset image");

		//m_NewsfeedPanel.Show( false ); //--- don't show dayz expansion news feed

		//m_ChooseServer.Show( false ); // Don't show choose server
		m_TutorialButton.Show( false ); // Don't show tutorial button
		//m_MessageButton.Show( false ); // Don't show credits button
		ref array<string> funny_strings = {
			"Bro, it's DayZ but with Battle Royale stuff!",
			"Welcome to the wild world of DayZ Battle Royale, where the only certainty is uncertainty.",
			"Next time, try to aim better...",
			"Just so you know, you're allergic to bullets.",
			"You Miss 100% Of The Shots You Don't Take.",
			"I should have improved the mod instead of writing this sentence.",
			"Remember, bushes aren't bulletproof.",
			"If at first, you don't succeed, respawn and try again.",
			"Pro tip: Running in a straight line isn't the best strategy.",
			"Warning: May cause sudden addiction to adrenaline and respawn buttons.",
			"Just a friendly reminder: camping in this game is for tents, not players.",
			"Remember, it's not about how many kills you get, but how stylishly you avoid death.",
			"Embrace the chaos, but don't forget to reload.",
			"Just when you thought it was safe to loot...",
			"In this game, karma has a faster firing rate than any weapon."
		};
		m_ModdedWarning.SetText(funny_strings.GetRandomElement());
		//m_ModdedWarning.Show( false ); // Hide modded message

		if ( GetDayZGame() )
		{
			string expansion_version = GetDayZGame().GetExpansionClientVersion();
			string dayzbr_version = GetDayZGame().GetBattleRoyaleClientVersion();
			m_Version.SetText( "Client #main_menu_version" + " " + version + " | " + expansion_version + " | " + dayzbr_version );
		}
		else
		{
			m_Version.SetText( "DayZ SA #main_menu_version" + " " + version );
		}

		if(OnStart())
		{
			Print("You are running a developer build of DayZBR");
		}
		else
		{
			Error("Something Went Wrong! BR Failed To Start?!");
		}

		//ensure popup message is initialized
		if(!m_PopupMessage)
		{
			m_PopupMessage = GetGame().GetWorkspace().CreateWidgets( "Vigrid-BattleRoyale/GUI/widgets/popup_message.layout", layoutRoot );
			m_PopupText = RichTextWidget.Cast( m_PopupMessage.FindAnyWidget( "MessageText" ) );
			m_PopupButton = ButtonWidget.Cast( m_PopupMessage.FindAnyWidget( "PopupButton" ) );
			m_PopupButton_2 = ButtonWidget.Cast( m_PopupMessage.FindAnyWidget( "PopupButton_2" ) ); //TODO: update this for new layout (with new button)
		}
		ClosePopup(); //if init is called twice, close any popup that exists

		BR_OverrideUI();

		m_PlayButtonLabel.SetText("#main_menu_play");

		return layoutRoot;
	}

	void BR_OverrideUI()
	{
	//		m_Play.Show(false);
		m_ChooseServer.Show(false);
		m_TutorialButton.Show(false);
		m_DlcFrame.Show(false);

		if (m_DisplayedDlcHandler)
			m_DisplayedDlcHandler.ShowInfoPanel(false);
	}

	override void FilterDlcs(inout array<ref ModInfo> modArray)
	{
		BR_OverrideUI();
	}

	// Hide the DLC frame
	override void ShowDlcFrame(bool show)
	{
		super.ShowDlcFrame(false);
	}

	// Connects to the server given with -connect, otherwise opens the server browser
	override void Play()
	{
		string address;
		if (GetCLIParam("connect", address) && address != "")
		{
			string port;
			string password;
			GetCLIParam("port", port);
			GetCLIParam("password", password);

			g_Game.ConnectFromServerBrowser(address, port.ToInt(), password);
			return;
		}

		OpenMenuServerBrowser();
	}

	bool OnStart(bool force_restart = false)
	{
		Print("DAYZ BATTLE ROYALE INIT");
		BiosUserManager p_UserManager = GetGame().GetUserManager();
		if(!p_UserManager)
		{
			Error("DBR ERROR: p_UserManager = NULL");
			return false;
		}

		BiosUser p_User = p_UserManager.GetTitleInitiator();
		if(!p_User)
		{
			Error("DBR ERROR: p_User = NULL");
			return false;
		}

		Print("DBR: User: " + p_User.GetUid() + " | " + p_User.GetName());

		return true;
	}

	override void LoadMods()
	{
		super.LoadMods(); //initialize like normal

		//No BR mod found? ya just delete the lists....
		if( m_ModsSimple )
			delete m_ModsSimple;

		if( m_ModsDetailed )
			delete m_ModsDetailed;
	}

	void CreatePopup(string message, string button_text = "", PopupButtonCallback onClickCallback = NULL, string button_text_2 = "", PopupButtonCallback onClickCallback_2 = NULL )
	{
		m_PopupText.SetText(message);
		popup_onClick = onClickCallback;
		popup_onClick2 = onClickCallback_2;
		if(button_text != "")
		{
			//Show button
			m_PopupButton.SetText(button_text);
			m_PopupButton.Show(true);
		}
		else
		{
			//hide button
			m_PopupButton.Show(false);
		}
		if(button_text_2 != "")
		{
			//Show button #2
			m_PopupButton_2.SetText(button_text_2);
			m_PopupButton_2.Show(true);
		}
		else
		{
			//hide button
			m_PopupButton_2.Show(false);
		}

		m_PopupMessage.Show(true);
	}

	void ClosePopup()
	{
		m_PopupMessage.Show(false);
	}

	void PopupActionClicked(int button_num)
	{
		if(button_num == 1)
		{
			if(!popup_onClick)
			{
				Error("POPUP BUTTON CALLBACK NULL!");
				return;
			}
			popup_onClick.OnButtonClick();
		}
		else
		{
			if(!popup_onClick2)
			{
				Error("POPUP BUTTON 2 CALLBACK NULL!");
				return;
			}
			popup_onClick2.OnButtonClick();
		}
	}

	override bool OnClick(Widget w, int x, int y, int button)
	{
		if (button == MouseState.LEFT)
		{
			if (w == m_Play)
			{
				m_LastFocusedButton = m_Play;
				Play();
				return true;
			}
			if(w == m_PopupButton)
			{
				Print("POPUP BUTTON CLICKED");
				PopupActionClicked(1);
				return true;
			}
			if(w == m_PopupButton_2)
			{
				Print("POPUP BUTTON 2 CLICKED");
				PopupActionClicked(2);
				return true;
			}
		}

		return super.OnClick(w, x, y, button);
	}

	// Avoid showing the tooltip
	override bool OnMouseEnter(Widget w, int x, int y)
	{
		if (w == m_Play)
		{
			if (IsFocusable(w))
			{
				ColorHighlight(w);
				return true;
			}
		}

		return super.OnMouseEnter(w, x, y);
	}

	override void ColorNormal(Widget w)
	{
		if (!w)
			return;

		int color_pnl = ARGB(0, 0, 0, 0);
		int color_lbl = ARGB(255, 255, 255, 255);
		int color_img = ARGB(255, 255, 255, 255);

		ButtonSetColor(w, color_pnl);
		ButtonSetTextColor(w, color_lbl);
		ImagenSetColor(w, color_img);
	}
};
#endif

#ifndef SERVER
class BattleRoyaleHud
{
    //! Labels for SetMatchPhase; each one names what its timer counts down to
    static const int PHASE_LABEL_NONE = 0;
    static const int PHASE_LABEL_LOBBY = 1;
    static const int PHASE_LABEL_SPAWN_SELECTION = 2;
    static const int PHASE_LABEL_PREPARING = 3;
    static const int PHASE_LABEL_MATCH_STARTS_IN = 4;
    static const int PHASE_LABEL_FIRST_ZONE_IN = 5;
    static const int PHASE_LABEL_ZONE_CLOSING = 6;
    static const int PHASE_LABEL_ROUND_ENDS_IN = 7;
    static const int PHASE_LABEL_ZONE_DISAPPEARS_IN = 8;
    static const int PHASE_LABEL_NO_SAFE_ZONE = 9;
    static const int PHASE_LABEL_FINAL_ZONE = 10;
    static const int PHASE_LABEL_MATCH_OVER = 11;
    static const int PHASE_LABEL_RESTARTING = 12;
    static const int PHASE_LABEL_STARTING = 13;

    //! Color states of the distance and countdown widgets
    protected static const int DISTANCE_INSIDE = 0;
    protected static const int DISTANCE_REACHABLE = 1;
    protected static const int DISTANCE_HARD = 2;
    protected static const int DISTANCE_UNREACHABLE = 3;

    protected Widget m_Root;

    protected Widget m_PlayerCountPanel;
    protected Widget m_GroupCountPanel;
    protected Widget m_ZoneDistancePanel;
    protected Widget m_KillCountPanel;
    protected Widget m_CountdownPanel;
    protected Widget m_PhasePanel;
    protected Widget m_ZoneDamagePanel;

    protected TextWidget m_PlayerTextWidget;
    protected TextWidget m_GroupTextWidget;
    protected TextWidget m_DistanceTextWidget;
    protected TextWidget m_KillTextWidget;
    protected TextWidget m_CountdownTextWidget;
    protected TextWidget m_PhaseTextWidget;

    protected ImageWidget m_DistanceZoneArrow;
    protected ImageWidget m_ImageClock;

    protected bool is_shown;

    protected int timeRemaining;

    // Last values written to the widgets, so the setters only touch them when something changed
    protected int m_DistanceColorState;
    protected int m_LastDistanceMeters;
    protected float m_LastArrowAngle;
    protected int m_LastPhaseLabel;
    protected int m_LastPhaseSeconds;

    void BattleRoyaleHud( Widget root )
    {
        m_Root = root;

        Init();
        ShowHud( false );
    }

    protected void Init()
    {
        m_PlayerCountPanel = Widget.Cast( m_Root.FindAnyWidget( "PlayerCountPanel" ) );
        m_GroupCountPanel = Widget.Cast( m_Root.FindAnyWidget( "GroupsCountPanel" ) );
        m_ZoneDistancePanel = Widget.Cast( m_Root.FindAnyWidget( "ZoneDistancePanel" ) );
        m_KillCountPanel = Widget.Cast( m_Root.FindAnyWidget( "KillCountPanel" ) );
        m_CountdownPanel = Widget.Cast( m_Root.FindAnyWidget( "CountdownPanel" ) );
        m_PhasePanel = Widget.Cast( m_Root.FindAnyWidget( "PhasePanel" ) );
        m_ZoneDamagePanel = Widget.Cast( m_Root.FindAnyWidget( "ZoneDamagePanel" ) );

        m_PlayerTextWidget = TextWidget.Cast( m_PlayerCountPanel.FindAnyWidget( "PlayerText" ) );
        m_GroupTextWidget = TextWidget.Cast( m_GroupCountPanel.FindAnyWidget( "GroupText" ) );
        m_DistanceTextWidget = TextWidget.Cast( m_ZoneDistancePanel.FindAnyWidget( "DistanceText" ) );
        m_KillTextWidget = TextWidget.Cast( m_KillCountPanel.FindAnyWidget( "KillCountText" ) );
        m_CountdownTextWidget = TextWidget.Cast( m_CountdownPanel.FindAnyWidget( "CountdownText" ) );
        m_PhaseTextWidget = TextWidget.Cast( m_Root.FindAnyWidget( "PhaseText" ) );

        m_DistanceZoneArrow = ImageWidget.Cast( m_Root.FindAnyWidget( "ZoneIcon" ) );
        m_ImageClock = ImageWidget.Cast( m_Root.FindAnyWidget( "CountdownIcon" ) );

        m_PlayerCountPanel.Show( false );
        m_GroupCountPanel.Show( false );
        m_ZoneDistancePanel.Show( false );
        m_KillCountPanel.Show( false );
        m_CountdownPanel.Show( false );
        SetVisible( m_PhasePanel, false );
        SetVisible( m_ZoneDamagePanel, false );

        m_DistanceColorState = -1;
        m_LastDistanceMeters = -2;
        m_LastArrowAngle = -1000;
        m_LastPhaseLabel = PHASE_LABEL_NONE;
        m_LastPhaseSeconds = -1;
    }

    bool Shown()
    {
        return is_shown;
    }

    void ShowHud( bool show )
    {
        is_shown = show;
        m_Root.Show( show );
    }

    //! Shows or hides a widget, calling it only when its visibility changes
    protected void SetVisible( Widget w, bool show )
    {
        if ( w && w.IsVisible() != show )
            w.Show( show );
    }

    //show/hide control
    void ShowCount( bool show )
    {
        SetVisible( m_PlayerCountPanel, show );
    }

    void ShowDistance( bool show )
    {
        SetVisible( m_ZoneDistancePanel, show );
    }

    void ShowKillCount( bool show )
    {
        SetVisible( m_KillCountPanel, show );
    }

    void ShowCountdown( bool show )
    {
        SetVisible( m_CountdownPanel, show );
    }

    void ShowZoneDamage( bool show )
    {
        SetVisible( m_ZoneDamagePanel, show );
    }

    //value control
    void SetDistance(bool isInsideZone, float distExt, float distInt, float angle)
    {
        if(!m_DistanceTextWidget || !m_DistanceZoneArrow || !m_CountdownTextWidget || !m_ImageClock)
        {
            Error("Called SetDistance but widget is null!");
            return;
        }

        int color_state = DISTANCE_INSIDE;
        if ( !isInsideZone )
        {
			// Calculate speed needed to reach the zone in time (m/s)
			float speedNeededToReachZone = 0;

			if (timeRemaining > 0)
			{
				speedNeededToReachZone = distExt / timeRemaining;
			}
			else
			{
				speedNeededToReachZone = 99999; // Infinite speed needed if no time
			}

			if ( BattleRoyaleUtils.CheckLogLevel( BattleRoyaleUtils.DEBUG ) )
				BattleRoyaleUtils.Debug(string.Format("SetDistance: distExt=%1 timeRemaining=%2 speedNeededToReachZone=%3", distExt, timeRemaining, speedNeededToReachZone));

			// Convert m/min thresholds to m/s for comparison (divide by 60)
			float fastThreshold = 400.0 / 60.0;    // 6.67 m/s
			float mediumThreshold = 240.0 / 60.0;  // 4.0 m/s

			if (speedNeededToReachZone > fastThreshold)
			{
				// Need to move faster than 400m/min (6.67m/s) - RED (impossible)
				color_state = DISTANCE_UNREACHABLE;
			}
			else if (speedNeededToReachZone > mediumThreshold)
			{
				// Need to move between 240m/min and 400m/min (4-6.67m/s) - ORANGE (difficult)
				color_state = DISTANCE_HARD;
			}
			else
			{
				// Need to move less than 240m/min (4m/s) - WHITE (feasible)
				color_state = DISTANCE_REACHABLE;
			}
        }

        if ( color_state != m_DistanceColorState )
        {
            m_DistanceColorState = color_state;
            ApplyDistanceColors( color_state );
        }

        // The arrow is turned only when it moved by half a degree or more
        if ( Math.AbsFloat( angle - m_LastArrowAngle ) >= 0.5 )
        {
            m_LastArrowAngle = angle;
            m_DistanceZoneArrow.SetRotation( 0, 0, angle );
        }

        // -1 stands for the "safe" text, otherwise whole metres to the zone
        int meters = -1;
        if ( !isInsideZone )
            meters = Math.Ceil(distExt);

        if ( meters != m_LastDistanceMeters )
        {
            m_LastDistanceMeters = meters;
            if ( isInsideZone )
                m_DistanceTextWidget.SetText( "#STR_BR_SAFE" );
            else
                m_DistanceTextWidget.SetText( meters.ToString() + "m");
        }
    }

    //! Inside the zone everything is white; outside, the distance is red and the countdown shows how hard the zone is to reach
    protected void ApplyDistanceColors( int color_state )
    {
        int distance_color = ARGB(255, 255, 255, 255);
        int countdown_color = ARGB(255, 255, 255, 255);

        if ( color_state != DISTANCE_INSIDE )
            distance_color = COLOR_EXPANSION_NOTIFICATION_ERROR;

        if ( color_state == DISTANCE_HARD )
            countdown_color = COLOR_EXPANSION_NOTIFICATION_ORANGE;
        else if ( color_state == DISTANCE_UNREACHABLE )
            countdown_color = COLOR_EXPANSION_NOTIFICATION_ERROR;

        m_DistanceZoneArrow.SetColor( distance_color );
        m_DistanceTextWidget.SetColor( distance_color );
        m_CountdownTextWidget.SetColor( countdown_color );
        m_ImageClock.SetColor( countdown_color );
    }

    void SetCount(int nb_players, int nb_groups)
    {
        if(!m_PlayerTextWidget || !m_GroupTextWidget)
        {
            Error("Called SetCount but widget is null!");
            return;
        }

        //BattleRoyaleUtils.Trace(string.Format("SetCount: %1 %2", nb_players, nb_groups));

        m_PlayerTextWidget.SetText( nb_players.ToString() );

        if ( nb_groups == -1 )
        {
            m_GroupTextWidget.SetText( "???" );
        }
        else if ( nb_groups == -2 )
		{
            m_GroupCountPanel.Show( false );
            m_GroupTextWidget.Show( false );
		}
        else
        {
            m_GroupTextWidget.SetText( nb_groups.ToString() );
            m_GroupCountPanel.Show( true );
            //m_GroupCountPanel.Show(nb_groups > 0);
        }
    }

    void SetKillCount(int count)
    {
        if(!m_KillTextWidget)
        {
            Error("Called SetKillCount but widget is null!");
            return;
        }
        m_KillTextWidget.SetText( count.ToString() );
    }

    void SetCountdown(int value)
    {
        if(!m_CountdownTextWidget)
        {
            Error("Called SetCountdown but widget is null!");
            return;
        }

        timeRemaining = value;

        m_CountdownTextWidget.SetText( FormatTime( value ) );
    }

    //! Shows the phase label and, when seconds >= 0, the time it counts down to
    void SetMatchPhase(int label, int seconds)
    {
        if ( label == m_LastPhaseLabel && seconds == m_LastPhaseSeconds )
            return;

        m_LastPhaseLabel = label;
        m_LastPhaseSeconds = seconds;

        string key = GetPhaseLabelKey( label );
        if ( key == "" || !m_PhaseTextWidget )
        {
            SetVisible( m_PhasePanel, false );
            return;
        }

        string text = Widget.TranslateString( key );
        if ( seconds >= 0 )
            text = text + " " + FormatTime( seconds );

        m_PhaseTextWidget.SetText( text );
        SetVisible( m_PhasePanel, true );
    }

    protected string GetPhaseLabelKey(int label)
    {
        switch ( label )
        {
            case PHASE_LABEL_LOBBY:
                return "#STR_BR_PHASE_LOBBY";
            case PHASE_LABEL_SPAWN_SELECTION:
                return "#STR_BR_PHASE_SPAWN_SELECTION";
            case PHASE_LABEL_PREPARING:
                return "#STR_BR_PHASE_PREPARING";
            case PHASE_LABEL_MATCH_STARTS_IN:
                return "#STR_BR_PHASE_MATCH_STARTS_IN";
            case PHASE_LABEL_FIRST_ZONE_IN:
                return "#STR_BR_PHASE_FIRST_ZONE_IN";
            case PHASE_LABEL_ZONE_CLOSING:
                return "#STR_BR_PHASE_ZONE_CLOSING";
            case PHASE_LABEL_ROUND_ENDS_IN:
                return "#STR_BR_PHASE_ROUND_ENDS_IN";
            case PHASE_LABEL_ZONE_DISAPPEARS_IN:
                return "#STR_BR_PHASE_ZONE_DISAPPEARS_IN";
            case PHASE_LABEL_NO_SAFE_ZONE:
                return "#STR_BR_PHASE_NO_SAFE_ZONE";
            case PHASE_LABEL_FINAL_ZONE:
                return "#STR_BR_PHASE_FINAL_ZONE";
            case PHASE_LABEL_MATCH_OVER:
                return "#STR_BR_PHASE_MATCH_OVER";
            case PHASE_LABEL_RESTARTING:
                return "#STR_BR_PHASE_RESTARTING";
            case PHASE_LABEL_STARTING:
                return "#STR_BR_PHASE_STARTING";
        }

        return "";
    }

    //! Formats a number of seconds as mm:ss
    protected string FormatTime(int value)
    {
        int seconds = (value % 60);
        string second_string = seconds.ToString();
        if(seconds < 10)
            second_string = "0" + second_string;

        int minutes = (value / 60);
        string minute_string = minutes.ToString();
        if(minutes < 10)
            minute_string = "0" + minute_string;

        return minute_string + ":" + second_string;
    }
}
#endif

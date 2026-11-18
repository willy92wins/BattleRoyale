#ifndef SERVER
//! Leaderboard panel: the top 30 of the current period, shown and hidden with a key (UADayZBRToggleLeaderboard).
//! The first three places are podium cards (second, first and third from left to right), the other 27 a table in
//! two columns. The panel is part of the HUD: it takes no input, hides while a menu is open and comes back after.
//! Opening it asks the server for the data (BattleRoyaleRPC stores the answer); what the client already has is
//! shown at once and replaced when the answer arrives.
//! Every widget has its place and size in leaderboard.layout (written by tools/gen_leaderboard_layout.py): the
//! script only shows, hides, colours and writes them
class BattleRoyaleLeaderboard
{
    static const int TOP_COUNT = 30;            //!< players the server sends
    static const int CARD_COUNT = 3;            //!< podium cards in leaderboard.layout
    static const int ROW_COUNT = 27;            //!< table rows in leaderboard.layout: places 4 to 30
    static const int ROW_LINES = 14;            //!< rows of a table column; the second column starts at place 18

    static const int REQUEST_INTERVAL_MS = 2500;    //!< the server ignores a request less than 2 s after the last one
    static const int RETRY_MS = 6000;               //!< an unanswered request is sent again after this long

    static const string ELLIPSIS = "...";
    static const string INPUT_NAME = "UADayZBRToggleLeaderboard";

    protected Widget m_Root;
    protected Widget m_BackTop;
    protected Widget m_BackTable[ROW_LINES];    //!< backdrop of the table for 1 .. ROW_LINES lines of rows
    protected Widget m_BackMessage;
    protected Widget m_Podium;
    protected Widget m_Table;
    protected Widget m_Message;
    protected Widget m_Head0;
    protected Widget m_Head1;
    protected TextWidget m_Period;
    protected TextWidget m_Status;
    protected TextWidget m_MessageTitle;
    protected TextWidget m_MessageHint;

    protected Widget m_Cards[CARD_COUNT];
    protected Widget m_CardOwn[CARD_COUNT];
    protected TextWidget m_CardName[CARD_COUNT];
    protected TextWidget m_CardPoints[CARD_COUNT];
    protected TextWidget m_CardKills[CARD_COUNT];
    protected TextWidget m_CardWins[CARD_COUNT];

    protected Widget m_Rows[ROW_COUNT];
    protected Widget m_RowBack[ROW_COUNT];
    protected Widget m_RowOwn[ROW_COUNT];
    protected TextWidget m_RowName[ROW_COUNT];
    protected TextWidget m_RowPoints[ROW_COUNT];
    protected TextWidget m_RowKills[ROW_COUNT];
    protected TextWidget m_RowWins[ROW_COUNT];

    protected bool m_Open;              //!< the player wants the panel on screen
    protected bool m_WantRequest;       //!< a request is due: sent as soon as REQUEST_INTERVAL_MS allows
    protected bool m_Waiting;           //!< a request was sent and its answer has not arrived
    protected bool m_Dirty;             //!< what the panel shows is out of date
    protected int m_LastRequest;        //!< GetGame().GetTime() of the last request, -1 for none
    protected int m_RequestSerial;      //!< leaderboard_serial when the last request was sent
    protected int m_ShownSerial;        //!< leaderboard_serial the panel was filled from
    protected int m_ShownScreenW;       //!< screen size the names were cut for
    protected int m_ShownScreenH;
    protected string m_LocalName;       //!< name of the local player's identity, empty until it is known

    void BattleRoyaleLeaderboard( Widget parent )
    {
        m_LastRequest = -1;
        m_RequestSerial = -1;
        m_ShownSerial = -1;
        m_ShownScreenW = -1;
        m_ShownScreenH = -1;

        if ( !GetGame() || !GetGame().GetWorkspace() )
            return;

        m_Root = GetGame().GetWorkspace().CreateWidgets( "Vigrid-BattleRoyale/GUI/layouts/hud/leaderboard.layout", parent );
        if ( !m_Root )
        {
            Error("[BattleRoyaleLeaderboard] leaderboard.layout not loaded!");
            return;
        }

        // A layout without one of the widgets disables the panel instead of failing when it is filled
        if ( !FindWidgets() )
        {
            Error("[BattleRoyaleLeaderboard] leaderboard.layout is not complete");
            m_Root.Unlink();
            m_Root = null;
            return;
        }

        m_Root.Show( false );
    }

    void ~BattleRoyaleLeaderboard()
    {
        if ( m_Root )
            m_Root.Unlink();
    }

    protected bool FindWidgets()
    {
        m_BackTop = m_Root.FindAnyWidget( "LeaderboardBackTop" );
        m_BackMessage = m_Root.FindAnyWidget( "LeaderboardBackMessage" );
        m_Podium = m_Root.FindAnyWidget( "LeaderboardPodium" );
        m_Table = m_Root.FindAnyWidget( "LeaderboardTable" );
        m_Message = m_Root.FindAnyWidget( "LeaderboardMessage" );
        m_Head0 = m_Root.FindAnyWidget( "LeaderboardHead0" );
        m_Head1 = m_Root.FindAnyWidget( "LeaderboardHead1" );
        m_Period = TextWidget.Cast( m_Root.FindAnyWidget( "LeaderboardPeriod" ) );
        m_Status = TextWidget.Cast( m_Root.FindAnyWidget( "LeaderboardStatus" ) );
        m_MessageTitle = TextWidget.Cast( m_Root.FindAnyWidget( "LeaderboardMessageTitle" ) );
        m_MessageHint = TextWidget.Cast( m_Root.FindAnyWidget( "LeaderboardMessageHint" ) );
        if ( !m_BackTop || !m_BackMessage || !m_Podium || !m_Table || !m_Message || !m_Head0 || !m_Head1 )
            return false;
        if ( !m_Period || !m_Status || !m_MessageTitle || !m_MessageHint )
            return false;

        int i;
        int lines;
        string name;
        for ( i = 0; i < ROW_LINES; i++ )
        {
            lines = i + 1;
            name = "LeaderboardBackTable" + lines.ToString();
            m_BackTable[i] = m_Root.FindAnyWidget( name );
            if ( !m_BackTable[i] )
                return false;
        }

        for ( i = 0; i < CARD_COUNT; i++ )
        {
            name = "LeaderboardCard" + i.ToString();
            m_Cards[i] = m_Root.FindAnyWidget( name );
            m_CardOwn[i] = m_Root.FindAnyWidget( name + "Own" );
            m_CardName[i] = TextWidget.Cast( m_Root.FindAnyWidget( name + "Name" ) );
            m_CardPoints[i] = TextWidget.Cast( m_Root.FindAnyWidget( name + "Points" ) );
            m_CardKills[i] = TextWidget.Cast( m_Root.FindAnyWidget( name + "Kills" ) );
            m_CardWins[i] = TextWidget.Cast( m_Root.FindAnyWidget( name + "Wins" ) );
            if ( !m_Cards[i] || !m_CardOwn[i] || !m_CardName[i] || !m_CardPoints[i] || !m_CardKills[i] || !m_CardWins[i] )
                return false;
        }

        for ( i = 0; i < ROW_COUNT; i++ )
        {
            name = "LeaderboardRow" + i.ToString();
            m_Rows[i] = m_Root.FindAnyWidget( name );
            m_RowBack[i] = m_Root.FindAnyWidget( name + "Back" );
            m_RowOwn[i] = m_Root.FindAnyWidget( name + "Own" );
            m_RowName[i] = TextWidget.Cast( m_Root.FindAnyWidget( name + "Name" ) );
            m_RowPoints[i] = TextWidget.Cast( m_Root.FindAnyWidget( name + "Points" ) );
            m_RowKills[i] = TextWidget.Cast( m_Root.FindAnyWidget( name + "Kills" ) );
            m_RowWins[i] = TextWidget.Cast( m_Root.FindAnyWidget( name + "Wins" ) );
            if ( !m_Rows[i] || !m_RowBack[i] || !m_RowOwn[i] || !m_RowName[i] || !m_RowPoints[i] || !m_RowKills[i] || !m_RowWins[i] )
                return false;
        }

        return true;
    }

    bool IsOpen()
    {
        return m_Open;
    }

    //! The key: shows the panel and asks for the data, or hides it
    void Toggle()
    {
        if ( !m_Root )
            return;

        m_Open = !m_Open;
        if ( m_Open )
        {
            m_WantRequest = true;
            m_Dirty = true;
        }
        else
        {
            m_WantRequest = false;
            m_Root.Show( false );
        }
    }

    //! Called every frame; menu_open: a menu is on screen, which hides the panel until it closes.
    //! A closed panel costs one comparison; an open one fills its widgets only when something changed
    void Update( bool menu_open )
    {
        if ( !m_Open || !m_Root )
            return;

        bool show = !menu_open;
        if ( m_Root.IsVisible() != show )
            m_Root.Show( show );
        if ( !show )
            return;

        BattleRoyaleRPC br_rpc = BattleRoyaleRPC.GetInstance();
        int now = GetGame().GetTime();

        if ( m_Waiting && br_rpc.leaderboard_serial != m_RequestSerial )
        {
            m_Waiting = false;
            m_Dirty = true;
        }
        if ( m_Waiting && now - m_LastRequest >= RETRY_MS )
            m_WantRequest = true;
        if ( m_WantRequest && ( m_LastRequest < 0 || now - m_LastRequest >= REQUEST_INTERVAL_MS ) )
        {
            m_WantRequest = false;
            m_Waiting = true;
            m_LastRequest = now;
            m_RequestSerial = br_rpc.leaderboard_serial;
            m_Dirty = true;
            GetRPCManager().SendRPC( RPC_DAYZBRSERVER_NAMESPACE, "RequestLeaderboard", NULL, true );
        }

        // The names are cut to the width of their column as it is drawn: another screen size cuts them again
        int screen_w;
        int screen_h;
        GetScreenSize( screen_w, screen_h );
        if ( br_rpc.leaderboard_serial != m_ShownSerial || screen_w != m_ShownScreenW || screen_h != m_ShownScreenH )
            m_Dirty = true;

        if ( !m_Dirty )
            return;

        m_Dirty = false;
        m_ShownSerial = br_rpc.leaderboard_serial;
        m_ShownScreenW = screen_w;
        m_ShownScreenH = screen_h;
        Refresh( br_rpc );
    }

    protected void Refresh( BattleRoyaleRPC br_rpc )
    {
        string wipe;
        string period;
        array<string> names;
        array<int> points;
        array<int> kills;
        array<int> wins;

        if ( !br_rpc.GetLeaderboard( wipe, period, names, points, kills, wins ) || !names || !points || !kills || !wins )
        {
            m_Period.SetText( "" );
            m_Status.SetText( GetCloseHint() );
            ShowMessage( "#STR_BR_LEADERBOARD_LOADING", "" );
            return;
        }

        m_Period.SetText( GetPeriodText( wipe, period ) );
        if ( m_Waiting )
            m_Status.SetText( "#STR_BR_LEADERBOARD_UPDATING" );
        else
            m_Status.SetText( GetCloseHint() );

        int count = names.Count();
        if ( points.Count() < count )
            count = points.Count();
        if ( kills.Count() < count )
            count = kills.Count();
        if ( wins.Count() < count )
            count = wins.Count();
        if ( count > TOP_COUNT )
            count = TOP_COUNT;

        if ( count == 0 )
        {
            ShowMessage( "#STR_BR_LEADERBOARD_EMPTY", "#STR_BR_LEADERBOARD_EMPTY_HINT" );
            return;
        }

        ShowContent( count );

        string local_name = GetLocalName();
        bool own;
        int i;
        int index;
        int value;
        Widget frame;
        Widget back;
        Widget own_mark;
        TextWidget name_text;
        TextWidget value_text;

        for ( i = 0; i < CARD_COUNT; i++ )
        {
            frame = m_Cards[i];
            frame.Show( i < count );
            if ( i >= count )
                continue;

            own = local_name != "" && names.Get( i ) == local_name;
            own_mark = m_CardOwn[i];
            own_mark.Show( own );
            name_text = m_CardName[i];
            SetFittedText( name_text, names.Get( i ) );
            value_text = m_CardPoints[i];
            value = points.Get( i );
            value_text.SetText( value.ToString() );
            value_text = m_CardKills[i];
            value = kills.Get( i );
            value_text.SetText( value.ToString() );
            value_text = m_CardWins[i];
            value = wins.Get( i );
            value_text.SetText( value.ToString() );
        }

        for ( i = 0; i < ROW_COUNT; i++ )
        {
            index = i + CARD_COUNT;
            frame = m_Rows[i];
            frame.Show( index < count );
            if ( index >= count )
                continue;

            own = local_name != "" && names.Get( index ) == local_name;
            back = m_RowBack[i];
            own_mark = m_RowOwn[i];
            name_text = m_RowName[i];
            own_mark.Show( own );
            if ( own )
            {
                // The gold of the kill feed rows that name the local player
                back.SetColor( ARGB( 66, 255, 196, 0 ) );
                name_text.SetColor( ARGB( 255, 255, 196, 0 ) );
            }
            else
            {
                // Alternate tones, by the line of the row in its column
                if ( ( i % ROW_LINES ) % 2 == 0 )
                    back.SetColor( ARGB( 56, 0, 0, 0 ) );
                else
                    back.SetColor( ARGB( 102, 0, 0, 0 ) );
                name_text.SetColor( ARGB( 255, 255, 255, 255 ) );
            }

            SetFittedText( name_text, names.Get( index ) );
            value_text = m_RowPoints[i];
            value = points.Get( index );
            value_text.SetText( value.ToString() );
            value_text = m_RowKills[i];
            value = kills.Get( index );
            value_text.SetText( value.ToString() );
            value_text = m_RowWins[i];
            value = wins.Get( index );
            value_text.SetText( value.ToString() );
        }
    }

    //! Podium and table for count players (at least one), with the backdrop as tall as the rows in use
    protected void ShowContent( int count )
    {
        m_BackMessage.Show( false );
        m_Message.Show( false );
        m_BackTop.Show( true );
        m_Podium.Show( true );
        m_Table.Show( true );

        int lines = count - CARD_COUNT;
        if ( lines > ROW_LINES )
            lines = ROW_LINES;

        Widget back;
        for ( int i = 0; i < ROW_LINES; i++ )
        {
            back = m_BackTable[i];
            back.Show( i + 1 == lines );
        }

        m_Head0.Show( count > CARD_COUNT );
        m_Head1.Show( count > CARD_COUNT + ROW_LINES );
    }

    //! Loading and empty states: the title band over a short backdrop with a line and, under it, a hint (empty for none)
    protected void ShowMessage( string title, string hint )
    {
        m_BackTop.Show( false );
        m_Podium.Show( false );
        m_Table.Show( false );

        Widget back;
        for ( int i = 0; i < ROW_LINES; i++ )
        {
            back = m_BackTable[i];
            back.Show( false );
        }

        m_BackMessage.Show( true );
        m_Message.Show( true );
        m_MessageTitle.SetText( title );
        m_MessageHint.SetText( hint );
    }

    //! Writes text in the widget, cut with an ellipsis when it is wider than the widget. The widget must be on
    //! screen: the size of a text is the size it is drawn with
    protected void SetFittedText( TextWidget widget, string text )
    {
        widget.SetText( text );

        float box_w;
        float box_h;
        widget.GetScreenSize( box_w, box_h );
        if ( box_w <= 0 )
            return;

        int text_w;
        int text_h;
        widget.GetTextSize( text_w, text_h );
        if ( text_w <= box_w )
            return;

        // First cut in proportion to the overflow, then one character at a time
        int chars = text.LengthUtf8();
        int fit = chars * box_w / text_w;
        if ( fit < chars )
            chars = fit + 1;
        while ( chars > 1 && text_w > box_w )
        {
            chars--;
            widget.SetText( text.SubstringUtf8( 0, chars ) + ELLIPSIS );
            widget.GetTextSize( text_w, text_h );
        }
    }

    //! "Monthly season: 2026-11"; the period label alone for a wipe mode this client does not know
    protected string GetPeriodText( string wipe, string period )
    {
        string key = "";
        if ( wipe == "monthly" )
            key = "STR_BR_LEADERBOARD_PERIOD_MONTHLY";
        else if ( wipe == "weekly" )
            key = "STR_BR_LEADERBOARD_PERIOD_WEEKLY";
        else if ( wipe == "yearly" )
            key = "STR_BR_LEADERBOARD_PERIOD_YEARLY";

        if ( key == "" || period == "" )
            return period;

        StringLocaliser text = new StringLocaliser( key, period );
        return text.Format();
    }

    //! "T: close" with the key the action is bound to, empty when it has none
    protected string GetCloseHint()
    {
        string button = InputUtils.GetButtonNameFromInput( INPUT_NAME, EInputDeviceType.MOUSE_AND_KEYBOARD );
        if ( button == "" )
            return "";

        StringLocaliser text = new StringLocaliser( "STR_BR_LEADERBOARD_CLOSE_HINT", button );
        return text.Format();
    }

    //! Name the leaderboard has for the local player: the one of the identity, read while the player has a
    //! character and kept (dead or spectating there is none); until then, the name of the profile
    protected string GetLocalName()
    {
        if ( m_LocalName != "" )
            return m_LocalName;

        Man player = GetGame().GetPlayer();
        if ( player && player.GetIdentity() )
        {
            m_LocalName = player.GetIdentity().GetName();
            return m_LocalName;
        }

        string profile_name;
        GetGame().GetPlayerName( profile_name );
        return profile_name;
    }
}
#endif

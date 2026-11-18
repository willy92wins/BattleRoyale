#ifndef SERVER
//! Leaderboard panel: the top 30 of the current period, shown and hidden with a key (UADayZBRToggleLeaderboard).
//! The first three places are podium cards (second, first and third from left to right), the other 27 a table:
//! two columns with the rows split evenly, or one centred column when the rows are few.
//! The panel is part of the HUD: it takes no input, hides while a menu is open and comes back after.
//! Opening it asks the server for the data (BattleRoyaleRPC stores the answer); what the client already has is
//! shown at once and replaced when the answer arrives. A match that ends with the panel open asks again.
//! Every widget has its place and size in leaderboard.layout (written by tools/gen_leaderboard_layout.py): the
//! script only shows, hides, colours and writes them
class BattleRoyaleLeaderboard
{
    static const int TOP_COUNT = 30;            //!< players the server sends
    static const int CARD_COUNT = 3;            //!< podium cards in leaderboard.layout
    static const int ROW_LINES = 14;            //!< rows of the left column: LeaderboardRow0 to 13
    static const int SIDE_ROWS = 27;            //!< rows of the left and right columns; the right one is rows 14 to 26
    static const int CENTRE_LINES = 5;          //!< rows of the centred column: LeaderboardRow27 to 31
    static const int ROW_COUNT = 32;            //!< SIDE_ROWS + CENTRE_LINES
    static const int HEAD_COUNT = 3;            //!< column headers: left, right, centred
    static const int BACK_COUNT = 16;           //!< backdrops: 0 loading and empty, 1 podium alone, 1 + n podium and n lines of rows

    static const int REQUEST_INTERVAL_MS = 2500;    //!< the server ignores a request less than 2 s after the last one
    static const int RETRY_MS = 6000;               //!< an unanswered request is sent again after this long
    static const int MATCH_END_DELAY_MS = 1500;     //!< after the match ends, before asking for the new points
    static const int FADE_IN_MS = 150;              //!< as the kill feed rows

    static const string ELLIPSIS = "...";
    static const string INPUT_NAME = "UADayZBRToggleLeaderboard";

    protected Widget m_Root;
    protected Widget m_Panel;
    protected Widget m_Backs[BACK_COUNT];       //!< one backdrop for each height of the panel; one is shown
    protected Widget m_Podium;
    protected Widget m_Table;
    protected Widget m_Message;
    protected Widget m_Heads[HEAD_COUNT];
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
    protected TextWidget m_RowRank[ROW_COUNT];
    protected TextWidget m_RowName[ROW_COUNT];
    protected TextWidget m_RowPoints[ROW_COUNT];
    protected TextWidget m_RowKills[ROW_COUNT];
    protected TextWidget m_RowWins[ROW_COUNT];

    protected bool m_Open;              //!< the player wants the panel on screen
    protected bool m_WantRequest;       //!< a request is due: sent as soon as REQUEST_INTERVAL_MS allows
    protected bool m_Waiting;           //!< a request was sent and its answer has not arrived
    protected bool m_Dirty;             //!< what the panel shows is out of date
    protected bool m_Fading;            //!< the panel is fading in
    protected int m_FadeStart;          //!< GetGame().GetTime() of the first frame of the fade, -1 until it is drawn
    protected int m_LastRequest;        //!< GetGame().GetTime() of the last request, -1 for none
    protected int m_RequestSerial;      //!< leaderboard_serial when the last request was sent
    protected int m_ShownSerial;        //!< leaderboard_serial the panel was filled from
    protected int m_ShownScreenW;       //!< screen size the names were cut for
    protected int m_ShownScreenH;
    protected int m_MatchPhase;         //!< match_phase last seen with the panel open
    protected int m_MatchEndRequest;    //!< GetGame().GetTime() at which to ask for the points of the match that ended, -1 for none
    protected string m_LocalName;       //!< name of the local player's identity, empty until it is known

    void BattleRoyaleLeaderboard( Widget parent )
    {
        m_FadeStart = -1;
        m_LastRequest = -1;
        m_RequestSerial = -1;
        m_ShownSerial = -1;
        m_ShownScreenW = -1;
        m_ShownScreenH = -1;
        m_MatchPhase = -1;
        m_MatchEndRequest = -1;

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
        m_Panel = m_Root.FindAnyWidget( "LeaderboardPanel" );
        m_Podium = m_Root.FindAnyWidget( "LeaderboardPodium" );
        m_Table = m_Root.FindAnyWidget( "LeaderboardTable" );
        m_Message = m_Root.FindAnyWidget( "LeaderboardMessage" );
        m_Period = TextWidget.Cast( m_Root.FindAnyWidget( "LeaderboardPeriod" ) );
        m_Status = TextWidget.Cast( m_Root.FindAnyWidget( "LeaderboardStatus" ) );
        m_MessageTitle = TextWidget.Cast( m_Root.FindAnyWidget( "LeaderboardMessageTitle" ) );
        m_MessageHint = TextWidget.Cast( m_Root.FindAnyWidget( "LeaderboardMessageHint" ) );
        if ( !m_Panel || !m_Podium || !m_Table || !m_Message )
            return false;
        if ( !m_Period || !m_Status || !m_MessageTitle || !m_MessageHint )
            return false;

        int i;
        string name;
        for ( i = 0; i < BACK_COUNT; i++ )
        {
            name = "LeaderboardBack" + i.ToString();
            m_Backs[i] = m_Root.FindAnyWidget( name );
            if ( !m_Backs[i] )
                return false;
        }

        for ( i = 0; i < HEAD_COUNT; i++ )
        {
            name = "LeaderboardHead" + i.ToString();
            m_Heads[i] = m_Root.FindAnyWidget( name );
            if ( !m_Heads[i] )
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
            m_RowRank[i] = TextWidget.Cast( m_Root.FindAnyWidget( name + "Rank" ) );
            m_RowName[i] = TextWidget.Cast( m_Root.FindAnyWidget( name + "Name" ) );
            m_RowPoints[i] = TextWidget.Cast( m_Root.FindAnyWidget( name + "Points" ) );
            m_RowKills[i] = TextWidget.Cast( m_Root.FindAnyWidget( name + "Kills" ) );
            m_RowWins[i] = TextWidget.Cast( m_Root.FindAnyWidget( name + "Wins" ) );
            if ( !m_Rows[i] || !m_RowBack[i] || !m_RowOwn[i] || !m_RowRank[i] || !m_RowName[i] || !m_RowPoints[i] || !m_RowKills[i] || !m_RowWins[i] )
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

        if ( m_Open )
        {
            Close();
            return;
        }

        m_Open = true;
        m_WantRequest = true;
        m_Dirty = true;
        m_MatchPhase = BattleRoyaleRPC.GetInstance().match_phase;
        m_MatchEndRequest = -1;

        // Fades in from the first frame it is drawn
        m_Fading = true;
        m_FadeStart = -1;
        m_Panel.SetAlpha( 0 );
    }

    //! Hides the panel until the key opens it again; also what the win screen does, which takes the same place
    void Close()
    {
        if ( !m_Root || !m_Open )
            return;

        m_Open = false;
        m_WantRequest = false;
        m_Root.Show( false );
    }

    //! Top edge of the panel in screen pixels while it is on screen, -1 when it is not: the kill feed keeps
    //! its rows above it
    float GetScreenTop()
    {
        if ( !m_Root || !m_Open || !m_Root.IsVisible() )
            return -1;

        float x;
        float y;
        m_Panel.GetScreenPos( x, y );
        return y;
    }

    //! Called every frame; menu_open: a menu is on screen, which hides the panel until it closes.
    //! A closed panel costs one comparison; an open one fills its widgets only when something changed
    void Update( bool menu_open )
    {
        if ( !m_Open || !m_Root )
            return;

        BattleRoyaleRPC br_rpc = BattleRoyaleRPC.GetInstance();
        int now = GetGame().GetTime();

        // The server adds the points of a match when it ends: with the panel open, ask for them
        if ( br_rpc.match_phase != m_MatchPhase )
        {
            m_MatchPhase = br_rpc.match_phase;
            if ( m_MatchPhase == EBRMatchPhase.FINISHED )
                m_MatchEndRequest = now + MATCH_END_DELAY_MS;
        }
        if ( m_MatchEndRequest >= 0 && now >= m_MatchEndRequest )
        {
            m_MatchEndRequest = -1;
            m_WantRequest = true;
        }

        bool show = !menu_open;
        if ( m_Root.IsVisible() != show )
            m_Root.Show( show );
        if ( !show )
            return;

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

        if ( m_Dirty )
        {
            m_Dirty = false;
            m_ShownSerial = br_rpc.leaderboard_serial;
            m_ShownScreenW = screen_w;
            m_ShownScreenH = screen_h;
            Refresh( br_rpc );
        }

        if ( m_Fading )
        {
            if ( m_FadeStart < 0 )
                m_FadeStart = now;

            float elapsed = now - m_FadeStart;
            float alpha = elapsed / FADE_IN_MS;
            if ( alpha >= 1 )
            {
                alpha = 1;
                m_Fading = false;
            }
            // Every widget of the panel inherits its alpha (inheritalpha 1 in the layout)
            m_Panel.SetAlpha( alpha );
        }
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

        // Rows of the table: split evenly between the left and the right column (the left one takes the odd
        // row), or all in the centred column when they are few. Same rule as tools/gen_leaderboard_layout.py
        int rows = count - CARD_COUNT;
        if ( rows < 0 )
            rows = 0;
        int left = 0;
        int right = 0;
        int centre = 0;
        if ( rows <= CENTRE_LINES )
        {
            centre = rows;
        }
        else
        {
            left = ( rows + 1 ) / 2;
            right = rows - left;
        }

        int lines = left;
        if ( centre > lines )
            lines = centre;
        ShowContent( 1 + lines, left > 0, right > 0, centre > 0 );

        string local_name = GetLocalName();
        bool own;
        int i;
        int value;
        Widget frame;
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
            value = points.Get( i );
            value_text = m_CardPoints[i];
            value_text.SetText( value.ToString() );
            value = kills.Get( i );
            value_text = m_CardKills[i];
            value_text.SetText( value.ToString() );
            value = wins.Get( i );
            value_text = m_CardWins[i];
            value_text.SetText( value.ToString() );
        }

        // Left column, right column, centred column: the first index each one shows and how many rows
        for ( i = 0; i < ROW_LINES; i++ )
        {
            FillRow( i, i, CARD_COUNT + i, i < left, names, points, kills, wins, local_name );
        }
        for ( i = ROW_LINES; i < SIDE_ROWS; i++ )
        {
            FillRow( i, i - ROW_LINES, CARD_COUNT + left + i - ROW_LINES, i - ROW_LINES < right, names, points, kills, wins, local_name );
        }
        for ( i = SIDE_ROWS; i < ROW_COUNT; i++ )
        {
            FillRow( i, i - SIDE_ROWS, CARD_COUNT + i - SIDE_ROWS, i - SIDE_ROWS < centre, names, points, kills, wins, local_name );
        }
    }

    //! row: the row widgets; line: its line in its column, for the alternate tones; index: the player it shows;
    //! shown false hides the row
    protected void FillRow( int row, int line, int index, bool shown, array<string> names, array<int> points, array<int> kills, array<int> wins, string local_name )
    {
        Widget frame = m_Rows[row];
        frame.Show( shown );
        if ( !shown )
            return;

        bool own = local_name != "" && names.Get( index ) == local_name;
        Widget back = m_RowBack[row];
        Widget own_mark = m_RowOwn[row];
        TextWidget name_text = m_RowName[row];
        TextWidget value_text;
        int value;

        // Colours of tools/gen_leaderboard_layout.py: OWN_TINT and OWN_NAME, ZEBRA and TEXT
        own_mark.Show( own );
        back.Show( own || line % 2 == 1 );
        if ( own )
        {
            // The local player: a narrow gold bar, a faint warm tint and the name in warm white
            back.SetColor( ARGB( 56, 143, 110, 5 ) );
            name_text.SetColor( ARGB( 255, 255, 237, 184 ) );
        }
        else
        {
            // Every other row a little lighter than the panel
            back.SetColor( ARGB( 51, 77, 82, 92 ) );
            name_text.SetColor( ARGB( 255, 242, 242, 242 ) );
        }

        value = index + 1;
        value_text = m_RowRank[row];
        value_text.SetText( value.ToString() );
        SetFittedText( name_text, names.Get( index ) );
        value = points.Get( index );
        value_text = m_RowPoints[row];
        value_text.SetText( value.ToString() );
        value = kills.Get( index );
        value_text = m_RowKills[row];
        value_text.SetText( value.ToString() );
        value = wins.Get( index );
        value_text = m_RowWins[row];
        value_text.SetText( value.ToString() );
    }

    //! Podium and table over the backdrop of their height (m_Backs index), with the headers of the columns in use
    protected void ShowContent( int back, bool left, bool right, bool centre )
    {
        ShowBack( back );
        m_Message.Show( false );
        m_Podium.Show( true );
        m_Table.Show( true );

        Widget widget;
        widget = m_Heads[0];
        widget.Show( left );
        widget = m_Heads[1];
        widget.Show( right );
        widget = m_Heads[2];
        widget.Show( centre );
    }

    //! Shows the backdrop of that index and hides the others
    protected void ShowBack( int index )
    {
        Widget back;
        for ( int i = 0; i < BACK_COUNT; i++ )
        {
            back = m_Backs[i];
            back.Show( i == index );
        }
    }

    //! Loading and empty states: the title band over a short backdrop with a line and, under it, a hint (empty for none)
    protected void ShowMessage( string title, string hint )
    {
        ShowBack( 0 );
        m_Podium.Show( false );
        m_Table.Show( false );
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

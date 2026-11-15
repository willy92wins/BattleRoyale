#ifndef SERVER
//! Kill feed: a fixed pool of rows filled from the events stored by BattleRoyaleRPC, newest on top.
//! A row is a dark panel with an icon for the cause, a sentence naming killer and victim and, after it,
//! the weapon and the distance. Rows slide and fade in, and fade out before they expire.
//! The panel is level with the top of the HUD and right aligned beside what its top right corner shows (match info
//! column, minimap); each row is as wide as its content, grows to the left and stays clear of the centred phase lines
class BattleRoyaleKillFeed
{
    static const int ROW_COUNT = 6;             //!< rows in killfeed.layout; at most BattleRoyaleRPC.KILL_FEED_SIZE
    static const int ROW_LIFETIME_MS = 6000;
    static const int FADE_IN_MS = 150;
    static const int FADE_OUT_MS = 700;
    static const float SLIDE_IN_PX = 12;        //!< a new row starts this far to the right

    // Geometry of killfeed.layout, in layout units (set from the script times PxPerUnit)
    static const float ROW_PITCH = 40;          //!< pitch of the match info rows: the first rows line up with them
    static const float ROW_HEIGHT = 30;
    static const float TEXT_X = 42;
    static const float DETAIL_Y = 5;
    static const float DETAIL_GAP = 12;         //!< between the sentence and the detail
    static const float ROW_PADDING = 10;        //!< after the last text
    static const float ROW_WIDTH_UNMEASURED = 620;  //!< row width when the text could not be measured
    static const string ELLIPSIS = "...";

    // Place of KillFeedPanel, right aligned in the HUD frame, in layout units
    static const float PANEL_TOP = 1;           //!< below the top of the HUD frame: row centres level with the match info rows
    static const float PANEL_GAP = 16;          //!< between the rows and what the top right corner shows
    static const float PLACE_HYSTERESIS = 24;   //!< the panel moves right only once the room grew this much
    static const float CENTRE_GAP = 16;         //!< between the rows and the centred phase lines
    static const float CENTRE_RELAYOUT_PX = 8;  //!< centred lines that grew this much (screen pixels) lay the rows out again

    static const int NAME_MAX_CHARS = 24;       //!< longer player names are cut
    static const int NAME_SHORT_CHARS = 12;     //!< names of a row too wide for its room

    static const float BACK_ALPHA = 0.55;
    static const float BACK_ALPHA_OWN = 0.8;    //!< row that names the local player
    static const float ACCENT_ALPHA = 0.5;

    protected Widget m_Root;
    protected Widget m_Panel;
    protected float m_PanelOffset;  //!< x of KillFeedPanel last set by Place (layout units), -1 while it has the one of the layout
    protected float m_PanelPxPerUnit;       //!< PxPerUnit() m_PanelOffset was set with
    protected float m_LayoutOffset;         //!< m_PanelOffset the rows on screen were laid out for
    protected float m_LayoutCentreRight;    //!< right edge of the centred lines the rows were laid out for, -1 for none
    protected int m_LayoutScreenW;          //!< screen width the rows on screen were laid out for, -1 for none
    protected int m_LayoutScreenH;          //!< screen height the rows on screen were laid out for, -1 for none
    protected Widget m_Rows[ROW_COUNT];
    protected Widget m_Backs[ROW_COUNT];
    protected Widget m_Accents[ROW_COUNT];
    protected ImageWidget m_Icons[ROW_COUNT];
    protected TextWidget m_Texts[ROW_COUNT];
    protected TextWidget m_Details[ROW_COUNT];

    protected int m_LastSerial;     //!< kill_feed_serial the rows were built from
    protected int m_NextExpiry;     //!< GetGame().GetTime() at which the oldest shown row expires, -1 if no row is shown
    protected int m_ShownRows;      //!< rows 0 .. m_ShownRows - 1 show an event

    protected int m_RowTime[ROW_COUNT];         //!< reception time of the event in the row
    protected int m_RowIcon[ROW_COUNT];         //!< EBRKillFeedType whose icon the row has loaded, -1 for the one of the layout
    protected bool m_RowOwn[ROW_COUNT];         //!< the row names the local player
    protected float m_RowAlpha[ROW_COUNT];      //!< fade last applied to the row, -1 to force it

    protected int m_LocalPlayerId;  //!< PlayerIdentity.GetPlayerId() of the local player, -1 until it is known

    void BattleRoyaleKillFeed( Widget parent )
    {
        m_LastSerial = -1;
        m_NextExpiry = -1;
        m_LocalPlayerId = -1;
        m_PanelOffset = -1;
        m_PanelPxPerUnit = -1;
        m_LayoutOffset = -1;
        m_LayoutCentreRight = -1;
        m_LayoutScreenW = -1;
        m_LayoutScreenH = -1;

        if ( !GetGame() || !GetGame().GetWorkspace() )
            return;

        m_Root = GetGame().GetWorkspace().CreateWidgets( "Vigrid-BattleRoyale/GUI/layouts/hud/killfeed.layout", parent );
        if ( !m_Root )
        {
            Error("[BattleRoyaleKillFeed] killfeed.layout not loaded!");
            return;
        }

        m_Panel = m_Root.FindAnyWidget( "KillFeedPanel" );
        if ( !m_Panel )
        {
            Error("[BattleRoyaleKillFeed] killfeed.layout has no KillFeedPanel");
            m_Root.Unlink();
            m_Root = null;
            return;
        }

        string row_name;
        for ( int i = 0; i < ROW_COUNT; i++ )
        {
            row_name = "KillFeedRow" + i.ToString();
            m_Rows[i] = m_Root.FindAnyWidget( row_name );
            m_Backs[i] = m_Root.FindAnyWidget( row_name + "Back" );
            m_Accents[i] = m_Root.FindAnyWidget( row_name + "Accent" );
            m_Icons[i] = ImageWidget.Cast( m_Root.FindAnyWidget( row_name + "Icon" ) );
            m_Texts[i] = TextWidget.Cast( m_Root.FindAnyWidget( row_name + "Text" ) );
            m_Details[i] = TextWidget.Cast( m_Root.FindAnyWidget( row_name + "Detail" ) );

            m_RowIcon[i] = -1;
            m_RowAlpha[i] = -1;

            // A layout without one of the widgets disables the feed instead of failing every frame
            if ( !m_Rows[i] || !m_Backs[i] || !m_Accents[i] || !m_Icons[i] || !m_Texts[i] || !m_Details[i] )
            {
                Error("[BattleRoyaleKillFeed] killfeed.layout has no complete " + row_name);
                m_Root.Unlink();
                m_Root = null;
                return;
            }

            Widget row_widget = m_Rows[i];
            row_widget.Show( false );
        }
    }

    void ~BattleRoyaleKillFeed()
    {
        if ( m_Root )
            m_Root.Unlink();
    }

    //! Called every frame; rebuilds the rows only when an event arrived or a row expired, keeps the panel beside
    //! the top right corner of the HUD while rows are on screen, and moves the fade of the rows.
    //! hud, minimap and compass may be null
    void Update( BattleRoyaleHud hud, BattleRoyaleMiniMap minimap, BattleRoyaleCompass compass )
    {
        if ( !m_Root )
            return;

        BattleRoyaleRPC br_rpc = BattleRoyaleRPC.GetInstance();
        int now = GetGame().GetTime();
        bool refresh = br_rpc.kill_feed_serial != m_LastSerial || ( m_NextExpiry >= 0 && now >= m_NextExpiry );

        // No row on screen and none coming: nothing to place or to fade
        if ( !refresh && m_ShownRows == 0 )
            return;

        Place( hud, minimap );

        // Rows stay clear of everything centred at the top: the phase lines and the compass under them
        float centre_right = -1;
        if ( hud )
            centre_right = hud.GetCentreLinesRight();
        if ( compass )
        {
            float compass_right = compass.GetScreenRight();
            if ( compass_right > centre_right )
                centre_right = compass_right;
        }

        int screen_w;
        int screen_h;
        GetScreenSize( screen_w, screen_h );

        // Rows on screen are laid out again when their room changed: the panel moved, the centred lines grew,
        // or the screen changed size (the right-anchored panel and the centred lines move, neither check above sees it)
        if ( !refresh && ( m_PanelOffset != m_LayoutOffset || centre_right > m_LayoutCentreRight + CENTRE_RELAYOUT_PX || screen_w != m_LayoutScreenW || screen_h != m_LayoutScreenH ) )
            refresh = true;

        if ( refresh )
        {
            m_LayoutOffset = m_PanelOffset;
            m_LayoutCentreRight = centre_right;
            m_LayoutScreenW = screen_w;
            m_LayoutScreenH = screen_h;
            Refresh( br_rpc, now, RowRoom( centre_right ) );
        }

        Animate( now );
    }

    //! Puts the right edge of the panel PANEL_GAP left of the leftmost thing the top right corner of the HUD shows
    //! (match info column, minimap). It moves left at once and right only past PLACE_HYSTERESIS, so a counter that
    //! gains and loses a digit does not shake the rows. With nothing in that corner the panel stays where it is
    protected void Place( BattleRoyaleHud hud, BattleRoyaleMiniMap minimap )
    {
        float corner_left = -1;
        if ( hud )
            corner_left = hud.GetMatchInfoLeft();
        if ( minimap )
        {
            float map_left = minimap.GetScreenLeft();
            if ( map_left >= 0 && ( corner_left < 0 || map_left < corner_left ) )
                corner_left = map_left;
        }
        if ( corner_left < 0 )
            return;

        float px_per_unit = PxPerUnit();
        if ( px_per_unit <= 0 )
            return;

        float root_x;
        float root_y;
        float root_w;
        float root_h;
        m_Root.GetScreenPos( root_x, root_y );
        m_Root.GetScreenSize( root_w, root_h );

        // KillFeedPanel is right aligned in the HUD frame: its x is the distance between both right edges
        float offset = ( root_x + root_w - corner_left ) / px_per_unit + PANEL_GAP;
        // A new screen height places the panel again: the same offset in units is another one in pixels
        if ( m_PanelOffset >= 0 && px_per_unit == m_PanelPxPerUnit )
        {
            if ( offset < m_PanelOffset && m_PanelOffset - offset < PLACE_HYSTERESIS )
                return;
            if ( Math.AbsFloat( offset - m_PanelOffset ) < 0.5 )
                return;
        }

        m_PanelOffset = offset;
        m_PanelPxPerUnit = px_per_unit;
        // Placed in layout units, set in screen pixels
        m_Panel.SetPos( offset * px_per_unit, PANEL_TOP * px_per_unit );
    }

    //! Widest a row can be, in layout units, to stay CENTRE_GAP right of centre_right (screen pixels, the right
    //! edge of the centred phase lines); -1 for no limit
    protected float RowRoom( float centre_right )
    {
        if ( centre_right < 0 )
            return -1;

        float px_per_unit = PxPerUnit();
        if ( px_per_unit <= 0 )
            return -1;

        float panel_x;
        float panel_y;
        float panel_w;
        float panel_h;
        m_Panel.GetScreenPos( panel_x, panel_y );
        m_Panel.GetScreenSize( panel_w, panel_h );

        // The floor never goes past the room there is: FillRow cuts a row down to it instead
        float room = ( panel_x + panel_w - centre_right ) / px_per_unit - CENTRE_GAP;
        return Math.Max( room, 0 );
    }

    //! Screen pixels per exact layout unit: the units are 1/1080 of the screen height. The layout is scaled with it,
    //! but SetPos and SetSize from a script take screen pixels: every place and size the script sets goes through it
    protected float PxPerUnit()
    {
        int screen_w;
        int screen_h;
        GetScreenSize( screen_w, screen_h );
        return screen_h / 1080.0;
    }

    //! max_width: widest a row can be (layout units), -1 for no limit
    protected void Refresh( BattleRoyaleRPC br_rpc, int now, float max_width )
    {
        m_LastSerial = br_rpc.kill_feed_serial;
        m_NextExpiry = -1;

        int kill_type;
        int flags;
        string killer;
        string victim;
        string item;
        int distance;
        int killer_id;
        int victim_id;
        int time;
        int expiry;
        int row = 0;

        // Newest event first; events arrive in time order, so the first expired one ends the list
        for ( int serial = br_rpc.kill_feed_serial - 1; serial >= 0 && row < ROW_COUNT; serial-- )
        {
            if ( !br_rpc.GetKillFeedEvent( serial, kill_type, flags, killer, victim, item, distance, killer_id, victim_id, time ) )
                break;

            expiry = time + ROW_LIFETIME_MS;
            if ( expiry <= now )
                break;

            FillRow( row, kill_type, flags, killer, victim, item, distance, IsLocalPlayer( killer_id ) || IsLocalPlayer( victim_id ), max_width );
            m_RowTime[row] = time;
            m_RowAlpha[row] = -1;

            // The last row filled is the oldest one, so it expires first
            m_NextExpiry = expiry;
            row++;
        }

        m_ShownRows = row;

        Widget unused_row;
        for ( ; row < ROW_COUNT; row++ )
        {
            unused_row = m_Rows[row];
            unused_row.Show( false );
        }
    }

    //! Applies to every shown row the fade and the slide of its age; touches a widget only when the fade changed
    protected void Animate( int now )
    {
        float age;
        float alpha;
        float slide;
        float back_alpha;
        Widget row_widget;
        Widget back;
        Widget accent;
        ImageWidget icon;
        TextWidget text;
        TextWidget detail;

        float px_per_unit = PxPerUnit();
        if ( px_per_unit <= 0 )
            px_per_unit = 1;

        for ( int row = 0; row < m_ShownRows; row++ )
        {
            age = now - m_RowTime[row];
            alpha = 1;
            slide = 0;

            if ( age < FADE_IN_MS )
            {
                alpha = age / FADE_IN_MS;
                slide = SLIDE_IN_PX * ( alpha - 1 );
            }
            else if ( age > ROW_LIFETIME_MS - FADE_OUT_MS )
            {
                alpha = ( ROW_LIFETIME_MS - age ) / FADE_OUT_MS;
            }

            alpha = Math.Clamp( alpha, 0, 1 );
            if ( alpha == m_RowAlpha[row] )
                continue;

            m_RowAlpha[row] = alpha;

            back_alpha = BACK_ALPHA;
            if ( m_RowOwn[row] )
                back_alpha = BACK_ALPHA_OWN;

            row_widget = m_Rows[row];
            back = m_Backs[row];
            accent = m_Accents[row];
            icon = m_Icons[row];
            text = m_Texts[row];
            detail = m_Details[row];

            back.SetAlpha( back_alpha * alpha );
            accent.SetAlpha( ACCENT_ALPHA * alpha );
            icon.SetAlpha( alpha );
            text.SetAlpha( alpha );
            detail.SetAlpha( alpha );
            // Right aligned row: x is measured from the right edge of the panel, a negative x is to the right.
            // The slide is in screen pixels already, the pitch in layout units
            row_widget.SetPos( slide, row * ROW_PITCH * px_per_unit );
        }
    }

    //! own: the event names the local player as killer or victim. max_width: widest the row can be (layout units),
    //! -1 for no limit; a wider row drops the weapon and the distance, then shortens the names, then cuts the sentence
    //! as measured, and with no room for any text keeps the icon alone or hides the row: it is never wider than max_width
    protected void FillRow( int row, int kill_type, int flags, string killer, string victim, string item, int distance, bool own, float max_width )
    {
        m_RowOwn[row] = own;

        Widget row_widget = m_Rows[row];
        Widget accent = m_Accents[row];
        ImageWidget icon = m_Icons[row];

        // Gold bar on the rows that name the local player
        if ( own )
            accent.SetColor( ARGB( 255, 255, 196, 0 ) );
        else
            accent.SetColor( ARGB( 255, 255, 255, 255 ) );

        if ( m_RowIcon[row] != kill_type )
        {
            icon.LoadImageFile( 0, GetIcon( kill_type ) );
            m_RowIcon[row] = kill_type;
        }

        string sentence = BuildSentence( kill_type, ClipName( killer, NAME_MAX_CHARS ), ClipName( victim, NAME_MAX_CHARS ) );
        string detail = BuildDetail( kill_type, flags, item, distance );

        // Shown before measuring: the size of a text is the size it is drawn with
        row_widget.Show( true );

        float width = LayoutRow( row, sentence, detail );
        if ( max_width < 0 || width <= max_width )
            return;

        if ( detail != "" )
        {
            width = LayoutRow( row, sentence, "" );
            if ( width <= max_width )
                return;
        }

        // Names cut to the most characters that fit, between NAME_SHORT_CHARS and NAME_MAX_CHARS - 1, so the row
        // uses all the room it has (binary search, a few measures on a refresh only)
        int fit = 0;
        int low = NAME_SHORT_CHARS;
        int high = NAME_MAX_CHARS - 1;
        int mid;
        while ( low <= high )
        {
            mid = ( low + high ) / 2;
            sentence = BuildSentence( kill_type, ClipName( killer, mid ), ClipName( victim, mid ) );
            if ( LayoutRow( row, sentence, "" ) <= max_width )
            {
                fit = mid;
                low = mid + 1;
            }
            else
            {
                high = mid - 1;
            }
        }

        if ( fit > 0 )
        {
            // The search can end on a failed measure: the text that fits is written again
            sentence = BuildSentence( kill_type, ClipName( killer, fit ), ClipName( victim, fit ) );
            LayoutRow( row, sentence, "" );
            return;
        }

        sentence = BuildSentence( kill_type, ClipName( killer, NAME_SHORT_CHARS ), ClipName( victim, NAME_SHORT_CHARS ) );
        CutRow( row, sentence, max_width );
    }

    //! Last cut of a row still wider than max_width: the longest start of the sentence, with an ellipsis, that fits
    //! as measured (binary search, a few measures on a refresh only); else the icon alone; else the row hidden
    protected void CutRow( int row, string sentence, float max_width )
    {
        int low = 0;                            // characters known to fit (0: none checked yet)
        int high = sentence.LengthUtf8() - 1;   // most characters to try: the whole sentence did not fit
        int mid;
        while ( low < high )
        {
            mid = ( low + high + 1 ) / 2;
            if ( LayoutRow( row, sentence.SubstringUtf8( 0, mid ) + ELLIPSIS, "" ) <= max_width )
                low = mid;
            else
                high = mid - 1;
        }

        // The search ends on a failed measure as often as not: the text it found is written again
        if ( low > 0 && LayoutRow( row, sentence.SubstringUtf8( 0, low ) + ELLIPSIS, "" ) <= max_width )
            return;

        // No text fits (or none could be measured): the icon alone, or nothing
        Widget row_widget = m_Rows[row];
        Widget back = m_Backs[row];
        TextWidget text_widget = m_Texts[row];
        TextWidget detail_widget = m_Details[row];
        text_widget.SetText( "" );
        detail_widget.Show( false );

        if ( max_width < TEXT_X )
        {
            row_widget.Show( false );
            return;
        }

        float px_per_unit = PxPerUnit();
        if ( px_per_unit <= 0 )
            px_per_unit = 1;
        row_widget.SetSize( TEXT_X * px_per_unit, ROW_HEIGHT * px_per_unit );
        back.SetSize( TEXT_X * px_per_unit, ROW_HEIGHT * px_per_unit );
    }

    //! Writes the sentence and the detail (empty for none) in the row, sizes the row to them and returns its width
    protected float LayoutRow( int row, string sentence, string detail )
    {
        Widget row_widget = m_Rows[row];
        Widget back = m_Backs[row];
        TextWidget text_widget = m_Texts[row];
        TextWidget detail_widget = m_Details[row];

        text_widget.SetText( sentence );

        int text_width;
        int text_height;
        text_widget.GetTextSize( text_width, text_height );

        // GetTextSize measures in screen pixels and SetPos/SetSize take them; the row is laid out in layout units
        float px_per_unit = PxPerUnit();
        if ( px_per_unit <= 0 )
            px_per_unit = 1;
        float units_per_px = 1.0 / px_per_unit;

        float width;
        if ( text_width <= 0 )
        {
            // Not measured: one text and a row of fixed width
            if ( detail != "" )
                text_widget.SetText( sentence + "  " + detail );
            detail_widget.Show( false );
            width = ROW_WIDTH_UNMEASURED;
        }
        else
        {
            float end = TEXT_X + text_width * units_per_px;
            if ( detail != "" )
            {
                int detail_width;
                int detail_height;
                detail_widget.Show( true );
                detail_widget.SetText( detail );
                detail_widget.SetPos( ( end + DETAIL_GAP ) * px_per_unit, DETAIL_Y * px_per_unit );
                detail_widget.GetTextSize( detail_width, detail_height );
                end = end + DETAIL_GAP + detail_width * units_per_px;
            }
            else
            {
                detail_widget.Show( false );
            }

            width = end + ROW_PADDING;
        }

        // The row hugs its content; it is right aligned, so its width moves its left edge and the content with it
        row_widget.SetSize( width * px_per_unit, ROW_HEIGHT * px_per_unit );
        back.SetSize( width * px_per_unit, ROW_HEIGHT * px_per_unit );
        return width;
    }

    //! True when player_id is the session id of the local player. Names are not compared: two players
    //! can have the same one. The id is read once, while the player has a character, and kept
    protected bool IsLocalPlayer( int player_id )
    {
        if ( player_id < 0 )
            return false;

        if ( m_LocalPlayerId < 0 )
        {
            Man player = GetGame().GetPlayer();
            if ( player && player.GetIdentity() )
                m_LocalPlayerId = player.GetIdentity().GetPlayerId();
        }

        return player_id == m_LocalPlayerId;
    }

    protected string ClipName( string name, int max_chars )
    {
        if ( name.LengthUtf8() <= max_chars )
            return name;

        return name.SubstringUtf8( 0, max_chars - 3 ) + "...";
    }

    //! Sentence for the cause: the one with killer and victim when the event names a killer, else the one with the victim alone
    protected string BuildSentence( int kill_type, string killer, string victim )
    {
        if ( killer != "" )
            return string.Format( Widget.TranslateString( GetKillKey( kill_type ) ), killer, victim );

        return string.Format( Widget.TranslateString( GetDeathKey( kill_type ) ), victim );
    }

    //! Stringtable key of "%1 (killer) ... %2 (victim)"
    protected string GetKillKey( int kill_type )
    {
        switch ( kill_type )
        {
            case EBRKillFeedType.EXPLOSIVE:
                return "#STR_BR_KILLFEED_KILL_EXPLOSIVE";
            case EBRKillFeedType.BLEED_OUT:
                return "#STR_BR_KILLFEED_KILL_BLEED_OUT";
        }

        return "#STR_BR_KILLFEED_KILL";
    }

    //! Stringtable key of "%1 (victim) ..." for a death that names no killer
    protected string GetDeathKey( int kill_type )
    {
        switch ( kill_type )
        {
            case EBRKillFeedType.EXPLOSIVE:
                return "#STR_BR_KILLFEED_EXPLOSIVE";
            case EBRKillFeedType.VEHICLE:
                return "#STR_BR_KILLFEED_VEHICLE";
            case EBRKillFeedType.VEHICLE_CRASH:
                return "#STR_BR_KILLFEED_VEHICLE_CRASH";
            case EBRKillFeedType.FALL:
                return "#STR_BR_KILLFEED_FALL";
            case EBRKillFeedType.ZONE:
                return "#STR_BR_KILLFEED_ZONE";
            case EBRKillFeedType.BLEED_OUT:
                return "#STR_BR_KILLFEED_BLEED_OUT";
            case EBRKillFeedType.INFECTED:
                return "#STR_BR_KILLFEED_INFECTED";
            case EBRKillFeedType.ANIMAL:
                return "#STR_BR_KILLFEED_ANIMAL";
            case EBRKillFeedType.SUICIDE:
                return "#STR_BR_KILLFEED_SUICIDE";
            case EBRKillFeedType.DROWNED:
                return "#STR_BR_KILLFEED_DROWNED";
            case EBRKillFeedType.FIRE:
                return "#STR_BR_KILLFEED_FIRE";
            case EBRKillFeedType.BARBED_WIRE:
                return "#STR_BR_KILLFEED_BARBED_WIRE";
        }

        return "#STR_BR_KILLFEED_DIED";
    }

    //! Icon of the cause, from the vanilla image sets
    protected string GetIcon( int kill_type )
    {
        switch ( kill_type )
        {
            case EBRKillFeedType.FIREARM:
                return "set:dayz_inventory image:pistol";
            case EBRKillFeedType.MELEE:
                return "set:dayz_inventory image:knife";
            case EBRKillFeedType.FISTS:
                return "set:dayz_inventory image:hands";
            case EBRKillFeedType.EXPLOSIVE:
                return "set:dayz_inventory image:grenade";
            case EBRKillFeedType.VEHICLE:
                return "set:dayz_inventory image:wheel";
            case EBRKillFeedType.VEHICLE_CRASH:
                return "set:dayz_inventory image:wheel";
            case EBRKillFeedType.FALL:
                return "set:dayz_gui image:iconBone";
            case EBRKillFeedType.ZONE:
                return "set:dayz_gui image:circle";
            case EBRKillFeedType.BLEED_OUT:
                return "set:dayz_gui image:iconBleeding";
            case EBRKillFeedType.INFECTED:
                return "set:dayz_gui image:iconBacteria";
            case EBRKillFeedType.ANIMAL:
                return "set:dayz_inventory image:animal_pelt";
            case EBRKillFeedType.DROWNED:
                return "set:dayz_gui image:liquid";
            case EBRKillFeedType.FIRE:
                return "set:dayz_inventory image:firewood";
            case EBRKillFeedType.BARBED_WIRE:
                return "set:dayz_inventory image:barbedwire";
        }

        return "set:dayz_gui image:iconSkull";
    }

    //! "<item>, <distance> m, headshot", with the parts the event has
    protected string BuildDetail( int kill_type, int flags, string item, int distance )
    {
        string detail = item;
        if ( kill_type == EBRKillFeedType.FISTS )
            detail = Widget.TranslateString( "#STR_BR_KILLFEED_FISTS" );

        if ( distance > 0 )
        {
            if ( detail != "" )
                detail = detail + ", ";
            detail = detail + distance.ToString() + " m";
        }

        if ( ( flags & BRKillFeedFlags.HEADSHOT ) != 0 )
        {
            if ( detail != "" )
                detail = detail + ", ";
            detail = detail + Widget.TranslateString( "#STR_BR_KILLFEED_HEADSHOT" );
        }

        return detail;
    }
}
#endif

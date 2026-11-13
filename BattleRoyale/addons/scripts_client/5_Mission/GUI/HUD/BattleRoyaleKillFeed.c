#ifndef SERVER
//! Kill feed: a fixed pool of rows filled from the events stored by BattleRoyaleRPC, newest on top.
//! A row is a dark panel with an icon for the cause, a sentence naming killer and victim and, after it,
//! the weapon and the distance. Rows slide and fade in, and fade out before they expire
class BattleRoyaleKillFeed
{
    static const int ROW_COUNT = 6;             //!< rows in killfeed.layout; at most BattleRoyaleRPC.KILL_FEED_SIZE
    static const int ROW_LIFETIME_MS = 6000;
    static const int FADE_IN_MS = 150;
    static const int FADE_OUT_MS = 700;
    static const float SLIDE_IN_PX = 12;        //!< a new row starts this far to the left

    // Geometry of killfeed.layout, in pixels
    static const float ROW_PITCH = 34;
    static const float ROW_HEIGHT = 30;
    static const float TEXT_X = 42;
    static const float DETAIL_Y = 5;
    static const float DETAIL_GAP = 12;         //!< between the sentence and the detail
    static const float ROW_PADDING = 10;        //!< after the last text
    static const float ROW_WIDTH_UNMEASURED = 620;  //!< panel width when the text could not be measured

    static const int NAME_MAX_CHARS = 24;       //!< longer player names are cut

    static const float BACK_ALPHA = 0.55;
    static const float BACK_ALPHA_OWN = 0.8;    //!< row that names the local player
    static const float ACCENT_ALPHA = 0.5;

    protected Widget m_Root;
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

    protected string m_LocalName;   //!< name of the local player, kept for when it has no character

    void BattleRoyaleKillFeed( Widget parent )
    {
        m_LastSerial = -1;
        m_NextExpiry = -1;

        if ( !GetGame() || !GetGame().GetWorkspace() )
            return;

        m_Root = GetGame().GetWorkspace().CreateWidgets( "Vigrid-BattleRoyale/GUI/layouts/hud/killfeed.layout", parent );
        if ( !m_Root )
        {
            Error("[BattleRoyaleKillFeed] killfeed.layout not loaded!");
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

    //! Called every frame; rebuilds the rows only when an event arrived or a row expired, and moves the
    //! fade of the rows on screen
    void Update()
    {
        if ( !m_Root )
            return;

        BattleRoyaleRPC br_rpc = BattleRoyaleRPC.GetInstance();
        int now = GetGame().GetTime();

        if ( br_rpc.kill_feed_serial != m_LastSerial || ( m_NextExpiry >= 0 && now >= m_NextExpiry ) )
            Refresh( br_rpc, now );

        Animate( now );
    }

    protected void Refresh( BattleRoyaleRPC br_rpc, int now )
    {
        m_LastSerial = br_rpc.kill_feed_serial;
        m_NextExpiry = -1;

        int kill_type;
        int flags;
        string killer;
        string victim;
        string item;
        int distance;
        int time;
        int expiry;
        int row = 0;

        // Newest event first; events arrive in time order, so the first expired one ends the list
        for ( int serial = br_rpc.kill_feed_serial - 1; serial >= 0 && row < ROW_COUNT; serial-- )
        {
            if ( !br_rpc.GetKillFeedEvent( serial, kill_type, flags, killer, victim, item, distance, time ) )
                break;

            expiry = time + ROW_LIFETIME_MS;
            if ( expiry <= now )
                break;

            FillRow( row, kill_type, flags, killer, victim, item, distance );
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
            row_widget.SetPos( slide, row * ROW_PITCH );
        }
    }

    protected void FillRow( int row, int kill_type, int flags, string killer, string victim, string item, int distance )
    {
        string local_name = GetLocalName();
        bool own = local_name != "" && ( killer == local_name || victim == local_name );
        m_RowOwn[row] = own;

        Widget row_widget = m_Rows[row];
        Widget back = m_Backs[row];
        Widget accent = m_Accents[row];
        ImageWidget icon = m_Icons[row];
        TextWidget text_widget = m_Texts[row];
        TextWidget detail_widget = m_Details[row];

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

        string text = BuildSentence( kill_type, ClipName( killer ), ClipName( victim ) );
        string detail = BuildDetail( kill_type, flags, item, distance );

        // Shown before measuring: the size of a text is the size it is drawn with
        row_widget.Show( true );
        text_widget.SetText( text );

        int text_width;
        int text_height;
        text_widget.GetTextSize( text_width, text_height );

        if ( text_width <= 0 )
        {
            // Not measured: one text and a panel of fixed width
            if ( detail != "" )
                text_widget.SetText( text + "  " + detail );
            detail_widget.Show( false );
            back.SetSize( ROW_WIDTH_UNMEASURED, ROW_HEIGHT );
            return;
        }

        float end = TEXT_X + text_width;
        if ( detail != "" )
        {
            int detail_width;
            int detail_height;
            detail_widget.Show( true );
            detail_widget.SetText( detail );
            detail_widget.SetPos( end + DETAIL_GAP, DETAIL_Y );
            detail_widget.GetTextSize( detail_width, detail_height );
            end = end + DETAIL_GAP + detail_width;
        }
        else
        {
            detail_widget.Show( false );
        }

        // The panel hugs its content
        back.SetSize( end + ROW_PADDING, ROW_HEIGHT );
    }

    //! Name of the local player; empty until it is known
    protected string GetLocalName()
    {
        PlayerBase player = PlayerBase.Cast( GetGame().GetPlayer() );
        if ( player )
        {
            string name = player.BR_GetPlayerName();
            if ( name != "" )
                m_LocalName = name;
        }

        return m_LocalName;
    }

    protected string ClipName( string name )
    {
        if ( name.LengthUtf8() <= NAME_MAX_CHARS )
            return name;

        return name.SubstringUtf8( 0, NAME_MAX_CHARS - 3 ) + "...";
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

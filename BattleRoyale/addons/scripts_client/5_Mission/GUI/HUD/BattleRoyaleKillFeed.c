#ifndef SERVER
//! Kill feed: a fixed pool of rows filled from the events stored by BattleRoyaleRPC, newest on top
class BattleRoyaleKillFeed
{
    static const int ROW_COUNT = 5;
    static const int ROW_LIFETIME_MS = 6000;

    protected Widget m_Root;
    protected TextWidget m_Rows[ROW_COUNT];

    protected int m_LastSerial;     //!< kill_feed_serial the rows were built from
    protected int m_NextExpiry;     //!< GetGame().GetTime() at which the oldest shown row expires, -1 if no row is shown

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

        m_Rows[0] = TextWidget.Cast( m_Root.FindAnyWidget( "KillFeedRow0" ) );
        m_Rows[1] = TextWidget.Cast( m_Root.FindAnyWidget( "KillFeedRow1" ) );
        m_Rows[2] = TextWidget.Cast( m_Root.FindAnyWidget( "KillFeedRow2" ) );
        m_Rows[3] = TextWidget.Cast( m_Root.FindAnyWidget( "KillFeedRow3" ) );
        m_Rows[4] = TextWidget.Cast( m_Root.FindAnyWidget( "KillFeedRow4" ) );

        for ( int i = 0; i < ROW_COUNT; i++ )
        {
            if ( m_Rows[i] )
                m_Rows[i].Show( false );
        }
    }

    void ~BattleRoyaleKillFeed()
    {
        if ( m_Root )
            m_Root.Unlink();
    }

    //! Called every frame; rebuilds the rows only when an event arrived or a row expired
    void Update()
    {
        if ( !m_Root )
            return;

        BattleRoyaleRPC br_rpc = BattleRoyaleRPC.GetInstance();
        int now = GetGame().GetTime();

        if ( br_rpc.kill_feed_serial == m_LastSerial && ( m_NextExpiry < 0 || now < m_NextExpiry ) )
            return;

        Refresh( br_rpc, now );
    }

    protected void Refresh( BattleRoyaleRPC br_rpc, int now )
    {
        m_LastSerial = br_rpc.kill_feed_serial;
        m_NextExpiry = -1;

        string killer;
        string victim;
        string weapon;
        float distance;
        int time;
        int expiry;
        int row = 0;

        // Newest event first; events arrive in time order, so the first expired one ends the list
        for ( int serial = br_rpc.kill_feed_serial - 1; serial >= 0 && row < ROW_COUNT; serial-- )
        {
            if ( !br_rpc.GetKillFeedEvent( serial, killer, victim, weapon, distance, time ) )
                break;

            expiry = time + ROW_LIFETIME_MS;
            if ( expiry <= now )
                break;

            if ( m_Rows[row] )
            {
                m_Rows[row].SetText( BuildRowText( killer, victim, weapon, distance ) );
                m_Rows[row].Show( true );
            }

            // The last row filled is the oldest one, so it expires first
            m_NextExpiry = expiry;
            row++;
        }

        for ( ; row < ROW_COUNT; row++ )
        {
            if ( m_Rows[row] )
                m_Rows[row].Show( false );
        }
    }

    //! "<killer> killed <victim> [<weapon>, <distance> m]"; without a killer (zone, fall, environment) "<victim> died"
    protected string BuildRowText( string killer, string victim, string weapon, float distance )
    {
        if ( killer == "" )
            return string.Format( Widget.TranslateString( "#STR_BR_KILLFEED_DIED" ), victim );

        // The server may send a localization key as weapon name
        if ( weapon.IndexOf( "#" ) == 0 )
            weapon = Widget.TranslateString( weapon );

        string text = string.Format( Widget.TranslateString( "#STR_BR_KILLFEED_KILL" ), killer, victim );

        int meters = Math.Round( distance );
        if ( weapon != "" && meters > 0 )
            text = text + " [" + weapon + ", " + meters.ToString() + " m]";
        else if ( weapon != "" )
            text = text + " [" + weapon + "]";
        else if ( meters > 0 )
            text = text + " [" + meters.ToString() + " m]";

        return text;
    }
}
#endif

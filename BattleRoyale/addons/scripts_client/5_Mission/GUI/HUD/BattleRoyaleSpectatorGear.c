#ifndef SERVER
//! Read-only equipment panel for spectators, toggled with TAB (UAGear) while BattleRoyaleRPC.s_Spectating is on.
//! Shows the watched player's name, item in hands (with weapon accessories), occupied equipment slots (with
//! shoulder weapon accessories), and any cargo items replicated on the client with their quantities.
//! Takes no mouse or keyboard focus so the spectator can keep switching players; closes on ESC, leaving
//! spectating, returning to the waiting room or lobby, or when the win screen opens.
class BattleRoyaleSpectatorGear
{
	static const int ROW_LINES = 20;            //!< rows of the left column: SpectatorGearRow0 to 19
	static const int SIDE_ROWS = 40;            //!< rows of both side columns; the right one is rows 20 to 39
	static const int CENTRE_LINES = 6;          //!< rows of the centred column: SpectatorGearRow40 to 45
	static const int ROW_COUNT = 46;            //!< SIDE_ROWS + CENTRE_LINES
	static const int BACK_COUNT = 21;           //!< backdrops: 0 header alone, 1..20 header and 1..20 lines of rows

	static const int REFRESH_INTERVAL_MS = 500; //!< at most two refreshes per second while open
	static const int FADE_IN_MS = 150;          //!< same fade-in as the leaderboard panel

	static const int KIND_SLOT = 0;
	static const int KIND_HANDS = 1;
	static const int KIND_EMPTY = 2;
	static const int KIND_SUB = 3;
	static const int KIND_CARGO = 4;
	static const int KIND_MORE = 5;

	static const string ELLIPSIS = "...";
	static const string INPUT_NAME = "UAGear";

	protected Widget m_Root;
	protected Widget m_Panel;
	protected Widget m_Table;
	protected Widget m_Backs[BACK_COUNT];
	protected TextWidget m_TargetText;
	protected TextWidget m_HintText;

	protected Widget m_Rows[ROW_COUNT];
	protected Widget m_RowBack[ROW_COUNT];
	protected Widget m_RowBar[ROW_COUNT];
	protected Widget m_RowDash[ROW_COUNT];
	protected TextWidget m_RowLabel[ROW_COUNT];
	protected TextWidget m_RowName[ROW_COUNT];
	protected TextWidget m_RowNameWide[ROW_COUNT];
	protected TextWidget m_RowSub[ROW_COUNT];
	protected TextWidget m_RowSubWide[ROW_COUNT];
	protected TextWidget m_RowQty[ROW_COUNT];

	protected ref array<int> m_OrderedSlots;
	protected ref array<EntityAI> m_VisitedAttachments;
	protected ref InventoryLocation m_TempLoc;

	protected ref array<int> m_SnapKinds;
	protected ref array<string> m_SnapLabels;
	protected ref array<string> m_SnapNames;
	protected ref array<string> m_SnapQtys;

	protected ref array<string> m_CargoNames;
	protected ref array<string> m_CargoQtys;
	protected ref array<int> m_CargoCounts;

	protected bool m_DrawnValid;
	protected bool m_DrawnHasTarget;
	protected string m_DrawnTargetName;
	protected string m_DrawnHint;
	protected ref array<int> m_DrawnKinds;
	protected ref array<string> m_DrawnLabels;
	protected ref array<string> m_DrawnNames;
	protected ref array<string> m_DrawnQtys;

	protected bool m_Open;
	protected bool m_Dirty;
	protected bool m_ForceDraw;
	protected bool m_Fading;
	protected int m_FadeStart;
	protected int m_LastRefresh;
	protected int m_ShownScreenW;
	protected int m_ShownScreenH;
	protected int m_ShownTargetLow;
	protected int m_ShownTargetHigh;
	protected string m_ShownTargetName;

	void BattleRoyaleSpectatorGear( Widget parent )
	{
		m_FadeStart = -1;
		m_LastRefresh = -1;
		m_ShownScreenW = -1;
		m_ShownScreenH = -1;
		m_ShownTargetLow = 0;
		m_ShownTargetHigh = 0;
		m_ShownTargetName = "";

		m_OrderedSlots = new array<int>;
		m_VisitedAttachments = new array<EntityAI>;
		m_TempLoc = new InventoryLocation;

		m_SnapKinds = new array<int>;
		m_SnapLabels = new array<string>;
		m_SnapNames = new array<string>;
		m_SnapQtys = new array<string>;

		m_CargoNames = new array<string>;
		m_CargoQtys = new array<string>;
		m_CargoCounts = new array<int>;

		m_DrawnKinds = new array<int>;
		m_DrawnLabels = new array<string>;
		m_DrawnNames = new array<string>;
		m_DrawnQtys = new array<string>;

		InitOrderedSlots();

		if ( !GetGame() || !GetGame().GetWorkspace() )
			return;

		m_Root = GetGame().GetWorkspace().CreateWidgets( "Vigrid-BattleRoyale/Scripts/Client/layouts/br_spectator_gear.layout", parent );
		if ( !m_Root )
		{
			Error( "[BattleRoyaleSpectatorGear] br_spectator_gear.layout not loaded!" );
			return;
		}

		// A layout missing any expected widget disables the panel cleanly instead of failing on refresh
		if ( !FindWidgets() )
		{
			Error( "[BattleRoyaleSpectatorGear] br_spectator_gear.layout is not complete" );
			m_Root.Unlink();
			m_Root = null;
			return;
		}

		m_Root.Show( false );
	}

	void ~BattleRoyaleSpectatorGear()
	{
		if ( m_VisitedAttachments )
			m_VisitedAttachments.Clear();
		if ( m_Root )
			m_Root.Unlink();
	}

	//! Standard equipment slots in top-to-bottom reading order; any extra slot on the player is appended after them
	protected void InitOrderedSlots()
	{
		m_OrderedSlots.Insert( InventorySlots.SHOULDER );
		m_OrderedSlots.Insert( InventorySlots.MELEE );
		m_OrderedSlots.Insert( InventorySlots.HEADGEAR );
		m_OrderedSlots.Insert( InventorySlots.MASK );
		m_OrderedSlots.Insert( InventorySlots.EYEWEAR );
		m_OrderedSlots.Insert( InventorySlots.BODY );
		m_OrderedSlots.Insert( InventorySlots.VEST );
		m_OrderedSlots.Insert( InventorySlots.GLOVES );
		m_OrderedSlots.Insert( InventorySlots.ARMBAND );
		m_OrderedSlots.Insert( InventorySlots.HIPS );
		m_OrderedSlots.Insert( InventorySlots.LEGS );
		m_OrderedSlots.Insert( InventorySlots.FEET );
		m_OrderedSlots.Insert( InventorySlots.BACK );
	}

	protected bool FindWidgets()
	{
		m_Panel = m_Root.FindAnyWidget( "SpectatorGearPanel" );
		m_Table = m_Root.FindAnyWidget( "SpectatorGearTable" );
		m_TargetText = TextWidget.Cast( m_Root.FindAnyWidget( "SpectatorGearTarget" ) );
		m_HintText = TextWidget.Cast( m_Root.FindAnyWidget( "SpectatorGearHint" ) );
		if ( !m_Panel || !m_Table || !m_TargetText || !m_HintText )
			return false;

		int i;
		string name;
		for ( i = 0; i < BACK_COUNT; i++ )
		{
			name = "SpectatorGearBack" + i.ToString();
			m_Backs[i] = m_Root.FindAnyWidget( name );
			if ( !m_Backs[i] )
				return false;
		}

		for ( i = 0; i < ROW_COUNT; i++ )
		{
			name = "SpectatorGearRow" + i.ToString();
			m_Rows[i] = m_Root.FindAnyWidget( name );
			m_RowBack[i] = m_Root.FindAnyWidget( name + "Back" );
			m_RowBar[i] = m_Root.FindAnyWidget( name + "Bar" );
			m_RowDash[i] = m_Root.FindAnyWidget( name + "Dash" );
			m_RowLabel[i] = TextWidget.Cast( m_Root.FindAnyWidget( name + "Label" ) );
			m_RowName[i] = TextWidget.Cast( m_Root.FindAnyWidget( name + "Name" ) );
			m_RowNameWide[i] = TextWidget.Cast( m_Root.FindAnyWidget( name + "NameWide" ) );
			m_RowSub[i] = TextWidget.Cast( m_Root.FindAnyWidget( name + "Sub" ) );
			m_RowSubWide[i] = TextWidget.Cast( m_Root.FindAnyWidget( name + "SubWide" ) );
			m_RowQty[i] = TextWidget.Cast( m_Root.FindAnyWidget( name + "Qty" ) );
			if ( !m_Rows[i] || !m_RowBack[i] || !m_RowBar[i] || !m_RowDash[i] || !m_RowLabel[i] )
				return false;
			if ( !m_RowName[i] || !m_RowNameWide[i] || !m_RowSub[i] || !m_RowSubWide[i] || !m_RowQty[i] )
				return false;
		}

		return true;
	}

	bool IsOpen()
	{
		return m_Open;
	}

	void Toggle()
	{
		if ( !m_Root || !BattleRoyaleRPC.s_Spectating )
			return;

		if ( m_Open )
		{
			Close();
			return;
		}

		m_Open = true;
		m_Dirty = true;
		m_ForceDraw = true;
		m_LastRefresh = -1;

		m_Fading = true;
		m_FadeStart = -1;
		m_Panel.SetAlpha( 0 );
	}

	void Close()
	{
		if ( !m_Root || !m_Open )
			return;

		m_Open = false;
		m_Dirty = false;
		m_VisitedAttachments.Clear();
		m_Root.Show( false );
	}

	//! Top edge of the panel in screen pixels while it is on screen, -1 when hidden: keeps the kill feed above it
	float GetScreenTop()
	{
		if ( !m_Root || !m_Open || !m_Root.IsVisible() )
			return -1;

		float x;
		float y;
		m_Panel.GetScreenPos( x, y );
		return y;
	}

	//! Called every frame; closes for good when spectating ends or a menu opens (ESC), refreshes at most 2 Hz or on target switch
	void Update( bool menu_open )
	{
		if ( !m_Open || !m_Root )
			return;

		if ( menu_open || !BattleRoyaleRPC.s_Spectating )
		{
			Close();
			return;
		}

		if ( !m_Root.IsVisible() )
			m_Root.Show( true );

		int now = GetGame().GetTime();
		int screen_w;
		int screen_h;
		GetScreenSize( screen_w, screen_h );

		if ( screen_w != m_ShownScreenW || screen_h != m_ShownScreenH )
		{
			m_Dirty = true;
			m_ForceDraw = true;
		}

		if ( BattleRoyaleRPC.s_SpectateTargetLow != m_ShownTargetLow || BattleRoyaleRPC.s_SpectateTargetHigh != m_ShownTargetHigh || BattleRoyaleRPC.s_SpectateTargetName != m_ShownTargetName )
			m_Dirty = true;

		if ( m_LastRefresh < 0 || now - m_LastRefresh >= REFRESH_INTERVAL_MS )
			m_Dirty = true;

		if ( m_Dirty )
		{
			m_Dirty = false;
			m_LastRefresh = now;
			m_ShownScreenW = screen_w;
			m_ShownScreenH = screen_h;
			m_ShownTargetLow = BattleRoyaleRPC.s_SpectateTargetLow;
			m_ShownTargetHigh = BattleRoyaleRPC.s_SpectateTargetHigh;
			m_ShownTargetName = BattleRoyaleRPC.s_SpectateTargetName;
			Refresh( m_ForceDraw );
			m_ForceDraw = false;
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
			m_Panel.SetAlpha( alpha );
		}
	}

	protected void Refresh( bool force_draw )
	{
		string target_name = BattleRoyaleRPC.s_SpectateTargetName;
		string hint = GetCloseHint();

		Human target = null;
		if ( BattleRoyaleRPC.s_SpectateTargetLow != 0 || BattleRoyaleRPC.s_SpectateTargetHigh != 0 )
		{
			Object net_obj = GetGame().GetObjectByNetworkId( BattleRoyaleRPC.s_SpectateTargetLow, BattleRoyaleRPC.s_SpectateTargetHigh );
			Class.CastTo( target, net_obj );
		}

		bool has_target = target != null;
		m_SnapKinds.Clear();
		m_SnapLabels.Clear();
		m_SnapNames.Clear();
		m_SnapQtys.Clear();

		if ( has_target )
			CollectTargetGear( target );

		if ( !force_draw && IsSameSnapshot( has_target, target_name, hint ) )
			return;

		SaveDrawnSnapshot( has_target, target_name, hint );
		DrawSnapshot( has_target, target_name, hint );
	}

	protected void CollectTargetGear( Human target )
	{
		string hands_label = Widget.TranslateString( "#STR_BR_SPECGEAR_HANDS" );
		string nothing_text = Widget.TranslateString( "#STR_BR_SPECGEAR_NOTHING" );

		HumanInventory human_inv = target.GetHumanInventory();
		EntityAI in_hands = null;
		if ( human_inv )
			in_hands = human_inv.GetEntityInHands();

		if ( in_hands )
		{
			AddSnapRow( KIND_HANDS, hands_label, GetItemName( in_hands ), GetItemQty( in_hands ) );
			if ( in_hands.IsWeapon() )
				CollectWeaponAccessories( in_hands );
		}
		else
		{
			AddSnapRow( KIND_EMPTY, hands_label, nothing_text, "" );
		}

		m_VisitedAttachments.Clear();
		GameInventory target_inv = target.GetInventory();
		if ( target_inv )
		{
			int ordered_count = m_OrderedSlots.Count();
			int i;
			int slot_id;
			EntityAI att;
			for ( i = 0; i < ordered_count; i++ )
			{
				slot_id = m_OrderedSlots.Get( i );
				if ( !InventorySlots.IsSlotIdValid( slot_id ) || !target.CanDisplayAttachmentSlot( slot_id ) )
					continue;

				att = target_inv.FindAttachment( slot_id );
				if ( !att || target_inv.IsPlaceholderEntity( att ) )
					continue;
				if ( m_VisitedAttachments.Find( att ) != -1 )
					continue;

				m_VisitedAttachments.Insert( att );
				AddSnapRow( KIND_SLOT, GetSlotLabel( slot_id ), GetItemName( att ), GetItemQty( att ) );
				if ( ( slot_id == InventorySlots.SHOULDER || slot_id == InventorySlots.MELEE ) && att.IsWeapon() )
					CollectWeaponAccessories( att );
			}

			int att_count = target_inv.AttachmentCount();
			int j;
			int extra_slot;
			EntityAI extra_att;
			for ( j = 0; j < att_count; j++ )
			{
				extra_att = target_inv.GetAttachmentFromIndex( j );
				if ( !extra_att || target_inv.IsPlaceholderEntity( extra_att ) )
					continue;
				if ( m_VisitedAttachments.Find( extra_att ) != -1 )
					continue;

				extra_slot = GetAttachedSlotId( extra_att );
				if ( extra_slot != InventorySlots.INVALID && !target.CanDisplayAttachmentSlot( extra_slot ) )
					continue;

				m_VisitedAttachments.Insert( extra_att );
				AddSnapRow( KIND_SLOT, GetSlotLabel( extra_slot ), GetItemName( extra_att ), GetItemQty( extra_att ) );
				if ( ( extra_slot == InventorySlots.SHOULDER || extra_slot == InventorySlots.MELEE ) && extra_att.IsWeapon() )
					CollectWeaponAccessories( extra_att );
			}
		}

		if ( m_VisitedAttachments.Count() == 0 )
		{
			string gear_label = Widget.TranslateString( "#STR_BR_SPECGEAR_GEAR" );
			AddSnapRow( KIND_EMPTY, gear_label, nothing_text, "" );
		}

		// Cargo is listed only when the client actually has items replicated in the player's containers
		m_CargoNames.Clear();
		m_CargoQtys.Clear();
		m_CargoCounts.Clear();

		CollectDirectCargo( target );
		if ( in_hands )
			CollectContainerCargo( in_hands );

		int visited_count = m_VisitedAttachments.Count();
		int v;
		EntityAI worn;
		for ( v = 0; v < visited_count; v++ )
		{
			worn = m_VisitedAttachments.Get( v );
			if ( worn && target.CanDisplayAttachmentCargo( worn ) )
				CollectContainerCargo( worn );
		}
		m_VisitedAttachments.Clear();

		int cargo_rows = m_CargoNames.Count();
		if ( cargo_rows > 0 )
		{
			string cargo_label = Widget.TranslateString( "#STR_BR_SPECGEAR_CARGO" );
			int c;
			string c_label;
			string c_qty;
			int c_count;
			for ( c = 0; c < cargo_rows; c++ )
			{
				c_label = "";
				if ( c == 0 )
					c_label = cargo_label;
				c_qty = m_CargoQtys.Get( c );
				c_count = m_CargoCounts.Get( c );
				if ( c_qty == "" && c_count > 1 )
					c_qty = "x" + c_count.ToString();
				AddSnapRow( KIND_CARGO, c_label, m_CargoNames.Get( c ), c_qty );
			}
		}

		// Keep the table within screen bounds; overflow ends with an ellipsis row
		if ( m_SnapKinds.Count() > SIDE_ROWS )
		{
			int trim_idx;
			while ( m_SnapKinds.Count() > SIDE_ROWS - 1 )
			{
				trim_idx = m_SnapKinds.Count() - 1;
				m_SnapKinds.RemoveOrdered( trim_idx );
				m_SnapLabels.RemoveOrdered( trim_idx );
				m_SnapNames.RemoveOrdered( trim_idx );
				m_SnapQtys.RemoveOrdered( trim_idx );
			}
			AddSnapRow( KIND_MORE, "", ELLIPSIS, "" );
		}
	}

	//! Visible attachments of a weapon (optics, suppressor, buttstock, handguard, light, detachable magazine)
	protected void CollectWeaponAccessories( EntityAI weapon )
	{
		if ( !weapon )
			return;

		GameInventory weapon_inv = weapon.GetInventory();
		if ( !weapon_inv )
			return;

		bool show_mag = !weapon.ConfigIsExisting( "DisplayMagazine" ) || weapon.ConfigGetBool( "DisplayMagazine" );
		int count = weapon_inv.AttachmentCount();
		int i;
		int acc_slot;
		EntityAI acc;
		for ( i = 0; i < count; i++ )
		{
			acc = weapon_inv.GetAttachmentFromIndex( i );
			if ( !acc || weapon_inv.IsPlaceholderEntity( acc ) )
				continue;
			if ( acc.IsMagazine() && !show_mag )
				continue;

			acc_slot = GetAttachedSlotId( acc );
			if ( acc_slot != InventorySlots.INVALID && !weapon.CanDisplayAttachmentSlot( acc_slot ) )
				continue;

			AddSnapRow( KIND_SUB, "", GetItemName( acc ), GetItemQty( acc ) );
		}
	}

	//! Reads direct cargo of a container and of its cargo-bearing sub-attachments (such as plate carrier pouches)
	protected void CollectContainerCargo( EntityAI container )
	{
		if ( !container )
			return;

		CollectDirectCargo( container );

		GameInventory inv = container.GetInventory();
		if ( !inv )
			return;

		int sub_count = inv.AttachmentCount();
		int i;
		EntityAI sub;
		for ( i = 0; i < sub_count; i++ )
		{
			sub = inv.GetAttachmentFromIndex( i );
			if ( !sub || inv.IsPlaceholderEntity( sub ) )
				continue;
			if ( !container.CanDisplayAttachmentCargo( sub ) )
				continue;
			CollectDirectCargo( sub );
		}
	}

	protected void CollectDirectCargo( EntityAI holder )
	{
		if ( !holder || !holder.CanDisplayCargo() )
			return;

		GameInventory inv = holder.GetInventory();
		if ( !inv )
			return;

		CargoBase cargo = inv.GetCargo();
		if ( !cargo )
			return;

		int count = cargo.GetItemCount();
		int i;
		EntityAI item;
		string item_name;
		string item_qty;
		for ( i = 0; i < count; i++ )
		{
			item = cargo.GetItem( i );
			if ( !item )
				continue;

			item_name = GetItemName( item );
			item_qty = GetItemQty( item );
			AddCargoEntry( item_name, item_qty );
		}
	}

	//! Collapses identical unstacked items (like grenades or injectors) into a single row with a count
	protected void AddCargoEntry( string item_name, string item_qty )
	{
		if ( item_qty == "" )
		{
			int existing = m_CargoNames.Count();
			int i;
			for ( i = 0; i < existing; i++ )
			{
				if ( m_CargoQtys.Get( i ) == "" && m_CargoNames.Get( i ) == item_name )
				{
					m_CargoCounts.Set( i, m_CargoCounts.Get( i ) + 1 );
					return;
				}
			}
		}

		m_CargoNames.Insert( item_name );
		m_CargoQtys.Insert( item_qty );
		m_CargoCounts.Insert( 1 );
	}

	protected void AddSnapRow( int kind, string label, string name, string qty )
	{
		m_SnapKinds.Insert( kind );
		m_SnapLabels.Insert( label );
		m_SnapNames.Insert( name );
		m_SnapQtys.Insert( qty );
	}

	protected int GetAttachedSlotId( EntityAI item )
	{
		if ( !item )
			return InventorySlots.INVALID;

		GameInventory inv = item.GetInventory();
		if ( !inv )
			return InventorySlots.INVALID;

		if ( !inv.GetCurrentInventoryLocation( m_TempLoc ) )
			return InventorySlots.INVALID;

		return m_TempLoc.GetSlot();
	}

	protected string GetSlotLabel( int slot_id )
	{
		if ( !InventorySlots.IsSlotIdValid( slot_id ) )
			return "";

		string label = InventorySlots.GetSlotDisplayName( slot_id );
		if ( label != "" )
		{
			if ( label.Substring( 0, 1 ) == "#" )
				label = Widget.TranslateString( label );
			if ( label != "" && label.Substring( 0, 1 ) != "#" )
				return label;
		}

		string fallback = InventorySlots.GetSlotName( slot_id );
		return fallback;
	}

	protected string GetItemName( EntityAI item )
	{
		if ( !item )
			return "";

		string name = item.GetDisplayName();
		if ( name != "" )
		{
			if ( name.Substring( 0, 1 ) == "#" )
				name = Widget.TranslateString( name );
			if ( name != "" )
				return name;
		}

		return item.GetType();
	}

	protected string GetItemQty( EntityAI item )
	{
		if ( !item )
			return "";

		Magazine mag;
		if ( Class.CastTo( mag, item ) )
		{
			int ammo = mag.GetAmmoCount();
			int ammo_max = mag.GetAmmoMax();
			if ( mag.IsAmmoPile() )
			{
				if ( ammo <= 0 )
					return "";
				return "x" + ammo.ToString();
			}
			if ( ammo_max > 0 )
				return ammo.ToString() + "/" + ammo_max.ToString();
			return "";
		}

		ItemBase item_base;
		if ( !Class.CastTo( item_base, item ) )
			return "";

		int qty_type = QuantityConversions.HasItemQuantity( item_base );
		if ( qty_type == QUANTITY_HIDDEN )
			return "";

		int qty_max = item_base.GetQuantityMax();
		if ( qty_max <= 0 )
			return "";

		float qty = item_base.GetQuantity();
		if ( qty_type == QUANTITY_PROGRESS )
		{
			int pct = Math.Clamp( Math.Round( ( qty / qty_max ) * 100.0 ), 0, 100 );
			return pct.ToString() + "%";
		}

		if ( qty_max <= 1 )
			return "";

		int count = Math.Round( qty );
		if ( count <= 0 )
			return "";

		return "x" + count.ToString();
	}

	protected bool IsSameSnapshot( bool has_target, string target_name, string hint )
	{
		if ( !m_DrawnValid || has_target != m_DrawnHasTarget )
			return false;
		if ( target_name != m_DrawnTargetName || hint != m_DrawnHint )
			return false;

		int count = m_SnapKinds.Count();
		if ( count != m_DrawnKinds.Count() )
			return false;

		int i;
		for ( i = 0; i < count; i++ )
		{
			if ( m_SnapKinds.Get( i ) != m_DrawnKinds.Get( i ) )
				return false;
			if ( m_SnapLabels.Get( i ) != m_DrawnLabels.Get( i ) )
				return false;
			if ( m_SnapNames.Get( i ) != m_DrawnNames.Get( i ) )
				return false;
			if ( m_SnapQtys.Get( i ) != m_DrawnQtys.Get( i ) )
				return false;
		}
		return true;
	}

	protected void SaveDrawnSnapshot( bool has_target, string target_name, string hint )
	{
		m_DrawnValid = true;
		m_DrawnHasTarget = has_target;
		m_DrawnTargetName = target_name;
		m_DrawnHint = hint;
		m_DrawnKinds.Clear();
		m_DrawnLabels.Clear();
		m_DrawnNames.Clear();
		m_DrawnQtys.Clear();

		int count = m_SnapKinds.Count();
		int i;
		for ( i = 0; i < count; i++ )
		{
			m_DrawnKinds.Insert( m_SnapKinds.Get( i ) );
			m_DrawnLabels.Insert( m_SnapLabels.Get( i ) );
			m_DrawnNames.Insert( m_SnapNames.Get( i ) );
			m_DrawnQtys.Insert( m_SnapQtys.Get( i ) );
		}
	}

	protected void DrawSnapshot( bool has_target, string target_name, string hint )
	{
		SetFittedText( m_TargetText, target_name );
		m_HintText.SetText( hint );

		// Null watched player on client: show only the header band with the player's name
		int count = m_SnapKinds.Count();
		if ( !has_target || count <= 0 )
		{
			ShowBack( 0 );
			m_Table.Show( false );
			return;
		}

		int left = 0;
		int right = 0;
		int centre = 0;
		if ( count <= CENTRE_LINES )
		{
			centre = count;
		}
		else
		{
			left = ( count + 1 ) / 2;
			right = count - left;
		}

		int lines = left;
		if ( centre > lines )
			lines = centre;
		ShowBack( lines );
		m_Table.Show( true );

		string cargo_repeat = Widget.TranslateString( "#STR_BR_SPECGEAR_CARGO" );
		int i;
		for ( i = 0; i < ROW_LINES; i++ )
			FillRow( i, i, i, i < left, cargo_repeat );
		for ( i = ROW_LINES; i < SIDE_ROWS; i++ )
			FillRow( i, i - ROW_LINES, left + i - ROW_LINES, i - ROW_LINES < right, cargo_repeat );
		for ( i = SIDE_ROWS; i < ROW_COUNT; i++ )
			FillRow( i, i - SIDE_ROWS, i - SIDE_ROWS, i - SIDE_ROWS < centre, cargo_repeat );
	}

	protected void FillRow( int row, int line, int index, bool shown, string cargo_repeat )
	{
		Widget frame = m_Rows[row];
		frame.Show( shown );
		if ( !shown )
			return;

		int kind = m_SnapKinds.Get( index );
		string label = m_SnapLabels.Get( index );
		string name = m_SnapNames.Get( index );
		string qty = m_SnapQtys.Get( index );

		// Repeat the Cargo label when cargo rows wrap onto the first line of the right column
		if ( kind == KIND_CARGO && label == "" && line == 0 )
			label = cargo_repeat;

		bool is_hands = kind == KIND_HANDS;
		bool is_sub = kind == KIND_SUB;
		bool is_muted = kind == KIND_EMPTY || kind == KIND_MORE;
		bool has_qty = qty != "";

		Widget back = m_RowBack[row];
		Widget bar = m_RowBar[row];
		Widget dash = m_RowDash[row];
		TextWidget label_w = m_RowLabel[row];
		TextWidget name_w = m_RowName[row];
		TextWidget name_wide_w = m_RowNameWide[row];
		TextWidget sub_w = m_RowSub[row];
		TextWidget sub_wide_w = m_RowSubWide[row];
		TextWidget qty_w = m_RowQty[row];

		bar.Show( is_hands );
		dash.Show( is_sub );
		back.Show( is_hands || line % 2 == 1 );
		if ( is_hands )
			back.SetColor( ARGB( 56, 143, 110, 5 ) );
		else
			back.SetColor( ARGB( 51, 77, 82, 92 ) );

		bool show_label = label != "";
		label_w.Show( show_label );
		if ( show_label )
			SetFittedText( label_w, label );

		qty_w.Show( has_qty );
		if ( has_qty )
			qty_w.SetText( qty );

		int primary_color = ARGB( 255, 242, 242, 242 );
		if ( is_hands )
			primary_color = ARGB( 255, 255, 237, 184 );
		else if ( is_muted )
			primary_color = ARGB( 255, 148, 153, 162 );

		name_w.Show( !is_sub && has_qty );
		name_wide_w.Show( !is_sub && !has_qty );
		sub_w.Show( is_sub && has_qty );
		sub_wide_w.Show( is_sub && !has_qty );

		if ( is_sub )
		{
			if ( has_qty )
				SetFittedText( sub_w, name );
			else
				SetFittedText( sub_wide_w, name );
		}
		else
		{
			if ( has_qty )
			{
				name_w.SetColor( primary_color );
				SetFittedText( name_w, name );
			}
			else
			{
				name_wide_w.SetColor( primary_color );
				SetFittedText( name_wide_w, name );
			}
		}
	}

	protected void ShowBack( int index )
	{
		Widget back;
		for ( int i = 0; i < BACK_COUNT; i++ )
		{
			back = m_Backs[i];
			back.Show( i == index );
		}
	}

	//! Writes text in the widget, cut with an ellipsis when wider than the widget; the widget must be visible on screen
	protected void SetFittedText( TextWidget widget, string text )
	{
		widget.SetText( text );
		if ( text == "" )
			return;

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

	protected string GetCloseHint()
	{
		string button = InputUtils.GetButtonNameFromInput( INPUT_NAME, EInputDeviceType.MOUSE_AND_KEYBOARD );
		if ( button == "" )
			return "";

		string format = Widget.TranslateString( "#STR_BR_SPECGEAR_CLOSE_HINT" );
		return string.Format( format, button );
	}
}
#endif

//! Widgets of one notification drawn as a kill feed row (gui/layouts/hud/notification.layout), and its text
class BR_NotificationRow
{
	static const int LAYOUT_LINES = 6;  //!< text lines in notification.layout; the next ones come from notification_line.layout

	Widget m_Root;
	Widget m_Back;
	Widget m_Accent;
	ImageWidget m_Icon;
	ref array<TextWidget> m_Lines;      //!< the lines of the layout, then the ones EnsureLines added; kept for every layout
	string m_Text;      //!< the whole text, translated; wrapped into the lines by NotificationUI.BR_LayoutRow

	//! Finds the widgets of the row under root; false when the layout lacks one
	bool Init(Widget root)
	{
		m_Root = root;
		m_Lines = new array<TextWidget>;
		m_Back = root.FindAnyWidget("BRNotificationBack");
		m_Accent = root.FindAnyWidget("BRNotificationAccent");
		m_Icon = ImageWidget.Cast(root.FindAnyWidget("BRNotificationIcon"));
		if (!m_Back || !m_Accent || !m_Icon)
			return false;

		string line_name;
		TextWidget line;
		for (int i = 0; i < LAYOUT_LINES; i++)
		{
			line_name = "BRNotificationLine" + i.ToString();
			line = TextWidget.Cast(root.FindAnyWidget(line_name));
			if (!line)
				return false;
			m_Lines.Insert(line);
		}

		return true;
	}

	//! At least count lines: the missing ones are created from line_layout under the row, once, and kept to be used
	//! again (hidden while a layout does not need them). Called when the row is laid out, never every frame.
	//! False when a line could not be created: the row then has fewer lines than count
	bool EnsureLines(int count, string line_layout)
	{
		Widget created;
		TextWidget line;
		while (m_Lines.Count() < count)
		{
			created = g_Game.GetWorkspace().CreateWidgets(line_layout, m_Root);
			line = TextWidget.Cast(created);
			if (!line)
			{
				if (created)
					created.Unlink();
				return false;
			}
			m_Lines.Insert(line);
		}
		return true;
	}
}

//! Notifications are display only: the root of each notification element takes mouse clicks
//! (click to dismiss), so a notification over the inventory or the map swallows the clicks meant for what is under it.
//! Each notification is drawn as a kill feed row, mirrored for the left side: a dark band that hugs the text, the icon,
//! the text in the font of the kill feed and a thin accent on the left edge; the text wraps short of what is centred
//! at the top. Battle royale notifications show their message alone, with their colour on the icon and the accent,
//! and play a short vanilla sound.
modded class NotificationUI
{
	//! Top left corner of the notification stack, as fractions of the screen: the corner of the BattleRoyale HUD
	//! (HudPanel, 90 % of the screen, centred). The phase line is centred and the kill feed is on the right,
	//! so the notifications cover neither
	protected static const float BR_NOTIFICATION_LEFT = 0.05;
	protected static const float BR_NOTIFICATION_TOP = 0.05;

	protected static const string BR_ROW_LAYOUT = "Vigrid-BattleRoyale/GUI/layouts/hud/notification.layout";
	//! One more text line, as BRNotificationLine1-5, for a text longer than the lines of BR_ROW_LAYOUT
	protected static const string BR_LINE_LAYOUT = "Vigrid-BattleRoyale/GUI/layouts/hud/notification_line.layout";

	// Geometry of notification.layout, in layout units: the one of a kill feed row (BattleRoyaleKillFeed)
	protected static const float BR_ROW_HEIGHT = 30;        //!< band of one line
	protected static const float BR_ROW_PITCH = 40;         //!< from one band to the next, as the kill feed rows
	protected static const float BR_LINE_PITCH = 26;        //!< from one text line to the next (BRNotificationLine*)
	protected static const float BR_LINE_Y = 1;             //!< top of the first text line (BRNotificationLine0)
	protected static const float BR_TEXT_X = 42;
	protected static const float BR_ROW_PADDING = 10;       //!< after the widest line
	protected static const float BR_ACCENT_WIDTH = 3;
	protected static const float BR_CENTRE_GAP = 16;        //!< between the bands and what is centred at the top
	protected static const float BR_MIN_ROOM = 240;         //!< narrowest room a band wraps to, whatever is centred
	protected static const float BR_ACCENT_ALPHA = 0.5;
	protected static const string BR_TITLE_SEPARATOR = " — ";

	//! Padding of NotificationContent (notifications.layout), in screen pixels: the grid puts it between the elements
	protected static const float BR_CONTENT_PADDING_PX = 4;
	//! The centred block that moved this far left (screen pixels) lays the rows out again
	protected static const float BR_RELAYOUT_PX = 8;

	//! Left edge, in screen pixels, of what is centred at the top (compass, phase, zone damage), -1 for none.
	//! Set every frame by the mission (5_Mission), which owns those widgets
	protected static float s_BR_CentreLeft = -1;

	protected ref map<NotificationRuntimeData, ref BR_NotificationRow> m_BR_Rows;    //!< rows on screen, not fading
	protected ref array<ref BR_NotificationRow> m_BR_FadingRows;    //!< removed rows vanilla still fades (m_WidgetTimers)
	protected float m_BR_LayoutLimit;       //!< right limit (screen pixels) the rows were laid out for
	protected int m_BR_LayoutScreenW;       //!< screen size the rows were laid out for
	protected int m_BR_LayoutScreenH;
	protected bool m_BR_LayoutOffset;       //!< m_OffsetEnabled the rows were laid out with
	protected float m_BR_LayoutRootX;       //!< left edge of the root (screen pixels) the rows were laid out for
	protected float m_BR_LayoutRootW;       //!< width of the root (screen pixels) the rows were laid out for
	protected bool m_BR_Unmeasured;         //!< a row could not measure its text: laid out again next frame

	void NotificationUI()
	{
		m_BR_Rows = new map<NotificationRuntimeData, ref BR_NotificationRow>;
		m_BR_FadingRows = new array<ref BR_NotificationRow>;
		m_BR_LayoutLimit = -1;
		m_BR_LayoutScreenW = -1;
		m_BR_LayoutScreenH = -1;
		m_BR_LayoutRootX = -1;
		m_BR_LayoutRootW = -1;
		s_BR_CentreLeft = -1;
		BR_PlaceRoot();
	}

	//! left: left edge, in screen pixels, of what is centred at the top of the screen, -1 for none
	static void BR_SetCentreLeft(float left)
	{
		s_BR_CentreLeft = left;
	}

	override void Update(float timeslice)
	{
		super.Update(timeslice);
		BR_PlaceRoot();
		BR_UpdateRows();
	}

	//! The root of notifications.layout is centred (halign center_ref), which a script cannot change, so the corner
	//! is set in screen pixels, again whenever it moved: a new screen size, or vanilla resizing the centred root
	protected void BR_PlaceRoot()
	{
		// While a menu shows its own notification panel, vanilla moves the stack under it (UpdateOffset)
		if (!m_Root || m_OffsetEnabled)
			return;

		int screenWidth, screenHeight;
		GetScreenSize(screenWidth, screenHeight);
		float x = screenWidth * BR_NOTIFICATION_LEFT;
		float y = screenHeight * BR_NOTIFICATION_TOP;

		float currentX, currentY;
		m_Root.GetScreenPos(currentX, currentY);
		if (Math.AbsFloat(currentX - x) > 0.5 || Math.AbsFloat(currentY - y) > 0.5)
			m_Root.SetScreenPos(x, y);
	}

	//! Creates the row of the mod in place of the element of vanilla (notificationui.c:48-72) and registers it in
	//! m_Notifications as vanilla does, so RemoveNotification and the fade of Update (notificationui.c:74-83,
	//! 196-229) take it as they take theirs. Without the layout of the mod, the element of vanilla
	override void AddNotification(NotificationRuntimeData data)
	{
		if (m_Root)
			m_Root.SetFlags(WidgetFlags.IGNOREPOINTER);
		if (m_Spacer)
			m_Spacer.SetFlags(WidgetFlags.IGNOREPOINTER);

		bool is_br = data.GetTitleText() == DAYZBR_MSG_TITLE;

		BR_NotificationRow row = BR_CreateRow(data, is_br);
		if (row)
		{
			m_Notifications.Insert(data, row.m_Root);
			m_BR_Rows.Insert(data, row);
			BR_LayoutRows();
			UpdateTargetHeight();
		}
		else
		{
			super.AddNotification(data);

			Widget notification;
			if (m_Notifications.Find(data, notification) && notification)
			{
				notification.SetFlags(WidgetFlags.IGNOREPOINTER);
				Widget vanilla_icon = notification.FindAnyWidget("Image");
				if (is_br && vanilla_icon)
					vanilla_icon.SetColor(data.GetColor());
			}
		}

		if (is_br)
			BR_PlayNotificationSound();
	}

	//! A removed row fades out with vanilla (Update, notificationui.c:196-227), which deletes its widget at the end; until
	//! then it is still laid out with the others, so a new screen size does not leave it clipped
	override void RemoveNotification(NotificationRuntimeData data)
	{
		super.RemoveNotification(data);
		if (!m_BR_Rows)
			return;

		BR_NotificationRow row;
		if (m_BR_Rows.Find(data, row) && row && m_BR_FadingRows)
			m_BR_FadingRows.Insert(row);
		m_BR_Rows.Remove(data);
	}

	//! Forgets the fading rows whose widget vanilla deleted (a Widget reference is nulled when it is deleted)
	protected void BR_PruneFadingRows()
	{
		if (!m_BR_FadingRows)
			return;

		for (int i = m_BR_FadingRows.Count() - 1; i >= 0; i--)
		{
			BR_NotificationRow row = m_BR_FadingRows.Get(i);
			if (!row || !row.m_Root)
				m_BR_FadingRows.Remove(i);
		}
	}

	//! The row of data in NotificationContent, its icon, colours and text set; null when the layout cannot be used.
	//! Its widgets are named BRNotification*: the fade of vanilla looks for TopSpacer, BottomSpacer, Title, Detail
	//! and Name (notificationui.c:203-207) and finds none of them, so it fades the root, whose children inherit alpha
	protected BR_NotificationRow BR_CreateRow(NotificationRuntimeData data, bool is_br)
	{
		if (!m_NotificationContent || !m_BR_Rows)
			return null;

		Widget root = g_Game.GetWorkspace().CreateWidgets(BR_ROW_LAYOUT, m_NotificationContent);
		if (!root)
			return null;

		BR_NotificationRow row = new BR_NotificationRow();
		if (!row.Init(root))
		{
			Error("[NotificationUI] " + BR_ROW_LAYOUT + " is not complete");
			root.Unlink();
			return null;
		}

		// Battle royale notifications carry their colour on the icon and the accent, the others are white
		int color = ARGB(255, 255, 255, 255);
		if (is_br)
			color = data.GetColor();

		string icon_path = data.GetIcon();
		if (icon_path != "")
			row.m_Icon.LoadImageFile(0, icon_path);
		row.m_Icon.SetColor(color);
		row.m_Accent.SetColor(color);
		row.m_Accent.SetAlpha(BR_ACCENT_ALPHA);

		row.m_Text = BR_RowText(data, is_br);
		return row;
	}

	//! The message alone for a battle royale notification (its title is the name of the mod); else title and
	//! message in one sentence
	protected string BR_RowText(NotificationRuntimeData data, bool is_br)
	{
		string title = BR_Translate(data.GetTitleText());
		string detail = BR_Translate(data.GetDetailText());

		if (is_br && detail != "")
			return detail;
		if (title == "")
			return detail;
		if (detail == "")
			return title;
		return title + BR_TITLE_SEPARATOR + detail;
	}

	//! The text of a stringtable key ("#STR_..."); any other text as it is. Translated before the text is cut into lines
	protected string BR_Translate(string text)
	{
		if (text.IndexOf("#") == 0)
			return Widget.TranslateString(text);
		return text;
	}

	//! Lays the rows out again when their room changed: a new screen size, the centred block grew toward them, a menu
	//! panel started or stopped moving the root (m_OffsetEnabled), or the root moved or was resized under the rows.
	//! A row that could not measure its text tries again every frame
	protected void BR_UpdateRows()
	{
		BR_PruneFadingRows();
		if (!m_BR_Rows || (m_BR_Rows.Count() == 0 && (!m_BR_FadingRows || m_BR_FadingRows.Count() == 0)))
			return;

		if (!m_BR_Unmeasured && BR_LayoutIsCurrent())
			return;

		BR_LayoutRows();
		UpdateTargetHeight();
	}

	//! True when the rows were laid out for the room there is now (see BR_UpdateRows)
	protected bool BR_LayoutIsCurrent()
	{
		int screen_w;
		int screen_h;
		GetScreenSize(screen_w, screen_h);
		if (screen_w != m_BR_LayoutScreenW || screen_h != m_BR_LayoutScreenH)
			return false;
		if (BR_CentreLimit(screen_w) < m_BR_LayoutLimit - BR_RELAYOUT_PX)
			return false;
		if (m_OffsetEnabled != m_BR_LayoutOffset)
			return false;
		if (!m_Root)
			return true;

		float root_x;
		float root_y;
		m_Root.GetScreenPos(root_x, root_y);
		if (Math.AbsFloat(root_x - m_BR_LayoutRootX) > 0.5)
			return false;

		// The root keeps the width BR_SetRootWidth gave it, except while vanilla moves it: then the rows follow its width
		if (m_OffsetEnabled)
		{
			float root_w;
			float root_h;
			m_Root.GetScreenSize(root_w, root_h);
			if (Math.AbsFloat(root_w - m_BR_LayoutRootW) > 0.5)
				return false;
		}

		return true;
	}

	//! Right limit of the rows, in screen pixels, before the gap: the left edge of what is centred at the top, or the
	//! middle of the screen when nothing is
	protected float BR_CentreLimit(int screen_w)
	{
		if (s_BR_CentreLeft >= 0)
			return s_BR_CentreLeft;
		return screen_w * 0.5;
	}

	//! Screen pixels per exact layout unit: the units are 1/1080 of the screen height. SetPos/SetSize from a script
	//! and GetTextSize take and give screen pixels
	protected float BR_PxPerUnit()
	{
		int screen_w;
		int screen_h;
		GetScreenSize(screen_w, screen_h);
		if (screen_h <= 0)
			return 1;
		return screen_h / 1080.0;
	}

	//! Width of the vanilla root in screen pixels; skipped while a menu shows its own notification panel (m_OffsetEnabled)
	protected void BR_SetRootWidth(float width_px)
	{
		if (!m_Root || m_OffsetEnabled)
			return;

		float root_w;
		float root_h;
		m_Root.GetScreenSize(root_w, root_h);
		if (Math.AbsFloat(root_w - width_px) <= 0.5)
			return;

		m_Width = width_px;
		m_Root.SetSize(width_px, root_h);
	}

	//! Lays every row out for the room there is now: from the left edge of the stack to BR_CENTRE_GAP short of what
	//! is centred at the top, never narrower than BR_MIN_ROOM. While a menu panel moves the root (m_OffsetEnabled),
	//! never wider than the root as it is on screen, which is not resized then
	protected void BR_LayoutRows()
	{
		int screen_w;
		int screen_h;
		GetScreenSize(screen_w, screen_h);
		float px_per_unit = BR_PxPerUnit();
		float limit = BR_CentreLimit(screen_w);

		float root_x = screen_w * BR_NOTIFICATION_LEFT;
		float root_y;
		float root_w = -1;
		float root_h;
		if (m_Root)
		{
			m_Root.GetScreenPos(root_x, root_y);
			m_Root.GetScreenSize(root_w, root_h);
		}

		float room_px = limit - BR_CENTRE_GAP * px_per_unit - root_x;
		room_px = Math.Max(room_px, BR_MIN_ROOM * px_per_unit);

		if (m_OffsetEnabled)
		{
			// Vanilla moves the root under the panel of the menu (UpdateOffset, notificationui.c:148-174) and gives it
			// its place back when the panel closes: the root is left alone, the rows fit in its width as it is
			if (root_w > 0)
				room_px = Math.Min(room_px, root_w);
		}
		else
		{
			// The vanilla root is 531 px wide (notifications.layout:3) and clips the rows: it takes the width of the
			// room. m_Width is the width vanilla Update keeps when it animates the height (notificationui.c:143,187,192)
			BR_SetRootWidth(room_px);
			root_w = room_px;
		}

		m_BR_LayoutLimit = limit;
		m_BR_LayoutScreenW = screen_w;
		m_BR_LayoutScreenH = screen_h;
		m_BR_LayoutOffset = m_OffsetEnabled;
		m_BR_LayoutRootX = root_x;
		m_BR_LayoutRootW = root_w;
		m_BR_Unmeasured = false;

		BR_NotificationRow row;
		for (int i = 0; i < m_BR_Rows.Count(); i++)
		{
			row = m_BR_Rows.GetElement(i);
			if (row && !BR_LayoutRow(row, room_px, px_per_unit))
				m_BR_Unmeasured = true;
		}

		// Rows still fading take the same room: they stay on screen up to NotificationSystem.NOTIFICATION_FADE_TIME
		if (m_BR_FadingRows)
		{
			for (int j = 0; j < m_BR_FadingRows.Count(); j++)
			{
				row = m_BR_FadingRows.Get(j);
				if (row && row.m_Root && !BR_LayoutRow(row, room_px, px_per_unit))
					m_BR_Unmeasured = true;
			}
		}
	}

	//! Writes the text of row in as many lines as it needs to be at most room_px wide (screen pixels, the whole band)
	//! and sizes the band, the accent and the element to them: the row grows down, one BR_LINE_PITCH per line.
	//! Nothing is cut: a word wider than a line is split, and a row with more lines than notification.layout gets
	//! more line widgets (BR_NotificationRow.EnsureLines). False when the text could not be measured: it is then
	//! written at its own line breaks on a band as wide as the room, and laid out again next frame
	protected bool BR_LayoutRow(BR_NotificationRow row, float room_px, float px_per_unit)
	{
		float text_room = room_px - (BR_TEXT_X + BR_ROW_PADDING) * px_per_unit;

		array<string> lines = new array<string>;
		bool measured = BR_WrapText(row, row.m_Text, text_room, lines);

		if (!row.EnsureLines(lines.Count(), BR_LINE_LAYOUT))
		{
			// Only when notification_line.layout cannot be loaded: the lines past the last widget join it, the
			// band overflows its room rather than losing text
			Error("[NotificationUI] " + BR_LINE_LAYOUT + " could not be loaded");
			int last = row.m_Lines.Count() - 1;
			while (lines.Count() > row.m_Lines.Count())
			{
				lines.Set(last, lines.Get(last) + " " + lines.Get(last + 1));
				lines.RemoveOrdered(last + 1);
			}
		}

		int line_count = lines.Count();
		float text_w = 0;
		int line_w;
		int line_h;
		TextWidget line_widget;
		for (int i = 0; i < row.m_Lines.Count(); i++)
		{
			line_widget = row.m_Lines.Get(i);
			if (i >= line_count)
			{
				line_widget.Show(false);
				continue;
			}

			// The lines of notification.layout have their place in it; the added ones are placed here, in screen pixels
			if (i >= BR_NotificationRow.LAYOUT_LINES)
				line_widget.SetPos(BR_TEXT_X * px_per_unit, (BR_LINE_Y + i * BR_LINE_PITCH) * px_per_unit);

			line_widget.Show(true);
			line_widget.SetText(lines.Get(i));
			line_widget.GetTextSize(line_w, line_h);
			if (line_w > text_w)
				text_w = line_w;
		}

		// GetTextSize measures in screen pixels; the band is laid out in layout units and set in screen pixels
		float width = BR_TEXT_X + text_w / px_per_unit + BR_ROW_PADDING;
		if (!measured)
			width = room_px / px_per_unit;
		float height = BR_ROW_HEIGHT + (line_count - 1) * BR_LINE_PITCH;

		row.m_Back.SetSize(width * px_per_unit, height * px_per_unit);
		row.m_Accent.SetSize(BR_ACCENT_WIDTH * px_per_unit, height * px_per_unit);
		// The element spans the band and the gap to the next one; the grid adds its padding between elements.
		// Its width stays relative (the cell of the grid), the band is drawn from its left edge
		row.m_Root.SetSize(1, height * px_per_unit + (BR_ROW_PITCH - BR_ROW_HEIGHT) * px_per_unit - BR_CONTENT_PADDING_PX);
		return measured;
	}

	//! Splits text into lines at most room_px wide as measured: at its line breaks, between words, and inside a word
	//! wider than a line. Empty lines of the text are left out. False when the text could not be measured: lines
	//! then holds the text cut at its line breaks only
	protected bool BR_WrapText(BR_NotificationRow row, string text, float room_px, array<string> lines)
	{
		array<string> paragraphs = new array<string>;
		array<string> words = new array<string>;
		text.Split("\n", paragraphs);

		string paragraph;
		string word;
		string line;
		string candidate;
		int width;
		int fit;
		int word_length;
		for (int p = 0; p < paragraphs.Count(); p++)
		{
			paragraph = paragraphs.Get(p);
			words.Clear();
			paragraph.Split(" ", words);
			line = "";

			for (int w = 0; w < words.Count(); w++)
			{
				word = words.Get(w);
				if (word == "")
					continue;

				candidate = word;
				if (line != "")
					candidate = line + " " + word;

				width = BR_MeasureText(row, candidate);
				if (width < 0)
				{
					// array.InsertAll (enscript.c:449)
					lines.Clear();
					lines.InsertAll(paragraphs);
					return false;
				}

				if (width <= room_px)
				{
					line = candidate;
					continue;
				}

				if (line != "")
				{
					lines.Insert(line);
					line = "";
					width = BR_MeasureText(row, word);
				}

				// A word wider than a line: the longest start that fits, as many times as it takes
				word_length = word.LengthUtf8();
				while (width > room_px && word_length > 1)
				{
					fit = BR_FitChars(row, word, room_px);
					lines.Insert(word.SubstringUtf8(0, fit));
					word = word.SubstringUtf8(fit, word_length - fit);
					word_length = word.LengthUtf8();
					width = BR_MeasureText(row, word);
				}

				line = word;
			}

			if (line != "")
				lines.Insert(line);
		}

		if (lines.Count() == 0)
			lines.Insert(text);

		return true;
	}

	//! Most characters of word (at least 1, fewer than all) whose start fits in room_px as measured (binary search)
	protected int BR_FitChars(BR_NotificationRow row, string word, float room_px)
	{
		int low = 1;
		int high = word.LengthUtf8() - 1;
		int mid;
		while (low < high)
		{
			mid = (low + high + 1) / 2;
			if (BR_MeasureText(row, word.SubstringUtf8(0, mid)) <= room_px)
				low = mid;
			else
				high = mid - 1;
		}
		return low;
	}

	//! Width in screen pixels of text written in the first line of row, -1 when it could not be measured.
	//! All lines have the same font and size, so the first one measures for all
	protected int BR_MeasureText(BR_NotificationRow row, string text)
	{
		TextWidget line_widget = row.m_Lines.Get(0);
		line_widget.SetText(text);

		int text_w;
		int text_h;
		line_widget.GetTextSize(text_w, text_h);
		if (text_w <= 0 && text != "")
			return -1;
		return text_w;
	}

	//! Tick time of the last notification sound; notifications closer than BR_MSG_SOUND_GAP play one sound
	protected static float s_BR_LastSoundTime = -1000;
	protected static const float BR_MSG_SOUND_GAP = 1.0;

	//! Plays the notification sound in gameplay only (a player exists), at most once per BR_MSG_SOUND_GAP seconds,
	//! and never in silent mode
	protected void BR_PlayNotificationSound()
	{
		if (!GetGame().GetPlayer() || BattleRoyaleSilentMode.IsActive())
			return;

		float now = GetGame().GetTickTime();
		if (now - s_BR_LastSoundTime < BR_MSG_SOUND_GAP)
			return;
		s_BR_LastSoundTime = now;

		EffectSound sound = SEffectManager.PlaySound(DAYZBR_MSG_SOUND, GetGame().GetCurrentCameraPosition());
		if (sound)
			sound.SetAutodestroy(true);
	}
}

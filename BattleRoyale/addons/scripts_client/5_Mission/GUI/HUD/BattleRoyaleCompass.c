#ifndef SERVER
//! Compass at the top centre of the HUD: a horizontal tape that scrolls with the camera heading (a mark every 15
//! degrees, letters on the multiples of 45, degrees on the others, a fixed mark in the centre), the heading in
//! degrees under the centre and the player position under it.
//! Every widget comes from br_hud.layout; an update only moves the marks and changes the texts that changed.
//! It shows while the local player is alive and the vanilla HUD shows, and the player can hide it with its key
class BattleRoyaleCompass
{
	static const int MARK_COUNT = 12;                   //!< CompassMark0 .. CompassMark11 in br_hud.layout

	// Geometry of the tape in br_hud.layout, in layout units
	protected static const float TAPE_WIDTH = 420;     //!< widest tape, the one of the layout; it sets the scale too
	protected static const float TAPE_MIN_WIDTH = 120; //!< narrowest tape: a 5:4 screen leaves 121 units between the notification stack and its mirror
	protected static const float PANEL_HEIGHT = 66;
	protected static const float TAPE_HEIGHT = 26;
	protected static const float BASE_LINE_HEIGHT = 2;
	protected static const float MARK_WIDTH = 60;
	protected static const float TICK_WIDTH = 2;
	protected static const float TICK_BOTTOM = 24;     //!< the ticks stand on the base line
	protected static const float MINOR_TICK_HEIGHT = 5;
	protected static const float MAJOR_TICK_HEIGHT = 9;
	protected static const float FADE_WIDTH = 60;      //!< the marks fade out over this distance before the tape ends

	// Notification stack on the top left (vanilla gui/layouts/new_ui/notifications/notifications.layout: NotificationRoot
	// is 531 exact units wide; NotificationUI.BR_PlaceRoot puts its left edge at 5 % of the screen width). The tape stays
	// NOTIFICATION_GAP right of it, so a narrow screen gets a narrower tape
	protected static const float NOTIFICATION_LEFT = 0.05;
	protected static const float NOTIFICATION_WIDTH = 531;
	protected static const float NOTIFICATION_GAP = 16;

	protected static const int STEP_DEG = 15;
	protected static const int STEP_COUNT = 24;        //!< steps in a full turn
	protected static const float FIELD_DEG = 160;      //!< degrees shown across the widest tape; a narrower one shows fewer
	protected static const float HEADING_THRESHOLD_DEG = 0.05;

	protected static const int COLOR_LETTER = 0xFFFFFFFF;
	protected static const int COLOR_NORTH = 0xFFFFC400;
	protected static const int COLOR_NUMBER = 0xFFC8C8C8;

	//! Hidden or shown with its key; kept when the HUD is built again for the next match
	protected static bool s_UserVisible = true;

	protected bool m_Ready;
	protected bool m_Shown;

	protected Widget m_Panel;
	protected Widget m_Tape;
	protected Widget m_BaseLine;
	protected TextWidget m_HeadingText;
	protected TextWidget m_CoordsText;

	protected Widget m_Marks[MARK_COUNT];
	protected Widget m_Ticks[MARK_COUNT];
	protected TextWidget m_Labels[MARK_COUNT];
	protected int m_MarkStep[MARK_COUNT];     //!< 15 degree step (0 .. 23) the mark shows, -1 before the first one
	protected bool m_MarkVisible[MARK_COUNT];

	protected bool m_HasHeading;
	protected float m_LastHeading;
	protected int m_LastHeadingText;          //!< whole degrees written, -1 for none
	protected bool m_HasCoords;
	protected int m_LastX;
	protected int m_LastZ;

	protected float m_TapeWidth;              //!< width of the tape now, in layout units
	protected float m_PxPerUnit;              //!< screen pixels per exact layout unit (screen height / 1080)
	protected int m_FitScreenW;               //!< screen size the tape width was fitted to, -1 for none
	protected int m_FitScreenH;

	void BattleRoyaleCompass(Widget root)
	{
		m_LastHeadingText = -1;
		m_TapeWidth = TAPE_WIDTH;
		m_PxPerUnit = 1;
		m_FitScreenW = -1;
		m_FitScreenH = -1;

		if (root)
		{
			m_Panel = root.FindAnyWidget("CompassPanel");
			m_Tape = root.FindAnyWidget("CompassTape");
			m_BaseLine = root.FindAnyWidget("CompassBaseLine");
			m_HeadingText = TextWidget.Cast(root.FindAnyWidget("CompassHeading"));
			m_CoordsText = TextWidget.Cast(root.FindAnyWidget("CompassCoords"));
		}

		m_Ready = true;
		if (!m_Panel || !m_Tape || !m_BaseLine || !m_HeadingText || !m_CoordsText)
			m_Ready = false;

		string mark_name;
		for (int i = 0; i < MARK_COUNT; i++)
		{
			m_MarkStep[i] = -1;
			m_MarkVisible[i] = true;
			if (!root)
				continue;

			mark_name = "CompassMark" + i.ToString();
			m_Marks[i] = root.FindAnyWidget(mark_name);
			m_Ticks[i] = root.FindAnyWidget(mark_name + "Tick");
			m_Labels[i] = TextWidget.Cast(root.FindAnyWidget(mark_name + "Label"));
			if (!m_Marks[i] || !m_Ticks[i] || !m_Labels[i])
				m_Ready = false;
		}

		if (!m_Ready)
		{
			Error("[BattleRoyaleCompass] compass widgets not found in the HUD layout");
			return;
		}

		// Shown by the first update that finds a living player
		SetShown(false);
		FitWidth();
	}

	void ToggleVisible()
	{
		s_UserVisible = !s_UserVisible;
	}

	//! Right edge of the tape, in screen pixels, while the compass shows; -1 while hidden. Kill feed rows stay right of it
	float GetScreenRight()
	{
		if (!m_Ready || !m_Shown || !m_Tape.IsVisibleHierarchy())
			return -1;

		float x;
		float y;
		float w;
		float h;
		m_Tape.GetScreenPos(x, y);
		m_Tape.GetScreenSize(w, h);
		return x + w;
	}

	/**
	 * Called once per frame by the client.
	 *
	 * @param hud  Vanilla HUD, whose visibility the compass follows; may be null
	 */
	void Update(IngameHud hud)
	{
		if (!m_Ready)
			return;

		FitWidth();

		PlayerBase player = PlayerBase.Cast(GetGame().GetPlayer());
		bool show = s_UserVisible && IsHudShown(hud);
		// Dead, spectating or waiting in the lobby while a match runs: no compass
		if (!player || !player.IsAlive() || BattleRoyaleRPC.s_Spectating || BattleRoyaleRPC.GetInstance().waiting)
			show = false;

		if (show != m_Shown)
			SetShown(show);

		if (!show)
			return;

		UpdateTape();
		UpdateCoords(player.GetPosition());
	}

	//! Same rule as the vanilla stats and notifiers (IngameHudVisibility.UpdateElement): hidden by the hide HUD key,
	//! the HUD option, an open menu or unconsciousness, unless the inventory is open
	protected bool IsHudShown(IngameHud hud)
	{
		if (!hud)
			return true;

		IngameHudVisibility visibility = hud.GetHudVisibility();
		if (!visibility)
			return true;

		if (!visibility.IsContextFlagActive(IngameHudVisibility.HUD_HIDE_FLAGS))
			return true;

		return visibility.IsContextFlagActive(EHudContextFlags.INVENTORY_OPEN);
	}

	//! Width of the tape for the screen size, set when the HUD is built and when the screen changes size: as wide as
	//! fits between the notification stack and its mirror on the right, from TAPE_MIN_WIDTH to TAPE_WIDTH. The scale
	//! stays, so a narrower tape shows fewer degrees.
	//! The layout scales exact units with the screen height, but SetPos and SetSize from a script take screen pixels,
	//! so every size and place the script sets is converted with m_PxPerUnit, refreshed here with the screen size
	protected void FitWidth()
	{
		int screen_w;
		int screen_h;
		GetScreenSize(screen_w, screen_h);
		if (screen_h <= 0 || (screen_w == m_FitScreenW && screen_h == m_FitScreenH))
			return;

		m_FitScreenW = screen_w;
		m_FitScreenH = screen_h;

		// Exact layout units are 1/1080 of the screen height; the panel is centred on the screen
		m_PxPerUnit = screen_h / 1080.0;
		float screen_units = screen_w * 1080.0 / screen_h;
		float notifications_right = screen_units * NOTIFICATION_LEFT + NOTIFICATION_WIDTH;
		float width = Math.Floor((screen_units * 0.5 - notifications_right - NOTIFICATION_GAP) * 2);
		width = Math.Clamp(width, TAPE_MIN_WIDTH, TAPE_WIDTH);

		// Applied even when the width in units did not change: the pixels per unit may have
		m_TapeWidth = width;
		m_Panel.SetSize(width * m_PxPerUnit, PANEL_HEIGHT * m_PxPerUnit);
		m_Tape.SetSize(width * m_PxPerUnit, TAPE_HEIGHT * m_PxPerUnit);
		m_BaseLine.SetSize(width * m_PxPerUnit, BASE_LINE_HEIGHT * m_PxPerUnit);

		// The marks are placed again, and their ticks sized again, for the new width and scale
		m_HasHeading = false;
		for (int i = 0; i < MARK_COUNT; i++)
		{
			m_MarkStep[i] = -1;
		}
	}

	protected void SetShown(bool show)
	{
		m_Shown = show;
		m_Panel.Show(show);

		// Nothing is updated while hidden, so everything is written again when shown
		m_HasHeading = false;
		m_HasCoords = false;
	}

	//! Heading of the camera: 0 north (+Z), clockwise, in [0, 360)
	protected float GetHeading()
	{
		return Math.NormalizeAngle(GetGame().GetCurrentCameraDirection().VectorToAngles()[0]);
	}

	protected void UpdateTape()
	{
		float heading = GetHeading();
		if (m_HasHeading && Math.AbsFloat(Math.DiffAngle(heading, m_LastHeading)) < HEADING_THRESHOLD_DEG)
			return;

		m_HasHeading = true;
		m_LastHeading = heading;

		float units_per_deg = TAPE_WIDTH / FIELD_DEG;
		float half_tape = m_TapeWidth * 0.5;
		float half_field = half_tape / units_per_deg;

		// The MARK_COUNT steps from the first one on the tape; a step always goes to the same mark (step modulo
		// MARK_COUNT), so a mark keeps its label while it scrolls across
		int first_step = Math.Ceil((heading - half_field) / STEP_DEG);
		int step;
		int slot;
		float offset;
		float alpha;
		bool visible;
		Widget mark;
		Widget tick;
		TextWidget label;

		for (int i = 0; i < MARK_COUNT; i++)
		{
			step = first_step + i;
			slot = ((step % MARK_COUNT) + MARK_COUNT) % MARK_COUNT;
			offset = (step * STEP_DEG - heading) * units_per_deg;
			visible = Math.AbsFloat(offset) <= half_tape;

			mark = m_Marks[slot];
			if (visible != m_MarkVisible[slot])
			{
				m_MarkVisible[slot] = visible;
				mark.Show(visible);
			}

			if (!visible)
				continue;

			tick = m_Ticks[slot];
			label = m_Labels[slot];
			SetMarkStep(slot, ((step % STEP_COUNT) + STEP_COUNT) % STEP_COUNT, tick, label);

			// Placed in layout units, set in screen pixels
			mark.SetPos((half_tape + offset - MARK_WIDTH * 0.5) * m_PxPerUnit, 0);

			alpha = Math.Clamp((half_tape - Math.AbsFloat(offset)) / FADE_WIDTH, 0, 1);
			tick.SetAlpha(alpha);
			label.SetAlpha(alpha);
		}

		int degrees = Math.Round(heading);
		if (degrees >= 360)
			degrees = 0;

		if (degrees != m_LastHeadingText)
		{
			m_LastHeadingText = degrees;
			m_HeadingText.SetText(FormatHeading(degrees));
		}
	}

	//! Writes the label and the tick of a 15 degree step (0 .. 23) on a mark, only when the mark shows another step
	protected void SetMarkStep(int slot, int step, Widget tick, TextWidget label)
	{
		if (m_MarkStep[slot] == step)
			return;

		m_MarkStep[slot] = step;

		int degrees = step * STEP_DEG;
		if (step % 3 == 0)
		{
			label.SetText(GetDirectionLetters(degrees));
			if (degrees == 0)
				label.SetColor(COLOR_NORTH);
			else
				label.SetColor(COLOR_LETTER);
			tick.SetSize(TICK_WIDTH * m_PxPerUnit, MAJOR_TICK_HEIGHT * m_PxPerUnit);
			tick.SetPos(0, (TICK_BOTTOM - MAJOR_TICK_HEIGHT) * m_PxPerUnit);
		}
		else
		{
			label.SetText(degrees.ToString());
			label.SetColor(COLOR_NUMBER);
			tick.SetSize(TICK_WIDTH * m_PxPerUnit, MINOR_TICK_HEIGHT * m_PxPerUnit);
			tick.SetPos(0, (TICK_BOTTOM - MINOR_TICK_HEIGHT) * m_PxPerUnit);
		}
	}

	protected string GetDirectionLetters(int degrees)
	{
		switch (degrees)
		{
			case 0:
				return "N";
			case 45:
				return "NE";
			case 90:
				return "E";
			case 135:
				return "SE";
			case 180:
				return "S";
			case 225:
				return "SW";
			case 270:
				return "W";
			case 315:
				return "NW";
		}

		return degrees.ToString();
	}

	//! Three digits, 000 .. 359
	protected string FormatHeading(int degrees)
	{
		string text = degrees.ToString();
		if (degrees < 10)
			return "00" + text;
		if (degrees < 100)
			return "0" + text;
		return text;
	}

	//! Position in whole metres, written only when it changed
	protected void UpdateCoords(vector pos)
	{
		int x = Math.Round(pos[0]);
		int z = Math.Round(pos[2]);
		if (m_HasCoords && x == m_LastX && z == m_LastZ)
			return;

		m_HasCoords = true;
		m_LastX = x;
		m_LastZ = z;
		m_CoordsText.SetText("X " + x.ToString() + "  Z " + z.ToString());
	}
}
#endif

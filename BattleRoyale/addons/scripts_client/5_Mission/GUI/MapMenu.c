#ifndef SERVER
//! Draws the zone in force (red) and the announced zone (yellow) on the 2D map.
//! Once the announced zone locks it is the zone in force and turns red.
//! It also draws the player's own marker, a filled arrow of a fixed screen size that points where the body faces;
//! it replaces the vanilla GPS circle and arrow, whose size follows the map zoom.
modded class MapMenu
{
	protected const string BR_MAP_ZONES_LAYOUT = "Vigrid-BattleRoyale/GUI/layouts/map/royale_map_zones.layout";
	protected const string BR_MAP_PLAYER_LAYOUT = "Vigrid-BattleRoyale/GUI/layouts/map/royale_map_player.layout";

	//! Arrow height in 1/1080 of the screen height, the unit of the exact layout sizes
	protected const float BR_PLAYER_ARROW_HEIGHT = 28;
	protected const int BR_PLAYER_ARROW_FILL = 0xFFFFFFFF;
	protected const int BR_PLAYER_ARROW_BORDER = 0xFF141414;
	protected const float BR_PLAYER_ARROW_BORDER_WIDTH = 3;
	//! The arrow is only redrawn when its centre moved this far in pixels or its direction changed this much (sine of the angle)
	protected const float BR_PLAYER_MOVE_THRESHOLD_PX = 0.25;
	protected const float BR_PLAYER_TURN_THRESHOLD = 0.005;

	protected CanvasWidget m_BR_ZoneCanvas;
	protected CanvasWidget m_BR_PlayerCanvas;

	// What the player canvas shows
	protected bool b_BR_PlayerDrawn;
	protected float f_BR_PlayerDrawnX;
	protected float f_BR_PlayerDrawnY;
	protected float f_BR_PlayerDrawnSin;
	protected float f_BR_PlayerDrawnCos;
	protected float f_BR_PlayerArrowScale = 1;

	// What the canvas shows; it is only redrawn when the zones or the map view change
	protected bool b_BR_ZonesDirty = true;
	protected vector v_BR_DrawnCurrentCenter;
	protected float f_BR_DrawnCurrentRadius;
	protected vector v_BR_DrawnNextCenter;
	protected float f_BR_DrawnNextRadius;
	protected float f_BR_DrawnMapScale;
	protected vector v_BR_DrawnMapPos;
	protected float f_BR_DrawnScreenX;
	protected float f_BR_DrawnScreenY;
	protected float f_BR_DrawnScreenW;
	protected float f_BR_DrawnScreenH;

	override Widget Init()
	{
		Widget root = super.Init();

		if (m_MapWidgetInstance)
			m_BR_ZoneCanvas = CanvasWidget.Cast(g_Game.GetWorkspace().CreateWidgets(BR_MAP_ZONES_LAYOUT, m_MapWidgetInstance));

		if (!m_BR_ZoneCanvas)
			Error("[MapMenu] zone canvas not created");

		if (m_MapWidgetInstance)
			m_BR_PlayerCanvas = CanvasWidget.Cast(g_Game.GetWorkspace().CreateWidgets(BR_MAP_PLAYER_LAYOUT, m_MapWidgetInstance));

		if (!m_BR_PlayerCanvas)
			Error("[MapMenu] player canvas not created");

		// The canvas draws in screen pixels, the arrow size is given in 1/1080 of the screen height
		int screen_w, screen_h;
		GetScreenSize(screen_w, screen_h);
		if (screen_h > 0)
			f_BR_PlayerArrowScale = screen_h / 1080.0;

		b_BR_ZonesDirty = true;
		b_BR_PlayerDrawn = false;
		return root;
	}

	override void Update(float timeslice)
	{
		super.Update(timeslice);

		BR_UpdateZoneCanvas();
		BR_UpdatePlayerCanvas();
	}

	//! The BR arrow replaces the vanilla GPS marker, so the vanilla circle and arrow stay hidden
	override protected void SetGPSMarkerVisibility(bool pState)
	{
		super.SetGPSMarkerVisibility(false);
	}

	override protected void SetGPSDirectionVisibility(bool pState)
	{
		super.SetGPSDirectionVisibility(false);
	}

	protected void BR_UpdatePlayerCanvas()
	{
		if (!m_BR_PlayerCanvas || !m_MapWidgetInstance)
			return;

		PlayerBase player = PlayerBase.Cast(g_Game.GetPlayer());
		if (!player)
		{
			BR_ClearPlayerCanvas();
			return;
		}

		float screen_x, screen_y, screen_w, screen_h;
		m_MapWidgetInstance.GetScreenPos(screen_x, screen_y);
		m_MapWidgetInstance.GetScreenSize(screen_w, screen_h);

		// Centre in canvas space; the canvas covers the map widget
		vector player_screen = m_MapWidgetInstance.MapToScreen(player.GetPosition());
		float cx = player_screen[0] - screen_x;
		float cy = player_screen[1] - screen_y;

		// The player is out of the visible map: nothing to draw over the map frame
		if (cx < 0 || cy < 0 || cx > screen_w || cy > screen_h)
		{
			BR_ClearPlayerCanvas();
			return;
		}

		// Heading of the body on the ground plane: sine along X (east), cosine along Z (north), 0 = north, clockwise
		vector dir = player.GetDirection();
		float sin_h = 0;
		float cos_h = 1;
		float len = Math.Sqrt(dir[0] * dir[0] + dir[2] * dir[2]);
		if (len > 0.0001)
		{
			sin_h = dir[0] / len;
			cos_h = dir[2] / len;
		}

		if (b_BR_PlayerDrawn && Math.AbsFloat(cx - f_BR_PlayerDrawnX) < BR_PLAYER_MOVE_THRESHOLD_PX && Math.AbsFloat(cy - f_BR_PlayerDrawnY) < BR_PLAYER_MOVE_THRESHOLD_PX)
		{
			if (Math.AbsFloat(sin_h - f_BR_PlayerDrawnSin) < BR_PLAYER_TURN_THRESHOLD && Math.AbsFloat(cos_h - f_BR_PlayerDrawnCos) < BR_PLAYER_TURN_THRESHOLD)
				return;
		}

		b_BR_PlayerDrawn = true;
		f_BR_PlayerDrawnX = cx;
		f_BR_PlayerDrawnY = cy;
		f_BR_PlayerDrawnSin = sin_h;
		f_BR_PlayerDrawnCos = cos_h;

		m_BR_PlayerCanvas.Clear();
		BR_DrawPlayerArrow(cx, cy, sin_h, cos_h);
	}

	protected void BR_ClearPlayerCanvas()
	{
		if (!b_BR_PlayerDrawn)
			return;

		b_BR_PlayerDrawn = false;
		m_BR_PlayerCanvas.Clear();
	}

	/**
	 * Draws a filled arrow head centred on (cx, cy) in canvas space.
	 * Local shape, y towards the tail: tip (0, -h/2), tail corners (+-w/2, h/2), notch (0, h/4).
	 * The two halves are filled with a fan of lines from the tip to the tail edges, then outlined.
	 */
	protected void BR_DrawPlayerArrow(float cx, float cy, float sin_h, float cos_h)
	{
		float half_h = BR_PLAYER_ARROW_HEIGHT * f_BR_PlayerArrowScale * 0.5;
		float half_w = half_h * 0.75;
		float notch = half_h * 0.5;

		// Screen axes of the arrow: forward points where the body faces (north is up), right is forward turned 90 degrees clockwise
		float fwd_x = sin_h;
		float fwd_y = -cos_h;
		float right_x = cos_h;
		float right_y = sin_h;

		float tip_x = cx + fwd_x * half_h;
		float tip_y = cy + fwd_y * half_h;
		float left_x = cx - right_x * half_w - fwd_x * half_h;
		float left_y = cy - right_y * half_w - fwd_y * half_h;
		float right_tail_x = cx + right_x * half_w - fwd_x * half_h;
		float right_tail_y = cy + right_y * half_w - fwd_y * half_h;
		float notch_x = cx - fwd_x * notch;
		float notch_y = cy - fwd_y * notch;

		// About one line per 1.5 px of tail edge; the 2 px line width closes the gaps between them
		float edge = Math.Sqrt(half_w * half_w + (half_h - notch) * (half_h - notch));
		int steps = Math.Max(4, Math.Ceil(edge / 1.5));
		float step_t = 1.0 / steps;
		float fill_width = 2 * f_BR_PlayerArrowScale;
		for (int i = 0; i <= steps; i++)
		{
			float t = i * step_t;
			float lx = left_x + (notch_x - left_x) * t;
			float ly = left_y + (notch_y - left_y) * t;
			float rx = right_tail_x + (notch_x - right_tail_x) * t;
			float ry = right_tail_y + (notch_y - right_tail_y) * t;
			m_BR_PlayerCanvas.DrawLine(tip_x, tip_y, lx, ly, fill_width, BR_PLAYER_ARROW_FILL);
			m_BR_PlayerCanvas.DrawLine(tip_x, tip_y, rx, ry, fill_width, BR_PLAYER_ARROW_FILL);
		}

		// Dark outline over the edges of the fill
		float border_width = BR_PLAYER_ARROW_BORDER_WIDTH * f_BR_PlayerArrowScale;
		m_BR_PlayerCanvas.DrawLine(tip_x, tip_y, left_x, left_y, border_width, BR_PLAYER_ARROW_BORDER);
		m_BR_PlayerCanvas.DrawLine(left_x, left_y, notch_x, notch_y, border_width, BR_PLAYER_ARROW_BORDER);
		m_BR_PlayerCanvas.DrawLine(notch_x, notch_y, right_tail_x, right_tail_y, border_width, BR_PLAYER_ARROW_BORDER);
		m_BR_PlayerCanvas.DrawLine(right_tail_x, right_tail_y, tip_x, tip_y, border_width, BR_PLAYER_ARROW_BORDER);
	}

	protected void BR_UpdateZoneCanvas()
	{
		if (!m_BR_ZoneCanvas || !m_MapWidgetInstance)
			return;

		BattleRoyaleClient br_client = BattleRoyaleClient.Cast(GetBR());
		if (!br_client)
			return;

		vector current_center = vector.Zero;
		float current_radius = 0;
		BattleRoyalePlayArea current_area = br_client.GetPlayArea();
		if (current_area)
		{
			current_center = current_area.GetCenter();
			current_radius = current_area.GetRadius();
		}

		vector next_center = vector.Zero;
		float next_radius = 0;
		BattleRoyalePlayArea next_area = br_client.GetNextArea();
		if (next_area)
		{
			next_center = next_area.GetCenter();
			next_radius = next_area.GetRadius();
		}

		float scale = m_MapWidgetInstance.GetScale();
		vector map_pos = m_MapWidgetInstance.GetMapPos();
		float screen_x, screen_y, screen_w, screen_h;
		m_MapWidgetInstance.GetScreenPos(screen_x, screen_y);
		m_MapWidgetInstance.GetScreenSize(screen_w, screen_h);

		if (!b_BR_ZonesDirty && current_center == v_BR_DrawnCurrentCenter && current_radius == f_BR_DrawnCurrentRadius && next_center == v_BR_DrawnNextCenter && next_radius == f_BR_DrawnNextRadius)
		{
			if (scale == f_BR_DrawnMapScale && map_pos == v_BR_DrawnMapPos && screen_x == f_BR_DrawnScreenX && screen_y == f_BR_DrawnScreenY && screen_w == f_BR_DrawnScreenW && screen_h == f_BR_DrawnScreenH)
				return;
		}

		b_BR_ZonesDirty = false;
		v_BR_DrawnCurrentCenter = current_center;
		f_BR_DrawnCurrentRadius = current_radius;
		v_BR_DrawnNextCenter = next_center;
		f_BR_DrawnNextRadius = next_radius;
		f_BR_DrawnMapScale = scale;
		v_BR_DrawnMapPos = map_pos;
		f_BR_DrawnScreenX = screen_x;
		f_BR_DrawnScreenY = screen_y;
		f_BR_DrawnScreenW = screen_w;
		f_BR_DrawnScreenH = screen_h;

		m_BR_ZoneCanvas.Clear();

		if (current_area && current_radius > 0)
			BattleRoyaleMapCanvas.DrawWorldOval(m_BR_ZoneCanvas, m_MapWidgetInstance, current_center[0], current_center[2], current_radius, current_radius, BR_ZONE_COLOR_LOCKED);

		if (next_area && next_radius > 0)
		{
			int next_color = BR_ZONE_COLOR_OPEN;
			if (current_area && current_center == next_center && current_radius == next_radius)
				next_color = BR_ZONE_COLOR_LOCKED;

			BattleRoyaleMapCanvas.DrawWorldOval(m_BR_ZoneCanvas, m_MapWidgetInstance, next_center[0], next_center[2], next_radius, next_radius, next_color);
		}
	}
}
#endif

#ifndef SERVER
//! Draws the zone in force (red) and the announced zone (yellow) on the 2D map.
//! Once the announced zone locks it is the zone in force and turns red.
//! It also draws the player's own marker, a filled arrow of a fixed screen size that points where the body faces;
//! it replaces the vanilla GPS circle and arrow, whose size follows the map zoom.
modded class MapMenu
{
	protected const string BR_MAP_ZONES_LAYOUT = "Vigrid-BattleRoyale/GUI/layouts/map/royale_map_zones.layout";
	protected const string BR_MAP_PLAYER_LAYOUT = "Vigrid-BattleRoyale/GUI/layouts/map/royale_map_player.layout";
	protected const string BR_MAP_PLANE_LAYOUT = "Vigrid-BattleRoyale/GUI/layouts/map/royale_map_plane.layout";

	//! Arrow height in 1/1080 of the screen height, the unit of the exact layout sizes
	protected const float BR_PLAYER_ARROW_HEIGHT = 28;
	protected const int BR_PLAYER_ARROW_FILL = 0xFFFFFFFF;
	protected const int BR_PLAYER_ARROW_BORDER = 0xFF141414;
	protected const float BR_PLAYER_ARROW_BORDER_WIDTH = 3;
	//! Drop-plane marker and dashed flight path shown during an active flight
	protected const float BR_PLANE_ICON_SIZE = 40.0;
	protected const int BR_PLANE_ICON_COLOR = 0xFFFFFFFF;
	protected const float BR_PLANE_HEADING_THRESHOLD_DEG = 0.5;
	protected const int BR_FLIGHT_LINE_COLOR = 0xE6FFFFFF;
	protected const int BR_FLIGHT_LINE_BORDER = 0x90000000;
	protected const float BR_FLIGHT_LINE_WIDTH = 2.0;
	protected const float BR_FLIGHT_DASH_PX = 14.0;
	protected const float BR_FLIGHT_GAP_PX = 9.0;
	//! The arrow is only redrawn when its centre moved this far in pixels or its direction changed this much (sine of the angle)
	protected const float BR_PLAYER_MOVE_THRESHOLD_PX = 0.25;
	protected const float BR_PLAYER_TURN_THRESHOLD = 0.005;

	protected CanvasWidget m_BR_ZoneCanvas;
	protected CanvasWidget m_BR_PlayerCanvas;
	protected ImageWidget m_BR_PlaneIcon;

	// What the player canvas shows
	protected bool b_BR_PlayerDrawn;
	protected float f_BR_PlayerDrawnX;
	protected float f_BR_PlayerDrawnY;
	protected float f_BR_PlayerDrawnSin;
	protected float f_BR_PlayerDrawnCos;
	// What the plane icon widget shows
	protected bool b_BR_PlaneIconVisible;
	protected bool b_BR_PlanePosSet;
	protected float f_BR_PlaneDrawnX;
	protected float f_BR_PlaneDrawnY;
	protected float f_BR_PlaneIconSize;
	protected bool b_BR_PlaneHeadingSet;
	protected float f_BR_PlaneDrawnHeading;
	protected float f_BR_PlayerArrowScale = 1;

	// What the canvas shows; it is only redrawn when the zones, flight line or map view change
	protected bool b_BR_ZonesDirty = true;
	protected vector v_BR_DrawnCurrentCenter;
	protected float f_BR_DrawnCurrentRadius;
	protected vector v_BR_DrawnNextCenter;
	protected float f_BR_DrawnNextRadius;
	protected bool b_BR_DrawnFlightLine;
	protected vector v_BR_DrawnFlightStart;
	protected vector v_BR_DrawnFlightEnd;
	protected float f_BR_DrawnMapScale;
	protected vector v_BR_DrawnMapPos;
	protected float f_BR_DrawnScreenX;
	protected float f_BR_DrawnScreenY;
	protected float f_BR_DrawnScreenW;
	protected float f_BR_DrawnScreenH;
	//! Seconds since the last canvas redraw, up to BattleRoyaleClient.MAP_SHRINK_REFRESH_SECONDS; while the zone closes
	//! the canvas waits for it, and what changed meanwhile is drawn then
	protected float f_BR_ShrinkRedrawWait;

	override Widget Init()
	{
		Widget root = super.Init();

		if (m_MapWidgetInstance)
			m_BR_ZoneCanvas = CanvasWidget.Cast(g_Game.GetWorkspace().CreateWidgets(BR_MAP_ZONES_LAYOUT, m_MapWidgetInstance));

		if (!m_BR_ZoneCanvas)
			Error("[MapMenu] zone canvas not created");

		if (m_MapWidgetInstance)
			m_BR_PlaneIcon = ImageWidget.Cast(g_Game.GetWorkspace().CreateWidgets(BR_MAP_PLANE_LAYOUT, m_MapWidgetInstance));

		if (!m_BR_PlaneIcon)
			Error("[MapMenu] plane icon widget not created");
		else
		{
			m_BR_PlaneIcon.SetColor(BR_PLANE_ICON_COLOR);
			m_BR_PlaneIcon.Show(false);
		}

		if (m_MapWidgetInstance)
			m_BR_PlayerCanvas = CanvasWidget.Cast(g_Game.GetWorkspace().CreateWidgets(BR_MAP_PLAYER_LAYOUT, m_MapWidgetInstance));

		if (!m_BR_PlayerCanvas)
			Error("[MapMenu] player canvas not created");

		BR_UpdatePlayerArrowScale();

		b_BR_ZonesDirty = true;
		b_BR_DrawnFlightLine = false;
		f_BR_ShrinkRedrawWait = BattleRoyaleClient.MAP_SHRINK_REFRESH_SECONDS;
		b_BR_PlayerDrawn = false;
		b_BR_PlaneIconVisible = false;
		b_BR_PlanePosSet = false;
		f_BR_PlaneIconSize = -1;
		b_BR_PlaneHeadingSet = false;
		return root;
	}

	override void Update(float timeslice)
	{
		super.Update(timeslice);

		BR_UpdateMapCompass();
		BR_UpdateZoneCanvas(timeslice);
		BR_UpdatePlayerCanvas();
		BR_UpdatePlaneIcon();
	}

	//! Vanilla turns the map's compass with the body (GetYawPitchRoll, P:\scripts\5_mission\gui\mapmenu.c:249-258), which
	//! lags the camera when turning on the spot: it is set again here with the heading of the HUD compass
	protected void BR_UpdateMapCompass()
	{
		if (!m_HasCompass || !m_ToolsCompassArrow || !m_ToolsCompassAzimuth)
			return;

		int degrees = BattleRoyaleCompass.HeadingDegrees(BattleRoyaleCompass.CameraHeading());
		m_ToolsCompassArrow.SetRotation(0, 0, degrees);
		m_ToolsCompassAzimuth.SetText(string.Format("%1°", degrees));
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

	//! True while the drop-plane route and icon should be drawn on the map
	protected bool BR_IsPlaneFlightVisible()
	{
		if (!BattleRoyalePlaneState.IsFlightVisible())
			return false;
		if (BattleRoyalePlaneState.IsActive())
			return true;

		BattleRoyaleRPC br_rpc = BattleRoyaleRPC.GetInstance();
		return br_rpc && br_rpc.match_phase == EBRMatchPhase.AIRDROP;
	}

	protected void BR_UpdatePlayerCanvas()
	{
		if (!m_BR_PlayerCanvas || !m_MapWidgetInstance)
			return;

		// After a screen size change with the map open, markers drawn at the old size go and are drawn again below
		if (BR_UpdatePlayerArrowScale())
			BR_ClearPlayerCanvas();

		if (!g_Game)
		{
			BR_ClearPlayerCanvas();
			return;
		}

		// While aboard the plane the plane icon replaces the 1 Hz stepped body arrow
		if (BattleRoyalePlaneState.IsActive())
		{
			BR_ClearPlayerCanvas();
			return;
		}

		PlayerBase player = PlayerBase.Cast(g_Game.GetPlayer());
		if (!player)
		{
			BR_ClearPlayerCanvas();
			return;
		}

		float screen_x, screen_y, screen_w, screen_h;
		m_MapWidgetInstance.GetScreenPos(screen_x, screen_y);
		m_MapWidgetInstance.GetScreenSize(screen_w, screen_h);

		vector player_screen = m_MapWidgetInstance.MapToScreen(player.GetPosition());
		float cx = player_screen[0] - screen_x;
		float cy = player_screen[1] - screen_y;
		if (cx < 0 || cy < 0 || cx > screen_w || cy > screen_h)
		{
			BR_ClearPlayerCanvas();
			return;
		}

		// Heading of the view, as the HUD compass shows it: sine along X (east), cosine along Z (north), 0 = north, clockwise
		float heading_rad = BattleRoyaleCompass.CameraHeading() * Math.DEG2RAD;
		float sin_h = Math.Sin(heading_rad);
		float cos_h = Math.Cos(heading_rad);

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

	protected void BR_SetPlaneIconVisible(bool visible)
	{
		if (!m_BR_PlaneIcon || b_BR_PlaneIconVisible == visible)
			return;

		b_BR_PlaneIconVisible = visible;
		m_BR_PlaneIcon.Show(visible);
	}

	//! Positions, sizes and rotates the drop-plane ImageWidget on the full map, hiding it when no flight is active
	//! or when the plane falls outside the map rectangle
	protected void BR_UpdatePlaneIcon()
	{
		if (!m_BR_PlaneIcon || !m_MapWidgetInstance || !g_Game || !BR_IsPlaneFlightVisible())
		{
			BR_SetPlaneIconVisible(false);
			return;
		}

		float screen_x, screen_y, screen_w, screen_h;
		m_MapWidgetInstance.GetScreenPos(screen_x, screen_y);
		m_MapWidgetInstance.GetScreenSize(screen_w, screen_h);

		vector plane_pos;
		vector plane_dir;
		float plane_heading;
		BattleRoyalePlaneState.ComputePose(plane_pos, plane_dir, plane_heading);
		vector plane_screen = m_MapWidgetInstance.MapToScreen(plane_pos);
		float px = plane_screen[0] - screen_x;
		float py = plane_screen[1] - screen_y;
		if (px < 0 || py < 0 || px > screen_w || py > screen_h)
		{
			BR_SetPlaneIconVisible(false);
			return;
		}

		float scale = f_BR_PlayerArrowScale;
		if (scale <= 0)
			scale = 1.0;

		float icon_size = BR_PLANE_ICON_SIZE * scale;
		bool size_changed = (icon_size != f_BR_PlaneIconSize);
		if (size_changed)
		{
			f_BR_PlaneIconSize = icon_size;
			m_BR_PlaneIcon.SetSize(icon_size, icon_size);
		}

		if (!b_BR_PlanePosSet || size_changed || Math.AbsFloat(px - f_BR_PlaneDrawnX) >= BR_PLAYER_MOVE_THRESHOLD_PX || Math.AbsFloat(py - f_BR_PlaneDrawnY) >= BR_PLAYER_MOVE_THRESHOLD_PX)
		{
			b_BR_PlanePosSet = true;
			f_BR_PlaneDrawnX = px;
			f_BR_PlaneDrawnY = py;
			m_BR_PlaneIcon.SetPos(px - icon_size * 0.5, py - icon_size * 0.5);
		}

		float heading = Math.NormalizeAngle(plane_heading);
		if (!b_BR_PlaneHeadingSet || Math.AbsFloat(Math.DiffAngle(heading, f_BR_PlaneDrawnHeading)) >= BR_PLANE_HEADING_THRESHOLD_DEG)
		{
			b_BR_PlaneHeadingSet = true;
			f_BR_PlaneDrawnHeading = heading;
			m_BR_PlaneIcon.SetRotation(0, 0, heading);
		}

		BR_SetPlaneIconVisible(true);
	}

	//! The canvas draws in screen pixels and the arrow size is given in 1/1080 of the screen height, so the scale follows
	//! the screen height. Read on every update while the map is open; true when it changed
	protected bool BR_UpdatePlayerArrowScale()
	{
		int screen_w, screen_h;
		GetScreenSize(screen_w, screen_h);
		if (screen_h <= 0)
			return false;

		float scale = screen_h / 1080.0;
		if (scale == f_BR_PlayerArrowScale)
			return false;

		f_BR_PlayerArrowScale = scale;
		return true;
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

	protected void BR_UpdateZoneCanvas(float timeslice)
	{
		if (!m_BR_ZoneCanvas || !m_MapWidgetInstance)
			return;

		if (f_BR_ShrinkRedrawWait < BattleRoyaleClient.MAP_SHRINK_REFRESH_SECONDS)
			f_BR_ShrinkRedrawWait += timeslice;

		BattleRoyaleClient br_client = BattleRoyaleClient.Cast(GetBR());
		if (!br_client)
			return;

		vector current_center = vector.Zero;
		float current_radius = 0;
		// While the zone closes this area changes at most 10 times a second
		BattleRoyalePlayArea current_area = br_client.GetMapPlayArea();
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

		bool flight_line = BR_IsPlaneFlightVisible();
		vector flight_start = vector.Zero;
		vector flight_end = vector.Zero;
		if (flight_line && !BattleRoyalePlaneState.GetFlightRoute(flight_start, flight_end))
			flight_line = false;

		float scale = m_MapWidgetInstance.GetScale();
		vector map_pos = m_MapWidgetInstance.GetMapPos();
		float screen_x, screen_y, screen_w, screen_h;
		m_MapWidgetInstance.GetScreenPos(screen_x, screen_y);
		m_MapWidgetInstance.GetScreenSize(screen_w, screen_h);

		if (!b_BR_ZonesDirty && current_center == v_BR_DrawnCurrentCenter && current_radius == f_BR_DrawnCurrentRadius && next_center == v_BR_DrawnNextCenter && next_radius == f_BR_DrawnNextRadius)
		{
			if (flight_line == b_BR_DrawnFlightLine && flight_start == v_BR_DrawnFlightStart && flight_end == v_BR_DrawnFlightEnd)
			{
				if (scale == f_BR_DrawnMapScale && map_pos == v_BR_DrawnMapPos && screen_x == f_BR_DrawnScreenX && screen_y == f_BR_DrawnScreenY && screen_w == f_BR_DrawnScreenW && screen_h == f_BR_DrawnScreenH)
					return;
			}
		}

		// While the zone closes the circle changes and the player pans and zooms: the canvas follows 10 times a second at
		// most. A redraw held back keeps the drawn values, so the next allowed frame still sees the change
		if (br_client.IsMapZoneShrinking() && f_BR_ShrinkRedrawWait < BattleRoyaleClient.MAP_SHRINK_REFRESH_SECONDS)
			return;

		f_BR_ShrinkRedrawWait = 0;
		b_BR_ZonesDirty = false;
		v_BR_DrawnCurrentCenter = current_center;
		f_BR_DrawnCurrentRadius = current_radius;
		v_BR_DrawnNextCenter = next_center;
		f_BR_DrawnNextRadius = next_radius;
		b_BR_DrawnFlightLine = flight_line;
		v_BR_DrawnFlightStart = flight_start;
		v_BR_DrawnFlightEnd = flight_end;
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

		if (flight_line)
		{
			float line_scale = f_BR_PlayerArrowScale;
			if (line_scale <= 0)
				line_scale = 1.0;

			BattleRoyaleMapCanvas.DrawWorldFlightLine(m_BR_ZoneCanvas, m_MapWidgetInstance, flight_start, flight_end, BR_FLIGHT_LINE_COLOR, BR_FLIGHT_LINE_BORDER, BR_FLIGHT_LINE_WIDTH * line_scale, BR_FLIGHT_DASH_PX * line_scale, BR_FLIGHT_GAP_PX * line_scale, screen_w, screen_h);
		}
	}
}
#endif

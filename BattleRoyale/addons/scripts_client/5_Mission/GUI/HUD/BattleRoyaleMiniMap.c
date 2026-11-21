#ifndef SERVER
//! Minimap panel of the HUD: north up, centred on the player, with the zone outlines drawn on a canvas over the map.
//! The map only moves when its centre moved by half a pixel or more, and the canvas is only redrawn when the map
//! view, the panel or the zones change; while the zone closes, 10 times a second at most.
class BattleRoyaleMiniMap
{
	static const int ZOOM_LEVEL_COUNT = 4;
	static const int ZOOM_LEVEL_DEFAULT = 1;

	//! The arrow icon (dayz_gui icon_arrow) points down at rest; this turns it to point up at heading 0
	protected static const float ARROW_ICON_OFFSET = 180;
	protected static const float MOVE_THRESHOLD_PX = 0.5;
	protected static const float HEADING_THRESHOLD_DEG = 1;
	protected static const float ZONE_LINE_WIDTH = 2;
	protected static const float BORDER_LINE_WIDTH = 2;
	protected static const int BORDER_COLOR = 0xB4000000;
	protected static const float PLANE_ICON_HALF_SIZE = 10.5;
	protected static const int PLANE_ICON_FILL = 0xFFFFFFFF;
	protected static const int PLANE_ICON_BORDER = 0xFF141414;
	protected static const int FLIGHT_LINE_COLOR = 0xE6FFFFFF;
	protected static const int FLIGHT_LINE_BORDER = 0x90000000;
	protected static const float FLIGHT_LINE_WIDTH = 2.0;
	protected static const float FLIGHT_DASH_PX = 10.0;
	protected static const float FLIGHT_GAP_PX = 7.0;
	protected static const float STATS_INTERVAL = 10;

	protected Widget m_Panel;
	protected MapWidget m_Map;
	protected CanvasWidget m_Canvas;
	protected ImageWidget m_Arrow;

	protected bool m_Visible;
	protected int m_ZoomLevel;
	protected bool m_ArrowVisibleInit;
	protected bool m_ArrowVisible;

	protected bool m_ViewDirty;  //!< scale or panel changed, the map is set up again
	protected bool m_CanvasDirty;
	//! Seconds since the last canvas redraw, up to BattleRoyaleClient.MAP_SHRINK_REFRESH_SECONDS; while the zone closes
	//! the canvas waits for it, the redraw stays pending
	protected float m_ShrinkRedrawWait;
	protected bool m_HasMapCenter;
	protected vector m_MapCenter;  //!< last centre given to the map
	protected float m_MetersPerPixel;
	protected bool m_HasHeading;
	protected float m_LastHeading;

	protected float m_PanelX;
	protected float m_PanelY;
	protected float m_PanelW;
	protected float m_PanelH;

	// Zones on the canvas; a radius of 0 means no zone
	protected vector m_DrawnLockedCenter;
	protected float m_DrawnLockedRadius;
	protected vector m_DrawnOpenCenter;
	protected float m_DrawnOpenRadius;

	// Active drop-plane flight line and plane icon drawn on the canvas
	protected bool m_DrawnFlightVisible;
	protected vector m_DrawnFlightStart;
	protected vector m_DrawnFlightEnd;
	protected vector m_DrawnPlanePos;
	protected float m_DrawnPlaneHeading;
	protected bool m_HasDrawnPlaneScreen;
	protected bool m_DrawnPlaneOnPanel;
	protected float m_DrawnPlaneScreenX;
	protected float m_DrawnPlaneScreenY;

	// Counters logged at the trace level to measure the panel
	protected float m_StatsTime;
	protected int m_StatsFrames;
	protected int m_StatsMapMoves;
	protected int m_StatsRedraws;
	protected int m_StatsSegments;

	void BattleRoyaleMiniMap(Widget root)
	{
		if (root)
		{
			m_Panel = root.FindAnyWidget("MiniMapPanel");
			m_Map = MapWidget.Cast(root.FindAnyWidget("MiniMap"));
			m_Canvas = CanvasWidget.Cast(root.FindAnyWidget("MiniMapCanvas"));
			m_Arrow = ImageWidget.Cast(root.FindAnyWidget("MiniMapPlayer"));
		}

		if (!m_Panel || !m_Map || !m_Canvas || !m_Arrow)
			Error("[BattleRoyaleMiniMap] minimap widgets not found in the HUD layout");

		m_ZoomLevel = ZOOM_LEVEL_DEFAULT;
		// Hidden until the player shows it with its key
		SetVisible(false);
	}

	bool IsVisible()
	{
		return m_Visible;
	}

	//! Left edge of the minimap, in screen pixels, while it shows; -1 while hidden
	float GetScreenLeft()
	{
		if (!m_Visible || !m_Panel || !m_Panel.IsVisibleHierarchy())
			return -1;

		float x, y;
		m_Panel.GetScreenPos(x, y);
		return x;
	}

	void SetVisible(bool visible)
	{
		m_Visible = visible;
		if (m_Panel)
			m_Panel.Show(visible);

		// Nothing is updated while hidden, so everything is applied again when shown
		m_ViewDirty = true;
		m_CanvasDirty = true;
		m_HasMapCenter = false;
		m_HasHeading = false;
		m_ArrowVisibleInit = false;
		m_DrawnFlightVisible = false;
		m_HasDrawnPlaneScreen = false;
		m_DrawnPlaneOnPanel = false;
		m_ShrinkRedrawWait = BattleRoyaleClient.MAP_SHRINK_REFRESH_SECONDS;
	}

	void ToggleVisible()
	{
		SetVisible(!m_Visible);
	}

	//! Zooms out one level, and back to the closest level after the farthest one
	void CycleZoom()
	{
		m_ZoomLevel++;
		if (m_ZoomLevel >= ZOOM_LEVEL_COUNT)
			m_ZoomLevel = 0;

		m_ViewDirty = true;
	}

	int GetZoomLevel()
	{
		return m_ZoomLevel;
	}

	//! MapWidget scale of each zoom level; a smaller scale shows less ground.
	//! Measured on the 200 px panel: scale 0.1 spans about 60 m, so these span about 150 m, 300 m, 600 m and 1.2 km
	static float GetZoomScale(int level)
	{
		switch (level)
		{
			case 0:
				return 0.25;
			case 1:
				return 0.5;
			case 2:
				return 1.0;
			case 3:
				return 2.0;
		}

		return 0.5;
	}

	/**
	 * Called once per frame by the client.
	 *
	 * @param timeslice    Frame time in seconds
	 * @param locked_area  Zone in force, drawn red; null when there is none
	 * @param open_area    Announced zone until it locks, drawn yellow; null when there is none
	 * @param shrinking    The zone closes: the canvas redraws at most every BattleRoyaleClient.MAP_SHRINK_REFRESH_SECONDS
	 */
	void Update(float timeslice, BattleRoyalePlayArea locked_area, BattleRoyalePlayArea open_area, bool shrinking)
	{
		if (!m_Visible || !m_Map || !m_Canvas)
			return;

		if (m_ShrinkRedrawWait < BattleRoyaleClient.MAP_SHRINK_REFRESH_SECONDS)
			m_ShrinkRedrawWait += timeslice;

		bool trace = BattleRoyaleUtils.CheckLogLevel(BattleRoyaleUtils.TRACE);
		if (trace)
			m_StatsFrames++;

		// The panel changes with the resolution and the interface size
		float panel_x, panel_y, panel_w, panel_h;
		m_Map.GetScreenPos(panel_x, panel_y);
		m_Map.GetScreenSize(panel_w, panel_h);
		if (panel_w <= 0 || panel_h <= 0)
			return;

		if (panel_x != m_PanelX || panel_y != m_PanelY || panel_w != m_PanelW || panel_h != m_PanelH)
		{
			m_PanelX = panel_x;
			m_PanelY = panel_y;
			m_PanelW = panel_w;
			m_PanelH = panel_h;
			m_ViewDirty = true;
		}

		vector center = GetViewCenter();

		if (m_ViewDirty)
		{
			m_ViewDirty = false;
			m_Map.SetScale(GetZoomScale(m_ZoomLevel));
			m_Map.SetMapPos(center);
			m_MapCenter = center;
			m_HasMapCenter = true;
			m_CanvasDirty = true;
			UpdateMetersPerPixel();
			if (trace)
				m_StatsMapMoves++;
		}
		else if (!m_HasMapCenter || HasCenterMoved(center))
		{
			m_Map.SetMapPos(center);
			m_MapCenter = center;
			m_HasMapCenter = true;
			m_CanvasDirty = true;
			if (trace)
				m_StatsMapMoves++;
		}

		bool in_plane = BattleRoyalePlaneState.IsActive();
		SetArrowVisible(!in_plane);
		if (!in_plane)
			UpdateHeading();

		vector locked_center = vector.Zero;
		float locked_radius = 0;
		if (locked_area)
		{
			locked_center = locked_area.GetCenter();
			locked_radius = locked_area.GetRadius();
		}

		vector open_center = vector.Zero;
		float open_radius = 0;
		if (open_area)
		{
			open_center = open_area.GetCenter();
			open_radius = open_area.GetRadius();
		}

		if (locked_center != m_DrawnLockedCenter || locked_radius != m_DrawnLockedRadius || open_center != m_DrawnOpenCenter || open_radius != m_DrawnOpenRadius)
		{
			m_DrawnLockedCenter = locked_center;
			m_DrawnLockedRadius = locked_radius;
			m_DrawnOpenCenter = open_center;
			m_DrawnOpenRadius = open_radius;
			m_CanvasDirty = true;
		}

		bool flight_visible = IsPlaneFlightVisible();
		vector flight_start = vector.Zero;
		vector flight_end = vector.Zero;
		vector plane_pos = vector.Zero;
		float plane_heading = 0;
		if (flight_visible)
		{
			if (!BattleRoyalePlaneState.GetFlightRoute(flight_start, flight_end))
			{
				flight_visible = false;
			}
			else
			{
				vector plane_dir;
				BattleRoyalePlaneState.ComputePose(plane_pos, plane_dir, plane_heading);
				plane_heading = Math.NormalizeAngle(plane_heading);
			}
		}

		if (flight_visible != m_DrawnFlightVisible)
		{
			m_DrawnFlightVisible = flight_visible;
			m_DrawnFlightStart = flight_start;
			m_DrawnFlightEnd = flight_end;
			m_DrawnPlanePos = plane_pos;
			m_DrawnPlaneHeading = plane_heading;
			m_HasDrawnPlaneScreen = false;
			m_CanvasDirty = true;
		}
		else if (flight_visible)
		{
			m_DrawnPlanePos = plane_pos;
			if (flight_start != m_DrawnFlightStart || flight_end != m_DrawnFlightEnd || Math.AbsFloat(Math.DiffAngle(plane_heading, m_DrawnPlaneHeading)) >= HEADING_THRESHOLD_DEG)
			{
				m_DrawnFlightStart = flight_start;
				m_DrawnFlightEnd = flight_end;
				m_DrawnPlaneHeading = plane_heading;
				m_CanvasDirty = true;
			}
			else if (!m_CanvasDirty)
			{
				vector plane_scr = m_Map.MapToScreen(Vector(plane_pos[0], 0, plane_pos[2]));
				float psx = plane_scr[0] - m_PanelX;
				float psy = plane_scr[1] - m_PanelY;
				bool plane_on_panel = (psx >= 0 && psy >= 0 && psx <= m_PanelW && psy <= m_PanelH);
				if (plane_on_panel != m_DrawnPlaneOnPanel)
					m_CanvasDirty = true;
				else if (plane_on_panel && (!m_HasDrawnPlaneScreen || Math.AbsFloat(psx - m_DrawnPlaneScreenX) >= MOVE_THRESHOLD_PX || Math.AbsFloat(psy - m_DrawnPlaneScreenY) >= MOVE_THRESHOLD_PX))
					m_CanvasDirty = true;
			}
		}

		// While the zone closes the circle and the map both move: the canvas follows them 10 times a second at most,
		// and a redraw held back stays pending. The map itself keeps following the player every frame
		if (m_CanvasDirty && (!shrinking || m_ShrinkRedrawWait >= BattleRoyaleClient.MAP_SHRINK_REFRESH_SECONDS))
		{
			m_CanvasDirty = false;
			m_ShrinkRedrawWait = 0;
			int segments = RedrawCanvas();
			if (trace)
			{
				m_StatsRedraws++;
				m_StatsSegments += segments;
			}
		}

		if (trace)
			UpdateStats(timeslice);
	}

	//! True while the drop-plane route and icon should be drawn on the minimap
	protected bool IsPlaneFlightVisible()
	{
		if (!BattleRoyalePlaneState.IsFlightVisible())
			return false;
		if (BattleRoyalePlaneState.IsActive())
			return true;

		BattleRoyaleRPC br_rpc = BattleRoyaleRPC.GetInstance();
		return br_rpc && br_rpc.match_phase == EBRMatchPhase.AIRDROP;
	}

	//! Shows or hides the centre player arrow only when its state changes
	protected void SetArrowVisible(bool visible)
	{
		if (!m_Arrow)
			return;
		if (m_ArrowVisibleInit && m_ArrowVisible == visible)
			return;

		m_ArrowVisibleInit = true;
		m_ArrowVisible = visible;
		m_Arrow.Show(visible);
	}

	//! Interpolated plane position while aboard; otherwise the player while alive, or the camera
	protected vector GetViewCenter()
	{
		if (BattleRoyalePlaneState.IsActive())
		{
			vector plane_pos;
			vector plane_dir;
			float heading_deg;
			BattleRoyalePlaneState.ComputePose(plane_pos, plane_dir, heading_deg);
			return Vector(plane_pos[0], 0, plane_pos[2]);
		}

		vector pos;
		PlayerBase player = PlayerBase.Cast(GetGame().GetPlayer());
		if (player && player.IsAlive())
			pos = player.GetPosition();
		else
			pos = GetGame().GetCurrentCameraPosition();

		return Vector(pos[0], 0, pos[2]);
	}

	protected bool HasCenterMoved(vector center)
	{
		float dx = center[0] - m_MapCenter[0];
		float dz = center[2] - m_MapCenter[2];
		float threshold = MOVE_THRESHOLD_PX * m_MetersPerPixel;

		return (dx * dx + dz * dz) >= (threshold * threshold);
	}

	//! Ground covered by one screen pixel at the current scale, from the panel width
	protected void UpdateMetersPerPixel()
	{
		vector left = m_Map.ScreenToMap(Vector(m_PanelX, m_PanelY, 0));
		vector right = m_Map.ScreenToMap(Vector(m_PanelX + m_PanelW, m_PanelY, 0));
		float span = Math.AbsFloat(right[0] - left[0]);
		m_MetersPerPixel = span / m_PanelW;

		if (BattleRoyaleUtils.CheckLogLevel(BattleRoyaleUtils.TRACE))
			BattleRoyaleUtils.Trace(string.Format("[BattleRoyaleMiniMap] zoom level %1 scale %2: %3 m across %4 px", m_ZoomLevel, GetZoomScale(m_ZoomLevel), span, m_PanelW));
	}

	//! The arrow follows the camera heading, turned only when it changed by a degree or more
	protected void UpdateHeading()
	{
		if (!m_Arrow)
			return;

		float heading = Math.NormalizeAngle(GetGame().GetCurrentCameraDirection().VectorToAngles()[0]);
		if (m_HasHeading && Math.AbsFloat(Math.DiffAngle(heading, m_LastHeading)) < HEADING_THRESHOLD_DEG)
			return;

		m_HasHeading = true;
		m_LastHeading = heading;
		m_Arrow.SetRotation(0, 0, heading + ARROW_ICON_OFFSET);
	}

	//! Draws the border and the zones clipped to the panel; returns the number of zone segments drawn
	protected int RedrawCanvas()
	{
		m_Canvas.Clear();

		float w = m_PanelW;
		float h = m_PanelH;
		m_Canvas.DrawLine(0, 0, w, 0, BORDER_LINE_WIDTH, BORDER_COLOR);
		m_Canvas.DrawLine(w, 0, w, h, BORDER_LINE_WIDTH, BORDER_COLOR);
		m_Canvas.DrawLine(w, h, 0, h, BORDER_LINE_WIDTH, BORDER_COLOR);
		m_Canvas.DrawLine(0, h, 0, 0, BORDER_LINE_WIDTH, BORDER_COLOR);

		int segments = 0;
		if (m_DrawnLockedRadius > 0)
			segments += BattleRoyaleMapCanvas.DrawWorldOval(m_Canvas, m_Map, m_DrawnLockedCenter[0], m_DrawnLockedCenter[2], m_DrawnLockedRadius, m_DrawnLockedRadius, BR_ZONE_COLOR_LOCKED, ZONE_LINE_WIDTH, w, h);

		if (m_DrawnOpenRadius > 0)
			segments += BattleRoyaleMapCanvas.DrawWorldOval(m_Canvas, m_Map, m_DrawnOpenCenter[0], m_DrawnOpenCenter[2], m_DrawnOpenRadius, m_DrawnOpenRadius, BR_ZONE_COLOR_OPEN, ZONE_LINE_WIDTH, w, h);

		if (m_DrawnFlightVisible)
		{
			float scale = h / 200.0;
			if (scale <= 0)
				scale = 1.0;

			BattleRoyaleMapCanvas.DrawWorldFlightLine(m_Canvas, m_Map, m_DrawnFlightStart, m_DrawnFlightEnd, FLIGHT_LINE_COLOR, FLIGHT_LINE_BORDER, FLIGHT_LINE_WIDTH * scale, FLIGHT_DASH_PX * scale, FLIGHT_GAP_PX * scale, w, h);

			vector plane_screen = m_Map.MapToScreen(Vector(m_DrawnPlanePos[0], 0, m_DrawnPlanePos[2]));
			float px = plane_screen[0] - m_PanelX;
			float py = plane_screen[1] - m_PanelY;
			m_HasDrawnPlaneScreen = true;
			m_DrawnPlaneScreenX = px;
			m_DrawnPlaneScreenY = py;
			m_DrawnPlaneOnPanel = (px >= 0 && py >= 0 && px <= w && py <= h);

			if (m_DrawnPlaneOnPanel)
			{
				float heading_rad = m_DrawnPlaneHeading * Math.DEG2RAD;
				BattleRoyaleMapCanvas.DrawPlaneIcon(m_Canvas, px, py, Math.Sin(heading_rad), Math.Cos(heading_rad), scale, PLANE_ICON_HALF_SIZE, PLANE_ICON_FILL, PLANE_ICON_BORDER);
			}
		}
		else
		{
			m_HasDrawnPlaneScreen = false;
			m_DrawnPlaneOnPanel = false;
		}

		return segments;
	}

	//! Logs the counters every STATS_INTERVAL seconds; compare with the panel hidden to measure the map widget itself
	protected void UpdateStats(float timeslice)
	{
		m_StatsTime += timeslice;
		if (m_StatsTime < STATS_INTERVAL)
			return;

		BattleRoyaleUtils.Trace(string.Format("[BattleRoyaleMiniMap] %1 s: %2 frames, %3 map moves, %4 canvas redraws, %5 zone segments, zoom level %6", m_StatsTime, m_StatsFrames, m_StatsMapMoves, m_StatsRedraws, m_StatsSegments, m_ZoomLevel));

		m_StatsTime = 0;
		m_StatsFrames = 0;
		m_StatsMapMoves = 0;
		m_StatsRedraws = 0;
		m_StatsSegments = 0;
	}
}
#endif

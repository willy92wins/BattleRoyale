#ifndef SERVER
//! Minimap panel of the HUD: north up, centred on the player, with the zone outlines drawn on a canvas over the map.
//! The map only moves when its centre moved by half a pixel or more, and the canvas is only redrawn when the map
//! view, the panel or the zones change.
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
	protected static const float STATS_INTERVAL = 10;

	protected Widget m_Panel;
	protected MapWidget m_Map;
	protected CanvasWidget m_Canvas;
	protected ImageWidget m_Arrow;

	protected bool m_Visible;
	protected int m_ZoomLevel;

	protected bool m_ViewDirty;  //!< scale or panel changed, the map is set up again
	protected bool m_CanvasDirty;
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
		SetVisible(true);
	}

	bool IsVisible()
	{
		return m_Visible;
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

	//! MapWidget scale of each zoom level; a smaller scale shows less ground
	static float GetZoomScale(int level)
	{
		switch (level)
		{
			case 0:
				return 0.05;
			case 1:
				return 0.1;
			case 2:
				return 0.2;
			case 3:
				return 0.4;
		}

		return 0.1;
	}

	/**
	 * Called once per frame by the client.
	 *
	 * @param timeslice    Frame time in seconds
	 * @param locked_area  Zone in force, drawn red; null when there is none
	 * @param open_area    Announced zone until it locks, drawn yellow; null when there is none
	 */
	void Update(float timeslice, BattleRoyalePlayArea locked_area, BattleRoyalePlayArea open_area)
	{
		if (!m_Visible || !m_Map || !m_Canvas)
			return;

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

		if (m_CanvasDirty)
		{
			m_CanvasDirty = false;
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

	//! The player while alive, otherwise the camera (spectating, dead or no player yet)
	protected vector GetViewCenter()
	{
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

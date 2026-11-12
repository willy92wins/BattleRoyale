#ifndef SERVER
//! Draws the zone in force (red) and the announced zone (yellow) on the 2D map.
//! Once the announced zone locks it is the zone in force and turns red.
modded class MapMenu
{
	protected CanvasWidget m_BR_ZoneCanvas;

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

		b_BR_ZonesDirty = true;
		return root;
	}

	override void Update(float timeslice)
	{
		super.Update(timeslice);

		BR_UpdateZoneCanvas();
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

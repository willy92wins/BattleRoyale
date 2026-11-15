#ifndef SERVER
//! Client-only wall along the edge of a play area, built from local translucent panels.
//! Only the stretch of the edge near the camera exists, so the cost does not grow with the zone size,
//! and nothing goes over the network.
class BattleRoyaleZoneWall
{
	static const string MODEL_LOCKED = "Vigrid-BattleRoyale/Models/Shapes/zonewall.p3d";  //!< red: the edge of the zone in force
	static const string MODEL_OPEN = "Vigrid-BattleRoyale/Models/Shapes/zonewall_yellow.p3d";  //!< yellow: the edge of a zone that has not locked yet
	static const float PANEL_WIDTH = 20;  //!< width of both models
	static const float SPACING = 20;  //!< metres of edge per panel: the chord between two slot boundaries is a hair under the width
	static const float SINK = 2;  //!< panels go this far below the lowest ground under them, so slopes leave no gap
	static const float VIEW_RANGE = 500;  //!< panels further than this from the camera are not created
	static const float REFRESH_SECONDS = 0.5;
	static const float MOVING_REFRESH_SECONDS = 0.2;  //!< while the zone closes: under 2 m of edge travel per refresh at 9 m/s

	protected string m_Model;
	protected vector m_Center;
	protected float m_Radius;
	protected int m_Count;  //!< number of panel slots around the whole edge
	protected float m_Elapsed;
	protected bool m_ModelMissing;
	protected bool m_Moved;  //!< the circle changed since the panels were placed: the ones kept are placed again

	//! panel slot index -> local object
	protected ref map<int, Object> m_Panels;

	void BattleRoyaleZoneWall(string model)
	{
		m_Model = model;
		m_Panels = new map<int, Object>;
		m_Elapsed = REFRESH_SECONDS;
	}

	void ~BattleRoyaleZoneWall()
	{
		Clear();
	}

	//! area null hides the wall; moving (the zone closes) refreshes it more often
	void Update(float delta, vector camera_pos, BattleRoyalePlayArea area, bool moving = false)
	{
		float refresh_seconds = REFRESH_SECONDS;
		if (moving)
			refresh_seconds = MOVING_REFRESH_SECONDS;

		m_Elapsed += delta;
		if (m_Elapsed < refresh_seconds)
			return;
		m_Elapsed = 0;

		if (!area || area.GetRadius() <= 0 || m_ModelMissing)
		{
			Clear();
			return;
		}

		// A new circle keeps the panels: Refresh moves them onto it and only creates or deletes the difference
		if (area.GetCenter() != m_Center || area.GetRadius() != m_Radius)
		{
			m_Center = area.GetCenter();
			m_Radius = area.GetRadius();
			m_Count = Math.Max(8, Math.Ceil(Math.PI2 * m_Radius / SPACING));
			m_Moved = true;
		}

		Refresh(camera_pos);
	}

	void Clear()
	{
		RemoveAllBut(null);
		m_Radius = 0;
		m_Count = 0;
	}

	protected void Refresh(vector camera_pos)
	{
		float dx = camera_pos[0] - m_Center[0];
		float dz = camera_pos[2] - m_Center[2];
		float distance = Math.Sqrt(dx * dx + dz * dz);

		// The camera is too far from the edge for any panel to be in range
		if (Math.AbsFloat(distance - m_Radius) > VIEW_RANGE)
		{
			RemoveAllBut(null);
			return;
		}

		// Slots within VIEW_RANGE of the camera lie within an angle theta either side of the camera's bearing from the
		// center. From the triangle center-camera-slot: VIEW_RANGE^2 = R^2 + d^2 - 2 R d cos(theta). With the camera
		// near the center of a small zone the whole edge can be in range
		float step = Math.PI2 / m_Count;
		float bearing = Math.Atan2(dz, dx);
		int half_span = m_Count / 2;
		if (distance > 1)
		{
			float cos_theta = (m_Radius * m_Radius + distance * distance - VIEW_RANGE * VIEW_RANGE) / (2 * m_Radius * distance);
			if (cos_theta > -1)
			{
				float theta = Math.Acos(Math.Clamp(cos_theta, -1, 1));
				int arc_span = Math.Ceil(theta / step) + 1;
				if (arc_span < half_span)
					half_span = arc_span;
			}
		}

		int first = Math.Round(bearing / step);
		float range_sq = VIEW_RANGE * VIEW_RANGE;
		set<int> wanted = new set<int>;
		array<int> wanted_order = new array<int>;

		for (int i = -half_span; i <= half_span; i++)
		{
			int index = (first + i) % m_Count;
			if (index < 0)
				index += m_Count;

			float angle = index * step;
			float x = m_Center[0] + m_Radius * Math.Cos(angle);
			float z = m_Center[2] + m_Radius * Math.Sin(angle);
			float px = x - camera_pos[0];
			float pz = z - camera_pos[2];
			if (px * px + pz * pz > range_sq)
				continue;

			// A small zone can bring the same slot twice around the circle
			if (wanted.Find(index) != -1)
				continue;

			wanted.Insert(index);
			wanted_order.Insert(index);
		}

		// Panels of slots no longer wanted are spares: they move to the new slots before any panel is created
		array<Object> spares = new array<Object>;
		array<int> stale = new array<int>;
		foreach (int panel_index, Object panel : m_Panels)
		{
			if (wanted.Find(panel_index) == -1)
			{
				if (panel)
					spares.Insert(panel);
				stale.Insert(panel_index);
			}
		}
		foreach (int stale_index : stale)
		{
			m_Panels.Remove(stale_index);
		}

		bool moved = m_Moved;
		m_Moved = false;
		foreach (int slot : wanted_order)
		{
			float slot_angle = slot * step;
			float slot_x = m_Center[0] + m_Radius * Math.Cos(slot_angle);
			float slot_z = m_Center[2] + m_Radius * Math.Sin(slot_angle);

			Object kept = m_Panels.Get(slot);
			if (kept)
			{
				if (moved)
					PlacePanel(kept, slot_angle, slot_x, slot_z);
				continue;
			}

			Object reused = null;
			if (spares.Count() > 0)
			{
				reused = spares[spares.Count() - 1];
				spares.Remove(spares.Count() - 1);
			}

			Object placed = PlacePanel(reused, slot_angle, slot_x, slot_z);
			if (!placed)
				break;
			m_Panels.Set(slot, placed);
		}

		// Spares nobody took, now out of range
		foreach (Object spare : spares)
		{
			if (spare)
				GetGame().ObjectDelete(spare);
		}
	}

	//! Point of the edge at angle, on the ground
	protected vector EdgePoint(float angle)
	{
		float x = m_Center[0] + m_Radius * Math.Cos(angle);
		float z = m_Center[2] + m_Radius * Math.Sin(angle);
		return Vector(x, GetGame().SurfaceY(x, z), z);
	}

	//! Puts panel on the slot at angle, or creates it there when panel is null; null when it cannot be created
	protected Object PlacePanel(Object panel, float angle, float x, float z)
	{
		// The panel runs from one slot boundary to the next on the ground, so neighbours share their
		// ends and the top edge follows the terrain instead of stepping
		float half_step = Math.PI / m_Count;
		vector left = EdgePoint(angle - half_step);
		vector right = EdgePoint(angle + half_step);

		// Sheared frame: X stretches the panel exactly from left to right (slope included) while Y stays
		// vertical, so the side edges of neighbouring panels coincide whatever their slopes
		vector aside = vector.Direction(left, right) * (1.0 / PANEL_WIDTH);
		vector up = "0 1 0";
		vector dir = Vector(-Math.Cos(angle), 0, -Math.Sin(angle));

		// Ground that bulges between the two ends would show under the panel: sink it that much deeper
		vector middle = (left + right) * 0.5;
		float bulge = Math.Max(0, GetGame().SurfaceY(x, z) - middle[1]);

		vector transform[4];
		transform[0] = aside;
		transform[1] = up;
		transform[2] = dir;
		transform[3] = middle - up * (SINK + bulge);

		Object placed = panel;
		if (!placed)
		{
			placed = GetGame().CreateStaticObjectUsingP3D(m_Model, transform[3], "0 0 0", 1.0, true);
			if (!placed)
			{
				BattleRoyaleUtils.Warn("ZoneWall: cannot create " + m_Model);
				m_ModelMissing = true;
				return null;
			}
		}

		placed.SetTransform(transform);
		return placed;
	}

	//! Deletes every panel whose slot is not in keep (all of them when keep is null)
	protected void RemoveAllBut(set<int> keep)
	{
		array<int> stale = new array<int>;
		foreach (int index, Object panel : m_Panels)
		{
			if (!keep || keep.Find(index) == -1)
			{
				if (panel)
					GetGame().ObjectDelete(panel);
				stale.Insert(index);
			}
		}

		foreach (int key : stale)
		{
			m_Panels.Remove(key);
		}
	}
}
#endif

#ifndef SERVER
//! Client-only wall along the edge of a play area, built from local translucent panels.
//! Only the stretch of the edge near the camera exists, so the cost does not grow with the zone size,
//! and nothing goes over the network.
class BattleRoyaleZoneWall
{
	static const string PANEL_MODEL = "Vigrid-BattleRoyale/Models/Shapes/zonewall.p3d";
	static const float PANEL_WIDTH = 20;  //!< width of zonewall.p3d
	static const float SPACING = 19;  //!< metres between two panels along the edge, a little under the width so curves leave no gap
	static const float SINK = 2;  //!< panels go this far below the lowest ground under them, so slopes leave no gap
	static const float VIEW_RANGE = 500;  //!< panels further than this from the camera are not created
	static const float REFRESH_SECONDS = 0.5;

	protected vector m_Center;
	protected float m_Radius;
	protected int m_Count;  //!< number of panel slots around the whole edge
	protected float m_Elapsed;
	protected bool m_ModelMissing;

	//! panel slot index -> local object
	protected ref map<int, Object> m_Panels;

	void BattleRoyaleZoneWall()
	{
		m_Panels = new map<int, Object>;
		m_Elapsed = REFRESH_SECONDS;
	}

	void ~BattleRoyaleZoneWall()
	{
		Clear();
	}

	//! area null hides the wall
	void Update(float delta, vector camera_pos, BattleRoyalePlayArea area)
	{
		m_Elapsed += delta;
		if (m_Elapsed < REFRESH_SECONDS)
			return;
		m_Elapsed = 0;

		if (!area || area.GetRadius() <= 0 || m_ModelMissing)
		{
			Clear();
			return;
		}

		if (area.GetCenter() != m_Center || area.GetRadius() != m_Radius)
		{
			Clear();
			m_Center = area.GetCenter();
			m_Radius = area.GetRadius();
			m_Count = Math.Max(8, Math.Ceil(Math.PI2 * m_Radius / SPACING));
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

		// Slots within VIEW_RANGE of the camera lie around the camera's bearing from the center
		float step = Math.PI2 / m_Count;
		float bearing = Math.Atan2(dz, dx);
		int half_span = Math.Ceil((VIEW_RANGE / m_Radius) / step) + 1;
		if (half_span * 2 + 1 > m_Count)
			half_span = m_Count / 2;

		int first = Math.Round(bearing / step);
		float range_sq = VIEW_RANGE * VIEW_RANGE;
		set<int> wanted = new set<int>;

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

			wanted.Insert(index);
			if (!m_Panels.Contains(index))
			{
				Object panel = CreatePanel(angle, x, z);
				if (!panel)
					return;
				m_Panels.Insert(index, panel);
			}
		}

		RemoveAllBut(wanted);
	}

	protected Object CreatePanel(float angle, float x, float z)
	{
		// The panel spans its local X axis, so it faces the center to lie along the edge
		vector radial = Vector(Math.Cos(angle), 0, Math.Sin(angle));
		vector tangent = Vector(-radial[2], 0, radial[0]);
		float half = PANEL_WIDTH * 0.5;
		float y = GetGame().SurfaceY(x, z);
		y = Math.Min(y, GetGame().SurfaceY(x + tangent[0] * half, z + tangent[2] * half));
		y = Math.Min(y, GetGame().SurfaceY(x - tangent[0] * half, z - tangent[2] * half));
		vector pos = Vector(x, y - SINK, z);

		Object panel = GetGame().CreateStaticObjectUsingP3D(PANEL_MODEL, pos, "0 0 0", 1.0, true);
		if (!panel)
		{
			BattleRoyaleUtils.Warn("ZoneWall: cannot create " + PANEL_MODEL);
			m_ModelMissing = true;
			return null;
		}

		panel.SetDirection(radial);
		panel.SetPosition(pos);
		return panel;
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

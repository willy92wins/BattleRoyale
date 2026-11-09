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

	protected string m_Model;
	protected vector m_Center;
	protected float m_Radius;
	protected int m_Count;  //!< number of panel slots around the whole edge
	protected float m_Elapsed;
	protected bool m_ModelMissing;

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

	//! Point of the edge at angle, on the ground
	protected vector EdgePoint(float angle)
	{
		float x = m_Center[0] + m_Radius * Math.Cos(angle);
		float z = m_Center[2] + m_Radius * Math.Sin(angle);
		return Vector(x, GetGame().SurfaceY(x, z), z);
	}

	protected Object CreatePanel(float angle, float x, float z)
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

		Object panel = GetGame().CreateStaticObjectUsingP3D(m_Model, transform[3], "0 0 0", 1.0, true);
		if (!panel)
		{
			BattleRoyaleUtils.Warn("ZoneWall: cannot create " + m_Model);
			m_ModelMissing = true;
			return null;
		}

		panel.SetTransform(transform);
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

#ifndef SERVER
//! One panel between two refreshes while the zone closes: its frame on the circle of the last refresh and on the circle
//! of the next one. Up and dir do not change for a slot, so only the X axis and the origin are kept twice
class BattleRoyaleZoneWallGlide
{
	Object m_Panel;
	vector m_Dir;
	vector m_FromAside;
	vector m_FromOrigin;
	vector m_ToAside;
	vector m_ToOrigin;
}

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
	//! While the zone closes the terrain, the slots and the panels are worked out at this rate, and in between every
	//! panel glides each frame to where the next refresh puts it (the circle this many seconds later, or at the end of
	//! the closing when it comes sooner)
	static const float MOVING_REFRESH_SECONDS = 0.2;

	protected string m_Model;
	protected vector m_Center;
	protected float m_Radius;
	protected int m_Count;  //!< number of panel slots around the whole edge
	protected float m_Phase;  //!< angle of slot 0: 0, except after a slot count change while the zone closes
	protected float m_Elapsed;
	protected bool m_ModelMissing;
	protected bool m_Moved;  //!< the circle changed since the panels were placed: the ones kept are placed again
	protected bool m_WasMoving;

	//! Circle of the next refresh while the zone closes; m_HasNext is false when there is none or it is the same circle
	protected bool m_HasNext;
	protected vector m_NextCenter;
	protected float m_NextRadius;
	protected int m_GlideStart;  //!< GetGame().GetTime() of the refresh the panels glide from
	protected float m_GlideSeconds;  //!< how long after m_GlideStart the panels reach the circle of the next refresh

	//! Pool of glides, the first m_GlideCount ones for the panels of the last refresh
	protected ref array<ref BattleRoyaleZoneWallGlide> m_Glides;
	protected int m_GlideCount;

	//! panel slot index -> local object
	protected ref map<int, Object> m_Panels;

	void BattleRoyaleZoneWall(string model)
	{
		m_Model = model;
		m_Panels = new map<int, Object>;
		m_Glides = new array<ref BattleRoyaleZoneWallGlide>;
		m_Elapsed = REFRESH_SECONDS;
	}

	void ~BattleRoyaleZoneWall()
	{
		Clear();
	}

	//! area null hides the wall. moving (the zone closes) refreshes it more often, and next_area, the circle next_seconds
	//! later (at most MOVING_REFRESH_SECONDS), makes the panels glide there frame by frame until the next refresh
	void Update(float delta, vector camera_pos, BattleRoyalePlayArea area, bool moving = false, BattleRoyalePlayArea next_area = null, float next_seconds = 0)
	{
		// A closing that starts or ends refreshes at once: the panels start gliding, or stop on the circle that ends it
		if (moving != m_WasMoving)
		{
			m_WasMoving = moving;
			m_Elapsed = REFRESH_SECONDS;
		}

		float refresh_seconds = REFRESH_SECONDS;
		if (moving)
			refresh_seconds = MOVING_REFRESH_SECONDS;

		// The glide runs on the clock of the circle; whichever clock reaches the refresh first starts the next one
		m_Elapsed += delta;
		float glide_fraction = GetGlideFraction();
		if (m_Elapsed < refresh_seconds && glide_fraction < 1)
		{
			Glide(glide_fraction);
			return;
		}
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
			int count = Math.Max(8, Math.Ceil(Math.PI2 * m_Radius / SPACING));
			if (count != m_Count)
			{
				// While the zone closes the count drops every few metres of radius: the slot nearest the camera keeps
				// its angle, so the panels in view barely shift sideways. Otherwise slot 0 sits at angle 0, as always
				if (moving && m_Count > 0)
					m_Phase = GetNearestSlotAngle(camera_pos);
				else
					m_Phase = 0;
				m_Count = count;
			}
			m_Moved = true;
		}

		// The panels glide to the circle of the next refresh, when it is another one
		m_HasNext = false;
		if (moving && next_area && next_seconds > 0 && (next_area.GetCenter() != m_Center || next_area.GetRadius() != m_Radius))
		{
			m_NextCenter = next_area.GetCenter();
			m_NextRadius = next_area.GetRadius();
			m_HasNext = true;
			m_GlideStart = GetGame().GetTime();
			m_GlideSeconds = Math.Min(next_seconds, MOVING_REFRESH_SECONDS);
		}

		Refresh(camera_pos);
	}

	void Clear()
	{
		RemoveAllBut(null);
		m_Radius = 0;
		m_Count = 0;
		m_HasNext = false;
		m_GlideCount = 0;
	}

	//! Share of the way from the last refresh to the next one while the panels glide, 0 when they do not
	protected float GetGlideFraction()
	{
		if (m_GlideCount == 0)
			return 0;

		float milliseconds = GetGame().GetTime() - m_GlideStart;
		return milliseconds / (m_GlideSeconds * 1000);
	}

	//! Puts every gliding panel at fraction of the way between its two frames. Both frames sit on circles of the same
	//! closing, which moves linearly in time, so the lerp follows it; only the height of the ground in between is guessed
	protected void Glide(float fraction)
	{
		if (m_GlideCount == 0)
			return;

		fraction = Math.Clamp(fraction, 0, 1);
		vector transform[4];
		transform[1] = "0 1 0";
		for (int i = 0; i < m_GlideCount; i++)
		{
			BattleRoyaleZoneWallGlide glide = m_Glides[i];
			if (!glide.m_Panel)
				continue;

			transform[0] = glide.m_FromAside + (glide.m_ToAside - glide.m_FromAside) * fraction;
			transform[2] = glide.m_Dir;
			transform[3] = glide.m_FromOrigin + (glide.m_ToOrigin - glide.m_FromOrigin) * fraction;
			glide.m_Panel.SetTransform(transform);
		}
	}

	//! Angle of the slot nearest the camera on the current slot layout, seen from m_Center
	protected float GetNearestSlotAngle(vector camera_pos)
	{
		float step = Math.PI2 / m_Count;
		float bearing = Math.Atan2(camera_pos[2] - m_Center[2], camera_pos[0] - m_Center[0]);
		return m_Phase + Math.Round((bearing - m_Phase) / step) * step;
	}

	protected void Refresh(vector camera_pos)
	{
		// The glides of the last refresh end here: the panels kept are placed again below
		m_GlideCount = 0;

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

		int first = Math.Round((bearing - m_Phase) / step);
		float range_sq = VIEW_RANGE * VIEW_RANGE;
		set<int> wanted = new set<int>;
		array<int> wanted_order = new array<int>;

		for (int i = -half_span; i <= half_span; i++)
		{
			int index = (first + i) % m_Count;
			if (index < 0)
				index += m_Count;

			float angle = m_Phase + index * step;
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

		// A glide needs the start frame of every panel, so they are all placed again even on an unchanged circle
		bool moved = m_Moved || m_HasNext;
		m_Moved = false;
		foreach (int slot : wanted_order)
		{
			float slot_angle = m_Phase + slot * step;

			Object kept = m_Panels.Get(slot);
			if (kept)
			{
				if (moved)
					PlacePanel(kept, slot_angle);
				continue;
			}

			Object reused = null;
			if (spares.Count() > 0)
			{
				reused = spares[spares.Count() - 1];
				spares.Remove(spares.Count() - 1);
			}

			Object placed = PlacePanel(reused, slot_angle);
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

	//! Point of the edge of the circle center/radius at angle, on the ground
	protected vector EdgePoint(vector center, float radius, float angle)
	{
		float x = center[0] + radius * Math.Cos(angle);
		float z = center[2] + radius * Math.Sin(angle);
		return Vector(x, GetGame().SurfaceY(x, z), z);
	}

	//! X axis and origin of the panel of the slot at angle on the circle center/radius
	protected void GetSlotFrame(vector center, float radius, float angle, out vector aside, out vector origin)
	{
		// The panel runs from one slot boundary to the next on the ground, so neighbours share their
		// ends and the top edge follows the terrain instead of stepping
		float half_step = Math.PI / m_Count;
		vector left = EdgePoint(center, radius, angle - half_step);
		vector right = EdgePoint(center, radius, angle + half_step);

		// Sheared frame: X stretches the panel exactly from left to right (slope included) while Y stays
		// vertical, so the side edges of neighbouring panels coincide whatever their slopes
		aside = vector.Direction(left, right) * (1.0 / PANEL_WIDTH);
		vector up = "0 1 0";

		// Ground that bulges between the two ends would show under the panel: sink it that much deeper
		float x = center[0] + radius * Math.Cos(angle);
		float z = center[2] + radius * Math.Sin(angle);
		vector middle = (left + right) * 0.5;
		float bulge = Math.Max(0, GetGame().SurfaceY(x, z) - middle[1]);

		origin = middle - up * (SINK + bulge);
	}

	//! Puts panel on the slot at angle, or creates it there when panel is null; null when it cannot be created.
	//! While the zone closes it also records where the next refresh will put the panel, for the glide in between
	protected Object PlacePanel(Object panel, float angle)
	{
		vector aside;
		vector origin;
		GetSlotFrame(m_Center, m_Radius, angle, aside, origin);
		vector dir = Vector(-Math.Cos(angle), 0, -Math.Sin(angle));

		vector transform[4];
		transform[0] = aside;
		transform[1] = "0 1 0";
		transform[2] = dir;
		transform[3] = origin;

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

		if (m_HasNext)
		{
			vector next_aside;
			vector next_origin;
			GetSlotFrame(m_NextCenter, m_NextRadius, angle, next_aside, next_origin);

			if (m_GlideCount == m_Glides.Count())
				m_Glides.Insert(new BattleRoyaleZoneWallGlide());
			BattleRoyaleZoneWallGlide glide = m_Glides[m_GlideCount];
			m_GlideCount++;

			glide.m_Panel = placed;
			glide.m_Dir = dir;
			glide.m_FromAside = aside;
			glide.m_FromOrigin = origin;
			glide.m_ToAside = next_aside;
			glide.m_ToOrigin = next_origin;
		}

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

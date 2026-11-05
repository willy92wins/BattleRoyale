#ifndef SERVER
//! Client-only wall of particles along the edge of the current play area.
//! Only the stretch of the edge near the camera is emitted, so the cost does not grow with the zone size.
class BattleRoyaleZoneWall
{
	static const float SPACING = 20;  //!< metres between two emitters along the edge
	static const float VIEW_RANGE = 300;  //!< emitters further than this from the camera are not created
	static const float REFRESH_SECONDS = 0.5;

	protected int m_ParticleId;
	protected vector m_Center;
	protected float m_Radius;
	protected int m_Count;  //!< number of emitter slots around the whole edge
	protected float m_Elapsed;

	//! emitter slot index -> particle
	protected ref map<int, ParticleSource> m_Particles;

	void BattleRoyaleZoneWall()
	{
		m_ParticleId = ParticleList.CONTAMINATED_AREA_GAS_AROUND;
		m_Particles = new map<int, ParticleSource>;
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

		if (!area || area.GetRadius() <= 0)
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
		foreach (int index, ParticleSource particle : m_Particles)
		{
			if (particle)
				particle.Stop();
		}
		m_Particles.Clear();
		m_Radius = 0;
		m_Count = 0;
	}

	protected void Refresh(vector camera_pos)
	{
		float dx = camera_pos[0] - m_Center[0];
		float dz = camera_pos[2] - m_Center[2];
		float distance = Math.Sqrt(dx * dx + dz * dz);

		// The camera is too far from the edge for any emitter to be in range
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
		ref set<int> wanted = new set<int>;

		for (int i = -half_span; i <= half_span; i++)
		{
			int index = (first + i) % m_Count;
			if (index < 0)
				index += m_Count;

			vector pos = SlotPosition(index, step);
			float px = pos[0] - camera_pos[0];
			float pz = pos[2] - camera_pos[2];
			if (px * px + pz * pz > range_sq)
				continue;

			wanted.Insert(index);
			if (!m_Particles.Contains(index))
			{
				ParticleSource particle = ParticleManager.GetInstance().PlayInWorld(m_ParticleId, pos);
				if (particle)
					m_Particles.Insert(index, particle);
			}
		}

		RemoveAllBut(wanted);
	}

	protected vector SlotPosition(int index, float step)
	{
		float angle = index * step;
		float x = m_Center[0] + m_Radius * Math.Cos(angle);
		float z = m_Center[2] + m_Radius * Math.Sin(angle);
		return Vector(x, GetGame().SurfaceRoadY(x, z), z);
	}

	//! Stops every emitter whose slot is not in keep (all of them when keep is null)
	protected void RemoveAllBut(set<int> keep)
	{
		array<int> stale = new array<int>;
		foreach (int index, ParticleSource particle : m_Particles)
		{
			if (!keep || keep.Find(index) == -1)
			{
				if (particle)
					particle.Stop();
				stale.Insert(index);
			}
		}

		foreach (int key : stale)
		{
			m_Particles.Remove(key);
		}
	}
}
#endif

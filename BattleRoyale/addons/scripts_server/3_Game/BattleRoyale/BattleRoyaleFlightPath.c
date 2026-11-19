#ifdef SERVER
// Pure flight line for the first zone. No match state and no RPC.
// Lives in 3_Game next to the config types: it only needs numbers, and Mission
// states must not call it until the flight state exists.

// Park-Miller generator (Schrage). Same recurrence as tools/flight_path.py.
class BRFlightRandom
{
	protected int m_State;

	void BRFlightRandom(int seed)
	{
		// Seeds outside 1..2147483646 are not used. Only the non-positive side is rewritten here:
		// a comparison against 2147483647 trips the engine's int-boundary check.
		if (seed <= 0)
			seed = 1;
		m_State = seed;
	}

	protected int NextRaw()
	{
		int hi = m_State / 127773;
		int lo = m_State % 127773;
		int next = 16807 * lo - 2836 * hi;
		if (next <= 0)
			next = next + 2147483647;
		m_State = next;
		return next;
	}

	// (0, 1)
	float Next01()
	{
		return NextRaw() / 2147483647.0;
	}
}

class BattleRoyaleFlightPath
{
	static const int SAMPLE_ATTEMPTS = 32;

	vector m_Start;
	vector m_End;
	float m_LengthM;
	float m_DurationS;
	bool m_UsedFallback;

	// Opposite edges of the inset square [margin, W-margin]^2.
	// Axis (west-east or north-south) and which end is the start are fair coin flips.
	// The two lateral coordinates are independent and uniform on that inset, and so are the two altitudes.
	// Nothing aims at the zone centre. Lines that miss the distance limit are rejected and redrawn;
	// that is the only centre bias, and it is exactly the configured limit.
	// A factor of 0 skips the distance test. Radius <= 0 with a positive factor, or a centre outside
	// [0, W]^2, skips sampling and uses the fallback. After SAMPLE_ATTEMPTS misses, the fallback is an
	// east-west segment through the zone centre (clamped to the inset) at the minimum altitude.
	static BattleRoyaleFlightPath FromSeed(int seed, float worldSize, vector zoneCenter, float zoneRadius, float speedMps, float minAltM, float maxAltM, float edgeMarginM, float maxZoneDistanceFactor)
	{
		BRFlightRandom random = new BRFlightRandom(seed);
		if (!random)
			return null;
		return FromRandom(random, worldSize, zoneCenter, zoneRadius, speedMps, minAltM, maxAltM, edgeMarginM, maxZoneDistanceFactor);
	}

	static BattleRoyaleFlightPath FromRandom(BRFlightRandom random, float worldSize, vector zoneCenter, float zoneRadius, float speedMps, float minAltM, float maxAltM, float edgeMarginM, float maxZoneDistanceFactor)
	{
		BattleRoyaleFlightPath path = new BattleRoyaleFlightPath;
		if (!random || worldSize <= 0 || edgeMarginM < 0 || edgeMarginM * 2 >= worldSize)
		{
			path.SetFallback(worldSize, zoneCenter, minAltM, edgeMarginM, speedMps);
			return path;
		}

		bool impossible = zoneRadius <= 0 && maxZoneDistanceFactor > 0;
		if (zoneCenter[0] < 0 || zoneCenter[2] < 0 || zoneCenter[0] > worldSize || zoneCenter[2] > worldSize)
			impossible = true;

		float limit = maxZoneDistanceFactor * zoneRadius;
		bool checkDistance = maxZoneDistanceFactor > 0;
		int attempt = 0;
		int axis = 0;
		int flipped = 0;
		float nearEdge = edgeMarginM;
		float farEdge = worldSize - edgeMarginM;
		float startAlong = nearEdge;
		float endAlong = farEdge;
		float span = farEdge - nearEdge;
		float crossStart = 0;
		float crossEnd = 0;
		float altStart = minAltM;
		float altEnd = minAltM;
		vector start = vector.Zero;
		vector finish = vector.Zero;
		while (!impossible && attempt < SAMPLE_ATTEMPTS)
		{
			attempt = attempt + 1;
			axis = 0;
			if (random.Next01() >= 0.5)
				axis = 1;
			flipped = 0;
			if (random.Next01() >= 0.5)
				flipped = 1;

			startAlong = nearEdge;
			endAlong = farEdge;
			if (flipped == 1)
			{
				startAlong = farEdge;
				endAlong = nearEdge;
			}

			crossStart = nearEdge + random.Next01() * span;
			crossEnd = nearEdge + random.Next01() * span;
			altStart = minAltM + random.Next01() * (maxAltM - minAltM);
			altEnd = minAltM + random.Next01() * (maxAltM - minAltM);

			if (axis == 0)
			{
				start = Vector(startAlong, altStart, crossStart);
				finish = Vector(endAlong, altEnd, crossEnd);
			}
			else
			{
				start = Vector(crossStart, altStart, startAlong);
				finish = Vector(crossEnd, altEnd, endAlong);
			}

			if (checkDistance && DistanceXZToSegment(zoneCenter, start, finish) > limit)
				continue;

			path.m_Start = start;
			path.m_End = finish;
			path.m_UsedFallback = false;
			path.SetLength(speedMps);
			return path;
		}

		path.SetFallback(worldSize, zoneCenter, minAltM, edgeMarginM, speedMps);
		return path;
	}

	protected void SetFallback(float worldSize, vector zoneCenter, float minAltM, float edgeMarginM, float speedMps)
	{
		float inset = edgeMarginM;
		if (inset < 0)
			inset = 0;
		if (worldSize <= 0)
			worldSize = inset * 2 + 1;
		if (inset * 2 >= worldSize)
			inset = worldSize * 0.25;

		float cross = zoneCenter[2];
		if (cross < inset)
			cross = inset;
		if (cross > worldSize - inset)
			cross = worldSize - inset;

		m_Start = Vector(inset, minAltM, cross);
		m_End = Vector(worldSize - inset, minAltM, cross);
		m_UsedFallback = true;
		SetLength(speedMps);
	}

	protected void SetLength(float speedMps)
	{
		m_LengthM = vector.Distance(m_Start, m_End);
		if (speedMps > 0)
			m_DurationS = m_LengthM / speedMps;
		else
			m_DurationS = 0;
	}

	protected static float DistanceXZToSegment(vector point, vector start, vector finish)
	{
		float abx = finish[0] - start[0];
		float abz = finish[2] - start[2];
		float ab2 = abx * abx + abz * abz;
		float t = 0;
		if (ab2 > 0)
		{
			t = ((point[0] - start[0]) * abx + (point[2] - start[2]) * abz) / ab2;
			if (t < 0)
				t = 0;
			if (t > 1)
				t = 1;
		}
		float dx = start[0] + abx * t - point[0];
		float dz = start[2] + abz * t - point[2];
		return Math.Sqrt(dx * dx + dz * dz);
	}
}
#endif

#ifdef SERVER
//! What a match leaves in the world and serial matches remove before the next one: player bodies (their inventory goes
//! with them), items players dropped on the ground, and objects the mod spawned for the match.
//! Entries are recorded when they appear, so the cleanup never scans the map. The references are weak: an entry the
//! Central Economy or anything else deletes first reads as null and is skipped.
class BRMatchDebris
{
	//! Dropped items recorded per match; past it a match records no more of them (bodies and objects are always recorded)
	static const int MAX_ITEMS = 20000;

	protected static ref BRMatchDebris s_Instance;

	//! Set by BattleRoyaleServer from server_settings.json; off, nothing is recorded
	static bool s_Enabled = false;

	//! Recording generation, advanced by TakeAll: an item stamped with it is already recorded in this match
	static int s_Generation = 1;

	protected ref array<PlayerBase> m_Bodies;
	protected ref array<EntityAI> m_Items;
	protected ref array<EntityAI> m_Objects;
	protected bool m_ItemsCapLogged;

	void BRMatchDebris()
	{
		m_Bodies = new array<PlayerBase>;
		m_Items = new array<EntityAI>;
		m_Objects = new array<EntityAI>;
	}

	static BRMatchDebris Get()
	{
		if (!s_Instance)
			s_Instance = new BRMatchDebris;
		return s_Instance;
	}

	void AddBody(PlayerBase body)
	{
		if (s_Enabled && body)
			m_Bodies.Insert(body);
	}

	void AddItem(EntityAI item)
	{
		if (!s_Enabled || !item)
			return;

		if (m_Items.Count() >= MAX_ITEMS)
		{
			if (!m_ItemsCapLogged)
			{
				m_ItemsCapLogged = true;
				BattleRoyaleUtils.Warn("[Serial] " + MAX_ITEMS + " dropped items recorded in this match; later drops are left to the Central Economy");
			}
			return;
		}

		// Stamped only once accepted (past the cap), so the same item dropped again in this match records once
		ItemBase ib = ItemBase.Cast(item);
		if (ib)
		{
			if (ib.br_debris_gen == s_Generation)
				return;
			ib.br_debris_gen = s_Generation;
		}

		m_Items.Insert(item);
	}

	void AddObject(EntityAI obj)
	{
		if (s_Enabled && obj)
			m_Objects.Insert(obj);
	}

	//! Hands the recorded entries to the cleanup and starts an empty record for the next match
	BRMatchDebrisBatch TakeAll()
	{
		BRMatchDebrisBatch batch = new BRMatchDebrisBatch(m_Bodies, m_Items, m_Objects);
		m_Bodies = new array<PlayerBase>;
		m_Items = new array<EntityAI>;
		m_Objects = new array<EntityAI>;
		m_ItemsCapLogged = false;
		// The next match records afresh: items kept across matches (a waiting player's own kit) stamp again
		s_Generation++;
		return batch;
	}
}

//! The entries of one finished match, deleted a few per server tick
class BRMatchDebrisBatch
{
	protected ref array<PlayerBase> m_Bodies;
	protected ref array<EntityAI> m_Items;
	protected ref array<EntityAI> m_Objects;
	protected int m_BodyIndex;
	protected int m_ItemIndex;
	protected int m_ObjectIndex;

	int m_DeletedBodies;
	int m_DeletedItems;
	int m_DeletedObjects;

	void BRMatchDebrisBatch(array<PlayerBase> bodies, array<EntityAI> items, array<EntityAI> objects)
	{
		m_Bodies = bodies;
		m_Items = items;
		m_Objects = objects;
	}

	bool IsDone()
	{
		return m_BodyIndex >= m_Bodies.Count() && m_ItemIndex >= m_Items.Count() && m_ObjectIndex >= m_Objects.Count();
	}

	//! Deletes up to budget entries (a body counts as several, its inventory goes with it); returns how many were deleted
	int DeleteSome(int budget)
	{
		int deleted = 0;

		while (budget > 0 && m_ObjectIndex < m_Objects.Count())
		{
			EntityAI obj = m_Objects[m_ObjectIndex];
			m_ObjectIndex++;
			budget--;
			if (!obj)
				continue;
			GetGame().ObjectDelete(obj);
			m_DeletedObjects++;
			deleted++;
		}

		while (budget > 0 && m_BodyIndex < m_Bodies.Count())
		{
			PlayerBase body = m_Bodies[m_BodyIndex];
			m_BodyIndex++;
			budget -= 5;
			// A body a player still controls is not a leftover
			if (!body || body.IsAlive())
				continue;
			PlayerIdentity owner = body.GetIdentity();
			if (owner && owner.GetPlayer() == body)
				continue;
			GetGame().ObjectDelete(body);
			m_DeletedBodies++;
			deleted++;
		}

		while (budget > 0 && m_ItemIndex < m_Items.Count())
		{
			EntityAI item = m_Items[m_ItemIndex];
			m_ItemIndex++;
			budget--;
			// Only what still lies on the ground: an item picked up again belongs to its new owner
			if (!item || item.GetHierarchyParent())
				continue;
			GetGame().ObjectDelete(item);
			m_DeletedItems++;
			deleted++;
		}

		return deleted;
	}
}
#endif

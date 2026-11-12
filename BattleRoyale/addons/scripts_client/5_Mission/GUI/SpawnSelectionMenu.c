#ifndef SERVER
class SpawnSelectionMenu extends UIScriptedMenu
{
	protected ref IngameHud m_Hud;
	protected ref BRMapHandler m_MapMenuHandler;
	protected ref MapWidget m_MapWidgetInstance;

	protected MapWidget m_MapWidget;
	protected TextWidget m_CountdownText;
	protected ButtonWidget m_ConfirmButton;

	protected vector m_SelectedSpawnPoint;

	protected CanvasWidget m_SpawnCanvas;

	protected int i_CountdownEnd;
	protected int i_CountdownShown = -1;	//!< last whole second written to m_CountdownText

	protected float spawn_size = 50.0;

	protected vector v_FirstZoneCenter = "0 0 0";
	protected float f_FirstZoneRadius = 0;

	protected ref map<string, vector> m_TeammateSpawnPoints = new map<string, vector>();
	protected ref map<string, int> m_TeammateSpawnPointsColor = new map<string, int>();

	protected CanvasWidget m_HeatMapCanvas;
	protected ref array<vector> m_HeatMapSpawnPoints = new array<vector>();

	// Heat map squares to draw, rebuilt from m_HeatMapSpawnPoints only when new data arrives
	protected static const int HEATMAP_KEY_OFFSET = 4096;	//!< keeps cell coordinates positive inside the integer cell key
	protected static const int HEATMAP_KEY_STRIDE = 8192;
	protected ref array<float> m_HeatMapCellX = new array<float>();
	protected ref array<float> m_HeatMapCellZ = new array<float>();
	protected ref array<int> m_HeatMapCellColor = new array<int>();
	protected float f_HeatMapCellSize;
	protected bool b_HeatMapDirty = true;

	// Map view of the last redraw; the canvases are only redrawn when it or their content changes
	protected bool b_CanvasDirty = true;
	protected float f_DrawnMapScale = -1;
	protected vector v_DrawnMapPos;
	protected float f_DrawnScreenX;
	protected float f_DrawnScreenY;
	protected float f_DrawnScreenW;
	protected float f_DrawnScreenH;

	void SpawnSelectionMenu()
	{
		BattleRoyaleUtils.Trace("SpawnSelectionMenu::SpawnSelectionMenu");
		g_Game.SetKeyboardHandle(this);
	}

	void ~SpawnSelectionMenu()
	{
		g_Game.SetKeyboardHandle(NULL);

		if (m_Hud)
		{
			m_Hud.ShowHudUI(true);
			m_Hud.ShowQuickbarUI(true);
		}
	}

	override void OnShow()
	{
		BattleRoyaleUtils.Trace("SpawnSelectionMenu::OnShow");
		super.OnShow();

		PPEffects.SetBlurMenu(1);
		SetFocus(layoutRoot);
	}

	override void OnHide()
	{
		BattleRoyaleUtils.Trace("SpawnSelectionMenu::OnHide");
		super.OnHide();

		PPEffects.SetBlurMenu(0);
	}

	override Widget Init()
	{
		BattleRoyaleUtils.Trace("SpawnSelectionMenu::Init");
		layoutRoot = GetGame().GetWorkspace().CreateWidgets("Vigrid-BattleRoyale/GUI/layouts/spawn_selection.layout");
		m_Hud = IngameHud.Cast(GetGame().GetMission().GetHud());

		m_MapWidget = MapWidget.Cast(layoutRoot.FindAnyWidget("SpawnMap"));

		m_CountdownText = TextWidget.Cast(layoutRoot.FindAnyWidget("CountdownText"));

		m_ConfirmButton = ButtonWidget.Cast(layoutRoot.FindAnyWidget("ConfirmButton"));

		if (m_MapWidget)
		{
			vector tempPosition = GetGame().ConfigGetVector(string.Format("CfgWorlds %1 centerPosition", GetGame().GetWorldName()));
			float scale = 0.33;
			vector mapPosition = Vector(tempPosition[0], tempPosition[1], tempPosition[2]);

			if (v_FirstZoneCenter != "0 0 0")
			{
				m_MapWidget.SetMapPos(v_FirstZoneCenter);
				m_MapWidget.SetScale(0.10);
			}

			m_MapMenuHandler = new BRMapHandler(m_MapWidget);

			if (m_Hud)
			{
				m_Hud.ShowHudUI(false);
				m_Hud.ShowQuickbarUI(false);
			}

			m_SpawnCanvas = CanvasWidget.Cast(m_MapWidget.FindAnyWidget("CanvasSpawnMap"));
			m_HeatMapCanvas = CanvasWidget.Cast(layoutRoot.FindAnyWidget("CanvasHeatmap"));
		}

		layoutRoot.Update();

		return layoutRoot;
	}

	override void Update(float timeslice)
	{
        super.Update(timeslice);

		UpdateCountdownText();

		if(m_MapWidget && m_SpawnCanvas)
		{
			// The heat map grid is only rebuilt when the server sent new spawn points
			if (b_HeatMapDirty)
			{
				RebuildHeatMap();
				b_HeatMapDirty = false;
				b_CanvasDirty = true;
			}

			// The canvases are only redrawn when their content or the map view changed
			if (HasMapViewChanged())
			{
				b_CanvasDirty = true;
			}

			if (b_CanvasDirty)
			{
				b_CanvasDirty = false;
				RedrawCanvases();
			}
		}
	}

	//! Rewrites the countdown text only when the displayed second changes
	protected void UpdateCountdownText()
	{
		if (!m_CountdownText)
			return;

		int time_left = (i_CountdownEnd - GetGame().GetTime()) / 1000;  // calculate time left in whole seconds
		if (time_left < 0)
			time_left = 0;

		if (time_left == i_CountdownShown)
			return;

		i_CountdownShown = time_left;

		StringLocaliser message;
		if (time_left > 1)
		{
			message = new StringLocaliser("STR_BR_TIMER_SPAWN_SELECTION_SECONDS");
			message.Set(0, time_left);  // replace the first parameter with the time left in seconds
		}
		else if (time_left == 1)
		{
			message = new StringLocaliser("STR_BR_TIMER_SPAWN_SELECTION_SECOND");
			message.Set(0, time_left);
		}
		else
		{
			message = new StringLocaliser("STR_BR_TIME_TO_DEPLOY");
		}
		m_CountdownText.SetText(message.Format());
	}

	//! True when the map was zoomed, panned, moved or resized since the last call
	protected bool HasMapViewChanged()
	{
		float scale = m_MapWidget.GetScale();
		vector map_pos = m_MapWidget.GetMapPos();
		float screen_x, screen_y, screen_w, screen_h;
		m_MapWidget.GetScreenPos(screen_x, screen_y);
		m_MapWidget.GetScreenSize(screen_w, screen_h);

		if (scale == f_DrawnMapScale && map_pos == v_DrawnMapPos && screen_x == f_DrawnScreenX && screen_y == f_DrawnScreenY && screen_w == f_DrawnScreenW && screen_h == f_DrawnScreenH)
			return false;

		f_DrawnMapScale = scale;
		v_DrawnMapPos = map_pos;
		f_DrawnScreenX = screen_x;
		f_DrawnScreenY = screen_y;
		f_DrawnScreenW = screen_w;
		f_DrawnScreenH = screen_h;
		return true;
	}

	protected void RedrawCanvases()
	{
		m_SpawnCanvas.Clear();

		// Show the teammates zones
		foreach (string playerId, vector spawn_point : m_TeammateSpawnPoints)
		{
			if (playerId)
			{
				WorldRenderOval(m_SpawnCanvas, m_MapWidget, spawn_point[0], spawn_point[2], spawn_size, spawn_size, m_TeammateSpawnPointsColor.Get(playerId));
			}
		}

		// Show the first zone, in yellow like every announced zone that has not locked yet
		if(v_FirstZoneCenter != "0 0 0" && f_FirstZoneRadius > 0)
		{
			WorldRenderOval(m_SpawnCanvas, m_MapWidget, v_FirstZoneCenter[0], v_FirstZoneCenter[2], f_FirstZoneRadius, f_FirstZoneRadius, BR_ZONE_COLOR_OPEN);
		}

		// Draw the heat map squares computed by RebuildHeatMap
		if (m_HeatMapCanvas)
		{
			m_HeatMapCanvas.Clear();

			if (m_HeatMapCellColor.Count() > 0 && f_HeatMapCellSize > 0)
			{
				// Every square has the same world size, so its screen size and the map size are converted once for the whole redraw
				float map_w, map_h;
				m_MapWidget.GetScreenSize(map_w, map_h);

				vector screen_origin = m_MapWidget.MapToScreen(Vector(0, 0, 0));
				vector screen_edge_x = m_MapWidget.MapToScreen(Vector(f_HeatMapCellSize, 0, 0));
				vector screen_edge_z = m_MapWidget.MapToScreen(Vector(0, 0, f_HeatMapCellSize));
				float square_w = vector.Distance(screen_origin, screen_edge_x);
				float square_h = vector.Distance(screen_origin, screen_edge_z);

				for (int i = 0; i < m_HeatMapCellColor.Count(); i++)
				{
					RenderFilledSquare(m_HeatMapCanvas, m_MapWidget, m_HeatMapCellX[i], m_HeatMapCellZ[i], f_HeatMapCellSize, square_w, square_h, map_w, map_h, m_HeatMapCellColor[i]);
				}
			}
		}
	}

	//! Counts the spawn points per grid cell and stores the squares to draw: the rings around each cell first, then the cells
	protected void RebuildHeatMap()
	{
		m_HeatMapCellX.Clear();
		m_HeatMapCellZ.Clear();
		m_HeatMapCellColor.Clear();

		// Size of each grid cell in the heat map
		f_HeatMapCellSize = spawn_size * HEATMAP_GRID_SIZE_MULTIPLIER;
		if (!m_HeatMapSpawnPoints || f_HeatMapCellSize <= 0)
			return;

		// Declare all variables upfront
		int grid_x, grid_z, grid_key;
		float world_x, world_z;
		float intensity, surroundIntensity;
		int capped_density, r, b, alpha, color, surroundColor;
		int offset, dx, dz;

		// Count spawn points in each grid cell, keyed by an integer built from the cell coordinates
		map<int, int> grid_density = new map<int, int>();
		for (int i = 0; i < m_HeatMapSpawnPoints.Count(); i++)
		{
			vector heatPoint = m_HeatMapSpawnPoints[i];

			// Convert to grid coordinates (snap to grid)
			grid_x = Math.Floor(heatPoint[0] / f_HeatMapCellSize);
			grid_z = Math.Floor(heatPoint[2] / f_HeatMapCellSize);
			if (grid_x < -HEATMAP_KEY_OFFSET || grid_x >= HEATMAP_KEY_OFFSET || grid_z < -HEATMAP_KEY_OFFSET || grid_z >= HEATMAP_KEY_OFFSET)
				continue;

			grid_key = (grid_x + HEATMAP_KEY_OFFSET) * HEATMAP_KEY_STRIDE + (grid_z + HEATMAP_KEY_OFFSET);

			// Increment the density counter for this cell
			if (grid_density.Contains(grid_key))
				grid_density.Set(grid_key, grid_density.Get(grid_key) + 1);
			else
				grid_density.Insert(grid_key, 1);
		}

		// First pass: degraded effect for surroundings (lowest layers first)
		foreach (int key, int density : grid_density)
		{
			// Skip if density is 0
			if (density <= 0)
				continue;

			// Cell center in world coordinates
			GetHeatMapCellCenter(key, world_x, world_z);

			// Cap density at max_density for color calculation
			capped_density = Math.Min(density, HEATMAP_MAX_DENSITY);

			// Calculate intensity factor (0.0 to 1.0)
			intensity = capped_density / (float)HEATMAP_MAX_DENSITY;

			// Add degraded effect to surrounding cells
			for (offset = 2; offset >= 1; offset--)
			{
				// Calculate alpha for surrounding cells (50-100 based on offset)
				alpha = 50 * (3 - offset);

				// Calculate color intensity for surrounding (reduced by distance)
				surroundIntensity = intensity * (1 - (offset * 0.3));

				// Calculate RGB values - blue (low) to red (high) gradient
				r = Math.Round(surroundIntensity * 255);
				b = Math.Round((1 - surroundIntensity) * 255);
				surroundColor = ARGB(alpha, r, 0, b);

				// Surrounding cells in a ring pattern
				for (dx = -offset; dx <= offset; dx++)
				{
					for (dz = -offset; dz <= offset; dz++)
					{
						// Skip the center and non-edge cells (only draw the ring)
						if ((Math.AbsInt(dx) != offset) && (Math.AbsInt(dz) != offset))
							continue;

						AddHeatMapSquare(world_x + (dx * f_HeatMapCellSize), world_z + (dz * f_HeatMapCellSize), surroundColor);
					}
				}
			}
		}

		// Second pass: the primary cells with full intensity
		foreach (int density_key, int density_value : grid_density)
		{
			// Skip if density is 0
			if (density_value <= 0)
				continue;

			// Cell center in world coordinates
			GetHeatMapCellCenter(density_key, world_x, world_z);

			// Cap density at max_density for color calculation
			capped_density = Math.Min(density_value, HEATMAP_MAX_DENSITY);

			// Calculate color intensity (0.0 to 1.0)
			intensity = capped_density / (float)HEATMAP_MAX_DENSITY;

			// Calculate RGB values - blue (low) to red (high) gradient
			r = Math.Round(intensity * 255);
			b = Math.Round((1 - intensity) * 255);
			color = ARGB(150, r, 0, b);

			AddHeatMapSquare(world_x, world_z, color);
		}
	}

	protected void GetHeatMapCellCenter(int grid_key, out float world_x, out float world_z)
	{
		int grid_x = (grid_key / HEATMAP_KEY_STRIDE) - HEATMAP_KEY_OFFSET;
		int grid_z = (grid_key % HEATMAP_KEY_STRIDE) - HEATMAP_KEY_OFFSET;

		// Convert back to world coordinates for rendering (use cell center)
		world_x = grid_x * f_HeatMapCellSize + (f_HeatMapCellSize / 2);
		world_z = grid_z * f_HeatMapCellSize + (f_HeatMapCellSize / 2);
	}

	protected void AddHeatMapSquare(float world_x, float world_z, int color)
	{
		m_HeatMapCellX.Insert(world_x);
		m_HeatMapCellZ.Insert(world_z);
		m_HeatMapCellColor.Insert(color);
	}

	void SetInitialCountdown(int countdown)
	{
		i_CountdownEnd = GetGame().GetTime() + (countdown * 1000);
	}

	void SetSpawnSize(float size)
	{
		spawn_size = size;
		b_HeatMapDirty = true;
		b_CanvasDirty = true;
	}

	float GetSpawnSize()
	{
		return spawn_size;
	}

	void SetFirstZone(vector pos, float size)
	{
		v_FirstZoneCenter = pos;
		f_FirstZoneRadius = size;
		b_CanvasDirty = true;

		if (m_MapWidget)
		{
			BattleRoyaleUtils.Trace("SpawnSelectionMenu::SetFirstZone");

			m_MapWidget.SetMapPos(v_FirstZoneCenter);
			m_MapWidget.SetScale(0.33);

			GetGame().GetCallQueue(CALL_CATEGORY_GUI).CallLater(DelayedSetMapPos, 100, false, v_FirstZoneCenter);
		}
	}

	void DelayedSetMapPos(vector pos)
	{
		if (m_MapWidget)
		{
			BattleRoyaleUtils.Trace("SpawnSelectionMenu::DelayedSetMapPos");
			m_MapWidget.SetMapPos(pos);
			m_MapWidget.SetScale(0.33);
		}
	}

	void SelectSpawnPoint(vector pos)
	{
		BattleRoyaleUtils.Trace("SpawnSelectionMenu::SelectSpawnPoint");

		// Check if some time is left
		if (GetGame().GetTime() > i_CountdownEnd)
		{
			BattleRoyaleUtils.Trace("SpawnSelectionMenu::SelectSpawnPoint Time expired");
			return;
		}

		vector tempPosition = m_MapWidget.ScreenToMap(pos);

		// Check if the position is valid, e.g. within the world bounds
		int worldSize = GetGame().GetWorld().GetWorldSize();  // Get the world size
		if (tempPosition[0] < (GetSpawnSize()/2) || tempPosition[2] < (GetSpawnSize()/2) || tempPosition[0] > (worldSize-(GetSpawnSize()/2)) || tempPosition[2] > (worldSize-(GetSpawnSize()/2)))
		{
			BattleRoyaleUtils.Trace("SpawnSelectionMenu::SelectSpawnPoint Invalid Position (out of world bounds)");
			BattleRoyaleUtils.Trace(string.Format("X: %1 Y: %2", (GetSpawnSize()/2), worldSize-(GetSpawnSize()/2)));
			return;
		}

		// Check if the position is valid, e.g. not in sea
		if(GetGame().SurfaceIsSea(tempPosition[0], tempPosition[2]))
		{
			BattleRoyaleUtils.Trace("SpawnSelectionMenu::SelectSpawnPoint Invalid Position (in sea)");
			return;
		}

		// Check if the position is valid, e.g. within the first zone
		if (v_FirstZoneCenter != "0 0 0" && f_FirstZoneRadius > 0)
		{
			float distance = vector.Distance(tempPosition, v_FirstZoneCenter);
			// Add a small tolerance (25 meters) to account for precision errors
			if (distance > (f_FirstZoneRadius + 25))
			{
				BattleRoyaleUtils.Trace("SpawnSelectionMenu::SelectSpawnPoint Invalid Position (out of first zone)");
				return;
			}
		}

		BattleRoyaleUtils.Trace(string.Format("SpawnSelectionMenu::SelectSpawnPoint: %1", tempPosition));

		PlayerBase player = PlayerBase.Cast( GetGame().GetPlayer() );
		GetRPCManager().SendRPC( RPC_DAYZBRSERVER_NAMESPACE, "OnPlayerSpawnSelected", new Param1<vector>( tempPosition ), true, NULL, player );
	}

	void WorldRenderOval(CanvasWidget canvas, MapWidget world_map, float world_x, float world_z, float radius_x, float radius_z, int color = -1)
	{
		BattleRoyaleMapCanvas.DrawWorldOval(canvas, world_map, world_x, world_z, radius_x, radius_z, color);
	}

	/**
	 * Renders a filled square on the given canvas, using world coordinates for positioning.
	 *
	 * @param canvas      The CanvasWidget instance used for rendering.
	 * @param world_map   The MapWidget instance used to convert world coordinates to screen coordinates.
	 * @param world_x     The X-coordinate of the square's center in world space.
	 * @param world_z     The Z-coordinate of the square's center in world space.
	 * @param size        The size of the square in world units.
	 * @param square_w    The width of the square in screen pixels, converted once by the caller for all squares of a redraw.
	 * @param square_h    The height of the square in screen pixels, converted once by the caller for all squares of a redraw.
	 * @param clip_w      When above 0 with clip_h, a square fully outside [0, clip_w] x [0, clip_h] draws nothing.
	 * @param clip_h      Height of the clip rectangle.
	 * @param color       The color of the square, specified as an ARGB integer. Defaults to -1 (white).
	 *
	 * The method calculates the square's position in screen space
	 * and renders it as one thick horizontal line that fills the area.
	 */
	void RenderFilledSquare(CanvasWidget canvas, MapWidget world_map, float world_x, float world_z, float size, float square_w, float square_h, float clip_w, float clip_h, int color = -1)
	{
		if (!world_map || size <= 0 || !canvas)
		{
			BattleRoyaleUtils.Trace("RenderFilledSquare: Invalid parameters");
			return;
		}

		float screen_x, screen_y;
		world_map.GetScreenPos(screen_x, screen_y);

		// Create the center point in world coordinates
		vector worldCenter = Vector(world_x, 0, world_z);
		vector screenCenter = world_map.MapToScreen(worldCenter);

		// Calculate the corners of the square in screen space
		float cx = screenCenter[0] - screen_x;
		float cy = screenCenter[1] - screen_y;

		float left = cx - square_w/2;
		float right = cx + square_w/2;
		float top = cy - square_h/2;
		float bottom = cy + square_h/2;

		// A square fully outside the map widget draws nothing
		if (clip_w > 0 && clip_h > 0 && (right < 0 || bottom < 0 || left > clip_w || top > clip_h))
			return;

		// One thick horizontal line fills the square; at least one pixel wide so a sub-pixel square still shows
		canvas.DrawLine(left, cy, right, cy, Math.Max(square_h, 1.0), color);
	}

	void UpdateHeatMap(array<vector> spawnPoints)
	{
		BattleRoyaleUtils.Trace("SpawnSelectionMenu::UpdateHeatMap");
		m_HeatMapSpawnPoints = spawnPoints;
		b_HeatMapDirty = true;
	}

	vector GetSelectedSpawnPoint()
	{
		return m_SelectedSpawnPoint;
	}

	void SetTeammateSpawnPoint(PlayerBase player, vector pos, int color)
	{
		BattleRoyaleUtils.Trace("SpawnSelectionMenu::SetTeammateSpawnPoint");
		if (player && player.GetIdentity() && player.GetIdentity().GetId())
		{
			string playerId = player.GetIdentity().GetId();

			BattleRoyaleUtils.Trace(string.Format("Player: %1", player.BR_GetPlayerName()));
			if (m_TeammateSpawnPoints.Contains(playerId))
			{
				BattleRoyaleUtils.Trace(string.Format("Player already in map: %1", player.BR_GetPlayerName()));
				m_TeammateSpawnPoints.Set(playerId, pos);
			} else {
				BattleRoyaleUtils.Trace(string.Format("Player not in map: %1", player.BR_GetPlayerName()));
				m_TeammateSpawnPoints.Insert(playerId, pos);
			}
			m_TeammateSpawnPointsColor.Set(playerId, color);
			b_CanvasDirty = true;
		}
	}
}
#endif

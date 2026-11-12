#ifndef SERVER
//! Canvas drawing in world coordinates for map widgets
class BattleRoyaleMapCanvas
{
	// Cohen-Sutherland outcode bits for canvas-local coordinates
	protected static const int OUTCODE_LEFT = 1;
	protected static const int OUTCODE_RIGHT = 2;
	protected static const int OUTCODE_TOP = 4;
	protected static const int OUTCODE_BOTTOM = 8;

	/**
	 * Draws an oval given in world coordinates on a canvas that covers the map widget.
	 *
	 * @param canvas      Canvas with the same screen position as the map widget
	 * @param world_map   Map widget that converts world coordinates to screen coordinates
	 * @param world_x     X of the oval centre in world space
	 * @param world_z     Z of the oval centre in world space
	 * @param radius_x    Radius along X in world units
	 * @param radius_z    Radius along Z in world units
	 * @param color       ARGB colour of the line
	 * @param width       Line width in pixels
	 * @param clip_w      When above 0 with clip_h, segments are clipped to [0, clip_w] x [0, clip_h] in canvas space
	 * @param clip_h      Height of the clip rectangle
	 * @return            Number of segments drawn
	 */
	static int DrawWorldOval(CanvasWidget canvas, MapWidget world_map, float world_x, float world_z, float radius_x, float radius_z, int color = -1, float width = 2, float clip_w = 0, float clip_h = 0)
	{
		if (!world_map || radius_x <= 0 || radius_z <= 0 || !canvas)
		{
			BattleRoyaleUtils.Trace("DrawWorldOval: Invalid parameters");
			return 0;
		}

		float screen_x, screen_y;
		world_map.GetScreenPos(screen_x, screen_y);

		vector screenCenter = world_map.MapToScreen(Vector(world_x, 0, world_z));
		vector screenEdgeX = world_map.MapToScreen(Vector(world_x + radius_x, 0, world_z));
		vector screenEdgeZ = world_map.MapToScreen(Vector(world_x, 0, world_z + radius_z));

		// Radii in screen pixels
		float screenWidth = vector.Distance(screenCenter, screenEdgeX);
		float screenHeight = vector.Distance(screenCenter, screenEdgeZ);

		// Centre in canvas space
		float cx = screenCenter[0] - screen_x;
		float cy = screenCenter[1] - screen_y;

		bool clip = clip_w > 0 && clip_h > 0;

		// An oval whose bounding box misses the clip rectangle draws nothing
		if (clip && (cx + screenWidth < 0 || cx - screenWidth > clip_w || cy + screenHeight < 0 || cy - screenHeight > clip_h))
			return 0;

		// 64 to 96 segments keep the error under a pixel up to a radius of about 1800 px
		// A clipped oval is a zoomed-in view of a large zone, so it may use up to 512 to keep the edge accurate
		int max_segments = 96;
		if (clip)
			max_segments = 512;
		int segments = Math.Clamp(Math.Round(Math.Max(screenWidth, screenHeight) / 8), 64, max_segments);
		float angleIncrement = 360.0 / segments;
		int drawn = 0;

		for (int i = 0; i < segments; i++)
		{
			float angle1 = i * angleIncrement;
			float angle2 = (i + 1) * angleIncrement;

			float x1 = cx + (screenWidth * Math.Cos(angle1 * Math.DEG2RAD));
			float y1 = cy + (screenHeight * Math.Sin(angle1 * Math.DEG2RAD));

			float x2 = cx + (screenWidth * Math.Cos(angle2 * Math.DEG2RAD));
			float y2 = cy + (screenHeight * Math.Sin(angle2 * Math.DEG2RAD));

			if (clip && !ClipSegment(x1, y1, x2, y2, clip_w, clip_h))
				continue;

			canvas.DrawLine(x1, y1, x2, y2, width, color);
			drawn++;
		}

		return drawn;
	}

	//! Cohen-Sutherland clip of a segment against [0, xmax] x [0, ymax]; false when nothing of it is left
	static bool ClipSegment(inout float x1, inout float y1, inout float x2, inout float y2, float xmax, float ymax)
	{
		int code1 = GetOutCode(x1, y1, xmax, ymax);
		int code2 = GetOutCode(x2, y2, xmax, ymax);

		// Each pass moves one end onto an edge: four passes clip any segment, the rest is a margin for rounding
		for (int pass = 0; pass < 8; pass++)
		{
			if ((code1 | code2) == 0)
				return true;

			if ((code1 & code2) != 0)
				return false;

			int code_out = code2;
			if (code1 != 0)
				code_out = code1;

			float dx = x2 - x1;
			float dy = y2 - y1;
			float x = 0;
			float y = 0;

			if ((code_out & OUTCODE_BOTTOM) != 0)
			{
				x = x1;
				if (dy != 0)
					x = x1 + dx * (ymax - y1) / dy;
				y = ymax;
			}
			else if ((code_out & OUTCODE_TOP) != 0)
			{
				x = x1;
				if (dy != 0)
					x = x1 + dx * (0 - y1) / dy;
				y = 0;
			}
			else if ((code_out & OUTCODE_RIGHT) != 0)
			{
				y = y1;
				if (dx != 0)
					y = y1 + dy * (xmax - x1) / dx;
				x = xmax;
			}
			else
			{
				y = y1;
				if (dx != 0)
					y = y1 + dy * (0 - x1) / dx;
				x = 0;
			}

			if (code_out == code1)
			{
				x1 = x;
				y1 = y;
				code1 = GetOutCode(x1, y1, xmax, ymax);
			}
			else
			{
				x2 = x;
				y2 = y;
				code2 = GetOutCode(x2, y2, xmax, ymax);
			}
		}

		return false;
	}

	protected static int GetOutCode(float x, float y, float xmax, float ymax)
	{
		int code = 0;
		if (x < 0)
			code = code | OUTCODE_LEFT;
		else if (x > xmax)
			code = code | OUTCODE_RIGHT;

		if (y < 0)
			code = code | OUTCODE_TOP;
		else if (y > ymax)
			code = code | OUTCODE_BOTTOM;

		return code;
	}
}
#endif

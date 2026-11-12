#ifndef SERVER
//! Canvas drawing in world coordinates for map widgets
class BattleRoyaleMapCanvas
{
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
	 */
	static void DrawWorldOval(CanvasWidget canvas, MapWidget world_map, float world_x, float world_z, float radius_x, float radius_z, int color = -1, float width = 2)
	{
		if (!world_map || radius_x <= 0 || radius_z <= 0 || !canvas)
		{
			BattleRoyaleUtils.Trace("DrawWorldOval: Invalid parameters");
			return;
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

		// 64 to 96 segments keep the error under a pixel up to a radius of about 1800 px
		int segments = Math.Clamp(Math.Round(Math.Max(screenWidth, screenHeight) / 8), 64, 96);
		float angleIncrement = 360.0 / segments;

		for (int i = 0; i < segments; i++)
		{
			float angle1 = i * angleIncrement;
			float angle2 = (i + 1) * angleIncrement;

			float x1 = cx + (screenWidth * Math.Cos(angle1 * Math.DEG2RAD));
			float y1 = cy + (screenHeight * Math.Sin(angle1 * Math.DEG2RAD));

			float x2 = cx + (screenWidth * Math.Cos(angle2 * Math.DEG2RAD));
			float y2 = cy + (screenHeight * Math.Sin(angle2 * Math.DEG2RAD));

			canvas.DrawLine(x1, y1, x2, y2, width, color);
		}
	}
}
#endif

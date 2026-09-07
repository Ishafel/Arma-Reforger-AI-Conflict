// Ограниченный read-only поиск возле совместимого stock slot. Никаких
// удалений декораций или перемещения уже созданной техники.
class AICF_LogisticsSpawnGeometry
{
	static const int CANDIDATES = 26;
	static const float MAX_OFFSET_M = 18;
	static const int EXIT_STEPS = 6;

	// Сначала исходный transform и разворот, затем кольца 6/12/18 м по
	// восемь направлений. Порядок зависит только от transform штатного slot.
	static bool Candidate(BaseWorld world, AICF_LogisticsVehicleFootprint footprint, vector slot[4], int index, out vector pose[4])
	{
		if (!world || !footprint || !footprint.m_bValid || index < 0 || index >= CANDIDATES) return false;
		for (int axis; axis < 4; axis++) pose[axis] = slot[axis];
		if (index == 0) return FitToSurface(world, footprint, pose);
		if (index == 1)
		{
			pose[0] = -slot[0];
			pose[2] = -slot[2];
			return FitToSurface(world, footprint, pose);
		}
		int ring = (index - 2) / 8 + 1;
		int bearing = (index - 2) % 8;
		float angle = bearing * 45 * Math.DEG2RAD;
		vector forward = slot[2] * Math.Cos(angle) + slot[0] * Math.Sin(angle);
		forward[1] = 0;
		if (forward.Length() < 0.5) return false;
		forward.Normalize();
		vector position = slot[3] + forward * (ring * 6);
		float x = position[0];
		float z = position[2];
		vector up = Vector(world.GetSurfaceY(x - 1, z) - world.GetSurfaceY(x + 1, z), 2,
			world.GetSurfaceY(x, z - 1) - world.GetSurfaceY(x, z + 1));
		up.Normalize();
		forward[1] = -(up[0] * forward[0] + up[2] * forward[2]) / up[1];
		forward.Normalize();
		Math3D.DirectionAndUpMatrix(forward, up, pose);
		pose[3] = position;
		pose[3][1] = world.GetSurfaceY(x, z);
		return FitToSurface(world, footprint, pose);
	}

	// Проверяем землю/воду под четырьмя углами реального OBB. Небольшой
	// подъём предотвращает старт колёс под землёй, большой зазор запрещён.
	static bool FitToSurface(BaseWorld world, AICF_LogisticsVehicleFootprint footprint, inout vector pose[4])
	{
		float lowest = float.MAX;
		float highest = -float.MAX;
		for (int corner; corner < 4; corner++)
		{
			float x = footprint.m_vMin[0];
			float z = footprint.m_vMin[2];
			if (corner % 2 == 1) x = footprint.m_vMax[0];
			if (corner >= 2) z = footprint.m_vMax[2];
			vector point = pose[3] + pose[0] * x + pose[1] * footprint.m_vMin[1] + pose[2] * z;
			float ground = world.GetSurfaceY(point[0], point[2]);
			vector waterPoint = Vector(point[0], ground, point[2]);
			if (ChimeraWorldUtils.TryGetWaterSurfaceSimple(world, waterPoint)) return false;
			float correction = ground - point[1];
			lowest = Math.Min(lowest, correction);
			highest = Math.Max(highest, correction);
		}
		if (highest - lowest > 0.6 || Math.AbsFloat(highest) > 1.5) return false;
		pose[3][1] = pose[3][1] + highest + 0.08;
		return true;
	}

	static bool ExitClear(BaseWorld world, AICF_LogisticsVehicleFootprint footprint, vector pose[4], out vector endpoint, out TraceOBB trace)
	{
		if (!world || !footprint || !footprint.m_bValid) return false;
		float distance = Math.Max(12, footprint.m_vMax[2] - footprint.m_vMin[2] + 4);
		if (distance > 18) return false;
		vector next[4];
		for (int axis; axis < 4; axis++) next[axis] = pose[axis];
		vector previous = pose[3];
		for (int step = 1; step <= EXIT_STEPS; step++)
		{
			next[3] = pose[3] + pose[2] * (distance * step / EXIT_STEPS);
			if (!FitToSurface(world, footprint, next)) return false;
			if (!footprint.IsClear(world, next, trace)) return false;
			trace.Start = previous;
			trace.End = next[3];
			if (world.TraceMove(trace, null) < 1) return false;
			previous = next[3];
		}
		endpoint = previous;
		return true;
	}
}

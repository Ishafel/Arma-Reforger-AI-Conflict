// Пустой диапазон не передаётся native RandomGenerator. Каждый boundary
// сохраняет собственную семантику и пишет caller для последующего soak.
modded class SCR_AICalculateNextCombatMovePos
{
	override protected vector RandomizeDestinationPos(float distance, vector centerPos)
	{
		if (distance <= 0)
		{
			Print("[AICF][AI_RANDOM_RANGE_REJECTED] caller=SCR_AICalculateNextCombatMovePos distance=" + distance, LogLevel.WARNING);
			return centerPos;
		}
		return super.RandomizeDestinationPos(distance, centerPos);
	}
}

modded class SCR_AIGetRandomLookPosition
{
	override ENodeResult EOnTaskSimulate(AIAgent owner, float dt)
	{
		IEntity entity = owner.GetControlledEntity();
		vector position;
		if (entity && GetVariableIn(PORT_IN_POS, position) &&
			vector.Distance(entity.GetOrigin(), position) * m_fTangent <= 0)
		{
			Print("[AICF][AI_RANDOM_RANGE_REJECTED] caller=SCR_AIGetRandomLookPosition tangent=" + m_fTangent, LogLevel.WARNING);
			SetVariableOut(PORT_OUT_POS, position);
			return ENodeResult.SUCCESS;
		}
		return super.EOnTaskSimulate(owner, dt);
	}
}

modded class SCR_AIGetRandomPointWithExclude
{
	override protected bool FindPosition2D(out vector randomPos, vector randomSphereOrigin, float randomSphereRadius,
		vector excludeSphereOrigin = vector.Zero, float excludeRadius = 0, int iterationCount = 50)
	{
		if (randomSphereRadius <= 0 || ((randomSphereOrigin == excludeSphereOrigin || excludeRadius < 1.0e-8) &&
			excludeRadius >= randomSphereRadius))
		{
			Print(string.Format("[AICF][AI_RANDOM_RANGE_REJECTED] caller=SCR_AIGetRandomPointWithExclude min=%1 max=%2",
				excludeRadius, randomSphereRadius), LogLevel.WARNING);
			randomPos = vector.Zero;
			return false;
		}
		return super.FindPosition2D(randomPos, randomSphereOrigin, randomSphereRadius, excludeSphereOrigin, excludeRadius, iterationCount);
	}
}

modded class SCR_AIGetRandomPoint
{
	override ENodeResult EOnTaskSimulate(AIAgent owner, float dt)
	{
		float radius = m_Radius;
		IEntity entity;
		if (GetVariableIn(WAYPOINT_PORT, entity))
		{
			AIWaypoint waypoint = AIWaypoint.Cast(entity);
			if (!waypoint)
				return super.EOnTaskSimulate(owner, dt);
			radius = waypoint.GetCompletionRadius();
		}
		else if (GetVariableType(true, RADIUS_PORT) == int)
		{
			int inputRadius;
			GetVariableIn(RADIUS_PORT, inputRadius);
			radius = inputRadius;
		}
		else if (GetVariableType(true, RADIUS_PORT) == float)
			GetVariableIn(RADIUS_PORT, radius);
		if (radius <= m_ExclusionRadius || radius <= 0)
		{
			Print(string.Format("[AICF][AI_RANDOM_RANGE_REJECTED] caller=SCR_AIGetRandomPoint min=%1 max=%2",
				m_ExclusionRadius, radius), LogLevel.WARNING);
			return ENodeResult.FAIL;
		}
		return super.EOnTaskSimulate(owner, dt);
	}
}

// Геометрия подхода к BASE. Entity и waypoint lifecycle остаются у planner/slot.
class AICF_InfantryApproachRoute
{
	protected SCR_CampaignMilitaryBaseComponent m_Target;
	protected EntityID m_TargetId;
	protected vector m_vOrigin;
	protected bool m_bDisabled;
	protected int m_iTileWaitStartedAtMs;

	void AICF_InfantryApproachRoute(SCR_CampaignMilitaryBaseComponent target, vector origin)
	{
		m_Target = target;
		m_TargetId = target.GetOwner().GetID();
		m_vOrigin = origin;
	}

	bool IsFor(SCR_CampaignMilitaryBaseComponent target)
	{
		return target && target == m_Target && target.GetOwner() && target.GetOwner().GetID() == m_TargetId;
	}

	void Disable()
	{
		m_bDisabled = true;
	}

	bool IsWaitingForTile()
	{
		return !m_bDisabled && m_iTileWaitStartedAtMs > 0;
	}

	// Чередование левого/правого фланга не зависит от role-local имени и tick.
	static float GetLaneOffset(int numericSlot)
	{
		int pair = numericSlot / 2;
		float offset = 40.0 + pair * 40.0;
		if (numericSlot % 2 == 0)
			return -offset;
		return offset;
	}

	bool BuildCandidate(vector leaderPosition, vector objective, int numericSlot, out vector candidate)
	{
		candidate = objective;
		if (m_bDisabled || numericSlot < 0 || vector.DistanceXZ(leaderPosition, objective) <= 250.0)
			return false;
		vector direction = objective - m_vOrigin;
		direction[1] = 0;
		float length = direction.Length();
		if (length <= 300.0)
			return false;
		direction = direction / length;
		vector fromOrigin = leaderPosition - m_vOrigin;
		float progress = fromOrigin[0] * direction[0] + fromOrigin[2] * direction[2];
		if (progress >= length - 250.0)
			return false;
		float nextProgress = Math.Min(length - 180.0, Math.Max(0, progress) + 220.0);
		vector lateral = Vector(-direction[2], 0, direction[0]);
		float offset = GetLaneOffset(numericSlot);
		float maxOffset = Math.Min(200.0, length * 0.2);
		offset = Math.Clamp(offset, -maxOffset, maxOffset);
		candidate = m_vOrigin + direction * nextProgress + lateral * offset;
		return vector.DistanceXZ(candidate, objective) + 40.0 < vector.DistanceXZ(leaderPosition, objective);
	}

	bool TryResolve(SCR_AIGroup group, int numericSlot, vector objective, out vector endpoint, out bool pending)
	{
		pending = false;
		endpoint = objective;
		IEntity leader = AICF_GroupRuntime.ResolveAliveLeader(group);
		vector candidate;
		if (!leader || !BuildCandidate(leader.GetOrigin(), objective, numericSlot, candidate))
			return false;
		AIPathfindingComponent pathfinding = AIPathfindingComponent.Cast(group.FindComponent(AIPathfindingComponent));
		if (!pathfinding)
			return false;
		NavmeshWorldComponent navmesh = pathfinding.GetNavmeshComponent();
		BaseWorld world = GetGame().GetWorld();
		if (!navmesh || !world)
			return false;
		candidate[1] = world.GetSurfaceY(candidate[0], candidate[2]);
		bool waitingForTile;
		if (!navmesh.IsTileLoaded(candidate))
		{
			if (m_iTileWaitStartedAtMs == 0)
				m_iTileWaitStartedAtMs = System.GetTickCount();
			if (System.GetTickCount(m_iTileWaitStartedAtMs) >= 30000)
			{
				Disable();
				return false;
			}
			if (!navmesh.IsTileRequested(candidate) && !navmesh.LoadTileIn(candidate))
				return false;
			// Пока грузится следующая полоса, держим действующий Move на проверенной
			// navmesh у живого leader. READY slot не остаётся без meaningful task.
			waitingForTile = true;
			candidate = leader.GetOrigin();
			candidate[1] = world.GetSurfaceY(candidate[0], candidate[2]);
			if (!navmesh.IsTileLoaded(candidate))
			{
				pending = true;
				return false;
			}
		}
		else
			m_iTileWaitStartedAtMs = 0;
		// Проекция не должна схлопывать соседние полосы в одну общую точку.
		if (!pathfinding.GetClosestPositionOnNavmesh(candidate, "8 5 8", endpoint) ||
			vector.DistanceSqXZ(candidate, endpoint) > 64.0 ||
			Math.AbsFloat(endpoint[1] - candidate[1]) > 3.0 ||
			ChimeraWorldUtils.TryGetWaterSurfaceSimple(world, endpoint - "0 0.5 0") ||
			(!waitingForTile && vector.DistanceXZ(endpoint, objective) + 30.0 >= vector.DistanceXZ(leader.GetOrigin(), objective)))
		{
			Disable();
			AICF_Stage2Diagnostics.Info("INFANTRY_APPROACH_FALLBACK",
				string.Format("numeric_slot=%1 target=%2 reason=NO_SAFE_LANE_ENDPOINT", numericSlot, AICF_Stage1Diagnostics.BaseKey(m_Target)));
			return false;
		}
		return true;
	}
}

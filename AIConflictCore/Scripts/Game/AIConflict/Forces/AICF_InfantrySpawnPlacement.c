// Проверка конечной позиции каждого бойца в момент обработки stock spawn queue.
// Применяется только к группам, явно созданным AICF_GroupSpawner.
class AICF_InfantrySpawnPlacement
{
	static bool IsUsable(SCR_AIGroup group, vector query, out vector position)
	{
		if (!group || !GetGame())
			return false;
		BaseWorld world = group.GetWorld();
		AIPathfindingComponent path = AIPathfindingComponent.Cast(group.FindComponent(AIPathfindingComponent));
		if (!world || !path || !path.GetNavmeshComponent())
			return false;
		NavmeshWorldComponent navmesh = path.GetNavmeshComponent();
		query[1] = world.GetSurfaceY(query[0], query[2]);
		if (!navmesh.IsTileLoaded(query))
		{
			navmesh.LoadTileIn(query);
			return false;
		}
		if (!path.GetClosestPositionOnNavmesh(query, "1 2 1", position) ||
			vector.DistanceSqXZ(query, position) > 2 ||
			Math.AbsFloat(position[1] - world.GetSurfaceY(position[0], position[2])) > 1 ||
			ChimeraWorldUtils.TryGetWaterSurfaceSimple(world, position))
			return false;
		position[1] = Math.Max(position[1], world.GetSurfaceY(position[0], position[2]));
		// Повторяем конечную stock проекцию: super не должен выбрать другой полигон.
		vector stockPosition;
		if (!path.GetClosestPositionOnNavmesh(position, "10 10 10", stockPosition) ||
			vector.Distance(position, stockPosition) > 0.1)
			return false;
		TraceBox body = new TraceBox();
		body.Start = position + "0 0.15 0";
		body.End = body.Start;
		body.Mins = "-0.4 0 -0.4";
		body.Maxs = "0.4 1.8 0.4";
		body.Flags = TraceFlags.ENTS | TraceFlags.WORLD | TraceFlags.OCEAN;
		body.LayerMask = EPhysicsLayerPresets.Projectile;
		body.Exclude = group;
		if (world.TracePosition(body, null) < 0)
			return false;
		// Свободный объём внутри закрытого контейнера недостаточен: нужен выход
		// с локального navmesh-полигона. Неуспешные queries не считаются проходом.
		for (int probe; probe < 8; probe++)
		{
			vector reachable;
			if (navmesh.GetReachablePoint(position, 40, reachable) &&
				vector.DistanceXZ(position, reachable) >= 25)
				return true;
		}
		return false;
	}

	static bool FindPosition(SCR_AIGroup group, vector requested, out vector position)
	{
		if (IsUsable(group, requested, position))
			return true;
		// Одинаковый ограниченный порядок для обеих фракций и любой карты.
		for (int ring = 1; ring <= 8; ring++)
		{
			for (int direction; direction < 16; direction++)
			{
				float angle = direction * 22.5 * Math.DEG2RAD;
				vector query = requested + Vector(Math.Cos(angle) * ring * 4, 0, Math.Sin(angle) * ring * 4);
				if (IsUsable(group, query, position))
					return true;
			}
		}
		return false;
	}
}

modded class SCR_AIGroup
{
	protected bool m_bAICFSafeInfantrySpawn;
	protected int m_iAICFPlacementRetryAt;

	void AICF_EnableSafeInfantrySpawn()
	{
		if (Replication.IsServer())
			m_bAICFSafeInfantrySpawn = true;
	}

	override protected bool SpawnGroupMember(bool snapToTerrain, int index, ResourceName res, bool editMode, bool isLast)
	{
		if (!m_bAICFSafeInfantrySpawn || !Replication.IsServer() || editMode)
			return super.SpawnGroupMember(snapToTerrain, index, res, editMode, isLast);
		if (System.GetTickCount() < m_iAICFPlacementRetryAt)
			return false;
		RplComponent rpl = RplComponent.Cast(FindComponent(RplComponent));
		if (!rpl || !rpl.IsMaster())
			return false;
		AIFormationComponent formation = AIFormationComponent.Cast(FindComponent(AIFormationComponent));
		vector offset = Vector(index, 0, 0);
		if (formation && formation.GetFormation())
			offset = formation.GetFormation().GetOffsetPosition(index);
		vector requested = CoordToParent(offset);
		vector position;
		if (!AICF_InfantrySpawnPlacement.FindPosition(this, requested, position))
		{
			m_iAICFPlacementRetryAt = System.GetTickCount() + 1000;
			AICF_Stage35Diagnostics.Warning("INFANTRY_SPAWN_PLACEMENT_DEFERRED",
				string.Format("group=%1 member=%2 requested=%3 reason=NO_SAFE_CONNECTED_POSITION", GetID(), index, requested));
			return false;
		}
		// Stock вычисляет formation offset внутри SpawnGroupMember. Временно
		// сдвигаем только пустой controller/anchor, сохраняя stock создание,
		// callbacks, roster identity и retry. Существующие бойцы не перемещаются.
		vector origin = GetOrigin();
		SetOrigin(origin + position - requested);
		bool spawned = super.SpawnGroupMember(false, index, res, editMode, isLast);
		SetOrigin(origin);
		if (spawned)
			AICF_Stage35Diagnostics.Info("INFANTRY_SPAWN_PLACEMENT",
				string.Format("group=%1 member=%2 requested=%3 selected=%4 shift_m=%5", GetID(), index, requested, position, vector.DistanceXZ(requested, position)));
		return spawned;
	}
}

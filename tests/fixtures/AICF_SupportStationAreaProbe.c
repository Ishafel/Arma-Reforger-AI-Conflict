// Isolated stage only: three real replicated repair trucks, with native init.
modded class AICF_MatchController
{
	protected bool m_bAICFAreaProbeSpawned;
	override protected void Update()
	{
		super.Update();
		if (!m_bRosterReady || m_bAICFAreaProbeSpawned || m_bStopped) return;
		m_bAICFAreaProbeSpawned = true;
		array<ResourceName> prefabs = {
			"{34178E3A92776DA6}Prefabs/Vehicles/Wheeled/Ural4320/Ural4320_repair_FIA.et",
			"{A5647958579A4149}Prefabs/Vehicles/Wheeled/Ural4320/Ural4320_repair.et",
			"{A042ACE5C2B13206}Prefabs/Vehicles/Wheeled/M923A1/M923A1_repair.et"
		};
		string key = "USSR";
		string factionKey;
		if (System.GetCLIParam("aicfBarracksFaction", factionKey)) key = factionKey;
		SCR_CampaignFaction faction = SCR_CampaignFaction.Cast(GetGame().GetFactionManager().GetFactionByKey(key));
		vector origin, angles;
		faction.GetMainBase().GetSpawnPoint().GetPositionAndRotation(origin, angles);
		foreach (int index, ResourceName prefab : prefabs)
		{
			EntitySpawnParams params = new EntitySpawnParams();
			params.TransformMode = ETransformMode.WORLD;
			Math3D.AnglesToMatrix(Vector(37 + index * 53, 0, 0), params.Transform);
			params.Transform[3] = origin + Vector(25 + index * 18, 0, -30);
			params.Transform[3][1] = GetGame().GetWorld().GetSurfaceY(params.Transform[3][0], params.Transform[3][2]) + 0.25;
			IEntity vehicle = GetGame().SpawnEntityPrefab(Resource.Load(prefab), null, params);
			Print(string.Format("[AREA_SPAWN] pass=%1 prefab=%2", vehicle != null, prefab));
		}
	}
}

modded class SCR_SupportStationAreaMeshComponent
{
	override protected void Init(IEntity owner)
	{
		SCR_BaseSupportStationComponent station = GetSupportStation(owner);
		ResourceName prefab;
		if (station) prefab = SCR_ResourceNameUtils.GetPrefabName(station.GetOwner());
		bool observed = prefab.Contains("/VehParts/") && prefab.EndsWith("repair_box.et");
		vector before[4], stationBefore[4];
		if (observed)
		{
			owner.GetLocalTransform(before);
			station.GetOwner().GetWorldTransform(stationBefore);
		}
		super.Init(owner);
		if (!observed) return;
		vector after[4], stationAfter[4];
		owner.GetLocalTransform(after);
		station.GetOwner().GetWorldTransform(stationAfter);
		bool stationUnchanged = true;
		for (int i; i < 4; i++) if (stationBefore[i] != stationAfter[i]) stationUnchanged = false;
		float distance = vector.Distance(owner.GetOrigin(), station.GetPosition());
		bool passed = owner.GetParent() == station.GetOwner() && after[3] == station.GetOffset() &&
			distance < 0.002 && stationUnchanged && GetRadius() == station.GetRange() && owner.GetVObject();
		Print(string.Format("[AREA_CASE] pass=%1 server=%2 before_mismatch=%3 station_unchanged=%4 center_error=%5 prefab=%6", passed, Replication.IsServer(), before[3] != station.GetOffset(), stationUnchanged, distance, prefab));
	}
}

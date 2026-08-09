// Chooses the nearest safe friendly Conflict base and uses the same stock empty
// terrain query as initial HQ vehicles. The caller reserves cap capacity before
// entering this class, so spawn validation and creation are atomic on authority.
class AICF_VehicleSpawner
{
	protected static const float SPAWN_SEARCH_RADIUS_METERS = 45.0;
	protected static const float VEHICLE_CLEARANCE_RADIUS_METERS = 8.0;

	bool TrySpawn(
		SCR_GameModeCampaign campaign,
		SCR_CampaignFaction faction,
		AICF_GroupSlot slot,
		AICF_ConflictAdapter conflictAdapter,
		ResourceName prefab,
		vector preferredPosition,
		float maximumSpawnDistanceMeters,
		AICF_VehicleRuntime runtime,
		out Vehicle vehicle,
		out SCR_AIVehicleUsageComponent vehicleUsage,
		out SCR_CampaignMilitaryBaseComponent spawnBase)
	{
		vehicle = null;
		vehicleUsage = null;
		spawnBase = null;
		if (!Replication.IsServer() || !campaign || !campaign.IsMaster() || !faction || !slot || !conflictAdapter || !runtime || prefab.IsEmpty())
			return false;

		array<SCR_CampaignMilitaryBaseComponent> candidates = {};
		SCR_CampaignMilitaryBaseComponent mainBase = faction.GetMainBase();
		if (mainBase)
			candidates.Insert(mainBase);

		SCR_CampaignMilitaryBaseManager baseManager = campaign.GetBaseManager();
		if (baseManager)
		{
			array<SCR_CampaignMilitaryBaseComponent> bases = {};
			baseManager.GetBases(bases);
			foreach (SCR_CampaignMilitaryBaseComponent base : bases)
			{
				if (base && !candidates.Contains(base))
					candidates.Insert(base);
			}
		}

		float bestDistanceSq = float.MAX;
		foreach (SCR_CampaignMilitaryBaseComponent candidate : candidates)
		{
			string rejectionReason = conflictAdapter.GetSpawnRejectionReason(candidate, faction);
			if (!rejectionReason.IsEmpty())
			{
				AICF_Stage3Diagnostics.Info(
					"VEHICLE_SPAWN_SITE_REJECTED",
					string.Format("%1 base=%2 reason=%3", runtime.DescribeContext(rejectionReason), AICF_Stage1Diagnostics.BaseKey(candidate), rejectionReason));
				continue;
			}

			float distanceSq = vector.DistanceSqXZ(candidate.GetOwner().GetOrigin(), preferredPosition);
			if (!spawnBase || distanceSq < bestDistanceSq)
			{
				spawnBase = candidate;
				bestDistanceSq = distanceSq;
			}
		}

		if (!spawnBase)
			return false;
		if (maximumSpawnDistanceMeters > 0 && bestDistanceSq > maximumSpawnDistanceMeters * maximumSpawnDistanceMeters)
		{
			AICF_Stage3Diagnostics.Info(
				"VEHICLE_SPAWN_SITE_REJECTED",
				string.Format("%1 base=%2 reason=TOO_FAR distance_m=%3 limit_m=%4", runtime.DescribeContext("TOO_FAR"), AICF_Stage1Diagnostics.BaseKey(spawnBase), Math.Sqrt(bestDistanceSq), maximumSpawnDistanceMeters));
			spawnBase = null;
			return false;
		}

		vector position;
		if (!SCR_WorldTools.FindEmptyTerrainPosition(
			position,
			spawnBase.GetOwner().GetOrigin(),
			SPAWN_SEARCH_RADIUS_METERS,
			VEHICLE_CLEARANCE_RADIUS_METERS))
		{
			AICF_Stage3Diagnostics.Info(
				"VEHICLE_SPAWN_SITE_REJECTED",
				string.Format("%1 base=%2 reason=NO_EMPTY_TERRAIN", runtime.DescribeContext("NO_EMPTY_TERRAIN"), AICF_Stage1Diagnostics.BaseKey(spawnBase)));
			spawnBase = null;
			return false;
		}

		AICF_Stage3Diagnostics.Info(
			"VEHICLE_SPAWN_SITE_SELECTED",
			string.Format("%1 base=%2 owner=%3 contested=0", runtime.DescribeContext("SAFE_FRIENDLY_BASE"), AICF_Stage1Diagnostics.BaseKey(spawnBase), faction.GetFactionKey()));

		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = position;
		vehicle = Vehicle.Cast(GetGame().SpawnEntityPrefabEx(prefab, false, params: params));
		if (!vehicle)
		{
			AICF_Stage3Diagnostics.Error("VEHICLE_SPAWN_FAILED", string.Format("%1 prefab=%2", runtime.DescribeContext("SPAWN_RETURNED_NULL"), prefab));
			return false;
		}

		SCR_FactionAffiliationComponent affiliation = SCR_FactionAffiliationComponent.Cast(vehicle.FindComponent(SCR_FactionAffiliationComponent));
		Faction vehicleFaction;
		if (affiliation)
			vehicleFaction = affiliation.GetAffiliatedFaction();
		if (!vehicleFaction || vehicleFaction.GetFactionKey() != faction.GetFactionKey())
		{
			AICF_Stage3Diagnostics.Error("VEHICLE_FACTION_MISMATCH", string.Format("%1 expected=%2 prefab=%3", runtime.DescribeContext("FACTION_MISMATCH"), faction.GetFactionKey(), prefab));
			RplComponent.DeleteRplEntity(vehicle, false);
			vehicle = null;
			return false;
		}

		vehicleUsage = SCR_AIVehicleUsageComponent.Cast(vehicle.FindComponent(SCR_AIVehicleUsageComponent));
		if (!vehicleUsage || !vehicleUsage.CanBePiloted() || !vehicleUsage.IsVehicleTypeValid())
		{
			AICF_Stage3Diagnostics.Error("VEHICLE_AI_USAGE_INVALID", string.Format("%1 prefab=%2", runtime.DescribeContext("AI_USAGE_INVALID"), prefab));
			RplComponent.DeleteRplEntity(vehicle, false);
			vehicle = null;
			vehicleUsage = null;
			return false;
		}

		Physics physicsComponent = vehicle.GetPhysics();
		if (physicsComponent)
			physicsComponent.SetVelocity("0 -0.1 0");

		return true;
	}
}

// Version-locked admission adapter: pinned Script Diff 1.8.0.13.
// Outside active authority quota, the complete original modded chain is used.
modded class SCR_AIGroup
{
	override protected bool SpawnGroupMember(bool snapToTerrain, int index, ResourceName res, bool editMode, bool isLast)
	{

		// AICF_QUOTA_BLOCK_BEGIN ENTRY
		AICF_AIWorldQuotaService quota = AICF_AIWorldQuotaService.GetAuthorityForWorld(GetWorld());
		if (editMode || !Replication.IsServer() || !quota)
			return super.SpawnGroupMember(snapToTerrain, index, res, editMode, isLast);
		// AICF_QUOTA_BLOCK_END ENTRY
		BaseWorld world = GetWorld();
		AIFormationDefinition formationDefinition;
		AIFormationComponent formationComponent = AIFormationComponent.Cast(this.FindComponent(AIFormationComponent));
		if (formationComponent)
			formationDefinition = formationComponent.GetFormation();
		EntitySpawnParams spawnParams = new EntitySpawnParams;
		spawnParams.TransformMode = ETransformMode.WORLD;
		GetWorldTransform(spawnParams.Transform);
		vector pos = spawnParams.Transform[3];

		if (formationDefinition)
			pos = CoordToParent(formationDefinition.GetOffsetPosition(index));
		else
			pos = CoordToParent(Vector(index, 0, 0));

		if (snapToTerrain)
		{
			float surfaceY = world.GetSurfaceY(pos[0], pos[2]);
			pos[1] = surfaceY;
		}


		//Snap to the nearest navmesh point
		AIPathfindingComponent pathFindindingComponent = AIPathfindingComponent.Cast(this.FindComponent(AIPathfindingComponent));

		if (!editMode)
		{
			NavmeshWorldComponent navmesh = pathFindindingComponent.GetNavmeshComponent();
			if (navmesh)
			{
				if (navmesh.IsTileRequested(pos))
				{
					m_iNavmeshStallFrames++;
					if (m_iNavmeshStallFrames < NAVMESH_STALL_LIMIT)
					{
						if (DBG_AI_LOGGING)
							Print(string.Format("[DBG_AI] SpawnGroupMember fail: navmesh tile REQUESTED at %1 stall=%2/%3 (group %4 slot %5)", pos, m_iNavmeshStallFrames, NAVMESH_STALL_LIMIT, this, index), LogLevel.WARNING);
						return false;
					}
					// Tile was requested but never loaded - no navmesh data at this position.
					// Give up waiting and let the spawn proceed.
					if (DBG_AI_LOGGING)
						Print(string.Format("[DBG_AI] SpawnGroupMember: navmesh stall limit reached at %1, proceeding without loaded tile (group %2 slot %3)", pos, this, index), LogLevel.WARNING);
				}
				else if (!navmesh.IsTileLoaded(pos))
				{
					// Call LoadTileIn to start async tile loading and count the attempt.
					// If the tile never transitions to IsTileRequested (e.g. navmesh is
					// pre-baked or not streamed on this server), LoadTileIn is a no-op and
					// IsTileRequested stays false indefinitely. Count these attempts with the
					// same stall counter so the give-up path fires after NAVMESH_STALL_LIMIT
					// calls, preventing an infinite retry loop on servers without tile streaming.
					m_iNavmeshStallFrames++;
					if (DBG_AI_LOGGING)
						Print(string.Format("[DBG_AI] SpawnGroupMember fail: navmesh tile NOT LOADED at %1 - LoadTileIn issued stall=%2/%3 (group %4 slot %5)", pos, m_iNavmeshStallFrames, NAVMESH_STALL_LIMIT, this, index), LogLevel.WARNING);
					navmesh.LoadTileIn(pos);
					if (m_iNavmeshStallFrames < NAVMESH_STALL_LIMIT)
						return false;
					// Stall limit reached without the tile loading - proceed without navmesh snap.
					if (DBG_AI_LOGGING)
						Print(string.Format("[DBG_AI] SpawnGroupMember: LoadTileIn stall limit reached at %1, proceeding without loaded tile (group %2 slot %3)", pos, this, index), LogLevel.WARNING);
				}
			}
		}

		if (pathFindindingComponent && pathFindindingComponent.GetClosestPositionOnNavmesh(pos, "10 10 10", pos))
		{
			float groundHeight = world.GetSurfaceY(pos[0], pos[2]);
			if (pos[1] < groundHeight)
				pos[1] = groundHeight;
			vector outWaterSurfacePoint;
			EWaterSurfaceType waterSurfaceType;
			vector transformWS[4];
			vector obbExtents;
			if (ChimeraWorldUtils.TryGetWaterSurface(GetWorld(), pos, outWaterSurfacePoint, waterSurfaceType, transformWS, obbExtents))
			{
				pos = outWaterSurfacePoint;
			}
		}

		spawnParams.Transform[3] = pos;

		// AICF_QUOTA_BLOCK_BEGIN BEGIN_FACTORY
		AICF_AIQuotaBirth quotaBirth = quota.BeginGroupBirth(this, index, res);
		if (!quotaBirth) return false;
		// AICF_QUOTA_BLOCK_END BEGIN_FACTORY
		IEntity member = GetGame().SpawnEntityPrefabEx(res, true, world, spawnParams);
		// AICF_QUOTA_BLOCK_BEGIN OBSERVE_FACTORY
		quota.ObserveGroupFactory(quotaBirth, member);
		if (quotaBirth.m_Status == AICF_EAIQuotaBirthStatus.NATIVE_NULL)
		{
			quota.EndGroupBirth(quotaBirth);
			return true;
		}
		if (quotaBirth.m_Status != AICF_EAIQuotaBirthStatus.BOUND)
			return AICF_RejectQuotaBirth(quota, quotaBirth, quotaBirth.m_sRejectReason);
		// AICF_QUOTA_BLOCK_END OBSERVE_FACTORY
		if (!member)
			return true;

		// Move in to vehicle
		SCR_EditableEntityComponent editableEntity = SCR_EditableEntityComponent.Cast(member.FindComponent(SCR_EditableEntityComponent));


		if (editMode)
			m_aSceneGroupUnitInstances.Insert(member);

		// AICF_QUOTA_BLOCK_BEGIN NONNULL_DISAPPEARED
		if (!member)
			return AICF_RejectQuotaBirth(quota, quotaBirth, "NONNULL_BODY_DISAPPEARED");
		// AICF_QUOTA_BLOCK_END NONNULL_DISAPPEARED
		// Even same null-check is above, in some situations, member can get deleted and it would result in VME
		if (!member)
			return true;

		// AICF_QUOTA_BLOCK_BEGIN PRE_ADD
		if (!quota.IsGroupBirthCurrent(quotaBirth, false))
			return AICF_RejectQuotaBirth(quota, quotaBirth, "PRE_ADD_OWNER_CHANGED");
		// AICF_QUOTA_BLOCK_END PRE_ADD
		AddAIEntityToGroup(member);
		// AICF_QUOTA_BLOCK_BEGIN POST_ADD
		if (!quota.IsGroupBirthCurrent(quotaBirth, true))
			return AICF_RejectQuotaBirth(quota, quotaBirth, "POST_ADD_OWNER_CHANGED");
		// AICF_QUOTA_BLOCK_END POST_ADD

		FactionAffiliationComponent factionAffiliation = FactionAffiliationComponent.Cast(member.FindComponent(FactionAffiliationComponent));

		if (factionAffiliation)
			factionAffiliation.SetAffiliatedFactionByKey(m_faction);

		// AICF_QUOTA_BLOCK_BEGIN PRE_EVENT
		if (!quota.IsGroupBirthCurrent(quotaBirth, true))
			return AICF_RejectQuotaBirth(quota, quotaBirth, "POST_FACTION_OWNER_CHANGED");
		// AICF_QUOTA_BLOCK_END PRE_EVENT
		if (isLast)
			InvokeEventOnInit();
		// AICF_QUOTA_BLOCK_BEGIN FINISH
		if (!quota.CompleteGroupBirth(quotaBirth)) return false;
		// AICF_QUOTA_BLOCK_END FINISH
		return true;

	}

	// Rollback belongs to the creation adapter, never to quota arithmetic. Only
	// a cancelled owner's exact newborn, still unassigned/nonplayer/out of any
	// vehicle, is eligible. No actor is deleted to fit a full quota.
	protected bool AICF_RejectQuotaBirth(AICF_AIWorldQuotaService quota, AICF_AIQuotaBirth birth, string reason)
	{
		if (quota.ObserveBirthAbsence(birth)) return false;
		if (birth && birth.m_Queue && birth.m_Queue.m_Group == this && quota.CanRollbackGroupBirth(birth))
		{
			IEntity created = birth.m_Entity;
			if (quota.CanRollbackGroupBirth(birth) && birth.m_Body.MatchesIdentity())
				RplComponent.DeleteRplEntity(created, false);
			if (quota.ObserveBirthAbsence(birth)) return false;
		}
		quota.RejectNonNullBirth(birth, reason);
		return false;
	}
}

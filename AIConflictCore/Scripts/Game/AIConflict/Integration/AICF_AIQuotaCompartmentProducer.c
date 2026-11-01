// Pinned1.8.0.13 BaseCompartmentSlot.SpawnCharacterInCompartment:
// отдельный producer до group membership, без native GetInVehicle override.
modded class BaseCompartmentSlot
{
	override IEntity SpawnCharacterInCompartment(ResourceName characterPrefab, inout AIGroup group,
		ResourceName groupPrefab = "{000CD338713F2B5A}Prefabs/AI/Groups/Group_Base.et")
	{
		AICF_AIWorldQuotaService quota;
		if (GetGame() && Replication.IsServer()) quota = AICF_AIWorldQuotaService.GetAuthorityForWorld(GetGame().GetWorld());
		if (!quota || !GetOwner() || GetOwner().GetWorld() != GetGame().GetWorld())
			return super.SpawnCharacterInCompartment(characterPrefab, group, groupPrefab);
		if (characterPrefab.IsEmpty() || !IsCompartmentAccessible() || GetOccupant()) return null;
		AIGroup capturedGroup = group;
		AICF_AIQuotaCompartmentCreation context = quota.BeginCompartmentCreation(this, characterPrefab, capturedGroup, groupPrefab);
		if (!context) return null;
		EntitySpawnParams params = new EntitySpawnParams();
		GetOwner().GetTransform(params.Transform);
		ChimeraWorld world = GetGame().GetWorld();
		// Сохраняем pinned expression без изменения native spawn position.
		params.Transform[3] + SPAWN_IN_VEHICLE_OFFSET;
		Resource resource = Resource.Load(characterPrefab);
		AICF_AIQuotaCreation creation = quota.BeginCompartmentFactory(context);
		if (!creation)
		{
			quota.CloseCompartmentCreation(context);
			return null;
		}
		if (!resource || !context.IsCurrent() || group != capturedGroup || !IsCompartmentAccessible() || GetOccupant() ||
			!quota.MarkExternalNativeCallStarted(creation))
		{
			quota.CancelExternalCreationBeforeFactory(creation);
			quota.CloseCompartmentCreation(context);
			return null;
		}
		IEntity spawnedCharacter = GetGame().SpawnEntityPrefab(resource, null, params);
		quota.ObserveExternalFactory(creation, spawnedCharacter);
		if (!spawnedCharacter)
		{
			quota.EndExternalCreation(creation);
			quota.CloseCompartmentCreation(context);
			return null;
		}
		if (!quota.CaptureCompartmentNewborn(context, spawnedCharacter))
		{
			quota.QuarantineCompartmentCreation(context, "NEWBORN_COMPARTMENT_FACTORY_IDENTITY_UNKNOWN");
			quota.CloseCompartmentCreation(context);
			return null;
		}
		AIControlComponent control = AIControlComponent.Cast(spawnedCharacter.FindComponent(AIControlComponent));
		if (!control || !quota.OwnsCurrentCompartmentNewborn(context))
			return AICF_QuotaAbortNewborn(context, spawnedCharacter);
		control.ActivateAI();
		CompartmentAccessComponent access = CompartmentAccessComponent.Cast(spawnedCharacter.FindComponent(CompartmentAccessComponent));
		IEntity seatOwner = GetOwner();
		bool performWhenPaused = world.IsGameTimePaused();
		if (!access || !quota.OwnsCurrentCompartmentNewborn(context) || group != capturedGroup ||
			seatOwner != context.m_SeatScope.m_SeatOwnerIdentity.m_Entity || !IsCompartmentAccessible() || GetOccupant() ||
			!quota.OwnsCurrentCompartmentNewborn(context))
			return AICF_QuotaAbortNewborn(context, spawnedCharacter);
		// До входа никаких generic null-group Entry permissions не выдаётся:
		// currentCreation/Body/actor/car/seat должны совпасть с этим producer.
		if (!quota.MarkCompartmentEntryDispatched(context)) return AICF_QuotaAbortNewborn(context, spawnedCharacter);
		bool accepted = access.GetInVehicle(seatOwner, this, true, -1, ECloseDoorAfterActions.INVALID, performWhenPaused);
		quota.ObserveCompartmentEntry(context, accepted);
		if (!accepted) return AICF_QuotaAbortNewborn(context, spawnedCharacter);
		if (!quota.OwnsCurrentCompartmentNewborn(context) || group != capturedGroup || !quota.EndExternalCreation(creation))
		{
			quota.QuarantineCompartmentCreation(context, "NEWBORN_COMPARTMENT_POST_ENTRY_UNKNOWN");
			quota.CloseCompartmentCreation(context);
			return null;
		}
		if (!group)
		{
			// Только metadata-proven пустой header. Старый header>0 factory API
			// и его full basket остаются неизменными.
			if (!context.IsNewbornCurrent() || !AICF_AIQuotaCompartmentCreation.ReadEmptyGroupMetadata(groupPrefab))
			{
				quota.QuarantineCompartmentCreation(context, "NEWBORN_COMPARTMENT_GROUP_METADATA_STALE");
				quota.CloseCompartmentCreation(context);
				return null;
			}
			IEntity groupEntity = GetGame().SpawnEntityPrefab(Resource.Load(groupPrefab));
			if (!groupEntity)
			{
				quota.CloseCompartmentCreation(context);
				return spawnedCharacter;
			}
			SCR_AIGroup createdGroup = SCR_AIGroup.Cast(groupEntity);
			AICF_AIQuotaCatalogIdentity identity = AICF_AIQuotaCatalogIdentity.Capture(groupEntity, true);
			if (!createdGroup || !identity || !identity.IsCurrent() || createdGroup.CountTowardsAIWorldLimit() ||
				createdGroup.GetAgentsCount() != 0 || createdGroup.GetPlayerCount() != 0 || !createdGroup.m_aUnitPrefabSlots ||
				!createdGroup.m_aUnitPrefabSlots.IsEmpty() || SCR_ResourceNameUtils.GetPrefabName(groupEntity) != groupPrefab ||
				!context.IsNewbornCurrent() || group != capturedGroup)
			{
				quota.QuarantineCompartmentCreation(context, "NEWBORN_COMPARTMENT_GROUP_FACTORY_UNKNOWN");
				quota.CloseCompartmentCreation(context);
				return null;
			}
			createdGroup.InitFactionKey(context.m_sFaction);
			if (!identity.IsCurrent() || !createdGroup.GetFaction() || createdGroup.GetFaction().GetFactionKey() != context.m_sFaction ||
				createdGroup.GetAgentsCount() != 0 || !context.IsNewbornCurrent() || group != capturedGroup)
			{
				quota.QuarantineCompartmentCreation(context, "NEWBORN_COMPARTMENT_GROUP_FACTION_UNKNOWN");
				quota.CloseCompartmentCreation(context);
				return null;
			}
			context.m_NewGroupIdentity = identity;
			group = createdGroup;
		}
		AIGroup destination = group;
		AIAgent agent = control.GetControlAIAgent();
		if (!destination || !agent || !context.IsNewbornCurrent() || agent.GetControlledEntity() != spawnedCharacter || agent.GetParentGroup() ||
			!context.IsDestinationCurrent(destination) || !context.IsNewbornCurrent())
		{
			quota.QuarantineCompartmentCreation(context, "NEWBORN_COMPARTMENT_BEFORE_GROUP_BIND_UNKNOWN");
			quota.CloseCompartmentCreation(context);
			return null;
		}
		destination.AddAgent(agent);
		quota.CloseCompartmentCreation(context);
		return spawnedCharacter;
	}

	protected IEntity AICF_QuotaAbortNewborn(AICF_AIQuotaCompartmentCreation context, IEntity actor)
	{
		if (!context || !context.m_Service) return null;
		AICF_AIWorldQuotaService service = context.m_Service;
		service.CloseCompartmentCreation(context);
		AICF_AIQuotaCreation creation = context.m_Creation;
		if (creation && creation.m_Entity == actor && service.CanRollbackExternalCreation(creation))
		{
			RplComponent.DeleteRplEntity(actor, false);
			if (service.ObserveExternalAbsence(creation)) return null;
		}
		service.QuarantineCompartmentCreation(context, "NEWBORN_COMPARTMENT_ABORT_IDENTITY_UNPROVEN");
		return null;
	}
}

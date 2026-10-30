// Единственный world-scoped владелец quota units. Поднятие native floor будет
// подключено composition root только вместе с guarded producer adapters.
class AICF_AIWorldQuotaService
{
	protected SCR_AIWorld m_World;
	protected AICF_AIQuotaWorldState m_WorldState;
	protected AICF_MatchController m_Controller;
	protected int m_iEpoch;
	protected int m_iNextContext = 1;
	protected ref AICF_AIQuotaLimits m_Limits;
	protected ref AICF_AIQuotaLedger m_Ledger;
	protected ref AICF_AIQuotaRegistry m_Owners;
	protected ref AICF_AIQuotaCensus m_Census;
	protected ref array<ref AICF_AIQuotaGroupQueue> m_aQueues = {};
	protected ref array<ref AICF_AIQuotaBody> m_aBodies = {};
	protected ref array<ref AICF_AIQuotaPlannedGroup> m_aPlannedGroups = {};
	protected bool m_bNativeBirthBusy;
	protected bool m_bQuotaCensusBusy;
	protected ref AICF_AIQuotaBirth m_CurrentBirth;
	protected ref AICF_AIQuotaCreation m_CurrentCreation;
	protected ref AICF_AIQuotaBirth m_QuarantinedBirth;
	protected ref AICF_AIQuotaCreation m_QuarantinedCreation;
	protected bool m_bQuarantined;
	protected bool m_bPolicyActive;
	protected bool m_bNativeFloorInstalled;
	protected int m_iInstalledNativeFloor;
	protected string m_sLastReason = "NOT_INITIALIZED";
	protected string m_sLastCensusReport;
	protected int m_iCapturedNativeRequests;
	protected int m_iCapturedNativeWaitingUnits;

	// Declared on the owning base layer so calls below dispatch into the typed
	// vehicle extension. Missing adapter is a strict veto, never zero occupancy.
	protected int BeginQuotaVehicleBootstrap(AICF_MatchController controller) { return 0; }
	protected bool EndQuotaVehicleBootstrap(AICF_MatchController controller, int serial) { return false; }
	protected bool ClassifyQuotaVehicleRow(AICF_AIQuotaAgentRow row, out AICF_EAIQuotaBucket bucket, out bool charge)
	{
		bucket = AICF_EAIQuotaBucket.STOCK_VEHICLES;
		charge = false;
		return false;
	}
	protected bool AddQuotaVehicleHolds(AICF_AIQuotaCensus snapshot, array<int> counts, array<IEntity> counted) { return false; }
	protected bool InitializeQuotaVehicleBindings(AICF_AIQuotaCensus snapshot) { return false; }

	void AICF_AIWorldQuotaService(SCR_AIWorld world, AICF_AIQuotaWorldState worldState)
	{
		m_World = world;
		m_WorldState = worldState;
		m_Owners = new AICF_AIQuotaRegistry(worldState);
	}

	static AICF_AIWorldQuotaService GetAuthorityForWorld(BaseWorld world)
	{
		if (!Replication.IsServer() || !GetGame() || !world || GetGame().GetWorld() != world) return null;
		SCR_AIWorld aiWorld = SCR_AIWorld.Cast(GetGame().GetAIWorld());
		if (!aiWorld || aiWorld.GetWorld() != world) return null;
		AICF_AIWorldQuotaService service = aiWorld.AICF_FindQuotaService();
		if (!service || !service.IsPolicyActive()) return null;
		return service;
	}

	bool IsCurrentWorld()
	{
		return Replication.IsServer() && GetGame() && m_World && m_WorldState && m_WorldState.IsCurrentWorld() &&
			GetGame().GetAIWorld() == m_World && m_World.AICF_FindQuotaService() == this;
	}

	bool IsOwnerCurrent()
	{
		return IsCurrentWorld() && m_Controller && m_WorldState.IsSessionCurrent(m_Controller, m_iEpoch);
	}

	bool IsConfigured()
	{
		return IsCurrentWorld() && m_Limits && m_Limits.IsConfigured() && m_Ledger && !m_bQuarantined;
	}

	// Published only after successful complete Initialize. Census failure/Stop
	// never switch an already-active policy back to vanilla increased headroom.
	bool IsPolicyActive() { return m_bPolicyActive; }
	bool IsQuotaAdmissionBusy() { return m_bNativeBirthBusy || m_bQuotaCensusBusy; }

	static bool IsPreparingForWorld(BaseWorld world)
	{
		if (!Replication.IsServer() || !GetGame() || world != GetGame().GetWorld()) return false;
		SCR_AIWorld nativeWorld = SCR_AIWorld.Cast(GetGame().GetAIWorld());
		AICF_MatchController controller = AICF_MatchController.GetActiveController();
		return nativeWorld && controller && nativeWorld.GetWorld() == world && nativeWorld.AICF_FindQuotaWorldState() &&
			controller.IsAIQuotaOwnerCurrent(nativeWorld);
	}

	bool InstallNativeFloor()
	{
		if (!IsPolicyActive() || !IsOwnerCurrent() || !IsConfigured() || IsQuotaAdmissionBusy() || !RefreshCensus()) return false;
		int previous = m_World.GetLimitOfActiveAIs();
		int required = m_Limits.GetTotalLimit();
		if (m_bNativeFloorInstalled)
		{
			if (required != m_iInstalledNativeFloor || previous < m_iInstalledNativeFloor)
			{
				m_sLastReason = "NATIVE_FLOOR_EXTERNALLY_LOWERED_OR_CHANGED";
				return false;
			}
			return true;
		}
		if (previous < required) m_World.SetLimitOfActiveAIs(required);
		int effective = m_World.GetLimitOfActiveAIs();
		AICF_Stage35Diagnostics.Info("AI_WORLD_CAPACITY", string.Format(
			"previous_limit=%1 required_limit=%2 effective_limit=%3 changed=%4 separate_quotas=1 original_native=%5",
			previous, required, effective, previous != effective, m_WorldState.GetOriginalNativeLimit()));
		if (!IsOwnerCurrent() || effective < required) return false;
		m_iInstalledNativeFloor = required;
		m_bNativeFloorInstalled = true;
		return true;
	}

	string GetLastReason() { return m_sLastReason; }
	AICF_AIQuotaRegistry GetOwners() { return m_Owners; }
	AICF_AIQuotaLimits GetLimits() { return m_Limits; }
	AICF_AIQuotaLedger GetLedger() { return m_Ledger; }
	AICF_MatchController GetQuotaController() { return m_Controller; }
	int GetEpoch() { return m_iEpoch; }

	string BudgetDetails(AICF_EAIQuotaBucket bucket, int requested)
	{
		if (!IsCurrentWorld() || !m_Limits || !m_Ledger || bucket < 0 || bucket >= AICF_EAIQuotaBucket.COUNT)
			return "quota_context=UNKNOWN";
		return string.Format("bucket=%1 physical=%2 reserved=%3 bucket_limit=%4 requested=%5 native_active=%6 native_intended=%7 native_hard=%8 original_native=%9",
			bucket, m_Ledger.GetPhysical(bucket), m_Ledger.GetReserved(bucket), m_Limits.GetLimit(bucket), requested,
			m_World.GetCurrentNumOfActiveAIs(), m_World.GetActiveGroupIntendedCount(), m_World.GetLimitOfActiveAIs(), m_WorldState.GetOriginalNativeLimit());
	}

	int AllocateContextSerial()
	{
		if (!IsConfigured() || IsQuotaAdmissionBusy() || m_iNextContext <= 0 || m_iNextContext == int.MAX) return 0;
		int result = m_iNextContext;
		m_iNextContext++;
		return result;
	}

	protected AICF_AIQuotaBody FindBody(IEntity entity)
	{
		if (!entity) return null;
		foreach (AICF_AIQuotaBody body : m_aBodies)
			if (body && !body.m_bReleased && body.m_Entity == entity) return body;
		return null;
	}

	protected bool ClassifyRow(AICF_AIQuotaAgentRow row, out AICF_EAIQuotaBucket bucket)
	{
		bucket = AICF_EAIQuotaBucket.STOCK;
		if (!row || !m_Owners) return false;
		AICF_AIQuotaBody body = FindBody(row.m_Entity);
		if (body)
		{
			if (!body.MatchesIdentity() || body.m_bQuarantined) return false;
			if (!body.m_bVehicle)
				return ClassifyCapturedActor(body, SCR_AIGroup.Cast(row.m_Group), bucket);
			bucket = body.m_Bucket;
			return bucket >= 0 && bucket < AICF_EAIQuotaBucket.COUNT;
		}
		if (row.m_bVehicle)
		{
			bucket = AICF_EAIQuotaBucket.STOCK_VEHICLES;
			return true;
		}
		return m_Owners.ClassifyGroup(SCR_AIGroup.Cast(row.m_Group), bucket);
	}

	protected bool ClassifyCapturedActor(AICF_AIQuotaBody body, SCR_AIGroup parent, out AICF_EAIQuotaBucket bucket)
	{
		bucket = AICF_EAIQuotaBucket.STOCK;
		if (!body || !body.MatchesIdentity() || body.m_bQuarantined || body.m_bVehicle) return false;
		if (body.m_GroupOwner && (!parent || parent == body.m_GroupOwner.m_Group))
		{
			if (!body.m_GroupOwner.MatchesIdentity()) return false;
			bucket = body.m_GroupOwner.GetBucket();
			return bucket == body.m_Bucket;
		}
		// Positive same-bucket native transfer (donor -> current slot) is accepted
		// even after old donor controller deletion. Cross-bucket needs an explicit
		// pre-transfer destination permit; guessing/reclassifying is not allowed.
		if (!m_Owners.ClassifyGroup(parent, bucket)) return false;
		return bucket == body.m_Bucket;
	}

	// Body entries ещё без counted native agent удерживают unit, но counted row
	// заменяет этот hold. Raw native active/intended остаются отдельной величиной.
	protected bool ReadQuotaCharacterCustody(AICF_AIQuotaAgentRow row, out bool hold)
	{
		hold = false;
		if (!IsCurrentWorld() || !row || row.m_bVehicle || !m_World || !row.IsCurrent(m_World.GetWorld())) return false;
		ChimeraCharacter actor = ChimeraCharacter.Cast(row.m_Entity);
		if (!actor) return true; // Empty group controllers do not cost a character.
		CharacterControllerComponent controller = actor.GetCharacterController();
		AIControlComponent control = actor.GetAIControlComponent();
		PlayerManager players = GetGame().GetPlayerManager();
		if (!controller || !control || !players || control.GetControlAIAgent() != row.m_Agent) return false;
		ECharacterLifeState life = controller.GetLifeState();
		bool human = controller.IsPlayerControlled() || players.GetPlayerIdFromControlledEntity(actor) != 0 ||
			SCR_PossessingManagerComponent.GetPlayerIdFromMainEntity(actor) > 0;
		if (life != ECharacterLifeState.ALIVE && life != ECharacterLifeState.INCAPACITATED && life != ECharacterLifeState.DEAD) return false;
		if (!IsCurrentWorld() || GetGame().GetPlayerManager() != players || !row.IsCurrent(m_World.GetWorld()) || actor.GetCharacterController() != controller ||
			actor.GetAIControlComponent() != control || control.GetControlAIAgent() != row.m_Agent ||
			controller.GetLifeState() != life) return false;
		bool repeatedHuman = controller.IsPlayerControlled() || players.GetPlayerIdFromControlledEntity(actor) != 0 ||
			SCR_PossessingManagerComponent.GetPlayerIdFromMainEntity(actor) > 0;
		if (human != repeatedHuman || !IsCurrentWorld() || !row.IsCurrent(m_World.GetWorld())) return false;
		hold = !human && life != ECharacterLifeState.DEAD;
		return true;
	}

	// Seed only actual registered living AI bodies, after the complete census is
	// applied. The zero-increment binding keeps an inactive body charged if its
	// agent temporarily disappears, until exact death/absence proves release.
	protected bool InitializeQuotaCharacterBindings(AICF_AIQuotaCensus snapshot)
	{
		if (!snapshot || !snapshot.IsCurrent() || !m_Ledger) return false;
		for (int index; index < snapshot.Count(); index++)
		{
			AICF_AIQuotaAgentRow row = snapshot.GetRow(index);
			if (!row) return false;
			if (row.m_bVehicle) continue;
			bool hold;
			if (!ReadQuotaCharacterCustody(row, hold)) return false;
			if (!hold) continue;
			AICF_EAIQuotaBucket bucket;
			if (!ClassifyRow(row, bucket)) return false;
			AICF_AIQuotaBody body = FindBody(row.m_Entity);
			if (!body)
			{
				AICF_AIQuotaGroupOwner owner = m_Owners.Find(SCR_AIGroup.Cast(row.m_Group));
				body = AICF_AIQuotaBody.Capture(row.m_Entity, bucket, owner);
				if (!body || m_aBodies.Count() >= AICF_AIQuotaMath.MAX_TOTAL_UNITS || !snapshot.IsCurrent() ||
					!ReadQuotaCharacterCustody(row, hold) || !hold || FindBody(row.m_Entity)) return false;
				if (!m_Ledger.RegisterExistingBinding(body.Key(), bucket)) return false;
				m_aBodies.Insert(body);
			}
			else if (!m_Ledger.RegisterExistingBinding(body.Key(), body.m_Bucket)) return false;
		}
		return snapshot.IsCurrent();
	}

	protected bool BuildCounts(AICF_AIQuotaCensus snapshot, array<int> counts)
	{
		if (!snapshot || !snapshot.IsCurrent() || !counts) return false;
		counts.Clear();
		counts.Resize(AICF_EAIQuotaBucket.COUNT);
		array<IEntity> counted = {};
		for (int index; index < snapshot.Count(); index++)
		{
			AICF_AIQuotaAgentRow row = snapshot.GetRow(index);
			if (!row) return false;
			if (row.m_bVehicle)
			{
				AICF_EAIQuotaBucket vehicleBucket;
				bool vehicleCharge;
				if (!ClassifyQuotaVehicleRow(row, vehicleBucket, vehicleCharge)) return false;
				if (!vehicleCharge) continue;
				if (vehicleBucket < 0 || vehicleBucket >= counts.Count() || counts[vehicleBucket] >= AICF_AIQuotaMath.MAX_TOTAL_UNITS) return false;
				counts[vehicleBucket] = counts[vehicleBucket] + 1;
				counted.Insert(row.m_Entity);
				continue;
			}
			bool characterHold;
			if (!ReadQuotaCharacterCustody(row, characterHold)) return false;
			if (!row.m_bCounted && !characterHold) continue;
			AICF_EAIQuotaBucket bucket;
			if (!ClassifyRow(row, bucket)) return false;
			if (counts[bucket] >= AICF_AIQuotaMath.MAX_TOTAL_UNITS) return false;
			counts[bucket] = counts[bucket] + 1;
			if (row.m_Entity) counted.Insert(row.m_Entity);
		}
		for (int bodyIndex = m_aBodies.Count() - 1; bodyIndex >= 0; bodyIndex--)
		{
			AICF_AIQuotaBody body = m_aBodies[bodyIndex];
			if (!body) return false;
			if (body.m_bVehicle) continue;
			if (body.m_bReleased)
			{
				m_aBodies.Remove(bodyIndex);
				continue;
			}
			if (body.IsDefinitelyGone() || body.IsNativeDeadAndInactive())
			{
				body.m_bReleased = true;
				if (m_Ledger) m_Ledger.RetireBinding(body.Key());
				m_aBodies.Remove(bodyIndex);
				continue;
			}
			if (!body.MatchesIdentity() || body.m_bQuarantined) return false;
			if (counted.Contains(body.m_Entity)) continue;
			AICF_EAIQuotaBucket bodyBucket = body.m_Bucket;
			if (!body.m_bVehicle)
			{
				ChimeraCharacter actor = ChimeraCharacter.Cast(body.m_Entity);
				if (!actor || !actor.GetAIControlComponent()) return false;
				AIAgent agent = actor.GetAIControlComponent().GetControlAIAgent();
				SCR_AIGroup parent;
				if (agent)
				{
					if (agent.GetControlledEntity() != actor) return false;
					parent = SCR_AIGroup.Cast(agent.GetParentGroup());
				}
				if (!ClassifyCapturedActor(body, parent, bodyBucket)) return false;
			}
			if (bodyBucket < 0 || bodyBucket >= AICF_EAIQuotaBucket.COUNT || counts[bodyBucket] >= AICF_AIQuotaMath.MAX_TOTAL_UNITS) return false;
			counts[bodyBucket] = counts[bodyBucket] + 1;
		}
		return AddQuotaVehicleHolds(snapshot, counts, counted) && snapshot.IsCurrent();
	}

	bool Initialize(AICF_MatchController controller, int managedUnits, int builderUnits, int fleetUnits, int fiaUnits)
	{
		if (IsQuotaAdmissionBusy()) return false;
		m_bQuotaCensusBusy = true;
		bool initialized = InitializeCurrent(controller, managedUnits, builderUnits, fleetUnits, fiaUnits);
		m_bQuotaCensusBusy = false;
		return initialized;
	}

	protected bool InitializeCurrent(AICF_MatchController controller, int managedUnits, int builderUnits, int fleetUnits, int fiaUnits)
	{
		if (!IsCurrentWorld() || !controller || !controller.IsAIQuotaOwnerCurrent(m_World) || m_bNativeBirthBusy || m_bQuarantined)
			return false;
		AICF_AIQuotaCensus snapshot = new AICF_AIQuotaCensus();
		array<int> counts = {};
		int bootstrapSerial = BeginQuotaVehicleBootstrap(controller);
		if (bootstrapSerial <= 0) return false;
		bool initialCensusValid = snapshot.Capture(m_World) && BuildCounts(snapshot, counts);
		if (!EndQuotaVehicleBootstrap(controller, bootstrapSerial)) initialCensusValid = false;
		if (!initialCensusValid)
		{
			m_sLastReason = "INITIAL_CENSUS_UNKNOWN";
			return false;
		}
		if (!m_bPolicyActive && !CapturePrepolicyNativeRequests())
		{
			m_sLastReason = "PREPOLICY_NATIVE_REQUEST_JOURNAL_UNKNOWN";
			return false;
		}
		AICF_AIQuotaLimits candidate;
		int epoch;
		if (!m_WorldState.BeginSession(controller, counts[AICF_EAIQuotaBucket.STOCK], counts[AICF_EAIQuotaBucket.STOCK_VEHICLES], candidate, epoch))
			return false;
		if (!candidate.Configure(managedUnits, builderUnits, fleetUnits, fiaUnits))
		{
			m_WorldState.EndSession(controller, epoch);
			m_sLastReason = "INVALID_QUOTA_LIMITS";
			return false;
		}
		if (m_Limits)
		{
			for (int index; index < AICF_EAIQuotaBucket.COUNT; index++)
			{
				if (m_Limits.GetLimit(index) == candidate.GetLimit(index)) continue;
				m_WorldState.EndSession(controller, epoch);
				m_sLastReason = "WORLD_QUOTA_CONFIG_CHANGED";
				return false;
			}
		}
		else
		{
			m_Limits = candidate;
			// Ledger epoch принадлежит самому world policy object, не root restart.
			m_Ledger = new AICF_AIQuotaLedger(m_Limits, 1);
		}
		if (!m_Owners.BeginSession(controller, epoch) || !m_Ledger.ApplyCensus(counts, snapshot.IsCurrent()))
		{
			m_WorldState.EndSession(controller, epoch);
			m_sLastReason = "QUOTA_CENSUS_OUTSIDE_LIMITS";
			return false;
		}
		m_Controller = controller;
		m_iEpoch = epoch;
		m_Census = snapshot;
		if (!InitializeQuotaCharacterBindings(snapshot) || !InitializeQuotaVehicleBindings(snapshot))
		{
			m_Ledger.ApplyCensus(null, false);
			m_WorldState.EndSession(controller, epoch);
			m_sLastReason = "INITIAL_BODY_BINDING_UNKNOWN";
			return false;
		}
		m_bPolicyActive = true;
		m_sLastReason = "READY";
		return true;
	}

	protected bool CapturePrepolicyNativeRequests()
	{
		AICF_AIQuotaNativeQueueJournal journal = m_World.AICF_GetNativeQuotaJournal();
		array<ref AICF_AIQuotaNativeQueueRecord> records = {};
		if (!journal || !journal.Snapshot(records)) return false;
		int waitingUnits;
		int metadataCeiling = AICF_AIQuotaMath.MAX_TOTAL_UNITS * AICF_AIQuotaMath.MAX_TOTAL_UNITS;
		foreach (AICF_AIQuotaNativeQueueRecord record : records)
		{
			if (!record || !record.IsCurrent()) return false;
			SCR_AIGroup group = record.m_Identity.m_Group;
			int missing = AICF_AIQuotaGroupQueue.RequestedUnits(group, record.m_iSlotsWanted);
			int sum;
			if (missing < 0 || !AICF_AIQuotaMath.AddBounded(waitingUnits, missing, metadataCeiling, sum)) return false;
			waitingUnits = sum;
			EntityID id = record.m_Identity.m_GroupId;
			AICF_Stage1Diagnostics.Info("AI_QUOTA_PREPOLICY_NATIVE_REQUEST", string.Format(
				"sequence=%1 group=%2 group_rpl=%3 actual=%4 authored=%5 requested_unmaterialized=%6 group_native_intended=%7 pending_charged_as_body=0 priority_unchanged=1",
				record.m_iSequence, id, record.m_Identity.m_sRpl, group.GetAgentsCount(), group.m_aUnitPrefabSlots.Count(), missing,
				group.GetIntendedActiveCount()));
		}
		if (!journal.IsCurrent()) return false;
		m_iCapturedNativeRequests = records.Count();
		m_iCapturedNativeWaitingUnits = waitingUnits;
		AICF_Stage1Diagnostics.Info("AI_QUOTA_PREPOLICY_REQUESTS_CAPTURED", string.Format(
			"known_unsettled_script_requests=%1 unmaterialized_units=%2 fabricated_physical_units=0 catalog_paid_queue_drained=1 native_queue_membership=UNPROVEN native_priority_unchanged=1",
			m_iCapturedNativeRequests, m_iCapturedNativeWaitingUnits));
		return true;
	}

	bool RefreshCensus()
	{
		if (IsQuotaAdmissionBusy()) return false;
		m_bQuotaCensusBusy = true;
		bool refreshed = RefreshCurrentCensus();
		m_bQuotaCensusBusy = false;
		return refreshed;
	}

	bool Maintain()
	{
		if (!IsOwnerCurrent()) return false;
		bool ready = RefreshCensus();
		if (ready)
		{
			if (!m_sLastCensusReport.IsEmpty())
				AICF_Stage1Diagnostics.Info("AI_QUOTA_CENSUS_RESTORED", "reason=COMPLETE native_actions_unchanged=1");
			m_sLastCensusReport = string.Empty;
			return true;
		}
		if (m_sLastCensusReport != m_sLastReason)
		{
			m_sLastCensusReport = m_sLastReason;
			AICF_Stage1Diagnostics.Info("AI_QUOTA_CENSUS_WAIT", string.Format(
				"reason=%1 new_dispatch_veto=1 existing_bodies_untouched=1 native_floor=%2", m_sLastReason, m_World.GetLimitOfActiveAIs()));
		}
		return false;
	}

	protected bool RefreshCurrentCensus()
	{
		if (!IsConfigured() || m_bNativeBirthBusy) return false;
		PruneGroupQueues();
		AICF_AIQuotaCensus snapshot = new AICF_AIQuotaCensus();
		array<int> counts = {};
		if (!snapshot.Capture(m_World) || !BuildCounts(snapshot, counts))
		{
			m_Ledger.ApplyCensus(null, false);
			m_sLastReason = "NATIVE_CENSUS_UNKNOWN";
			return false;
		}
		if (!m_Ledger.ApplyCensus(counts, snapshot.IsCurrent()))
		{
			m_sLastReason = "PARTITION_OCCUPANCY_MISMATCH";
			return false;
		}
		m_Census = snapshot;
		if (!InitializeQuotaCharacterBindings(snapshot) || !InitializeQuotaVehicleBindings(snapshot))
		{
			m_Ledger.ApplyCensus(null, false);
			m_sLastReason = "BODY_BINDING_UNKNOWN";
			return false;
		}
		m_sLastReason = "READY";
		return true;
	}

	bool CanReserve(AICF_EAIQuotaBucket bucket, int units)
	{
		return RefreshCensus() && m_Ledger.CanReserve(bucket, units);
	}

	void StopOwner(AICF_MatchController controller)
	{
		if (!IsCurrentWorld() || !controller || controller != m_Controller || m_iEpoch <= 0 || !m_Ledger) return;
		foreach (AICF_AIQuotaPlannedGroup planned : m_aPlannedGroups)
			if (planned && planned.m_Controller == controller && planned.m_iEpoch == m_iEpoch)
				m_Ledger.CancelAvailable(planned.m_Reservation, planned.m_iSerial);
		m_aPlannedGroups.Clear();
		foreach (AICF_AIQuotaGroupQueue queue : m_aQueues)
			if (queue && queue.m_Owner && queue.m_Owner.m_Controller == controller && queue.m_Owner.m_iEpoch == m_iEpoch)
				queue.CancelAvailable(m_Ledger);
		m_WorldState.EndSession(controller, m_iEpoch);
		// Policy/floor and physical custody survive Stop; no new dispatch under
		// the stale owner, and no temporary vanilla headroom bypass.
	}

	bool ReserveReplacement(AICF_FactionState state, AICF_GroupSlot slot)
	{
		if (!IsOwnerCurrent() || !m_Controller.IsAIQuotaSlotCurrent(state, slot) || slot.GetGroup()) return false;
		return ReservePlannedGroup(state, slot, null, slot.GetDeploymentSize());
	}

	bool ReserveRecruit(AICF_InfantryRecruitmentOrder recruit)
	{
		if (!IsOwnerCurrent() || !recruit || recruit.m_Donor || !recruit.m_RecruitmentService ||
			!recruit.m_RecruitmentService.IsCurrentOrder(recruit)) return false;
		return ReservePlannedGroup(null, null, recruit, 1);
	}

	protected bool ReservePlannedGroup(AICF_FactionState state, AICF_GroupSlot slot,
		AICF_InfantryRecruitmentOrder recruit, int units)
	{
		if (!IsOwnerCurrent() || units <= 0 || units > 10 || IsQuotaAdmissionBusy() || !RefreshCensus()) return false;
		for (int index = m_aPlannedGroups.Count() - 1; index >= 0; index--)
		{
			AICF_AIQuotaPlannedGroup existing = m_aPlannedGroups[index];
			if (!existing) return false;
			if ((recruit && existing.m_Recruit == recruit) || (slot && existing.m_Slot == slot))
			{
				if (existing.IsWaitingCurrent() && existing.m_iUnits == units &&
					m_Ledger.HasAvailableReservation(existing.m_Reservation, existing.m_iSerial)) return true;
				m_Ledger.CancelAvailable(existing.m_Reservation, existing.m_iSerial);
				m_aPlannedGroups.Remove(index);
			}
			else if (!existing.IsPendingCurrent())
			{
				// No native request has been issued for an unbound plan. The exact
				// synchronous caller binds it before any group dispatch.
				m_Ledger.CancelAvailable(existing.m_Reservation, existing.m_iSerial);
				m_aPlannedGroups.Remove(index);
			}
		}
		if (m_aPlannedGroups.Count() >= AICF_AIQuotaMath.MAX_TOTAL_UNITS) return false;
		AICF_AIQuotaPlannedGroup planned = new AICF_AIQuotaPlannedGroup();
		planned.m_Service = this;
		planned.m_Controller = m_Controller;
		planned.m_iEpoch = m_iEpoch;
		planned.m_iSerial = AllocateContextSerial();
		planned.m_iUnits = units;
		planned.m_State = state;
		planned.m_Slot = slot;
		planned.m_Recruit = recruit;
		if (slot) planned.m_iGeneration = slot.GetSpawnGeneration();
		else { planned.m_iGeneration = recruit.m_iGeneration; planned.m_iToken = recruit.m_iToken; }
		if (!planned.IsWaitingCurrent()) return false;
		planned.m_Reservation = m_Ledger.Reserve(AICF_EAIQuotaBucket.COMBAT, units, planned.m_iSerial);
		if (!planned.m_Reservation)
		{
			m_sLastReason = "COMBAT_BUCKET_FULL";
			return false;
		}
		if (!planned.IsWaitingCurrent())
		{
			m_Ledger.CancelAvailable(planned.m_Reservation, planned.m_iSerial);
			return false;
		}
		m_aPlannedGroups.Insert(planned);
		return true;
	}

	bool BindPlannedGroup(AICF_AIQuotaGroupOwner owner)
	{
		if (!IsOwnerCurrent() || !owner || !owner.CanDispatch()) return false;
		for (int index; index < m_aPlannedGroups.Count(); index++)
		{
			AICF_AIQuotaPlannedGroup planned = m_aPlannedGroups[index];
			if (!planned) return false;
			if (!((owner.m_Recruit && planned.m_Recruit == owner.m_Recruit) ||
				(owner.m_Slot && planned.m_Slot == owner.m_Slot))) continue;
			if (!planned.CanBind(owner) || !m_Ledger.HasAvailableReservation(planned.m_Reservation, planned.m_iSerial) ||
				!owner.m_Group.m_aUnitPrefabSlots || owner.m_Group.m_aUnitPrefabSlots.Count() != planned.m_iUnits ||
				owner.m_Group.GetAgentsCount() != 0) return false;
			foreach (AICF_AIQuotaGroupQueue existing : m_aQueues)
				if (!existing || existing.m_Group == owner.m_Group) return false;
			AICF_AIQuotaGroupQueue queue = AICF_AIQuotaGroupQueue.Capture(owner.m_Group, owner, planned.m_iSerial);
			if (!queue || !queue.AdoptBasket(m_Ledger, planned.m_Reservation) || !planned.CanBind(owner)) return false;
			m_aQueues.Insert(queue);
			planned.m_bBound = true;
			m_aPlannedGroups.Remove(index);
			return true;
		}
		// Initial free roster and bounded service owners reserve at RequestSpawn.
		return true;
	}

	bool AdoptExternalGroupReservation(SCR_AIGroup group, AICF_AIQuotaGroupOwner owner,
		AICF_AIQuotaReservation basket, int serial)
	{
		if (!IsOwnerCurrent() || !IsConfigured() || IsQuotaAdmissionBusy() || !group || !owner || owner.m_Group != group ||
			!owner.CanDispatch() || owner.m_Controller != m_Controller || owner.m_iEpoch != m_iEpoch || !basket ||
			basket.m_Bucket != owner.GetBucket() || !m_Ledger.HasAvailableReservation(basket, serial) ||
			group.GetAgentsCount() != 0 || !group.m_aUnitPrefabSlots || group.m_aUnitPrefabSlots.Count() != basket.m_iAvailable) return false;
		foreach (AICF_AIQuotaGroupQueue existing : m_aQueues)
		{
			if (!existing) return false;
			if (existing.m_Group != group) continue;
			return existing.m_Owner == owner && existing.HasBasket(basket, serial);
		}
		if (m_aQueues.Count() >= AICF_AIQuotaMath.MAX_TOTAL_UNITS) return false;
		AICF_AIQuotaGroupQueue queue = AICF_AIQuotaGroupQueue.Capture(group, owner, serial);
		if (!queue || !queue.AdoptBasket(m_Ledger, basket) || !owner.CanDispatch()) return false;
		m_aQueues.Insert(queue);
		return true;
	}

	void CancelPlannedSlot(AICF_GroupSlot slot)
	{
		if (!IsCurrentWorld() || !m_Ledger || !slot) return;
		for (int index = m_aPlannedGroups.Count() - 1; index >= 0; index--)
		{
			AICF_AIQuotaPlannedGroup planned = m_aPlannedGroups[index];
			if (!planned || planned.m_Slot != slot) continue;
			m_Ledger.CancelAvailable(planned.m_Reservation, planned.m_iSerial);
			m_aPlannedGroups.Remove(index);
		}
	}

	AICF_AIQuotaGroupQueue EnsureGroupQueue(SCR_AIGroup group, int slotsWanted)
	{
		if (!IsConfigured() || IsQuotaAdmissionBusy() || !group || !RefreshCensus()) return null;
		PruneGroupQueues();
		AICF_AIQuotaGroupOwner owner = m_Owners.Find(group);
		if (owner && !owner.CanDispatch()) return null;
		AICF_AIQuotaGroupQueue queue;
		foreach (AICF_AIQuotaGroupQueue existing : m_aQueues)
		{
			if (!existing || existing.m_Group != group) continue;
			if (existing.m_Owner != owner || !existing.IsCurrent()) return null;
			queue = existing;
			break;
		}
		if (!queue)
		{
			if (m_aQueues.Count() >= AICF_AIQuotaMath.MAX_TOTAL_UNITS) return null;
			queue = AICF_AIQuotaGroupQueue.Capture(group, owner, AllocateContextSerial());
			if (!queue) return null;
			m_aQueues.Insert(queue);
		}
		int required = AICF_AIQuotaGroupQueue.RequestedUnits(group, slotsWanted);
		if (required < 0 || !queue.EnsureAvailable(m_Ledger, required)) return null;
		return queue;
	}

	// Admission precedes native EnqueueSpawnRequest/intended accounting, and is
	// repeated at the factory. A queue reservation is a basket, never a body.
	bool AdmitGroupRequest(SCR_AIGroup group, int slotsWanted)
	{
		if (!IsConfigured() || IsQuotaAdmissionBusy() || !group || group.GetWorld() != m_World.GetWorld()) return false;
		int required = AICF_AIQuotaGroupQueue.RequestedUnits(group, slotsWanted);
		if (required < 0) return false;
		if (required == 0) return true;
		return EnsureGroupQueue(group, slotsWanted) != null;
	}

	void CancelGroupRequest(SCR_AIGroup group)
	{
		if (!IsCurrentWorld() || !m_Ledger || !group) return;
		for (int index = m_aQueues.Count() - 1; index >= 0; index--)
		{
			AICF_AIQuotaGroupQueue queue = m_aQueues[index];
			if (!queue || queue.m_Group != group) continue;
			queue.CancelAvailable(m_Ledger);
			// The local factory keeps a strong queue until its exact ticket closes.
			m_aQueues.Remove(index);
		}
	}

	protected void PruneGroupQueues()
	{
		if (!m_Ledger) return;
		for (int index = m_aQueues.Count() - 1; index >= 0; index--)
		{
			AICF_AIQuotaGroupQueue queue = m_aQueues[index];
			if (m_CurrentBirth && m_CurrentBirth.m_Queue == queue) continue;
			if (queue && queue.IsCurrent() && !queue.m_Group.IsExpandComplete()) continue;
			if (queue) queue.CancelAvailable(m_Ledger);
			m_aQueues.Remove(index);
		}
	}

	// Никакого native вызова здесь: его единственный владелец остаётся
	// version-locked SCR_AIGroup.SpawnGroupMember local factory boundary.
	AICF_AIQuotaBirth BeginGroupBirth(SCR_AIGroup group, int index, ResourceName prefab)
	{
		if (!IsConfigured() || IsQuotaAdmissionBusy() || !group || index < 0 || prefab.IsEmpty() ||
			!group.m_aUnitPrefabSlots || !group.m_aUnitPrefabSlots.IsIndexValid(index) || group.m_aUnitPrefabSlots[index] != prefab)
			return null;
		AICF_AIQuotaGroupQueue queue = EnsureGroupQueue(group, -1);
		if (!queue || !queue.IsCurrent()) return null;
		ChimeraAIWorld nativeWorld = ChimeraAIWorld.Cast(m_World);
		Faction faction = group.GetFaction();
		if (!nativeWorld || !faction || !nativeWorld.CanAIBeActivated() || !nativeWorld.CanActivateGroup(group) ||
			!nativeWorld.CanLimitedAIBeAddedForFaction(faction.GetFactionKey()))
			return null;
		AICF_AIQuotaDispatch ticket = queue.BeginDispatch(m_Ledger);
		if (!ticket || !queue.IsCurrent())
		{
			if (ticket) m_Ledger.FailDispatch(ticket, queue.m_iSerial);
			return null;
		}
		AICF_AIQuotaBirth birth = new AICF_AIQuotaBirth();
		birth.m_Service = this;
		birth.m_Queue = queue;
		birth.m_Ticket = ticket;
		birth.m_iSlotIndex = index;
		birth.m_Prefab = prefab;
		birth.m_Status = AICF_EAIQuotaBirthStatus.IN_NATIVE_FACTORY;
		m_CurrentBirth = birth;
		m_bNativeBirthBusy = true;
		return birth;
	}

	protected void QuarantineBirth(AICF_AIQuotaBirth birth, string reason)
	{
		if (!birth || birth != m_CurrentBirth || birth.m_Service != this || birth.m_bFinished || !m_bNativeBirthBusy) return;
		if (m_QuarantinedBirth == birth) return;
		m_QuarantinedBirth = birth;
		birth.m_Status = AICF_EAIQuotaBirthStatus.QUARANTINED;
		if (birth.m_Body) birth.m_Body.m_bQuarantined = true;
		if (birth.m_Queue) birth.m_Queue.m_bQuarantined = true;
		m_bQuarantined = true;
		m_sLastReason = reason;
		AICF_Stage1Diagnostics.Error("AI_QUOTA_BIRTH_QUARANTINED", string.Format(
			"reason=%1 entity=%2 prefab=%3 index=%4 native_null=0 retry_factory_allowed=0",
			reason, birth.m_Entity, birth.m_Prefab, birth.m_iSlotIndex));
	}

	void ObserveGroupFactory(AICF_AIQuotaBirth birth, IEntity entity)
	{
		if (!IsCurrentWorld() || !birth || birth.m_Service != this || birth != m_CurrentBirth || !m_bNativeBirthBusy ||
			birth.m_bFactoryObserved || birth.m_bFinished ||
			(birth.m_Status != AICF_EAIQuotaBirthStatus.IN_NATIVE_FACTORY && birth != m_QuarantinedBirth))
		{
			QuarantineBirth(birth, "FACTORY_CONTEXT_CHANGED");
			return;
		}
		birth.m_bFactoryObserved = true;
		birth.m_Entity = entity;
		if (RetainQuarantinedGroupReturn(birth, entity)) return;
		if (!entity)
		{
			if (!m_Ledger.FailDispatch(birth.m_Ticket, birth.m_Queue.m_iSerial))
			{
				QuarantineBirth(birth, "NATIVE_NULL_TICKET_MISMATCH");
				return;
			}
			birth.m_Status = AICF_EAIQuotaBirthStatus.NATIVE_NULL;
			return;
		}
		birth.m_Body = AICF_AIQuotaBody.Capture(entity, birth.m_Queue.GetBucket(), birth.m_Queue.m_Owner);
		if (!birth.m_Body || !birth.IsRequestCurrent() || !AICF_AIQuotaBirth.IsNonPlayerUnassigned(entity) ||
			SCR_ResourceNameUtils.GetPrefabName(entity) != birth.m_Prefab || m_aBodies.Count() >= AICF_AIQuotaMath.MAX_TOTAL_UNITS ||
			FindBody(entity))
		{
			birth.m_Status = AICF_EAIQuotaBirthStatus.QUARANTINED;
			birth.m_sRejectReason = "NONNULL_BODY_IDENTITY_CHANGED";
			return;
		}
		if (!m_Ledger.Bind(birth.m_Ticket, birth.m_Queue.m_iSerial, birth.m_Body.Key()))
		{
			birth.m_Status = AICF_EAIQuotaBirthStatus.QUARANTINED;
			birth.m_sRejectReason = "EXACT_BODY_BIND_FAILED";
			return;
		}
		m_aBodies.Insert(birth.m_Body);
		birth.m_Status = AICF_EAIQuotaBirthStatus.BOUND;
	}

	bool IsGroupBirthCurrent(AICF_AIQuotaBirth birth, bool assigned)
	{
		if (!IsCurrentWorld() || !birth || birth != m_CurrentBirth || birth.m_Service != this || !m_bNativeBirthBusy || birth.m_bFinished ||
			birth.m_Status != AICF_EAIQuotaBirthStatus.BOUND || !birth.m_bFactoryObserved || !birth.IsRequestCurrent() ||
			!birth.m_Body || !birth.m_Body.MatchesIdentity() || birth.m_Body.m_Entity != birth.m_Entity)
			return false;
		ChimeraCharacter character = ChimeraCharacter.Cast(birth.m_Entity);
		if (!character || !character.GetAIControlComponent() || !character.GetCharacterController() ||
			character.GetCharacterController().IsPlayerControlled() || !GetGame().GetPlayerManager() ||
			GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(character) > 0 ||
			SCR_PossessingManagerComponent.GetPlayerIdFromMainEntity(character) > 0)
			return false;
		AIAgent agent = character.GetAIControlComponent().GetControlAIAgent();
		if (assigned) return agent && agent.GetControlledEntity() == character && agent.GetParentGroup() == birth.m_Queue.m_Group;
		return AICF_AIQuotaBirth.IsNonPlayerUnassigned(character);
	}

	// Exact bound newborn capability only inside the local factory dispatch.
	// A nested unrelated actor cannot inherit the native-birth busy scope.
	AICF_AIQuotaBirth FindCurrentBoundBirth(IEntity actor)
	{
		AICF_AIQuotaBirth birth = m_CurrentBirth;
		if (!OwnsCurrentBoundBirth(birth, actor)) return null;
		return birth;
	}

	bool OwnsCurrentBoundBirth(AICF_AIQuotaBirth birth, IEntity actor)
	{
		return actor && birth && birth == m_CurrentBirth && birth.m_Entity == actor &&
			birth.m_Ticket && birth.m_Ticket.m_bClosed && birth.m_Queue && birth.m_Queue.m_Owner &&
			birth.m_Queue.m_Owner.CanDispatch() && IsGroupBirthCurrent(birth, false);
	}

	void EndGroupBirth(AICF_AIQuotaBirth birth)
	{
		if (!birth || birth.m_Service != this || birth != m_CurrentBirth || birth.m_bFinished || !m_bNativeBirthBusy) return;
		if (!birth.m_bFactoryObserved) return;
		if (birth.m_Status == AICF_EAIQuotaBirthStatus.IN_NATIVE_FACTORY) return;
		birth.m_bFinished = true;
		m_CurrentBirth = null;
		m_bNativeBirthBusy = false;
	}

	void RejectNonNullBirth(AICF_AIQuotaBirth birth, string reason)
	{
		QuarantineBirth(birth, reason);
		if (birth == m_CurrentBirth) EndGroupBirth(birth);
	}

	protected bool RetainQuarantinedGroupReturn(AICF_AIQuotaBirth birth, IEntity entity)
	{
		if (!IsCurrentWorld() || !birth || birth != m_CurrentBirth || birth != m_QuarantinedBirth || birth.m_Service != this || birth.m_Entity != entity ||
			birth.m_bFinished || !birth.m_bFactoryObserved || !m_bNativeBirthBusy) return false;
		if (entity)
		{
			birth.m_Body = AICF_AIQuotaBody.Capture(entity, birth.m_Queue.GetBucket(), birth.m_Queue.m_Owner);
			if (birth.m_Body) birth.m_Body.m_bQuarantined = true;
		}
		else if (birth.m_Ticket && !birth.m_Ticket.m_bClosed)
			m_Ledger.FailDispatch(birth.m_Ticket, birth.m_Queue.m_iSerial);
		// No BOUND/native-NULL success projection can replace the earlier failure.
		birth.m_Status = AICF_EAIQuotaBirthStatus.QUARANTINED;
		string identity = "UNKNOWN";
		if (birth.m_Body) identity = birth.m_Body.Key();
		AICF_Stage1Diagnostics.Info("AI_QUOTA_QUARANTINED_NATIVE_RETURN", string.Format(
			"kind=GROUP exact_receipt_retained=1 native_nonnull=%1 body_identity_captured=%2 prior_failure_retained=1 permission_reused=0 entity_key=%3 owner_serial=%4",
			entity != null, birth.m_Body != null, identity, birth.m_Queue.m_iSerial));
		EndGroupBirth(birth);
		return true;
	}

	bool ObserveBirthAbsence(AICF_AIQuotaBirth birth)
	{
		if (!birth || birth != m_CurrentBirth || birth.m_Service != this || birth.m_bFinished ||
			!birth.m_bFactoryObserved || !birth.m_Ticket || !birth.m_Queue || !birth.m_Body || !birth.m_Body.IsDefinitelyGone())
			return false;
		if (birth.m_Ticket.m_bClosed)
		{
			birth.m_Body.m_bReleased = true;
			m_Ledger.RetireBinding(birth.m_Body.Key());
		}
		else if (!m_Ledger.FailDispatch(birth.m_Ticket, birth.m_Queue.m_iSerial))
			return false;
		birth.m_Status = AICF_EAIQuotaBirthStatus.ROLLED_BACK;
		AICF_Stage1Diagnostics.Info("AI_QUOTA_NATIVE_BIRTH_CANCELLED", string.Format(
			"group=%1 prefab=%2 index=%3 actual_absence=1 known_native_null=0 permission_refunded=1",
			birth.m_Queue.m_GroupId, birth.m_Prefab, birth.m_iSlotIndex));
		EndGroupBirth(birth);
		return true;
	}

	bool CompleteGroupBirth(AICF_AIQuotaBirth birth)
	{
		if (IsGroupBirthCurrent(birth, true))
		{
			EndGroupBirth(birth);
			return true;
		}
		if (ObserveBirthAbsence(birth)) return false;
		RejectNonNullBirth(birth, "POST_NATIVE_OWNER_CHANGED");
		return false;
	}

	bool CanRollbackGroupBirth(AICF_AIQuotaBirth birth)
	{
		return IsCurrentWorld() && birth && birth == m_CurrentBirth && birth.m_Service == this && m_bNativeBirthBusy &&
			!birth.m_bFinished && birth.m_bFactoryObserved && birth.m_Entity && birth.m_Body &&
			birth.m_Status != AICF_EAIQuotaBirthStatus.NATIVE_NULL && !birth.IsRequestCurrent() &&
			birth.m_Body.m_Entity == birth.m_Entity && birth.m_Body.MatchesIdentity() &&
			SCR_ResourceNameUtils.GetPrefabName(birth.m_Entity) == birth.m_Prefab &&
			AICF_AIQuotaBirth.IsNonPlayerUnassigned(birth.m_Entity);
	}

	AICF_AIQuotaReservation ReserveUnits(AICF_EAIQuotaBucket bucket, int units, int serial)
	{
		if (serial <= 0 || serial >= m_iNextContext || !RefreshCensus()) return null;
		return m_Ledger.Reserve(bucket, units, serial);
	}

	bool CancelReservation(AICF_AIQuotaReservation reservation, int serial)
	{
		if (!IsCurrentWorld() || !m_Ledger || !reservation) return false;
		return m_Ledger.CancelAvailable(reservation, serial);
	}

	AICF_AIQuotaCreation BeginExternalCreation(AICF_AIQuotaReservation basket, int serial, ResourceName prefab,
		IEntity producerOwner, bool headerOnly = false, AICF_AIQuotaCreationGuard guard = null)
	{
		if (!IsConfigured() || IsQuotaAdmissionBusy() || !guard || !guard.IsCurrent() || !producerOwner || prefab.IsEmpty() ||
			!RefreshCensus() || !m_Ledger.HasAvailableReservation(basket, serial)) return null;
		if (basket.m_Bucket != guard.GetBucket() || (headerOnly && basket.m_Bucket != AICF_EAIQuotaBucket.STOCK)) return null;
		ChimeraAIWorld nativeWorld = ChimeraAIWorld.Cast(m_World);
		FactionKey faction = guard.GetFactionKey();
		if (!nativeWorld || faction.IsEmpty() || (!headerOnly && (!nativeWorld.CanAIBeActivated() ||
			!nativeWorld.CanActivateGroup(null) || !nativeWorld.CanLimitedAIBeAddedForFaction(faction)))) return null;
		AICF_AIQuotaCreation creation = new AICF_AIQuotaCreation();
		creation.m_Service = this;
		creation.m_Reservation = basket;
		creation.m_Guard = guard;
		creation.m_iSerial = serial;
		creation.m_Prefab = prefab;
		creation.m_Producer = producerOwner;
		creation.m_ProducerId = producerOwner.GetID();
		creation.m_World = producerOwner.GetWorld();
		creation.m_ProducerRpl = RplComponent.Cast(producerOwner.FindComponent(RplComponent));
		if (creation.m_ProducerRpl) creation.m_sProducerRpl = creation.m_ProducerRpl.Id().ToString();
		creation.m_bHeaderOnly = headerOnly;
		creation.m_iHeaderUnits = basket.m_iAvailable;
		creation.m_iNativeActiveBefore = m_World.GetCurrentNumOfActiveAIs();
		creation.m_iNativeIntendedBefore = m_World.GetActiveGroupIntendedCount();
		if (!creation.IsProducerCurrent()) return null;
		if (!headerOnly)
		{
			creation.m_Ticket = m_Ledger.BeginDispatch(basket, serial);
			if (!creation.m_Ticket) return null;
		}
		creation.m_Status = AICF_EAIQuotaBirthStatus.IN_NATIVE_FACTORY;
		m_CurrentCreation = creation;
		m_bNativeBirthBusy = true;
		return creation;
	}

	bool MarkExternalNativeCallStarted(AICF_AIQuotaCreation creation, bool commitAttempt = false)
	{
		if (!IsCurrentWorld() || !creation || creation != m_CurrentCreation || creation.m_Service != this ||
			!m_bNativeBirthBusy || creation.m_bFinished || creation.m_bNativeCallStarted || creation.m_bFactoryObserved ||
			creation.m_Status != AICF_EAIQuotaBirthStatus.IN_NATIVE_FACTORY || !creation.IsProducerCurrent()) return false;
		// Repeat after the producer guard's virtual/native read boundary.
		if (!IsCurrentWorld() || creation != m_CurrentCreation || creation.m_bFinished || !m_bNativeBirthBusy || creation.m_bNativeCallStarted) return false;
		if (commitAttempt)
		{
			// Sample the deadline clock after every native producer/replication read.
			int nowMs = System.GetTickCount();
			if (!creation.m_Guard.CommitNativeAttempt(nowMs)) return false;
		}
		creation.m_bNativeCallStarted = true;
		return true;
	}

	void ObserveExternalFactory(AICF_AIQuotaCreation creation, IEntity entity)
	{
		if (!IsCurrentWorld() || !creation || creation != m_CurrentCreation || creation.m_Service != this ||
			!m_bNativeBirthBusy || creation.m_bFactoryObserved || creation.m_bFinished) return;
		creation.m_bFactoryObserved = true;
		creation.m_Entity = entity;
		if (!creation.m_bNativeCallStarted)
		{
			RejectExternalCreation(creation, "NATIVE_FACTORY_NOT_MARKED");
			return;
		}
		if (RetainQuarantinedExternalReturn(creation, entity)) return;
		AICF_AIQuotaCreationGuard guard = creation.m_Guard;
		AICF_AIQuotaReservation reservation = creation.m_Reservation;
		AICF_AIQuotaDispatch ticket = creation.m_Ticket;
		if (!entity)
		{
			if (creation.m_Ticket && !m_Ledger.FailDispatch(creation.m_Ticket, creation.m_iSerial))
			{
				creation.m_Status = AICF_EAIQuotaBirthStatus.QUARANTINED;
				creation.m_sRejectReason = "NULL_TICKET_MISMATCH";
				return;
			}
			creation.m_Status = AICF_EAIQuotaBirthStatus.NATIVE_NULL;
			return;
		}
		creation.m_Body = AICF_AIQuotaBody.Capture(entity, creation.m_Reservation.m_Bucket);
		AICF_AIQuotaBody observedBody = creation.m_Body;
		if (!IsExternalObservationCurrent(creation, guard, reservation, ticket, observedBody, entity)) return;
		if (creation.m_Body && creation.m_Body.m_bVehicle) creation.m_Body.m_CreationGuard = creation.m_Guard;
		bool valid = creation.m_Body && creation.IsProducerCurrent() && SCR_ResourceNameUtils.GetPrefabName(entity) == creation.m_Prefab;
		if (!IsExternalObservationCurrent(creation, guard, reservation, ticket, observedBody, entity)) return;
		if (creation.m_bHeaderOnly)
		{
			bool headerValid = creation.IsHeaderValid();
			if (!IsExternalObservationCurrent(creation, guard, reservation, ticket, observedBody, entity)) return;
			valid = valid && headerValid && m_World.GetCurrentNumOfActiveAIs() == creation.m_iNativeActiveBefore &&
				m_World.GetActiveGroupIntendedCount() == creation.m_iNativeIntendedBefore;
			if (!IsExternalObservationCurrent(creation, guard, reservation, ticket, observedBody, entity)) return;
		}
		else
		{
			bool producedKind;
			bool expectsVehicle = guard.ExpectsVehicle();
			if (!IsExternalObservationCurrent(creation, guard, reservation, ticket, observedBody, entity)) return;
			if (expectsVehicle) producedKind = Vehicle.Cast(entity) != null;
			else producedKind = ChimeraCharacter.Cast(entity) && AICF_AIQuotaBirth.IsNonPlayerUnassigned(entity);
			valid = valid && producedKind &&
				!FindBody(entity) && m_aBodies.Count() < AICF_AIQuotaMath.MAX_TOTAL_UNITS;
			if (!IsExternalObservationCurrent(creation, guard, reservation, ticket, observedBody, entity)) return;
			if (valid) valid = m_Ledger.Bind(creation.m_Ticket, creation.m_iSerial, creation.m_Body.Key());
			if (valid) m_aBodies.Insert(creation.m_Body);
		}
		if (!valid)
		{
			creation.m_Status = AICF_EAIQuotaBirthStatus.QUARANTINED;
			creation.m_sRejectReason = "EXTERNAL_NATIVE_BODY_OR_HEADER_MISMATCH";
			return;
		}
		creation.m_Status = AICF_EAIQuotaBirthStatus.BOUND;
	}

	protected bool IsExternalObservationCurrent(AICF_AIQuotaCreation creation, AICF_AIQuotaCreationGuard guard,
		AICF_AIQuotaReservation reservation, AICF_AIQuotaDispatch ticket, AICF_AIQuotaBody body, IEntity entity)
	{
		bool current = IsCurrentWorld() && creation && creation == m_CurrentCreation && creation.m_Service == this &&
			!creation.m_bFinished && creation.m_bFactoryObserved && creation.m_bNativeCallStarted && m_bNativeBirthBusy;
		if (current)
			current = creation.m_Status == AICF_EAIQuotaBirthStatus.IN_NATIVE_FACTORY && creation.m_Guard == guard &&
				creation.m_Reservation == reservation && creation.m_Ticket == ticket && creation.m_Body == body && creation.m_Entity == entity;
		if (!current && creation && creation == m_QuarantinedCreation && creation.m_Body)
			creation.m_Body.m_bQuarantined = true;
		return current;
	}

	bool EndExternalCreation(AICF_AIQuotaCreation creation)
	{
		if (!creation || creation != m_CurrentCreation || creation.m_Service != this || !m_bNativeBirthBusy || creation.m_bFinished) return false;
		if (creation.m_bNativeCallStarted && !creation.m_bFactoryObserved) return false;
		if (!creation.m_bFactoryObserved && creation.m_Status != AICF_EAIQuotaBirthStatus.VETO &&
			creation.m_Status != AICF_EAIQuotaBirthStatus.QUARANTINED) return false;
		if (creation.m_Status == AICF_EAIQuotaBirthStatus.IN_NATIVE_FACTORY) return false;
		creation.m_bFinished = true;
		m_CurrentCreation = null;
		m_bNativeBirthBusy = false;
		return true;
	}

	bool CancelExternalCreationBeforeFactory(AICF_AIQuotaCreation creation)
	{
		if (!creation || creation != m_CurrentCreation || creation.m_Service != this || creation.m_bFactoryObserved ||
			creation.m_bNativeCallStarted || creation.m_bFinished) return false;
		if (creation.m_Ticket && !m_Ledger.FailDispatch(creation.m_Ticket, creation.m_iSerial)) return false;
		creation.m_Status = AICF_EAIQuotaBirthStatus.VETO;
		return EndExternalCreation(creation);
	}

	bool CanRollbackExternalCreation(AICF_AIQuotaCreation creation)
	{
		if (!IsCurrentWorld() || !creation || creation != m_CurrentCreation || creation.m_Service != this || !m_bNativeBirthBusy ||
			creation.m_bFinished || !creation.m_bFactoryObserved || !creation.m_Entity || !creation.m_Body ||
			creation.m_Body.m_Entity != creation.m_Entity || !creation.m_Body.MatchesIdentity() ||
			SCR_ResourceNameUtils.GetPrefabName(creation.m_Entity) != creation.m_Prefab) return false;
		if (creation.m_bHeaderOnly)
		{
			SCR_AIGroup group = SCR_AIGroup.Cast(creation.m_Entity);
			return group && group.GetAgentsCount() == 0 && group.GetPlayerCount() == 0 &&
				(!creation.IsProducerCurrent() || !creation.IsHeaderValid());
		}
		return !creation.IsProducerCurrent() && AICF_AIQuotaBirth.IsNonPlayerUnassigned(creation.m_Entity);
	}

	bool ObserveExternalAbsence(AICF_AIQuotaCreation creation)
	{
		if (!creation || creation != m_CurrentCreation || creation.m_Service != this || creation.m_bFinished ||
			!creation.m_bFactoryObserved || !creation.m_Body || creation.m_Body.m_Entity != creation.m_Entity || !creation.m_Body.IsDefinitelyGone()) return false;
		if (creation.m_Ticket)
		{
			if (creation.m_Ticket.m_bClosed)
			{
				creation.m_Body.m_bReleased = true;
				m_Ledger.RetireBinding(creation.m_Body.Key());
			}
			else if (!m_Ledger.FailDispatch(creation.m_Ticket, creation.m_iSerial)) return false;
		}
		creation.m_Status = AICF_EAIQuotaBirthStatus.ROLLED_BACK;
		return EndExternalCreation(creation);
	}

	void RejectExternalCreation(AICF_AIQuotaCreation creation, string reason)
	{
		if (!creation || creation != m_CurrentCreation || creation.m_Service != this || creation.m_bFinished || !m_bNativeBirthBusy) return;
		if (m_QuarantinedCreation == creation)
		{
			EndExternalCreation(creation);
			return;
		}
		m_QuarantinedCreation = creation;
		creation.m_Status = AICF_EAIQuotaBirthStatus.QUARANTINED;
		if (creation.m_Body) creation.m_Body.m_bQuarantined = true;
		m_bQuarantined = true;
		m_sLastReason = reason;
		AICF_Stage1Diagnostics.Error("AI_QUOTA_EXTERNAL_BIRTH_QUARANTINED", string.Format(
			"reason=%1 entity=%2 prefab=%3 header=%4 native_null=0 retry_factory_allowed=0",
			reason, creation.m_Entity, creation.m_Prefab, creation.m_bHeaderOnly));
		if (creation == m_CurrentCreation) EndExternalCreation(creation);
	}

	protected bool RetainQuarantinedExternalReturn(AICF_AIQuotaCreation creation, IEntity entity)
	{
		if (!IsCurrentWorld() || !creation || creation != m_CurrentCreation || creation != m_QuarantinedCreation || creation.m_Service != this || creation.m_Entity != entity ||
			creation.m_bFinished || !creation.m_bFactoryObserved || !creation.m_bNativeCallStarted || !m_bNativeBirthBusy) return false;
		if (entity)
		{
			creation.m_Body = AICF_AIQuotaBody.Capture(entity, creation.m_Reservation.m_Bucket);
			if (creation.m_Body) creation.m_Body.m_bQuarantined = true;
		}
		else if (creation.m_Ticket && !creation.m_Ticket.m_bClosed)
			m_Ledger.FailDispatch(creation.m_Ticket, creation.m_iSerial);
		creation.m_Status = AICF_EAIQuotaBirthStatus.QUARANTINED;
		string identity = "UNKNOWN";
		if (creation.m_Body) identity = creation.m_Body.Key();
		AICF_Stage1Diagnostics.Info("AI_QUOTA_QUARANTINED_NATIVE_RETURN", string.Format(
			"kind=EXTERNAL exact_receipt_retained=1 native_nonnull=%1 body_identity_captured=%2 prior_failure_retained=1 permission_reused=0 entity_key=%3 owner_serial=%4",
			entity != null, creation.m_Body != null, identity, creation.m_iSerial));
		EndExternalCreation(creation);
		return true;
	}
}

modded class SCR_AIWorld
{
	protected ref AICF_AIWorldQuotaService m_AICFQuotaService;

	AICF_AIWorldQuotaService AICF_FindQuotaService()
	{
		if (!Replication.IsServer() || !GetGame() || GetGame().GetAIWorld() != this || GetWorld() != GetGame().GetWorld()) return null;
		return m_AICFQuotaService;
	}

	AICF_AIWorldQuotaService AICF_PrepareQuotaService()
	{
		if (!Replication.IsServer() || !GetGame() || GetGame().GetAIWorld() != this || GetWorld() != GetGame().GetWorld()) return null;
		if (!m_AICFQuotaService) m_AICFQuotaService = new AICF_AIWorldQuotaService(this, AICF_GetQuotaWorldState());
		return m_AICFQuotaService;
	}
}

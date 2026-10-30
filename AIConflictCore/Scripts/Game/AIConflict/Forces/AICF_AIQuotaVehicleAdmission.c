class AICF_AIQuotaVehiclePermit
{
	AICF_AIWorldQuotaService m_Service;
	ref AICF_AIQuotaVehicleEntry m_Entry;
	ref AICF_AIQuotaVehicleOwner m_Owner;
	AICF_AIQuotaBody m_Body;
	int m_iEpoch;
	bool m_bDispatched;
	bool m_bHasNativeResult;
	bool m_bNativeAccepted;
	bool m_bCancelRequested;
	bool m_bClosed;
	bool m_bUnknownLogged;
}

// Единственный владелец car holds/pending entries. Ни action, ни seat helper
// не освобождают unit только по собственному завершению/таймауту.
modded class AICF_AIWorldQuotaService
{
	protected ref array<ref AICF_AIQuotaVehiclePermit> m_aVehiclePermits = {};
	protected ref array<ref AICF_AIQuotaCompartmentCreation> m_aCompartmentCreations = {};
	protected ref array<string> m_aVehicleTransferWait = {};
	protected AICF_MatchController m_VehicleBootstrapController;
	protected int m_iVehicleBootstrapSerial;

	override protected int BeginQuotaVehicleBootstrap(AICF_MatchController controller)
	{
		if (!IsCurrentWorld() || !controller || !controller.IsAIQuotaOwnerCurrent(m_World) || m_bNativeBirthBusy ||
			m_VehicleBootstrapController || m_iVehicleBootstrapSerial == int.MAX) return 0;
		m_iVehicleBootstrapSerial++;
		m_VehicleBootstrapController = controller;
		return m_iVehicleBootstrapSerial;
	}

	override protected bool EndQuotaVehicleBootstrap(AICF_MatchController controller, int serial)
	{
		if (!m_VehicleBootstrapController || controller != m_VehicleBootstrapController || serial != m_iVehicleBootstrapSerial) return false;
		m_VehicleBootstrapController = null;
		return true;
	}

	AICF_MatchController GetQuotaVehicleBootstrapController() { return m_VehicleBootstrapController; }
	int GetQuotaVehicleBootstrapSerial() { return m_iVehicleBootstrapSerial; }

	bool IsQuotaVehicleBootstrapCurrent(AICF_MatchController controller, int serial)
	{
		return IsCurrentWorld() && controller && controller == m_VehicleBootstrapController && serial > 0 &&
			serial == m_iVehicleBootstrapSerial && controller.IsAIQuotaOwnerCurrent(m_World) && !m_bNativeBirthBusy;
	}

	protected AICF_AIQuotaVehicleOwner CaptureVehicleCensusOwner(IEntity vehicle)
	{
		if (m_VehicleBootstrapController) return AICF_AIQuotaVehicleOwner.CaptureForCensus(this, vehicle);
		return AICF_AIQuotaVehicleOwner.Capture(this, vehicle);
	}

	protected bool IsVehiclePermitOwned(AICF_AIQuotaVehiclePermit permit)
	{
		return IsCurrentWorld() && permit && permit.m_Service == this && m_aVehiclePermits.Contains(permit);
	}

	protected bool HasVehiclePending(AICF_AIQuotaBody body)
	{
		foreach (AICF_AIQuotaVehiclePermit permit : m_aVehiclePermits)
			if (permit && !permit.m_bClosed && permit.m_Body == body) return true;
		foreach (AICF_AIQuotaCompartmentCreation creation : m_aCompartmentCreations)
			if (creation && !creation.m_bClosed && creation.m_CarBody == body) return true;
		return false;
	}

	protected bool IsVehicleBucket(AICF_EAIQuotaBucket bucket)
	{
		return bucket == AICF_EAIQuotaBucket.STOCK_VEHICLES || bucket == AICF_EAIQuotaBucket.FLEET ||
			bucket == AICF_EAIQuotaBucket.COMBAT || bucket == AICF_EAIQuotaBucket.FIA_EXTRA;
	}

	protected bool IsVehicleEntrySettled(AICF_AIQuotaVehicleEntry entry)
	{
		if (!entry || !entry.m_ActorIdentity || !entry.m_VehicleIdentity || !entry.m_SeatOwnerIdentity) return false;
		if (!entry.m_ActorIdentity.IsCurrent() || !entry.m_VehicleIdentity.IsCurrent() || !entry.m_SeatOwnerIdentity.IsCurrent() || !entry.IsSeatCurrent()) return false;
		ChimeraCharacter actor = entry.m_Actor;
		BaseCompartmentSlot seat = entry.m_Seat;
		if (!actor || !seat || seat.GetOccupant() != actor || !actor.IsInVehicle()) return false;
		CompartmentAccessComponent access = actor.GetCompartmentAccessComponent();
		return access && access.GetCompartment() == seat && !access.IsGettingIn() && !access.IsGettingOut() &&
			!access.IsSwitchingSeatsAnim() && CompartmentAccessComponent.GetVehicleIn(actor) == entry.m_Vehicle;
	}

	protected bool IsVehicleEntryActorGone(AICF_AIQuotaVehicleEntry entry)
	{
		if (!entry || !entry.m_ActorIdentity || !entry.m_ActorIdentity.m_World || !GetGame() ||
			GetGame().GetWorld() != entry.m_ActorIdentity.m_World) return false;
		return !entry.m_ActorIdentity.m_Entity && !entry.m_ActorIdentity.m_World.FindEntityByID(entry.m_ActorIdentity.m_Id);
	}

	protected void ReconcileVehiclePermits()
	{
		for (int sourceIndex = m_aCompartmentCreations.Count() - 1; sourceIndex >= 0; sourceIndex--)
		{
			AICF_AIQuotaCompartmentCreation source = m_aCompartmentCreations[sourceIndex];
			if (!source || !source.m_CarBody)
			{
				m_bQuarantined = true;
				m_sLastReason = "COMPARTMENT_PRODUCER_CUSTODY_UNKNOWN";
				return;
			}
			if (source.m_CarBody.IsDefinitelyGone() || (source.m_CharacterBody && source.m_CharacterBody.IsDefinitelyGone()))
				source.m_bClosed = true;
			if (!source.m_bClosed && source.m_bSourceEnded && source.m_Newborn && source.m_Newborn.IsCurrent() && source.IsSeatScopeCurrent())
			{
				ChimeraCharacter actor = ChimeraCharacter.Cast(source.m_Newborn.m_Entity);
				CompartmentAccessComponent access;
				if (actor) access = actor.GetCompartmentAccessComponent();
				if (access && source.m_SeatScope.m_Seat.GetOccupant() == actor && access.GetCompartment() == source.m_SeatScope.m_Seat &&
					!access.IsGettingIn() && !access.IsGettingOut() && !access.IsSwitchingSeatsAnim() &&
					CompartmentAccessComponent.GetVehicleIn(actor) == source.m_SeatScope.m_Vehicle)
					source.m_bClosed = true;
			}
			if (source.m_bClosed) m_aCompartmentCreations.Remove(sourceIndex);
		}
		for (int index = m_aVehiclePermits.Count() - 1; index >= 0; index--)
		{
			AICF_AIQuotaVehiclePermit permit = m_aVehiclePermits[index];
			if (!permit || !permit.m_Entry || !permit.m_Body)
			{
				m_bQuarantined = true;
				m_sLastReason = "VEHICLE_PERMIT_CUSTODY_UNKNOWN";
				return;
			}
			if (!permit.m_bClosed && permit.m_Body)
			{
				if (permit.m_Body.IsDefinitelyGone() || IsVehicleEntryActorGone(permit.m_Entry) || IsVehicleEntrySettled(permit.m_Entry))
					permit.m_bClosed = true;
			}
			if (permit.m_bClosed) m_aVehiclePermits.Remove(index);
		}
	}

	protected void RetireVehicleBody(AICF_AIQuotaBody body)
	{
		if (!body || body.m_bReleased) return;
		body.m_bReleased = true;
		body.m_CreationGuard = null;
		if (m_Ledger) m_Ledger.RetireBinding(body.Key());
		AICF_Stage1Diagnostics.Info("AI_QUOTA_VEHICLE_HOLD_RETIRED", string.Format(
			"vehicle=%1 rpl=%2 source_bucket=%3 pending=0 native_inactive_or_absent=1", body.m_Id, body.m_sRpl, body.m_Bucket));
	}

	protected bool VehicleCharge(AICF_AIQuotaVehicleOwner owner, AICF_AIQuotaVehicleUse use,
		AICF_AIQuotaBody body, out AICF_EAIQuotaBucket bucket, out bool charge)
	{
		bucket = AICF_EAIQuotaBucket.STOCK_VEHICLES;
		charge = false;
		if (!owner || !owner.IsCurrent() || !use || !use.IsCurrent()) return false;
		bucket = owner.m_Bucket;
		if (!body)
		{
			charge = use.m_bActive || use.m_bHasAI;
			return true;
		}
		if (!body.MatchesIdentity() || body.m_bQuarantined || !body.m_bVehicle || body.m_bReleased || !IsVehicleBucket(body.m_Bucket)) return false;
		if (body.m_CreationGuard && owner.m_Lease && body.m_CreationGuard.OwnsVehicleLease(owner.m_Lease, body.m_Entity) && owner.IsCurrent())
			body.m_CreationGuard = null;
		// Потеря producer context не доказывает исчезновение его newborn car.
		// До exact own-bind receipt либо actual absence исходный unit остаётся
		// в custody даже у пустой неактивной машины со stale guard.
		bool provisional = body.m_CreationGuard != null;
		if (provisional && body.m_CreationGuard.GetBucket() != body.m_Bucket) return false;
		if (!provisional && !use.m_bActive && !use.m_bHasAI && !HasVehiclePending(body) && !owner.m_Lease)
		{
			// Полный actual empty/inactive unleased car. Временный gate exit
			// под прежним lease или pending native queue не освобождает unit.
			if (!use.IsCurrent() || !owner.IsCurrent() || HasVehiclePending(body)) return false;
			RetireVehicleBody(body);
			return true;
		}
		bucket = body.m_Bucket;
		charge = true;
		return true;
	}

	override protected bool ClassifyQuotaVehicleRow(AICF_AIQuotaAgentRow row, out AICF_EAIQuotaBucket bucket, out bool charge)
	{
		bucket = AICF_EAIQuotaBucket.STOCK_VEHICLES;
		charge = false;
		if (!row || !row.m_bVehicle || !row.m_Entity || !row.IsCurrent(m_World.GetWorld())) return false;
		ReconcileVehiclePermits();
		AICF_AIQuotaVehicleOwner owner = CaptureVehicleCensusOwner(row.m_Entity);
		AICF_AIQuotaVehicleUse use = AICF_AIQuotaVehicleUse.CaptureRow(row.m_Entity, row);
		return VehicleCharge(owner, use, FindBody(row.m_Entity), bucket, charge);
	}

	override protected bool AddQuotaVehicleHolds(AICF_AIQuotaCensus snapshot, array<int> counts, array<IEntity> counted)
	{
		if (!snapshot || !counts || !counted) return false;
		if (!m_Ledger) return m_VehicleBootstrapController && m_aBodies.IsEmpty() && m_aVehiclePermits.IsEmpty();
		ReconcileVehiclePermits();
		for (int index = m_aBodies.Count() - 1; index >= 0; index--)
		{
			AICF_AIQuotaBody body = m_aBodies[index];
			if (!body) return false;
			if (!body.m_bVehicle) continue;
			if (body.m_bReleased)
			{
				if (!HasVehiclePending(body)) m_aBodies.Remove(index);
				continue;
			}
			if (body.IsDefinitelyGone())
			{
				RetireVehicleBody(body);
				continue;
			}
			AICF_AIQuotaVehicleOwner owner = CaptureVehicleCensusOwner(body.m_Entity);
			AICF_AIQuotaVehicleUse use = AICF_AIQuotaVehicleUse.Capture(body.m_Entity, snapshot);
			AICF_EAIQuotaBucket bucket;
			bool charge;
			if (!VehicleCharge(owner, use, body, bucket, charge)) return false;
			if (!charge) continue;
			if (!counted.Contains(body.m_Entity))
			{
				if (bucket < 0 || bucket >= counts.Count() || counts[bucket] >= AICF_AIQuotaMath.MAX_TOTAL_UNITS) return false;
				counts[bucket] = counts[bucket] + 1;
				counted.Insert(body.m_Entity);
			}
			if (!owner.m_bCensusOnly && !body.m_CreationGuard && body.m_Bucket != owner.m_Bucket)
			{
				AICF_EAIQuotaBucket source = body.m_Bucket;
				AICF_EAIQuotaBucket target = owner.m_Bucket;
				if (counts[source] <= 0) return false;
				bool room = AICF_AIQuotaMath.FitsBucket(counts[target], m_Ledger.GetReserved(target), m_Limits.GetLimit(target), 1);
				if (room && TransferVehicleHold(body, owner))
				{
					counts[source] = counts[source] - 1;
					counts[target] = counts[target] + 1;
				}
				else if (!room) ReportVehicleTransferWait(body, owner);
			}
		}
		return snapshot.IsCurrent();
	}

	// Idempotent после КАЖДОГО successful ApplyCensus. Уже counted car получает
	// только binding-key, а не дополнительное увеличение physical occupancy.
	override protected bool InitializeQuotaVehicleBindings(AICF_AIQuotaCensus snapshot)
	{
		if (!snapshot || !m_Ledger) return false;
		for (int index; index < snapshot.Count(); index++)
		{
			AICF_AIQuotaAgentRow row = snapshot.GetRow(index);
			if (!row || !row.m_bVehicle) continue;
			AICF_EAIQuotaBucket bucket;
			bool charge;
			if (!ClassifyQuotaVehicleRow(row, bucket, charge)) return false;
			if (!charge) continue;
			AICF_AIQuotaBody body = FindBody(row.m_Entity);
			if (!body)
			{
				body = AICF_AIQuotaBody.Capture(row.m_Entity, bucket);
				if (!body || m_aBodies.Count() >= AICF_AIQuotaMath.MAX_TOTAL_UNITS || !m_Ledger.RegisterExistingBinding(body.Key(), bucket)) return false;
				m_aBodies.Insert(body);
			}
			else if (!m_Ledger.RegisterExistingBinding(body.Key(), body.m_Bucket)) return false;
		}
		return snapshot.IsCurrent();
	}

	protected bool NativeVehicleEntryAllowed(AICF_AIQuotaVehicleEntry entry, AICF_AIQuotaVehicleUse use)
	{
		if (!entry || !entry.IsCurrent() || !use || !use.IsCurrent()) return false;
		if (use.m_bActive && use.m_bCounted) return true;
		ChimeraAIWorld nativeWorld = ChimeraAIWorld.Cast(m_World);
		return nativeWorld && nativeWorld.CanAIBeActivated() && nativeWorld.CanActivateGroup(entry.m_Group) &&
			nativeWorld.CanLimitedAIBeAddedForFaction(entry.m_sGroupFactionKey);
	}

	bool PeekVehicleEntry(AICF_AIQuotaVehicleEntry entry)
	{
		if (!IsOwnerCurrent() || !IsConfigured() || IsQuotaAdmissionBusy() || !entry || !entry.IsCurrent() || !m_Census) return false;
		if (!m_Census.HasCompleteCapture() || m_Ledger.GetPhysical(AICF_EAIQuotaBucket.STOCK) < 0) return false;
		if (m_World.GetCurrentNumOfActiveAIs() != m_Census.GetNativeActive() || m_World.GetActiveGroupIntendedCount() != m_Census.GetNativeIntended()) return false;
		AICF_AIQuotaVehicleOwner owner = AICF_AIQuotaVehicleOwner.Capture(this, entry.m_Vehicle);
		AICF_AIQuotaVehicleUse use = AICF_AIQuotaVehicleUse.Capture(entry.m_Vehicle, m_Census);
		if (!owner || !owner.CanEnter() || !NativeVehicleEntryAllowed(entry, use)) return false;
		AICF_AIQuotaBody body = FindBody(entry.m_Vehicle);
		if (body && (!body.MatchesIdentity() || body.m_bQuarantined)) return false;
		if (body && (!IsVehicleBucket(body.m_Bucket) || m_Ledger.GetBindingBucket(body.Key()) != body.m_Bucket)) return false;
		if (body && body.m_CreationGuard && (!owner.m_Lease || !body.m_CreationGuard.OwnsVehicleLease(owner.m_Lease, body.m_Entity))) return false;
		bool room = body && body.m_Bucket == owner.m_Bucket;
		if (!room) room = m_Ledger.CanReserve(owner.m_Bucket, 1);
		return room && entry.IsCurrent() && owner.IsCurrent() && use.IsCurrent();
	}

	protected bool TransferVehicleHold(AICF_AIQuotaBody body, AICF_AIQuotaVehicleOwner owner)
	{
		if (!body || !owner || !owner.IsCurrent() || !body.MatchesIdentity()) return false;
		if (body.m_Bucket == owner.m_Bucket) return true;
		AICF_EAIQuotaBucket source = body.m_Bucket;
		if (!m_Ledger.TransferBinding(body.Key(), source, owner.m_Bucket))
		{
			ReportVehicleTransferWait(body, owner);
			return false;
		}
		body.m_Bucket = owner.m_Bucket;
		AICF_Stage1Diagnostics.Info("AI_QUOTA_VEHICLE_TRANSFERRED", string.Format(
			"vehicle=%1 rpl=%2 source_bucket=%3 target_bucket=%4 units=1", body.m_Id, body.m_sRpl, source, body.m_Bucket));
		return true;
	}

	protected void ReportVehicleTransferWait(AICF_AIQuotaBody body, AICF_AIQuotaVehicleOwner owner)
	{
		if (!body || !owner) return;
		string key = body.Key() + "/" + owner.m_Bucket;
		if (m_aVehicleTransferWait.Contains(key)) return;
		if (m_aVehicleTransferWait.Count() < AICF_AIQuotaMath.MAX_TOTAL_UNITS) m_aVehicleTransferWait.Insert(key);
		AICF_Stage1Diagnostics.Info("AI_QUOTA_VEHICLE_TRANSFER_WAIT", string.Format(
			"vehicle=%1 rpl=%2 source_bucket=%3 target_bucket=%4 retained_source_charge=1 target_room_unproven=1 source_reusable=0",
			body.m_Id, body.m_sRpl, body.m_Bucket, owner.m_Bucket));
	}

	AICF_AIQuotaVehiclePermit BeginVehicleEntry(AICF_AIQuotaVehicleEntry entry)
	{
		if (!IsOwnerCurrent() || !IsConfigured() || IsQuotaAdmissionBusy() || !entry || !entry.IsCurrent() || !RefreshCensus()) return null;
		AICF_AIQuotaCensus capturedCensus = m_Census;
		ReconcileVehiclePermits();
		if (m_aVehiclePermits.Count() >= AICF_AIQuotaMath.MAX_TOTAL_UNITS) return null;
		AICF_AIQuotaVehicleOwner owner = AICF_AIQuotaVehicleOwner.Capture(this, entry.m_Vehicle);
		AICF_AIQuotaVehicleUse use = AICF_AIQuotaVehicleUse.Capture(entry.m_Vehicle, m_Census);
		if (!owner || !owner.CanEnter() || !NativeVehicleEntryAllowed(entry, use)) return null;
		if (m_Census != capturedCensus || !capturedCensus || !capturedCensus.IsCurrent()) return null;
		AICF_AIQuotaBody body = FindBody(entry.m_Vehicle);
		if (body)
		{
			if (!IsVehicleBucket(body.m_Bucket) || m_Ledger.GetBindingBucket(body.Key()) != body.m_Bucket) return null;
			if (body.m_CreationGuard) return null;
			if (body.m_bQuarantined || !TransferVehicleHold(body, owner)) return null;
		}
		else
		{
			int serial = AllocateContextSerial();
			if (serial <= 0 || m_aBodies.Count() >= AICF_AIQuotaMath.MAX_TOTAL_UNITS) return null;
			AICF_AIQuotaReservation reservation = m_Ledger.Reserve(owner.m_Bucket, 1, serial);
			if (!reservation) return null;
			AICF_AIQuotaDispatch ticket = m_Ledger.BeginDispatch(reservation, serial);
			body = AICF_AIQuotaBody.Capture(entry.m_Vehicle, owner.m_Bucket);
			if (!ticket || !body || !entry.IsCurrent() || !owner.IsCurrent() || !use.IsCurrent())
			{
				if (ticket) m_Ledger.FailDispatch(ticket, serial);
				m_Ledger.CancelAvailable(reservation, serial);
				return null;
			}
			if (!m_Ledger.Bind(ticket, serial, body.Key()))
			{
				m_Ledger.FailDispatch(ticket, serial);
				m_Ledger.CancelAvailable(reservation, serial);
				return null;
			}
			m_aBodies.Insert(body);
		}
		if (!entry.IsCurrent() || !owner.IsCurrent() || !body.MatchesIdentity() || m_Census != capturedCensus || !capturedCensus.IsCurrent()) return null;
		if (m_Ledger.GetPhysical(AICF_EAIQuotaBucket.STOCK) < 0) return null;
		AICF_AIQuotaVehiclePermit permit = new AICF_AIQuotaVehiclePermit();
		permit.m_Service = this;
		permit.m_Entry = entry;
		permit.m_Owner = owner;
		permit.m_Body = body;
		permit.m_iEpoch = m_iEpoch;
		m_aVehiclePermits.Insert(permit);
		return permit;
	}

	bool IsVehicleEntryPermitCurrent(AICF_AIQuotaVehiclePermit permit, AICF_AIQuotaVehicleEntry entry)
	{
		if (!IsVehiclePermitOwned(permit) || !IsOwnerCurrent() || !IsConfigured() || permit.m_bClosed || permit.m_bCancelRequested) return false;
		if (m_Ledger.GetPhysical(AICF_EAIQuotaBucket.STOCK) < 0) return false;
		if (permit.m_iEpoch != m_iEpoch || permit.m_Entry != entry || !entry || !entry.IsCurrent()) return false;
		if (!permit.m_Owner || !permit.m_Owner.CanEnter() || !permit.m_Body || !permit.m_Body.MatchesIdentity()) return false;
		return !permit.m_Body.m_bReleased && !permit.m_Body.m_bQuarantined && permit.m_Body.m_Bucket == permit.m_Owner.m_Bucket;
	}

	void MarkVehicleEntryDispatched(AICF_AIQuotaVehiclePermit permit)
	{
		if (IsVehiclePermitOwned(permit) && !permit.m_bClosed && !permit.m_bCancelRequested) permit.m_bDispatched = true;
	}

	void ObserveVehicleEntryResult(AICF_AIQuotaVehiclePermit permit, bool nativeAccepted)
	{
		if (!IsVehiclePermitOwned(permit) || permit.m_bHasNativeResult) return;
		permit.m_bHasNativeResult = true;
		permit.m_bNativeAccepted = nativeAccepted;
		permit.m_bDispatched = true;
		if (!nativeAccepted || IsVehicleEntrySettled(permit.m_Entry)) permit.m_bClosed = true;
	}

	void CancelVehicleEntry(AICF_AIQuotaVehiclePermit permit)
	{
		if (!IsVehiclePermitOwned(permit) || permit.m_bClosed) return;
		permit.m_bCancelRequested = true;
		if (!permit.m_bDispatched || IsVehicleEntrySettled(permit.m_Entry) || IsVehicleEntryActorGone(permit.m_Entry))
			permit.m_bClosed = true;
		else if (!permit.m_bUnknownLogged && permit.m_Body && permit.m_Entry && permit.m_Entry.m_ActorIdentity)
		{
			permit.m_bUnknownLogged = true;
			AICF_Stage1Diagnostics.Info("AI_QUOTA_VEHICLE_ENTRY_PENDING_UNKNOWN", string.Format(
				"vehicle=%1 actor=%2 has_native_result=%3 accepted=%4 source_ended=1 retained_hold=1 queue_abort_proven=0",
				permit.m_Body.m_Id, permit.m_Entry.m_ActorIdentity.m_Id, permit.m_bHasNativeResult, permit.m_bNativeAccepted));
		}
	}

	// Car1 и character1 — две явные части одной typed операции. Первый hold
	// публикуется до ReserveUnits character, чтобы её Refresh не освободил
	// ещё пустую/inactive машину в промежутке между двумя reservations.
	AICF_AIQuotaCompartmentCreation BeginCompartmentCreation(BaseCompartmentSlot seat,
		ResourceName characterPrefab, AIGroup group, ResourceName groupPrefab)
	{
		if (!IsOwnerCurrent() || !IsConfigured() || IsQuotaAdmissionBusy() || !RefreshCensus()) return null;
		AICF_AIQuotaCompartmentCreation context = AICF_AIQuotaCompartmentCreation.Capture(this, seat, characterPrefab, group, groupPrefab);
		if (!context || !m_Owners || m_aCompartmentCreations.Count() >= AICF_AIQuotaMath.MAX_TOTAL_UNITS) return null;
		AICF_EAIQuotaBucket groupBucket;
		if (!m_Owners.ClassifyGroup(SCR_AIGroup.Cast(group), groupBucket) || groupBucket != AICF_EAIQuotaBucket.STOCK) return null;
		ChimeraAIWorld nativeWorld = ChimeraAIWorld.Cast(m_World);
		if (!nativeWorld || !nativeWorld.CanAIBeActivated() || !nativeWorld.CanActivateGroup(group) ||
			!nativeWorld.CanLimitedAIBeAddedForFaction(context.m_sFaction)) return null;
		AICF_AIQuotaVehicleOwner owner = AICF_AIQuotaVehicleOwner.Capture(this, context.m_SeatScope.m_Vehicle);
		// VehicleUse держит weak row. Character ReserveUnits обновляет census:
		// старый snapshot обязан жить до последнего повторного physical fence.
		ref AICF_AIQuotaCensus capturedCensus = m_Census;
		AICF_AIQuotaVehicleUse use = AICF_AIQuotaVehicleUse.Capture(context.m_SeatScope.m_Vehicle, capturedCensus);
		if (!owner || !owner.CanEnter() || !use || !use.IsCurrent()) return null;
		AICF_AIQuotaBody body = FindBody(context.m_SeatScope.m_Vehicle);
		if (body)
		{
			if (body.m_CreationGuard || body.m_bQuarantined || !IsVehicleBucket(body.m_Bucket) ||
				m_Ledger.GetBindingBucket(body.Key()) != body.m_Bucket || !TransferVehicleHold(body, owner)) return null;
		}
		else
		{
			if (m_aBodies.Count() >= AICF_AIQuotaMath.MAX_TOTAL_UNITS) return null;
			AICF_AIQuotaReservation carReservation = m_Ledger.Reserve(owner.m_Bucket, 1, context.m_iSerial);
			if (!carReservation) return null;
			AICF_AIQuotaDispatch ticket = m_Ledger.BeginDispatch(carReservation, context.m_iSerial);
			body = AICF_AIQuotaBody.Capture(context.m_SeatScope.m_Vehicle, owner.m_Bucket);
			if (!ticket || !body || !context.IsCurrent() || !owner.IsCurrent() || !use.IsCurrent() ||
				!m_Ledger.Bind(ticket, context.m_iSerial, body.Key()))
			{
				if (ticket) m_Ledger.FailDispatch(ticket, context.m_iSerial);
				m_Ledger.CancelAvailable(carReservation, context.m_iSerial);
				return null;
			}
			m_aBodies.Insert(body);
		}
		context.m_CarOwner = owner;
		context.m_CarBody = body;
		m_aCompartmentCreations.Insert(context);
		context.m_CharacterReservation = ReserveUnits(AICF_EAIQuotaBucket.STOCK, 1, context.m_iSerial);
		if (!context.m_CharacterReservation || !context.IsCurrent() || !capturedCensus.IsCurrent() || !use.IsCurrent())
		{
			CloseCompartmentCreation(context);
			return null;
		}
		return context;
	}

	bool OwnsCompartmentCreation(AICF_AIQuotaCompartmentCreation context)
	{
		return IsOwnerCurrent() && context && context.m_Service == this && context.m_iEpoch == m_iEpoch &&
			m_aCompartmentCreations.Contains(context) && !context.m_bClosed && context.IsCurrent();
	}

	bool IsCompartmentCarHoldCurrent(AICF_AIQuotaCompartmentCreation context)
	{
		if (!context || context.m_Service != this || !IsOwnerCurrent() || context.m_iEpoch != m_iEpoch || !m_Ledger) return false;
		AICF_AIQuotaBody body = context.m_CarBody;
		AICF_AIQuotaVehicleOwner owner = context.m_CarOwner;
		if (!body || !owner || body.m_bReleased || body.m_bQuarantined || !body.MatchesIdentity() || !owner.CanEnter()) return false;
		return body.m_Bucket == owner.m_Bucket && m_Ledger.GetBindingBucket(body.Key()) == body.m_Bucket &&
			m_Ledger.GetPhysical(AICF_EAIQuotaBucket.STOCK) >= 0;
	}

	AICF_AIQuotaCreation BeginCompartmentFactory(AICF_AIQuotaCompartmentCreation context)
	{
		if (!OwnsCompartmentCreation(context) || context.m_Creation || !context.m_CharacterReservation) return null;
		context.m_Creation = BeginExternalCreation(context.m_CharacterReservation, context.m_iSerial,
			context.m_CharacterPrefab, context.m_SeatScope.m_SeatOwnerIdentity.m_Entity, false, context);
		return context.m_Creation;
	}

	bool CaptureCompartmentNewborn(AICF_AIQuotaCompartmentCreation context, IEntity raw)
	{
		if (!OwnsCompartmentCreation(context) || !context.m_Creation || context.m_Creation != m_CurrentCreation ||
			!m_bNativeBirthBusy || context.m_Creation.m_bFinished || context.m_Creation.m_Guard != context ||
			context.m_Creation.m_Status != AICF_EAIQuotaBirthStatus.BOUND || context.m_Creation.m_Entity != raw ||
			!context.m_Creation.m_Body || FindBody(raw) != context.m_Creation.m_Body || !AICF_AIQuotaBirth.IsNonPlayerUnassigned(raw)) return false;
		context.m_Newborn = AICF_AIQuotaVehicleEntity.Capture(raw);
		context.m_CharacterBody = context.m_Creation.m_Body;
		ChimeraCharacter actor = ChimeraCharacter.Cast(raw);
		if (actor) context.m_Control = actor.GetAIControlComponent();
		return context.IsNewbornCurrent() && OwnsCompartmentCreation(context);
	}

	bool OwnsCurrentCompartmentNewborn(AICF_AIQuotaCompartmentCreation context)
	{
		return OwnsCompartmentCreation(context) && context.IsNewbornCurrent() && context.m_Creation == m_CurrentCreation &&
			m_bNativeBirthBusy && !context.m_Creation.m_bFinished && context.m_Creation.m_Guard == context &&
			FindBody(context.m_Newborn.m_Entity) == context.m_Creation.m_Body;
	}

	void ObserveCompartmentEntry(AICF_AIQuotaCompartmentCreation context, bool accepted)
	{
		if (!context || context.m_Service != this || !m_aCompartmentCreations.Contains(context) || context.m_bHasNativeResult) return;
		context.m_bNativeIssued = true;
		context.m_bHasNativeResult = true;
		context.m_bAccepted = accepted;
	}

	bool MarkCompartmentEntryDispatched(AICF_AIQuotaCompartmentCreation context)
	{
		if (!OwnsCurrentCompartmentNewborn(context) || context.m_bNativeIssued || context.m_bSourceEnded || context.m_bClosed) return false;
		context.m_bNativeIssued = true;
		return true;
	}

	void CloseCompartmentCreation(AICF_AIQuotaCompartmentCreation context)
	{
		if (!context || context.m_Service != this || !m_aCompartmentCreations.Contains(context) || context.m_bSourceEnded) return;
		context.m_bSourceEnded = true;
		if (context.m_CharacterReservation) CancelReservation(context.m_CharacterReservation, context.m_iSerial);
		if (!context.m_bNativeIssued || (context.m_bHasNativeResult && !context.m_bAccepted)) context.m_bClosed = true;
	}

	void QuarantineCompartmentCreation(AICF_AIQuotaCompartmentCreation context, string reason)
	{
		if (!context || context.m_Service != this || !m_aCompartmentCreations.Contains(context)) return;
		m_bQuarantined = true;
		m_sLastReason = reason;
		if (context.m_Creation && !context.m_Creation.m_bFinished) RejectExternalCreation(context.m_Creation, reason);
		AICF_Stage1Diagnostics.Error("AI_QUOTA_COMPARTMENT_QUARANTINED", reason);
	}

	override void StopOwner(AICF_MatchController controller)
	{
		if (IsCurrentWorld() && controller && controller == m_Controller)
		{
			foreach (AICF_AIQuotaCompartmentCreation source : m_aCompartmentCreations)
				if (source && source.m_iEpoch == m_iEpoch) CloseCompartmentCreation(source);
		}
		super.StopOwner(controller);
	}

	void ~AICF_AIWorldQuotaService()
	{
		// World уничтожен: script refs освобождаются без native действий и без
		// ложного receipts об исчезновении ещё существующих engine entities.
		m_aCompartmentCreations.Clear();
	}
}

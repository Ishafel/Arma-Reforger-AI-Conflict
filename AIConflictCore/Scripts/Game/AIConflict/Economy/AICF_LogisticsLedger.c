// Один authority-owned ledger обеих сторон. Reservations не списывают stock.
class AICF_LogisticsLedger
{
	ref AICF_LogisticsResourceAdapter m_Resources = new AICF_LogisticsResourceAdapter();
	ref array<ref AICF_LogisticsJob> m_aJobs = {};
	protected int m_iNextToken;
	protected bool m_bStopped;

	float Reserved(AICF_LogisticsResourcePool pool, bool incoming, AICF_LogisticsJob exclude = null)
	{
		float amount;
		int now = System.GetTickCount();
		foreach (AICF_LogisticsJob job : m_aJobs)
		{
			if (!job || job == exclude || job.m_bCancelled || now >= job.m_iExpiresAtMs) continue;
			if (incoming && job.m_Destination && job.m_Destination.m_Pool.Overlaps(pool)) amount += job.m_fReservedIncoming;
			if (!incoming && job.m_Source && job.m_Source.m_Pool.Overlaps(pool)) amount += job.m_fReservedSource;
		}
		return amount;
	}

	bool Reserve(AICF_LogisticsWorker w, AICF_LogisticsEndpoint source, AICF_LogisticsEndpoint destination, float amount, bool returning, AICF_LogisticsConfig config, int revision)
	{
		if (m_bStopped || !Replication.IsServer() || w.m_DriverInteraction || !w.Ready() || w.m_Job || !destination || !destination.IdentityValid() ||
			!(amount > 0) || (source && (!source.IdentityValid() || source.m_Pool.Overlaps(destination.m_Pool)))) return false;
		float free = destination.m_Pool.Capacity() - destination.m_Pool.Value() - Reserved(destination.m_Pool, true);
		if (amount > free + AICF_LogisticsConfig.RESOURCE_EPSILON) return false;
		if (source && amount > source.m_Pool.Value() - Reserved(source.m_Pool, false)) return false;
		AICF_LogisticsJob job = new AICF_LogisticsJob();
		job.m_sToken = string.Format("L%1_S%2_G%3", ++m_iNextToken, w.m_iSlot, w.m_iGeneration);
		job.m_iGeneration = w.m_iGeneration;
		job.m_iGraphRevision = revision;
		job.m_iExpiresAtMs = System.GetTickCount() + config.m_iReservationTtlMs;
		job.m_Source = source;
		job.m_OriginalSource = source;
		if (!source && !w.m_aCargo.IsEmpty()) job.m_OriginalSource = w.m_aCargo[0].m_Source;
		job.m_Destination = destination;
		job.m_vUnload = destination.m_vPosition;
		if (source) job.m_vLoad = source.m_vPosition;
		if (source) job.m_fReservedSource = amount;
		job.m_fReservedIncoming = amount;
		job.m_bLoaded = source == null;
		job.m_bReturn = returning;
		m_aJobs.Insert(job);
		w.m_Job = job;
		string detail = string.Format("source=%1 destination=%2 depth=%3 graph_revision=%4 amount=%5 purpose_return=%6 supply_debited=0", SourceKey(source), destination.Key(), destination.m_iDepth, revision, amount, returning);
		detail += string.Format(" source_kind=%1 reserved_source=%2 reserved_incoming=%3 cargo=%4", SourceKind(source, w.m_Faction), job.m_fReservedSource, job.m_fReservedIncoming, w.m_fObservedCargo);
		w.Log("LOGISTICS_JOB_RESERVED", detail);
		return true;
	}

	static string SourceKey(AICF_LogisticsEndpoint source)
	{
		if (source) return source.Key();
		return "UNKNOWN";
	}

	static string SourceKind(AICF_LogisticsEndpoint source, Faction faction)
	{
		if (!source) return "LOADED_CARGO";
		if (source.m_Base.GetType() != SCR_ECampaignBaseType.SOURCE_BASE) return "OWNED_BASE";
		if (source.m_Owner == faction) return "OWNED_SOURCE";
		return "NEUTRAL_SOURCE";
	}

	bool Renew(AICF_LogisticsWorker w, AICF_LogisticsConfig config, int now)
	{
		AICF_LogisticsJob job = w.m_Job;
		bool nativeWait = w.m_DriverInteraction && w.m_DriverInteraction.CanRenew(w, System.GetTickCount());
		bool routeRecovery = w.m_RouteRecovery && w.m_RouteRecovery.CanRenew(w, now);
		if (m_bStopped || !job || job.m_bCancelled || !m_aJobs.Contains(job) || job.m_iGeneration != w.m_iGeneration ||
			(!w.Ready() && !nativeWait) || (w.m_DriverInteraction && !nativeWait) || now >= job.m_iExpiresAtMs ||
			(w.ProgressAgeMs(now) > AICF_LogisticsConfig.PROGRESS_TIMEOUT_MS && !routeRecovery && !nativeWait)) return false;
		job.m_iExpiresAtMs = now + config.m_iReservationTtlMs;
		return true;
	}

	void Cancel(AICF_LogisticsWorker w)
	{
		if (!w || !w.m_Job) return;
		w.m_Job.m_bCancelled = true;
		w.m_Job.m_fReservedSource = 0;
		w.m_Job.m_fReservedIncoming = 0;
		m_Resources.ForgetJob(w.m_Job.m_sToken);
		m_aJobs.RemoveItem(w.m_Job);
		w.m_Job = null;
	}

	void Stop()
	{
		if (m_bStopped) return;
		m_bStopped = true;
		m_Resources.Stop();
		foreach (AICF_LogisticsJob job : m_aJobs)
		{
			job.m_bCancelled = true;
			job.m_fReservedSource = 0;
			job.m_fReservedIncoming = 0;
		}
		m_aJobs.Clear();
	}

	static void ConsumeBatches(AICF_LogisticsWorker w, float amount)
	{
		while (amount > 0 && !w.m_aCargo.IsEmpty())
		{
			AICF_LogisticsCargoBatch batch = w.m_aCargo[0];
			float consumed = Math.Min(batch.m_fAmount, amount);
			batch.m_fAmount -= consumed;
			amount -= consumed;
			if (batch.m_fAmount <= 0) w.m_aCargo.Remove(0);
		}
	}

	static void Observe(AICF_LogisticsWorker w)
	{
		if (!w || !w.m_bCustody || !Replication.IsServer()) return;
		// Unknown absence never manufactures a loss receipt.
		if (!w.m_Vehicle || w.m_Vehicle.GetID() != w.m_VehicleId || !w.m_CargoPool || !w.m_CargoPool.Valid())
		{
			w.m_bCargoFault = true;
			return;
		}
		float actual = w.m_CargoPool.Value();
		float delta = actual - w.m_fObservedCargo;
		if (delta == 0) return;
		SCR_VehicleDamageManagerComponent damage = SCR_VehicleDamageManagerComponent.Cast(w.m_Vehicle.FindComponent(SCR_VehicleDamageManagerComponent));
		if (delta < 0 && damage && damage.IsDestroyed())
		{
			w.m_fLost -= delta;
			ConsumeBatches(w, -delta);
			w.Log("LOGISTICS_CARGO_LOST", string.Format("amount=%1 before=%2 after=%3 reason=DESTROYED_RESOURCE_DECREASE", -delta, w.m_fObservedCargo, actual));
		}
		else
		{
			if (delta > 0)
			{
				w.m_fExternalIn += delta;
				AICF_LogisticsCargoBatch batch = new AICF_LogisticsCargoBatch();
				batch.m_fAmount = delta;
				w.m_aCargo.Insert(batch);
			}
			else
			{
				w.m_fExternalOut -= delta;
				ConsumeBatches(w, -delta);
			}
			w.Log("LOGISTICS_EXTERNAL_TRANSFER", string.Format("delta=%1 before=%2 after=%3 original_source=UNKNOWN", delta, w.m_fObservedCargo, actual));
		}
		w.m_fObservedCargo = actual;
	}

	static void RecordStockFireLoss(AICF_LogisticsWorker w, float before)
	{
		if (!w || !w.m_bCustody || w.m_bCargoFault || !Replication.IsServer()) return;
		if (!w.m_CargoPool || !w.m_CargoPool.Valid())
		{
			w.m_bCargoFault = true;
			return;
		}
		float after = w.m_CargoPool.Value();
		if (after >= before) return;
		float lost = before - after;
		w.m_fLost += lost;
		ConsumeBatches(w, lost);
		w.m_fObservedCargo = after;
		w.Log("LOGISTICS_CARGO_LOST", string.Format("amount=%1 before=%2 after=%3 reason=STOCK_SUPPLIES_FIRE", lost, before, after));
	}

	static void ReleaseCargo(AICF_LogisticsWorker w)
	{
		if (!w || !w.m_bCustody || w.m_bCargoFault) return;
		Observe(w);
		if (w.m_bCargoFault) return;
		w.m_fReleased += w.m_fObservedCargo;
		w.Log("LOGISTICS_CARGO_RELEASED", string.Format("amount=%1 reason=SURVIVING_WORLD_POOL", w.m_fObservedCargo));
		w.m_bCustody = false;
		if (w.m_Vehicle && w.m_Vehicle.GetID() == w.m_VehicleId)
		{
			SCR_VehicleDamageManagerComponent damage = SCR_VehicleDamageManagerComponent.Cast(w.m_Vehicle.FindComponent(SCR_VehicleDamageManagerComponent));
			if (damage) damage.AICF_SetLogisticsCustody(null);
		}
		w.m_aCargo.Clear();
	}

	bool Commit(AICF_LogisticsWorker w, bool loading, float allowed, float deficit)
	{
		AICF_LogisticsJob job = w.m_Job;
		if (m_bStopped || !job || job.m_bCancelled || w.m_DriverInteraction || !w.Ready() || job.m_iGeneration != w.m_iGeneration ||
			System.GetTickCount() >= job.m_iExpiresAtMs || !w.m_CargoPool || !w.m_CargoPool.Valid() || !(allowed > 0)) return false;
		Observe(w);
		if (w.m_bCargoFault) return false;
		if (!m_aJobs.Contains(job)) return false;
		SCR_ResourceComponent endpointResource = job.m_Destination.m_Resource;
		AICF_LogisticsResourcePool endpointPool = job.m_Destination.m_Pool;
		if (loading && job.m_Source)
		{
			endpointResource = job.m_Source.m_Resource;
			endpointPool = job.m_Source.m_Pool;
		}
		if (!AICF_LogisticsResourceAdapter.SupportsTransfer(endpointResource, SCR_ResourceComponent.FindResourceComponent(w.m_Vehicle), loading, endpointPool, w.m_CargoPool)) return false;
		AICF_LogisticsResourcePool from = w.m_CargoPool;
		AICF_LogisticsResourcePool to = job.m_Destination.m_Pool;
		string operation = job.m_sToken + ":UNLOAD";
		string purpose = "DELIVERY";
		if (job.m_bReturn) purpose = "RETURN";
		if (loading)
		{
			if (job.m_bLoaded || !job.m_Source) return false;
			from = job.m_Source.m_Pool;
			to = w.m_CargoPool;
			operation = job.m_sToken + ":LOAD";
			purpose = "LOAD";
		}
		AICF_LogisticsReceipt receipt = m_Resources.Transfer(operation, purpose, from, to, allowed);
		if (!receipt) return false;
		if (receipt.m_bAccounted) return false;
		receipt.m_bAccounted = true;
		float amount = receipt.m_fAmount;
		if (loading)
		{
			w.m_fLoaded += Math.Max(0, receipt.m_fBeforeFrom - receipt.m_fAfterFrom);
			if (amount > 0)
			{
				AICF_LogisticsCargoBatch batch = new AICF_LogisticsCargoBatch();
				batch.m_Source = job.m_Source;
				batch.m_fAmount = amount;
				w.m_aCargo.Insert(batch);
			}
			job.m_bLoaded = true;
			job.m_fReservedSource = 0;
			job.m_fReservedIncoming = amount;
		}
		else
		{
			if (job.m_bReturn) w.m_fReturned += amount;
			else w.m_fDelivered += amount;
			ConsumeBatches(w, Math.Max(0, receipt.m_fBeforeFrom - receipt.m_fAfterFrom));
			job.m_fReservedIncoming = Math.Max(0, job.m_fReservedIncoming - amount);
			if (amount > 0) w.m_iReturnAttempts = 0;
		}
		if (w.m_CargoPool.Valid()) w.m_fObservedCargo = w.m_CargoPool.Value();
		w.m_fDiscrepancy += receipt.m_fDiscrepancy;
		string detail = string.Format("operation=%1 purpose=%2 amount=%3 from_before=%4 from_after=%5 to_before=%6 to_after=%7 cargo=%8 discrepancy=%9", operation, purpose, amount,
			receipt.m_fBeforeFrom, receipt.m_fAfterFrom, receipt.m_fBeforeTo, receipt.m_fAfterTo, w.m_fObservedCargo, receipt.m_fDiscrepancy);
		detail += string.Format(" source=%1 destination=%2 depth=%3 graph_revision=%4 from_pool=%5 to_pool=%6", SourceKey(job.m_Source), job.m_Destination.Key(), job.m_Destination.m_iDepth, job.m_iGraphRevision, from.m_sKey, to.m_sKey);
		detail += string.Format(" unknown_state=%1 pending_unverified=%2", receipt.m_bUnknownState, receipt.m_fPending);
		detail += string.Format(" source_kind=%1 reserved_source=%2 reserved_incoming=%3 deficit=%4", SourceKind(job.m_Source, w.m_Faction), job.m_fReservedSource, job.m_fReservedIncoming, deficit);
		if (job.m_bReturn)
			detail += string.Format(" original_source=%1 return_destination=%2 same_pool=%3", SourceKey(job.m_OriginalSource), to.m_sKey, job.m_OriginalSource && job.m_OriginalSource.m_Pool == to);
		if (!receipt.m_bCommitted)
		{
			w.m_bCargoFault = true;
			w.Log("LOGISTICS_TRANSFER_FAILED", detail + " reason=PAIR_POSTCONDITION");
			return false;
		}
		if (loading) w.Log("LOGISTICS_LOAD_COMMITTED", detail);
		else w.Log("LOGISTICS_UNLOAD_COMMITTED", detail);
		return amount > 0;
	}

	static void LogBalance(AICF_LogisticsWorker w)
	{
		string detail = string.Format("loaded=%1 delivered=%2 returned=%3 in_transit=%4 lost=%5 released=%6 external_in=%7 external_out=%8 balance_delta=%9",
			w.m_fLoaded, w.m_fDelivered, w.m_fReturned, CustodyCargo(w), w.m_fLost, w.m_fReleased, w.m_fExternalIn, w.m_fExternalOut, w.BalanceDelta());
		detail += string.Format(" discrepancy=%1 retained_lease=%2 custody=%3 fault=%4", w.m_fDiscrepancy, w.m_Lease != null, w.m_bCustody, w.m_bCargoFault);
		w.Log("LOGISTICS_BALANCE", detail);
	}

	static float CustodyCargo(AICF_LogisticsWorker w)
	{
		if (w.m_bCustody) return w.m_fObservedCargo;
		return 0;
	}
}

class AICF_LogisticsEndpointSafety
{
	protected SCR_CampaignFaction m_Faction;
	protected bool m_bDanger;
	bool Safe(AICF_LogisticsEndpoint endpoint, SCR_CampaignFaction faction)
	{
		if (!endpoint || !endpoint.IdentityValid()) return false;
		m_Faction = faction;
		m_bDanger = false;
		GetGame().GetWorld().QueryEntitiesBySphere(endpoint.m_Base.GetOwner().GetOrigin(), Math.Min(100, endpoint.m_Base.GetRadius()), CheckCharacter, null, EQueryEntitiesFlags.DYNAMIC);
		return !m_bDanger;
	}
	protected bool CheckCharacter(IEntity entity)
	{
		if (!AICF_GroupRuntime.IsAliveCharacter(entity)) return true;
		Faction faction = SCR_Faction.GetEntityFaction(entity);
		if (!faction || m_Faction.IsFactionEnemy(faction)) m_bDanger = true;
		return !m_bDanger;
	}
}

// Scheduler службы; vehicle mechanics выполняет существующий vehicle domain.
class AICF_LogisticsService
{
	protected SCR_GameModeCampaign m_Campaign;
	protected AICF_VehicleCoordinator m_Vehicles;
	protected AICF_ObjectiveGraph m_Graph;
	protected AICF_LogisticsConfig m_Config;
	protected AICF_LogisticsLedger m_Book;
	protected ref AICF_LogisticsDepotRegistry m_Registry = new AICF_LogisticsDepotRegistry();
	protected ref AICF_LogisticsPlanner m_Planner;
	protected ref AICF_LogisticsEndpointSafety m_Safety = new AICF_LogisticsEndpointSafety();
	protected int m_iNextPlannerMs;
	protected int m_iNextHeartbeatMs;
	protected int m_iGraphRevision;
	protected int m_iAdmissionCursor;
	protected bool m_bStopped;

	void AICF_LogisticsService(SCR_GameModeCampaign campaign, AICF_VehicleCoordinator vehicles, AICF_ObjectiveGraph graph, AICF_LogisticsConfig config, AICF_EconomySystem economy)
	{
		m_Campaign = campaign;
		m_Vehicles = vehicles;
		m_Graph = graph;
		m_Config = config;
		m_Book = economy.LogisticsLedger();
		m_Registry.Start(campaign);
		m_Planner = new AICF_LogisticsPlanner(graph, config, economy, m_Book);
		economy.LogisticsWorkers(m_Registry.m_aWorkers);
	}

	void Update(SCR_CampaignFaction us, SCR_CampaignFaction ussr, bool graphReady)
	{
		if (m_bStopped || !Replication.IsServer() || !m_Campaign || !m_Campaign.IsMaster() || !m_Campaign.IsRunning()) return;
		int now = System.GetTickCount();
		m_Planner.BeginTick();
		if (graphReady)
		{
			m_Registry.Update(m_Config, now);
			if (now >= m_iNextPlannerMs || m_iGraphRevision != m_Graph.GetRevision())
			{
				m_Planner.Reconcile(us, ussr);
				m_iNextPlannerMs = now + m_Config.m_iPlannerIntervalMs;
				m_iGraphRevision = m_Graph.GetRevision();
			}
		}
		foreach (AICF_LogisticsWorker worker : m_Registry.m_aWorkers)
		{
			if (now < worker.m_iNextPollMs) continue;
			worker.m_iNextPollMs = now + m_Config.m_iWorkerPollMs;
			AICF_LogisticsLedger.Observe(worker);
			m_Vehicles.TickLogisticsWorker(worker, m_Config, m_Book, graphReady);
			if (worker.m_Lease && !worker.m_bCleanupQueued) UpdateJob(worker, System.GetTickCount(), graphReady);
		}
		// Один admission/search на tick, постоянная ротация всех depot slots.
		if (graphReady && !m_Registry.m_aWorkers.IsEmpty())
		{
			m_iAdmissionCursor = m_iAdmissionCursor % m_Registry.m_aWorkers.Count();
			AICF_LogisticsWorker candidate = m_Registry.m_aWorkers[m_iAdmissionCursor++];
			SelectWork(candidate, now);
		}
		if (now >= m_iNextHeartbeatMs)
		{
			m_iNextHeartbeatMs = now + 60000;
			foreach (AICF_LogisticsWorker report : m_Registry.m_aWorkers) AICF_LogisticsLedger.LogBalance(report);
		}
	}

	protected void SelectWork(AICF_LogisticsWorker w, int now)
	{
		if (w.m_bCargoFault || w.m_DriverInteraction || now < w.m_iRetryAtMs || w.m_Job) return;
		if (w.m_bCleanupComplete)
		{
			if (now - w.m_iRetryAtMs < m_Config.m_iReplacementCooldownMs) return;
			w.m_bCleanupQueued = false;
		}
		if (w.m_bCleanupQueued) return;
		bool homeLive = w.m_bEligible && AICF_LogisticsDepotRegistry.Live(w);
		if (!w.m_Lease)
		{
			if (!homeLive || w.m_Group || w.m_Vehicle) return;
			if (w.m_ExitHistory && w.m_ExitHistory.AllCooling(w, now))
			{
				DeferWorker(w, now, "ALL_EXACT_DEPOT_EXITS_COOLING");
				return;
			}
			if (m_Planner.Select(w, true))
			{
				if (!m_Vehicles.BeginLogisticsSpawn(w)) DeferWorker(w, now, "SHARED_FLEET_OR_AI_ADMISSION");
			}
			else if (!w.m_Search) DeferWorker(w, now, w.m_sPlanningReason);
			return;
		}
		if (!w.Ready()) return;
		if (w.m_ePhase != AICF_ELogisticsPhase.DRIVER_READY && w.m_ePhase != AICF_ELogisticsPhase.IDLE_AT_DEPOT &&
			w.m_ePhase != AICF_ELogisticsPhase.RETURN_HOME && w.m_ePhase != AICF_ELogisticsPhase.WAIT_RETRY) return;
		if (w.m_fObservedCargo > 0)
		{
			bool selected;
			if (!w.m_Search || !w.m_Search.m_bReturn) selected = m_Planner.Select(w);
			if (!selected && w.m_Search && !w.m_Search.m_bReturn) return;
			if (!selected) selected = m_Planner.SelectReturn(w);
			if (selected)
			{
				BeginJobLeg(w);
				return;
			}
			if (w.m_Search) return;
			w.m_iReturnAttempts++;
			if (w.m_iReturnAttempts >= m_Config.m_iReturnSearchMaxAttempts)
				m_Vehicles.RetireLogistics(w, m_Book, "NO_SAFE_RETURN_STORAGE_BUDGET_EXHAUSTED");
			else DeferWorker(w, now, "NO_SAFE_RETURN_STORAGE");
			return;
		}
		if (!homeLive || w.m_bRetireAfterCargo)
		{
			m_Vehicles.RetireLogistics(w, m_Book, "DEPOT_UNAVAILABLE_EMPTY");
			return;
		}
		if (m_Planner.Select(w))
		{
			BeginJobLeg(w);
			return;
		}
		if (w.m_Search) return;
		if (w.m_ePhase == AICF_ELogisticsPhase.RETURN_HOME) return;
		if (w.m_ePhase == AICF_ELogisticsPhase.IDLE_AT_DEPOT && w.m_iIdleAtMs > 0 &&
			vector.DistanceXZ(w.m_Vehicle.GetOrigin(), w.m_vParking) <= m_Config.m_fArrivalRadiusM)
		{
			Physics parked = w.m_Vehicle.GetPhysics();
			if (!parked || parked.GetVelocity().Length() > m_Config.m_fStationarySpeedMps)
			{
				w.m_iIdleAtMs = now;
				return;
			}
			if (now - w.m_iIdleAtMs >= m_Config.m_iIdleRetireMs) m_Vehicles.RetireLogistics(w, m_Book, "IDLE_EMPTY_HOME_PARKING");
			return;
		}
		w.Log("LOGISTICS_RETURNING", "reason=EMPTY_RETURN_HOME");
		if (!m_Vehicles.BeginLogisticsLeg(w, w.m_vParking, AICF_ELogisticsPhase.RETURN_HOME)) m_Vehicles.RetireLogistics(w, m_Book, "HOME_LEG_REJECTED");
	}

	protected void DeferWorker(AICF_LogisticsWorker w, int now, string reason)
	{
		w.m_iRetryAtMs = now + m_Config.m_iBlockedRetryMs;
		if (w.m_Lease) m_Vehicles.CancelLogisticsLeg(w, reason);
		if (w.m_sLastReason != reason || now - w.m_iLastLogMs >= m_Config.m_iBlockedRetryMs)
		{
			w.m_sLastReason = reason;
			w.m_iLastLogMs = now;
			w.Log("LOGISTICS_WAITING", "reason=" + reason);
		}
	}

	protected void BeginJobLeg(AICF_LogisticsWorker w)
	{
		AICF_LogisticsJob job = w.m_Job;
		if (!job) return;
		vector position = job.m_vLoad;
		AICF_ELogisticsPhase phase = AICF_ELogisticsPhase.TO_SOURCE;
		if (job.m_bLoaded)
		{
			position = job.m_vUnload;
			phase = AICF_ELogisticsPhase.TO_DESTINATION;
		}
		if (!m_Vehicles.BeginLogisticsLeg(w, position, phase))
		{
			m_Book.Cancel(w);
			DeferWorker(w, System.GetTickCount(), "VEHICLE_LEG_REJECTED");
		}
	}

	protected bool ValidJob(AICF_LogisticsWorker w)
	{
		AICF_LogisticsJob job = w.m_Job;
		if (!job || job.m_bCancelled || !job.m_Destination.IdentityValid()) return false;
		AICF_LogisticsEndpoint destination = m_Planner.Find(job.m_Destination.m_Base);
		if (!destination || destination.m_Pool != job.m_Destination.m_Pool) return false;
		if (!job.m_bReturn && (!AICF_LogisticsPlanner.OwnedSafe(destination, w.m_Faction) || destination.m_iDepth < 0)) return false;
		if (job.m_bReturn && !AICF_LogisticsPlanner.OwnedSafe(destination, w.m_Faction) &&
			!(job.m_OriginalSource && job.m_OriginalSource.m_Pool == destination.m_Pool && AICF_LogisticsPlanner.NeutralSource(destination, w.m_Faction))) return false;
		if (!job.m_bLoaded && (!job.m_Source || !job.m_Source.IdentityValid() ||
			(!AICF_LogisticsPlanner.OwnedSafe(job.m_Source, w.m_Faction) && !AICF_LogisticsPlanner.NeutralSource(job.m_Source, w.m_Faction)))) return false;
		if (m_Book.m_Resources.Resolve(destination.m_Resource) != destination.m_Pool) return false;
		if (!job.m_bLoaded && m_Book.m_Resources.Resolve(job.m_Source.m_Resource) != job.m_Source.m_Pool) return false;
		if (job.m_iGraphRevision != m_Graph.GetRevision())
		{
			vector verified;
			vector verifiedDestination;
			if (job.m_bLoaded)
			{
				if (!m_Planner.Route(w.m_Vehicle.GetOrigin(), destination, verified)) return false;
			}
			else if (!m_Planner.Route(w.m_Vehicle.GetOrigin(), job.m_Source, verified) || !m_Planner.Route(verified, destination, verifiedDestination)) return false;
		}
		job.m_iGraphRevision = m_Graph.GetRevision();
		return true;
	}

	protected void UpdateJob(AICF_LogisticsWorker w, int now, bool graphReady)
	{
		if (w.m_bCargoFault)
		{
			m_Vehicles.RetireLogistics(w, m_Book, "RESOURCE_DISCREPANCY_OR_IDENTITY");
			return;
		}
		if (!AICF_LogisticsDepotRegistry.Live(w)) w.m_bRetireAfterCargo = true;
		if (!w.m_Job) return;
		if (!graphReady && w.m_DriverInteraction)
		{
			m_Vehicles.RetireLogistics(w, m_Book, "DRIVER_INTERACTION_GRAPH_PENDING");
			return;
		}
		if (!graphReady) return;
		if (!ValidJob(w) || (w.m_bRetireAfterCargo && !w.m_Job.m_bLoaded) || !m_Book.Renew(w, m_Config, now))
		{
			if (w.m_DriverInteraction)
			{
				m_Vehicles.RetireLogistics(w, m_Book, "DRIVER_INTERACTION_JOB_CONTEXT_OR_RESERVATION_CHANGED");
				return;
			}
			m_Book.Cancel(w);
			m_Vehicles.CancelLogisticsLeg(w, "JOB_CONTEXT_OR_RESERVATION_CHANGED");
			return;
		}
		if (w.m_DriverInteraction) return;
		bool loading = w.m_ePhase == AICF_ELogisticsPhase.LOADING;
		if (!loading && w.m_ePhase != AICF_ELogisticsPhase.UNLOADING) return;
		AICF_LogisticsJob job = w.m_Job;
		AICF_LogisticsEndpoint endpoint = job.m_Destination;
		if (loading) endpoint = job.m_Source;
		Physics physics = w.m_Vehicle.GetPhysics();
		if (!w.Ready() || !physics || vector.DistanceXZ(w.m_Vehicle.GetOrigin(), w.m_vEndpoint) > m_Config.m_fArrivalRadiusM ||
			vector.DistanceXZ(w.m_Vehicle.GetOrigin(), endpoint.m_Base.GetOwner().GetOrigin()) > endpoint.m_Base.GetRadius() ||
			physics.GetVelocity().Length() > m_Config.m_fStationarySpeedMps)
		{
			w.m_iStationaryAtMs = 0;
			return;
		}
		if (!w.m_iStationaryAtMs) w.m_iStationaryAtMs = now;
		if (now - w.m_iStationaryAtMs < m_Config.m_iStationaryHoldMs) return;
		if (!m_Vehicles.LogisticsTransferSafe(w) || !m_Safety.Safe(endpoint, w.m_Faction) || m_Book.m_Resources.Resolve(endpoint.m_Base.GetResourceComponent()) != endpoint.m_Pool) return;
		float q = Math.Min(w.m_fObservedCargo, m_Planner.Need(endpoint, job, job.m_bReturn));
		q = Math.Min(q, job.m_fReservedIncoming);
		if (loading)
		{
			q = Math.Min(job.m_fReservedSource, m_Planner.Available(endpoint, w.m_Faction, job));
			q = Math.Min(q, Math.Max(0, w.m_CargoPool.Capacity() - w.m_CargoPool.Value()));
			q = Math.Min(q, m_Planner.Need(job.m_Destination, job));
		}
		bool committed = m_Book.Commit(w, loading, q, m_Planner.Need(job.m_Destination, job, job.m_bReturn));
		if (w.m_bCargoFault)
		{
			m_Vehicles.RetireLogistics(w, m_Book, "TRANSFER_FAILED_CLOSED");
			return;
		}
		if (q > 0 && !committed)
		{
			m_Book.Cancel(w);
			w.m_iTransferFailures++;
			if (w.m_iTransferFailures >= 3) m_Vehicles.RetireLogistics(w, m_Book, "RESOURCE_OPERATION_RETRY_EXHAUSTED");
			else DeferWorker(w, now, "RESOURCE_OPERATION_UNSUPPORTED_OR_REJECTED");
			return;
		}
		if (committed) w.m_iTransferFailures = 0;
		if (loading && committed)
		{
			BeginJobLeg(w);
			return;
		}
		m_Book.Cancel(w);
		m_Vehicles.CancelLogisticsLeg(w, "TRANSFER_FINISHED_OR_NO_CAPACITY");
	}

	void Stop()
	{
		if (m_bStopped) return;
		m_bStopped = true;
		AICF_Stage4Diagnostics.Info("LOGISTICS_STOP", string.Format("schema_version=2 workers=%1 transfers_closed=1", m_Registry.m_aWorkers.Count()));
		m_Registry.Stop();
		foreach (AICF_LogisticsWorker w : m_Registry.m_aWorkers)
		{
			w.m_bStopped = true;
			if (w.m_Lease) m_Vehicles.RetireLogistics(w, m_Book, "LOGISTICS_STOP");
			else m_Book.Cancel(w);
		}
		m_Book.Stop();
	}
}

class AICF_LogisticsExitFailure
{
	SCR_EntitySpawnerSlotComponent m_Slot;
	EntityID m_SlotId;
	vector m_vPosition;
	vector m_vForward;
	int m_iUntilMs;
	int m_iFailures;
	int m_iLastGeneration;
	int m_iLastWorkerSlot;
}

// Один экземпляр на exact depot/faction; ordinals делят доказанные отказы.
// Registry lifetime ограничивает память и очищает её при Stop.
class AICF_LogisticsExitHistory
{
	ref array<ref AICF_LogisticsExitFailure> m_aFailures = {};

	bool Cooling(SCR_EntitySpawnerSlotComponent slot, int now)
	{
		for (int i = m_aFailures.Count() - 1; i >= 0; i--)
		{
			AICF_LogisticsExitFailure failure = m_aFailures[i];
			if (!failure.m_Slot || !failure.m_Slot.GetOwner() || failure.m_Slot.GetOwner().GetID() != failure.m_SlotId || now >= failure.m_iUntilMs)
			{
				m_aFailures.Remove(i);
				continue;
			}
			if (failure.m_Slot != slot) continue;
			vector transform[4];
			slot.GetOwner().GetWorldTransform(transform);
			if (transform[3] == failure.m_vPosition && transform[2] == failure.m_vForward) return failure.m_iFailures >= 2;
			m_aFailures.Remove(i);
		}
		return false;
	}

	bool AllCooling(AICF_LogisticsWorker w, int now)
	{
		array<SCR_EntitySpawnerSlotComponent> slots = {};
		w.m_Production.AICF_LogisticsCandidateSlots(w.m_Entry, slots, false);
		if (slots.IsEmpty()) return false;
		foreach (SCR_EntitySpawnerSlotComponent slot : slots)
		{
			if (!Cooling(slot, now)) return false;
		}
		return true;
	}

	void Record(AICF_LogisticsWorker w, string reason, int now)
	{
		bool routeExhausted = reason == "BOUNDED_ROUTE_RECOVERY_EXHAUSTED" && w.m_iRouteRetries >= AICF_LogisticsConfig.MAX_ROUTE_RETRIES;
		AICF_LogisticsDriverRecovery driver = AICF_LogisticsDriverRecovery.Cast(w.m_DriverInteraction);
		bool failedExit = reason == "UNKNOWN_DRIVER_RECOVERY_DEADLINE" && driver && driver.Matches(w) && driver.m_bNoVehicleMotion;
		if ((!routeExhausted && !failedExit) || (routeExhausted && !w.Ready()) ||
			w.m_bExitedSpawn || !AICF_LogisticsDepotRegistry.Live(w) || !w.VehicleIdentity() || !w.DriverIdentity() || w.HasForeignOccupant(true) ||
			!w.m_SpawnSlot || !w.m_SpawnSlot.GetOwner() || w.m_SpawnSlot.GetOwner().GetID() != w.m_SpawnSlotId ||
			vector.DistanceXZ(w.m_Vehicle.GetOrigin(), w.m_aSpawnTransform[3]) > 90 || Cooling(w.m_SpawnSlot, now)) return;
		AICF_LogisticsExitFailure failure;
		foreach (AICF_LogisticsExitFailure old : m_aFailures)
		{
			if (old.m_Slot == w.m_SpawnSlot && old.m_SlotId == w.m_SpawnSlotId) failure = old;
		}
		if (!failure)
		{
			failure = new AICF_LogisticsExitFailure();
			m_aFailures.Insert(failure);
		}
		if (failure.m_iLastGeneration == w.m_iGeneration && failure.m_iLastWorkerSlot == w.m_iSlot) return;
		failure.m_iLastGeneration = w.m_iGeneration;
		failure.m_iLastWorkerSlot = w.m_iSlot;
		failure.m_iFailures++;
		if (routeExhausted) failure.m_iFailures = 2;
		failure.m_Slot = w.m_SpawnSlot;
		failure.m_SlotId = w.m_SpawnSlotId;
		failure.m_vPosition = w.m_aSlotTransform[3];
		failure.m_vForward = w.m_aSlotTransform[2];
		failure.m_iUntilMs = now + AICF_LogisticsConfig.EXIT_SLOT_COOLDOWN_MS;
		w.Log("LOGISTICS_EXIT_FAILURE", string.Format("reason=%1 spawn_slot=%2 failures=%3 until_ms=%4", reason, w.m_SpawnSlotId, failure.m_iFailures, failure.m_iUntilMs));
		if (failure.m_iFailures >= 2) w.Log("LOGISTICS_EXIT_COOLDOWN", string.Format("reason=%1 spawn_slot=%2 attempts=%3 until_ms=%4", reason, w.m_SpawnSlotId, failure.m_iFailures, failure.m_iUntilMs));
	}
}

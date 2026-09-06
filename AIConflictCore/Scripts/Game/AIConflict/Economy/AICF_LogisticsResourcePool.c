// Canonical pool identity = exact набор физических leaf containers.
// Provider/base aliases не создают дополнительных supplies или reservations.
class AICF_LogisticsLeaf
{
	SCR_ResourceContainer m_Container;
	EntityID m_OwnerId = EntityID.INVALID;
	bool Valid()
	{
		if (!m_Container || !m_Container.GetComponent() || !m_Container.GetOwner() || m_Container.GetOwner().GetID() != m_OwnerId ||
			m_Container.GetOnEmptyBehavior() == EResourceContainerOnEmptyBehavior.DELETE) return false;
		float value = m_Container.GetResourceValue();
		float capacity = m_Container.GetMaxResourceValue();
		return value >= 0 && value <= capacity && capacity >= 0 && capacity <= 100000000;
	}
}

class AICF_LogisticsResourcePool
{
	string m_sKey;
	ref array<ref AICF_LogisticsLeaf> m_aLeaves = {};

	bool Add(SCR_ResourceContainer container, int depth = 0)
	{
		if (!container || !container.GetOwner() || depth > 4 || m_aLeaves.Count() >= 128 ||
			container.GetResourceType() != EResourceType.SUPPLIES) return false;
		SCR_ResourceEncapsulator encapsulator = container.GetResourceEncapsulator();
		if (encapsulator)
		{
			SCR_ResourceContainerQueueBase queue = encapsulator.GetContainerQueue();
			if (!queue || encapsulator.GetContainerCount() > 128) return false;
			for (int i; i < encapsulator.GetContainerCount(); i++)
			{
				if (!Add(queue.GetContainerAt(i), depth + 1)) return false;
			}
			return true;
		}
		if (container.GetOnEmptyBehavior() == EResourceContainerOnEmptyBehavior.DELETE) return false;
		foreach (AICF_LogisticsLeaf old : m_aLeaves)
		{
			if (old.m_Container == container) return true;
		}
		AICF_LogisticsLeaf leaf = new AICF_LogisticsLeaf();
		leaf.m_Container = container;
		leaf.m_OwnerId = container.GetOwner().GetID();
		m_aLeaves.Insert(leaf);
		return true;
	}

	bool Valid()
	{
		if (m_aLeaves.IsEmpty()) return false;
		foreach (AICF_LogisticsLeaf leaf : m_aLeaves)
		{
			if (!leaf.Valid()) return false;
		}
		return true;
	}

	bool Overlaps(AICF_LogisticsResourcePool other)
	{
		if (!other) return false;
		foreach (AICF_LogisticsLeaf leaf : m_aLeaves)
		{
			foreach (AICF_LogisticsLeaf candidate : other.m_aLeaves)
			{
				if (leaf.m_Container == candidate.m_Container) return true;
			}
		}
		return false;
	}

	bool Same(AICF_LogisticsResourcePool other)
	{
		if (!other || m_aLeaves.Count() != other.m_aLeaves.Count()) return false;
		foreach (AICF_LogisticsLeaf leaf : m_aLeaves)
		{
			bool found;
			foreach (AICF_LogisticsLeaf candidate : other.m_aLeaves)
			{
				if (leaf.m_Container == candidate.m_Container && leaf.m_OwnerId == candidate.m_OwnerId) found = true;
			}
			if (!found) return false;
		}
		return true;
	}

	float Value()
	{
		float value;
		foreach (AICF_LogisticsLeaf leaf : m_aLeaves)
		{
			if (leaf.Valid()) value += leaf.m_Container.GetResourceValue();
		}
		return value;
	}

	float Capacity()
	{
		float value;
		foreach (AICF_LogisticsLeaf leaf : m_aLeaves)
		{
			if (leaf.Valid()) value += leaf.m_Container.GetMaxResourceValue();
		}
		return value;
	}
}

class AICF_LogisticsReceipt
{
	string m_sOperation;
	string m_sPurpose;
	string m_sFrom;
	string m_sTo;
	float m_fBeforeFrom;
	float m_fAfterFrom;
	float m_fBeforeTo;
	float m_fAfterTo;
	float m_fAmount;
	float m_fDiscrepancy;
	bool m_bCommitted;
	bool m_bAccounted;
	bool m_bUnknownState;
	float m_fPending;
}

// Узкий adapter экономики: без waypoint, callback и выбора jobs.
class AICF_LogisticsResourceAdapter
{
	protected ref array<ref AICF_LogisticsResourcePool> m_aPools = {};
	protected ref array<ref AICF_LogisticsReceipt> m_aReceipts = {};
	protected bool m_bStopped;
	protected int m_iNextPool;

	void Stop() { m_bStopped = true; }

	void ForgetJob(string token)
	{
		for (int i = m_aReceipts.Count() - 1; i >= 0; i--)
		{
			if (m_aReceipts[i].m_sOperation == token + ":LOAD" || m_aReceipts[i].m_sOperation == token + ":UNLOAD") m_aReceipts.Remove(i);
		}
	}

	// Права на stock transfer проверяются через те же operation endpoints,
	// что SCR_SuppliesTransferWaypoint. Мутируем только exact canonical leaves.
	static bool OperationMatchesPool(SCR_ResourceInteractor operation, AICF_LogisticsResourcePool expected)
	{
		if (!operation || !expected || !expected.Valid()) return false;
		GetGame().GetResourceGrid().UpdateInteractor(operation);
		SCR_ResourceContainerQueueBase queue = operation.GetContainerQueue();
		if (!queue || operation.GetContainerCount() > 128) return false;
		AICF_LogisticsResourcePool actual = new AICF_LogisticsResourcePool();
		for (int i; i < operation.GetContainerCount(); i++)
		{
			SCR_ResourceContainer container = queue.GetContainerAt(i);
			if (!container || !operation.CanInteractWith(container) || !actual.Add(container)) return false;
		}
		return actual.Valid() && expected.Same(actual);
	}

	static bool SupportsTransfer(SCR_ResourceComponent baseResource, SCR_ResourceComponent cargoResource, bool loading,
		AICF_LogisticsResourcePool basePool, AICF_LogisticsResourcePool cargoPool)
	{
		if (!baseResource || !cargoResource) return false;
		if (!baseResource.IsResourceTypeEnabled() || !cargoResource.IsResourceTypeEnabled()) return false;
		SCR_ResourceConsumer consumer;
		SCR_ResourceGenerator generator;
		if (loading)
		{
			consumer = baseResource.GetConsumer(EResourceGeneratorID.DEFAULT, EResourceType.SUPPLIES);
			generator = cargoResource.GetGenerator(EResourceGeneratorID.VEHICLE_LOAD, EResourceType.SUPPLIES);
		}
		else
		{
			consumer = cargoResource.GetConsumer(EResourceGeneratorID.VEHICLE_UNLOAD, EResourceType.SUPPLIES);
			generator = baseResource.GetGenerator(EResourceGeneratorID.DEFAULT, EResourceType.SUPPLIES);
		}
		// BuyMultiplier — цена обмена, не коэффициент physical consumption.
		// У stock VEHICLE_UNLOAD он равен 0; RequestConsumtion списывает
		// переданный объём без умножения. Учитываем реальное consuming state.
		if (!consumer || !generator || !consumer.IsConsuming() || generator.GetResourceMultiplier() != 1) return false;
		if (loading) return OperationMatchesPool(consumer, basePool) && OperationMatchesPool(generator, cargoPool);
		return OperationMatchesPool(consumer, cargoPool) && OperationMatchesPool(generator, basePool);
	}

	AICF_LogisticsResourcePool Resolve(SCR_ResourceComponent resource)
	{
		if (!resource || !resource.GetOwner()) return null;
		for (int i = m_aPools.Count() - 1; i >= 0; i--)
		{
			if (!m_aPools[i].Valid()) m_aPools.Remove(i);
		}
		AICF_LogisticsResourcePool pool = new AICF_LogisticsResourcePool();
		SCR_ResourceContainer container = resource.GetContainer(EResourceType.SUPPLIES);
		if (container)
		{
			if (!pool.Add(container)) return null;
		}
		else
		{
			SCR_ResourceConsumer consumer = resource.GetConsumer(EResourceGeneratorID.DEFAULT, EResourceType.SUPPLIES);
			if (!consumer) return null;
			GetGame().GetResourceGrid().UpdateInteractor(consumer);
			SCR_ResourceContainerQueueBase queue = consumer.GetContainerQueue();
			if (!queue || consumer.GetContainerCount() > 128) return null;
			for (int c; c < consumer.GetContainerCount(); c++)
			{
				if (!pool.Add(queue.GetContainerAt(c))) return null;
			}
		}
		if (!pool.Valid()) return null;
		foreach (AICF_LogisticsResourcePool existing : m_aPools)
		{
			if (existing.Same(pool)) return existing;
			// Partial aliases сохраняют набор leaves; ledger резервирует также
			// пересечения, консервативно удерживая весь объём общего job.
		}
		pool.m_sKey = string.Format("POOL_%1_%2", resource.GetOwner().GetID(), ++m_iNextPool);
		// EntityID debug formatting содержит пробелы; pool token должен целиком
		// помещаться в одно key=value поле, включая revision suffix.
		pool.m_sKey.Replace(" ", "");
		m_aPools.Insert(pool);
		return pool;
	}

	static bool VehicleCargo(Vehicle vehicle, out float value, out float capacity)
	{
		value = 0;
		capacity = 0;
		if (!vehicle) return false;
		SCR_ResourceComponent resource = SCR_ResourceComponent.FindResourceComponent(vehicle);
		if (!resource) return false;
		SCR_ResourceContainer container = resource.GetContainer(EResourceType.SUPPLIES);
		if (!container) return false;
		value = container.GetResourceValue();
		capacity = container.GetMaxResourceValue();
		return value >= 0 && capacity > 0 && value <= capacity;
	}

	// Применяется также к released world-pool vehicles и Stop deletion.
	static bool CanDeleteVehicle(Vehicle vehicle)
	{
		if (!vehicle) return false;
		array<IEntity> pending = {vehicle};
		array<IEntity> checked = {};
		while (!pending.IsEmpty())
		{
			IEntity entity = pending[pending.Count() - 1];
			pending.Remove(pending.Count() - 1);
			if (!entity || checked.Contains(entity)) continue;
			if (checked.Count() >= 512) return false;
			checked.Insert(entity);
			SCR_ResourceComponent resource = SCR_ResourceComponent.Cast(entity.FindComponent(SCR_ResourceComponent));
			if (resource)
			{
				SCR_ResourceContainer container = resource.GetContainer(EResourceType.SUPPLIES);
				if (container && container.GetResourceValue() != 0) return false;
			}
			for (IEntity child = entity.GetChildren(); child; child = child.GetSibling())
			{
				if (pending.Count() >= 512) return false;
				pending.Insert(child);
			}
			SlotManagerComponent slots = SlotManagerComponent.Cast(entity.FindComponent(SlotManagerComponent));
			if (!slots) continue;
			array<EntitySlotInfo> infos = {};
			slots.GetSlotInfos(infos);
			foreach (EntitySlotInfo info : infos)
			{
				if (pending.Count() >= 512) return false;
				if (info.GetAttachedEntity()) pending.Insert(info.GetAttachedEntity());
			}
		}
		return true;
	}

	// Исключение только для новой, ещё не опубликованной spawn transaction.
	bool EmptySpawnCargo(Vehicle vehicle)
	{
		if (m_bStopped || !Replication.IsServer() || !vehicle) return false;
		SCR_ResourceComponent resource = SCR_ResourceComponent.FindResourceComponent(vehicle);
		AICF_LogisticsResourcePool pool = Resolve(resource);
		if (!pool || pool.Capacity() <= 0) return false;
		foreach (AICF_LogisticsLeaf leaf : pool.m_aLeaves)
		{
			IEntity parent = leaf.m_Container.GetOwner();
			for (int depth; parent && parent != vehicle && depth < 16; depth++) parent = parent.GetParent();
			if (parent != vehicle) return false;
		}
		foreach (AICF_LogisticsLeaf empty : pool.m_aLeaves)
		{
			if (empty.m_Container.GetResourceValue() == 0) continue;
			empty.m_Container.SetResourceValue(0);
			empty.m_Container.GetComponent().Replicate();
		}
		return pool.Valid() && pool.Value() == 0;
	}

	// Узкий production seam для injection отказа записи в Enforce fixture.
	protected void WriteLeaf(AICF_LogisticsLeaf leaf, float value)
	{
		if (leaf.Valid()) leaf.m_Container.SetResourceValue(value);
	}

	// Бounded leaf шаги, synchronous mutation. Измеряем обе стороны, затем
	// компенсируем только доказанный debit exact source, без создания груза.
	AICF_LogisticsReceipt Transfer(string operation, string purpose, AICF_LogisticsResourcePool from, AICF_LogisticsResourcePool to, float requested)
	{
		foreach (AICF_LogisticsReceipt old : m_aReceipts)
		{
			if (old.m_sOperation == operation) return old;
		}
		if (m_bStopped || !Replication.IsServer() || operation.IsEmpty() || !from || !to || from == to ||
			!from.Valid() || !to.Valid() || from.Overlaps(to) || !(requested > 0 && requested <= 100000000)) return null;
		AICF_LogisticsReceipt receipt = new AICF_LogisticsReceipt();
		receipt.m_sOperation = operation;
		receipt.m_sPurpose = purpose;
		receipt.m_sFrom = from.m_sKey;
		receipt.m_sTo = to.m_sKey;
		receipt.m_fBeforeFrom = from.Value();
		receipt.m_fBeforeTo = to.Value();
		receipt.m_fAfterFrom = receipt.m_fBeforeFrom;
		receipt.m_fAfterTo = receipt.m_fBeforeTo;
		m_aReceipts.Insert(receipt);
		float remaining = Math.Min(requested, Math.Min(from.Value(), Math.Max(0, to.Capacity() - to.Value())));
		foreach (AICF_LogisticsLeaf source : from.m_aLeaves)
		{
			foreach (AICF_LogisticsLeaf destination : to.m_aLeaves)
			{
				if (!(remaining > 0) || !source.Valid() || !destination.Valid()) break;
				float sourceBefore = source.m_Container.GetResourceValue();
				float destinationBefore = destination.m_Container.GetResourceValue();
				float q = Math.Min(remaining, Math.Min(sourceBefore, Math.Max(0, destination.m_Container.GetMaxResourceValue() - destinationBefore)));
				if (!(q > 0)) continue;
				WriteLeaf(source, sourceBefore - q);
				if (!source.Valid())
				{
					receipt.m_bUnknownState = true;
					receipt.m_fPending = q;
					remaining = 0;
					break;
				}
				float debited = sourceBefore - source.m_Container.GetResourceValue();
				if (!(debited > 0)) continue;
				if (destination.Valid()) WriteLeaf(destination, destinationBefore + debited);
				if (!destination.Valid())
				{
					receipt.m_bUnknownState = true;
					receipt.m_fPending = debited;
					remaining = 0;
					break;
				}
				float credited = destination.m_Container.GetResourceValue() - destinationBefore;
				float escrow = debited - credited;
				if (escrow > 0 && source.Valid())
					WriteLeaf(source, source.m_Container.GetResourceValue() + escrow);
				if (source.Valid()) source.m_Container.GetComponent().Replicate();
				if (destination.Valid()) destination.m_Container.GetComponent().Replicate();
				remaining -= Math.Max(0, credited);
				if (!source.Valid())
				{
					receipt.m_bUnknownState = true;
					receipt.m_fPending = Math.Max(0, escrow);
					remaining = 0;
					break;
				}
				float discrepancy = sourceBefore - source.m_Container.GetResourceValue() - credited;
				if (Math.AbsFloat(discrepancy) > AICF_LogisticsConfig.RESOURCE_EPSILON || !from.Valid() || !to.Valid())
				{
					receipt.m_fDiscrepancy += discrepancy;
					remaining = 0;
					break;
				}
			}
			if (!(remaining > 0)) break;
		}
		if (from.Valid()) receipt.m_fAfterFrom = from.Value();
		else receipt.m_bUnknownState = true;
		if (to.Valid()) receipt.m_fAfterTo = to.Value();
		else receipt.m_bUnknownState = true;
		receipt.m_fAmount = Math.Max(0, receipt.m_fAfterTo - receipt.m_fBeforeTo);
		receipt.m_fDiscrepancy = receipt.m_fBeforeFrom - receipt.m_fAfterFrom - receipt.m_fAmount;
		receipt.m_bCommitted = !receipt.m_bUnknownState && from.Valid() && to.Valid() && Math.AbsFloat(receipt.m_fDiscrepancy) <= AICF_LogisticsConfig.RESOURCE_EPSILON;
		return receipt;
	}
}

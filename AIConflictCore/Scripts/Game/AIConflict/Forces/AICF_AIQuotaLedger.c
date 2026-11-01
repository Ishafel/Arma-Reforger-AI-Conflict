// Только unit ledger. Native identities/current owner/census полнота проверяются
// authority adapter перед вызовами; этот класс не создаёт/удаляет AI.
class AICF_AIQuotaReservation
{
	AICF_AIQuotaLedger m_Ledger;
	int m_iEpoch;
	int m_iOwnerSerial;
	AICF_EAIQuotaBucket m_Bucket;
	int m_iAvailable;
	bool m_bCancelled;
}

class AICF_AIQuotaDispatch
{
	AICF_AIQuotaLedger m_Ledger;
	ref AICF_AIQuotaReservation m_Reservation;
	int m_iEpoch;
	int m_iOwnerSerial;
	bool m_bClosed;
}

class AICF_AIQuotaLedger
{
	protected AICF_AIQuotaLimits m_Limits;
	protected int m_iEpoch;
	protected bool m_bCensusKnown;
	protected int m_iOutstanding;
	protected ref array<int> m_aPhysical = {};
	protected ref array<int> m_aReserved = {};
	protected ref array<string> m_aBoundKeys = {};
	protected ref array<int> m_aBoundBuckets = {};

	void AICF_AIQuotaLedger(AICF_AIQuotaLimits limits, int epoch)
	{
		m_Limits = limits;
		m_iEpoch = epoch;
		m_aPhysical.Resize(AICF_EAIQuotaBucket.COUNT);
		m_aReserved.Resize(AICF_EAIQuotaBucket.COUNT);
	}

	protected bool IsConfigured()
	{
		return m_Limits && m_Limits.IsConfigured() && m_iEpoch > 0;
	}

	// counts уже содержат все physical agents, включая ранее Bind-нутые.
	// Reserved включает только ещё НЕ materialized, поэтому второго учёта нет.
	bool ApplyCensus(array<int> counts, bool complete)
	{
		m_bCensusKnown = false;
		if (!IsConfigured() || !complete || !counts || counts.Count() != AICF_EAIQuotaBucket.COUNT)
			return false;
		for (int index; index < AICF_EAIQuotaBucket.COUNT; index++)
		{
			int occupied;
			if (!AICF_AIQuotaMath.AddBounded(counts[index], m_aReserved[index], m_Limits.GetLimit(index), occupied))
				return false;
		}
		for (int copyIndex; copyIndex < AICF_EAIQuotaBucket.COUNT; copyIndex++)
			m_aPhysical[copyIndex] = counts[copyIndex];
		m_bCensusKnown = true;
		return true;
	}

	bool CanReserve(AICF_EAIQuotaBucket bucket, int units)
	{
		if (!IsConfigured() || !m_bCensusKnown || bucket < 0 || bucket >= AICF_EAIQuotaBucket.COUNT || units <= 0)
			return false;
		return AICF_AIQuotaMath.FitsBucket(m_aPhysical[bucket], m_aReserved[bucket], m_Limits.GetLimit(bucket), units);
	}

	// Full basket до native Enqueue/PerformSpawn. Partial reservation запрещена.
	AICF_AIQuotaReservation Reserve(AICF_EAIQuotaBucket bucket, int units, int ownerSerial)
	{
		if (ownerSerial <= 0 || !CanReserve(bucket, units) || units > AICF_AIQuotaMath.MAX_TOTAL_UNITS - m_iOutstanding)
			return null;
		AICF_AIQuotaReservation reservation = new AICF_AIQuotaReservation();
		reservation.m_Ledger = this;
		reservation.m_iEpoch = m_iEpoch;
		reservation.m_iOwnerSerial = ownerSerial;
		reservation.m_Bucket = bucket;
		reservation.m_iAvailable = units;
		m_aReserved[bucket] = m_aReserved[bucket] + units;
		m_iOutstanding += units;
		return reservation;
	}

	protected bool OwnsReservation(AICF_AIQuotaReservation reservation, int ownerSerial)
	{
		return IsConfigured() && reservation && reservation.m_Ledger == this &&
			reservation.m_iEpoch == m_iEpoch && ownerSerial > 0 && reservation.m_iOwnerSerial == ownerSerial &&
			reservation.m_Bucket >= 0 && reservation.m_Bucket < AICF_EAIQuotaBucket.COUNT;
	}

	bool HasAvailableReservation(AICF_AIQuotaReservation reservation, int ownerSerial)
	{
		return m_bCensusKnown && OwnsReservation(reservation, ownerSerial) && !reservation.m_bCancelled && reservation.m_iAvailable > 0;
	}

	// Caller обязан взять ticket ДО super/native; nested callback получает свой
	// exact ticket, а не неявный ambient permission текущего stack.
	AICF_AIQuotaDispatch BeginDispatch(AICF_AIQuotaReservation reservation, int ownerSerial)
	{
		if (!m_bCensusKnown || !OwnsReservation(reservation, ownerSerial) || reservation.m_bCancelled || reservation.m_iAvailable <= 0)
			return null;
		AICF_AIQuotaDispatch ticket = new AICF_AIQuotaDispatch();
		ticket.m_Ledger = this;
		ticket.m_Reservation = reservation;
		ticket.m_iEpoch = m_iEpoch;
		ticket.m_iOwnerSerial = ownerSerial;
		reservation.m_iAvailable--;
		return ticket;
	}

	protected bool OwnsDispatch(AICF_AIQuotaDispatch ticket, int ownerSerial)
	{
		return ticket && ticket.m_Ledger == this && ticket.m_iEpoch == m_iEpoch && !ticket.m_bClosed &&
			OwnsReservation(ticket.m_Reservation, ownerSerial) && ticket.m_iOwnerSerial == ownerSerial;
	}

	// Exact engine identity был доказан adapter. Конвертация атомарна ДО чужих
	// native OnAgentAdded listeners: pending-- и physical++ в одном методе.
	bool Bind(AICF_AIQuotaDispatch ticket, int ownerSerial, string entityKey)
	{
		if (!m_bCensusKnown || !OwnsDispatch(ticket, ownerSerial) || entityKey.IsEmpty() || m_aBoundKeys.Contains(entityKey) ||
			m_aBoundKeys.Count() >= AICF_AIQuotaMath.MAX_TOTAL_UNITS)
			return false;
		AICF_EAIQuotaBucket bucket = ticket.m_Reservation.m_Bucket;
		if (m_aReserved[bucket] <= 0 || m_iOutstanding <= 0 || m_aPhysical[bucket] >= m_Limits.GetLimit(bucket))
			return false;
		ticket.m_bClosed = true;
		m_aReserved[bucket] = m_aReserved[bucket] - 1;
		m_iOutstanding--;
		m_aPhysical[bucket] = m_aPhysical[bucket] + 1;
		m_aBoundKeys.Insert(entityKey);
		m_aBoundBuckets.Insert(bucket);
		return true;
	}

	bool FailDispatch(AICF_AIQuotaDispatch ticket, int ownerSerial)
	{
		if (!OwnsDispatch(ticket, ownerSerial)) return false;
		AICF_EAIQuotaBucket bucket = ticket.m_Reservation.m_Bucket;
		if (m_aReserved[bucket] <= 0 || m_iOutstanding <= 0) return false;
		ticket.m_bClosed = true;
		m_aReserved[bucket] = m_aReserved[bucket] - 1;
		m_iOutstanding--;
		return true;
	}

	bool CancelAvailable(AICF_AIQuotaReservation reservation, int ownerSerial)
	{
		if (!OwnsReservation(reservation, ownerSerial) || reservation.m_bCancelled) return false;
		int available = reservation.m_iAvailable;
		if (available < 0 || available > m_aReserved[reservation.m_Bucket] || available > m_iOutstanding) return false;
		reservation.m_bCancelled = true;
		reservation.m_iAvailable = 0;
		m_aReserved[reservation.m_Bucket] = m_aReserved[reservation.m_Bucket] - available;
		m_iOutstanding -= available;
		return true;
	}

	// Retire только по доказанному исчезновению exact identity. Число physical
	// здесь НЕ уменьшается: новая capacity появится только после fresh census.
	bool RetireBinding(string entityKey)
	{
		int index = m_aBoundKeys.Find(entityKey);
		if (entityKey.IsEmpty() || index < 0) return false;
		m_aBoundKeys.Remove(index);
		m_aBoundBuckets.Remove(index);
		return true;
	}

	int GetBindingBucket(string entityKey)
	{
		int index = m_aBoundKeys.Find(entityKey);
		if (entityKey.IsEmpty() || index < 0 || index >= m_aBoundBuckets.Count()) return -1;
		return m_aBoundBuckets[index];
	}

	// The existing physical unit is already in a complete census. No increment.
	bool RegisterExistingBinding(string entityKey, AICF_EAIQuotaBucket bucket)
	{
		if (!IsConfigured() || !m_bCensusKnown || entityKey.IsEmpty() || bucket < 0 || bucket >= AICF_EAIQuotaBucket.COUNT) return false;
		int existing = m_aBoundKeys.Find(entityKey);
		if (existing >= 0) return GetBindingBucket(entityKey) == bucket;
		if (m_aBoundKeys.Count() >= AICF_AIQuotaMath.MAX_TOTAL_UNITS || m_aBoundKeys.Count() != m_aBoundBuckets.Count()) return false;
		int bound;
		foreach (int assignedBucket : m_aBoundBuckets)
			if (assignedBucket == bucket) bound++;
		if (bound >= m_aPhysical[bucket]) return false;
		m_aBoundKeys.Insert(entityKey);
		m_aBoundBuckets.Insert(bucket);
		return true;
	}

	// Caller repeats actual ownership/lease epoch before this pure conversion.
	bool TransferBinding(string entityKey, AICF_EAIQuotaBucket fromBucket, AICF_EAIQuotaBucket toBucket)
	{
		if (!m_bCensusKnown || fromBucket < 0 || fromBucket >= AICF_EAIQuotaBucket.COUNT ||
			toBucket < 0 || toBucket >= AICF_EAIQuotaBucket.COUNT || GetBindingBucket(entityKey) != fromBucket) return false;
		if (fromBucket == toBucket) return true;
		if (m_aPhysical[fromBucket] <= 0 || !AICF_AIQuotaMath.FitsBucket(m_aPhysical[toBucket], m_aReserved[toBucket], m_Limits.GetLimit(toBucket), 1)) return false;
		int index = m_aBoundKeys.Find(entityKey);
		m_aPhysical[fromBucket] = m_aPhysical[fromBucket] - 1;
		m_aPhysical[toBucket] = m_aPhysical[toBucket] + 1;
		m_aBoundBuckets[index] = toBucket;
		return true;
	}

	// Reuse: one destination reservation is consumed; no new body is fabricated.
	bool BindTransferred(AICF_AIQuotaDispatch ticket, int ownerSerial, string entityKey, AICF_EAIQuotaBucket fromBucket)
	{
		if (!m_bCensusKnown || !OwnsDispatch(ticket, ownerSerial) || fromBucket < 0 || fromBucket >= AICF_EAIQuotaBucket.COUNT ||
			GetBindingBucket(entityKey) != fromBucket || m_aPhysical[fromBucket] <= 0) return false;
		AICF_EAIQuotaBucket target = ticket.m_Reservation.m_Bucket;
		if (m_aReserved[target] <= 0 || m_iOutstanding <= 0 || (fromBucket != target && m_aPhysical[target] >= m_Limits.GetLimit(target))) return false;
		ticket.m_bClosed = true;
		m_aReserved[target] = m_aReserved[target] - 1;
		m_iOutstanding--;
		if (fromBucket != target)
		{
			m_aPhysical[fromBucket] = m_aPhysical[fromBucket] - 1;
			m_aPhysical[target] = m_aPhysical[target] + 1;
			m_aBoundBuckets[m_aBoundKeys.Find(entityKey)] = target;
		}
		return true;
	}

	int GetPhysical(AICF_EAIQuotaBucket bucket)
	{
		if (!m_bCensusKnown || bucket < 0 || bucket >= AICF_EAIQuotaBucket.COUNT) return -1;
		return m_aPhysical[bucket];
	}

	int GetReserved(AICF_EAIQuotaBucket bucket)
	{
		if (bucket < 0 || bucket >= AICF_EAIQuotaBucket.COUNT) return -1;
		return m_aReserved[bucket];
	}
}

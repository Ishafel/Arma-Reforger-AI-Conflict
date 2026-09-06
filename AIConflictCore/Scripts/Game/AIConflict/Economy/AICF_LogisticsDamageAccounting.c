// Наблюдаем exact synchronous stock supplies fire; не меняем damage behavior.
// Пожар может уменьшать cargo ещё до DESTROYED всего vehicle. Такой расход
// доказан stock operation, поэтому не записывается как внешний player transfer.
modded class SCR_VehicleDamageManagerComponent
{
	protected AICF_LogisticsWorker m_AICFLogisticsCustody;

	void AICF_SetLogisticsCustody(AICF_LogisticsWorker worker)
	{
		m_AICFLogisticsCustody = worker;
	}

	protected override void UpdateSuppliesFireState(float fireRate, float timeSlice)
	{
		AICF_LogisticsWorker worker = m_AICFLogisticsCustody;
		bool observed = Replication.IsServer() && worker && worker.m_bCustody &&
			worker.m_Vehicle == GetOwner() && worker.m_VehicleId == GetOwner().GetID() &&
			worker.m_CargoPool && worker.m_CargoPool.Valid();
		float before;
		if (observed)
		{
			AICF_LogisticsLedger.Observe(worker);
			before = worker.m_CargoPool.Value();
		}
		super.UpdateSuppliesFireState(fireRate, timeSlice);
		if (observed) AICF_LogisticsLedger.RecordStockFireLoss(worker, before);
	}
}

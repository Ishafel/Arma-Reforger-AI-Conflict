// Изолированный observer: не меняет native решение или registration lifecycle.
modded class SCR_AIGetEmptyCompartment
{
	protected IEntity m_AICFProbeVehicle;
	override void OnEnter(AIAgent owner)
	{
		super.OnEnter(owner);
		GetVariableIn(PORT_VEHICLE, m_AICFProbeVehicle);
	}
	override ENodeResult EOnTaskSimulate(AIAgent owner, float dt)
	{
		SCR_AIGroup group = SCR_AIGroup.Cast(owner);
		if (!group && owner) group = SCR_AIGroup.Cast(owner.GetParentGroup());
		SCR_AIGroupUtilityComponent utility;
		if (group) utility = group.GetGroupUtilityComponent();
		if (m_AICFProbeVehicle && utility && utility.m_VehicleMgr && !utility.m_VehicleMgr.FindVehicle(m_AICFProbeVehicle))
		{
			IEntity actor = owner.GetControlledEntity();
			string actorId = "NONE";
			if (actor) actorId = actor.GetID().ToString();
			Print(string.Format("[AICF][NATIVE_REGISTRATION_LOST] actor=%1 agent=%2 group=%3 vehicle=%4 manager=%5", actorId, owner.GetID(), group.GetID(), m_AICFProbeVehicle.GetID(), utility.m_VehicleMgr));
		}
		return super.EOnTaskSimulate(owner, dt);
	}
}

modded class SCR_AIGroupVehicleManager
{
	override bool RemoveVehicle(notnull SCR_AIVehicleUsageComponent vehicleUsageComp)
	{
		IEntity vehicle = vehicleUsageComp.GetOwner();
		string vehicleId = "NONE";
		if (vehicle) vehicleId = vehicle.GetID().ToString();
		Print(string.Format("[AICF][NATIVE_REGISTRATION_REMOVE] vehicle=%1 manager=%2 had_record=%3", vehicleId, this, FindVehicle(vehicleUsageComp) != null));
		Debug.DumpStack();
		return super.RemoveVehicle(vehicleUsageComp);
	}
}

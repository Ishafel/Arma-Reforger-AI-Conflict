// Native остаётся единственным создателем/исполнителем behavior этого message.
[BaseContainerProps()]
modded class SCR_AIGoalReaction_GetInVehicle
{
	override void PerformReaction(notnull SCR_AIUtilityComponent utility, SCR_AIMessageBase message)
	{
		SCR_AIMessage_GetIn getIn = SCR_AIMessage_GetIn.Cast(message);
		AICF_VehiclePresetNativeSeatRequest request = AICF_VehiclePresetNativeSeatPolicy.Prepare(utility, getIn);
		if (!request)
		{
			super.PerformReaction(utility, message);
			return;
		}
		if (!request.m_bDispatch) return;
		if (!request.IsCurrent())
		{
			request.RollbackReservation();
			request.Log("PRE_DISPATCH_IDENTITY_CHANGED");
			return;
		}
		super.PerformReaction(utility, request.m_Message);
		if (!AICF_VehiclePresetNativeSeatPolicy.HasOwnedExactAction(request))
		{
			request.RollbackReservation();
			request.Log("NATIVE_ACTION_NOT_CREATED");
		}
	}
}

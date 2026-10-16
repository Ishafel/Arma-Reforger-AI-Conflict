// Только post-fix stage, после AICF_FIAProximityProbe.c.
modded class AICF_MatchController
{
	protected bool m_bAICFFIASafetyChecked;
	override protected void AICF_FIAProbeSafetyChecks()
	{
		super.AICF_FIAProbeSafetyChecks();
		if (m_bAICFFIASafetyChecked) return;
		m_bAICFFIASafetyChecked = true;
		AICF_FIAGarrison g = m_AICFFIAProbeTarget;
		AICF_FIAProximityProbe.Check("CURRENT_IDENTITY", g.m_Lifecycle && g.m_Lifecycle.Current());
		g.m_iGeneration++;
		AICF_FIAProximityProbe.Check("STALE_GENERATION", !g.m_Lifecycle.Current() && !g.m_Group.ExpandOneMember());
		g.m_iGeneration--;
		EntityID saved = g.m_VehicleId;
		g.m_VehicleId = EntityID.INVALID;
		AICF_FIAProximityProbe.Check("STALE_VEHICLE_ID", !g.m_Lifecycle.Current() && !g.m_Group.ExpandOneMember());
		g.m_VehicleId = saved;
		string reason;
		AICF_FIAProximityProbe.Check("UNKNOWN_OCCUPANCY_IDENTITY", !AICF_VehicleCleanupManager.CanDormantFIA(null, saved, g.m_Group, g.m_aCrew, g.m_aCrewIds, g.m_DesantGroup, reason));
	}
}

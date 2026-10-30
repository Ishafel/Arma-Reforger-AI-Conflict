// Initial FIA assets have their own bounded plan and actual service registry.
// Faction alone is never sufficient proof of ownership.
class AICF_AIQuotaFIAVehicleCreation : AICF_AIQuotaVehicleCreationGuard
{
	protected AICF_FIAGarrison m_Garrison;
	protected AICF_FIAPatrol m_Patrol;
	protected int m_iGeneration;
	protected SCR_CampaignMilitaryBaseComponent m_Base;
	protected EntityID m_BaseId;
	protected ResourceName m_Prefab;

	static AICF_AIQuotaFIAVehicleCreation CaptureGarrison(AICF_FIAGarrison garrison, AICF_FactionFleet fleet)
	{
		if (!garrison || !garrison.BaseIdentity() || !garrison.m_Faction) return null;
		AICF_AIQuotaFIAVehicleCreation guard = new AICF_AIQuotaFIAVehicleCreation();
		guard.m_Garrison = garrison;
		guard.m_iGeneration = garrison.m_iGeneration;
		guard.m_Base = garrison.m_Base;
		guard.m_BaseId = garrison.m_BaseId;
		guard.m_Prefab = garrison.m_sPrefab;
		if (guard.m_Prefab.IsEmpty()) return null;
		if (!guard.Initialize(AICF_AIWorldQuotaService.GetAuthorityForWorld(GetGame().GetWorld()),
			AICF_EAIQuotaBucket.FIA_EXTRA, 1, garrison.m_Faction.GetFactionKey(), guard.m_Base.GetOwner(), false)) return null;
		if (!guard.AttachPendingLease(fleet, garrison.m_Lease)) return null;
		return guard;
	}

	static AICF_AIQuotaFIAVehicleCreation CapturePatrol(AICF_FIAPatrol patrol, AICF_FactionFleet fleet, ResourceName prefab)
	{
		if (!patrol || !patrol.m_Current || !patrol.m_Current.GetOwner() || !patrol.m_Faction) return null;
		AICF_AIQuotaFIAVehicleCreation guard = new AICF_AIQuotaFIAVehicleCreation();
		guard.m_Patrol = patrol;
		guard.m_iGeneration = patrol.m_iGeneration;
		guard.m_Base = patrol.m_Current;
		guard.m_BaseId = guard.m_Base.GetOwner().GetID();
		guard.m_Prefab = prefab;
		if (guard.m_Prefab.IsEmpty()) return null;
		if (!guard.Initialize(AICF_AIWorldQuotaService.GetAuthorityForWorld(GetGame().GetWorld()),
			AICF_EAIQuotaBucket.FIA_EXTRA, 1, patrol.m_Faction.GetFactionKey(), guard.m_Base.GetOwner(), false)) return null;
		if (!guard.AttachPendingLease(fleet, patrol.m_Lease)) return null;
		return guard;
	}

	override bool IsDomainCurrent()
	{
		if (!m_Controller || !m_Base || !m_Base.GetOwner() || m_Base.GetOwner().GetID() != m_BaseId ||
			!m_Base.GetFaction() || m_Base.GetFaction().GetFactionKey() != m_sFaction) return false;
		if (m_Garrison)
			return !m_Patrol && m_Controller.OwnsAIQuotaGarrison(m_Garrison) && !m_Garrison.m_bRetired &&
				m_Garrison.m_iGeneration == m_iGeneration && m_Garrison.m_Base == m_Base && m_Garrison.m_sPrefab == m_Prefab &&
				(!m_Lease || m_Garrison.m_Lease == m_Lease);
		return m_Patrol && m_Controller.OwnsAIQuotaPatrol(m_Patrol) && !m_Patrol.m_bRetired &&
			m_Patrol.m_iGeneration == m_iGeneration && m_Patrol.m_Current == m_Base &&
			(!m_Lease || m_Patrol.m_Lease == m_Lease);
	}

	override AICF_AIQuotaCreation BeginFactory(ResourceName prefab)
	{
		if (prefab != m_Prefab) return null;
		return super.BeginFactory(prefab);
	}
}

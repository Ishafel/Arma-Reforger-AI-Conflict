// Creation permission for one exact stock Defend waypoint returned by our
// authoritative factory. Stock AI waypoints can be native local entities with
// no RplComponent; that is not permission to adopt any other local entity.
class AICF_AcquisitionHoldWaypointCreation
{
	protected AICF_VehicleWaypointFactory m_Factory;
	protected AICF_AcquisitionStagingReceipt m_Receipt;
	protected AIWaypoint m_Waypoint;
	protected EntityID m_Id;
	protected BaseWorld m_World;
	protected string m_sPrefab;
	protected string m_sRpl;
	protected bool m_bHadRpl;

	static AICF_AcquisitionHoldWaypointCreation Capture(AICF_VehicleWaypointFactory factory, AIWaypoint waypoint,
		string prefab, AICF_AcquisitionStagingReceipt receipt)
	{
		if (!Replication.IsServer() || !GetGame() || !factory || !waypoint || !receipt || prefab.IsEmpty()) return null;
		BaseWorld world = waypoint.GetWorld();
		if (!world || receipt.GetWorld() != world || GetGame().GetWorld() != world || world.FindEntityByID(waypoint.GetID()) != waypoint ||
			SCR_ResourceNameUtils.GetPrefabName(waypoint) != prefab) return null;
		RplComponent rpl = RplComponent.Cast(waypoint.FindComponent(RplComponent));
		if (rpl && !AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(waypoint)) return null;
		AICF_AcquisitionHoldWaypointCreation created = new AICF_AcquisitionHoldWaypointCreation();
		created.m_Factory = factory;
		created.m_Receipt = receipt;
		created.m_Waypoint = waypoint;
		created.m_Id = waypoint.GetID();
		created.m_World = world;
		created.m_sPrefab = prefab;
		created.m_sRpl = AICF_VehicleBehaviorBinding.ComponentRpl(waypoint);
		created.m_bHadRpl = rpl != null;
		if (!created.Matches(waypoint, receipt)) return null;
		return created;
	}

	bool Matches(AIWaypoint waypoint, AICF_AcquisitionStagingReceipt receipt)
	{
		if (!Replication.IsServer() || !GetGame() || !m_Factory || !waypoint || waypoint != m_Waypoint ||
			waypoint.GetID() != m_Id || !receipt || receipt != m_Receipt || !m_World || receipt.GetWorld() != m_World || GetGame().GetWorld() != m_World ||
			waypoint.GetWorld() != m_World || m_World.FindEntityByID(m_Id) != waypoint ||
			SCR_ResourceNameUtils.GetPrefabName(waypoint) != m_sPrefab ||
			AICF_VehicleBehaviorBinding.ComponentRpl(waypoint) != m_sRpl) return false;
		RplComponent rpl = RplComponent.Cast(waypoint.FindComponent(RplComponent));
		if ((rpl != null) != m_bHadRpl) return false;
		return !rpl || AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(waypoint);
	}

	static bool HasAuthority(AIWaypoint waypoint, AICF_AcquisitionStagingReceipt receipt, AICF_AcquisitionHoldWaypointCreation created)
	{
		if (created) return created.Matches(waypoint, receipt);
		return AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(waypoint);
	}
}

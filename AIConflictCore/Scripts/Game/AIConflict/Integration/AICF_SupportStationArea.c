// The client can receive a vehicle's visual child in rounded world coordinates.
// Restore the station-local center before native validation and mesh generation.
modded class SCR_SupportStationAreaMeshComponent
{
	override protected void Init(IEntity owner)
	{
		AICF_AlignSupportArea(owner);
		super.Init(owner);
	}

	override void GenerateAreaMesh()
	{
		AICF_AlignSupportArea(GetOwner());
		super.GenerateAreaMesh();
	}

	protected void AICF_AlignSupportArea(IEntity owner)
	{
		if (!owner) return;
		if (!m_SupportStationComponent) m_SupportStationComponent = GetSupportStation(owner);
		if (!m_SupportStationComponent || !m_SupportStationComponent.UsesRange()) return;
		// Only this visual child belongs to the station's coordinate frame.
		// Other hierarchies retain native validation; the station never moves.
		if (owner.GetParent() != m_SupportStationComponent.GetOwner()) return;
		vector offset = m_SupportStationComponent.GetOffset();
		if (offset == vector.Zero) return;
		vector localTransform[4];
		owner.GetLocalTransform(localTransform);
		if (localTransform[3] == offset) return;
		localTransform[3] = offset;
		owner.SetLocalTransform(localTransform);
	}
}

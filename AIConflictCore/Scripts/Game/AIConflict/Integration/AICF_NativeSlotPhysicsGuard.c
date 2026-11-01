// Invocation-local evidence: an unknown trace hit cannot prove a free slot.
class AICF_NativeSlotOccupancyFrame
{
	bool m_bUnknown;
}

modded class SCR_EntitySpawnerSlotComponent
{
	protected ref AICF_NativeSlotOccupancyFrame m_AICF_OccupancyFrame;

	override bool IsOccupied()
	{
		AICF_NativeSlotOccupancyFrame previous = m_AICF_OccupancyFrame;
		AICF_NativeSlotOccupancyFrame frame = new AICF_NativeSlotOccupancyFrame();
		m_AICF_OccupancyFrame = frame;
		bool occupied = super.IsOccupied();
		bool current = m_AICF_OccupancyFrame == frame;
		if (current)
			m_AICF_OccupancyFrame = previous;
		if (!current)
			return true;
		return occupied || frame.m_bUnknown;
	}

	protected bool AICF_IsOccupancyFrameCurrent(AICF_NativeSlotOccupancyFrame frame)
	{
		if (m_AICF_OccupancyFrame == frame)
			return true;
		if (frame)
			frame.m_bUnknown = true;
		return false;
	}

	// Diagnostic extension point. The unknown/occupied decision is already final.
	protected void AICF_ObserveUnknownOccupancyHit(IEntity ent, string reason)
	{
	}

	// Projection of pinned 1.8.0.13 TraceCallback, SHA256 of its source file:
	// 4E0FCC958A1DD15951689F4038D7AA0692352846E7BE5B05F83F19EAC2CAB24F.
	// The original loaded/cloth/weapon/proxy/character/wreck policy is retained.
	// Unknown Physics accepts the collision; the frame also covers null TraceEnt.
	override protected bool TraceCallback(IEntity ent)
	{
		AICF_NativeSlotOccupancyFrame frame = m_AICF_OccupancyFrame;
		if (!ent)
		{
			if (frame)
				frame.m_bUnknown = true;
			AICF_ObserveUnknownOccupancyHit(ent, "NULL_ENTITY");
			return true;
		}
		if (ent.IsLoaded())
		{
			if (!AICF_IsOccupancyFrameCurrent(frame))
				return true;
			return false;
		}
		Physics physics = ent.GetPhysics();
		if (!physics)
		{
			if (frame)
				frame.m_bUnknown = true;
			AICF_ObserveUnknownOccupancyHit(ent, "NULL_PHYSICS");
			return true;
		}
		if (physics.GetSimulationState() == 0 || ent.FindComponent(BaseLoadoutClothComponent) || ent.FindComponent(WeaponComponent))
		{
			if (!AICF_IsOccupancyFrameCurrent(frame))
				return true;
			return false;
		}

		EntityFlags entityFlags = ent.GetFlags();
		if (entityFlags & EntityFlags.PROXY)
		{
			if (!AICF_IsOccupancyFrameCurrent(frame))
				return true;
			return false;
		}
		if (ent.IsInherited(ChimeraCharacter))
		{
			if (!AICF_IsOccupancyFrameCurrent(frame))
				return true;
			return false;
		}

		if (IsEntityDestroyed(ent))
		{
			if (ent.IsInherited(Vehicle) && m_RplComponent)
			{
				if (!m_RplComponent.IsProxy())
				{
					if (!AICF_IsOccupancyFrameCurrent(frame))
						return true;
					SCR_EntityHelper.DeleteEntityAndChildren(ent);
				}
				if (!AICF_IsOccupancyFrameCurrent(frame))
					return true;
				return false;
			}
		}
		return true;
	}
}

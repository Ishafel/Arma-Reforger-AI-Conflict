// Навык охраны FIA применяется после инициализации faction и AI ownership.
modded class SCR_AICombatComponent
{
	protected EntityID m_iAICFFIACombatIdentity;

	override void EOnInit(IEntity owner)
	{
		super.EOnInit(owner);
		if (!Replication.IsServer() || !GetGame() || !owner ||
			owner.GetWorld() != GetGame().GetWorld())
			return;
		m_iAICFFIACombatIdentity = owner.GetID();
		GetGame().GetCallqueue().CallLater(AICF_ApplyFIACombatSkill, 1500, false);
	}

	protected void AICF_ApplyFIACombatSkill()
	{
		IEntity owner = GetOwner();
		if (!owner || owner.GetID() != m_iAICFFIACombatIdentity ||
			!AICF_VehicleBoardingMutationFence.IsAuthoritativeAIEntity(owner))
			return;
		Faction faction = SCR_Faction.GetEntityFaction(owner);
		if (!faction || faction.GetFactionKey() != "FIA")
			return;
		SetAISkill(EAISkill.EXPERT);
		AICF_Stage35Diagnostics.Info("FIA_COMBAT_POLICY_APPLIED",
			string.Format("entity=%1 faction=FIA skill=EXPERT", owner.GetID()));
	}

	override void OnDelete(IEntity owner)
	{
		if (GetGame())
			GetGame().GetCallqueue().Remove(AICF_ApplyFIACombatSkill);
		super.OnDelete(owner);
	}
}

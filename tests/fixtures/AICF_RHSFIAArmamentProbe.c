// Только RHS stage, поверх общей AICF_FIAArmamentProbe.
class AICF_ARMNegative
{
	static bool s_bUnavailable;
}

modded class AICF_RHSContentProfile
{
	override ResourceName GetPMCPrimaryOverride(ResourceName source, int variant)
	{
		if (AICF_ARMNegative.s_bUnavailable) return "{0000000000000052}Prefabs/AICF_MissingWeapon.et";
		return super.GetPMCPrimaryOverride(source, variant);
	}
}

modded class SCR_ChimeraCharacter
{
	void AICF_ARMForceVariant(int variant) { m_iAICFPMCWeaponVariant = variant; }
	void AICF_ARMCancelInit() { GetGame().GetCallqueue().Remove(AICF_ApplyPMCEquipment); }
	void AICF_ARMPending(bool pending) { m_bAICFPMCEquipmentPending = pending; }
}

modded class AICF_RHSPMCTransaction
{
	static bool s_bARMRefuse;
	override bool RestoreNew(SCR_ChimeraCharacter character, string snapshot)
	{
		if (s_bARMRefuse)
		{
			s_bARMRefuse = false;
			// Native insertion получает заведомо несовместимый с weapon slot предмет.
			ResourceName primary = AICF_RHSPMCArmament.PrimaryPrefab(SCR_ResourceNameUtils.GetPrefabName(character), character.AICF_PMCWeaponVariant());
			string guid = SCR_ResourceNameUtils.GetPrefabGUID(primary);
			bool matched = snapshot.Contains(guid);
			snapshot.Replace(guid, "645C73791ECA1698");
			Print(string.Format("[AICF][FIA_ARMAMENT_INSERTION_FAULT] matched=%1 guid=%2", matched, guid));
			return AICF_LoadoutInventory.Restore(character, snapshot);
		}
		return super.RestoreNew(character, snapshot);
	}
}

modded class SCR_GameModeCampaign
{
	protected ref array<string> m_aARMSecondary = {};
	protected ref set<int> m_aARMAutomatic = new set<int>();

	override protected int AICF_ARMCount(ResourceName source) { return AICF_RHSPMCArmament.VariantCount(source); }

	override protected void AICF_ARMSpawned(SCR_ChimeraCharacter character, int variant)
	{
		m_aARMSecondary.Insert(AICF_ARMInventory(character, true));
		if (variant >= 0) character.AICF_ARMForceVariant(variant);
	}

	override protected void AICF_ARMExtra()
	{
		ResourceName rifleman = "{84B40583F4D1B7A3}Prefabs/Characters/Factions/INDFOR/FIA/Character_FIA_Rifleman.et";
		for (int i; i < 8; i++) AICF_ARMSpawnOne(rifleman, -1);
		AICF_ARMSpawnOne(rifleman, 0);
		SCR_ChimeraCharacter negative = m_aARMCharacters[m_aARMCharacters.Count() - 1];
		negative.AICF_ARMCancelInit();
		Print("[AICF][FIA_ARMAMENT_NEGATIVE] phase=BEGIN");
		string before;
		AICF_LoadoutInventory.Capture(negative, before);
		negative.AICF_ARMForceVariant(999);
		bool invalid = AICF_RHSPMCEquipment.Apply(negative);
		string after;
		AICF_LoadoutInventory.Capture(negative, after);
		AICF_ARMCheck("INVALID_SELECTION_UNCHANGED", !invalid && before == after && negative.AICF_IsPMCEquipmentPending());
		negative.AICF_ARMForceVariant(0);
		AICF_ARMNegative.s_bUnavailable = true;
		bool unavailable = AICF_RHSPMCEquipment.Apply(negative);
		AICF_ARMNegative.s_bUnavailable = false;
		AICF_LoadoutInventory.Capture(negative, after);
		AICF_ARMCheck("UNAVAILABLE_CANDIDATE_UNCHANGED", !unavailable && before == after && negative.AICF_PMCWeaponVariant() == 0);
		AICF_RHSPMCTransaction.s_bARMRefuse = true;
		bool refused = AICF_RHSPMCEquipment.Apply(negative);
		AICF_LoadoutInventory.Capture(negative, after);
		AICF_ARMCheck("NATIVE_REFUSAL_ROLLBACK", !refused && before == after && negative.AICF_PMCWeaponVariant() == 0 && negative.AICF_IsPMCEquipmentPending());
		AICF_ARMCheck("RETRY_STABLE", AICF_RHSPMCEquipment.Apply(negative) && negative.AICF_PMCWeaponVariant() == 0 && negative.AICF_PMCEquipmentApplied());
		Print("[AICF][FIA_ARMAMENT_NEGATIVE] phase=END");
	}

	override protected void AICF_ARMVerify(SCR_ChimeraCharacter character, int index)
	{
		ResourceName source = SCR_ResourceNameUtils.GetPrefabName(character);
		int variant = character.AICF_PMCWeaponVariant();
		BaseWeaponComponent weapon = AICF_RHSPMCArmament.Primary(character);
		bool valid = character.AICF_PMCEquipmentApplied() && !character.AICF_IsPMCEquipmentPending() && AICF_RHSPMCArmament.Validate(character, source, variant);
		ResourceName prefab = AICF_RHSPMCArmament.PrimaryPrefab(source, variant);
		Resource resource = Resource.Load(prefab);
		AICF_ARMCheck("CANONICAL_RESOLVED", resource && resource.IsValid() && weapon && SCR_ResourceNameUtils.GetPrefabName(weapon.GetOwner()) == prefab, "source=" + source + " variant=" + variant.ToString() + " weapon=" + prefab);
		AICF_ARMCheck("LIVE_ARMAMENT", valid, "source=" + source + " variant=" + variant.ToString() + " inventory=" + AICF_ARMInventory(character));
		if (weapon && weapon.GetCurrentMuzzle() && weapon.GetCurrentMuzzle().GetMagazine())
		{
			BaseMuzzleComponent muzzle = weapon.GetCurrentMuzzle();
			Print(string.Format("[AICF][FIA_ARMAMENT_READBACK] source=%1 variant=%2 weapon=%3 magazine=%4 spares=%5 suppressed=%6", source, variant, SCR_ResourceNameUtils.GetPrefabName(weapon.GetOwner()), SCR_ResourceNameUtils.GetPrefabName(muzzle.GetMagazine().GetOwner()), AICF_RHSPMCArmament.SpareMagazines(character, muzzle), muzzle.IsMuzzleSuppressed()));
		}
		AICF_ARMCheck("SECONDARY_PRESERVED", AICF_ARMInventory(character, true) == m_aARMSecondary[index], "source=" + source);
		if (m_aARMVariants[index] >= 0) AICF_ARMCheck("SELECTION_STABLE", variant == m_aARMVariants[index]);
		else m_aARMAutomatic.Insert(variant);
		if (weapon)
		{
			EWeaponType type = weapon.GetWeaponType();
			if (source.EndsWith("_MG.et")) AICF_ARMCheck("MG_ROLE", type == EWeaponType.WT_MACHINEGUN);
			if (source.EndsWith("_Sharpshooter.et")) AICF_ARMCheck("SNIPER_ROLE", type == EWeaponType.WT_SNIPERRIFLE || type == EWeaponType.WT_RIFLE);
			// Изменение игрока: расход одного патрона после успешной подготовки.
			BaseMagazineComponent magazine = weapon.GetCurrentMuzzle().GetMagazine();
			if (magazine) magazine.SetAmmoCount(Math.Max(0, magazine.GetAmmoCount() - 1));
		}
		string before;
		AICF_LoadoutInventory.Capture(character, before);
		EntityID primaryId;
		if (weapon) primaryId = weapon.GetOwner().GetID();
		bool repeated = AICF_RHSPMCEquipment.Apply(character);
		string after;
		AICF_LoadoutInventory.Capture(character, after);
		AICF_ARMCheck("REPEAT_PRESERVES_USER_CHANGE", repeated && before == after && (!weapon || weapon.GetOwner().GetID() == primaryId));
	}

	override protected void AICF_ARMExtraChecks()
	{
		ResourceName source = "{84B40583F4D1B7A3}Prefabs/Characters/Factions/INDFOR/FIA/Character_FIA_Rifleman.et";
		AICF_ARMCheck("AUTOMATIC_DIVERSITY", m_aARMAutomatic.Count() == AICF_RHSPMCArmament.VariantCount(source));
		AICF_ARMCheck("PATROL_PATH", AICF_ARMPaths.s_iPatrolReady > 0 && AICF_ARMPaths.s_bPatrolPending);
		AICF_ARMCheck("GARRISON_DESANT_PATH", AICF_ARMPaths.s_iGarrisonReady > 0 && AICF_ARMPaths.s_bGarrisonPending && AICF_ARMPaths.s_iPassengers > 0);
		AICF_ARMCheck("PATHS_NO_FAILURES", AICF_ARMPaths.s_iFailures == 0);
	}
}

class AICF_ARMPaths
{
	static bool s_bPatrolPending;
	static bool s_bGarrisonPending;
	static int s_iPatrolReady;
	static int s_iGarrisonReady;
	static int s_iPassengers;
	static int s_iFailures;

	static bool Ready(SCR_AIGroup group)
	{
		array<AIAgent> agents = {};
		group.GetAgents(agents);
		if (agents.IsEmpty()) return false;
		foreach (AIAgent agent : agents)
		{
			SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(agent.GetControlledEntity());
			if (!character || !character.AICF_PMCEquipmentApplied()) return false;
		}
		return true;
	}

	static SCR_ChimeraCharacter First(SCR_AIGroup group)
	{
		array<AIAgent> agents = {};
		group.GetAgents(agents);
		if (agents.IsEmpty()) return null;
		return SCR_ChimeraCharacter.Cast(agents[0].GetControlledEntity());
	}
}

modded class AICF_FIAPatrolCrew
{
	override bool Board(AICF_FIAPatrol p)
	{
		if (p && p.GroupIdentity() && !AICF_ARMPaths.s_bPatrolPending && AICF_ARMPaths.Ready(p.m_Group))
		{
			SCR_ChimeraCharacter first = AICF_ARMPaths.First(p.m_Group);
			bool before = first.GetCompartmentAccessComponent().IsInCompartment();
			first.AICF_ARMPending(true);
			bool admitted = super.Board(p);
			first.AICF_ARMPending(false);
			bool valid = !admitted && before == first.GetCompartmentAccessComponent().IsInCompartment();
			if (!valid) AICF_ARMPaths.s_iFailures++;
			AICF_ARMPaths.s_bPatrolPending = true;
			Print(string.Format("[AICF][FIA_ARMAMENT_PATH] kind=PATROL phase=PENDING passed=%1", valid));
		}
		bool result = super.Board(p);
		if (result)
		{
			bool ready = AICF_ARMPaths.Ready(p.m_Group);
			if (!ready) AICF_ARMPaths.s_iFailures++;
			AICF_ARMPaths.s_iPatrolReady++;
			Print(string.Format("[AICF][FIA_ARMAMENT_PATH] kind=PATROL phase=READY passed=%1", ready));
		}
		return result;
	}
}

modded class AICF_FIAGarrisonCrew
{
	override bool BoardGarrison(AICF_FIAGarrison g)
	{
		if (g && g.GroupIdentity() && !AICF_ARMPaths.s_bGarrisonPending && AICF_ARMPaths.Ready(g.m_Group))
		{
			SCR_ChimeraCharacter first = AICF_ARMPaths.First(g.m_Group);
			bool before = first.GetCompartmentAccessComponent().IsInCompartment();
			first.AICF_ARMPending(true);
			bool admitted = super.BoardGarrison(g);
			first.AICF_ARMPending(false);
			bool valid = !admitted && before == first.GetCompartmentAccessComponent().IsInCompartment();
			if (!valid) AICF_ARMPaths.s_iFailures++;
			AICF_ARMPaths.s_bGarrisonPending = true;
			Print(string.Format("[AICF][FIA_ARMAMENT_PATH] kind=GARRISON phase=PENDING passed=%1", valid));
		}
		bool result = super.BoardGarrison(g);
		if (result)
		{
			bool ready = AICF_ARMPaths.Ready(g.m_Group);
			if (!ready) AICF_ARMPaths.s_iFailures++;
			AICF_ARMPaths.s_iGarrisonReady++;
			for (int i; i < g.m_aEssentialSeats.Count(); i++)
			{
				if (!g.m_aEssentialSeats[i]) AICF_ARMPaths.s_iPassengers++;
			}
			Print(string.Format("[AICF][FIA_ARMAMENT_PATH] kind=GARRISON phase=READY passed=%1 passengers=%2", ready, AICF_ARMPaths.s_iPassengers));
		}
		return result;
	}
}

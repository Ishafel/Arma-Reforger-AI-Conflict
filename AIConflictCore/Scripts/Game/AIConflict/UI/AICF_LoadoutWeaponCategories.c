enum AICF_ELoadoutWeaponCategory
{
	ALL,
	ASSAULT,
	RIFLE,
	MARKSMAN,
	SNIPER,
	MACHINE_GUN,
	SUBMACHINE_GUN,
	SHOTGUN,
	ROCKET_LAUNCHER,
	GRENADE_LAUNCHER,
	OTHER
}

// Только представление каталога: допуск prefab и native slot проверяются раньше.
// Кэш содержит значения, не ссылки на entities временного preview world.
class AICF_LoadoutWeaponCategories
{
	protected ref map<ResourceName, int> m_mCategories = new map<ResourceName, int>();

	void Clear()
	{
		m_mCategories.Clear();
	}

	static void Fill(array<string> names)
	{
		array<string> labels = {
			"{AICF:AICF_UI_AllWeapons}", "{AICF:AICF_UI_AssaultRifles}", "{AICF:AICF_UI_RiflesCarbines}",
			"{AICF:AICF_UI_MarksmanRifles}", "{AICF:AICF_UI_SniperRifles}",
			"{AICF:AICF_UI_MachineGuns}", "{AICF:AICF_UI_SubmachineGuns}",
			"{AICF:AICF_UI_Shotguns}", "{AICF:AICF_UI_RocketLaunchers}",
			"{AICF:AICF_UI_GrenadeLaunchers}", "{AICF:AICF_UI_OtherWeapons}"};
		names.Copy(labels);
	}

	static bool Model(ResourceName prefab, string model)
	{
		string name = FilePath.StripExtension(FilePath.StripPath(prefab));
		return name == model || name.StartsWith(model + "_");
	}

	int Category(ResourceName prefab, ItemPreviewManagerEntity preview)
	{
		int category;
		if (m_mCategories.Find(prefab, category))
			return category;
		if (!preview)
			return AICF_ELoadoutWeaponCategory.OTHER;
		IEntity entity = preview.ResolvePreviewEntityForPrefab(prefab);
		if (!entity || entity.GetWorld() == GetGame().GetWorld())
			return AICF_ELoadoutWeaponCategory.OTHER;
		BaseWeaponComponent weapon = BaseWeaponComponent.Cast(entity.FindComponent(BaseWeaponComponent));
		if (!weapon || weapon.GetWeaponSlotType() != "primary")
			return AICF_ELoadoutWeaponCategory.OTHER;
		category = AICF_ContentProfile.GetActive().GetLoadoutWeaponCategory(prefab);
		if (category == AICF_ELoadoutWeaponCategory.OTHER)
		{
			switch (weapon.GetWeaponType())
			{
				case EWeaponType.WT_RIFLE: category = AICF_ELoadoutWeaponCategory.ASSAULT; break;
				case EWeaponType.WT_SNIPERRIFLE: category = AICF_ELoadoutWeaponCategory.SNIPER; break;
				case EWeaponType.WT_MACHINEGUN: category = AICF_ELoadoutWeaponCategory.MACHINE_GUN; break;
				case EWeaponType.WT_ROCKETLAUNCHER: category = AICF_ELoadoutWeaponCategory.ROCKET_LAUNCHER; break;
				case EWeaponType.WT_GRENADELAUNCHER: category = AICF_ELoadoutWeaponCategory.GRENADE_LAUNCHER; break;
			}
		}
		m_mCategories.Insert(prefab, category);
		return category;
	}

	void Filter(array<string> prefabs, int category, ItemPreviewManagerEntity preview)
	{
		if (category == AICF_ELoadoutWeaponCategory.ALL)
			return;
		array<string> matches = {};
		foreach (ResourceName prefab : prefabs)
			if (Category(prefab, preview) == category)
				matches.Insert(prefab);
		prefabs.Copy(matches);
	}
}

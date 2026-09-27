// Script Diff 1.8.0.13: первый update передаёт selectedWeaponComp в notnull
// helper до проверки оружия в ResolveFireTree. При потере/смене оружия
// отменяем текущую ветку; первый update повторится при следующем запуске.
modded class SCR_AIUpdateTargetAttackData
{
	override ENodeResult EOnTaskSimulate(AIAgent owner, float dt)
	{
		if (m_bFirstSimulate && m_CombatComponent)
		{
			BaseWeaponComponent weapon;
			int muzzleId;
			m_CombatComponent.GetSelectedWeapon(weapon, muzzleId);
			if (!weapon)
				return ENodeResult.FAIL;
		}
		return super.EOnTaskSimulate(owner, dt);
	}
}

// В stock оба узла уже возвращают FAIL при отсутствии volume, но сначала
// вызывают NodeError/Debug.Error. Устаревший вход после отмены подавления
// обрабатываем тем же FAIL без VM exception; живой volume обрабатывает stock.
modded class SCR_AIGetSuppressionVolumeCenterPosition
{
	override ENodeResult EOnTaskSimulate(AIAgent owner, float dt)
	{
		SCR_AISuppressionVolumeBase volume;
		GetVariableIn(SUPPRESSION_VOLUME, volume);
		if (!volume)
			return ENodeResult.FAIL;
		return super.EOnTaskSimulate(owner, dt);
	}
}

modded class SCR_AIGetSuppressionVolumeLine
{
	override ENodeResult EOnTaskSimulate(AIAgent owner, float dt)
	{
		SCR_AISuppressionVolumeBase volume;
		GetVariableIn(SUPPRESSION_VOLUME_PORT, volume);
		if (!volume)
			return ENodeResult.FAIL;
		return super.EOnTaskSimulate(owner, dt);
	}
}

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
		if (!owner)
			return ENodeResult.FAIL;
		IEntity shooter = owner.GetControlledEntity();
		if (!shooter || !AICF_SuppressionInputGuard.CanGenerateLine(volume, shooter.GetOrigin()))
			return ENodeResult.FAIL;
		return super.EOnTaskSimulate(owner, dt);
	}
}

// Script Diff 1.8.0.13: box делит на slope = rightDir.x / rightDir.z.
// При совпадении Z стрелка и центра rightDir.x == 0. Stock защищает только
// rightDir.z, поэтому существующий (в том числе BaseTarget) box ещё не безопасен.
class AICF_SuppressionInputGuard
{
	static bool CanGenerateLine(SCR_AISuppressionVolumeBase volume, vector shooterPos)
	{
		if (!volume)
			return false;
		vector centerPos = volume.GetCenterPosition();
		shooterPos[1] = 0;
		centerPos[1] = 0;
		// Нет горизонтального направления; также исключает нулевой distancePerDeg.
		if (!(vector.DistanceXZ(shooterPos, centerPos) > 0.001))
			return false;

		SCR_AISuppressionVolumeBox box = SCR_AISuppressionVolumeBox.Cast(volume);
		if (!box)
			return true;
		if (!(box.m_vBBMax[0] > box.m_vBBMin[0]) || !(box.m_vBBMax[2] > box.m_vBBMin[2]) || !(box.m_vBBMax[1] >= box.m_vBBMin[1]))
			return false;
		vector direction = vector.Direction(shooterPos, centerPos).Normalized();
		// Безразмерный допуск около нулевого slope; другую ось обрабатывает stock.
		return Math.AbsFloat(direction[2]) > 0.000001;
	}
}

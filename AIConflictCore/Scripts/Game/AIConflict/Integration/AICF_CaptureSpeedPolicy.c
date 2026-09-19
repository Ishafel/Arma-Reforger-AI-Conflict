// Общая скорость захвата для stock/RHS: штатная длительность уменьшается втрое.
modded class SCR_CampaignSeizingComponent
{
	protected static const float AICF_CAPTURE_SPEED_MULTIPLIER = 3.0;

	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (!GetGame().InPlayMode() || !Replication.IsServer() || IsProxy())
			return;

		// Масштабируем и надбавки, чтобы stock multiplier для служб/радиосвязей
		// сохранился. Таймер, оспаривание и репликация остаются штатными.
		m_fMaximumSeizingTime /= AICF_CAPTURE_SPEED_MULTIPLIER;
		m_fMinimumSeizingTime /= AICF_CAPTURE_SPEED_MULTIPLIER;
		m_fExtraTimePerService /= AICF_CAPTURE_SPEED_MULTIPLIER;
		m_fExtraTimePerRadioConnection /= AICF_CAPTURE_SPEED_MULTIPLIER;
	}
}

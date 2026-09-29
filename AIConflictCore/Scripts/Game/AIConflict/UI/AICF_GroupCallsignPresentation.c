// Штатные списки групп и HUD получают тот же ключ, что AICF map/command UI.
// Поле переносится на новую entity из slot и доступно позднему JIP.
modded class SCR_AIGroup
{
	[RplProp(onRplName: "AICF_OnCallsignReplicated")]
	protected string m_sAICFCallsign;

	void AICF_SetCallsign(string callsign)
	{
		if (!Replication.IsServer() || callsign == m_sAICFCallsign)
			return;
		m_sAICFCallsign = callsign;
		Replication.BumpMe();
		AICF_OnCallsignReplicated();
	}

	string AICF_GetCallsign()
	{
		return m_sAICFCallsign;
	}

	protected void AICF_OnCallsignReplicated()
	{
		GetOnCustomNameChanged().Invoke(this);
		OnCallsignChanged();
	}

	override string GetCustomName()
	{
		if (!m_sAICFCallsign.IsEmpty())
			return AICF_Localization.Resolve(m_sAICFCallsign);
		return super.GetCustomName();
	}

	override string GetCustomNameWithOriginal()
	{
		if (!m_sAICFCallsign.IsEmpty())
			return AICF_Localization.Resolve(m_sAICFCallsign);
		return super.GetCustomNameWithOriginal();
	}
}

modded class SCR_CallsignGroupComponent
{
	override bool GetCallsignNames(out string company, out string platoon, out string squad, out string character, out string format)
	{
		SCR_AIGroup group = SCR_AIGroup.Cast(GetOwner());
		if (!group || group.AICF_GetCallsign().IsEmpty())
			return super.GetCallsignNames(company, platoon, squad, character, format);
		company = string.Empty;
		platoon = string.Empty;
		squad = AICF_Localization.Resolve(group.AICF_GetCallsign());
		character = string.Empty;
		format = "%3";
		return true;
	}
}

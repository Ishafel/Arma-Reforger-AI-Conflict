// Родительские civilian/renegade configs ссылаются на stock стороны.
// В RHS world переносим эти связи на существующие runtime factions.
modded class SCR_Faction
{
	override void InitFactionRelationships()
	{
		FactionManager manager = GetGame().GetFactionManager();
		if (manager && manager.GetFactionByKey("RHS_USAF") && manager.GetFactionByKey("RHS_AFRF"))
		{
			for (int i; i < m_aFriendlyFactionsIds.Count(); i++)
			{
				string key = m_aFriendlyFactionsIds[i];
				if (manager.GetFactionByKey(key))
					continue;
				if (key == "US")
					m_aFriendlyFactionsIds[i] = "RHS_USAF";
				else if (key == "USSR")
					m_aFriendlyFactionsIds[i] = "RHS_AFRF";
			}
		}
		super.InitFactionRelationships();
	}
}

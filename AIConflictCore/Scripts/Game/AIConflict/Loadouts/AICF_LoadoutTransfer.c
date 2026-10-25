// Character в recipe — исходная модель для воспроизведения комплекта,
// а не ограничение роли получателя. Перед назначением переносим полный
// server-generated inventory на независимый экземпляр целевой роли.
class AICF_LoadoutTransfer
{
	static bool Fits(AICF_LoadoutBinding binding, SCR_CampaignFaction faction, ResourceName target, out string reason)
	{
		reason = "INCOMPATIBLE_TARGET";
		if (!Replication.IsServer() || !binding || !binding.m_Recipe || !faction || target.IsEmpty()) return false;
		if (binding.m_Recipe.m_sCharacter == target) return true;
		AICF_LoadoutRecipe probe = AICF_LoadoutRecipe.Decode(binding.m_Recipe.Encode());
		if (!probe) return false;
		while (!probe.m_aPaths.IsEmpty()) probe.Undo();
		probe.m_sCharacter = target;
		AICF_LoadoutDraft draft = new AICF_LoadoutDraft();
		AICF_LoadoutCatalog catalog = new AICF_LoadoutCatalog(faction);
		string failure;
		if (!draft.Build(probe, catalog, failure) || !AICF_LoadoutInventory.Restore(draft.GetCharacter(), binding.m_sInventory)) return false;
		int cost;
		return AICF_LoadoutInventory.Signature(draft.GetCharacter(), catalog, cost) == binding.m_sSignature;
	}
}

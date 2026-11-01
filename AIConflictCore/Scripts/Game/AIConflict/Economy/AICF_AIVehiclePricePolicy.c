// Только цены каталога управляемых отрядов AICF. Native player catalog не меняется.
class AICF_AIVehiclePricePolicy
{
	static bool IsCombat(SCR_EntityCatalogEntry entry)
	{
		return entry && (entry.HasEditableEntityLabel(EEditableEntityLabel.TRAIT_ARMED) ||
			entry.HasEditableEntityLabel(EEditableEntityLabel.TRAIT_ARMOR) ||
			entry.HasEditableEntityLabel(EEditableEntityLabel.VEHICLE_APC));
	}

	static int Quote(int nativeCost, bool combat)
	{
		if (nativeCost <= 0 || !combat) return nativeCost;
		// Точное округление к ближайшему целому без float и переполнения суммы.
		int reduced = nativeCost / 3;
		if (nativeCost % 3 == 2) reduced++;
		return Math.Max(1, reduced);
	}
}

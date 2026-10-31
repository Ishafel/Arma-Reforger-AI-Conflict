// Pinned 1.8.0.13 GetRandomVariant: one level only, without drawing RNG.
// The snapshot is captured before the native constructor and never recatalogued.
class AICF_AIQuotaVariantSet
{
	protected ResourceName m_Root;
	protected ref array<ResourceName> m_aAllowed = {};

	static AICF_AIQuotaVariantSet Capture(ResourceName prefab)
	{
		if (prefab.IsEmpty()) return null;
		Resource resource = Resource.Load(prefab);
		if (!resource || !resource.IsValid()) return null;
		AICF_AIQuotaVariantSet result = new AICF_AIQuotaVariantSet();
		result.m_Root = prefab;
		IEntityComponentSource source = SCR_BaseContainerTools.FindComponentSource(resource, SCR_EditableEntityComponent);
		SCR_EditableEntityVariantData data;
		if (source) source.Get("m_VariantData", data);
		array<SCR_EditableEntityVariant> variants = {};
		if (!data || data.GetVariants(variants) == 0)
		{
			result.m_aAllowed.Insert(prefab);
			return result;
		}
		if (variants.Count() > 256) return null;
		int validEntries;
		foreach (SCR_EditableEntityVariant variant : variants)
		{
			if (!variant || !variant.m_bEnabled) return null;
			ResourceName candidate = variant.m_sVariantPrefab;
			int weight = variant.m_iRandomizerWeight;
			resource = Resource.Load(candidate);
			if (!resource || !resource.IsValid()) continue;
			if (!SCR_EditableEntityComponentClass.GetEditableEntitySource(resource)) continue;
			// Native IsEmpty counts entries even when their weight is zero.
			validEntries++;
			if (weight < 0) return null;
			if (weight > 0 && !result.m_aAllowed.Contains(candidate)) result.m_aAllowed.Insert(candidate);
		}
		if (validEntries == 0)
		{
			result.m_aAllowed.Insert(prefab);
			return result;
		}
		if (data.m_bRandomizeDefaultVariant)
		{
			if (data.m_iDefaultVariantRandomizerWeight < 0) return null;
			if (data.m_iDefaultVariantRandomizerWeight > 0 && !result.m_aAllowed.Contains(prefab)) result.m_aAllowed.Insert(prefab);
		}
		// Zero-total / unsupported metadata is a pre-factory veto, not a root fallback.
		if (result.m_aAllowed.IsEmpty()) return null;
		return result;
	}

	bool MatchesRoot(ResourceName prefab)
	{
		return !prefab.IsEmpty() && m_Root == prefab && !m_aAllowed.IsEmpty();
	}

	bool Contains(ResourceName prefab)
	{
		return !prefab.IsEmpty() && m_aAllowed.Contains(prefab);
	}
}

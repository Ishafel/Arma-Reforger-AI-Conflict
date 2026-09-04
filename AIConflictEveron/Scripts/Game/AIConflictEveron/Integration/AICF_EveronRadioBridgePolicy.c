// Everon's randomized active-base layouts can leave the reachable US or USSR component with no
// relay contact after its local objectives are captured. Extend exactly one reachable ordinary
// base to the nearest useful relay only after the faction has exhausted every reachable target.
class AICF_EveronRadioBridgeNormalizer : AICF_StockRadioBridgeNormalizer
{
	override protected int NormalizeMapSpecificFrontiers(string trigger)
	{
		int normalizedCount;
		SCR_CampaignFaction blufor = m_Campaign.GetFactionByEnum(SCR_ECampaignFaction.BLUFOR);
		if (NormalizeIsolatedFaction(blufor, trigger))
			normalizedCount++;

		SCR_CampaignFaction opfor = m_Campaign.GetFactionByEnum(SCR_ECampaignFaction.OPFOR);
		if (NormalizeIsolatedFaction(opfor, trigger))
			normalizedCount++;

		return normalizedCount;
	}

	override protected bool NormalizeMapSpecificFrontierAfterCapture(
		SCR_CampaignFaction faction,
		string trigger)
	{
		return NormalizeIsolatedFaction(faction, trigger);
	}

	override protected string GetPolicyKey()
	{
		return "ONE_WAY_OR_ISOLATED_COMPONENT";
	}

	protected bool NormalizeIsolatedFaction(SCR_CampaignFaction faction, string trigger)
	{
		if (!IsManagedFaction(faction) || !faction.GetMainBase())
			return false;

		array<SCR_MilitaryBaseComponent> rawBases = {};
		m_BaseSystem.GetBases(rawBases);
		array<SCR_CampaignMilitaryBaseComponent> reachableOwnedBases = {};
		if (HasReachableStrategicTarget(faction, rawBases, reachableOwnedBases))
			return false;

		SCR_CampaignMilitaryBaseComponent selectedBase;
		SCR_CampaignMilitaryBaseComponent selectedRelay;
		float selectedDistance = float.MAX;
		int selectedBaseIndex = int.MAX;
		int selectedRelayIndex = int.MAX;
		for (int baseIndex = 0; baseIndex < rawBases.Count(); baseIndex++)
		{
			SCR_CampaignMilitaryBaseComponent base = SCR_CampaignMilitaryBaseComponent.Cast(rawBases[baseIndex]);
			if (!base || !reachableOwnedBases.Contains(base) || !base.GetOwner() || base.IsHQ() ||
				base.GetType() == SCR_ECampaignBaseType.RELAY)
			{
				continue;
			}

			for (int relayIndex = 0; relayIndex < rawBases.Count(); relayIndex++)
			{
				SCR_CampaignMilitaryBaseComponent relay = SCR_CampaignMilitaryBaseComponent.Cast(rawBases[relayIndex]);
				if (!IsUsefulIsolationRelay(relay, faction, rawBases))
					continue;
				if (base.CanReachByRadio(relay.GetOwner()))
					continue;

				float distance = vector.DistanceXZ(base.GetOwner().GetOrigin(), relay.GetOwner().GetOrigin());
				float requestedRange = Math.Ceil(distance + RANGE_MARGIN_METERS);
				if (requestedRange <= base.GetRadioRange())
					continue;

				if (distance > selectedDistance ||
					(distance == selectedDistance && baseIndex > selectedBaseIndex) ||
					(distance == selectedDistance && baseIndex == selectedBaseIndex && relayIndex >= selectedRelayIndex))
				{
					continue;
				}

				selectedBase = base;
				selectedRelay = relay;
				selectedDistance = distance;
				selectedBaseIndex = baseIndex;
				selectedRelayIndex = relayIndex;
			}
		}

		if (!selectedBase || !selectedRelay)
			return false;

		float oldRange = selectedBase.GetRadioRange();
		float requiredRange = Math.Ceil(selectedDistance + RANGE_MARGIN_METERS);
		if (!selectedBase.AICF_ExtendRadioRangeForBridge(requiredRange))
			return false;

		RefreshAuthoritativeCoverage();
		string detail = string.Format(
			"trigger=%1 faction=%2 base=%3 relay=%4 reachable_owned=%5",
			trigger,
			faction.GetFactionKey(),
			AICF_Stage1Diagnostics.BaseKey(selectedBase),
			AICF_Stage1Diagnostics.BaseKey(selectedRelay),
			reachableOwnedBases.Count());
		detail += string.Format(
			" old_range_m=%1 new_range_m=%2 distance_m=%3 margin_m=%4",
			oldRange,
			requiredRange,
			Math.Ceil(selectedDistance),
			RANGE_MARGIN_METERS);
		AICF_Stage1Diagnostics.Info("RADIO_COMPONENT_BRIDGE_NORMALIZED", detail);
		return true;
	}

	protected bool HasReachableStrategicTarget(
		SCR_CampaignFaction faction,
		array<SCR_MilitaryBaseComponent> rawBases,
		out array<SCR_CampaignMilitaryBaseComponent> reachableOwnedBases)
	{
		reachableOwnedBases = {};
		SCR_CampaignMilitaryBaseComponent mainBase = faction.GetMainBase();
		if (!mainBase || !mainBase.GetOwner())
			return false;

		array<SCR_CampaignMilitaryBaseComponent> queue = {};
		queue.Insert(mainBase);
		reachableOwnedBases.Insert(mainBase);
		for (int queueIndex = 0; queueIndex < queue.Count(); queueIndex++)
		{
			SCR_CampaignMilitaryBaseComponent source = queue[queueIndex];
			foreach (SCR_MilitaryBaseComponent rawCandidate : rawBases)
			{
				SCR_CampaignMilitaryBaseComponent candidate = SCR_CampaignMilitaryBaseComponent.Cast(rawCandidate);
				if (!candidate || candidate == source || !candidate.GetOwner() || !candidate.IsInitialized() ||
					!source.CanReachByRadio(candidate.GetOwner()))
				{
					continue;
				}

				if (candidate.GetFaction() != faction)
				{
					if (!candidate.IsHQ() && candidate.IsValidTarget(faction))
						return true;
					continue;
				}

				if (reachableOwnedBases.Contains(candidate))
					continue;

				reachableOwnedBases.Insert(candidate);
				queue.Insert(candidate);
			}
		}

		return false;
	}

	protected bool IsUsefulIsolationRelay(
		SCR_CampaignMilitaryBaseComponent relay,
		SCR_CampaignFaction faction,
		array<SCR_MilitaryBaseComponent> rawBases)
	{
		if (!relay || !relay.GetOwner() || !relay.IsInitialized() ||
			relay.GetType() != SCR_ECampaignBaseType.RELAY)
		{
			return false;
		}

		if (relay.GetFaction() != faction)
			return true;

		foreach (SCR_MilitaryBaseComponent rawCandidate : rawBases)
		{
			SCR_CampaignMilitaryBaseComponent candidate = SCR_CampaignMilitaryBaseComponent.Cast(rawCandidate);
			if (!candidate || candidate == relay || !candidate.GetOwner() || !candidate.IsInitialized() ||
				candidate.IsHQ() || candidate.GetFaction() == faction)
			{
				continue;
			}

			if (relay.CanReachByRadio(candidate.GetOwner()))
				return true;
		}

		return false;
	}
}

// Reuse the shared stock lifecycle but compose Everon's map-specific radio policy.
modded class SCR_GameModeCampaign
{
	override protected AICF_StockRadioBridgeNormalizer AICF_CreateRadioBridgeNormalizer()
	{
		return new AICF_EveronRadioBridgeNormalizer();
	}
}

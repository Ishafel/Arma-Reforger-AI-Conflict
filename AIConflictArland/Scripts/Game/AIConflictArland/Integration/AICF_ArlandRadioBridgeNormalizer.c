// Arland's stock radio layout contains intentional high-range relay transmitters. Some ordinary
// bases can receive a relay, but cannot answer it. Normalise only US/USSR-owned, non-HQ bases
// when that one-way frontier becomes relevant to the live match.
modded class SCR_CampaignMilitaryBaseComponent
{
	bool AICF_ArlandExtendRadioRange(float requestedRange)
	{
		if (!Replication.IsServer() || IsProxy() || !m_RadioComponent)
			return false;

		float newRange = Math.Ceil(requestedRange);
		if (newRange <= m_fRadioRange)
			return false;

		bool transceiverUpdated;
		for (int index = 0, count = m_RadioComponent.TransceiversCount(); index < count; index++)
		{
			RelayTransceiver transceiver = RelayTransceiver.Cast(m_RadioComponent.GetTransceiver(index));
			if (!transceiver)
				continue;

			transceiver.SetRange(newRange);
			transceiverUpdated = true;
		}

		if (!transceiverUpdated)
			return false;

		// RecalculateRadioRange() starts from this value, so later stock service updates preserve
		// the Arland correction. m_fRadioRange is an RplProp with OnRadioRangeChanged callback.
		m_fRadioRangeDefault = newRange;
		m_fRadioRange = newRange;
		Replication.BumpMe();
		OnRadioRangeChanged();
		return true;
	}
}

class AICF_ArlandRadioBridgeNormalizer
{
	protected static const float RANGE_MARGIN_METERS = 10.0;

	protected SCR_GameModeCampaign m_Campaign;
	protected SCR_MilitaryBaseSystem m_BaseSystem;
	protected bool m_bStarted;

	bool Start(SCR_GameModeCampaign campaign)
	{
		if (m_bStarted)
			return true;

		if (!GetGame().InPlayMode() || !Replication.IsServer() || !campaign || !campaign.IsMaster())
			return false;

		m_BaseSystem = SCR_MilitaryBaseSystem.GetInstance();
		if (!m_BaseSystem)
			return false;

		m_Campaign = campaign;
		m_BaseSystem.GetOnBaseFactionChanged().Insert(OnBaseFactionChanged);
		m_bStarted = true;

		int initialNormalizations = NormalizeAllPlayableOwnedBases("BOOTSTRAP");
		AICF_Stage1Diagnostics.Info(
			"RADIO_BRIDGE_READY",
			string.Format(
				"authority=server_master margin_m=%1 initial_normalized=%2",
				RANGE_MARGIN_METERS,
				initialNormalizations));
		return true;
	}

	void Stop()
	{
		if (!m_bStarted)
			return;

		GetGame().GetCallqueue().Remove(NormalizeAfterFactionChange);
		if (m_BaseSystem)
			m_BaseSystem.GetOnBaseFactionChanged().Remove(OnBaseFactionChanged);

		m_bStarted = false;
		m_BaseSystem = null;
		m_Campaign = null;
	}

	protected void OnBaseFactionChanged(SCR_MilitaryBaseComponent rawBase, Faction newFaction)
	{
		if (!m_bStarted || !Replication.IsServer())
			return;

		SCR_CampaignMilitaryBaseComponent base = SCR_CampaignMilitaryBaseComponent.Cast(rawBase);
		SCR_CampaignFaction campaignFaction = SCR_CampaignFaction.Cast(newFaction);
		if (!base || !IsManagedFaction(campaignFaction))
			return;

		// SCR_CampaignMilitaryBaseComponent invokes the global base event from super.OnFactionChanged()
		// before it applies the new faction radio encryption. Run after that stack has completed.
		GetGame().GetCallqueue().CallLater(NormalizeAfterFactionChange, 0, false, base, campaignFaction);
	}

	protected void NormalizeAfterFactionChange(
		SCR_CampaignMilitaryBaseComponent base,
		SCR_CampaignFaction expectedFaction)
	{
		if (!m_bStarted || !Replication.IsServer() || !base || base.GetFaction() != expectedFaction)
			return;

		NormalizeBase(base, "BASE_OWNER_CHANGED");
	}

	protected int NormalizeAllPlayableOwnedBases(string trigger)
	{
		array<SCR_MilitaryBaseComponent> rawBases = {};
		m_BaseSystem.GetBases(rawBases);

		int normalizedCount;
		foreach (SCR_MilitaryBaseComponent rawBase : rawBases)
		{
			SCR_CampaignMilitaryBaseComponent base = SCR_CampaignMilitaryBaseComponent.Cast(rawBase);
			if (NormalizeBase(base, trigger))
				normalizedCount++;
		}

		return normalizedCount;
	}

	protected bool NormalizeBase(SCR_CampaignMilitaryBaseComponent base, string trigger)
	{
		if (!base || !base.GetOwner() || !base.IsInitialized() || base.IsHQ() ||
			base.GetType() == SCR_ECampaignBaseType.RELAY)
			return false;

		SCR_CampaignFaction ownerFaction = SCR_CampaignFaction.Cast(base.GetFaction());
		if (!IsManagedFaction(ownerFaction))
			return false;

		array<SCR_MilitaryBaseComponent> rawBases = {};
		m_BaseSystem.GetBases(rawBases);

		float oldRange = base.GetRadioRange();
		float requiredRange = oldRange;
		float farthestDistance;
		int frontierCount;
		SCR_CampaignMilitaryBaseComponent farthestRelay;

		foreach (SCR_MilitaryBaseComponent rawRelay : rawBases)
		{
			SCR_CampaignMilitaryBaseComponent relay = SCR_CampaignMilitaryBaseComponent.Cast(rawRelay);
			if (!relay || !relay.GetOwner() || !relay.IsInitialized() ||
				relay.GetType() != SCR_ECampaignBaseType.RELAY)
				continue;

			// CanReachByRadio() is the official directed physical-radio query in 1.7.0.54.
			if (!relay.CanReachByRadio(base.GetOwner()) || base.CanReachByRadio(relay.GetOwner()))
				continue;

			float distance = vector.DistanceXZ(base.GetOwner().GetOrigin(), relay.GetOwner().GetOrigin());
			float candidateRange = Math.Ceil(distance + RANGE_MARGIN_METERS);
			frontierCount++;
			if (candidateRange <= requiredRange)
				continue;

			requiredRange = candidateRange;
			farthestDistance = distance;
			farthestRelay = relay;
		}

		if (!farthestRelay || !base.AICF_ArlandExtendRadioRange(requiredRange))
			return false;

		RefreshAuthoritativeCoverage();
		AICF_Stage1Diagnostics.Info(
			"RADIO_BRIDGE_NORMALIZED",
			string.Format(
				"trigger=%1 faction=%2 base=%3 relay=%4 frontiers=%5 old_range_m=%6 new_range_m=%7 distance_m=%8 margin_m=%9",
				trigger,
				ownerFaction.GetFactionKey(),
				AICF_Stage1Diagnostics.BaseKey(base),
				AICF_Stage1Diagnostics.BaseKey(farthestRelay),
				frontierCount,
				oldRange,
				requiredRange,
				Math.Ceil(farthestDistance),
				RANGE_MARGIN_METERS));
		return true;
	}

	protected bool IsManagedFaction(SCR_CampaignFaction faction)
	{
		if (!faction)
			return false;

		FactionKey stableKey = AICF_ContentProfile.GetActive().GetStableFactionKey(
			faction.GetFactionKey());
		return stableKey == "US" || stableKey == "USSR";
	}

	protected void RefreshAuthoritativeCoverage()
	{
		if (!m_Campaign)
			return;

		SCR_CampaignMilitaryBaseManager baseManager = m_Campaign.GetBaseManager();
		if (!baseManager)
			return;

		SCR_CampaignFaction blufor = m_Campaign.GetFactionByEnum(SCR_ECampaignFaction.BLUFOR);
		if (blufor)
			baseManager.RecalculateRadioCoverageForced(blufor);

		SCR_CampaignFaction opfor = m_Campaign.GetFactionByEnum(SCR_ECampaignFaction.OPFOR);
		if (opfor)
			baseManager.RecalculateRadioCoverageForced(opfor);
	}
}

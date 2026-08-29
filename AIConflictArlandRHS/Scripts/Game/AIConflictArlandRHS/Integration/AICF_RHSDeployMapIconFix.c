// RHS faction rules can expose an ASSUMED_* military identity to the Conflict
// deployment map. Vanilla SCR_MapUISpawnPoint handles only the four base
// identities and consequently requests empty selection/highlight image quads.
// Resolve the two managed RHS sides through the active content profile and keep
// the complete stock spawn-point setup for every other faction.
modded class SCR_MapUISpawnPoint
{
	override void UpdateIcon()
	{
		SCR_MilitarySymbol symbol = new SCR_MilitarySymbol();
		FactionManager factionManager = GetGame().GetFactionManager();
		SCR_Faction faction;
		if (factionManager)
			faction = SCR_Faction.Cast(factionManager.GetFactionByKey(m_sFactionKey));

		if (faction)
		{
			SCR_GroupIdentityCore core = SCR_GroupIdentityCore.Cast(
				SCR_GroupIdentityCore.GetInstance(SCR_GroupIdentityCore));
			if (core && core.GetSymbolRuleSet())
				core.GetSymbolRuleSet().UpdateSymbol(symbol, faction);
		}

		EMilitarySymbolIdentity identity = NormalizeIdentity(symbol.GetIdentity());
		if (faction)
		{
			FactionKey stableFactionKey = AICF_ContentProfile.GetActive().GetStableFactionKey(
				faction.GetFactionKey());
			if (stableFactionKey == "US")
				identity = EMilitarySymbolIdentity.BLUFOR;
			else if (stableFactionKey == "USSR")
				identity = EMilitarySymbolIdentity.OPFOR;
		}
		symbol.SetIdentity(identity);

		string selection = "Unknown_Select";
		string highlight = "Unknown_Focus";
		switch (identity)
		{
			case EMilitarySymbolIdentity.INDFOR:
				selection = "Neutral_Select";
				highlight = "Neutral_Focus";
				break;
			case EMilitarySymbolIdentity.OPFOR:
				selection = "Hostile_Select";
				highlight = "Hostile_Focus";
				break;
			case EMilitarySymbolIdentity.BLUFOR:
				selection = "Friend_Select";
				highlight = "Friend_Focus";
				break;
		}

		m_bVisible = true;
		m_wHighlightImg.LoadImageFromSet(0, m_sImageSetARO, highlight);
		m_wSelectImg.LoadImageFromSet(0, m_sImageSetARO, selection);

		if (SCR_PlayerSpawnPoint.Cast(m_SpawnPoint))
			symbol.SetIcons(EMilitarySymbolIcon.RELAY);
		else
			symbol.SetIcons(EMilitarySymbolIcon.RESPAWN);

		m_wSymbolOverlay.SetColor(GetColorForFaction(m_sFactionKey));
		if (m_wGradient)
			m_wGradient.SetColor(GetColorForFaction(m_sFactionKey));
		if (faction)
			m_MilitarySymbolComponent.Update(symbol);
	}

	protected EMilitarySymbolIdentity NormalizeIdentity(
		EMilitarySymbolIdentity identity)
	{
		switch (identity)
		{
			case EMilitarySymbolIdentity.ASSUMED_BLUFOR:
				return EMilitarySymbolIdentity.BLUFOR;
			case EMilitarySymbolIdentity.ASSUMED_OPFOR:
				return EMilitarySymbolIdentity.OPFOR;
			case EMilitarySymbolIdentity.ASSUMED_INDFOR:
				return EMilitarySymbolIdentity.INDFOR;
			case EMilitarySymbolIdentity.CIVILIAN:
			case EMilitarySymbolIdentity.ASSUMED_UNKNOWN:
			case EMilitarySymbolIdentity.ASSUMED_CIVILIAN:
				return EMilitarySymbolIdentity.UNKNOWN;
		}

		return identity;
	}
}

// Vanilla treats the first underscore-delimited token in the base image name as
// the complete faction key. RHS keys contain an underscore (RHS_USAF/RHS_AFRF),
// so both the side and the remaining base-type tokens become misaligned. Parse
// the exact runtime key first and retain the stock base/spawn behavior.
modded class SCR_CampaignMapUIBase
{
	override void SetImage(string image, string imageset)
	{
		if (!m_wImage || !m_wBaseIcon)
			return;

		SCR_GameModeCampaign campaign = SCR_GameModeCampaign.GetInstance();
		if (!campaign)
			return;

		TStringArray imageParts = {};
		string runtimePrefix = m_sFactionKey + "_";
		if (!m_sFactionKey.IsEmpty() && image.StartsWith(runtimePrefix))
		{
			string imageSuffix = image;
			imageSuffix.Replace(runtimePrefix, "");
			imageSuffix.Split("_", imageParts, true);
		}
		else
		{
			TStringArray rawParts = {};
			image.Split("_", rawParts, true);
			for (int rawIndex = 1; rawIndex < rawParts.Count(); rawIndex++)
				imageParts.Insert(rawParts[rawIndex]);
		}

		string baseType = "Base";
		string baseSize;
		if (!imageParts.IsEmpty())
			baseType = imageParts[0];
		if (imageParts.Count() > 1)
			baseSize = imageParts[1];

		Color factionColor = GetColorForFaction(m_sFactionKey);
		m_wBaseIcon.SetColor(factionColor);
		if (m_wGradient)
			m_wGradient.SetColor(factionColor);

		SCR_MilitarySymbolUIComponent symbolUI = SCR_MilitarySymbolUIComponent.Cast(
			m_wBaseIcon.FindHandler(SCR_MilitarySymbolUIComponent));
		if (!symbolUI)
			return;

		SCR_MilitarySymbol baseIcon = new SCR_MilitarySymbol();
		EMilitarySymbolIdentity identity = ResolveBaseIdentity(campaign);
		baseIcon.SetIdentity(identity);
		string selection = "Unknown_Select";
		string highlight = "Unknown_Installation_Focus_Land";
		Color dialogColor = Color.FromRGBA(249, 210, 103, 255);
		switch (identity)
		{
			case EMilitarySymbolIdentity.INDFOR:
				selection = "Neutral_Select";
				highlight = "Neutral_Installation_Focus_Land";
				dialogColor = Color.FromRGBA(0, 177, 79, 255);
				break;
			case EMilitarySymbolIdentity.OPFOR:
				selection = "Hostile_Select";
				highlight = "Hostile_Installation_Focus_Land";
				dialogColor = Color.FromRGBA(238, 49, 47, 255);
				break;
			case EMilitarySymbolIdentity.BLUFOR:
				selection = "Friend_Select";
				highlight = "Friend_Installation_Focus_Land";
				dialogColor = Color.FromRGBA(31, 195, 243, 255);
				break;
		}

		m_w_NameDialog = TextWidget.Cast(m_wRoot.FindAnyWidget("m_w_NameDialog"));
		if (m_w_NameDialog)
			m_w_NameDialog.SetColor(dialogColor);
		if (identity == EMilitarySymbolIdentity.INDFOR ||
			identity == EMilitarySymbolIdentity.UNKNOWN)
		{
			m_wAntennaImg.SetVisible(false);
		}

		SCR_SpawnPoint spawnPoint;
		switch (baseType)
		{
			case "Relay":
				baseIcon.SetIcons(EMilitarySymbolIcon.RELAY);
				m_wImageOverlay.SetWidthOverride(m_iDefRelaySize);
				m_wImageOverlay.SetHeightOverride(m_iDefRelaySize);
				m_wAntennaImg.SetVisible(false);
				break;
			case "Mobile":
				baseIcon.SetIcons(
					EMilitarySymbolIcon.MOBILEHQ | EMilitarySymbolIcon.RELAY);
				if (m_MobileAssembly)
					spawnPoint = SCR_SpawnPoint.Cast(m_MobileAssembly.GetOwner());
				m_wAntennaImg.SetVisible(false);
				m_wImageOverlay.SetWidthOverride(m_iDefRelaySize);
				m_wImageOverlay.SetHeightOverride(m_iDefRelaySize);
				break;
			case "SourceBase":
				if (m_Base)
					spawnPoint = m_Base.GetSpawnPoint();
				TStringArray highlightParts = {};
				highlight.Split("_", highlightParts, true);
				if (highlightParts.Count() >= 2)
					highlight = string.Format("%1_%2", highlightParts[0], highlightParts[1]);
				baseIcon.SetIcons(EMilitarySymbolIcon.SUPPLY);
				m_wAntennaImg.SetVisible(false);
				break;
			default:
				if (m_Base)
				{
					spawnPoint = m_Base.GetSpawnPoint();
					if (identity != EMilitarySymbolIdentity.UNKNOWN &&
						m_Base.GetType() != SCR_ECampaignBaseType.SOURCE_BASE &&
						!m_Base.IsHQRadioTrafficPossible(
							m_Base.GetCampaignFaction(),
							SCR_ERadioCoverageStatus.BOTH_WAYS) &&
						m_Base.GetFaction() == m_PlayerFaction)
					{
						m_wAntennaImg.SetVisible(true);
					}
					else
					{
						m_wAntennaImg.SetVisible(false);
					}
				}
				if (baseSize == "Small")
				{
					m_wImageOverlay.SetWidthOverride(38);
					m_wImageOverlay.SetHeightOverride(38);
				}
				break;
		}

		baseIcon.SetDimension(EMilitarySymbolDimension.INSTALLATION);
		symbolUI.Update(baseIcon);
		m_wHighlightImg.LoadImageFromSet(0, m_sImageSetARO, highlight);
		m_wSelectImg.LoadImageFromSet(0, m_sImageSetARO, selection);

		if (!spawnPoint)
			return;
		m_SpawnPoint = spawnPoint;
		CheckIfCanRespawn();
		if (m_bCanRespawn)
		{
			if (baseType == "Mobile")
				baseIcon.SetIcons(EMilitarySymbolIcon.RESPAWN |
					EMilitarySymbolIcon.MOBILEHQ);
			else
				baseIcon.SetIcons(EMilitarySymbolIcon.RESPAWN);
		}
		symbolUI.Update(baseIcon);
	}

	protected EMilitarySymbolIdentity ResolveBaseIdentity(
		SCR_GameModeCampaign campaign)
	{
		FactionKey stableFactionKey = AICF_ContentProfile.GetActive().GetStableFactionKey(
			m_sFactionKey);
		if (stableFactionKey == "US")
			return EMilitarySymbolIdentity.BLUFOR;
		if (stableFactionKey == "USSR")
			return EMilitarySymbolIdentity.OPFOR;
		if (m_sFactionKey == campaign.GetFactionKeyByEnum(SCR_ECampaignFaction.INDFOR))
			return EMilitarySymbolIdentity.INDFOR;
		if (m_sFactionKey == campaign.GetFactionKeyByEnum(SCR_ECampaignFaction.OPFOR))
			return EMilitarySymbolIdentity.OPFOR;
		if (m_sFactionKey == campaign.GetFactionKeyByEnum(SCR_ECampaignFaction.BLUFOR))
			return EMilitarySymbolIdentity.BLUFOR;
		return EMilitarySymbolIdentity.UNKNOWN;
	}
}

// Только isolated stage: отказ completion на 45 секунд у первых малых казарм
// каждой стороны. Поиск, оплата, физический подход, инструмент и completion
// остаются production; после снятия отказа обязательна native clearance.
class AICF_BuilderCompletionCase
{
	ref AICF_ConstructionOrder m_Order;
	ref AICF_BaseBuilder m_Builder;
	int m_iStarted;
	int m_iLastCheck;
	int m_iDenied;
	int m_iSamples;
	int m_iResumed;
	int m_iGeneration;
	EntityID m_GroupId;
	EntityID m_CharacterId;
	float m_fProgress;
	bool m_bDone;
}

class AICF_BuilderCompletionProbe
{
	static ref array<ref AICF_BuilderCompletionCase> s_aCases = {};
	static ref array<string> s_aFailures = {};

	static bool Enabled()
	{
		string value;
		return System.GetCLIParam("aicfBuilderCompletionProbe", value) && value == "1";
	}

	static void Fail(string rule)
	{
		if (s_aFailures.Contains(rule))
			return;
		s_aFailures.Insert(rule);
		Print("[AICF][BUILDER_COMPLETION_PROBE_FAILURE] rule=" + rule);
	}

	static AICF_BuilderCompletionCase Find(AICF_ConstructionOrder receipt)
	{
		foreach (AICF_BuilderCompletionCase test : s_aCases)
		{
			if (test.m_Order == receipt)
				return test;
		}
		return null;
	}

	static void Register(AICF_BaseBuilder builder)
	{
		if (!Enabled() || !builder.m_Target)
			return;
		AICF_ConstructionOrder receipt = builder.m_Target.m_AICFConstructionReceipt;
		if (!receipt || !receipt.m_bAccepted || receipt.m_eType != AICF_EConstructionType.SMALL_BARRACKS)
			return;
		foreach (AICF_BuilderCompletionCase existing : s_aCases)
		{
			if (existing.m_Order.m_sFaction == receipt.m_sFaction)
				return;
		}
		AICF_BuilderCompletionCase test = new AICF_BuilderCompletionCase();
		test.m_Order = receipt;
		test.m_Builder = builder;
		test.m_iGeneration = builder.m_iGeneration;
		test.m_GroupId = builder.m_GroupId;
		test.m_CharacterId = builder.m_CharacterId;
		s_aCases.Insert(test);
	}

	static bool Block(AICF_ConstructionOrder check)
	{
		if (!Enabled() || !check || check.m_iQueryPhase != 5)
			return false;
		AICF_BuilderCompletionCase test;
		foreach (AICF_BuilderCompletionCase candidate : s_aCases)
		{
			if (candidate.m_Order.m_sToken == check.m_sToken)
				test = candidate;
		}
		if (!test)
			return false;
		SCR_CampaignBuildingLayoutComponent layout = test.m_Order.m_Composition.GetCompositionLayout();
		int now = System.GetTickCount();
		if (test.m_iStarted == 0)
		{
			test.m_iStarted = now;
			test.m_fProgress = layout.GetCurrentBuildValue();
		}
		if (now - test.m_iStarted >= 45000)
			return false;
		if (test.m_iLastCheck != 0 && now - test.m_iLastCheck < 15000)
			Fail(test.m_Order.m_sFaction + "_RETRY_TOO_EARLY");
		test.m_iLastCheck = now;
		test.m_iDenied++;
		check.m_sReason = "TEST_COMPLETION_OBSTRUCTION";
		Print(string.Format("[AICF][BUILDER_COMPLETION_PROBE_DENIED] faction=%1 token=%2 elapsed_ms=%3 progress=%4",
			test.m_Order.m_sFaction, test.m_Order.m_sToken, now - test.m_iStarted, layout.GetCurrentBuildValue()));
		return true;
	}

	static void Observe()
	{
		foreach (AICF_BuilderCompletionCase test : s_aCases)
		{
			if (test.m_bDone || test.m_iStarted == 0)
				continue;
			AICF_BaseBuilder builder = test.m_Builder;
			string side = test.m_Order.m_sFaction;
			if (builder.m_iGeneration != test.m_iGeneration || builder.m_GroupId != test.m_GroupId || builder.m_CharacterId != test.m_CharacterId)
				Fail(side + "_WORKER_REPLACED");
			SCR_CampaignBuildingCompositionComponent composition = test.m_Order.m_Composition;
			if (!composition || !composition.GetOwner() || composition.GetOwner().GetID() != test.m_Order.m_LayoutId)
			{
				Fail(side + "_LAYOUT_REPLACED");
				continue;
			}
			if (composition.IsCompositionSpawned())
			{
				if (!AICF_ConstructionMetadata.HasOnlineService(composition.GetOwner(), test.m_Order.m_eType))
					continue;
				test.m_bDone = true;
				if (test.m_iDenied < 3 || test.m_iSamples < 20 || test.m_iResumed < 1)
					Fail(side + "_WAIT_NOT_PROVEN");
				if (builder.m_iBlockedUntilMs != 0)
					Fail(side + "_WAIT_NOT_CLEARED");
				Print(string.Format("[AICF][BUILDER_COMPLETION_PROBE_CASE] faction=%1 token=%2 done=1 denied=%3 paused_samples=%4 resumed=%5 service_online=1",
					side, test.m_Order.m_sToken, test.m_iDenied, test.m_iSamples, test.m_iResumed));
				continue;
			}
			if (System.GetTickCount() - test.m_iStarted >= 45000)
				continue;
			test.m_iSamples++;
			SCR_CampaignBuildingLayoutComponent layout = composition.GetCompositionLayout();
			if (!layout || layout.GetCurrentBuildValue() != test.m_fProgress)
				Fail(side + "_PROGRESS_WHILE_BLOCKED");
			if (builder.m_iBlockedUntilMs == 0 || builder.m_bToolActive || builder.m_UsedTool || builder.m_ToolController)
				Fail(side + "_TOOL_NOT_STOPPED");
			CharacterControllerComponent controller = CharacterControllerComponent.Cast(builder.m_Character.FindComponent(CharacterControllerComponent));
			if (System.GetTickCount() - test.m_iStarted >= 3000 && controller.IsUsingItem())
				Fail(side + "_ITEM_ANIMATION_NOT_STOPPED");
		}
	}
}

modded class AICF_ConstructionSiteSearch
{
	override bool LiveClear(AICF_ConstructionOrder order, IEntity excludedRoot = null)
	{
		if (AICF_BuilderCompletionProbe.Block(order))
			return false;
		return super.LiveClear(order, excludedRoot);
	}
}

modded class AICF_BaseBuilderService
{
	protected int m_iAICFCompletionProbeStarted;
	protected bool m_bAICFCompletionProbeFinished;

	override protected void Build(AICF_BaseBuilder builder, int now)
	{
		AICF_BuilderCompletionProbe.Register(builder);
		super.Build(builder, now);
	}

	override protected void Log(AICF_BaseBuilder builder, string eventName, string details = "")
	{
		super.Log(builder, eventName, details);
		if (eventName != "BUILDER_WORK_RESUMED" || !builder.m_Target)
			return;
		AICF_BuilderCompletionCase test = AICF_BuilderCompletionProbe.Find(builder.m_Target.m_AICFConstructionReceipt);
		if (test)
			test.m_iResumed++;
	}

	override void Update()
	{
		super.Update();
		if (!AICF_BuilderCompletionProbe.Enabled() || m_bAICFCompletionProbeFinished)
			return;
		int now = System.GetTickCount();
		if (m_iAICFCompletionProbeStarted == 0)
			m_iAICFCompletionProbeStarted = now;
		AICF_BuilderCompletionProbe.Observe();
		int completed;
		foreach (AICF_BuilderCompletionCase test : AICF_BuilderCompletionProbe.s_aCases)
		{
			if (test.m_bDone)
				completed++;
		}
		if (completed != 2 && now - m_iAICFCompletionProbeStarted < 420000)
			return;
		m_bAICFCompletionProbeFinished = true;
		if (completed != 2)
			AICF_BuilderCompletionProbe.Fail("BOTH_FACTIONS_NOT_COMPLETED");
		Print(string.Format("[AICF][BUILDER_COMPLETION_PROBE_FINISHED] completed=%1 failures=%2", completed, AICF_BuilderCompletionProbe.s_aFailures.Count()));
		GetGame().RequestClose();
	}
}

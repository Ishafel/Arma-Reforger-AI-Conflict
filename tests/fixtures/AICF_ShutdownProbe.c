// Только isolated stage. Mode idle исключает loadout fixture и preview world.
class AICF_ShutdownProbe
{
	static int s_iDrafts;
	static int s_iCreated;
	static int s_iDestroyed;
	// IEntity elements являются weak references; probe не владеет entities/worlds.
	static ref array<IEntity> s_aClearedItems = {};

	static void Snapshot(string phase)
	{
		int retained;
		set<IEntity> seen = new set<IEntity>();
		foreach (IEntity item : s_aClearedItems)
			if (item && !seen.Contains(item)) { seen.Insert(item); retained++; }
		Print(string.Format("[AICF][SHUTDOWN_RETAINED] phase=%1 sampled=%2 alive=%3", phase, s_aClearedItems.Count(), retained));
		Print(string.Format("[AICF][SHUTDOWN_OBJECTS] phase=%1 drafts=%2 created=%3 destroyed=%4", phase, s_iDrafts, s_iCreated, s_iDestroyed));
		MemoryStatsSnapshot snapshot = new MemoryStatsSnapshot();
		for (int i = 0; i < MemoryStatsSnapshot.GetStatsCount(); i++)
			Print(string.Format("[AICF][SHUTDOWN_MEMORY] phase=%1 name=%2 value=%3", phase, MemoryStatsSnapshot.GetStatName(i), snapshot.GetStatValue(i)));
	}

	static void Batch(int batch)
	{
		int failures;
		for (int iteration = 0; iteration < 4; iteration++)
		{
			foreach (string side : {"USSR", "US"})
			{
				SCR_CampaignFaction faction = SCR_CampaignFaction.Cast(GetGame().GetFactionManager().GetFactionByKey(AICF_ContentProfile.GetActive().GetRuntimeFactionKey(side)));
				AICF_LoadoutCatalog catalog = new AICF_LoadoutCatalog(faction);
				AICF_LoadoutRecipe recipe = new AICF_LoadoutRecipe();
				AICF_GroupSpawner spawner = new AICF_GroupSpawner();
				string role, reason, snapshot;
				int cost;
				recipe.m_sCharacter = spawner.ResolveRecruitPrefab(faction, 0, role);
				recipe.m_sCharacter = AICF_ContentProfile.GetActive().GetPersonalLoadoutSource(faction, recipe.m_sCharacter);
				recipe.m_sProfile = AICF_ContentProfile.GetActive().GetProfileKey();
				recipe.m_sFaction = side;
				recipe.m_sName = "Shutdown batch";
				AICF_LoadoutDraft draft;
				string equipment;
				if (System.GetCLIParam("aicfShutdownEquipmentBatch", equipment) && equipment == "1")
					draft = new AICF_EquipmentDraft();
				else
					draft = new AICF_LoadoutDraft();
				bool passed = draft.Build(recipe, catalog, reason);
				if (passed)
				{
					string before = AICF_LoadoutInventory.Signature(draft.GetCharacter(), catalog, cost);
					passed = AICF_LoadoutInventory.Capture(draft.GetCharacter(), snapshot);
					passed = passed && AICF_LoadoutInventory.Restore(draft.GetCharacter(), snapshot);
					passed = passed && before == AICF_LoadoutInventory.Signature(draft.GetCharacter(), catalog, cost);
					AICF_LoadoutNavigation tree = new AICF_LoadoutNavigation();
					tree.Build(draft.GetCharacter(), catalog);
					tree.Clear();
				}
				IEntity character = draft.GetCharacter();
				ItemPreviewManagerEntity preview = draft.GetPreview();
				draft.Clear();
				passed = passed && !draft.GetCharacter() && !draft.GetPreview() && !character && !preview;
				if (equipment == "1")
				{
					// Draft-only batches never execute EquipmentPlan's native muzzle
					// reads. Include that owner and its destructor in every operation.
					AICF_EquipmentPlan plan = new AICF_EquipmentPlan();
					bool built = plan.Build(recipe.m_sCharacter, faction, null);
					IEntity model;
					if (plan.m_Draft) model = plan.m_Draft.GetCharacter();
					plan = null;
					bool cleared = built && !model;
					passed = passed && cleared;
					Print(string.Format("[AICF][SHUTDOWN_PLAN_CLEAR] batch=%1 iteration=%2 side=%3 passed=%4", batch, iteration, side, cleared));
				}
				if (!passed) failures++;
				Print(string.Format("[AICF][SHUTDOWN_OPERATION] batch=%1 iteration=%2 side=%3 passed=%4 reason=%5", batch, iteration, side, passed, reason));
			}
		}
		Print(string.Format("[AICF][SHUTDOWN_BATCH] batch=%1 operations=8 failures=%2", batch, failures));
	}
}

modded class AICF_LoadoutDraft
{
	void AICF_LoadoutDraft()
	{
		AICF_ShutdownProbe.s_iDrafts++;
		AICF_ShutdownProbe.s_iCreated++;
	}

	void ~AICF_LoadoutDraft()
	{
		AICF_ShutdownProbe.s_iDrafts--;
		AICF_ShutdownProbe.s_iDestroyed++;
	}
}

modded class AICF_EquipmentDraft
{
	override void Clear()
	{
		array<IEntity> items = {};
		if (m_Character)
		{
			array<IEntity> raw = {};
			InventoryStorageManagerComponent manager = AICF_EquipmentInventory.Manager(m_Character);
			if (manager) manager.GetItems(raw);
			AICF_EquipmentInventory.UniqueItems(raw, items);
		}
		super.Clear();
		AICF_ShutdownProbe.s_aClearedItems.InsertAll(items);
		int alive;
		foreach (IEntity item : items) if (item) alive++;
		if (!items.IsEmpty()) Print(string.Format("[AICF][SHUTDOWN_EQUIPMENT_CLEAR] items=%1 alive=%2", items.Count(), alive));
	}
}

modded class AICF_MatchController
{
	protected int m_iAICFShutdownAt;
	protected int m_iAICFShutdownStep;
	protected bool m_bAICFShutdownSeen;
	protected bool m_bAICFShutdownClosing;
	protected int m_iAICFDisconnectedAt;

	override protected void Stop(bool cleanupEntities)
	{
		AICF_ShutdownProbe.Snapshot("before_stop");
		super.Stop(cleanupEntities);
		AICF_ShutdownProbe.Snapshot("after_stop");
	}

	override protected void Update()
	{
		super.Update();
		string mode;
		if (!m_bRosterReady || m_bAICFShutdownClosing || !System.GetCLIParam("aicfShutdownMode", mode)) return;
		if (m_iAICFShutdownAt == 0)
		{
			m_iAICFShutdownAt = System.GetTickCount();
			Print("[AICF][SHUTDOWN_START] mode=" + mode);
			AICF_ShutdownProbe.Snapshot("ready");
		}
		int players = GetGame().GetPlayerManager().GetPlayerCount();
		if (players > 0) m_bAICFShutdownSeen = true;
		if (players > 0) m_iAICFDisconnectedAt = 0;
		if (m_bAICFShutdownSeen && players == 0 && m_iAICFDisconnectedAt == 0)
			m_iAICFDisconnectedAt = System.GetTickCount();
		int elapsed = System.GetTickCount(m_iAICFShutdownAt);
		// Fixed schedule: idle baseline, 3 x 8 operations, cleanup and 10s rest.
		if (m_iAICFShutdownStep < 4 && elapsed >= 10000 + m_iAICFShutdownStep * 10000)
		{
			AICF_ShutdownProbe.Snapshot("before_" + m_iAICFShutdownStep.ToString());
			if (m_iAICFShutdownStep > 0 && mode == "batches") AICF_ShutdownProbe.Batch(m_iAICFShutdownStep);
			AICF_ShutdownProbe.Snapshot("cleanup_" + m_iAICFShutdownStep.ToString());
			m_iAICFShutdownStep++;
		}
		string standalone;
		bool close = System.GetCLIParam("aicfShutdownStandalone", standalone) && elapsed >= 55000;
		string grace;
		int graceMs;
		if (System.GetCLIParam("aicfShutdownGraceMs", grace)) graceMs = Math.Max(0, grace.ToInt());
		bool disconnected = m_iAICFDisconnectedAt > 0 && System.GetTickCount(m_iAICFDisconnectedAt) >= graceMs;
		if ((disconnected && elapsed >= 55000) || close || elapsed >= 180000)
		{
			m_bAICFShutdownClosing = true;
			AICF_ShutdownProbe.Snapshot("rest");
			Print(string.Format("[AICF][SHUTDOWN_DONE] mode=%1 steps=%2 connected=%3 elapsed_ms=%4", mode, m_iAICFShutdownStep, m_bAICFShutdownSeen, elapsed));
			GetGame().RequestClose();
		}
	}
}

modded class AICF_EquipmentService
{
	override void Update(AICF_FactionState us, SCR_CampaignFaction usFaction, AICF_FactionState ussr,
		SCR_CampaignFaction ussrFaction, AICF_VehicleCoordinator vehicles, bool graphReady)
	{
		string disabled;
		if (System.GetCLIParam("aicfShutdownNoEquipment", disabled) && disabled == "1") return;
		super.Update(us, usFaction, ussr, ussrFaction, vehicles, graphReady);
	}
}

modded class SCR_GameModeCampaign
{
	override void OnGameStart()
	{
		super.OnGameStart();
		string mode;
		if (!Replication.IsServer() && System.GetCLIParam("aicfShutdownMode", mode))
			GetGame().GetCallqueue().CallLater(AICF_ShutdownClientClose, 45000, false);
	}

	void AICF_ShutdownClientClose()
	{
		Print("[AICF][SHUTDOWN_CLIENT_DONE]");
		GetGame().RequestClose();
	}

	override void OnGameEnd()
	{
		GetGame().GetCallqueue().Remove(AICF_ShutdownClientClose);
		super.OnGameEnd();
	}
}

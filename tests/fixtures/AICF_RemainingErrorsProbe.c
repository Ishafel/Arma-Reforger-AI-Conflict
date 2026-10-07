// Только изолированная terminal fixture. Не добавлять в production Scripts.
// Наблюдает identity/parent/physics; не перемещает и не удаляет предметы.
class AICF_RemainingWeaponTrace
{
	IEntity m_Entity;
	EntityID m_Id;
	EntityID m_FirstCharacter;
	IEntity m_Parent;
	int m_iNextSample;
}

class AICF_RemainingErrorsProbe
{
	static ref array<SCR_ChimeraCharacter> s_Characters = {};
	static ref array<ref AICF_RemainingWeaponTrace> s_Weapons = {};
	static int s_iStart;
	static int s_iDurationMs = 260000;
	static bool s_bStopped;

	static void Register(SCR_ChimeraCharacter character)
	{
		if (Replication.IsServer() && character.GetWorld() == GetGame().GetWorld())
			s_Characters.Insert(character);
	}

	static void Track(IEntity entity, EntityID character)
	{
		if (!entity) return;
		foreach (AICF_RemainingWeaponTrace known : s_Weapons)
		{
			if (known.m_Entity == entity && known.m_Id == entity.GetID()) return;
		}
		AICF_RemainingWeaponTrace trace = new AICF_RemainingWeaponTrace();
		trace.m_Entity = entity;
		trace.m_Id = entity.GetID();
		trace.m_FirstCharacter = character;
		s_Weapons.Insert(trace);
		string model;
		if (entity.GetVObject()) model = entity.GetVObject().GetResourceName();
		Print(string.Format("[AICF][WEAPON_TRACE_REGISTER] entity=%1 character=%2 prefab=%3 model=%4 position=%5", trace.m_Id, character, SCR_ResourceNameUtils.GetPrefabName(entity), model, entity.GetOrigin()));
	}

	static void ResourceInfo(ResourceName name, bool category = false)
	{
		Resource resource = Resource.Load(name);
		if (!resource || !resource.IsValid())
		{
			Print("[AICF][RESOURCE_PROBE] missing=" + name);
			return;
		}
		BaseContainer container = resource.GetResource().ToBaseContainer();
		if (!container) return;
		array<string> addons = {};
		container.GetSourceAddons(addons);
		Print(string.Format("[AICF][RESOURCE_PROBE] resource=%1 class=%2 sources=%3", name, container.GetClassName(), SCR_StringHelper.Join(",", addons)));
		if (category)
		{
			SCR_FilterCategory instance = SCR_ConfigHelperT<SCR_FilterCategory>.GetConfigObject(name);
			int count;
			bool linked = instance != null;
			if (instance)
			{
				foreach (SCR_FilterEntry entry : instance.GetFilters())
				{
					count++;
					if (entry.GetCategory() != instance) linked = false;
				}
			}
			Print(string.Format("[AICF][FILTER_ROOT_PROBE] resource=%1 created=%2 count=%3 linked=%4", name, instance != null, count, linked));
		}
	}

	static void Start()
	{
		if (s_iStart || s_bStopped || !Replication.IsServer()) return;
		s_iStart = System.GetTickCount();
		string seconds;
		if (System.GetCLIParam("aicfRemainingProbeSeconds", seconds))
			s_iDurationMs = Math.Clamp(seconds.ToInt(), 30, 260) * 1000;
		// Этот opt-in воспроизводит stock root factory error; обычная probe
		// только читает provenance и не добавляет ошибок меню в server log.
		bool menuFactories = System.IsCLIParam("aicfRemainingProbeMenuConfigs");
		ResourceInfo("{A557E41062372854}Configs/ContentBrowser/Filters/category_type.conf", menuFactories);
		ResourceInfo("{FB69283C67BD9E67}Configs/ContentBrowser/Filters/category_manw.conf", menuFactories);
		ResourceInfo("{D3BFEE28E7D5B6A1}Configs/ServerBrowser/KickDialogs.conf");
		bool shutdown = SCR_ConfigurableDialogUi.IsPresetValid("{D3BFEE28E7D5B6A1}Configs/ServerBrowser/KickDialogs.conf", "REPLICATION_SHUTDOWN");
		bool fallback = SCR_ConfigurableDialogUi.IsPresetValid("{D3BFEE28E7D5B6A1}Configs/ServerBrowser/KickDialogs.conf", "REPLICATION");
		Print(string.Format("[AICF][KICK_PRESET_PROBE] shutdown=%1 replication=%2", shutdown, fallback));
		ResourceInfo("{B4C861A6DA2F5E05}AI/AIAgents/SCR_ChimeraAIAgentFull.et");
		ResourceInfo("{C8FAB83670BCF7C3}Prefabs/Structures/Cultural/Calvaries/CalvaryLarge_01.et");
		Resource reactionResource = BaseContainerTools.CreateContainer("SCR_AIDangerReaction_UnsafeArea");
		SCR_AIDangerReaction reaction;
		if (reactionResource && reactionResource.IsValid())
			reaction = SCR_AIDangerReaction.Cast(BaseContainerTools.CreateInstanceFromContainer(reactionResource.GetResource().ToBaseContainer()));
		Print(string.Format("[AICF][DANGER_CONFIG_PROBE] created=%1", reaction != null));
		Print(string.Format("[AICF][REMAINING_PROBE_STARTED] duration_ms=%1 sampling_ms=250", s_iDurationMs));
		GetGame().GetCallqueue().CallLater(Tick, 250, true);
	}

	static void Tick()
	{
		int now = System.GetTickCount();
		foreach (SCR_ChimeraCharacter character : s_Characters)
		{
			if (!character) continue;
			BaseWeaponManagerComponent manager = BaseWeaponManagerComponent.Cast(character.FindComponent(BaseWeaponManagerComponent));
			if (!manager) continue;
			array<WeaponSlotComponent> slots = {};
			manager.GetWeaponsSlots(slots);
			foreach (WeaponSlotComponent slot : slots) Track(slot.GetWeaponEntity(), character.GetID());
		}
		foreach (AICF_RemainingWeaponTrace trace : s_Weapons)
		{
			IEntity entity = trace.m_Entity;
			if (!entity || entity.GetID() != trace.m_Id) continue;
			vector position = entity.GetOrigin();
			float surface = GetGame().GetWorld().GetSurfaceY(position[0], position[2]);
			IEntity parent = entity.GetParent();
			if (parent == trace.m_Parent && (position[1] >= surface - 5 || now < trace.m_iNextSample)) continue;
			trace.m_Parent = parent;
			trace.m_iNextSample = now + 1000;
			vector velocity;
			Physics physics = entity.GetPhysics();
			if (physics) velocity = physics.GetVelocity();
			Print(string.Format("[AICF][WEAPON_TRACE_SAMPLE] entity=%1 character=%2 parent=%3 position=%4 surface_y=%5 velocity=%6", trace.m_Id, trace.m_FirstCharacter, parent, position, surface, velocity));
		}
		if (now - s_iStart < s_iDurationMs) return;
		Stop();
		Print(string.Format("[AICF][REMAINING_PROBE_FINISHED] duration_ms=%1", s_iDurationMs));
		GetGame().RequestClose();
	}

	static void Stop()
	{
		s_bStopped = true;
		GetGame().GetCallqueue().Remove(Tick);
		s_Characters.Clear();
		s_Weapons.Clear();
	}
}

modded class SCR_ChimeraCharacter
{
	override void EOnInit(IEntity owner)
	{
		super.EOnInit(owner);
		AICF_RemainingErrorsProbe.Register(this);
	}
}

modded class AICF_FIAPatrolService
{
	override void Update(bool graphReady)
	{
		super.Update(graphReady);
		AICF_RemainingErrorsProbe.Start();
	}

	override void Stop()
	{
		AICF_RemainingErrorsProbe.Stop();
		super.Stop();
	}
}

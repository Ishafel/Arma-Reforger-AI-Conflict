// Permission belongs to one actual AI inventory-removal event, never to the
// diagnostic loose-weapon observer. No actor/vehicle/physics mutation occurs.
class AICF_DroppedWeaponFallRecord
{
	IEntity m_Item;
	EntityID m_Id;
	RplComponent m_Rpl;
	RplId m_RplId;
	string m_sRpl;
	InventoryItemComponent m_Inventory;
	BaseWeaponComponent m_Weapon;
	ChimeraCharacter m_Source;
	EntityID m_SourceId;
	RplComponent m_SourceRpl;
	string m_sSourceRpl;
	CharacterControllerComponent m_Controller;
	InventoryStorageManagerComponent m_Manager;
	BaseWeaponManagerComponent m_Weapons;
	AIControlComponent m_Control;
	AIAgent m_Agent;
	EntityID m_AgentId;
	bool m_bKnownAI;
	BaseWorld m_World;
	int m_iEpoch;
	int m_iCreatedAt;
	bool m_bCancelled;
	bool m_bAbove;
	vector m_vAbove;
	ref array<IEntity> m_aTreeNodes = {};
}

class AICF_DroppedWeaponInventoryFrame
{
	AICF_DroppedWeaponFallSafety m_Owner;
	ScriptedInventoryStorageManagerComponent m_Manager;
	IEntity m_Item;
	int m_iEpoch;
}

class AICF_DroppedWeaponFallDeletion
{
	EntityID m_Id;
	RplId m_RplId;
	string m_sRpl;
	int m_iRequestedAt;
	int m_iEpoch;
}

class AICF_DroppedWeaponFallSafety
{
	protected static AICF_DroppedWeaponFallSafety s_Active;
	static const int INTERVAL_MS = 100;
	static const int TTL_MS = 10000;
	static const int MAX_RECORDS = 64;
	static const int MAX_SUBTREE = 32;
	static const int MAX_PLAYERS = 128;
	static const int SAMPLES_PER_TICK = 8;
	static const int TRACES_PER_TICK = 2;
	static const float MIN_DEPTH = 2;
	protected AICF_MatchController m_Owner;
	protected SCR_GameModeCampaign m_Campaign;
	protected BaseWorld m_World;
	protected bool m_bStarted;
	protected bool m_bStopped;
	protected bool m_bTicking;
	protected bool m_bOverflowReported;
	protected int m_iEpoch;
	protected int m_iCursor;
	protected int m_iTraces;
	protected ref array<ref AICF_DroppedWeaponFallRecord> m_aRecords = {};
	protected ref array<ref AICF_DroppedWeaponInventoryFrame> m_aFrames = {};
	protected ref array<ref AICF_DroppedWeaponFallDeletion> m_aDeletions = {};

	bool Start(AICF_MatchController owner, SCR_GameModeCampaign campaign)
	{
		if (m_bStarted || m_bStopped || s_Active || !owner || !campaign || !GetGame()) return false;
		if (!Replication.IsServer() || !campaign.IsMaster()) return false;
		m_Owner = owner;
		m_Campaign = campaign;
		m_World = GetGame().GetWorld();
		if (!m_World || !owner.OwnsDroppedWeaponFallSafety(this)) return false;
		m_bStarted = true;
		m_iEpoch++;
		s_Active = this;
		GetGame().GetCallqueue().CallLater(Tick, INTERVAL_MS, true);
		return true;
	}

	void Stop()
	{
		if (m_bStopped) return;
		m_bStopped = true;
		m_iEpoch++;
		if (GetGame()) GetGame().GetCallqueue().Remove(Tick);
		if (s_Active == this) s_Active = null;
		foreach (AICF_DroppedWeaponFallRecord record : m_aRecords) record.m_bCancelled = true;
		m_aRecords.Clear();
		m_aFrames.Clear();
		m_aDeletions.Clear();
		m_Owner = null;
		m_Campaign = null;
		m_World = null;
	}

	protected bool Owned(int epoch)
	{
		return m_bStarted && !m_bStopped && s_Active == this && epoch == m_iEpoch &&
			m_Owner && m_Owner.OwnsDroppedWeaponFallSafety(this);
	}

	protected bool Authority(int epoch)
	{
		if (!Owned(epoch) || !Replication.IsServer() || !GetGame() || !m_Campaign) return false;
		if (GetGame().GetWorld() != m_World || !m_Campaign.IsMaster() || !m_Campaign.IsRunning()) return false;
		return Owned(epoch);
	}

	protected void Overflow()
	{
		if (m_bOverflowReported || !Owned(m_iEpoch)) return;
		m_bOverflowReported = true;
		AICF_Stage35Diagnostics.Warning("DROPPED_WEAPON_FALL_SAFETY_OVERFLOW", "accepted=0 coverage=INCOMPLETE maximum=64");
	}

	// Installed before the original callback: a nested Add/Remove supersedes the
	// outer frame. Any inventory event revokes the old removal permission first.
	static AICF_DroppedWeaponInventoryFrame BeginAddedEvent(ScriptedInventoryStorageManagerComponent manager, IEntity item)
	{
		AICF_DroppedWeaponFallSafety safety = s_Active;
		if (!safety || !safety.Owned(safety.m_iEpoch)) return null;
		if (safety.m_aRecords.Count() == 0 && safety.m_aFrames.Count() == 0) return null;
		return BeginInventoryEvent(manager, item);
	}

	static AICF_DroppedWeaponInventoryFrame BeginInventoryEvent(ScriptedInventoryStorageManagerComponent manager, IEntity item)
	{
		AICF_DroppedWeaponFallSafety safety = s_Active;
		if (!safety || !manager || !item || !safety.Owned(safety.m_iEpoch)) return null;
		safety.Invalidate(item);
		for (int i = safety.m_aFrames.Count() - 1; i >= 0; i--)
			if (safety.m_aFrames[i].m_Item == item) safety.m_aFrames.RemoveOrdered(i);
		if (safety.m_aFrames.Count() >= MAX_RECORDS) { safety.Overflow(); return null; }
		AICF_DroppedWeaponInventoryFrame frame = new AICF_DroppedWeaponInventoryFrame();
		frame.m_Owner = safety;
		frame.m_Manager = manager;
		frame.m_Item = item;
		frame.m_iEpoch = safety.m_iEpoch;
		safety.m_aFrames.Insert(frame);
		IEntity eventOwner = manager.GetOwner();
		if (safety.FrameCurrent(frame)) safety.Invalidate(eventOwner);
		return frame;
	}

	static void EndInventoryEvent(AICF_DroppedWeaponInventoryFrame frame, BaseInventoryStorageComponent storage, bool removed)
	{
		if (!frame || !frame.m_Owner) return;
		AICF_DroppedWeaponFallSafety safety = frame.m_Owner;
		if (!safety.FrameOwned(frame)) return;
		if (removed && safety.FrameCurrent(frame)) safety.Admit(frame, storage);
		if (safety.FrameOwned(frame)) safety.m_aFrames.RemoveItem(frame);
	}

	protected bool FrameOwned(AICF_DroppedWeaponInventoryFrame frame)
	{
		return frame && frame.m_Owner == this && Owned(frame.m_iEpoch) && m_aFrames.Contains(frame);
	}

	protected bool FrameCurrent(AICF_DroppedWeaponInventoryFrame frame)
	{
		return FrameOwned(frame) && frame.m_Manager && frame.m_Item;
	}

	protected void Invalidate(IEntity item)
	{
		if (!item) return;
		for (int i = m_aRecords.Count() - 1; i >= 0; i--)
		{
			AICF_DroppedWeaponFallRecord record = m_aRecords[i];
			if (record.m_Item != item && !record.m_aTreeNodes.Contains(item)) continue;
			record.m_bCancelled = true;
			m_aRecords.RemoveOrdered(i);
		}
	}

	protected bool RecordCurrent(AICF_DroppedWeaponFallRecord record)
	{
		return record && !record.m_bCancelled && record.m_World == m_World &&
			Owned(record.m_iEpoch) && m_aRecords.Contains(record);
	}

	protected bool Context(AICF_DroppedWeaponFallRecord record)
	{
		return record && Owned(record.m_iEpoch) && record.m_World == m_World &&
			record.m_Item && record.m_Source && record.m_Inventory && record.m_Weapon && record.m_Controller;
	}

	protected static bool Master(IEntity entity, EntityID id, RplComponent rpl, string rplId, BaseWorld world)
	{
		if (!entity || !world || id == EntityID.INVALID || !rpl || rplId.IsEmpty()) return false;
		if (entity.GetWorld() != world || !entity || !world) return false;
		if (entity.GetID() != id || !entity || !world) return false;
		if (world.FindEntityByID(id) != entity || !entity || !rpl) return false;
		if (entity.FindComponent(RplComponent) != rpl || !entity || !rpl) return false;
		if (!rpl.IsMaster() || !rpl || !entity) return false;
		if (rpl.IsProxy() || !rpl || !entity) return false;
		RplId actual = rpl.Id();
		return entity && rpl && actual.IsValid() && actual.ToString() == rplId;
	}

	protected void Admit(AICF_DroppedWeaponInventoryFrame frame, BaseInventoryStorageComponent storage)
	{
		if (!FrameCurrent(frame) || !Authority(frame.m_iEpoch) || !storage) return;
		IEntity item = frame.m_Item;
		ChimeraCharacter source = ChimeraCharacter.Cast(frame.m_Manager.GetOwner());
		if (!source || storage.GetOwner() != source || !item) return;
		if (!FrameCurrent(frame) || !source || !item) return;
		BaseWeaponComponent weapon = BaseWeaponComponent.Cast(item.FindComponent(BaseWeaponComponent));
		if (!weapon) return;
		if (!AICF_EquipmentInventory.Primary(weapon) && weapon.GetWeaponType() != EWeaponType.WT_ROCKETLAUNCHER) return;
		if (!FrameCurrent(frame) || !source || !item || !weapon) return;
		if (m_aRecords.Count() >= MAX_RECORDS) { Overflow(); return; }
		AICF_DroppedWeaponFallRecord record = new AICF_DroppedWeaponFallRecord();
		record.m_Item = item;
		record.m_Id = item.GetID();
		if (!FrameCurrent(frame) || !item || !source) return;
		record.m_Rpl = RplComponent.Cast(item.FindComponent(RplComponent));
		if (!record.m_Rpl) return;
		record.m_RplId = record.m_Rpl.Id();
		record.m_sRpl = record.m_RplId.ToString();
		record.m_Inventory = InventoryItemComponent.Cast(item.FindComponent(InventoryItemComponent));
		if (!FrameCurrent(frame) || !item || !source || !weapon) return;
		record.m_Weapon = weapon;
		record.m_Source = source;
		record.m_SourceId = source.GetID();
		if (!FrameCurrent(frame) || !source) return;
		record.m_SourceRpl = RplComponent.Cast(source.FindComponent(RplComponent));
		if (!record.m_SourceRpl) return;
		record.m_sSourceRpl = record.m_SourceRpl.Id().ToString();
		record.m_Controller = source.GetCharacterController();
		if (!FrameCurrent(frame) || !source) return;
		record.m_Manager = frame.m_Manager;
		record.m_Weapons = BaseWeaponManagerComponent.Cast(source.FindComponent(BaseWeaponManagerComponent));
		if (!FrameCurrent(frame) || !source) return;
		record.m_Control = AIControlComponent.Cast(source.FindComponent(AIControlComponent));
		if (!record.m_Control) return;
		record.m_Agent = record.m_Control.GetControlAIAgent();
		if (!FrameCurrent(frame) || !record.m_Agent || !source) return;
		record.m_AgentId = record.m_Agent.GetID();
		if (!FrameCurrent(frame) || !record.m_Agent || !source) return;
		if (record.m_AgentId == EntityID.INVALID || record.m_Agent.GetWorld() != m_World) return;
		if (!FrameCurrent(frame) || !record.m_Agent || !m_World) return;
		if (m_World.FindEntityByID(record.m_AgentId) != record.m_Agent) return;
		record.m_bKnownAI = true;
		record.m_World = m_World;
		record.m_iEpoch = frame.m_iEpoch;
		record.m_iCreatedAt = System.GetTickCount();
		if (!ItemIdentity(record) || !SourceIdentity(record) || !HumansClear(record)) return;
		if (!FrameCurrent(frame) || !Context(record) || !item) return;
		vector position = item.GetOrigin();
		if (!FrameCurrent(frame) || !Context(record) || !m_World) return;
		if (!FinitePoint(position)) return;
		float terrain = m_World.GetSurfaceY(position[0], position[2]);
		if (terrain != terrain || Math.AbsFloat(terrain) >= float.MAX) return;
		if (!FrameCurrent(frame) || !Authority(frame.m_iEpoch)) return;
		// Only a drop observed above this terrain can later prove a crossing.
		if (position[1] < terrain) return;
		record.m_bAbove = true;
		record.m_vAbove = position;
		if (!FrameCurrent(frame) || m_aRecords.Count() >= MAX_RECORDS) return;
		m_aRecords.Insert(record);
	}

	protected bool ItemIdentity(AICF_DroppedWeaponFallRecord record)
	{
		if (!Context(record)) return false;
		IEntity item = record.m_Item;
		if (ChimeraCharacter.Cast(item) || Vehicle.Cast(item)) return false;
		if (!Master(item, record.m_Id, record.m_Rpl, record.m_sRpl, record.m_World)) return false;
		if (!Context(record) || !item) return false;
		if (item.FindComponent(InventoryItemComponent) != record.m_Inventory) return false;
		if (!Context(record) || !item) return false;
		if (item.FindComponent(BaseWeaponComponent) != record.m_Weapon) return false;
		if (!Context(record) || !item) return false;
		if (record.m_Inventory.GetOwner() != item) return false;
		if (!Context(record)) return false;
		if (record.m_Weapon.GetOwner() != item) return false;
		return Context(record);
	}

	protected bool ItemFree(AICF_DroppedWeaponFallRecord record)
	{
		if (!ItemIdentity(record)) return false;
		IEntity item = record.m_Item;
		if (!Context(record) || !item || item.GetParent()) return false;
		if (!Context(record) || !item) return false;
		if (item.GetRootParent() != item) return false;
		if (!Context(record)) return false;
		if (record.m_Inventory.GetParentSlot()) return false;
		if (!Context(record)) return false;
		bool locked = record.m_Inventory.IsLocked();
		return Context(record) && !locked;
	}

	protected static bool EntityHolds(IEntity owner, IEntity item)
	{
		if (!owner) return false;
		if (owner == item) return true;
		InventoryStorageManagerComponent manager = AICF_EquipmentInventory.Manager(owner);
		if (manager && manager.Contains(item)) return true;
		if (!owner || !item) return true;
		CharacterControllerComponent controller = CharacterControllerComponent.Cast(owner.FindComponent(CharacterControllerComponent));
		if (!controller) return false;
		if (controller.GetCurrentItemInHands() == item || controller.GetRightHandItem() == item) return true;
		if (!controller || !owner || !item) return true;
		CharacterInputContext input = controller.GetInputContext();
		if (input && input.GetLeftHandGadgetEntity() == item) return true;
		return false;
	}

	protected bool SourceIdentity(AICF_DroppedWeaponFallRecord record)
	{
		if (!record || !record.m_bKnownAI || record.m_AgentId == EntityID.INVALID) return false;
		if (!record.m_Source || !record.m_Controller || !record.m_Manager || !record.m_Weapons || !record.m_Control) return false;
		ChimeraCharacter source = record.m_Source;
		if (!Master(source, record.m_SourceId, record.m_SourceRpl, record.m_sSourceRpl, record.m_World)) return false;
		if (!Context(record) || !source) return false;
		if (source.GetCharacterController() != record.m_Controller || AICF_EquipmentInventory.Manager(source) != record.m_Manager) return false;
		if (source.FindComponent(BaseWeaponManagerComponent) != record.m_Weapons || source.FindComponent(AIControlComponent) != record.m_Control) return false;
		if (!Context(record) || !record.m_Control) return false;
		AIAgent agent = record.m_Control.GetControlAIAgent();
		if (!Context(record)) return false;
		if (agent)
		{
			if (agent != record.m_Agent || agent.GetID() != record.m_AgentId) return false;
			if (!Context(record) || !agent) return false;
			if (agent.GetWorld() != record.m_World || !agent) return false;
			if (!Context(record) || record.m_World.FindEntityByID(record.m_AgentId) != agent || !agent) return false;
			IEntity controlled = agent.GetControlledEntity();
			if (!Context(record)) return false;
			if (controlled != source && (controlled || record.m_Controller.GetLifeState() != ECharacterLifeState.DEAD)) return false;
		}
		else if (record.m_Controller.GetLifeState() != ECharacterLifeState.DEAD) return false;
		if (record.m_Controller.IsPlayerControlled()) return false;
		if (!Context(record) || !GetGame()) return false;
		PlayerManager players = GetGame().GetPlayerManager();
		if (!players || players.GetPlayerIdFromControlledEntity(source) > 0) return false;
		return SCR_PossessingManagerComponent.GetPlayerIdFromMainEntity(source) <= 0;
	}

	protected bool SourceFree(AICF_DroppedWeaponFallRecord record)
	{
		if (!SourceIdentity(record)) return false;
		if (record.m_Controller.IsChangingItem() || EntityHolds(record.m_Source, record.m_Item)) return false;
		if (!Context(record) || !record.m_Weapons) return false;
		array<WeaponSlotComponent> slots = {};
		record.m_Weapons.GetWeaponsSlots(slots);
		if (!Context(record) || slots.Count() > 16) return false;
		foreach (WeaponSlotComponent slot : slots)
			if (slot && slot.GetWeaponEntity() == record.m_Item) return false;
		return Context(record);
	}

	protected bool HumansClear(AICF_DroppedWeaponFallRecord record)
	{
		if (!record || !record.m_Source || !record.m_Item || !GetGame()) return false;
		PlayerManager players = GetGame().GetPlayerManager();
		if (!players) return false;
		array<int> ids = {};
		players.GetAllPlayers(ids);
		if (ids.Count() > MAX_PLAYERS) return false;
		foreach (int id : ids)
		{
			PlayerController player = players.GetPlayerController(id);
			if (!player) return false;
			IEntity controlled = player.GetControlledEntity();
			if (!Context(record) || !player) return false;
			IEntity main = SCR_PossessingManagerComponent.GetPlayerMainEntity(id);
			if (controlled == record.m_Source || main == record.m_Source) return false;
			if (EntityHolds(controlled, record.m_Item) || EntityHolds(main, record.m_Item)) return false;
			if (!Context(record)) return false;
		}
		if (!Context(record) || !GetGame() || GetGame().GetPlayerManager() != players) return false;
		if (GetGame().GetWorld() != record.m_World) return false;
		return !record.m_Controller.IsPlayerControlled() && players.GetPlayerIdFromControlledEntity(record.m_Source) <= 0 &&
			SCR_PossessingManagerComponent.GetPlayerIdFromMainEntity(record.m_Source) <= 0;
	}

	// Deleting a weapon deletes its descendants too. Every descendant must be a
	// bounded inventory item physically owned by this tree, never an actor/car.
	protected bool SubtreeSafe(AICF_DroppedWeaponFallRecord record)
	{
		if (!RecordCurrent(record)) return false;
		array<IEntity> nodes = {record.m_Item};
		for (int index; index < nodes.Count(); index++)
		{
			IEntity parent = nodes[index];
			if (!RecordCurrent(record) || !parent || parent.GetWorld() != record.m_World) return false;
			if (!RecordCurrent(record) || !parent) return false;
			IEntity child = parent.GetChildren();
			if (!RecordCurrent(record)) return false;
			while (child)
			{
				if (nodes.Count() >= MAX_SUBTREE || nodes.Contains(child)) return false;
				IEntity actualParent = child.GetParent();
				if (!RecordCurrent(record) || !child || actualParent != parent) return false;
				BaseWorld actualWorld = child.GetWorld();
				if (!RecordCurrent(record) || !child || actualWorld != record.m_World) return false;
				if (ChimeraCharacter.Cast(child) || Vehicle.Cast(child)) return false;
				InventoryItemComponent childItem = InventoryItemComponent.Cast(child.FindComponent(InventoryItemComponent));
				if (!RecordCurrent(record) || !child || !childItem) return false;
				nodes.Insert(child);
				child = child.GetSibling();
				if (!RecordCurrent(record)) return false;
			}
		}
		foreach (IEntity node : nodes)
		{
			if (!RecordCurrent(record) || !node || node.GetWorld() != record.m_World) return false;
			if (!RecordCurrent(record) || !node) return false;
			InventoryItemComponent inventory = InventoryItemComponent.Cast(node.FindComponent(InventoryItemComponent));
			if (!RecordCurrent(record) || !node || !inventory || inventory.IsLocked()) return false;
			if (!RecordCurrent(record) || !node || !inventory) return false;
			InventoryStorageSlot slot = inventory.GetParentSlot();
			if (!RecordCurrent(record) || !node) return false;
			if (node.GetRootParent() != record.m_Item) return false;
			if (!RecordCurrent(record) || !node) return false;
			RplComponent rpl = RplComponent.Cast(node.FindComponent(RplComponent));
			if (!RecordCurrent(record) || !node) return false;
			if (rpl)
			{
				if (!rpl.IsMaster() || !rpl || !node) return false;
				if (rpl.IsProxy() || !rpl || !node) return false;
				if (!rpl.Id().IsValid()) return false;
				if (!RecordCurrent(record) || !node) return false;
			}
			if (node == record.m_Item) { if (slot) return false; }
			else
			{
				if (!slot) return false;
				IEntity nodeParent = node.GetParent();
				if (!RecordCurrent(record) || !node || !nodes.Contains(nodeParent)) return false;
				BaseInventoryStorageComponent storage = slot.GetStorage();
				if (!RecordCurrent(record) || !storage) return false;
				IEntity storageOwner = storage.GetOwner();
				if (!RecordCurrent(record) || !nodes.Contains(storageOwner)) return false;
			}
		}
		if (!RecordCurrent(record)) return false;
		record.m_aTreeNodes.Copy(nodes);
		return true;
	}

	protected static bool FinitePoint(vector point)
	{
		for (int axis; axis < 3; axis++)
			if (point[axis] != point[axis] || Math.AbsFloat(point[axis]) >= float.MAX) return false;
		return true;
	}

	protected bool PhysicalContext(AICF_DroppedWeaponFallRecord record)
	{
		if (!RecordCurrent(record) || !Context(record) || !Authority(record.m_iEpoch)) return false;
		if (!Context(record) || !record.m_Item) return false;
		BaseWorld actualWorld = record.m_Item.GetWorld();
		if (!RecordCurrent(record) || !Context(record) || actualWorld != record.m_World) return false;
		return Authority(record.m_iEpoch) && RecordCurrent(record) && Context(record);
	}

	protected bool CrossedTerrain(AICF_DroppedWeaponFallRecord record)
	{
		if (!RecordCurrent(record) || !record.m_bAbove || m_iTraces >= TRACES_PER_TICK) return false;
		if (!PhysicalContext(record)) return false;
		Physics physics = record.m_Item.GetPhysics();
		if (!physics || !PhysicalContext(record)) return false;
		bool dynamicBody = physics.IsDynamic();
		if (!dynamicBody || !physics || !PhysicalContext(record)) return false;
		vector velocity = physics.GetVelocity();
		if (!PhysicalContext(record)) return false;
		vector position = record.m_Item.GetOrigin();
		if (!PhysicalContext(record) || !m_World) return false;
		if (!FinitePoint(position) || !FinitePoint(velocity) || !FinitePoint(record.m_vAbove)) return false;
		float terrain = m_World.GetSurfaceY(position[0], position[2]);
		if (terrain != terrain || Math.AbsFloat(terrain) >= float.MAX) return false;
		if (position[1] >= terrain)
		{
			if (RecordCurrent(record)) record.m_vAbove = position;
			return false;
		}
		if (terrain - position[1] < MIN_DEPTH || velocity[1] >= -1) return false;
		TraceParam trace = new TraceParam();
		trace.Flags = TraceFlags.WORLD;
		trace.Start = record.m_vAbove;
		trace.End = position;
		m_iTraces++;
		float fraction = m_World.TraceMove(trace, null);
		if (!RecordCurrent(record) || !Authority(record.m_iEpoch)) return false;
		if (!FinitePoint(trace.TraceNorm)) return false;
		return fraction > 0 && fraction < 1 && trace.TraceNorm[1] > 0.2;
	}

	protected void Inspect(AICF_DroppedWeaponFallRecord record)
	{
		if (!RecordCurrent(record) || !Authority(record.m_iEpoch)) return;
		if (System.GetTickCount() - record.m_iCreatedAt >= TTL_MS) { record.m_bCancelled = true; return; }
		if (!ItemIdentity(record) || !SourceIdentity(record) || !HumansClear(record))
		{
			if (RecordCurrent(record)) record.m_bCancelled = true;
			return;
		}
		// Native item-change/slot/lock transitions are observations, not permission.
		// Wait inside the original fixed TTL; a real Add/Remove revokes the token.
		if (!ItemFree(record) || !SourceFree(record)) return;
		if (!CrossedTerrain(record)) return;
		if (!RecordCurrent(record) || !SubtreeSafe(record) || !HumansClear(record)) return;
		if (!RecordCurrent(record) || !ItemFree(record) || !SourceFree(record)) return;
		if (!SubtreeSafe(record) || !ItemFree(record) || !Authority(record.m_iEpoch)) return;
		if (!SourceFree(record) || !HumansClear(record)) return;
		if (!RecordCurrent(record)) return;
		if (m_aDeletions.Count() >= MAX_RECORDS) { Overflow(); return; }
		IEntity item = record.m_Item;
		string itemId = record.m_Id.ToString();
		string itemRpl = record.m_sRpl;
		string sourceId = record.m_SourceId.ToString();
		AICF_DroppedWeaponFallDeletion deletion = new AICF_DroppedWeaponFallDeletion();
		deletion.m_Id = record.m_Id;
		deletion.m_RplId = record.m_RplId;
		deletion.m_sRpl = record.m_sRpl;
		int requestedAt = System.GetTickCount();
		int age = requestedAt - record.m_iCreatedAt;
		if (!RecordCurrent(record)) return;
		if (age < 0 || age >= TTL_MS) { record.m_bCancelled = true; return; }
		deletion.m_iRequestedAt = requestedAt;
		deletion.m_iEpoch = record.m_iEpoch;
		if (!RecordCurrent(record)) return;
		// No native getter between this final owned permission and deletion.
		m_aDeletions.Insert(deletion);
		record.m_bCancelled = true;
		m_aRecords.RemoveItem(record);
		RplComponent.DeleteRplEntity(item, false);
		AICF_Stage35Diagnostics.Warning("DROPPED_WEAPON_FALL_SAFETY_DELETE_REQUESTED", string.Format("entity_id_canonical=%1 rpl_id=%2 source_id_canonical=%3 reason=KNOWN_FREE_AI_DROP_CROSSED_TERRAIN confirmed_absent=0 teleport=0 physics_mutated=0 health_mutated=0", itemId, itemRpl, sourceId));
	}

	protected void VerifyDeletions()
	{
		// Read-only acknowledgement on a later Tick; never retries deletion.
		array<ref AICF_DroppedWeaponFallDeletion> pending = {};
		for (int i; i < m_aDeletions.Count() && i < SAMPLES_PER_TICK; i++) pending.Insert(m_aDeletions[i]);
		foreach (AICF_DroppedWeaponFallDeletion deletion : pending)
		{
			if (!Authority(deletion.m_iEpoch) || !m_aDeletions.Contains(deletion)) return;
			bool entityAbsent = !m_World.FindEntityByID(deletion.m_Id);
			bool rplAbsent = !Replication.FindItem(deletion.m_RplId);
			if (!Authority(deletion.m_iEpoch) || !m_aDeletions.Contains(deletion)) return;
			if (entityAbsent && rplAbsent)
			{
				m_aDeletions.RemoveItem(deletion);
				AICF_Stage35Diagnostics.Info("DROPPED_WEAPON_FALL_SAFETY_DELETE_CONFIRMED", string.Format("entity_id_canonical=%1 rpl_id=%2 confirmed_absent=1 same_main_world=1", deletion.m_Id.ToString(), deletion.m_sRpl));
			}
			else if (System.GetTickCount() - deletion.m_iRequestedAt >= TTL_MS)
			{
				m_aDeletions.RemoveItem(deletion);
				AICF_Stage35Diagnostics.Warning("DROPPED_WEAPON_FALL_SAFETY_DELETE_UNCONFIRMED", string.Format("entity_id_canonical=%1 rpl_id=%2 confirmed_absent=0 retry=0", deletion.m_Id.ToString(), deletion.m_sRpl));
			}
		}
	}

	protected void Tick()
	{
		if (m_bTicking || !Authority(m_iEpoch)) return;
		m_bTicking = true;
		VerifyDeletions();
		if (!Owned(m_iEpoch)) { m_bTicking = false; return; }
		m_iTraces = 0;
		array<ref AICF_DroppedWeaponFallRecord> pending = {};
		for (int count; count < SAMPLES_PER_TICK && count < m_aRecords.Count(); count++)
		{
			if (m_iCursor >= m_aRecords.Count()) m_iCursor = 0;
			pending.Insert(m_aRecords[m_iCursor++]);
		}
		foreach (AICF_DroppedWeaponFallRecord record : pending)
		{
			if (!Owned(m_iEpoch)) break;
			Inspect(record);
		}
		for (int i = m_aRecords.Count() - 1; i >= 0; i--)
			if (m_aRecords[i].m_bCancelled) m_aRecords.RemoveOrdered(i);
		m_bTicking = false;
	}
}

modded class ScriptedInventoryStorageManagerComponent
{
	override protected void OnItemRemoved(BaseInventoryStorageComponent storageOwner, IEntity item)
	{
		AICF_DroppedWeaponInventoryFrame frame = AICF_DroppedWeaponFallSafety.BeginInventoryEvent(this, item);
		super.OnItemRemoved(storageOwner, item);
		AICF_DroppedWeaponFallSafety.EndInventoryEvent(frame, storageOwner, true);
	}

	override protected void OnItemAdded(BaseInventoryStorageComponent storageOwner, IEntity item)
	{
		AICF_DroppedWeaponInventoryFrame frame = AICF_DroppedWeaponFallSafety.BeginAddedEvent(this, item);
		super.OnItemAdded(storageOwner, item);
		AICF_DroppedWeaponFallSafety.EndInventoryEvent(frame, storageOwner, false);
	}
}

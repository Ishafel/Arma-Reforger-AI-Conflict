// Только isolated stress stage. Меняет начальные рецепты, но не cap/экономику/AI.
modded class AICF_CoopFleetDefaults
{
	override static void Apply(AICF_FactionState state, AICF_MatchRules rules)
	{
		if (!System.IsCLIParam("aicfLongBattle") || !state || !rules || rules.GetMode() != AICF_EMatchMode.STANDARD)
		{
			super.Apply(state, rules);
			return;
		}
		AICF_EMatchMode view = AICF_EMatchMode.COOP_US;
		if (AICF_ContentProfile.GetActive().GetStableFactionKey(state.GetFactionKey()) == "US") view = AICF_EMatchMode.COOP_USSR;
		super.Apply(state, new AICF_MatchRules(view));
		Print("[AICF][LONG_BATTLE_FLEET] controlled_initial_recipes=1 faction=" + state.GetFactionKey());
	}
}

class AICF_LongBattleView
{
	static vector s_Center;
	static vector s_Target;
	static RplId s_Actor;
	static bool s_bReady;
	static bool s_bFrontline;
	static string s_sGroup;
}

modded class AICF_MatchController
{
	protected int m_iSoakStarted;
	protected int m_iSoakNextView;
	protected int m_iSoakSwitch;
	protected AICF_GroupSlot m_SoakSlot;
	protected bool m_bSoakClosed;

	protected void AICF_SoakChoose(AICF_FactionState state, inout float best)
	{
		if (!state) return;
		for (int i; i < state.GetSlotCount(); i++)
		{
			AICF_GroupSlot slot = state.GetSlot(i);
			if (!slot || !slot.IsCombatReady() || slot.GetRole() != AICF_EGroupRole.ATTACK ||
				slot.IsRecruitingInfantry() || !slot.GetTargetBase()) continue;
			IEntity leader = AICF_GroupRuntime.ResolveAliveLeader(slot.GetGroup());
			if (!leader || !slot.GetTargetBase().GetOwner()) continue;
			// Recruitment/muster targets on own bases cannot qualify as battle view.
			if (slot.GetTargetBase().GetFaction() == slot.GetGroup().GetFaction()) continue;
			float distance = vector.DistanceXZ(leader.GetOrigin(), slot.GetTargetBase().GetOwner().GetOrigin());
			if (distance >= best) continue;
			best = distance;
			m_SoakSlot = slot;
		}
	}

	override protected void Update()
	{
		super.Update();
		if (!System.IsCLIParam("aicfLongBattle") || !m_bRosterReady) return;
		int now = System.GetTickCount();
		if (!m_iSoakStarted) m_iSoakStarted = now;
		string value;
		int duration = 1920000;
		if (System.GetCLIParam("aicfLongBattleDurationMs", value)) duration = value.ToInt();
		if (!m_bSoakClosed && duration >= 60000 && duration <= 3600000 && now - m_iSoakStarted >= duration)
		{
			m_bSoakClosed = true;
			Print("[AICF][LONG_BATTLE_DONE] normal_shutdown_requested=1");
			GetGame().RequestClose();
			return;
		}
		if (now < m_iSoakNextView) return;
		m_iSoakNextView = now + 5000;
		IEntity leader;
		if (m_SoakSlot && m_SoakSlot.GetGroup()) leader = AICF_GroupRuntime.ResolveAliveLeader(m_SoakSlot.GetGroup());
		if (!leader || now >= m_iSoakSwitch)
		{
			float best = float.MAX;
			m_SoakSlot = null;
			AICF_SoakChoose(m_USState, best);
			AICF_SoakChoose(m_USSRState, best);
			if (!m_SoakSlot && m_USState && m_USState.GetSlotCount() > 0) m_SoakSlot = m_USState.GetSlot(0);
			m_iSoakSwitch = now + 120000;
		}
		AICF_LongBattleView.s_bReady = false;
		if (!m_SoakSlot || !m_SoakSlot.GetGroup()) return;
		leader = AICF_GroupRuntime.ResolveAliveLeader(m_SoakSlot.GetGroup());
		if (!leader || !m_SoakSlot.GetTargetBase() || !m_SoakSlot.GetTargetBase().GetOwner()) return;
		RplComponent rpl = RplComponent.Cast(leader.FindComponent(RplComponent));
		if (!rpl || !rpl.Id().IsValid()) return;
		AICF_LongBattleView.s_Center = leader.GetOrigin();
		AICF_LongBattleView.s_Target = m_SoakSlot.GetTargetBase().GetOwner().GetOrigin();
		AICF_LongBattleView.s_Actor = rpl.Id();
		AICF_LongBattleView.s_bFrontline = m_SoakSlot.GetTargetBase().GetFaction() != m_SoakSlot.GetGroup().GetFaction() &&
			AICF_GroupRuntime.CountAliveAgents(m_SoakSlot.GetGroup()) >= 3 &&
			vector.DistanceXZ(AICF_LongBattleView.s_Center, AICF_LongBattleView.s_Target) <= 500;
		AICF_LongBattleView.s_sGroup = m_SoakSlot.GetGroup().GetID().ToString();
		AICF_LongBattleView.s_bReady = true;
	}
}

class AICF_SoakCameraClass : CameraBaseClass {}
class AICF_SoakCamera : CameraBase {}

modded class SCR_PlayerController
{
	protected AICF_SoakCamera m_SoakCamera;
	protected int m_iSoakRequestAt;
	void AICF_RequestSoakView() { Rpc(RpcAsk_AICFSoakView); }
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_AICFSoakView()
	{
		if (!System.IsCLIParam("aicfLongBattle") || !Replication.IsServer() || !AICF_LongBattleView.s_bReady ||
			GetPlayerId() <= 0 || GetGame().GetPlayerManager().GetPlayerController(GetPlayerId()) != this) return;
		int now = System.GetTickCount();
		if (now < m_iSoakRequestAt) return;
		m_iSoakRequestAt = now + 4000;
		Rpc(RpcDo_AICFSoakView, AICF_LongBattleView.s_Center, AICF_LongBattleView.s_Target,
			AICF_LongBattleView.s_Actor, AICF_LongBattleView.s_bFrontline, AICF_LongBattleView.s_sGroup);
	}
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_AICFSoakView(vector center, vector target, RplId actor, bool frontline, string group)
	{
		if (Replication.IsServer() || this != GetGame().GetPlayerController()) return;
		if (!m_SoakCamera) m_SoakCamera = AICF_SoakCamera.Cast(GetGame().SpawnEntity(AICF_SoakCamera, GetGame().GetWorld()));
		if (!m_SoakCamera) return;
		CameraManager cameraManager = GetGame().GetCameraManager();
		if (!cameraManager || !cameraManager.SetCamera(m_SoakCamera)) return;
		vector actual[4];
		GetGame().GetWorld().GetCurrentCamera(actual);
		float previousCameraError = vector.Distance(actual[3], m_SoakCamera.GetOrigin());
		vector direction = target - center;
		direction[1] = 0;
		if (direction.LengthSq() < 1) direction = "0 0 1";
		direction.Normalize();
		vector position = center - direction * 18 + "0 7 0";
		position[1] = Math.Max(position[1], GetGame().GetWorld().GetSurfaceY(position[0], position[2]) + 3);
		m_SoakCamera.SetOrigin(position);
		vector lookAt = center;
		lookAt[1] = lookAt[1] + 1.5;
		vector lookDirection = lookAt - position;
		m_SoakCamera.SetYawPitchRoll(lookDirection.VectorToAngles());
		RplComponent replicated = RplComponent.Cast(Replication.FindItem(actor));
		bool entityPresent = replicated && replicated.GetEntity();
		float entityDistance = -1;
		if (entityPresent) entityDistance = vector.DistanceXZ(center, replicated.GetEntity().GetOrigin());
		Print(string.Format("[AICF][LONG_BATTLE_VIEW] frontline=%1 actor_replicated=%2 actor_distance_m=%3 camera_error_m=%4 group=%5 camera=%6 target=%7 fps=%8 camera_actor_distance_m=%9", frontline, entityPresent, entityDistance, previousCameraError, group, actual[3], center, System.GetFPS(), vector.Distance(actual[3], center)));
	}
	void ~SCR_PlayerController() { if (m_SoakCamera) delete m_SoakCamera; }
}

modded class SCR_BaseGameMode
{
	protected int m_iSoakClientStart;
	protected int m_iSoakClientViewAt;
	protected bool m_bSoakLoadoutRequested;
	protected bool m_bSoakSpawnRequested;
	override void OnGameStart()
	{
		super.OnGameStart();
		if (Replication.IsServer() || !System.IsCLIParam("aicfLongBattle")) return;
		m_iSoakClientStart = System.GetTickCount();
		GetGame().GetCallqueue().CallLater(AICF_SoakClientTick, 1000, true);
	}
	protected void AICF_SoakClientTick()
	{
		string value;
		int duration = 1800000;
		if (System.GetCLIParam("aicfLongBattleClientDurationMs", value)) duration = value.ToInt();
		if (duration >= 60000 && System.GetTickCount() - m_iSoakClientStart >= duration)
		{
			GetGame().GetCallqueue().Remove(AICF_SoakClientTick);
			Print("[AICF][LONG_BATTLE_CLIENT_DONE] normal_shutdown_requested=1");
			GetGame().RequestClose();
			return;
		}
		SCR_PlayerController player = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!player) return;
		Faction faction = GetGame().GetFactionManager().GetFactionByKey(AICF_ContentProfile.GetActive().GetRuntimeFactionKey("US"));
		SCR_PlayerFactionAffiliationComponent affiliation = SCR_PlayerFactionAffiliationComponent.Cast(player.FindComponent(SCR_PlayerFactionAffiliationComponent));
		if (!faction || !affiliation) return;
		if (affiliation.GetAffiliatedFaction() != faction) { affiliation.RequestFaction(faction); return; }
		IEntity avatar = player.GetControlledEntity();
		ChimeraCharacter character = ChimeraCharacter.Cast(avatar);
		if (!character || !character.GetCharacterController() || character.GetCharacterController().IsDead())
		{
			array<ref SCR_BasePlayerLoadout> loadouts = {};
			GetGame().GetLoadoutManager().GetPlayerLoadoutsByFaction(faction, loadouts);
			SCR_BasePlayerLoadout loadout;
			foreach (SCR_BasePlayerLoadout candidate : loadouts)
				if (candidate && !SCR_PlayerArsenalLoadout.Cast(candidate)) { loadout = candidate; break; }
			SCR_PlayerLoadoutComponent component = SCR_PlayerLoadoutComponent.Cast(player.FindComponent(SCR_PlayerLoadoutComponent));
			if (!loadout || !component) return;
			if (component.GetLoadout() != loadout)
			{
				if (!m_bSoakLoadoutRequested) { m_bSoakLoadoutRequested = true; component.RequestLoadout(loadout); }
				return;
			}
			if (m_bSoakSpawnRequested) return;
			array<SCR_SpawnPoint> points = SCR_SpawnPoint.GetSpawnPointsForFaction(faction.GetFactionKey());
			foreach (SCR_SpawnPoint point : points)
			{
				if (!point || !point.IsSpawnPointActive() || !point.IsSpawnPointEnabled() || !point.GetRplId().IsValid()) continue;
				SCR_RespawnComponent respawn = SCR_RespawnComponent.Cast(player.GetRespawnComponent());
				if (respawn) m_bSoakSpawnRequested = respawn.RequestSpawn(new SCR_SpawnPointSpawnData(loadout.GetLoadoutResource(), point.GetRplId()));
				break;
			}
			return;
		}
		m_bSoakSpawnRequested = false;
		if (System.GetTickCount() < m_iSoakClientViewAt) return;
		m_iSoakClientViewAt = System.GetTickCount() + 5000;
		player.AICF_RequestSoakView();
	}
	void ~SCR_BaseGameMode() { if (GetGame()) GetGame().GetCallqueue().Remove(AICF_SoakClientTick); }
}

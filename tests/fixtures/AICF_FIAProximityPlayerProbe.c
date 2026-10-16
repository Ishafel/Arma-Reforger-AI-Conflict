// После общего и safety probe. Реальный client, без управления UI.
modded class AICF_MatchController
{
	protected SCR_PlayerController m_AICFFIAPlayer;
	protected ChimeraCharacter m_AICFFIAPlayerEntity;
	protected bool m_bAICFFIAPlayerReadback;
	protected int m_iAICFFIAOccupancyPhase;
	protected int m_iAICFFIAOccupancyAt;
	protected int m_iAICFFIAHandoffAt;

	override protected bool AICF_FIAProbePrepareActor()
	{
		if (m_sAICFFIAProbeMode != "player") return super.AICF_FIAProbePrepareActor();
		array<int> players = {};
		GetGame().GetPlayerManager().GetPlayers(players);
		foreach (int id : players) m_AICFFIAPlayer = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(id));
		if (!m_AICFFIAPlayer) return false;
		SCR_PlayerFactionAffiliationComponent affiliation = SCR_PlayerFactionAffiliationComponent.Cast(m_AICFFIAPlayer.FindComponent(SCR_PlayerFactionAffiliationComponent));
		if (!affiliation || affiliation.GetAffiliatedFaction() != m_USFaction) return false;
		if (!m_iAICFFIAHandoffAt)
		{
			m_Social.Stop();
			m_iAICFFIAHandoffAt = System.GetTickCount() + 3000;
			return false;
		}
		if (System.GetTickCount() < m_iAICFFIAHandoffAt) return false;
		m_AICFFIAPlayerEntity = ChimeraCharacter.Cast(AICF_GroupRuntime.ResolveAliveLeader(m_AICFFIAProbeActor));
		if (!m_AICFFIAPlayerEntity) return false;
		m_AICFFIAPlayer.SetInitialMainEntity(m_AICFFIAPlayerEntity);
		m_ManagedObservers.Stop();
		return true;
	}

	override protected void AICF_FIAProbePlaceActor(vector position)
	{
		if (m_sAICFFIAProbeMode != "player") { super.AICF_FIAProbePlaceActor(position); return; }
		if (!m_AICFFIAPlayer || m_AICFFIAPlayer.GetControlledEntity() != m_AICFFIAPlayerEntity || m_iAICFFIAOccupancyPhase) return;
		if (!m_bAICFFIAPlayerReadback)
		{
			m_bAICFFIAPlayerReadback = true;
			AICF_FIAProximityProbe.Check("PLAYER_CONTROLLED_ENTITY", GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(m_AICFFIAPlayerEntity) == m_AICFFIAPlayer.GetPlayerId());
			Print(string.Format("[AICF][FIA_PROXIMITY_PLAYER] id=%1 controlled=%2", m_AICFFIAPlayer.GetPlayerId(), m_AICFFIAPlayerEntity.GetID()));
		}
		position[1] = GetGame().GetWorld().GetSurfaceY(position[0], position[2]) + 0.1;
		m_AICFFIAPlayerEntity.GetDamageManager().EnableDamageHandling(false);
		m_Campaign.AICF_ProbePlayerPosition(position);
		m_AICFFIAPlayerEntity.SetOrigin(position);
	}

	override protected void AICF_FIAProbeSafetyChecks()
	{
		if (m_sAICFFIAProbeMode != "player") { super.AICF_FIAProbeSafetyChecks(); return; }
		AICF_FIAGarrison g = m_AICFFIAProbeTarget;
		if (!m_iAICFFIAOccupancyPhase)
		{
			super.AICF_FIAProbeSafetyChecks();
			// Настоящий игрок получает уже сидящего исходного члена экипажа.
			// Known identity остаётся прежней: veto обязан распознать игрока.
			foreach (ChimeraCharacter member : g.m_aCrew)
			{
				if (!AICF_GroupRuntime.IsAliveCharacter(member) || CompartmentAccessComponent.GetVehicleIn(member) != g.m_Vehicle) continue;
				m_AICFFIAPlayerEntity = member;
				m_Campaign.AICF_ProbePlayerOccupancy(RplComponent.Cast(g.m_Vehicle.FindComponent(RplComponent)).Id());
				m_AICFFIAPlayer.SetInitialMainEntity(member);
				m_iAICFFIAOccupancyPhase = 1;
				m_iAICFFIAOccupancyAt = System.GetTickCount();
				return;
			}
			AICF_FIAProximityProbe.Check("PLAYER_SEATED_CREW", false);
			AICF_FIAProbeFinish();
			return;
		}
		if (System.GetTickCount() - m_iAICFFIAOccupancyAt < 8000) return;
		bool occupied = m_AICFFIAPlayer.GetControlledEntity() == m_AICFFIAPlayerEntity &&
			GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(m_AICFFIAPlayerEntity) == m_AICFFIAPlayer.GetPlayerId() &&
			CompartmentAccessComponent.GetVehicleIn(m_AICFFIAPlayerEntity) == g.m_Vehicle;
		string reason;
		int before = AICF_FIAProbeAlive();
		bool allowed = AICF_VehicleCleanupManager.CanDormantFIA(g.m_Vehicle, g.m_VehicleId, g.m_Group, g.m_aCrew, g.m_aCrewIds, g.m_DesantGroup, reason);
		g.m_Group.DespawnMembers();
		Print(string.Format("[AICF][FIA_PROXIMITY_OCCUPANCY] occupied=%1 allowed=%2 alive_before=%3 alive_after=%4 reason=%5", occupied, allowed, before, AICF_FIAProbeAlive(), reason));
		AICF_FIAProximityProbe.Check("PLAYER_OCCUPANCY_VETO", occupied && !allowed && before == AICF_FIAProbeAlive() && g.m_Vehicle == m_AICFFIAProbeVehicle);
		m_iAICFFIAOccupancyPhase = 2;
		AICF_FIAProbeFinish();
	}

	override protected bool AICF_FIAProbeSafetyPending() { return m_sAICFFIAProbeMode == "player" && m_iAICFFIAOccupancyPhase < 2; }

	override protected void AICF_FIAProbeFinish()
	{
		if (m_sAICFFIAProbeMode != "player") { super.AICF_FIAProbeFinish(); return; }
		if (m_iAICFFIAProbePhase == 99) return;
		Print(string.Format("[AICF][FIA_PROXIMITY_DONE] mode=%1 checks=%2 failures=%3", m_sAICFFIAProbeMode, AICF_FIAProximityProbe.s_iChecks, AICF_FIAProximityProbe.s_iFailures));
		m_iAICFFIAProbePhase = 99;
		AICF_FIAProximityProbe.s_bBudget = false;
		m_Campaign.AICF_ProbePlayerFinish();
		// Client закрывается до server disconnect и возврата в native menu.
		GetGame().GetCallqueue().CallLater(AICF_FIAPlayerServerClose, 6000, false);
	}

	protected void AICF_FIAPlayerServerClose()
	{
		GetGame().GetCallqueue().Remove(AICF_FIAPlayerServerClose);
		GetGame().RequestClose();
	}

	override protected void Stop(bool cleanupEntities)
	{
		GetGame().GetCallqueue().Remove(AICF_FIAPlayerServerClose);
		super.Stop(cleanupEntities);
	}
}

modded class SCR_GameModeCampaign
{
	[RplProp()]
	protected vector m_vAICFFIAPlayerPosition;
	[RplProp()]
	protected bool m_bAICFFIAPlayerPositionSet;
	[RplProp()]
	protected RplId m_AICFFIAOccupancyVehicle = RplId.Invalid();
	[RplProp()]
	protected bool m_bAICFFIAPlayerFinished;
	protected bool m_bAICFFIAClientReadback;
	protected bool m_bAICFFIAOccupancyReadback;

	void AICF_ProbePlayerPosition(vector position)
	{
		if (!Replication.IsServer() || (m_bAICFFIAPlayerPositionSet && m_vAICFFIAPlayerPosition == position)) return;
		m_bAICFFIAPlayerPositionSet = true;
		m_vAICFFIAPlayerPosition = position;
		Replication.BumpMe();
	}

	void AICF_ProbePlayerOccupancy(RplId vehicle)
	{
		if (!Replication.IsServer()) return;
		m_AICFFIAOccupancyVehicle = vehicle;
		Replication.BumpMe();
	}

	void AICF_ProbePlayerFinish()
	{
		if (!Replication.IsServer() || m_bAICFFIAPlayerFinished) return;
		m_bAICFFIAPlayerFinished = true;
		Replication.BumpMe();
	}

	override void OnGameStart()
	{
		super.OnGameStart();
		string mode;
		if (!Replication.IsServer() && System.GetCLIParam("aicfFIAProximityProbe", mode))
		{
			GetGame().GetCallqueue().CallLater(AICF_FIAClientTick, 2000, true);
			GetGame().GetCallqueue().CallLater(AICF_FIAClientClose, 300000, false);
		}
	}

	protected void AICF_FIAClientTick()
	{
		if (m_bAICFFIAPlayerFinished) { AICF_FIAClientClose(); return; }
		SCR_PlayerController player = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!player) return;
		Faction desired = GetGame().GetFactionManager().GetFactionByKey(AICF_ContentProfile.GetActive().GetRuntimeFactionKey("US"));
		SCR_PlayerFactionAffiliationComponent affiliation = SCR_PlayerFactionAffiliationComponent.Cast(player.FindComponent(SCR_PlayerFactionAffiliationComponent));
		if (!desired || !affiliation) return;
		if (affiliation.GetAffiliatedFaction() != desired) { affiliation.RequestFaction(desired); return; }
		if (m_AICFFIAOccupancyVehicle != RplId.Invalid() && player.GetControlledEntity())
		{
			RplComponent vehicle = RplComponent.Cast(Replication.FindItem(m_AICFFIAOccupancyVehicle));
			if (!vehicle || CompartmentAccessComponent.GetVehicleIn(player.GetControlledEntity()) != vehicle.GetEntity() || m_bAICFFIAOccupancyReadback) return;
			m_bAICFFIAOccupancyReadback = true;
			Print(string.Format("[AICF][FIA_PROXIMITY_CLIENT_OCCUPANT] controlled=1 occupied=1 entity=%1", player.GetControlledEntity().GetID()));
			return;
		}
		if (m_bAICFFIAPlayerPositionSet && player.GetControlledEntity())
			player.GetControlledEntity().SetOrigin(m_vAICFFIAPlayerPosition);
		if (player.GetControlledEntity() && !m_bAICFFIAClientReadback)
		{
			m_bAICFFIAClientReadback = true;
			Print(string.Format("[AICF][FIA_PROXIMITY_CLIENT] connected=1 controlled=1 entity=%1", player.GetControlledEntity().GetID()));
		}
	}

	protected void AICF_FIAClientClose()
	{
		GetGame().GetCallqueue().Remove(AICF_FIAClientClose);
		GetGame().GetCallqueue().Remove(AICF_FIAClientTick);
		SCR_PlayerController player = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		Print(string.Format("[AICF][FIA_PROXIMITY_CLIENT] connected=%1 controlled=%2", player != null, player && player.GetControlledEntity() != null));
		GetGame().RequestClose();
	}

	override void OnGameEnd()
	{
		GetGame().GetCallqueue().Remove(AICF_FIAClientClose);
		GetGame().GetCallqueue().Remove(AICF_FIAClientTick);
		string mode;
		if (!Replication.IsServer() && System.GetCLIParam("aicfFIAProximityProbe", mode)) GetGame().RequestClose();
		super.OnGameEnd();
	}
}

// Isolated: controlled native result, actual callback/physical hop/approach/debit.
class AICF_RecruitmentShortProbeNode : SCR_AIProcessFailedMovementResult
{
	bool Inject(SCR_AIGroup group, int result, int handler, bool related)
	{
		OnInit(group);
		return AICF_HandleRecruitmentFailedMovement(result, handler, related, group.GetCurrentWaypoint().GetOrigin());
	}
}

class AICF_RecruitmentShortProbeState
{
	static bool s_bInjected;
	static bool s_bRejectPlacement;
	static bool s_bSubmitted;
	static bool s_bObserved;
	static bool s_bApproached;
	static bool s_bArrived;
	static bool s_bPaid;
	static int s_iChecks;
	static int s_iFailures;
	static int s_iCallbacks;
	static int s_iStarted;
	static int s_iSubmitted;
	static int s_iOriginalStarted;
	static int s_iPhase;
	static vector s_vBefore;
	static vector s_vAfter;
	static float s_fAfterDistance;
	static AICF_InfantryRecruitmentOrder s_Order;
	static ChimeraCharacter s_Actor;
	static ref AICF_RecruitmentMoveFailure s_Receipt;
	static AICF_InfantryRecruitmentOrder s_NegativeOrder;
	static ChimeraCharacter s_NegativeActor;
	static ref AICF_RecruitmentMoveFailure s_NegativeReceipt;

	static bool Enabled()
	{
		string flag;
		return System.GetCLIParam("aicfRecruitmentShortRecoveryProbe", flag) && flag == "1";
	}

	static void Check(string name, bool passed, string details = "")
	{
		s_iChecks++;
		if (!passed) s_iFailures++;
		Print(string.Format("[AICF][RECRUITMENT_SHORT_CASE] case=%1 passed=%2 details=[%3]", name, passed, details));
	}

	static void Callback(int result, IEntity vehicle, bool related, vector location)
	{
		s_iCallbacks++;
		Check("CAPTURE_BEFORE_NATIVE_CALLBACK", s_Order && s_Order.m_MoveFailure &&
			s_Order.m_MoveFailure.IsCapturedCurrent(s_Order) && result == EMoveError.STUCK && !vehicle && related);
	}

	static bool NoMutation(AICF_InfantryRecruitmentOrder order, AICF_RecruitmentMoveFailure receipt)
	{
		vector before = s_Actor.GetOrigin();
		bool submitted = receipt.TryRecover(order);
		return !submitted && s_Actor.GetOrigin() == before;
	}

	static void Negatives(AICF_InfantryRecruitmentOrder order, SCR_AIGroupUtilityComponent utility)
	{
		int handler = AIGroupMovementComponent.DEFAULT_HANDLER_ID;
		Check("UNKNOWN_NO_CAPTURE", !AICF_RecruitmentMoveFailure.Capture(order, utility, EMoveError.UNKNOWN, handler, true));
		Check("FOREIGN_HANDLER_NO_CAPTURE", !AICF_RecruitmentMoveFailure.Capture(order, utility, EMoveError.STUCK, handler + 71, true));
		Check("UNRELATED_NO_CAPTURE", !AICF_RecruitmentMoveFailure.Capture(order, utility, EMoveError.STUCK, handler, false));
		AICF_RecruitmentMoveFailure receipt = AICF_RecruitmentMoveFailure.Capture(order, utility, EMoveError.STUCK, handler, true);
		Check("CURRENT_NATIVE_MOVE_OWNER", receipt != null);
		if (!receipt) return;
		order.m_MoveFailure = receipt;
		bool previousFailed = order.m_bMovementFailed;
		order.m_bMovementFailed = true;
		order.m_iApproachRepairs++;
		order.m_iToken++;
		Check("STALE_TOKEN_NO_MUTATION", NoMutation(order, receipt)); order.m_iToken--;
		order.m_iGeneration++;
		Check("STALE_GENERATION_NO_MUTATION", NoMutation(order, receipt)); order.m_iGeneration--;
		order.m_iGraphRevision++;
		Check("STALE_GRAPH_NO_MUTATION", NoMutation(order, receipt)); order.m_iGraphRevision--;
		AICF_InfantryRecruitmentOrder foreign = new AICF_InfantryRecruitmentOrder();
		Check("FOREIGN_ORDER_NO_MUTATION", NoMutation(foreign, receipt));
		s_bRejectPlacement = true;
		Check("NO_SAFE_DESTINATION_NO_MUTATION", NoMutation(order, receipt));
		s_bRejectPlacement = false;
		order.m_iApproachRepairs--;
		order.m_bMovementFailed = previousFailed;
		order.m_MoveFailure = null;
	}
}

modded class AICF_RecruitmentMoveFailure
{
	override protected bool SafeDestination(vector query, out vector destination)
	{
		if (AICF_RecruitmentShortProbeState.Enabled() && AICF_RecruitmentShortProbeState.s_bRejectPlacement)
			return false;
		return super.SafeDestination(query, destination);
	}
}

modded class AICF_InfantryRecruitmentService
{
	override protected string CheckApproach(AICF_InfantryRecruitmentOrder order, vector position, int now)
	{
		if (AICF_RecruitmentShortProbeState.Enabled() && AICF_RecruitmentShortProbeState.s_bPaid &&
			order.m_Slot.GetSlotId() == 1 && order.m_Faction.GetFactionKey() == "US" &&
			!AICF_RecruitmentShortProbeState.s_NegativeReceipt)
		{
			AICF_RecruitmentMoveFailure secondary = AICF_RecruitmentMoveFailure.Capture(order, order.m_Group.GetGroupUtilityComponent(),
				EMoveError.UNREACHABLE, AIGroupMovementComponent.DEFAULT_HANDLER_ID, true);
			if (secondary)
			{
				AICF_RecruitmentShortProbeState.s_NegativeOrder = order;
				AICF_RecruitmentShortProbeState.s_NegativeActor = ChimeraCharacter.Cast(AICF_GroupRuntime.ResolveAliveLeader(order.m_Group));
				AICF_RecruitmentShortProbeState.s_NegativeReceipt = secondary;
			}
		}
		if (AICF_RecruitmentShortProbeState.Enabled() && order.m_Slot.GetSlotId() == 0 &&
			order.m_Faction.GetFactionKey() == "US" && !AICF_RecruitmentShortProbeState.s_bInjected)
		{
			SCR_AIGroupUtilityComponent utility = order.m_Group.GetGroupUtilityComponent();
			AICF_RecruitmentMoveFailure valid = AICF_RecruitmentMoveFailure.Capture(order, utility,
				EMoveError.STUCK, AIGroupMovementComponent.DEFAULT_HANDLER_ID, true);
			if (valid && vector.DistanceXZ(position, order.m_vPosition) > 48)
			{
				AICF_RecruitmentShortProbeState.s_Order = order;
				AICF_RecruitmentShortProbeState.s_Actor = ChimeraCharacter.Cast(AICF_GroupRuntime.ResolveAliveLeader(order.m_Group));
				AICF_RecruitmentShortProbeState.Negatives(order, utility);
				AICF_RecruitmentShortProbeState.s_vBefore = AICF_RecruitmentShortProbeState.s_Actor.GetOrigin();
				AICF_RecruitmentShortProbeState.s_iOriginalStarted = order.m_iStartedAtMs;
				utility.GetOnMoveFailed().Insert(AICF_RecruitmentShortProbeState.Callback);
				AICF_RecruitmentShortProbeNode node = new AICF_RecruitmentShortProbeNode();
				bool handled = node.Inject(order.m_Group, EMoveError.STUCK, AIGroupMovementComponent.DEFAULT_HANDLER_ID, true);
				utility.GetOnMoveFailed().Remove(AICF_RecruitmentShortProbeState.Callback);
				AICF_RecruitmentShortProbeState.Check("NATIVE_CALLBACK_ONCE", handled && AICF_RecruitmentShortProbeState.s_iCallbacks == 1);
				AICF_RecruitmentShortProbeState.s_Receipt = order.m_MoveFailure;
				AICF_RecruitmentShortProbeState.s_bInjected = true;
			}
		}
		string result = super.CheckApproach(order, position, now);
		if (AICF_RecruitmentShortProbeState.Enabled() && order == AICF_RecruitmentShortProbeState.s_Order &&
			!AICF_RecruitmentShortProbeState.s_bSubmitted && order.m_bHiddenRecoveryPending)
		{
			AICF_RecruitmentShortProbeState.s_bSubmitted = true;
			AICF_RecruitmentShortProbeState.s_iSubmitted = now;
			AICF_RecruitmentShortProbeState.Check("ABSOLUTE_DEADLINE_UNCHANGED", order.m_iStartedAtMs == AICF_RecruitmentShortProbeState.s_iOriginalStarted);
			AICF_RecruitmentShortProbeState.Check("REPAIR_BUDGET_TWO", order.m_iApproachRepairs == 1);
			AICF_RecruitmentShortProbeState.Check("RECEIPT_CONSUMED_ONCE", AICF_RecruitmentShortProbeState.s_Receipt &&
				!AICF_RecruitmentShortProbeState.s_Receipt.TryRecover(order));
		}
		return result;
	}

	override protected string Tick(AICF_InfantryRecruitmentOrder order, bool graphReady)
	{
		int before = AICF_GroupRuntime.CountAliveAgents(order.m_Group);
		float stock = 0;
		if (order.m_Base) stock = order.m_Base.GetSupplies();
		string result = super.Tick(order, graphReady);
		if (AICF_RecruitmentShortProbeState.Enabled() && order == AICF_RecruitmentShortProbeState.s_Order)
		{
			if (!AICF_RecruitmentShortProbeState.s_bArrived && order.m_iArrivedAtMs > 0)
			{
				AICF_RecruitmentShortProbeState.s_bArrived = true;
				AICF_RecruitmentShortProbeState.Check("REAL_ARRIVAL", order.IsPhysicallyPresent() && AICF_RecruitmentShortProbeState.s_bApproached);
			}
			if (!AICF_RecruitmentShortProbeState.s_bPaid && before == 1 && AICF_GroupRuntime.CountAliveAgents(order.m_Group) == 2)
			{
				AICF_RecruitmentShortProbeState.s_bPaid = true;
				AICF_RecruitmentShortProbeState.Check("PAID_RECRUIT_JOIN", AICF_RecruitmentShortProbeState.s_bArrived &&
					order.m_Base.GetSupplies() < stock && order.IsPhysicallyPresent(), string.Format("stock_before=%1 stock_after=%2", stock, order.m_Base.GetSupplies()));
			}
		}
		return result;
	}
}

modded class AICF_MatchController
{
	protected SCR_CampaignBuildingCompositionComponent m_RSPBuilding;
	protected bool m_bRSPFinished;
	protected Vehicle m_RSPVehicle;
	protected int m_iRSPNegativeStep;

	override protected void Update()
	{
		if (!AICF_RecruitmentShortProbeState.Enabled() || !m_bRosterReady || m_bStopped)
		{
			super.Update(); return;
		}
		int now = System.GetTickCount();
		if (!AICF_RecruitmentShortProbeState.s_iStarted)
		{
			AICF_RecruitmentShortProbeState.s_iStarted = now;
			m_Construction.Stop();
			for (int i; i < m_USState.GetSlotCount(); i++)
			{
				m_USState.GetSlot(i).SetDesiredSize(1);
				m_USSRState.GetSlot(i).SetDesiredSize(1);
			}
			m_USState.GetSlot(0).SetDesiredSize(2);
			AICF_RSPPlace();
		}
		if (m_RSPBuilding && !m_RSPBuilding.IsCompositionSpawned() && m_RSPBuilding.GetCompositionLayout())
		{
			SCR_CampaignBuildingLayoutComponent layout = m_RSPBuilding.GetCompositionLayout();
			layout.AddBuildingValue(layout.GetToBuildValue());
		}
		AICF_InfantryRecruitmentOrder order = AICF_RecruitmentShortProbeState.s_Order;
		ChimeraCharacter actor = AICF_RecruitmentShortProbeState.s_Actor;
		if (AICF_RecruitmentShortProbeState.s_bSubmitted && actor)
		{
			if (!AICF_RecruitmentShortProbeState.s_bObserved && now - AICF_RecruitmentShortProbeState.s_iSubmitted >= 1000)
			{
				float displacement = vector.Distance(actor.GetOrigin(), AICF_RecruitmentShortProbeState.s_vBefore);
				AICF_RecruitmentShortProbeState.Check("PHYSICAL_SHORT_HOP", displacement >= 2 && displacement <= 8,
					string.Format("actor=%1 before=%2 after=%3 displacement_m=%4", actor.GetID(), AICF_RecruitmentShortProbeState.s_vBefore, actor.GetOrigin(), displacement));
				AICF_RecruitmentShortProbeState.s_vAfter = actor.GetOrigin();
				AICF_RecruitmentShortProbeState.s_fAfterDistance = vector.DistanceXZ(actor.GetOrigin(), order.m_vPosition);
				AICF_RecruitmentShortProbeState.s_bObserved = true;
			}
			if (AICF_RecruitmentShortProbeState.s_bObserved && !AICF_RecruitmentShortProbeState.s_bApproached &&
				vector.DistanceXZ(actor.GetOrigin(), order.m_vPosition) < AICF_RecruitmentShortProbeState.s_fAfterDistance - 5)
			{
				AICF_RecruitmentShortProbeState.s_bApproached = true;
				AICF_RecruitmentShortProbeState.Check("CONTINUED_NATIVE_APPROACH", vector.DistanceXZ(actor.GetOrigin(), AICF_RecruitmentShortProbeState.s_vAfter) >= 5);
			}
		}
		if (AICF_RecruitmentShortProbeState.s_bPaid && m_iRSPNegativeStep < 6) AICF_RSPNegatives();
		if (m_iRSPNegativeStep == 6 || now - AICF_RecruitmentShortProbeState.s_iStarted >= 240000)
		{
			if (m_iRSPNegativeStep != 6) AICF_RecruitmentShortProbeState.Check("TIMEOUT", false);
			AICF_RSPFinish(); return;
		}
		super.Update();
	}

	protected bool AICF_RSPNegativeAttempt()
	{
		AICF_InfantryRecruitmentOrder order = AICF_RecruitmentShortProbeState.s_NegativeOrder;
		AICF_RecruitmentMoveFailure receipt = AICF_RecruitmentShortProbeState.s_NegativeReceipt;
		ChimeraCharacter actor = AICF_RecruitmentShortProbeState.s_NegativeActor;
		vector before = actor.GetOrigin();
		order.m_MoveFailure = receipt;
		bool oldFailed = order.m_bMovementFailed;
		order.m_bMovementFailed = true;
		order.m_iApproachRepairs++;
		bool submitted = receipt.TryRecover(order);
		order.m_iApproachRepairs--;
		order.m_bMovementFailed = oldFailed;
		order.m_MoveFailure = null;
		return !submitted && actor.GetOrigin() == before;
	}

	protected void AICF_RSPNegatives()
	{
		if (m_iRSPNegativeStep == 0)
		{
			AICF_GroupSlot slot = m_USState.GetSlot(1);
			slot.SetDesiredSize(2);
			string reason;
			if (m_InfantryRecruitment.RequestPlayerRecruitment(slot, m_USFaction, reason)) m_iRSPNegativeStep = 1;
			return;
		}
		ChimeraCharacter actor = AICF_RecruitmentShortProbeState.s_NegativeActor;
		if (!actor || !AICF_RecruitmentShortProbeState.s_NegativeReceipt) return;
		CompartmentAccessComponent access = actor.GetCompartmentAccessComponent();
		if (!access) return;
		if (m_iRSPNegativeStep == 1)
		{
			EntitySpawnParams params = new EntitySpawnParams(); params.TransformMode = ETransformMode.WORLD;
			params.Transform[3] = actor.GetOrigin() + "3 0 0";
			m_RSPVehicle = Vehicle.Cast(GetGame().SpawnEntityPrefab(Resource.Load("{259EE7B78C51B624}Prefabs/Vehicles/Wheeled/UAZ469/UAZ469.et"), GetGame().GetWorld(), params));
			BaseCompartmentSlot seat;
			if (m_RSPVehicle) seat = access.FindFreeCompartment(m_RSPVehicle, ECompartmentType.CARGO);
			if (!seat || !access.GetInVehicle(m_RSPVehicle, seat, true, -1, ECloseDoorAfterActions.CLOSE_DOOR, true))
			{
				AICF_RecruitmentShortProbeState.Check("VEHICLE_SETUP", false); m_iRSPNegativeStep = 6; return;
			}
			m_iRSPNegativeStep = 2; return;
		}
		if (m_iRSPNegativeStep == 2 && actor.IsInVehicle() && access.IsInCompartment())
		{
			AICF_RecruitmentShortProbeState.Check("VEHICLE_NO_MUTATION", AICF_RSPNegativeAttempt(), "actual_in_vehicle=1 submitted=0");
			access.GetOutVehicle(EGetOutType.TELEPORT, -1, ECloseDoorAfterActions.CLOSE_DOOR, false);
			m_iRSPNegativeStep = 3; return;
		}
		if (m_iRSPNegativeStep == 3 && !actor.IsInVehicle() && !access.IsInCompartment() && !access.IsGettingOut())
		{
			AICF_InfantryRecruitmentOrder order = AICF_RecruitmentShortProbeState.s_NegativeOrder;
			AICF_RecruitmentMoveFailure fresh = AICF_RecruitmentMoveFailure.Capture(order, order.m_Group.GetGroupUtilityComponent(),
				EMoveError.UNREACHABLE, AIGroupMovementComponent.DEFAULT_HANDLER_ID, true);
			if (!fresh) return;
			AICF_RecruitmentShortProbeState.s_NegativeReceipt = fresh;
			SCR_CharacterDamageManagerComponent damage = SCR_CharacterDamageManagerComponent.Cast(actor.FindComponent(SCR_CharacterDamageManagerComponent));
			if (!damage) { AICF_RecruitmentShortProbeState.Check("INCAP_SETUP", false); m_iRSPNegativeStep = 6; return; }
			damage.ForceUnconsciousness(0); m_iRSPNegativeStep = 4; return;
		}
		if (m_iRSPNegativeStep == 4 && actor.GetCharacterController() && actor.GetCharacterController().GetLifeState() == ECharacterLifeState.INCAPACITATED)
		{
			AICF_RecruitmentShortProbeState.Check("INCAP_NO_MUTATION", AICF_RSPNegativeAttempt(), "actual_incapacitated=1 submitted=0 revive=0 heal=0");
			m_iRSPNegativeStep = 6;
		}
	}

	protected void AICF_RSPPlace()
	{
		SCR_CampaignMilitaryBaseComponent base = m_USFaction.GetMainBase();
		if (!base || !base.GetMasterProvider()) return;
		base.AddSupplies(base.GetSuppliesMax() - base.GetSupplies());
		IEntity leader = AICF_GroupRuntime.ResolveAliveLeader(m_USState.GetSlot(0).GetGroup());
		if (!leader) return;
		vector position = leader.GetOrigin() + "70 0 30";
		position[1] = GetGame().GetWorld().GetSurfaceY(position[0], position[2]);
		ResourceName prefab = AICF_ContentProfile.GetActive().GetConstructionPrefab("US", AICF_EConstructionType.SMALL_BARRACKS);
		EntitySpawnParams params = new EntitySpawnParams(); params.TransformMode = ETransformMode.WORLD; params.Transform[3] = position;
		SCR_EditorLinkComponent.IgnoreSpawning(true);
		IEntity entity = GetGame().SpawnEntityPrefabEx(prefab, false, params: params);
		SCR_EditorLinkComponent.IgnoreSpawning(false);
		if (!entity) return;
		FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(entity.FindComponent(FactionAffiliationComponent));
		if (affiliation) affiliation.SetAffiliatedFaction(m_USFaction);
		m_RSPBuilding = SCR_CampaignBuildingCompositionComponent.Cast(entity.FindComponent(SCR_CampaignBuildingCompositionComponent));
		if (m_RSPBuilding) m_RSPBuilding.SetProviderEntity(base.GetMasterProvider().GetOwner());
	}

	protected void AICF_RSPFinish()
	{
		if (m_bRSPFinished) return;
		m_bRSPFinished = true;
		Print(string.Format("[AICF][RECRUITMENT_SHORT_FINISHED] checks=%1 failures=%2", AICF_RecruitmentShortProbeState.s_iChecks, AICF_RecruitmentShortProbeState.s_iFailures));
		Stop(true);
		GetGame().GetCallqueue().CallLater(AICF_RSPClose, 1000, false);
	}
	protected void AICF_RSPClose() { GetGame().RequestClose(); }
	override protected void Stop(bool cleanupEntities)
	{
		GetGame().GetCallqueue().Remove(AICF_RSPClose);
		super.Stop(cleanupEntities);
		if (m_RSPBuilding && m_RSPBuilding.GetOwner()) SCR_EntityHelper.DeleteEntityAndChildren(m_RSPBuilding.GetOwner());
		m_RSPBuilding = null;
		ChimeraCharacter actor = AICF_RecruitmentShortProbeState.s_NegativeActor;
		if (m_RSPVehicle && (!actor || CompartmentAccessComponent.GetVehicleIn(actor) != m_RSPVehicle))
			SCR_EntityHelper.DeleteEntityAndChildren(m_RSPVehicle);
		m_RSPVehicle = null;
	}
}

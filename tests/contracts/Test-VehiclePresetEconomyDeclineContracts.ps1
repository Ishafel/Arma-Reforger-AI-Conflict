param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$prefix=Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict'
$receipt=[IO.File]::ReadAllText((Join-Path $prefix 'Economy/AICF_VehiclePresetEconomyDecision.c'))
$economy=[IO.File]::ReadAllText((Join-Path $prefix 'Economy/AICF_EconomySystem.c'))
$trip=[IO.File]::ReadAllText((Join-Path $prefix 'State/Vehicles/AICF_TransportTrip.c'))
$flow=[IO.File]::ReadAllText((Join-Path $prefix 'Vehicles/AICF_VehicleAcquisitionFlow.c'))
$fixture=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'tests/fixtures/AICF_VehiclePresetEconomyDeclineProbe.c'))
$phaseStates=[IO.File]::ReadAllText((Join-Path $prefix 'State/Vehicles/AICF_VehiclePhaseStates.c'))
$requestSource=$phaseStates.Substring($phaseStates.IndexOf('class AICF_VehicleRequestState'))
foreach($guard in @('trip = super.Create(assignment, nowMs, absoluteDeadlineMs, causationId)','AICF_PresetEconomyProbe.s_Operations++;','AICF_TransportTrip currentTrip = m_VBCoordinator.GetTrip(slot);','!AICF_PresetEconomyProbe.SameObservedTrip(m_EconomyProbeOriginal, currentTrip)','now - m_iEconomyProbeTerminalAt < 20000','currentTrip == trip && active == 1','current_trip_exact=%6')){if(!$fixture.Contains($guard)){throw "Actual creation/same-intent boundary missing: $guard"}}
function Body($s,$name){Get-AICFMethodBody ([pscustomobject]@{Source=$s;Code=(ConvertTo-AICFCodeText $s)}) $name}
function FixtureRules($s){
 $maintain=Body $s 'AICF_EconomyMaintainFootOrder'
 foreach($guard in @('m_Config.GetCommanderIntervalMs()','m_iEconomyFootNextReconcile = nowMs + interval','m_OrderPlanner.ReconcileStrategicOrder(slot, m_VBFaction, m_ObjectiveGraph, m_TargetSelector,','m_Config.GetRoleMinimumDwellMs()','!AICF_EconomyFootContextCurrent(slot)')){if(!$maintain.Contains($guard)){'NORMAL_PLANNING_OWNER_WITH_UNCHANGED_CONTEXT_AND_CADENCE';break}}
 if($maintain -match 'AssignPlayerPointOrder|CreateWaypoint|CommitTransition|ClearLatched'){'NO_FAKE_ORDER_OR_PHASE_PROGRESS'}
}
if(@(FixtureRules $fixture).Count){throw 'Fixture infantry owner/cadence invalid'}
foreach($guard in @('decision.MatchesIdentity(trip)','LIVE_TYPED_RECEIPT_BEFORE_HANDOFF','AICF_PresetEconomyProbe.s_DeclineReceipt = decision;','AICF_PresetEconomyProbe.TerminalReceiptSame(')){if(!$fixture.Contains($guard)){throw 'Typed live-before-handoff capture missing'}}
foreach($guard in @('m_OrderPlanner.ReconcileStrategicOrder(slot, m_VBFaction, m_ObjectiveGraph, m_TargetSelector,','m_iEconomyFootNextReconcile = nowMs + interval')){if(!@(FixtureRules ($fixture.Replace($guard,'MUTANT'))).Count){throw 'Fixture owner/cadence mutation escaped'}}
function Cs($s){$s=[regex]::Replace($s,'\bbase\b','spawnBase');$s=$s.Replace('AICF_EconomySystem economy;','AICF_EconomySystem economy = null;').Replace('EntityID.INVALID','-1L');return $s}
function Rules($f,$t,$e){
 $commit=Body $f 'ProcessSpawnCommit';$early=Body $f 'InspectPresetEconomyBeforeLease';$purchase=Body $f 'BindAcceptedCandidate'
 if($commit.IndexOf('InspectPresetEconomyBeforeLease(') -lt $commit.IndexOf('plan.ObserveStaging(') -or $commit.IndexOf('InspectPresetEconomyBeforeLease(') -gt $commit.IndexOf('EnsureReservedLease(') -or $commit.IndexOf('InspectPresetEconomyBeforeLease(') -gt $commit.IndexOf('requestState.BeginAttempt(')){'PREFLIGHT_AFTER_STAGING_BEFORE_SIDE_EFFECTS'}
 foreach($guard in @('AICF_VehiclePresetEconomyDecision.IsOwnUnusedReservation(trip, fleet, existingLease, existingLease.GetLeaseGeneration())','AICF_EVehiclePresetEconomyDecision.INVALID_CONTEXT','!economy.IsVehiclePresetPreflightCurrent(trip, faction, decision)','!trip.RecordVehiclePresetEconomyDecline(decision)','"VEHICLE_PRESET_ECONOMY_DECLINED", nowMs, false')){if(!$early.Contains($guard)){'TYPED_NO_ASSET_ONLY_EXPECTED_DECLINE';break}}
 if(!$purchase.Contains('"INSUFFICIENT_SUPPLIES", nowMs, true') -or !$purchase.Contains('RollbackUnacceptedCandidate(candidate)')){'POSTSPAWN_PAYMENT_FAILURE_REMAINS_FAILURE'}
 if((Body $t 'RecordVehiclePresetEconomyDecline') -match '\b(?:Clear\w*|Reset\w*|MarkComplete|CommitTransition)\s*\('){'NO_FAILURE_OR_PHASE_RESET'}
 if((Body $e 'InspectVehiclePresetPreflight') -match '\.(AddSupplies|Spawn|Reserve|Purchase)\s*\('){'QUOTE_READONLY'}
}
if(@(Rules $flow $trip $economy).Count){throw "Economy decline source: $(Rules $flow $trip $economy)"}
foreach($value in @('AICF_VehiclePresetEconomyDecision.IsOwnUnusedReservation(trip, fleet, existingLease, existingLease.GetLeaseGeneration())','!economy.IsVehiclePresetPreflightCurrent(trip, faction, decision)','!trip.RecordVehiclePresetEconomyDecline(decision)','"VEHICLE_PRESET_ECONOMY_DECLINED", nowMs, false')){
 if(!@(Rules ($flow.Replace($value,'false')) $trip $economy).Count){throw "Source mutant escaped: $value"}
}
$code=@'
using System;using System.Collections.Generic;
namespace AICFDeclineActual {
public class N {public static bool operator !(N a){return ReferenceEquals(a,null);}public static implicit operator bool(N a){return !ReferenceEquals(a,null);}}
public static class E {public static bool IsEmpty(this string s){return String.IsNullOrEmpty(s);}}
public static class Replication {public static bool Server=true;public static bool IsServer(){return Server;}}
public enum AICF_EVehiclePresetEconomyDecision {INVALID_CONTEXT,AFFORDABLE,INSUFFICIENT_AVAILABLE_SUPPLIES}
public enum AICF_ETransportTripPhase {SPAWN_COMMIT,FALLBACK,TRANSIT}
public class IEntity:N {public long Id=5;public string Rpl="BR";public bool Authority=true;public long GetID(){return Id;}}
public class W:N {public IEntity Owner;public IEntity FindEntityByID(long id){return Owner!=null&&Owner.Id==id?Owner:null;}}
public class G:N {public W World=new W();public W GetWorld(){return World;}public static G Instance=new G();}
public static class AICF_VehicleBehaviorBinding {public static string ComponentRpl(IEntity e){return e.Rpl;}}
public static class AICF_VehicleBoardingMutationFence {public static bool IsAuthoritativeReplicatedEntity(IEntity e){return e.Authority;}}
public class SCR_CampaignMilitaryBaseComponent:N {public IEntity Owner=new IEntity();public int Stock=100;public IEntity GetOwner(){return Owner;}public int GetSupplies(){return Stock;}}
public class SCR_CampaignFaction:N {public string Key="US";public string GetFactionKey(){return Key;}}
public class P:N {public string m_sPrefab="CAR";}
public class Selection:N {public SCR_CampaignMilitaryBaseComponent m_Base;}
public class AICF_VehicleSpawnPlan:N {public Selection Selection=new Selection();public Selection GetSelection(){return Selection;}}
public class AICF_VehicleRequestState:N {public int Generation=1,TotalAttempts;public int m_iRequestGeneration{get{return Generation;}set{Generation=value;}}public int m_iTotalAttemptCount{get{return TotalAttempts;}set{TotalAttempts=value;}}public AICF_VehicleSpawnPlan m_SpawnPlan{get{return Plan;}set{Plan=value;}}public int m_iStartedAtMs,m_iAbsoluteDeadlineMs,m_iNextAttemptAtMs,m_iAttemptCount,m_iMaximumAttempts,m_iWaitingStartedAtMs,m_iTotalWaitingStartedAtMs,m_iContextResetCount,m_iCohesionStartedAtMs;public string m_sContextResetReason,m_sLastFailureReason;public float m_fCohesionSpreadMeters;public bool m_bCohesionRecoveryAttempted;public int ClearCalls;public int QuotaCancels;public void ClearQuotaCreation(){QuotaCancels++;}public void ClearNoRangeObservation(){ClearCalls++;}public void Reset() ACTUAL_REQUEST_RESET
public int GetTotalAttemptCount(){return TotalAttempts;}public AICF_VehicleSpawnPlan Plan=new AICF_VehicleSpawnPlan();public int GetRequestGeneration(){return Generation;}public AICF_VehicleSpawnPlan GetSpawnPlan(){return Plan;}}
public class AICF_TransportTrip:N {public bool Valid=true,Current=true,Lease,Debited;public AICF_VehicleLease ActualLease;public int Group=2,Trip=3,Preset=4;public string Operation="OP";public P Recipe=new P();public AICF_VehicleRequestState Request=new AICF_VehicleRequestState();public AICF_VehicleRequestState m_RequestState {get{return Request;}}public AICF_ETransportTripPhase Phase;public AICF_VehiclePresetEconomyDecision m_VehiclePresetEconomyDecline;public bool IsValid(){return Valid;}public bool IsVehiclePresetCurrent(){return Current;}public P GetVehiclePreset(){return Recipe;}public string GetOperationId(){return Operation;}public int GetGroupGeneration(){return Group;}public int GetTripGeneration(){return Trip;}public int GetVehiclePresetRevision(){return Preset;}public AICF_VehicleRequestState GetRequestState(){return Request;}public string GetFactionKey(){return "US";}public int GetSlotId(){return 0;}public AICF_VehicleLease GetLease(){return ActualLease;}public bool HasLease(){return Lease||ActualLease!=null;}public bool IsVehiclePresetDebited(){return Debited;}public AICF_ETransportTripPhase GetPhase(){return Phase;}public bool RecordVehiclePresetEconomyDecline(AICF_VehiclePresetEconomyDecision decision) RECORD}
public class Network:N {public bool Operational=true;public bool IsOperationalOwnedBase(SCR_CampaignMilitaryBaseComponent b,SCR_CampaignFaction f){return Operational;}}
public class Adapter:N {public string Rejection="";public string GetSpawnRejectionReason(SCR_CampaignMilitaryBaseComponent b,SCR_CampaignFaction f){return Rejection;}}
public class AICF_VehiclePresetCatalog {public static int Quote=475;public AICF_VehiclePresetCatalog(SCR_CampaignFaction f){}public int Cost(string p){return Quote;}}
public class AICF_VehiclePresetEconomyDecision:N {
protected AICF_TransportTrip m_Trip;protected AICF_FactionFleet m_Fleet;protected AICF_VehicleLease m_ReservedLease;protected bool m_bHadReservedLease;protected int m_iReservedLeaseGeneration;protected AICF_VehicleRequestState m_Request;protected AICF_VehicleSpawnPlan m_Plan;protected SCR_CampaignMilitaryBaseComponent m_Base;protected IEntity m_BaseOwner;protected long m_BaseId;protected string m_sBaseRpl,m_sOperation,m_Prefab;protected int m_iGroupGeneration,m_iTripGeneration,m_iRequestGeneration,m_iPresetRevision,m_iStock,m_iObligations,m_iCost;protected AICF_EVehiclePresetEconomyDecision m_Status;
public int CapturedRequestGeneration(){return m_iRequestGeneration;}public AICF_VehicleRequestState CapturedRequest(){return m_Request;}
public static AICF_EVehiclePresetEconomyDecision ClassifyQuote(int cost,int stock,int obligations) CLASSIFY
public AICF_VehiclePresetEconomyDecision(AICF_TransportTrip trip,SCR_CampaignMilitaryBaseComponent spawnBase,int cost,int stock,int obligations,AICF_EVehiclePresetEconomyDecision status,AICF_FactionFleet fleet=null) CONSTRUCTOR
public static bool IsOwnUnusedReservation(AICF_TransportTrip trip,AICF_FactionFleet fleet,AICF_VehicleLease lease,int leaseGeneration) UNUSED_RESERVATION
public bool HasCurrentLeaseFence(AICF_TransportTrip trip) LEASE_FENCE
public G GetGame(){return G.Instance;}public bool MatchesIdentity(AICF_TransportTrip trip) MATCH
public bool IsCurrentPreflight(AICF_TransportTrip trip) CURRENT
public AICF_EVehiclePresetEconomyDecision GetStatus(){return m_Status;}public SCR_CampaignMilitaryBaseComponent GetBase(){return m_Base;}public int GetStock(){return m_iStock;}public int GetObligations(){return m_iObligations;}public int GetCost(){return m_iCost;}public AICF_FactionFleet GetFleet(){return m_Fleet;}public bool HadReservedLease(){return m_bHadReservedLease;}public int GetReservedLeaseGeneration(){return m_iReservedLeaseGeneration;}public string Details(){return "TYPED";}
}
public class AICF_EconomySystem:N {public bool Enabled=true,m_bGraphContextReady=true;public N m_Graph=new N();public Network m_SupplyNetwork=new Network();public Adapter m_ConflictAdapter=new Adapter();public int Obligations;public bool IsEnabled(){return Enabled;}public int LogisticsObligations(SCR_CampaignMilitaryBaseComponent b){return Obligations;}
public AICF_VehiclePresetEconomyDecision InspectVehiclePresetPreflight(AICF_TransportTrip trip,SCR_CampaignFaction faction,SCR_CampaignMilitaryBaseComponent spawnBase,AICF_FactionFleet fleet) INSPECT
public bool IsVehiclePresetPreflightCurrent(AICF_TransportTrip trip,SCR_CampaignFaction faction,AICF_VehiclePresetEconomyDecision decision) RECHECK
}
public class AICF_MatchController:N {public static AICF_MatchController Active;public AICF_EconomySystem Economy=new AICF_EconomySystem();public static AICF_MatchController GetActiveController(){return Active;}public AICF_EconomySystem VehiclePresetEconomy(){return Economy;}}
public enum AICF_EVehicleLeaseState {RESERVED,ACTIVE,RELEASED}
public class AICF_VehicleLease:N {public int Gen=1,VehicleGen,Slot,Group=2,Trip=3;public long Entity=-1;public string Faction="US",EntityText="",Rpl="",Prefab="",Lifecycle="";public AICF_EVehicleLeaseState State;public IEntity Vehicle;public N Cleanup;public bool Physical;public int GetLeaseGeneration(){return Gen;}public int GetVehicleGeneration(){return VehicleGen;}public long GetEntityId(){return Entity;}public string GetEntityIdString(){return EntityText;}public string GetRplId(){return Rpl;}public string GetPrefab(){return Prefab;}public string GetVehicleLifecycleId(){return Lifecycle;}public AICF_EVehicleLeaseState GetState(){return State;}public bool HasPhysicalAsset(){return Physical;}public IEntity GetVehicle(){return Vehicle;}public N GetCleanupSnapshot(){return Cleanup;}public bool MatchesTripIdentity(string f,int s,int g,int t){return Faction==f&&Slot==s&&Group==g&&Trip==t;}}
public class AICF_FactionFleet:N {public string Key="US";public AICF_VehicleLease Registered;public string GetFactionKey(){return Key;}public bool OwnsVehicleLease(AICF_VehicleLease l){return Registered!=null&&Registered==l;}public AICF_VehicleLease FindLeaseForSlot(int s){return Registered!=null&&Registered.Slot==s?Registered:null;}}
public class AICF_TripOutcome:N {public bool Fail;public string Reason;}
public static class AICF_Stage4Diagnostics {public static int Events;public static void Info(string e,string d){Events++;}}
public class Flow {public int Ends;public AICF_TripOutcome FailClosed(AICF_TransportTrip t,string r,string c){return new AICF_TripOutcome{Fail=true,Reason=r};}public AICF_TripOutcome EndRequest(AICF_TransportTrip t,AICF_FactionFleet f,AICF_VehicleRequestState r,string reason,int now,bool fail){Ends++;return new AICF_TripOutcome{Fail=fail,Reason=reason};}public AICF_TripOutcome InspectPresetEconomyBeforeLease(AICF_TransportTrip trip,AICF_FactionFleet fleet,SCR_CampaignFaction faction,AICF_VehicleRequestState requestState,AICF_VehicleSpawnPlan plan,int nowMs) EARLY
public AICF_TripOutcome BroadLeaseBypassMutant(AICF_TransportTrip trip,AICF_FactionFleet fleet,SCR_CampaignFaction faction,AICF_VehicleRequestState requestState,AICF_VehicleSpawnPlan plan,int nowMs) BROAD_BYPASS
}
public struct Vec {public double X,Y,Z;public Vec(double x,double y,double z){X=x;Y=y;Z=z;}public static bool operator ==(Vec a,Vec b){return a.X==b.X&&a.Y==b.Y&&a.Z==b.Z;}public static bool operator !=(Vec a,Vec b){return !(a==b);}public override bool Equals(object x){return x is Vec&&(Vec)x==this;}public override int GetHashCode(){return X.GetHashCode();}}
public static class Observed {public static bool SameObservedTrip(AICF_TransportTrip original,AICF_TransportTrip current) OBSERVED
public static bool FootContextSame(bool ownerCurrent,bool tripCurrent,bool groupCurrent,int generation,int currentGeneration,int intent,int currentIntent,int kind,int currentKind,Vec target,Vec currentTarget,bool sameBase) FOOT_CONTEXT
public static bool FootReconcileDue(bool eligible,int nowMs,int nextAtMs,int intervalMs) FOOT_DUE
public static bool TerminalReceiptSame(bool capturedCurrent,bool sameTrip,bool sameReceipt,bool sameDetails,bool sameRequest,int group,int currentGroup,int trip,int currentTrip,int request,int currentRequest,int preset,int currentPreset,string operation,string currentOperation) TERMINAL_RECEIPT
}
public static class Run {public static int Execute(){int n=0;Action<bool,string>check=(b,s)=>{if(!b)throw new Exception(s);n++;};var f=new SCR_CampaignFaction();Func<AICF_TransportTrip> fresh=()=>{var t=new AICF_TransportTrip();var b=new SCR_CampaignMilitaryBaseComponent();t.Request.Plan.Selection.m_Base=b;G.Instance.World.Owner=b.Owner;AICF_MatchController.Active=new AICF_MatchController();return t;};
foreach(var input in new int[][]{new[]{475,474,0,2},new[]{475,475,0,1},new[]{475,500,26,2},new[]{475,500,25,1},new[]{0,500,0,0},new[]{-1,500,0,0},new[]{1,-1,0,0},new[]{1,0,-1,0},new[]{int.MaxValue,int.MaxValue,0,1},new[]{1,0,int.MaxValue,2}}){check((int)AICF_VehiclePresetEconomyDecision.ClassifyQuote(input[0],input[1],input[2])==input[3],"quote boundary");}
var observed=fresh();check(Observed.SameObservedTrip(observed,observed),"same retained native trip");check(!Observed.SameObservedTrip(observed,null),"absent current trip rejected");check(!Observed.SameObservedTrip(observed,fresh()),"replacement current trip rejected");var t0=fresh();var flow=new Flow();var o=flow.InspectPresetEconomyBeforeLease(t0,new AICF_FactionFleet(),f,t0.Request,t0.Request.Plan,10);check(o!=null&&!o.Fail&&t0.m_VehiclePresetEconomyDecline!=null&&!t0.Lease&&!t0.Debited&&flow.Ends==1,"typed insufficient terminal foot");check(!t0.RecordVehiclePresetEconomyDecline(t0.m_VehiclePresetEconomyDecline),"immutable once");
for(int mutation=0;mutation<9;mutation++){var target=new Vec(1,2,3);var actual=new Vec(1,2,3);if(mutation==7)actual.Z++;bool same=Observed.FootContextSame(mutation!=1,mutation!=2,mutation!=3,1,mutation==4?2:1,2,mutation==5?3:2,3,mutation==6?4:3,target,actual,mutation!=8);check(same==(mutation==0),"actual preserved foot owner/group/generation/intent/kind/target/base");}
check(Observed.FootReconcileDue(true,20,20,10),"normal cadence due");check(!Observed.FootReconcileDue(true,19,20,10),"no per-update forced repair");check(!Observed.FootReconcileDue(true,20,20,0),"invalid interval");check(!Observed.FootReconcileDue(false,20,20,10),"ineligible context");
for(int mutation=0;mutation<12;mutation++){check(Observed.TerminalReceiptSame(mutation!=1,mutation!=2,mutation!=3,mutation!=9,mutation!=10,1,mutation==4?2:1,2,mutation==5?3:2,mutation==11?0:3,mutation==6?4:0,4,mutation==7?5:4,"op",mutation==8?"other":"op")== (mutation==0),"actual immutable receipt boundary after handoff and normal request reset");}
t0.Phase=AICF_ETransportTripPhase.FALLBACK;t0.Request.Plan=null;check(t0.m_VehiclePresetEconomyDecline.MatchesIdentity(t0)&&!t0.m_VehiclePresetEconomyDecline.IsCurrentPreflight(t0),"plan-only cleanup with unchanged live generation is not actual terminal Reset");
var resetTrip=fresh();new Flow().InspectPresetEconomyBeforeLease(resetTrip,new AICF_FactionFleet(),f,resetTrip.Request,resetTrip.Request.Plan,10);var savedDecision=resetTrip.m_VehiclePresetEconomyDecline;var savedRequest=resetTrip.Request;check(savedDecision.MatchesIdentity(resetTrip)&&savedDecision.CapturedRequestGeneration()==1,"live typed capture before actual reset");resetTrip.Request.Reset();resetTrip.Phase=AICF_ETransportTripPhase.FALLBACK;check(resetTrip.Valid&&resetTrip.Request.Generation==0&&resetTrip.Request.Plan==null&&resetTrip.Request.ClearCalls==1&&!savedDecision.MatchesIdentity(resetTrip)&&!savedDecision.IsCurrentPreflight(resetTrip),"actual extracted Request.Reset invalidates live receipt matching without assignment change");check(savedDecision==resetTrip.m_VehiclePresetEconomyDecline&&savedDecision.CapturedRequest()==savedRequest&&savedDecision.CapturedRequestGeneration()==1,"immutable captured receipt epoch survives actual reset");
Action<Action<AICF_TransportTrip,AICF_EconomySystem>,string> invalid=(change,label)=>{var t=fresh();var eco=AICF_MatchController.Active.Economy;change(t,eco);var r=new Flow().InspectPresetEconomyBeforeLease(t,new AICF_FactionFleet(),f,t.Request,t.Request.Plan,10);check(r!=null&&r.Fail&&t.m_VehiclePresetEconomyDecline==null,label);};
invalid((t,e)=>e.m_bGraphContextReady=false,"graph unavailable");invalid((t,e)=>e.Enabled=false,"disabled");invalid((t,e)=>e.m_SupplyNetwork.Operational=false,"foreign or unsafe base");invalid((t,e)=>e.m_ConflictAdapter.Rejection="unsafe","native spawn rejection");invalid((t,e)=>e.Obligations=-1,"invalid obligations");invalid((t,e)=>t.Current=false,"stale preset");invalid((t,e)=>t.Valid=false,"invalid assignment");invalid((t,e)=>t.Request.TotalAttempts=1,"earlier spawn attempt cannot claim zero side effects");invalid((t,e)=>t.Phase=AICF_ETransportTripPhase.TRANSIT,"wrong phase");invalid((t,e)=>t.Request.Plan.Selection.m_Base.Owner.Rpl="","missing component Rpl");invalid((t,e)=>G.Instance.World.Owner=new IEntity(),"world identity swap");invalid((t,e)=>t.Request.Plan.Selection.m_Base.Owner.Authority=false,"proxy base");
var affordable=fresh();affordable.Request.Plan.Selection.m_Base.Stock=475;check(new Flow().InspectPresetEconomyBeforeLease(affordable,new AICF_FactionFleet(),f,affordable.Request,affordable.Request.Plan,10)==null&&affordable.m_VehiclePresetEconomyDecline==null,"affordable proceeds");
foreach(int bypass in new[]{0,1,2}){var t=fresh();var fleet=new AICF_FactionFleet();if(bypass==0){t.ActualLease=new AICF_VehicleLease{Physical=true,Vehicle=new IEntity(),State=AICF_EVehicleLeaseState.ACTIVE,VehicleGen=1};fleet.Registered=t.ActualLease;}if(bypass==1)t.Debited=true;if(bypass==2)t.Recipe=null;AICF_MatchController.Active=null;check(new Flow().InspectPresetEconomyBeforeLease(t,fleet,f,t.Request,t.Request.Plan,10)==null,"existing physical asset/paid/nonpreset bypass");}
Action<Action<AICF_TransportTrip>,string> stale=(change,label)=>{var t=fresh();var b=t.Request.Plan.Selection.m_Base;var d=AICF_MatchController.Active.Economy.InspectVehiclePresetPreflight(t,f,b,new AICF_FactionFleet());check(d.IsCurrentPreflight(t),"before mutation current");change(t);check(!d.IsCurrentPreflight(t),label);};
stale(t=>t.Group++,"group gen");stale(t=>t.Trip++,"trip gen");stale(t=>t.Preset++,"preset revision");stale(t=>t.Request.Generation++,"request generation");stale(t=>t.Operation="OTHER","operation");stale(t=>t.Request.Plan=new AICF_VehicleSpawnPlan(),"plan replacement");stale(t=>t.Request.Plan.Selection.m_Base.Owner.Id++,"base identity");stale(t=>t.Request.Plan.Selection.m_Base.Owner.Rpl="OTHER","base component Rpl");stale(t=>t.Request.Plan.Selection.m_Base.Stock++,"stock changed");stale(t=>t.Lease=true,"new lease");stale(t=>t.Debited=true,"new debit");
var changed=fresh();var ec=AICF_MatchController.Active.Economy;var q=ec.InspectVehiclePresetPreflight(changed,f,changed.Request.Plan.Selection.m_Base,new AICF_FactionFleet());ec.Obligations++;check(!ec.IsVehiclePresetPreflightCurrent(changed,f,q),"obligations changed");ec.Obligations--;f.Key="USSR";check(!ec.IsVehiclePresetPreflightCurrent(changed,f,q),"faction changed");f.Key="US";AICF_VehiclePresetCatalog.Quote++;check(!ec.IsVehiclePresetPreflightCurrent(changed,f,q),"native catalog price changed");AICF_VehiclePresetCatalog.Quote--;ec.m_SupplyNetwork.Operational=false;check(!ec.IsVehiclePresetPreflightCurrent(changed,f,q),"base ownership changed");
foreach(int cost in new[]{1,-1}){var t=fresh();var d=new AICF_VehiclePresetEconomyDecision(t,t.Request.Plan.Selection.m_Base,cost,100,0,AICF_EVehiclePresetEconomyDecision.INSUFFICIENT_AVAILABLE_SUPPLIES);check(!d.MatchesIdentity(t)&&!t.RecordVehiclePresetEconomyDecline(d),"inconsistent public constructor status or invalid cost");}
var missing=fresh();var mb=missing.Request.Plan.Selection.m_Base;missing.Request=null;var md=new AICF_VehiclePresetEconomyDecision(missing,mb,475,100,0,AICF_EVehiclePresetEconomyDecision.INSUFFICIENT_AVAILABLE_SUPPLIES);check(!md.MatchesIdentity(missing)&&!missing.RecordVehiclePresetEconomyDecline(md),"missing request constructor fail closed");
var wrongRequest=fresh();check(new Flow().InspectPresetEconomyBeforeLease(wrongRequest,null,f,null,wrongRequest.Request.Plan,10).Fail,"null request fails without EndRequest dereference");check(new Flow().InspectPresetEconomyBeforeLease(wrongRequest,null,f,new AICF_VehicleRequestState(),wrongRequest.Request.Plan,10).Fail,"foreign request");
var reserved=fresh();var book=new AICF_FactionFleet();var token=new AICF_VehicleLease();reserved.ActualLease=token;book.Registered=token;var refused=new Flow().InspectPresetEconomyBeforeLease(reserved,book,f,reserved.Request,reserved.Request.Plan,10);check(refused!=null&&!refused.Fail&&reserved.m_VehiclePresetEconomyDecline.HadReservedLease()&&reserved.m_VehiclePresetEconomyDecline.GetReservedLeaseGeneration()==1,"reserved empty stock drained expected decline");var history=reserved.m_VehiclePresetEconomyDecline;reserved.Phase=AICF_ETransportTripPhase.FALLBACK;reserved.ActualLease=null;book.Registered=null;token.State=AICF_EVehicleLeaseState.RELEASED;typeof(AICF_VehiclePresetEconomyDecision).GetField("m_ReservedLease",System.Reflection.BindingFlags.NonPublic|System.Reflection.BindingFlags.Instance).SetValue(history,null);check(history.MatchesIdentity(reserved)&&!history.IsCurrentPreflight(reserved)&&history.HadReservedLease()&&history.GetReservedLeaseGeneration()==1,"released weak lease historical receipt survives");
var regression=fresh();var regressionBook=new AICF_FactionFleet();regression.ActualLease=new AICF_VehicleLease();regressionBook.Registered=regression.ActualLease;check(new Flow().BroadLeaseBypassMutant(regression,regressionBook,f,regression.Request,regression.Request.Plan,10)==null&&regression.m_VehiclePresetEconomyDecline==null,"actual broad-lease mutant loses expected decline receipt");
Action<Action<AICF_TransportTrip,AICF_FactionFleet,AICF_VehicleLease>,string> badLease=(change,label)=>{var t=fresh();var b=new AICF_FactionFleet();var l=new AICF_VehicleLease();t.ActualLease=l;b.Registered=l;change(t,b,l);var result=new Flow().InspectPresetEconomyBeforeLease(t,b,f,t.Request,t.Request.Plan,10);check(result!=null&&result.Fail&&t.m_VehiclePresetEconomyDecline==null,label);};
badLease((t,b,l)=>b.Registered=null,"unregistered");badLease((t,b,l)=>b.Registered=new AICF_VehicleLease(),"different registered object");badLease((t,b,l)=>b.Key="USSR","foreign fleet");badLease((t,b,l)=>l.Group++,"stale lease group");badLease((t,b,l)=>l.Trip++,"stale lease trip");badLease((t,b,l)=>l.Faction="USSR","foreign lease faction");badLease((t,b,l)=>l.State=AICF_EVehicleLeaseState.RELEASED,"released lease");badLease((t,b,l)=>l.Gen=0,"invalid lease epoch");badLease((t,b,l)=>l.VehicleGen=1,"ever bound now empty");badLease((t,b,l)=>l.Entity=42,"old entity identity");badLease((t,b,l)=>l.Rpl="R","old Rpl");badLease((t,b,l)=>l.EntityText="OLD","old entity text");badLease((t,b,l)=>l.Prefab="CAR","old prefab");badLease((t,b,l)=>l.Lifecycle="L","old lifecycle");badLease((t,b,l)=>l.Cleanup=new N(),"cleanup snapshot");badLease((t,b,l)=>l.Vehicle=new IEntity(),"inconsistent physical reference");badLease((t,b,l)=>t.Request.TotalAttempts=1,"prior attempt with empty reservation");
var orphan=fresh();var orphanBook=new AICF_FactionFleet{Registered=new AICF_VehicleLease{Group=1}};check(new Flow().InspectPresetEconomyBeforeLease(orphan,orphanBook,f,orphan.Request,orphan.Request.Plan,10).Fail&&orphan.m_VehiclePresetEconomyDecline==null,"no-trip-lease cannot certify zero when old slot registration exists");
var epoch=fresh();var eb=new AICF_FactionFleet();epoch.ActualLease=new AICF_VehicleLease();eb.Registered=epoch.ActualLease;var ed=AICF_MatchController.Active.Economy.InspectVehiclePresetPreflight(epoch,f,epoch.Request.Plan.Selection.m_Base,eb);check(ed.IsCurrentPreflight(epoch),"captured reserved token before mutation");epoch.ActualLease.Gen++;check(!ed.IsCurrentPreflight(epoch),"lease epoch changed before record");return n;}
}
}
'@
$code=$code.Replace('CLASSIFY',(Cs (Body $receipt 'ClassifyQuote'))).Replace('CONSTRUCTOR',(Cs (Body $receipt 'AICF_VehiclePresetEconomyDecision'))).Replace('MATCH',(Cs (Body $receipt 'MatchesIdentity'))).Replace('CURRENT',(Cs (Body $receipt 'IsCurrentPreflight'))).Replace('UNUSED_RESERVATION',(Cs (Body $receipt 'IsOwnUnusedReservation'))).Replace('LEASE_FENCE',(Cs (Body $receipt 'HasCurrentLeaseFence'))).Replace('INSPECT',(Cs (Body $economy 'InspectVehiclePresetPreflight'))).Replace('RECHECK',(Cs (Body $economy 'IsVehiclePresetPreflightCurrent'))).Replace('RECORD',(Cs (Body $trip 'RecordVehiclePresetEconomyDecline'))).Replace('EARLY',(Cs (Body $flow 'InspectPresetEconomyBeforeLease'))).Replace('BROAD_BYPASS',(Cs ((Body $flow 'InspectPresetEconomyBeforeLease').Replace('if (!trip.GetVehiclePreset()) return null;','if (!trip.GetVehiclePreset() || trip.HasLease()) return null;')))).Replace('OBSERVED',(Cs (Body $fixture 'SameObservedTrip'))).Replace('FOOT_CONTEXT',(Cs (Body $fixture 'FootContextSame'))).Replace('FOOT_DUE',(Cs (Body $fixture 'FootReconcileDue'))).Replace('TERMINAL_RECEIPT',(Cs (Body $fixture 'TerminalReceiptSame'))).Replace('ACTUAL_REQUEST_RESET',(Cs (Body $requestSource 'Reset')))
Add-Type -TypeDefinition $code
$count=[AICFDeclineActual.Run]::Execute()
Write-Output "PASS typed economy preflight actual_cases=$count source_mutants=4 native=NOT_RUN"
$audit=Join-Path $RepositoryRoot 'tests/log-audits/Test-VehiclePresetEconomyDeclineLog.ps1'
$scratch=Join-Path $RepositoryRoot ('.codex-runtime/economy-decline-log-contract-'+[guid]::NewGuid().ToString('N'))
$logs=Join-Path $scratch 'profile/logs/test';[void][IO.Directory]::CreateDirectory($logs)
try{
 [IO.File]::WriteAllText((Join-Path $scratch 'launcher.txt'),'AICF_RUNTIME_MANIFEST_JSON={"arguments":["-probe","1"]}')
 @{native_exit=0;source_drift=0}|ConvertTo-Json|Set-Content (Join-Path $scratch 'summary.json')
 $raw=@'
CLI Params: -probe 1
[ROSTER_READY]
[PRESET_ECONOMY_TRIP_CREATED] operation_id=OP group_generation=2 trip_generation=3 created=1
[PRESET_ECONOMY_CHECK] name=EXACT_CONTROLLED_STOCK passed=1
[PRESET_ECONOMY_CHECK] name=REAL_FOOT_DESTINATION passed=1
[PRESET_ECONOMY_CHECK] name=EXPECTED_ATOMIC_DECLINE passed=1
[VEHICLE_PRESET_ECONOMY_DECLINED] operation_id=OP group_generation=2 trip_generation=3 decision=2 quote=475 stock=0 obligations=0 available=0 cap_reserved=0 main_world_spawned=0 vehicle_debit=0 base_rpl=BR
[ORDER_RESTORE_RESULT] operation_id=OP group_generation=2 trip_generation=3 state=FALLBACK success=1 is_current=1 postcondition_meaningful_task=1
[PRESET_ECONOMY_CHECK] name=SAME_INTENT_ONE_SHOT_NATIVE_FOOT_WALK passed=1 moved_m=13 operations=1 barrier_ms=20000 trip_creation_boundary=REGISTRY_CREATE current_trip_exact=1
[PRESET_ECONOMY_DONE] failures=0 original_route_proven=0 affordable=0 leases=0 main_world_cars=0 purchase_calls=0 paid=0 operations=1
Game destroyed.
'@
 $cases=@(@('positive',$raw,'' ,'PASS','PASS'),
 @('no done',$raw.Replace('[PRESET_ECONOMY_DONE]','[OTHER]'),'','FAIL','PASS'),
 @('fake success',$raw.Replace('failures=0','failures=1'),'','FAIL','PASS'),
 @('route falsely claimed',$raw.Replace('original_route_proven=0','original_route_proven=1'),'','FAIL','PASS'),
 @('spawn before decline',$raw.Replace('main_world_cars=0','main_world_cars=1'),'','FAIL','PASS'),
 @('lease before decline',$raw.Replace('leases=0','leases=1'),'','FAIL','PASS'),
 @('paid refusal',$raw.Replace('paid=0','paid=475'),'','FAIL','PASS'),
 @('retry churn',$raw.Replace('operations=1','operations=2'),'','FAIL','PASS'),
 @('false movement',$raw.Replace('moved_m=13','moved_m=0'),'','FAIL','PASS'),
 @('new request still approaching',($raw.Replace('[PRESET_ECONOMY_DONE]',"[PRESET_ECONOMY_TRIP_CREATED] operation_id=NEW group_generation=2 trip_generation=4 created=2 phase=APPROACHING_SITE`n[PRESET_ECONOMY_DONE]")),'','FAIL','PASS'),
 @('short barrier',$raw.Replace('barrier_ms=20000','barrier_ms=19999'),'','FAIL','PASS'),
 @('absent current trip',$raw.Replace('current_trip_exact=1','current_trip_exact=0'),'','FAIL','PASS'),
 @('affordable quote falsely declined',$raw.Replace('available=0','available=475'),'','FAIL','PASS'),
 @('wrong restore generation',$raw.Replace('state=FALLBACK','state=FAILED_CLOSED'),'','FAIL','PASS'),
 @('missing current handoff',$raw.Replace('is_current=1','is_current=0'),'','FAIL','PASS'),
 @('earlier acceptance failure',('[ACCEPTANCE_FAILURE_LATCHED] reason=OTHER'+"`n"+$raw),'','FAIL','PASS'),
 @('sibling script fatal',$raw,'SCRIPT (E): bad pointer','PASS','FAIL'),
 @('sibling native resource',$raw,'RESOURCES (E): missing material','PASS','PASS'))
 $positive=[regex]::Replace($raw,'(?m)^\[VEHICLE_PRESET_ECONOMY_DECLINED\][^\r\n]*\r?\n','')
 $positive=[regex]::Replace($positive,'(?m)^\[PRESET_ECONOMY_CHECK\] name=SAME_INTENT[^\r\n]*\r?\n','')
 $positive=$positive.Replace('name=EXPECTED_ATOMIC_DECLINE passed=1','name=AFFORDABLE_REAL_PURCHASE passed=1 current_trip_exact=1 current_cap=1 current_physical=1 cap_peak=1 empty_releases=1 accepted_lease_generation=2').Replace('affordable=0 leases=0 main_world_cars=0 purchase_calls=0 paid=0','affordable=1 leases=2 main_world_cars=1 purchase_calls=1 paid=475')
 $ledger=@'
[PRESET_ECONOMY_LEASE_RESERVED] operation_id=OP lease_generation=1 current_cap=1 has_asset=0
[PRESET_ECONOMY_EMPTY_RELEASE] operation_id=OP lease_generation=1 empty=1 cap_active=0 registered=0
[PRESET_ECONOMY_LEASE_RESERVED] operation_id=OP lease_generation=2 current_cap=1 has_asset=0
[VEHICLE_PRESET_PURCHASE] operation=OP supplies=475
[VEHICLE_SPAWNED] operation_id=OP lease_generation=2 vehicle_generation=1 rpl_id=REAL
'@
 $positive=$positive.Replace('[PRESET_ECONOMY_DONE]',$ledger+"`n[PRESET_ECONOMY_DONE]")
 $stop='[ACCEPTANCE_FAILURE_LATCHED] operation_id=OP group_gen=2 trip_gen=3 reason=COORDINATOR_STOP'
 $cases+=@(@('affordable with exact empty release',$positive,'','PASS','PASS',$true),
 @('scoped post purchase stop',($positive+"`n"+$stop),'','PASS','PASS',$true),
 @('early stop',($stop+"`n"+$positive),'','FAIL','PASS',$true),
 @('different post done error',($positive+"`n"+$stop.Replace('COORDINATOR_STOP','OTHER_FAILURE')),'','FAIL','PASS',$true),
 @('foreign post done stop',($positive+"`n"+$stop.Replace('operation_id=OP','operation_id=FOREIGN')),'','FAIL','PASS',$true),
 @('missing empty release',$positive.Replace('[PRESET_ECONOMY_EMPTY_RELEASE]','[UNKNOWN]'),'','FAIL','PASS',$true),
 @('cap leak',$positive.Replace('cap_active=0','cap_active=1'),'','FAIL','PASS',$true),
 @('cap overlap',$positive.Replace('current_cap=1','current_cap=2'),'','FAIL','PASS',$true),
 @('positive registry identity lost',$positive.Replace('current_trip_exact=1','current_trip_exact=0'),'','FAIL','PASS',$true),
 @('missing payment ledger',$positive.Replace('[VEHICLE_PRESET_PURCHASE]','[UNKNOWN]'),'','FAIL','PASS',$true))
 $drain=$raw.Replace('cap_reserved=0','cap_reserved=1 captured_lease_generation=1').Replace('affordable=0 leases=0','affordable=0 leases=1')
 $drain=$drain.Replace('[VEHICLE_PRESET_ECONOMY_DECLINED]',"[PRESET_ECONOMY_CHECK] name=AFTER_NATIVE_RESERVE_STOCK_DRAIN passed=1`n[PRESET_ECONOMY_STOCK_DRAIN] operation_id=OP lease_generation=1 before=675 after=0 tick_ms=100`n[VEHICLE_PRESET_ECONOMY_DECLINED]")
 $drain=$drain.Replace('[ORDER_RESTORE_RESULT]',"[PRESET_ECONOMY_DECLINE_RETURNED] operation_id=OP cap_reserved=1 captured_lease_generation=1 own_cap_before=1 physical_before=0 tick_ms=200`n[PRESET_ECONOMY_EMPTY_RELEASE] operation_id=OP lease_generation=1 empty=1 cap_active=0 registered=0`n[PRESET_ECONOMY_CHECK] name=EXACT_EMPTY_LEASE_RELEASE passed=1`n[PRESET_ECONOMY_CHECK] name=EXACT_RESERVED_EMPTY_DRAIN_WINDOW passed=1 own_cap_after=0 actual_same_drained_lease=1`n[ORDER_RESTORE_RESULT]")
 $cases+=@(@('reserved empty cross-frame positive',$drain,'','PASS','PASS',$false,$true),
 @('drain without reserved capture',$drain.Replace('cap_reserved=1','cap_reserved=0'),'','FAIL','PASS',$false,$true),
 @('different drained lease epoch',$drain.Replace('before=675','before=675').Replace('lease_generation=1 before=675','lease_generation=2 before=675'),'','FAIL','PASS',$false,$true),
 @('old release before decision',$drain.Replace('[PRESET_ECONOMY_STOCK_DRAIN]',"[PRESET_ECONOMY_EMPTY_RELEASE] operation_id=OP lease_generation=1 empty=1 cap_active=0 registered=0`n[PRESET_ECONOMY_STOCK_DRAIN]"),'','FAIL','PASS',$false,$true),
 @('drain cap retained',$drain.Replace('cap_active=0','cap_active=1'),'','FAIL','PASS',$false,$true),
 @('drain physical asset exists',$drain.Replace('physical_before=0','physical_before=1'),'','FAIL','PASS',$false,$true),
 @('drain no release',$drain.Replace('[PRESET_ECONOMY_EMPTY_RELEASE]','[UNKNOWN]'),'','FAIL','PASS',$false,$true))
 foreach($case in $cases){
  [IO.File]::WriteAllText((Join-Path $logs 'console.log'),$case[1]);[IO.File]::WriteAllText((Join-Path $logs 'error.log'),$case[2])
  $auditArgs=@('-NoProfile','-ExecutionPolicy','Bypass','-File',$audit,'-EvidenceRoot',$scratch);if($case.Count -gt 5 -and $case[5]){$auditArgs+='-Affordable'}
  if($case.Count -gt 6 -and $case[6]){$auditArgs+='-ReservedEmpty'}
  & powershell.exe @auditArgs|Out-Null
  $result=Get-Content (Join-Path $scratch 'economy-audit.json') -Raw|ConvertFrom-Json
  if($result.functional -ne $case[3] -or $result.fatal -ne $case[4]){throw "Economy log case: $($case[0])"}
  if($case[0] -eq 'sibling native resource' -and $result.resources -ne 'FAIL'){throw 'Resource error hidden'}
  Write-Output "PASS typed economy log=$($case[0])"
 }
}finally{if(Test-Path -LiteralPath $scratch){$resolved=[IO.Path]::GetFullPath($scratch);if(!$resolved.StartsWith([IO.Path]::GetFullPath((Join-Path $RepositoryRoot '.codex-runtime'))+[IO.Path]::DirectorySeparatorChar)){throw 'Unsafe cleanup path'};Remove-Item -LiteralPath $resolved -Recurse -Force}}

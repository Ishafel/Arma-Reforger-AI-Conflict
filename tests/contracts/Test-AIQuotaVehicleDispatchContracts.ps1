param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
# Reuse the same actual-entry translation/native shims; this first runs its own
# cases. The adapter replay has a separate namespace, without substituting the
# service for a claim that production ledger accounting has passed.
. (Join-Path $PSScriptRoot 'Test-AIQuotaVehicleEntryContracts.ps1') -RepositoryRoot $RepositoryRoot
$entryCode=[regex]::Replace($code,'(?m)^using [^\r\n]+;\s*','')
$root=Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Integration'
function Adapt([string]$s){
 $s=$s.Replace('ref ','').Replace('GetGame()','Runtime.Game').Replace('super.','base.')
 $s=$s.Replace('protected ','public ').Replace('override void ','override void ').Replace('IEntity actor;','IEntity actor = null;')
 $s=[regex]::Replace($s,'(?m)^(\s*)override (void|float) ','$1public override $2 ')
 $s=[regex]::Replace($s,'(?m)^(\s*)(static AICF_\w+|static bool|bool|void) (\w+)\(','$1public $2 $3(')
 return $s
}
$dispatch=Adapt ([IO.File]::ReadAllText((Join-Path $root 'AICF_AIQuotaVehicleDispatch.c')))
$dispatch=$dispatch.Replace('class AICF_AIQuotaVehicleDispatch','public class AICF_AIQuotaVehicleDispatch:Native')
$failure=Adapt ([IO.File]::ReadAllText((Join-Path $root 'AICF_AIQuotaVehicleFailure.c')))
$failure=$failure.Replace('class AICF_AIQuotaVehicleFailure','public class AICF_AIQuotaVehicleFailure:Native')
$action=Adapt ([IO.File]::ReadAllText((Join-Path $root 'AICF_AIQuotaVehicleAction.c')))
$action=$action.Replace('modded class SCR_AIGetInVehicle','public class SCR_AIGetInVehicle:SCR_AIBehaviorBase')
$replay="using System;using System.Collections.Generic;namespace QuotaVehicleDispatchReplay {`n"+$entryCode
$replay+=@'
public class AICF_AIQuotaVehiclePermit:Native {public AICF_AIQuotaVehicleEntry Entry;public bool Closed;}
public class AICF_AIWorldQuotaService:Native
{
 public static AICF_AIWorldQuotaService Current;public bool Active=true,Allowed=true,PermitCurrent=true;public int PeekCalls,BeginCalls,CancelCalls,MarkCalls,ResultCalls;public bool LastResult;public Action BeginHook,PeekHook,MarkHook;
 public static AICF_AIWorldQuotaService GetAuthorityForWorld(BaseWorld w){return Current!=null&&Current.Active&&Runtime.Game!=null&&w==Runtime.Game.World?Current:null;}
 public bool PeekVehicleEntry(AICF_AIQuotaVehicleEntry e){PeekCalls++;if(PeekHook!=null)PeekHook();return Allowed;}
 public AICF_AIQuotaVehiclePermit BeginVehicleEntry(AICF_AIQuotaVehicleEntry e){BeginCalls++;if(BeginHook!=null)BeginHook();return Allowed?new AICF_AIQuotaVehiclePermit{Entry=e}:null;}
 public bool IsVehicleEntryPermitCurrent(AICF_AIQuotaVehiclePermit p,AICF_AIQuotaVehicleEntry e){return Active&&PermitCurrent&&p!=null&&!p.Closed&&p.Entry==e;}
 public void MarkVehicleEntryDispatched(AICF_AIQuotaVehiclePermit p){MarkCalls++;if(MarkHook!=null)MarkHook();}
 public void ObserveVehicleEntryResult(AICF_AIQuotaVehiclePermit p,bool accepted){ResultCalls++;LastResult=accepted;}
 public void CancelVehicleEntry(AICF_AIQuotaVehiclePermit p){if(!p.Closed){CancelCalls++;p.Closed=true;}}
}
public class SCR_AIUtilityComponent:Native {public IEntity m_OwnerEntity;public AIInfo m_AIInfo=new AIInfo();}
public class AIInfo:Native {public int Available;public void SetAIState(int state){Available++;}}
public static class EUnitAIState {public const int AVAILABLE=0;}
public static class EAIActionState {public const int FAILED=4;}
public static class AICF_VehicleBoardingMutationFence {public static bool IsAuthoritativeAIEntity(IEntity e){var c=e as ChimeraCharacter;return c!=null&&c.Controller!=null&&!c.Controller.Player;}}
public class SCR_AIActivityBase:Native {public int Finished;public static SCR_AIActivityBase Cast(object o){return o as SCR_AIActivityBase;}public void OnChildBehaviorFinished(SCR_AIBehaviorBase o){Finished++;}}
public class Param<T> {public T m_Value;}
public class AIActionBase:Native
{
 public SCR_AIUtilityComponent m_Utility=new SCR_AIUtilityComponent();public Param<IEntity> m_Vehicle=new Param<IEntity>();public Param<BaseCompartmentSlot> m_CompartmentToGetIn=new Param<BaseCompartmentSlot>();public SCR_AIActivityBase Related=new SCR_AIActivityBase();public int NativeSelected,NativeCleanup,Invoked,State;public Action CompleteHook;
 public float GetPriority(){return 130;}public virtual float CustomEvaluate(){return 130;}public virtual float EvaluatePriorityLevel(){return 1000;}
 public void Fail(){OnActionFailed();Invoked++;}public void SetActionState(int state){State=state;}public object GetRelatedGroupActivity(){return Related;}
 public virtual void OnActionSelected(){NativeSelected++;}public virtual void OnActionFailed(){NativeCleanup++;}public virtual void OnActionCompleted(){if(CompleteHook!=null)CompleteHook();}public virtual void OnActionDeselected(){}public virtual void OnActionRemoved(){}
}
public class SCR_AIBehaviorBase:AIActionBase{public static SCR_AIBehaviorBase Cast(object value){return value as SCR_AIBehaviorBase;}}
'@
$replay+=$dispatch+$failure+$action
$replay+=@'
public static class DispatchCases
{
 static int Cases;static AICF_AIQuotaVehicleEntry Entry;static AICF_AIWorldQuotaService S;
 static void Need(bool yes,string why){Cases++;if(!yes)throw new Exception(why);}
 static SCR_AIGetInVehicle Setup()
 {
  Replication.Server=true;SCR_PossessingManagerComponent.MainId=0;Runtime.Game=new Game();var w=Runtime.Game.World;
  var actor=new ChimeraCharacter{Id=1,World=w};var group=new SCR_AIGroup{Id=2,World=w};var agent=new AIAgent{Id=3,World=w,Actor=actor,Group=group};actor.Control.Agent=agent;
  var car=new IEntity{Id=4,World=w};var manager=new BaseCompartmentManagerComponent();car.Manager=manager;var seat=new BaseCompartmentSlot{Vehicle=car,Owner=car,Manager=manager};manager.Seats.Add(seat);
  foreach(var e in new IEntity[]{actor,group,agent,car})w.Entities[e.Id]=e;
  S=new AICF_AIWorldQuotaService();AICF_AIWorldQuotaService.Current=S;Entry=AICF_AIQuotaVehicleEntry.Capture(actor,car,seat);
  return new SCR_AIGetInVehicle{m_Utility=new SCR_AIUtilityComponent{m_OwnerEntity=actor},m_Vehicle=new Param<IEntity>{m_Value=car},m_CompartmentToGetIn=new Param<BaseCompartmentSlot>{m_Value=seat}};
 }
 static AICF_AIQuotaVehicleDispatch Begin(){return AICF_AIQuotaVehicleDispatch.Begin(Entry.m_Actor,Entry.m_Vehicle,Entry.m_Seat);}
 public static int Run()
 {
  Cases=0;var a=Setup();Need(a.CustomEvaluate()==130&&a.EvaluatePriorityLevel()==1000,"positive native priorities preserved");Need(S.PeekCalls==2&&S.BeginCalls==0&&S.MarkCalls==0,"both evaluations are peek-only");
  S.Allowed=false;Need(a.CustomEvaluate()<0&&a.EvaluatePriorityLevel()<0,"both additive priorities suppress quota veto");Need(S.BeginCalls==0,"evaluation never reserves even on repeated veto");
  a.OnActionSelected();Need(S.BeginCalls==1&&a.NativeSelected==0&&a.NativeCleanup==0&&a.State==4&&a.Invoked==1,"selection veto fails without native seat cleanup");
  a=Setup();a.OnActionSelected();Need(S.BeginCalls==1&&S.MarkCalls==1&&a.NativeSelected==1,"permit before selected/native dispatch");Need(S.ResultCalls==0,"void selection never fabricates actual native acceptance");
  a.OnActionSelected();Need(S.BeginCalls==1&&a.NativeSelected==2,"same current action reuses its permit");a.OnActionCompleted();Need(S.CancelCalls==1,"completion ends only its source claim");
  a=Setup();a.OnActionSelected();a.OnActionFailed();Need(S.CancelCalls==1&&a.NativeCleanup==1,"admitted failure preserves native path and cancels source");
  a=Setup();a.OnActionSelected();a.OnActionDeselected();Need(S.CancelCalls==1,"deselection cancels source");
  a=Setup();a.OnActionSelected();a.OnActionRemoved();Need(S.CancelCalls==1,"removal cancels source");
  a=Setup();var d=Begin();Need(d!=null&&d.IsCurrent(),"direct entry current before native");d.ObserveNativeResult(false);Need(S.ResultCalls==1&&!S.LastResult,"actual bool false retained");d.Cancel();d.Cancel();Need(S.CancelCalls==1,"caller cancel idempotent");
  a=Setup();d=Begin();d.ObserveNativeResult(true);Need(S.ResultCalls==1&&S.LastResult&&S.CancelCalls==0,"accepted is observation, no automatic claim refund");d.Cancel();
  a=Setup();S.BeginHook=()=>Entry.m_Control.Agent=new AIAgent();Need(Begin()==null&&S.CancelCalls==1,"callback identity drift cancels captured permit before native");
  a=Setup();S.PeekHook=()=>Entry.m_Control.Agent=new AIAgent();Need(!AICF_AIQuotaVehicleDispatch.Peek(Entry.m_Actor,Entry.m_Vehicle,Entry.m_Seat)&&S.BeginCalls==0,"peek callback drift rejected without reservation");
  a=Setup();S.MarkHook=()=>Entry.m_Actor.Controller.Player=true;a.OnActionSelected();Need(S.MarkCalls==1&&S.CancelCalls==1&&a.NativeSelected==0&&a.NativeCleanup==0,"post-admission callback takeover veto before super");
  a=Setup();Runtime.Game.Players.PlayerId=8;Need(a.CustomEvaluate()==130,"human seat not charged");a.OnActionSelected();Need(S.BeginCalls==0&&S.PeekCalls==0&&a.NativeSelected==1,"human dispatch original delegate");
  a=Setup();S.Active=false;d=Begin();Need(d!=null&&d.IsCurrent()&&S.BeginCalls==0,"outside active policy delegates");S.Active=true;Need(!d.IsCurrent(),"new policy callback cannot inherit unguarded dispatch");
  a=Setup();Need(AICF_AIQuotaVehicleDispatch.Begin(null,Entry.m_Vehicle,Entry.m_Seat)==null,"active unknown actor fail closed");
  a=Setup();S.PermitCurrent=false;Need(Begin()==null&&S.CancelCalls==1,"service permit unknown rejected before dispatch");
  a=Setup();var old=Begin();var replacement=Begin();a.m_AICFQuotaEntry=old;a.CompleteHook=()=>a.m_AICFQuotaEntry=replacement;a.OnActionCompleted();Need(a.m_AICFQuotaEntry==replacement&&S.CancelCalls==1,"terminal callback never clears replacement claim");replacement.Cancel();
  return Cases;
 }
}
}
'@
Add-Type -TypeDefinition $replay -Language CSharp
$cases=[QuotaVehicleDispatchReplay.DispatchCases]::Run()
"PASS AI quota vehicle adapters actual_extracted_cases=$cases production_service_accounting=NOT_TESTED native=NOT_RUN"

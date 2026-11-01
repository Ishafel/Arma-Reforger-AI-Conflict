param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$source=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Integration/AICF_AIQuotaVehicleFailure.c'))
$pinned=Join-Path $RepositoryRoot '.cache/reforger-api/Arma-Reforger-Script-Diff-1.8.0.13/scripts/Game/AI/Behavior'
if(!(Test-Path $pinned)){throw 'Pinned1.8.0.13 required for actual failure-chain replay'}
function Body([string]$text,[string]$method){Get-AICFMethodBody ([pscustomobject]@{Source=$text;Code=(ConvertTo-AICFCodeText $text)}) $method}
$actions=[IO.File]::ReadAllText((Join-Path $pinned 'SCR_AIAction.c'))
$vehicle=[IO.File]::ReadAllText((Join-Path $pinned 'SCR_AIVehicleBehavior.c'))
$getIn=$vehicle.Substring($vehicle.IndexOf('class SCR_AIGetInVehicle'))
$behavior=[IO.File]::ReadAllText((Join-Path $pinned 'SCR_AIBehavior.c'))
$onFail=Body $actions 'OnFail'
$onFail=[regex]::Replace($onFail,'(?s)#ifdef AI_DEBUG.*?#endif','')
$stock=Body $getIn 'OnActionFailed'
$baseFailure=Body $actions 'OnActionFailed'
$behaviorFailure=(Body $behavior 'OnActionFailed').Replace('super.OnActionFailed()','base.OnActionFailed()')
$vehicleFailure=(Body ($vehicle.Substring(0,$vehicle.IndexOf('class SCR_AIGetInVehicle'))) 'OnActionFailed').Replace('super.OnActionFailed()','base.OnActionFailed()')
$actual=$source.Replace('class AICF_AIQuotaVehicleFailure','public class AICF_AIQuotaVehicleFailure : Native')
$actual=[regex]::Replace($actual,'(?m)^(\s*)(void|bool) (\w+)\(','$1public $2 $3(')
$stock=$stock.Replace('super.OnActionFailed()','base.OnActionFailed()')
$code=@'
using System;
public class Native {public static implicit operator bool(Native v){return v!=null;}}
public class IEntity:Native {public bool Safe=true;}
public class Access:Native {public int Interrupted,GetOut;public bool GettingIn,Inside;public bool IsGettingIn(){return GettingIn;}public IEntity Car;public IEntity GetVehicleIn(ChimeraCharacter c){return Inside?Car:null;}public void GetOutVehicle(int a,int b,int c,bool d){GetOut++;}public void InterruptVehicleActionQueue(bool a,bool b,bool c){Interrupted++;}}
public class CompartmentAccessComponent:Access {}
public class ChimeraCharacter:IEntity {public CompartmentAccessComponent Access=new CompartmentAccessComponent();public static ChimeraCharacter Cast(object v){return v as ChimeraCharacter;}public CompartmentAccessComponent GetCompartmentAccessComponent(){return Access;}}
public class AIInfo:Native {public int Available;public void SetAIState(int state){Available++;}}
public class SCR_AIUtilityComponent:Native {public IEntity m_OwnerEntity=new ChimeraCharacter();public AIInfo m_AIInfo=new AIInfo();public int Added;public void AddAction(object a){Added++;}}
public class Slot:Native {public int AccessibleWrites,ReservationWrites;public IEntity Car=new IEntity();public void SetCompartmentAccessible(bool b){AccessibleWrites++;}public void SetReserved(IEntity e){ReservationWrites++;}public IEntity GetVehicle(){return Car;}}
public class Param {public Slot m_Value=new Slot();}
public static class EUnitAIState {public const int AVAILABLE=0;}
public static class EAIActionState {public const int FAILED=4;}
public static class EGetOutType {public const int TELEPORT=0;}
public static class ECloseDoorAfterActions {public const int LEAVE_OPEN=0;}
public static class AICF_VehicleBoardingMutationFence {public static bool IsAuthoritativeAIEntity(IEntity e){return e!=null&&e.Safe;}}
public class SCR_AIGetOutVehicle {public SCR_AIGetOutVehicle(object a,object b,object c,float priority=0,float priorityLevel=0){}}
public class Invoker {public int Calls;public void Invoke(object o){Calls++;}}
public class SCR_AIActivityBase:Native {public int ChildFinished;public void OnChildBehaviorFinished(SCR_AIBehaviorBase child){ChildFinished++;}public static SCR_AIActivityBase Cast(object value){return value as SCR_AIActivityBase;}}
public class AIActionBase:Native
{
 public const int PRIORITY_BEHAVIOR_GET_OUT_VEHICLE_HIGH_PRIORITY=1;public SCR_AIUtilityComponent m_Utility=new SCR_AIUtilityComponent();public Param m_CompartmentToGetIn=new Param();public Invoker m_OnActionFailed=new Invoker();public int State,NativeFailures;
 public SCR_AIActivityBase Related=new SCR_AIActivityBase();public object GetRelatedGroupActivity(){return Related;}
 public void SetActionState(int state){State=state;}public float EvaluatePriorityLevel(){return 0;}
 public virtual void OnActionFailed()
'@
$code+="{NativeFailures++;`n$baseFailure`n}`n"
$code+=@'
 public void Fail(){OnFail();}
 public void OnFail()
'@
$code+="{`n$onFail`n}}`npublic class SCR_AIBehaviorBase:AIActionBase {public static SCR_AIBehaviorBase Cast(object value){return value as SCR_AIBehaviorBase;}public override void OnActionFailed(){`n$behaviorFailure`n}}`npublic class PinnedVehicle:SCR_AIBehaviorBase {public override void OnActionFailed(){`n$vehicleFailure`n}}`npublic class PinnedGetIn:PinnedVehicle {public override void OnActionFailed(){`n$stock`n}}`n"+$actual
$code+=@'
public class SCR_AIGetInVehicle:PinnedGetIn
{
 public AICF_AIQuotaVehicleFailure Veto=new AICF_AIQuotaVehicleFailure();
 public void AICF_FailBeforeQuotaDispatch(){Veto.FailBeforeDispatch(this,m_Utility);}
 public void AICF_AllowQuotaDispatch(){Veto.AllowDispatch();}
 public override void OnActionFailed(){if(!Veto.ApplyFailure(this,m_Utility))base.OnActionFailed();}
}
'@
$code+=@'
public static class VetoCases
{
 static int Cases;
 static void Need(bool value,string why){Cases++;if(!value)throw new Exception(why);}
 static SCR_AIGetInVehicle Make(){return new SCR_AIGetInVehicle();}
 static void NoSeat(SCR_AIGetInVehicle a,string why){Need(a.m_CompartmentToGetIn.m_Value.ReservationWrites==0&&a.m_CompartmentToGetIn.m_Value.AccessibleWrites==0&&((ChimeraCharacter)a.m_Utility.m_OwnerEntity).Access.Interrupted==0&&((ChimeraCharacter)a.m_Utility.m_OwnerEntity).Access.GetOut==0&&a.m_Utility.Added==0,why);}
 public static int Run()
 {
  Cases=0;var a=Make();a.Fail();Need(a.NativeFailures==1&&a.m_OnActionFailed.Calls==1&&a.State==4,"unvetoed original full failure chain");Need(a.m_CompartmentToGetIn.m_Value.ReservationWrites==1,"control proves pinned foreign reservation clear");
  a=Make();a.AICF_FailBeforeQuotaDispatch();NoSeat(a,"veto never clears/opens/interrupts/ejects/reserves foreign seat");Need(a.m_OnActionFailed.Calls==1&&a.State==4&&a.m_Utility.m_AIInfo.Available==1,"actual pinned OnFail invoker and failed/available state retained");
  Need(a.Related.ChildFinished==1,"related group activity child completion retained before OnFail invoker");
  a.OnActionFailed();NoSeat(a,"veto latch survives repeated/reentrant failure");Need(a.NativeFailures==0,"no stock vehicle cleanup after repeated failure");
  a.AICF_AllowQuotaDispatch();a.Fail();Need(a.NativeFailures==1&&a.m_CompartmentToGetIn.m_Value.ReservationWrites==1&&a.m_OnActionFailed.Calls==2,"admitted dispatch explicitly restores original failure path");
  a=Make();((ChimeraCharacter)a.m_Utility.m_OwnerEntity).Access.GettingIn=true;a.AICF_FailBeforeQuotaDispatch();NoSeat(a,"never-dispatched veto does not add GetOut even if foreign transition exists");
  a=Make();((ChimeraCharacter)a.m_Utility.m_OwnerEntity).Access.Inside=true;a.AICF_FailBeforeQuotaDispatch();NoSeat(a,"veto does not teleport out of existing vehicle");
  a=Make();a.m_Utility.m_OwnerEntity.Safe=false;a.AICF_FailBeforeQuotaDispatch();Need(a.m_Utility.m_AIInfo.Available==0&&a.m_OnActionFailed.Calls==1&&a.State==4,"player/stale owner does not get AIInfo mutation, failure observer still notified");NoSeat(a,"player owner no vehicle mutation");
  a=Make();a.m_Utility=null;a.AICF_FailBeforeQuotaDispatch();Need(a.State==4&&a.m_OnActionFailed.Calls==1,"weak utility null fail is safe");
  a=Make();a.m_Utility.m_AIInfo=null;a.AICF_FailBeforeQuotaDispatch();Need(a.State==4&&a.m_OnActionFailed.Calls==1,"missing AIInfo fails without native seat cleanup");
  var generic=new AIActionBase();var veto=new AICF_AIQuotaVehicleFailure();veto.FailBeforeDispatch(generic,generic.m_Utility);Need(!veto.ApplyFailure(generic,generic.m_Utility),"nonbehavior action cannot be passed to typed group child callback");
  return Cases;
 }
}
'@
Add-Type -TypeDefinition $code -Language CSharp
$cases=[VetoCases]::Run()
"PASS AI quota failure lifecycle actual_extracted_cases=$cases pinned=1.8.0.13 dispatch_adapter=NOT_CONNECTED native=NOT_RUN"

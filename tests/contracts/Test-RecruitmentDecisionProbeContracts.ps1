param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$source=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'tests/fixtures/AICF_RecruitmentDecisionProbe.c'))
function Body([string]$text,[string]$name){Get-AICFMethodBody ([pscustomobject]@{Source=$text;Code=(ConvertTo-AICFCodeText $text)}) $name}
$names=@('EOnTaskSimulate','IsCurrent','TryRecover','FindDestination','SafeDestination')
function Rules([string]$text){
 $code=ConvertTo-AICFCodeText $text
 if($code-match'\b(?:Teleport|SetOrigin|SetWorldTransform|SetVelocity|LoadTileIn|GetClosestPositionOnNavmesh|GetReachablePoint|CallLater|Remove|Insert|AddAction|Fail|Complete|SetHealth|SetLifeState|RequestSpawn|RepairInfantryRecruitmentApproach)\s*\('){'NO_GAMEPLAY_QUERIES_WRITES_OR_TIMERS'}
 if($code-match'\b(?:order|m_Order|slot|m_Slot)\.m_\w+\s*(?:=(?!=)|\+\+|--|\+=|-=)'){'NO_OWNER_WRITES'}
 if($text-match'override\s+static|static\s+.*\bCapture\('){'NO_STATIC_CAPTURE_OVERRIDE'}
 if(!(Body $text Enabled).Contains('System.GetCLIParam("aicfRecruitmentDecisionProbe", value) && value == "1"')-or!(Body $text Enabled).Contains('Replication.IsServer()')){'EXPLICIT_AUTHORITY_OPT_IN'}
 if(!(Body $text Read).Contains('!Enabled() || s_iEvents >= MAXIMUM_EVENTS')-or$text-notmatch'MAXIMUM_EVENTS = 256;' -or!(Body $text Record).Contains('event=OVERFLOW')-or!(Body $text Record).Contains('completeness=UNPROVEN')){'BOUNDED_NEGATIVE_DIAGNOSTIC'}
 if(!(Body $text Record).Contains('if (!Enabled()) return;')){'DISABLED_NO_EVENTS'}
 foreach($name in $names){$b=Body $text $name;if(@([regex]::Matches($b,'super\.'+[regex]::Escape($name)+'\(')).Count-ne1-or@([regex]::Matches($b,'return ')).Count-ne1){'ONE_ORIGINAL_DELEGATE';break};if($b.IndexOf('AICF_RecruitmentDecisionProbe.Read(')-gt$b.IndexOf('super.')-or$b.IndexOf('AICF_RecruitmentDecisionProbe.Record(')-lt$b.IndexOf('super.')){'PRE_CAPTURE_POST_DECISION';break}}
 foreach($part in @('m_World.FindEntityByID(m_GroupId) != m_Group','m_Group.FindComponent(RplComponent) != m_Rpl','m_Slot.AICF_RDOrder() != m_Order','m_Slot.GetSpawnGeneration() != m_iGeneration','m_Order.m_iToken != m_iToken','m_Order.m_iAssignment != m_iAssignment','m_Order.m_iIntent != m_iIntent','m_Order.m_iGraphRevision != m_iGraph','m_Order.m_iApproachRepairs != m_iRepair','m_Order.m_Waypoint != m_Waypoint','m_Utility.GetCurrentAction() != m_Current','m_Utility.GetExecutedAction() != m_Executed','m_Agent.GetControlledEntity() != m_Actor','m_Agent.GetParentGroup() != m_Group','m_Actor.FindComponent(RplComponent) != m_ActorRpl','m_bHadActor && !m_Actor')){if(!(Body $text Stable).Contains($part)){'REPEATED_IMMUTABLE_CONTEXT';break}}
 foreach($field in @('captured_age_ms=','captured_token=','captured_repair=','current_state=','executed_state=','related_waypoint=','receipt_present=','stable_read=','unknown_is_not_permission=1','output_valid_only_if_result=1','AFTER_DECISION=1','cause_proven=0')){if(!$text.Contains($field)){'HONEST_FIELDS';break}}
 if(!(Body $text AICF_RDOrder).Contains('return m_RecruitmentOrder;')){'RAW_DIAGNOSTIC_ORDER_ONLY'}
 $node=Body $text EOnTaskSimulate
 if(!$node.Contains('super.EOnTaskSimulate(owner, dt)')-or!$node.Contains('return nodeResult;')-or$text.Contains('override protected bool AICF_HandleRecruitmentFailedMovement')){'NATIVE_DECLARED_NODE_BOUNDARY'}
 foreach($port in @('PORT_MOVE_RESULT, moveResult','PORT_HANDLER_ID, handler','PORT_IS_WAYPOINT_RELATED, related','PORT_MOVE_LOCATION, location')){if(!$node.Contains('GetVariableIn('+$port+')')){'PINNED_NATIVE_PORTS';break}}
 if(!$node.Contains('internal_handler_result=UNKNOWN result_is_node_fail_only=1')){'NO_HANDLER_ACCEPTANCE_INFERENCE'}
}
$bad=@(Rules $source);if($bad.Count){throw ($bad-join',')}
$sourceMutants=@(@('MAXIMUM_EVENTS = 256;','MAXIMUM_EVENTS = 999999;'),@('m_Order.m_iToken != m_iToken','false'),@('m_Slot.AICF_RDOrder() != m_Order','false'),@('m_Utility.GetExecutedAction() != m_Executed','false'),@('m_bHadActor && !m_Actor','false'),@('unknown_is_not_permission=1','unknown_is_not_permission=0'),@('return m_RecruitmentOrder;','return null;'),@('if (!Enabled()) return;','if (false) return;'))
foreach($m in $sourceMutants){if(!$source.Contains($m[0])){throw 'Stale source mutant'};if(!@(Rules $source.Replace($m[0],$m[1])).Count){throw ('Unsafe source survived '+$m[0])}}

# Execute the five actual wrapper bodies. All native/snapshot boundaries are explicit mocks.
function ReplayCode([string]$text,[string]$ns){
 $node=Body $text EOnTaskSimulate
 $current=Body $text IsCurrent;$recover=Body $text TryRecover;$find=Body $text FindDestination;$safe=Body $text SafeDestination
 $code=@'
using System;namespace NS {
 public class Truth{public static implicit operator bool(Truth x){return x!=null;}}
 public enum ENodeResult{NONE,FAIL,SUCCESS,RUNNING,ABORTED,FINISHED,TERMINAL}
 public class AIAgent:Truth{}
 public struct vector{public float X;public float this[int i]{get{return X;}}public static float Distance(vector a,vector b){return Math.Abs(a.X-b.X);}public static float DistanceXZ(vector a,vector b){return Distance(a,b);}}
 public static class Math{public static float AbsFloat(float x){return global::System.Math.Abs(x);}public static float Abs(float x){return global::System.Math.Abs(x);}}
 public static class Clock{public static int GetTickCount(){return 100;}}
 public class SCR_AIGroup:Truth{}public class SCR_AIGroupUtilityComponent:Truth{}
 public class AICF_InfantryRecruitmentOrder:Truth{}public class AICF_GroupSlot:Truth{public AICF_InfantryRecruitmentOrder AICF_RDOrder(){return Test.ExpectedOrder;}}
 public class AICF_MatchController:Truth{public static AICF_MatchController GetActiveController(){return new AICF_MatchController();}public AICF_GroupSlot FindManagedInfantrySlot(SCR_AIGroup g){return new AICF_GroupSlot();}}
 public class AICF_RecruitmentDecisionSnapshot:Truth{}
 public static class AICF_RecruitmentDecisionProbe{public static bool Enabled(){return Test.Enabled;}public static AICF_RecruitmentDecisionSnapshot Read(AICF_InfantryRecruitmentOrder o,SCR_AIGroup g,SCR_AIGroupUtilityComponent u){Test.Reads++;return Test.Snapshot?new AICF_RecruitmentDecisionSnapshot():null;}public static void Record(string e,bool r,AICF_RecruitmentDecisionSnapshot b,AICF_InfantryRecruitmentOrder o,SCR_AIGroup g,SCR_AIGroupUtilityComponent u,string d){if(Test.Calls!=1)throw new Exception("record before original or original repeated");Test.Recorded++;Test.Details=d;}}
 public class Native {protected SCR_AIGroup m_Group=new SCR_AIGroup();protected SCR_AIGroupUtilityComponent m_GroupUtilityComponent=new SCR_AIGroupUtilityComponent();protected AICF_InfantryRecruitmentOrder m_Order;protected int m_iRepair,m_iCapturedAt,m_iToken,m_iGeneration,m_iAssignment,m_iIntent,m_iGraph,m_ActorId,m_WaypointId;protected bool m_bConsumed;protected vector m_vOrigin,m_vService;
  public Native(){m_Order=Test.ExpectedOrder;}
  protected const string PORT_MOVE_RESULT="result",PORT_HANDLER_ID="handler",PORT_IS_WAYPOINT_RELATED="related",PORT_MOVE_LOCATION="location";
  protected bool GetVariableIn(string port,out int value){Test.PortReads++;if(port!="result"&&port!="handler")throw new Exception("integer port");bool found=Test.MissingPort!=(port=="result"?1:2);value=found?(port=="result"?7:9):-1;return found;}
  protected bool GetVariableIn(string port,out bool value){Test.PortReads++;if(port!="related")throw new Exception("bool port");value=Test.MissingPort!=3;return value;}
  protected bool GetVariableIn(string port,out vector value){Test.PortReads++;if(port!="location")throw new Exception("vector port");bool found=Test.MissingPort!=4;value=new vector{X=found?11:0};return found;}
  public virtual ENodeResult EOnTaskSimulate(AIAgent owner,float dt){Test.Calls++;if(owner!=Test.ExpectedOwner||dt!=0.25f)throw new Exception("node argument changed");return Test.NodeReturn;}
  public virtual bool IsCurrent(AICF_InfantryRecruitmentOrder order,int repair){Test.Calls++;if(order!=Test.ExpectedOrder||repair!=13)throw new Exception("current argument changed");return Test.Return;}
  public virtual bool TryRecover(AICF_InfantryRecruitmentOrder order){Test.Calls++;if(order!=Test.ExpectedOrder)throw new Exception("recover argument changed");return Test.Return;}
  public virtual bool FindDestination(out vector destination){Test.Calls++;destination=new vector{X=17};return Test.Return;}
  public virtual bool SafeDestination(vector query,out vector destination){Test.Calls++;if(query.X!=19)throw new Exception("query changed");destination=new vector{X=23};return Test.Return;}
 }
 public class Probe:Native{public override ENodeResult EOnTaskSimulate(AIAgent owner,float dt) NODE
  public override bool IsCurrent(AICF_InfantryRecruitmentOrder order,int repair) CURRENT
  public override bool TryRecover(AICF_InfantryRecruitmentOrder order) RECOVER
  public override bool FindDestination(out vector destination) FIND
  public override bool SafeDestination(vector query,out vector destination) SAFE
 }
 public static class Test {public static bool Enabled,Snapshot,Return;public static int Calls,Reads,Recorded,PortReads,MissingPort;public static string Details;public static ENodeResult NodeReturn;public static AIAgent ExpectedOwner;public static AICF_InfantryRecruitmentOrder ExpectedOrder;
  public static int Run(){int n=0;foreach(ENodeResult ret in Enum.GetValues(typeof(ENodeResult)))foreach(bool enabled in new[]{false,true})foreach(int missing in new[]{0,1,2,3,4}){Enabled=Snapshot=enabled;MissingPort=missing;Calls=Reads=Recorded=PortReads=0;ExpectedOrder=new AICF_InfantryRecruitmentOrder();ExpectedOwner=new AIAgent();NodeReturn=ret;var p=new Probe();if(p.EOnTaskSimulate(ExpectedOwner,0.25f)!=ret||Calls!=1||Reads!=1||Recorded!=1||PortReads!=(enabled?4:0))throw new Exception("node result/call/disabled/port mismatch");int handler=enabled&&missing!=2?9:-1;bool hasMove=enabled&&missing!=1;if(!Details.Contains("handler="+handler)||!Details.Contains("has_result="+hasMove)||!Details.Contains("internal_handler_result=UNKNOWN"))throw new Exception("port telemetry mismatch");n++;}
   foreach(bool ret in new[]{false,true})foreach(bool snap in new[]{false,true})foreach(int method in new[]{1,2,3,4}){Enabled=snap;Snapshot=snap;Return=ret;Calls=Reads=Recorded=0;ExpectedOrder=new AICF_InfantryRecruitmentOrder();var p=new Probe();bool got;vector output;
   if(method==1)got=p.IsCurrent(ExpectedOrder,13);else if(method==2)got=p.TryRecover(ExpectedOrder);else if(method==3){got=p.FindDestination(out output);if(output.X!=17)throw new Exception("find output changed");}else{got=p.SafeDestination(new vector{X=19},out output);if(output.X!=23)throw new Exception("safe output changed");}
   if(got!=ret||Calls!=1||Recorded!=1||Reads!=1)throw new Exception("result/delegate/snapshot mismatch "+method);n++;}return n;}
 }
}
'@
 $code=$code.Replace('NS',$ns).Replace('NODE',$node).Replace('CURRENT',$current).Replace('RECOVER',$recover).Replace('FIND',$find).Replace('SAFE',$safe)
 $code=$code.Replace('super.','base.').Replace('System.GetTickCount()','Clock.GetTickCount()').Replace('string details;','string details = "";')
 $code=$code.Replace('vector location = "0 0 0";','vector location = new vector();')
 foreach($pair in @(@('PORT_MOVE_RESULT, moveResult','PORT_MOVE_RESULT, out moveResult'),@('PORT_HANDLER_ID, handler','PORT_HANDLER_ID, out handler'),@('PORT_IS_WAYPOINT_RELATED, related','PORT_IS_WAYPOINT_RELATED, out related'),@('PORT_MOVE_LOCATION, location','PORT_MOVE_LOCATION, out location'))){$code=$code.Replace('GetVariableIn('+$pair[0]+')','GetVariableIn('+$pair[1]+')')}
 $code=[regex]::Replace($code,'%([1-9])',{param($m) '{'+([int]$m.Groups[1].Value-1)+'}'})
 $code=$code.Replace('AICF_InfantryRecruitmentOrder order;','AICF_InfantryRecruitmentOrder order = null;').Replace('AICF_GroupSlot slot;','AICF_GroupSlot slot = null;')
 $code=$code.Replace('base.FindDestination(destination)','base.FindDestination(out destination)').Replace('base.SafeDestination(query, destination)','base.SafeDestination(query, out destination)').Replace('base.SafeDestination(new vector(), destination)','base.SafeDestination(new vector(), out destination)')
 return $code
}
$actual=ReplayCode $source RecruitmentDecisionActual
Add-Type -TypeDefinition $actual
$cases=[RecruitmentDecisionActual.Test]::Run()
$same=Body $source SameOwner
$identityCode=@'
using System;namespace RecruitmentDecisionIdentity {
 public class Snapshot {public object m_Order,m_Slot,m_Group,m_World,m_Rpl,m_Waypoint,m_Actor,m_Agent,m_ActorRpl;public int m_GroupId,m_ActorId,m_iToken,m_iGeneration,m_iAssignment,m_iIntent,m_iGraph,m_iRepair;public string m_sRpl,m_sActorRpl;public bool m_bHadActor;public static implicit operator bool(Snapshot x){return x!=null;}public bool SameOwner(Snapshot other) BODY}
 public static class Cases {public static int Run(){int n=0;var a=new Snapshot();var fields=typeof(Snapshot).GetFields();foreach(var f in fields){if(f.FieldType==typeof(object))f.SetValue(a,new object());else if(f.FieldType==typeof(int))f.SetValue(a,1);else if(f.FieldType==typeof(string))f.SetValue(a,"id");else if(f.FieldType==typeof(bool))f.SetValue(a,true);}var b=new Snapshot();foreach(var f in fields)f.SetValue(b,f.GetValue(a));if(!a.SameOwner(b)||a.SameOwner(null))throw new Exception("positive/null");n+=2;foreach(var f in fields){object old=f.GetValue(b);if(f.FieldType==typeof(object))f.SetValue(b,new object());else if(f.FieldType==typeof(int))f.SetValue(b,2);else if(f.FieldType==typeof(string))f.SetValue(b,"other");else if(f.FieldType==typeof(bool))f.SetValue(b,false);if(a.SameOwner(b))throw new Exception("context replacement "+f.Name);f.SetValue(b,old);n++;}return n;}}
}
'@
$identityCode=$identityCode.Replace('BODY',$same)
Add-Type -TypeDefinition $identityCode
$identityCases=[RecruitmentDecisionIdentity.Cases]::Run()
$identityUnsafe=$identityCode.Replace('namespace RecruitmentDecisionIdentity','namespace RecruitmentDecisionIdentityUnsafe').Replace($same,'{ return true; }')
Add-Type -TypeDefinition $identityUnsafe
$caught=$false;try{[RecruitmentDecisionIdentityUnsafe.Cases]::Run()|Out-Null}catch{$caught=$true};if(!$caught){throw 'Unsafe callback owner continuity survived'}
$unsafe=@(
 @('super.EOnTaskSimulate(owner, dt)','super.EOnTaskSimulate(null, dt)'),
 @('super.EOnTaskSimulate(owner, dt)','super.EOnTaskSimulate(owner, dt + 1)'),
 @('return nodeResult;','return ENodeResult.SUCCESS;'),
 @('return result;','return !result;'),
 @('super.FindDestination(destination);','super.FindDestination(destination); destination = new vector();'),
 @('super.SafeDestination(query, destination);','super.SafeDestination(new vector(), destination);'),
 @('bool result = super.TryRecover(order);','bool result = super.TryRecover(order); super.TryRecover(order);')
)
$executed=0
foreach($m in $unsafe){if(!$source.Contains($m[0])){throw ('Stale executable mutant '+$m[0])};$ns='RecruitmentDecisionUnsafe'+$executed;$mutant=ReplayCode ($source.Replace($m[0],$m[1])) $ns;Add-Type -TypeDefinition $mutant;$caught=$false;try{([type]($ns+'.Test')).GetMethod('Run').Invoke($null,@())|Out-Null}catch{$caught=$true};if(!$caught){throw ('Unsafe wrapper survived '+$m[0])};$executed++}
"PASS Recruitment decision fixture: actual_wrappers=$cases actual_identity=$identityCases source_mutants=$($sourceMutants.Count) executed_unsafe=$($executed+1) native_boundaries_simulated=1 WB_NOT_RUN=1 native_NOT_RUN=1"

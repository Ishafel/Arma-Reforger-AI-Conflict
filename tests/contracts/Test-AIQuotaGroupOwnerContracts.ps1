param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$base=Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict'
function Body([string]$path,[string]$method){$s=[IO.File]::ReadAllText((Join-Path $base $path));Get-AICFMethodBody ([pscustomobject]@{Source=$s;Code=(ConvertTo-AICFCodeText $s)}) $method}
$source=[IO.File]::ReadAllText((Join-Path $base 'Forces/AICF_AIQuotaGroupOwner.c'))
$owner=Body 'Bootstrap/AICF_MatchController.c' 'OwnsAIQuotaGroupContext'
$providers=@{
 builder=(Body 'Construction/AICF_BaseBuilderService.c' 'OwnsAIQuotaBuilder');
 logistics=(Body 'Economy/AICF_LogisticsService.c' 'OwnsAIQuotaWorker');
 garrison=(Body 'Vehicles/AICF_FIAGarrisonService.c' 'OwnsAIQuotaGarrison');
 patrol=(Body 'Vehicles/AICF_FIAPatrolService.c' 'OwnsAIQuotaPatrol')
}
function Adapt([string]$s){
 $s=[regex]::Replace($s,'(?s)enum AICF_EAIQuotaGroupOwner\s*\{.*?\}','')
 $s=$s.Replace('EntityID','long').Replace('GetGame()','AICF_QuotaOwnerRuntime.Game')
 $s=$s.Replace('AICF_EAIQuotaGroupOwner kind','int kind').Replace('AICF_EAIQuotaGroupOwner m_Kind','int m_Kind').Replace('AICF_EAIQuotaBucket GetBucket()','int GetBucket()')
 $s=$s.Replace('m_sGroupRpl.IsEmpty()','string.IsNullOrEmpty(m_sGroupRpl)').Replace('FindComponent(RplComponent)','FindComponent(typeof(RplComponent))')
 $s=[regex]::Replace($s,'(?m)^(\s*)(static AICF_AIQuotaGroupOwner|bool|int) (\w+)\(','$1public $2 $3(')
 $s=[regex]::Replace($s,'(?m)^(\s*)(AICF_\w+|SCR_\w+|BaseWorld|Faction|RplComponent|string|int|long) (m_\w+);','$1public $2 $3;')
 $s=$s.Replace('class AICF_AIQuotaGroupOwner','public class AICF_AIQuotaGroupOwner : AICF_OwnerNative')
 return $s
}
$code=@'
using System;
using System.Collections.Generic;
public class AICF_OwnerNative {public static implicit operator bool(AICF_OwnerNative v){return v!=null;}}
public static class AICF_EAIQuotaGroupOwner {public const int SLOT=0,DONOR=1,BUILDER=2,LOGISTICS=3,FIA_GARRISON=4,FIA_DESANT=5,FIA_PATROL=6;}
public static class AICF_EAIQuotaBucket {public const int STOCK=0,COMBAT=1,BUILDERS=2,FLEET=3,FIA_EXTRA=4,STOCK_VEHICLES=5,COUNT=6;}
public static class Replication {public static bool Server=true;public static bool IsServer(){return Server;}}
public class RplId {public int Value=88;public bool IsValid(){return Value>0;}public override string ToString(){return Value.ToString();}}
public class RplComponent:AICF_OwnerNative {public RplId R=new RplId();public bool Master=true;public RplId Id(){return R;}public bool IsMaster(){return Master;}public static RplComponent Cast(object v){return v as RplComponent;}}
public class Faction:AICF_OwnerNative {}
public class BaseWorld:AICF_OwnerNative {public Dictionary<long,SCR_AIGroup> Groups=new Dictionary<long,SCR_AIGroup>();public SCR_AIGroup FindEntityByID(long id){SCR_AIGroup g;return Groups.TryGetValue(id,out g)?g:null;}}
public class SCR_AIWorld:AICF_OwnerNative {public static SCR_AIWorld Cast(SCR_AIWorld v){return v;}}
public class SCR_AIGroup:AICF_OwnerNative {public BaseWorld World;public long Id=7;public Faction Faction=new Faction();public RplComponent Rpl=new RplComponent();public BaseWorld GetWorld(){return World;}public long GetID(){return Id;}public Faction GetFaction(){return Faction;}public object FindComponent(Type type){return Rpl;}}
public class AICF_OwnerGame:AICF_OwnerNative {public BaseWorld World;public SCR_AIWorld AI=new SCR_AIWorld();public BaseWorld GetWorld(){return World;}public SCR_AIWorld GetAIWorld(){return AI;}}
public static class AICF_QuotaOwnerRuntime {public static AICF_OwnerGame Game;}
public class AICF_AIQuotaWorldState:AICF_OwnerNative {public bool Current=true;public AICF_MatchController Owner;public bool IsCurrentWorld(){return Current;}public bool IsSessionCurrent(AICF_MatchController owner,int epoch){return Current&&owner==Owner&&owner.Active&&epoch==1;}}
public class AICF_GroupSlot:AICF_OwnerNative {public SCR_AIGroup Group;public int Generation=2;public int GetSlotId(){return 0;}public SCR_AIGroup GetGroup(){return Group;}public int GetSpawnGeneration(){return Generation;}}
public class AICF_FactionState:AICF_OwnerNative {public AICF_GroupSlot Slot;public AICF_GroupSlot GetSlot(int slot){return Slot;}}
public class AICF_InfantryRecruitmentOrder:AICF_OwnerNative {public SCR_AIGroup m_Donor;public long m_DonorId=7;public int m_iGeneration=2,m_iToken=19;}
public class AICF_Recruits:AICF_OwnerNative {public AICF_InfantryRecruitmentOrder Order;public bool IsCurrentOrder(AICF_InfantryRecruitmentOrder r){return r!=null&&r==Order;}}
public class AICF_BaseBuilder:AICF_OwnerNative {public SCR_AIGroup m_Group;public long m_GroupId=7;public int m_iGeneration=2;public bool m_bRetiring;}
public class AICF_LogisticsWorker:AICF_OwnerNative {public bool m_bQuotaSpawnClosed;public SCR_AIGroup m_Group;public long m_GroupId=7;public int m_iGeneration=2;}
public class AICF_FIAGarrison:AICF_OwnerNative {public SCR_AIGroup m_Group,m_DesantGroup;public long m_GroupId=7,m_DesantGroupId=7;public int m_iGeneration=2;public bool m_bRetired;}
public class AICF_FIAPatrol:AICF_OwnerNative {public SCR_AIGroup m_Group;public long m_GroupId=7;public int m_iGeneration=2;public bool m_bRetired;}
public class AICF_Campaign:AICF_OwnerNative {public bool Master=true;public bool IsMaster(){return Master;}}
public class AICF_Registry:AICF_OwnerNative {public List<AICF_LogisticsWorker> m_aWorkers=new List<AICF_LogisticsWorker>();}
public class AICF_Plan:AICF_OwnerNative {public List<AICF_FIAGarrison> m_aPlan=new List<AICF_FIAGarrison>();}
public class AICF_Builders:AICF_OwnerNative
{
 public bool m_bStopped;public static AICF_Builders s_Instance;public AICF_Campaign m_Campaign=new AICF_Campaign();public List<AICF_BaseBuilder> m_aBuilders=new List<AICF_BaseBuilder>();
 public bool OwnsAIQuotaBuilder(AICF_BaseBuilder builder)
'@
$code+="{`n"+(Adapt $providers.builder)+"`n}}`n"
$code+=@'
public class AICF_Logistics:AICF_OwnerNative
{
 public bool m_bStopped;public AICF_Campaign m_Campaign=new AICF_Campaign();public AICF_Registry m_Registry=new AICF_Registry();
 public bool OwnsAIQuotaWorker(AICF_LogisticsWorker worker)
'@
$code+="{`n"+(Adapt $providers.logistics)+"`n}}`n"
$code+=@'
public class AICF_Garrisons:AICF_OwnerNative
{
 public bool m_bStopped,m_bPlanValid=true;public AICF_Plan m_Planner=new AICF_Plan();public static List<AICF_FIAGarrison> s_aDefenders=new List<AICF_FIAGarrison>();
 public bool OwnsAIQuotaGarrison(AICF_FIAGarrison garrison)
'@
$code+="{`n"+(Adapt $providers.garrison)+"`n}}`n"
$code+=@'
public class AICF_Patrols:AICF_OwnerNative
{
 public bool m_bStopped;public AICF_OwnerNative m_Graph=new AICF_OwnerNative(),m_Fleet=new AICF_OwnerNative();public List<AICF_FIAPatrol> m_aPatrols=new List<AICF_FIAPatrol>();
 public bool OwnsAIQuotaPatrol(AICF_FIAPatrol patrol)
'@
$code+="{`n"+(Adapt $providers.patrol)+"`n}}`n"
$code+=@'
public class AICF_MatchController:AICF_OwnerNative
{
 public bool Active=true;public AICF_FactionState m_USState,m_USSRState;public AICF_Recruits m_InfantryRecruitment=new AICF_Recruits();public AICF_Builders m_BaseBuilders=new AICF_Builders();public AICF_Logistics m_Logistics=new AICF_Logistics();public AICF_Garrisons m_FIAGarrisons=new AICF_Garrisons();public AICF_Patrols m_FIAPatrols=new AICF_Patrols();
 public bool IsAIQuotaOwnerCurrent(SCR_AIWorld w){return Active&&w==AICF_QuotaOwnerRuntime.Game.AI;}
 public bool OwnsAIQuotaGroupContext(AICF_AIQuotaGroupOwner record)
'@
$code+="{`n"+(Adapt $owner)+"`n}}`n"+(Adapt $source)
$code+=@'
public static class AICF_QuotaOwnerCases
{
 static int Cases;
 static void Need(bool yes,string reason){Cases++;if(!yes)throw new Exception(reason);}
 static AICF_AIQuotaGroupOwner Setup(int kind)
 {
  Replication.Server=true;var main=new BaseWorld();AICF_QuotaOwnerRuntime.Game=new AICF_OwnerGame{World=main};var g=new SCR_AIGroup{World=main};main.Groups.Add(g.Id,g);var c=new AICF_MatchController();var w=new AICF_AIQuotaWorldState{Owner=c};
  var record=AICF_AIQuotaGroupOwner.Capture(c,w,1,3,g,kind);if(record==null)throw new Exception("valid capture failed");record.m_iGeneration=2;record.m_iRequestToken=19;
  record.m_Slot=new AICF_GroupSlot{Group=g};record.m_FactionState=new AICF_FactionState{Slot=record.m_Slot};c.m_USState=record.m_FactionState;
  record.m_Recruit=new AICF_InfantryRecruitmentOrder{m_Donor=g};c.m_InfantryRecruitment.Order=record.m_Recruit;
  record.m_Builder=new AICF_BaseBuilder{m_Group=g};c.m_BaseBuilders.m_aBuilders.Add(record.m_Builder);AICF_Builders.s_Instance=c.m_BaseBuilders;
  record.m_Logistics=new AICF_LogisticsWorker{m_Group=g};c.m_Logistics.m_Registry.m_aWorkers.Add(record.m_Logistics);
  record.m_Garrison=new AICF_FIAGarrison{m_Group=g,m_DesantGroup=g};c.m_FIAGarrisons.m_Planner.m_aPlan.Add(record.m_Garrison);AICF_Garrisons.s_aDefenders.Clear();AICF_Garrisons.s_aDefenders.Add(record.m_Garrison);
  record.m_Patrol=new AICF_FIAPatrol{m_Group=g};c.m_FIAPatrols.m_aPatrols.Add(record.m_Patrol);return record;
 }
 public static int Run()
 {
  Cases=0;
  for(int kind=0;kind<=6;kind++)
  {
   var r=Setup(kind);Need(r.MatchesIdentity()&&r.CanDispatch(),"registered current exact domain owner");Need(r.GetBucket()==(kind==2?2:kind>=4?4:1),"bucket derives explicit context, not faction");
   r.m_iGeneration++;Need(!r.CanDispatch(),"stale domain incarnation rejected");r.m_iGeneration--;
   r.m_Controller.Active=false;Need(r.MatchesIdentity()&&!r.CanDispatch(),"historical identity retains bucket but cannot dispatch after owner stop");r.m_Controller.Active=true;
   r.m_iEpoch=2;Need(!r.CanDispatch(),"same-world new epoch rejects old callback");
  }
  var a=Setup(0);a.m_Slot=null;Need(!a.CanDispatch(),"same-faction foreign group has no registered slot ownership");
  a=Setup(0);a.m_FactionState=new AICF_FactionState{Slot=a.m_Slot};Need(!a.CanDispatch(),"foreign faction-state object rejected even if group faction matches");
  a=Setup(1);a.m_iRequestToken++;Need(!a.CanDispatch(),"donor request token replaced");
  a=Setup(1);a.m_Controller.m_InfantryRecruitment.Order=null;Need(!a.CanDispatch(),"old donor removed from service");
  a=Setup(2);a.m_Controller.m_BaseBuilders.m_aBuilders.Clear();Need(!a.CanDispatch(),"unregistered builder object rejected");
  a=Setup(2);a.m_Builder.m_bRetiring=true;Need(!a.CanDispatch(),"retiring builder cannot dispatch");
  a=Setup(2);AICF_Builders.s_Instance=new AICF_Builders();Need(!a.CanDispatch(),"old builder service object rejected");
  a=Setup(3);a.m_Controller.m_Logistics.m_Registry.m_aWorkers.Clear();Need(!a.CanDispatch(),"foreign logistics worker rejected");
  a=Setup(4);a.m_Controller.m_FIAGarrisons.m_Planner.m_aPlan.Clear();Need(!a.CanDispatch(),"unplanned project FIA group rejected");
  a=Setup(5);a.m_Garrison.m_DesantGroup=new SCR_AIGroup();Need(!a.CanDispatch(),"desant group replaced");
  a=Setup(6);a.m_Patrol.m_bRetired=true;Need(!a.CanDispatch(),"retired FIA patrol cannot dispatch");
  a=Setup(0);a.m_MainWorld.Groups.Clear();Need(!a.MatchesIdentity(),"deleted exact native group rejected");
  a=Setup(0);a.m_Group.Rpl=new RplComponent();Need(!a.MatchesIdentity(),"same textual component Rpl with replaced component pointer rejected");
  a=Setup(0);a.m_GroupRpl.R.Value++;Need(!a.MatchesIdentity(),"captured component replication epoch rejected");
  a=Setup(0);a.m_GroupRpl.Master=false;Need(!a.MatchesIdentity(),"non-authority group rejected");
  a=Setup(0);a.m_Group.Faction=new Faction();Need(!a.MatchesIdentity(),"ownership transfer faction pointer rejected");
  a=Setup(0);a.m_Kind=99;Need(a.GetBucket()==-1&&!a.CanDispatch(),"unknown context cannot become project FIA");
  a=Setup(0);Replication.Server=false;Need(!a.MatchesIdentity()&&!a.CanDispatch(),"client cannot register current ownership");
  return Cases;
 }
}
'@
Add-Type -TypeDefinition $code -Language CSharp
$cases=[AICF_QuotaOwnerCases]::Run()
"PASS AI quota typed group owner actual_extracted_cases=$cases role_contexts=7 runtime=NOT_RUN"

param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
# Reuse native adapters and actual owner/provider bodies, without running that
# contract's Add-Type/entrypoint. Both files are part of the source inventory.
$ownerContract=[IO.File]::ReadAllText((Join-Path $PSScriptRoot 'Test-AIQuotaGroupOwnerContracts.ps1'))
$end=$ownerContract.IndexOf('Add-Type -TypeDefinition $code')
if($end-lt0){throw 'Owner adapter boundary missing'}
$script:AICFQuotaOwnerAdapterDirectory=$PSScriptRoot
$ownerPrefix=$ownerContract.Substring(0,$end).Replace('$PSScriptRoot','$script:AICFQuotaOwnerAdapterDirectory')
. ([scriptblock]::Create($ownerPrefix)) -RepositoryRoot $RepositoryRoot
$code=$code.Replace('static AICF_AIQuotaGroupOwner Setup(int kind)','public static AICF_AIQuotaGroupOwner Setup(int kind)')
$registry=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Forces/AICF_AIQuotaRegistry.c'))
if($registry-match'\b(SetLimitOfActiveAIs|SpawnEntity|DeleteRplEntity|ActivateAI|SetLOD|CallLater)\s*\('){throw 'Registry changes native gameplay'}
function AdaptRegistry([string]$text){
 $text=Adapt $text
 $text=$text.Replace('out AICF_EAIQuotaBucket bucket','out int bucket').Replace('int.MAX','int.MaxValue')
 $text=[regex]::Replace($text,'(?m)^(\s*)(AICF_AIQuotaGroupOwner) (\w+)\(','$1public $2 $3(')
 $text=$text.Replace('void AICF_AIQuotaRegistry(','public AICF_AIQuotaRegistry(')
 $text=$text.Replace('class AICF_AIQuotaRegistry','public class AICF_AIQuotaRegistry : AICF_OwnerNative')
 $text=$text.Replace('protected ref array<ref AICF_AIQuotaGroupOwner> m_aGroups = {};','protected List<AICF_AIQuotaGroupOwner> m_aGroups = new List<AICF_AIQuotaGroupOwner>();')
 $text=$text.Replace('m_aGroups.Count()','m_aGroups.Count').Replace('m_aGroups.Insert(record)','m_aGroups.Add(record)').Replace('m_aGroups.Remove(index)','m_aGroups.RemoveAt(index)')
 $text=[regex]::Replace($text,'foreach \((AICF_AIQuotaGroupOwner) (\w+) : m_aGroups\)','foreach ($1 $2 in m_aGroups)')
 return $text
}
$code+="`npublic static class AICF_AIQuotaMath {public const int MAX_TOTAL_UNITS=4096;}`n"+(AdaptRegistry $registry)
$code+=@'
public static class AICF_QuotaRegistryCases
{
 static int Cases;
 static void Need(bool v,string reason){Cases++;if(!v)throw new Exception(reason);}
 static AICF_AIQuotaGroupOwner Register(AICF_AIQuotaRegistry registry,AICF_AIQuotaGroupOwner r,int kind)
 {
  switch(kind){case 0:return registry.RegisterSlot(r.m_FactionState,r.m_Slot);case 1:return registry.RegisterDonor(r.m_Recruit);case 2:return registry.RegisterBuilder(r.m_Builder);case 3:return registry.RegisterLogistics(r.m_Logistics);case 4:return registry.RegisterGarrison(r.m_Garrison,false);case 5:return registry.RegisterGarrison(r.m_Garrison,true);case 6:return registry.RegisterPatrol(r.m_Patrol);default:return null;}
 }
 public static int Run()
 {
  Cases=0;
  for(int kind=0;kind<=6;kind++)
  {
   var context=AICF_QuotaOwnerCases.Setup(kind);var registry=new AICF_AIQuotaRegistry(context.m_WorldState);Need(registry.BeginSession(context.m_Controller,1),"actual session initialized");
   var record=Register(registry,context,kind);Need(record!=null&&record.CanDispatch(),"actual typed registration captures exact domain");
   Need(registry.Find(context.m_Group)==record,"actual group object lookup");Need(Register(registry,context,kind)==record,"repeated identical registration returns original serial");
   int bucket;Need(registry.ClassifyGroup(context.m_Group,out bucket)&&bucket==record.GetBucket(),"registered ownership maps exact bucket");
   var stock=new SCR_AIGroup{World=context.m_MainWorld,Id=8,Faction=context.m_Faction};context.m_MainWorld.Groups.Add(8,stock);
   Need(registry.ClassifyGroup(stock,out bucket)&&bucket==0,"same-faction unowned stock/FIA stays STOCK");
   context.m_Controller.Active=false;Need(!record.CanDispatch()&&registry.ClassifyGroup(context.m_Group,out bucket)&&bucket==record.GetBucket(),"old physical ownership retained after controller stops");
   Need(Register(registry,context,kind)==null,"stopped owner cannot reserve another callback");
   context.m_Controller.Active=true;context.m_Group.Rpl.R.Value++;Need(!registry.ClassifyGroup(context.m_Group,out bucket),"stale registered replication identity never falls through STOCK");
  }
  var c=AICF_QuotaOwnerCases.Setup(0);var q=new AICF_AIQuotaRegistry(c.m_WorldState);Need(!q.BeginSession(new AICF_MatchController(),1),"foreign root cannot own registry session");Need(q.BeginSession(c.m_Controller,1),"current owner follows failed foreign attempt");
  var first=q.RegisterSlot(c.m_FactionState,c.m_Slot);Need(first!=null,"first slot generation");c.m_Slot.Generation++;Need(q.RegisterSlot(c.m_FactionState,c.m_Slot)==null,"same group pointer cannot replace old generation ownership");
  int b;c.m_WorldState.Current=false;Need(!q.ClassifyGroup(null,out b),"stale world cannot classify UNKNOWN as free stock");
  c=AICF_QuotaOwnerCases.Setup(2);q=new AICF_AIQuotaRegistry(c.m_WorldState);q.BeginSession(c.m_Controller,1);c.m_Controller.m_BaseBuilders.m_aBuilders.Clear();Need(q.RegisterBuilder(c.m_Builder)==null,"unregistered same-faction builder cannot claim managed reserve");
  c=AICF_QuotaOwnerCases.Setup(4);q=new AICF_AIQuotaRegistry(c.m_WorldState);q.BeginSession(c.m_Controller,1);c.m_Controller.m_FIAGarrisons.m_Planner.m_aPlan.Clear();Need(q.RegisterGarrison(c.m_Garrison,false)==null,"foreign unplanned FIA cannot claim FIA_EXTRA");
  Need(q.RegisterSlot(null,null)==null&&q.RegisterDonor(null)==null&&q.RegisterBuilder(null)==null&&q.RegisterLogistics(null)==null&&q.RegisterGarrison(null,false)==null&&q.RegisterPatrol(null)==null,"null domain contexts cannot create claims");
  return Cases;
 }
}
'@
Add-Type -TypeDefinition $code -Language CSharp
$cases=[AICF_QuotaRegistryCases]::Run()
"PASS AI quota registry actual_extracted_cases=$cases same_faction_contexts=7 native_integration=NOT_IMPLEMENTED runtime=NOT_RUN"

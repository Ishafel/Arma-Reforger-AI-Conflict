param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
$source=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Forces/AICF_AIQuotaPlannedGroup.c'))
function Adapt([string]$text){
 $text=$text.Replace('ref AICF_AIQuotaReservation','AICF_AIQuotaReservation').Replace('int.MAX','int.MaxValue')
 $text=[regex]::Replace($text,'(?m)^class AICF_AIQuotaPlannedGroup','public class AICF_AIQuotaPlannedGroup:N')
 $text=[regex]::Replace($text,'(?m)^(\s*)(AICF_\w+|int|bool) (m_\w+);','$1public $2 $3;')
 $text=[regex]::Replace($text,'(?m)^(\s*)bool (\w+)\(','$1public bool $2(')
 return $text
}
$preamble=@'
using System;
public class N {public static implicit operator bool(N value){return value!=null;}}
public enum AICF_EAIQuotaGroupOwner {SLOT,DONOR,BUILDER}
public enum AICF_EAIQuotaBucket {STOCK,COMBAT,BUILDERS}
public class AICF_AIQuotaReservation:N{}
public class Group:N {public int Id=10;}
public class AICF_FactionState:N{}
public class AICF_GroupSlot:N {public int Gen,Units=5;public Group Group;public Group GetGroup(){return Group;}public int GetSpawnGeneration(){return Gen;}public int GetDeploymentSize(){return Units;}}
public class AICF_MatchController:N {public bool Current=true;public bool IsAIQuotaSlotCurrent(AICF_FactionState s,AICF_GroupSlot slot){return Current&&s!=null&&slot!=null;}}
public class AICF_AIWorldQuotaService:N {public bool Current=true;public AICF_MatchController Root=new AICF_MatchController();public int Epoch=1;public bool IsOwnerCurrent(){return Current;}public AICF_MatchController GetQuotaController(){return Root;}public int GetEpoch(){return Epoch;}}
public class RecruitService:N {public bool Current=true;public bool IsCurrentOrder(AICF_InfantryRecruitmentOrder r){return Current;}}
public class AICF_InfantryRecruitmentOrder:N {public AICF_GroupSlot m_Slot=new AICF_GroupSlot();public RecruitService m_RecruitmentService=new RecruitService();public Group m_Donor;public int m_DonorId,m_iGeneration=3,m_iToken=40;public bool Current=true;public bool IsCurrent(AICF_GroupSlot slot){return Current&&slot==m_Slot;}}
public class AICF_AIQuotaGroupOwner:N {public bool Current=true;public AICF_MatchController m_Controller;public int m_iEpoch=1,m_iGeneration,m_iRequestToken,m_GroupId;public AICF_EAIQuotaGroupOwner m_Kind;public AICF_EAIQuotaBucket Bucket=AICF_EAIQuotaBucket.COMBAT;public AICF_FactionState m_FactionState;public AICF_GroupSlot m_Slot;public AICF_InfantryRecruitmentOrder m_Recruit;public Group m_Group;public bool CanDispatch(){return Current;}public AICF_EAIQuotaBucket GetBucket(){return Bucket;}}
'@
$cases=@'
public static class Cases {
 static AICF_AIQuotaPlannedGroup Slot(){var s=new AICF_AIWorldQuotaService();return new AICF_AIQuotaPlannedGroup{m_Service=s,m_Controller=s.Root,m_iEpoch=1,m_iSerial=2,m_iUnits=5,m_State=new AICF_FactionState(),m_Slot=new AICF_GroupSlot(),m_iGeneration=0};}
 static AICF_AIQuotaGroupOwner Bound(AICF_AIQuotaPlannedGroup p){var g=new Group();p.m_Slot.Gen=1;p.m_Slot.Group=g;return new AICF_AIQuotaGroupOwner{m_Controller=p.m_Controller,m_iEpoch=1,m_iGeneration=1,m_FactionState=p.m_State,m_Slot=p.m_Slot,m_Group=g};}
 public static int Execute(){int n=0;Action<bool,string> need=(ok,label)=>{if(!ok)throw new Exception(label);n++;};var p=Slot();need(p.IsWaitingCurrent()&&p.IsPendingCurrent(),"stable waiting0 before generation");var o=Bound(p);need(!p.IsWaitingCurrent()&&p.IsPendingCurrent()&&p.CanBind(o),"one exact next generation can adopt full basket");p.m_bBound=true;need(!p.CanBind(o)&&!p.IsPendingCurrent(),"already adopted receipt cannot bind again");
 p=Slot();o=Bound(p);o.m_Controller=new AICF_MatchController();need(!p.CanBind(o),"foreign root");o.m_Controller=p.m_Controller;o.m_iEpoch=2;need(!p.CanBind(o),"foreign session");o.m_iEpoch=1;o.m_iGeneration=2;need(!p.CanBind(o),"wrong spawn generation");o.m_iGeneration=1;o.Bucket=AICF_EAIQuotaBucket.STOCK;need(!p.CanBind(o),"same faction stock not owned combat");o.Bucket=AICF_EAIQuotaBucket.COMBAT;o.m_Kind=AICF_EAIQuotaGroupOwner.BUILDER;need(!p.CanBind(o),"wrong owner kind");o.m_Kind=AICF_EAIQuotaGroupOwner.SLOT;o.m_Slot=new AICF_GroupSlot();need(!p.CanBind(o),"foreign stable slot");o.m_Slot=p.m_Slot;o.m_Group=new Group();need(!p.CanBind(o),"different actual controller group");o.m_Group=p.m_Slot.Group;p.m_Slot.Units=4;need(!p.CanBind(o),"changed expected roster");p.m_Slot.Units=5;o.Current=false;need(!p.CanBind(o),"stale native owner fence");
 p=Slot();p.m_iGeneration=int.MaxValue;p.m_Slot.Gen=int.MaxValue;o=new AICF_AIQuotaGroupOwner{m_Controller=p.m_Controller,m_FactionState=p.m_State,m_Slot=p.m_Slot,m_Group=new Group(),m_iGeneration=int.MinValue};p.m_Slot.Group=o.m_Group;need(!p.CanBind(o),"generation overflow cannot bind");
 p=Slot();p.m_Slot.Gen=1;need(p.IsPendingCurrent(),"native controller factory callback preserves in-transition basket");p.m_Slot.Gen=2;need(!p.IsPendingCurrent(),"later attempt cannot inherit old basket");p=Slot();p.m_Controller.Current=false;need(!p.IsWaitingCurrent()&&!p.IsPendingCurrent(),"inactive root no admission");
 p=Slot();p.m_State=null;p.m_Slot=null;p.m_Recruit=new AICF_InfantryRecruitmentOrder();p.m_iUnits=1;p.m_iGeneration=3;p.m_iToken=40;need(p.IsWaitingCurrent(),"exact donor request before native controller creation");var donor=new Group();p.m_Recruit.m_Donor=donor;p.m_Recruit.m_DonorId=donor.Id;o=new AICF_AIQuotaGroupOwner{m_Controller=p.m_Controller,m_iGeneration=3,m_iRequestToken=40,m_Kind=AICF_EAIQuotaGroupOwner.DONOR,m_Recruit=p.m_Recruit,m_Group=donor,m_GroupId=donor.Id};need(!p.IsWaitingCurrent()&&p.IsPendingCurrent()&&p.CanBind(o),"same token donor adopts before request");o.m_iRequestToken=41;need(!p.CanBind(o),"donor wrong token");o.m_iRequestToken=40;o.m_GroupId=11;need(!p.CanBind(o),"wrong captured donor entityID");o.m_GroupId=donor.Id;p.m_Recruit.m_RecruitmentService.Current=false;need(!p.IsPendingCurrent(),"ended visit retires available basket");p.m_Recruit.m_RecruitmentService.Current=true;p.m_Recruit.m_iToken=41;need(!p.IsPendingCurrent(),"reused request object token cannot retain basket");return n;}
}
'@
$code=$preamble+(Adapt $source)+$cases
Add-Type $code
$actual=[Cases]::Execute()
$unsafe=0
foreach($mutation in @(
 @('slot-generation','owner.m_iGeneration == m_iGeneration + 1','true'),
 @('donor-token','owner.m_iRequestToken == m_iToken','true'),
 @('bound-reuse','&& !m_bBound',''),
 @('wrong-body','m_Slot.GetGroup() == owner.m_Group','true'),
 @('wrong-donor-ID','m_Recruit.m_DonorId == owner.m_GroupId','true')
)){
 if(!$source.Contains($mutation[1])){throw "Missing unsafe target $($mutation[0])"}
 $name='PlannedUnsafe'+[guid]::NewGuid().ToString('N')
 $mutated=($preamble+(Adapt $source.Replace($mutation[1],$mutation[2]))+$cases).Replace('public class N {',"namespace $name {public class N {")+'}'
 Add-Type $mutated
 $failed=$false;try{$null=([type]("$name.Cases")).GetMethod('Execute').Invoke($null,@())}catch{$failed=$true}
 if(!$failed){throw "Unsafe planned group survived $($mutation[0])"};$unsafe++
}
"PASS AI quota planned group actual=$actual executed_unsafe=$unsafe generation_token_preserved=1 runtime=NOT_RUN"

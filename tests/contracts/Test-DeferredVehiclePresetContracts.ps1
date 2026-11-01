param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$root=Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict'
function Read([string]$p){[IO.File]::ReadAllText((Join-Path $root $p))}
function Body([string]$s,[string]$m){Get-AICFMethodBody ([pscustomobject]@{Source=$s;Code=(ConvertTo-AICFCodeText $s)}) $m}
$model=Read 'Vehicles/AICF_VehiclePreset.c'
$deferred=Read 'State/Vehicles/AICF_DeferredVehiclePreset.c'
$service=Read 'UI/AICF_VehiclePresetService.c'
$coordinator=Read 'Vehicles/AICF_VehicleCoordinator.c'
$editor=Read 'UI/AICF_VehiclePresetEditor.c'
$planned=(Body $model 'ValidatePlannedRoster').Replace('for (int seatIndex;','for (int seatIndex = 0;').Replace('seats.Count()','seats.Count').Replace('int pilots, assigned;','int pilots=0, assigned=0;')
$needs=Body $deferred 'NeedsRoster'
$current=(Body $deferred 'ConfirmationCurrent').Replace('for (int index;','for (int index = 0;').Replace('m_aMembers.Count()','m_aMembers.Count')
$binding=Body $deferred 'BindingCurrent'
$clear=Body (Read 'State/AICF_GroupSlot.c') 'ClearDeferredVehicleCrew'
$update=(Body $service 'UpdateDeferred').Replace('AICF_DeferredVehiclePreset','Deferred').Replace('AICF_GroupSlot','Slot').Replace('AICF_VehiclePresetCatalog','Catalog').Replace('AICF_VehicleCrewPlan','Plan').Replace('AICF_VehiclePreset','Preset').Replace('m_aDeferred.Count()','m_aDeferred.Count').Replace('m_aDeferred.Remove(index)','m_aDeferred.RemoveAt(index)').Replace('pending.m_Faction, reason)','pending.m_Faction, out reason)').Replace('candidate.m_sPrefab, reason)','candidate.m_sPrefab, out reason)').Replace('catalog.Seats(), reason, true)','catalog.Seats(), out reason, true)').Replace('candidate, reason)','candidate, out reason)')
$stop=(Body $service 'Stop').Replace('foreach (AICF_VehiclePresetViewer viewer : m_aViewers)','foreach (Viewer viewer in m_aViewers)').Replace('foreach (AICF_DeferredVehiclePreset pending : m_aDeferred)','foreach (Deferred pending in m_aDeferred)')
# Native v1 exposed a NULL after the inline missing-member/continue branch.
# C# replay alone accepted that body; retain the explicit native-safe else.
if($current -notmatch 'else if \(member.GetID\(\) != m_aMembers\[index\]\)' -or $current -match 'continue;'){throw 'Deferred confirmation lacks native missing-member branch fence'}
$shim=@'
using System;
using System.Collections.Generic;
namespace __NS__ {
public class Native {public static implicit operator bool(Native n){return n!=null;}}
public static class Replication {public static bool Server=true;public static bool IsServer(){return Server;}}
public static class Clock {public static int Now=100;}
public static class AICF_Stage1Config {public const int MAX_GROUP_SIZE=10;}
public static class ECompartmentType {public const int PILOT=0,TURRET=1,CARGO=2;}
public class Member:Native {public int Id;public int GetID(){return Id;}}
public class Group:Native {public int Id=3,Alive=1;public Faction Faction=new Faction();public int GetID(){return Id;}public Faction GetFaction(){return Faction;}}
public class Faction:Native{}
public static class AICF_GroupRuntime {public static int CountAliveAgents(Group g){return g.Alive;}}
public class Slot:Native {public int Desired=10,Revision=4,Generation=2,m_iDeferredVehicleCrewRevision=4;public bool Ready=true,Recruiting=true;public Group Group=new Group();public string Data="preset";public Member[] Members=new Member[10];public int GetDesiredSize(){return Desired;}public bool IsCombatReady(){return Ready;}public Group GetGroup(){return Group;}public bool IsRecruitingInfantry(){return Recruiting;}public int GetVehiclePresetRevision(){return Revision;}public int GetSpawnGeneration(){return Generation;}public int GetStrategicAssignmentRevision(){return 0;}public int GetLoadoutRevision(){return 0;}public string GetVehiclePresetData(){return Data;}public Member GetRosterMember(int i){return Members[i];}public void ClearDeferredVehicleCrew(int revision){__CLEAR__}}
public class BaseCompartmentSlot {public int Type;public bool Attached;public new int GetType(){return Type;}public bool GetAttachedTurret(){return Attached;}}
public static class AICF_ContentProfile {public static AICF_Profile GetActive(){return new AICF_Profile();}}
public class AICF_Profile {public bool IsVehiclePresetCrewSeat(string prefab,string faction,BaseCompartmentSlot seat){return seat.GetType()==ECompartmentType.PILOT||seat.GetType()==ECompartmentType.TURRET||seat.GetAttachedTurret();}}
public class Preset:Native {public string m_sPrefab="uaz",m_sFaction="USSR";public bool Shape=true,Topology=true;public List<int> m_aMembers=new List<int>();public bool ValidateShape(out string reason){reason="SHAPE";return Shape;}public bool MatchesSeats(List<BaseCompartmentSlot> s){return Topology;}public bool ValidatePlannedRoster(Slot slot,List<BaseCompartmentSlot> seats,out string reason){__PLANNED__}public static Preset Decode(string s){return new Preset();}public bool ValidateRoster(Slot s,List<BaseCompartmentSlot> seats,out string reason,bool partial){reason="STUB_NOT_USED";return false;}}
public class Deferred {public Slot m_Slot;public Faction m_Faction;public Group m_Group;public int m_GroupId=3,m_iVehicleRevision=4,m_iGeneration=2,m_iAssignment=0,m_iLoadout=0,m_iSize=10,m_iExpires=1000;public string m_sPreset="preset";public List<int> m_aMembers=new List<int>();public static bool NeedsRoster(Slot slot){__NEEDS__}public bool ConfirmationCurrent(Slot slot,string data){__CURRENT__}public bool BindingCurrent(){__BINDING__}}
public class Plan {public Plan(Slot s,Preset p){if(s==null)throw new Exception("NULL_SLOT_PLAN");}}
public class Catalog {public Catalog(Faction f){}public bool Inspect(string p,out string reason){reason="STUB_NOT_USED";return false;}public List<BaseCompartmentSlot> Seats(){return new List<BaseCompartmentSlot>();}public void Clear(){}}
public class ReturnService {public bool CanReturn(Plan p,Faction f,out string reason){reason="STUB_NOT_USED";return false;}public bool Commit(Plan p,Faction f,Preset c,out string reason){reason="STUB_NOT_USED";return false;}}
public class Match {public int Calls;public bool CanResizeVehicleCrew(Slot s){Calls++;if(s==null)throw new Exception("NULL_SLOT_DISPATCH");return false;}public ReturnService VehicleCrewReturn(){return new ReturnService();}}
public class Player:Native {public string View="original";public void AICF_PublishVehiclePresetView(string s){View=s;}}
public class Viewer {public Player m_Player;}
public class Service {public List<Deferred> m_aDeferred=new List<Deferred>();public List<Viewer> m_aViewers=new List<Viewer>();public Match m_Match=new Match();public void UpdateDeferred(){__UPDATE__}public void Stop(){__STOP__}public void UpdateUnsafe(){__UPDATE_UNSAFE__}public void StopUnsafe(){__STOP_UNSAFE__}}
public static class Suite {
static int N;static void Need(bool b,string s){if(!b)throw new Exception(s);N++;}
static List<BaseCompartmentSlot> Seats(int n){var a=new List<BaseCompartmentSlot>();for(int i=0;i<n;i++)a.Add(new BaseCompartmentSlot{Type=i==0?0:2});return a;}
static Preset P(int n){var p=new Preset();for(int i=0;i<n;i++)p.m_aMembers.Add(i);return p;}
static Deferred D(Slot s){var d=new Deferred{m_Slot=s,m_Group=s.Group,m_Faction=s.Group.Faction};for(int i=0;i<10;i++)d.m_aMembers.Add(s.Members[i]==null?-1:s.Members[i].Id);return d;}
public static int Run(){N=0;string r;var s=new Slot();var p=P(10);var seats=Seats(10);
Need(p.ValidatePlannedRoster(s,seats,out r),"future full roster assignable with only seed");Need(s.Group.Alive==1&&s.Desired==10,"validation preserves live and desired roster");
s.Group=null;s.Ready=false;Need(p.ValidatePlannedRoster(s,seats,out r),"replacement with no group remains configurable");s=new Slot();p=P(3);seats=Seats(3);Need(p.ValidatePlannedRoster(s,seats,out r),"small machine plan can request explicit reduction");p.m_aMembers[2]=9;Need(p.ValidatePlannedRoster(s,seats,out r),"future numeric position selectable");p.m_aMembers[2]=10;Need(!p.ValidatePlannedRoster(s,seats,out r)&&r=="MEMBER_INACTIVE","out of desired position refused");p=P(3);p.m_aMembers[0]=-1;Need(!p.ValidatePlannedRoster(s,seats,out r)&&r=="MANDATORY_CREW_MISSING","driver required");p=P(3);seats[1].Type=1;p.m_aMembers[1]=-1;Need(!p.ValidatePlannedRoster(s,seats,out r)&&r=="MANDATORY_CREW_MISSING","gunner required");seats[1].Type=2;seats[1].Attached=true;Need(!p.ValidatePlannedRoster(s,seats,out r),"attached turret required");seats=Seats(3);seats[1].Type=0;p=P(3);Need(!p.ValidatePlannedRoster(s,seats,out r)&&r=="DRIVER_SEAT_MISSING","multiple pilots refused");seats=Seats(3);p.Topology=false;Need(!p.ValidatePlannedRoster(s,seats,out r)&&r=="SEAT_TOPOLOGY_CHANGED","topology required");p.Topology=true;p.Shape=false;Need(!p.ValidatePlannedRoster(s,seats,out r),"shape gate retained");s.Desired=0;p.Shape=true;Need(!p.ValidatePlannedRoster(s,seats,out r),"invalid desired size refused");
s=new Slot();Need(Deferred.NeedsRoster(s),"recruiting waits");s.Recruiting=false;Need(Deferred.NeedsRoster(s),"partial live roster waits");s.Group.Alive=10;Need(!Deferred.NeedsRoster(s),"full ready roster released");s.Ready=false;Need(Deferred.NeedsRoster(s),"spawn readiness retained");s.Ready=true;s.Group=null;Need(Deferred.NeedsRoster(s),"replacement waits");Need(!Deferred.NeedsRoster(null),"null not a permission");
s=new Slot();s.Members[0]=new Member{Id=7};var d=D(s);Need(d.ConfirmationCurrent(s,"preset"),"confirmation current");s.Members[1]=new Member{Id=8};Need(!d.ConfirmationCurrent(s,"preset"),"new recruit invalidates unconfirmed snapshot");s.Members[1]=null;s.Generation++;Need(!d.ConfirmationCurrent(s,"preset"),"generation invalidates confirmation");s.Generation--;s.Desired--;Need(!d.ConfirmationCurrent(s,"preset"),"desired size invalidates confirmation");s.Desired++;s.Revision++;Need(!d.ConfirmationCurrent(s,"preset"),"revision invalidates confirmation");s.Revision--;Clock.Now=1001;Need(!d.ConfirmationCurrent(s,"preset"),"expired confirmation refused");Clock.Now=100;Need(!d.ConfirmationCurrent(s,"different"),"different recipe refused");Need(d.BindingCurrent(),"confirmed binding current");s.Generation++;s.Group=new Group{Id=9,Faction=d.m_Faction};Need(d.BindingCurrent(),"confirmed stable-slot intent survives replacement");s.Data="different";Need(!d.BindingCurrent(),"changed binding cancels deferred reduction");s.Data="preset";s.Group.Faction=new Faction();Need(!d.BindingCurrent(),"foreign faction cancels");s.Group.Faction=d.m_Faction;Replication.Server=false;Need(!d.BindingCurrent()&&!d.ConfirmationCurrent(s,"preset"),"authority required");Replication.Server=true;
var service=new Service();s=new Slot();d=D(s);d.m_Slot=null;service.m_aDeferred.Add(d);service.UpdateDeferred();Need(service.m_aDeferred.Count==0&&service.m_Match.Calls==0,"actual UpdateDeferred NULL owner removes record without dispatch");
service=new Service();s=new Slot();d=D(s);s.Revision++;service.m_aDeferred.Add(d);service.UpdateDeferred();Need(service.m_aDeferred.Count==0&&s.m_iDeferredVehicleCrewRevision==-1&&service.m_Match.Calls==0,"actual UpdateDeferred invalid binding clears only its revision");
service=new Service();s=new Slot();d=D(s);s.Revision++;s.m_iDeferredVehicleCrewRevision=5;service.m_aDeferred.Add(d);service.UpdateDeferred();Need(s.m_iDeferredVehicleCrewRevision==5,"actual invalid wrapper preserves newer marker");
service=new Service();s=new Slot();d=D(s);d.m_Slot=null;service.m_aDeferred.Add(d);var live=D(s);service.m_aDeferred.Add(live);var viewer=new Viewer{m_Player=new Player()};service.m_aViewers.Add(viewer);service.Stop();Need(service.m_aDeferred.Count==0&&service.m_aViewers.Count==0&&service.m_Match==null&&viewer.m_Player.View==""&&s.m_iDeferredVehicleCrewRevision==-1,"actual Stop handles NULL and valid owners plus viewer cleanup");
service=new Service();s=new Slot();d=D(s);d.m_Slot=null;service.m_aDeferred.Add(d);bool caught=false;try{service.UpdateUnsafe();}catch(NullReferenceException){caught=true;}Need(caught,"unsafe NULL UpdateDeferred guard removal rejected");
service=new Service();service.m_aDeferred.Add(d);caught=false;try{service.StopUnsafe();}catch(NullReferenceException){caught=true;}Need(caught,"unsafe NULL Stop guard removal rejected");
return N;}
}
}
'@
$shim=$shim.Replace('__PLANNED__',$planned).Replace('__NEEDS__',$needs).Replace('__CURRENT__',$current.Replace('System.GetTickCount()','Clock.Now')).Replace('__BINDING__',$binding).Replace('__CLEAR__',$clear).Replace('__UPDATE__',$update).Replace('__STOP__',$stop).Replace('__UPDATE_UNSAFE__',$update.Replace('if (pending.m_Slot) ','')).Replace('__STOP_UNSAFE__',$stop.Replace('if (pending.m_Slot) ','')).Replace('EntityID.INVALID','-1').Replace('ValidateShape(reason)','ValidateShape(out reason)').Replace('IEntity member','Member member')
$ns='Deferred'+[guid]::NewGuid().ToString('N')
Add-Type -TypeDefinition $shim.Replace('__NS__',$ns) -WarningAction SilentlyContinue
$count=([type]($ns+'.Suite'))::Run()
foreach($guard in @('ValidatePlannedRoster','CONFIRM_CREW && deferredConfirmation','m_aDeferred.Insert(planned)','slot.MarkDeferredVehicleCrew()','planned.m_iAssigned < slot.GetDesiredSize()','WAITING_FOR_REINFORCEMENTS')) {if(!$service.Contains($guard)){throw "Missing service boundary $guard"}}
$update=Body $service 'UpdateDeferred'
foreach($guard in @('BindingCurrent()','NeedsRoster(slot)','CanResizeVehicleCrew(slot)','ValidateRoster(slot, catalog.Seats(), reason, true)','new AICF_VehicleCrewPlan(slot, candidate)','VehicleCrewReturn().Commit')){if(!$update.Contains($guard)){throw "Missing deferred execution boundary $guard"}}
if($planned -match 'SpawnEntity|SetDesiredSize|SetVehiclePreset|AddSupplies|Commit\('){throw 'Planned validation changed gameplay'}
$admission=Body $coordinator 'IsAdmissionEligible'
if(!$admission.Contains('slot.IsVehicleCrewResizePending()') -or !$admission.Contains('AICF_DeferredVehiclePreset.NeedsRoster(slot)')){throw 'Pending roster admitted to vehicle execution'}
if(!(Body $service 'Publish').Contains('json.WriteValue("activeMembers", activeMembers)') -or !(Body $service 'Publish').Contains('json.WriteValue("selectableMembers", selectableMembers)')){throw 'Future members misreported as alive'}
if(!(Body $editor 'OnClick').Contains('AutoAssign(m_Draft, m_aSelectableMembers, m_aMandatorySeats)')){throw 'Auto seats cannot select future roster'}
Write-Output "PASS deferred vehicle preset actual predicates checks=$count; authority/confirmation/runtime boundaries retained"

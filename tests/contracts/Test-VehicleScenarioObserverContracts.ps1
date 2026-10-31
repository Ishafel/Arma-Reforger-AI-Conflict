param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$source=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'tests/fixtures/AICF_VehicleScenarioProbe.c'))
$parsed=[pscustomobject]@{Source=$source;Code=(ConvertTo-AICFCodeText $source)}
function Body([string]$name){Get-AICFMethodBody $parsed $name}
$guard=Body 'ResolveCurrent';$update=Body 'AICF_VSUpdateObserver';$stop=Body 'AICF_VSStopObserver'
if($source -notmatch 'class AICF_VSObserverPolicy : AICF_ManagedObservers' -or $guard -notmatch 'return ResolveMember\(group, expected\)'){throw 'Use real root native leader/faction policy'}
if($source -notmatch 'AICF_VS_OBSERVER_KEY = -1513170001;' -or $update -notmatch 'InsertObserverSP\(AICF_VS_OBSERVER_KEY, 0, 0, member\)' -or $stop -notmatch 'RemoveObserverSP\(AICF_VS_OBSERVER_KEY\)'){throw 'Dedicated exact native observer ownership required'}
$root=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Forces/AICF_ManagedObservers.c'))
if($root.Contains('-1513170001') -or -1513170001 -ge -1513160000){throw 'Fixture observer collides with root nonnegative slot range'}
foreach($method in @('AICF_VSFinish','Stop','OnGameEnd')){
 if((Body $method) -notmatch 'AICF_VSStopObserver\('){throw "Missing explicit lifecycle cleanup $method"}
}
if((Body 'Stop').IndexOf('AICF_VSStopObserver(') -gt (Body 'Stop').IndexOf('super.Stop(') -or
 (Body 'OnGameEnd').IndexOf('AICF_VSStopObserver(') -gt (Body 'OnGameEnd').IndexOf('super.OnGameEnd(') -or
 (Body 'AICF_VSFinish').IndexOf('AICF_VSStopObserver(') -gt (Body 'AICF_VSFinish').IndexOf('m_VSGroup.DespawnMembers(')){throw 'Remove own observer before native shutdown/entity disposal'}
if((Body 'Update') -notmatch '(?s)super.Update\(\);\s*AICF_VSUpdateObserver\(\);'){throw 'Refresh observer on every existing Update before scenario early returns'}
if($update -notmatch 'registered_request=1 key_readback_available=0 setup_parity_only=1 causal_foot_fix_proven=0' -or $update -match 'registered_proven=1|native_readback=1'){throw 'Native void Insert has no per-key registration readback'}
if(($guard+$update+$stop) -match 'SetOrigin|Teleport|AddAction|CallLater|LoadTileIn|SetLOD|SetHealth|GetTickCount'){throw 'Observer parity must not change physical/action/timer state'}
function Adapt([string]$body){
 foreach($name in @('campaign','state','slot','group','faction','member','m_VSState','m_VSObserverPolicy','m_VSObservers')){
  $body=[regex]::Replace($body,'!'+$name+'(?=\s*[|&)])',($name+' == null'))
  $body=[regex]::Replace($body,'(?<![\w!])'+$name+'(?=\s*&&)',($name+' != null'))
 }
 $body=$body.Replace('m_bRosterReady && m_VSObserverPolicy)', 'm_bRosterReady && m_VSObserverPolicy != null)')
 $body=$body.Replace('AICF_GroupSlot slot;', 'AICF_GroupSlot slot = null;').Replace('IEntity member;', 'IEntity member = null;').Replace('if (m_VSState)', 'if (m_VSState != null)')
 $body=$body.Replace('ObserversSystem.Cast(GetGame().GetWorld().FindSystem(ObserversSystem))','NativeSystem')
 return $body
}
$adapter=@'
using System;
public static class Replication{public static bool Server=true;public static bool IsServer(){return Server;}}
public class IEntity{public int Id;public int GetID(){return Id;}}
public class Faction{public string Key="USSR";public string GetFactionKey(){return Key;}}
public class SCR_GameModeCampaign{public bool Master=true,Running=true;public bool IsMaster(){return Master;}public bool IsRunning(){return Running;}}
public class SCR_AIGroup:IEntity{public Faction Faction;public bool Authority=true;public Faction GetFaction(){return Faction;}}
public class AICF_GroupSlot{public bool Ready=true;public SCR_AIGroup Group;public bool IsCombatReady(){return Ready;}public SCR_AIGroup GetGroup(){return Group;}}
public class AICF_FactionState{public string Key="USSR";public AICF_GroupSlot Slot;public string GetFactionKey(){return Key;}public AICF_GroupSlot GetSlot(int i){return Slot;}}
public class AICF_ContentProfile{public static AICF_ContentProfile GetActive(){return new AICF_ContentProfile();}public string GetStableFactionKey(string key){return key;}}
public static class AICF_VehicleBoardingMutationFence{public static bool IsAuthoritativeReplicatedEntity(SCR_AIGroup group){return group.Authority;}}
public class ObserversSystem{public int Inserts,Removes,Key;public IEntity Member;public void InsertObserverSP(int key,int x,int z,IEntity member){if(x!=0||z!=0)throw new Exception("Offsets");Inserts++;Key=key;Member=member;}public void RemoveObserverSP(int key){Removes++;Key=key;Member=null;}}
public static class AICF_GroupTrace{public static string Entity(IEntity member){return member.Id.ToString();}}
public class AICF_VSObserverPolicy{public IEntity Leader=new IEntity(){Id=100};public int Queries;public bool ChangeSecond;public IEntity ResolveMember(SCR_AIGroup group,string faction){Queries++;if(ChangeSecond&&Queries==2)return new IEntity();return Leader;}public IEntity ResolveCurrent(SCR_GameModeCampaign campaign,AICF_FactionState state,AICF_GroupSlot slot,SCR_AIGroup group,int groupId,Faction faction,string expected) GUARD}
public class AICF_ObserverOwner{
 public const int AICF_VS_OBSERVER_KEY=-1513170001;public bool m_bStopped,m_bVSDone,m_bRosterReady=true,m_bVSObserverRequested;public string m_sVSCase="repeat5",m_sVSFaction="USSR";public int m_VSGroupId=55;public Faction m_VSFaction=new Faction();public SCR_GameModeCampaign m_Campaign=new SCR_GameModeCampaign();public AICF_FactionState m_VSState=new AICF_FactionState();public SCR_AIGroup m_VSGroup;public AICF_VSObserverPolicy m_VSObserverPolicy=new AICF_VSObserverPolicy();public ObserversSystem NativeSystem=new ObserversSystem(),m_VSObservers;public IEntity m_VSObservedMember;
 public AICF_ObserverOwner(){m_VSGroup=new SCR_AIGroup(){Id=55,Faction=m_VSFaction};m_VSState.Slot=new AICF_GroupSlot(){Group=m_VSGroup};}
 public void Print(string value){}
 public void AICF_VSUpdateObserver() UPDATE
 public void AICF_VSStopObserver(string reason) STOP_BODY
 public static int[] Evaluate(int scenario){Replication.Server=true;var owner=new AICF_ObserverOwner();var native=owner.NativeSystem;if(scenario==23){owner.m_sVSFaction="US";owner.m_VSFaction.Key="US";owner.m_VSState.Key="US";}if(scenario==24){owner.m_sVSFaction="CIV";owner.m_VSFaction.Key="CIV";owner.m_VSState.Key="CIV";}
 switch(scenario){case 1:Replication.Server=false;break;case 2:owner.m_Campaign=null;break;case 3:owner.m_Campaign.Master=false;break;case 4:owner.m_Campaign.Running=false;break;case 5:owner.m_VSState=null;break;case 6:owner.m_VSState.Slot=null;break;case 7:owner.m_VSState.Slot.Ready=false;break;case 8:owner.m_VSGroup.Id=56;break;case 9:owner.m_VSState.Slot.Group=new SCR_AIGroup();break;case 10:owner.m_VSFaction=null;break;case 11:owner.m_VSGroup.Faction=new Faction();break;case 12:owner.m_VSGroup.Authority=false;break;case 13:owner.m_VSState.Key="US";break;case 14:owner.m_VSFaction.Key="US";owner.m_VSState.Key="US";break;case 15:owner.m_VSObserverPolicy.Leader=null;break;case 16:owner.m_bStopped=true;break;case 17:owner.m_bVSDone=true;break;case 18:owner.m_bRosterReady=false;break;case 19:owner.m_VSObserverPolicy=null;break;case 20:owner.NativeSystem=null;break;case 21:owner.m_VSObserverPolicy.ChangeSecond=true;break;case 22:owner.m_VSGroup=null;break;}
 owner.AICF_VSUpdateObserver();owner.AICF_VSUpdateObserver();int requested=owner.m_bVSObserverRequested?1:0;owner.AICF_VSStopObserver("FINISH");owner.AICF_VSStopObserver("STOP");return new int[]{native.Inserts,native.Removes,requested,native.Key};}
 public static int[] Stale(){Replication.Server=true;var owner=new AICF_ObserverOwner();owner.AICF_VSUpdateObserver();owner.m_VSState.Slot.Ready=false;owner.AICF_VSUpdateObserver();return new int[]{owner.NativeSystem.Inserts,owner.NativeSystem.Removes,owner.m_bVSObserverRequested?1:0};}
}
'@
$adapter=$adapter.Replace('GUARD',(Adapt $guard)).Replace('UPDATE',(Adapt $update)).Replace('STOP_BODY',(Adapt $stop))
if(!('AICF_ObserverOwner' -as [type])){Add-Type -TypeDefinition $adapter}
for($case=0;$case -le 24;$case++){
 $actual=[AICF_ObserverOwner]::Evaluate($case);$expected=@(0,0,0,0)
 if($case -in 0,23){$expected=@(2,1,1,-1513170001)}
 if($case -eq 21){$expected=@(1,1,1,-1513170001)}
 if(($actual -join ',') -ne ($expected -join ',')){throw "Actual observer guard case=$case actual=$actual expected=$expected"}
}
if(([AICF_ObserverOwner]::Stale() -join ',') -ne '1,1,0'){throw 'Stale owner must remove only its own outstanding observer request'}
$selectionBody=Body 'AICF_VSSelectFaction';$modelBody=Body 'AICF_VSUSModelCandidate'
if(!$selectionBody -or !$modelBody){throw 'Missing actual selected-faction/catalog predicate'}
$selectionBody=$selectionBody.Replace('m_VSFaction || m_VSState','m_VSFaction != null || m_VSState != null').Replace('SCR_CampaignFaction','Faction').Replace('!selected ||','selected == null ||').Replace('!selected.GetMainBase()','selected.GetMainBase() == null').Replace('System.GetCLIParam("aicfVehicleScenarioFaction", requested)','CLI.GetCLIParam("aicfVehicleScenarioFaction", ref requested)')
$factionAdapter=@'
public class AICF_VSFactionSelection{
 public class Faction{public string Key;public object Base=new object();public string GetFactionKey(){return Key;}public object GetMainBase(){return Base;}}
 public static class CLI{public static string Requested;public static bool GetCLIParam(string name,ref string target){if(Requested==null){target="";return false;}target=Requested;return true;}}
 public static class AICF_ContentProfile{public static AICF_Profile GetActive(){return new AICF_Profile();}}
 public class AICF_Profile{public string GetStableFactionKey(string key){return key;}}
 public static class AICF_EVehicleCatalogSource{public const int FACTION=1;}
 public Faction m_USFaction=new Faction(){Key="US"},m_USSRFaction=new Faction(){Key="USSR"},m_VSFaction;public object m_VSState;public string m_sVSCase="repeat5",m_sVSFaction="USSR";
 public bool Select() SELECT_BODY
 public bool Model(string prefab,int source,int seats,int cost) MODEL_BODY
 public static string Evaluate(int scenario){CLI.Requested=null;var owner=new AICF_VSFactionSelection();Faction prior=null;switch(scenario){case 1:CLI.Requested="USSR";break;case 2:CLI.Requested="US";break;case 3:CLI.Requested="US";owner.m_sVSCase="uaz5";break;case 4:CLI.Requested="CIV";break;case 5:owner.m_USSRFaction=null;break;case 6:owner.m_USSRFaction.Base=null;break;case 7:owner.m_USSRFaction.Key="US";break;case 8:prior=new Faction(){Key="PINNED"};owner.m_VSFaction=prior;break;case 9:owner.m_VSState=new object();break;case 10:CLI.Requested="";break;case 11:CLI.Requested="US";owner.m_USFaction=null;break;case 12:CLI.Requested="US";owner.m_USFaction.Base=null;break;}bool accepted=owner.Select();if(scenario==8&&owner.m_VSFaction!=prior)throw new System.Exception("Reassignment");bool same=owner.m_VSFaction!=null&&(owner.m_VSFaction==owner.m_USFaction||owner.m_VSFaction==owner.m_USSRFaction);return accepted+","+owner.m_sVSFaction+","+same;}
}
'@
if(!('AICF_VSFactionSelection' -as [type])){Add-Type -TypeDefinition ($factionAdapter.Replace('SELECT_BODY',$selectionBody).Replace('MODEL_BODY',$modelBody))}
for($selectionCase=0;$selectionCase -le 12;$selectionCase++){
 $expected='False,USSR,False';if($selectionCase -in 0,1){$expected='True,USSR,True'};if($selectionCase -eq 2){$expected='True,US,True'}
 if([AICF_VSFactionSelection]::Evaluate($selectionCase) -ne $expected){throw "Actual faction selector case=$selectionCase"}
}
$nativeCandidate=New-Object AICF_VSFactionSelection
foreach($candidate in @(@('Prefabs/M1025_armed_M2HB.et',1,5,475,$true),@('Prefabs/UAZ469.et',1,5,475,$false),@('Prefabs/M1025_armed_M2HB.et',2,5,475,$false),@('Prefabs/M1025_armed_M2HB.et',1,4,475,$false),@('Prefabs/M1025_armed_M2HB.et',1,5,0,$false),@('Prefabs/M1025_unarmed.et',1,5,475,$false))){if($nativeCandidate.Model($candidate[0],$candidate[1],$candidate[2],$candidate[3]) -ne $candidate[4]){throw 'Actual own catalog exact5 HMMWV predicate'}}
$catalogBody=Body 'AICF_VSSelectUSModel'
foreach($needle in @('catalog.List()','catalog.Inspect(prefab, reason)','AICF_VSUSModelCandidate(prefab, catalog.Source(prefab), catalog.Seats().Count(), catalog.Cost(prefab))')){if(!$catalogBody.Contains($needle)){throw "Native catalog source is bypassed: $needle"}}
$runner=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'tools/Invoke-AICFVehicleScenarios.ps1'))
foreach($needle in @('[ValidateSet(''US'',''USSR'')][string]$Faction=''USSR''','''-aicfVehicleScenarioFaction'',$Faction','''-aicfGroupTraceFaction'',$Faction','-Faction $Faction')){if(!$runner.Contains($needle)){throw "Canonical selected-faction launch missing: $needle"}}
if((Body 'ResolveVehicleSpawnApproachOwner') -notmatch 'faction = m_VSFaction;' -or (Body 'AICF_VSUpdateVehicles') -notmatch 'Update\(m_VSState, m_VSFaction, null, null' -or (Body 'AICF_VSUpdateVehicles') -notmatch 'Update\(null, null, m_VSState, m_VSFaction'){throw 'Selected faction must own failure/recovery/coordinator contexts'}
'PASS Vehicle scenario observer: owner controls=26, actual faction selector controls=13, actual own-catalog HMMWV controls=6; both factions/exact lifecycle/request-only marker, native runtime required'

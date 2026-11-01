param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$source=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Vehicles/AICF_VehicleNativeSubgroup.c'))
function Body($name){Get-AICFMethodBody ([pscustomobject]@{Source=$source;Code=(ConvertTo-AICFCodeText $source)}) $name}
function Adapt($body){
 $body=$body.Replace('array<AIAgent>','List<AIAgent>').Replace('array<ref SCR_AIGroupVehicle>','List<SCR_AIGroupVehicle>')
 # Native out-array shims preserve the sampled contents/counts.
 $body=[regex]::Replace($body,'List<AIAgent> (\w+) = \{\};','List<AIAgent> $1 = new List<AIAgent>();')
 $body=[regex]::Replace($body,'List<SCR_AIGroupVehicle> (\w+) = \{\};','List<SCR_AIGroupVehicle> $1 = new List<SCR_AIGroupVehicle>();')
 $body=$body.Replace('int position;','int position = 0;').Replace('int ownRecords;','int ownRecords = 0;')
 $body=$body.Replace('.Count()','.Count').Replace('seen.Insert(agent)','seen.Add(agent)').Replace('ChimeraCharacter.Cast(actor)','actor as ChimeraCharacter').Replace('CompartmentAccessComponent access','Access access')
 $body=[regex]::Replace($body,'foreach \((\w+) (\w+) : (\w+)\)','foreach ($1 $2 in $3)')
 return $body
}
$code=@'
using System;using System.Collections.Generic;
public class N{public static implicit operator bool(N o){return !Object.ReferenceEquals(o,null);}public static bool operator !(N o){return Object.ReferenceEquals(o,null);}}
public class IEntity:N{public bool Current=true,Conscious=true;}
public class Vehicle:IEntity{}
public class Compartment:N{public Vehicle Vehicle;public IEntity Occupant;public Vehicle GetVehicle(){return Vehicle;}public IEntity GetOccupant(){return Occupant;}}
public class Access:N{public bool In=true,Entering,Exiting,Switching;public Vehicle Linked;public Compartment Seat;public bool IsInCompartment(){return In;}public bool IsGettingIn(){return Entering;}public bool IsGettingOut(){return Exiting;}public bool IsSwitchingSeatsAnim(){return Switching;}public Compartment GetCompartment(){return Seat;}}
public class ChimeraCharacter:IEntity{public bool In=true;public Access Access=new Access();public bool IsInVehicle(){return In;}public Access GetCompartmentAccessComponent(){return Access;}}
public static class CompartmentAccessComponent{public static Vehicle GetVehicleIn(ChimeraCharacter c){return c.Access.Linked;}}
public class AIAgent:N{public SCR_AIGroup Parent;public IEntity Actor;public int Handler;public SCR_AIGroup GetParentGroup(){return Parent;}public IEntity GetControlledEntity(){return Actor;}}
public class SCR_AIGroup:N{public List<AIAgent> Agents=new List<AIAgent>();public void GetAgents(List<AIAgent> dest){dest.AddRange(Agents);}public int GetAgentsCount(){return Agents.Count;}}
public class AICF_GroupSlot:N{public int Desired=3;public int GetDesiredSize(){return Desired;}}
public class AICF_TransportTrip:N{public AICF_GroupSlot Slot=new AICF_GroupSlot();public List<IEntity> Actors=new List<IEntity>();public IEntity VehiclePresetMember(int n){return n<Actors.Count?Actors[n]:null;}public AICF_GroupSlot GetVehiclePresetSlot(){return Slot;}}
public static class AICF_Stage1Config{public const int MAX_GROUP_SIZE=10;}
public static class AICF_VehicleShortRecovery{public static bool IsCurrentMember(AICF_TransportTrip t,IEntity a){return a.Current;}}
public static class SCR_AIDamageHandling{public static bool IsConscious(IEntity a){return a.Conscious;}}
public class SCR_AIGroupVehicle:N{public Vehicle Vehicle;public int Handler=2;public Vehicle GetEntity(){return Vehicle;}public int GetSubgroupHandleId(){return Handler;}}
public class VehicleManager:N{public List<SCR_AIGroupVehicle> Records=new List<SCR_AIGroupVehicle>();public void GetAllVehicles(List<SCR_AIGroupVehicle> dest){dest.AddRange(Records);}public SCR_AIGroupVehicle FindVehicleBySubgroupId(int h){return Records.Find(r=>r.Handler==h);}}
public class SCR_AIGroupUtilityComponent:N{public VehicleManager m_VehicleMgr=new VehicleManager();}
public class AIGroupMovementComponent:N{
 public const int DEFAULT_HANDLER_ID=0;public Dictionary<int,List<AIAgent>> Members=new Dictionary<int,List<AIAgent>>();public Dictionary<int,int> ReportedCount=new Dictionary<int,int>();public Dictionary<int,AIAgent> Leaders=new Dictionary<int,AIAgent>();public HashSet<int> Formations=new HashSet<int>{1,2};
 public N GetFormationDefinition(int h){return Formations.Contains(h)?new N():null;}public int GetAgentMoveHandlerId(AIAgent a){return a.Handler;}
 public int GetAgentsInHandler(List<AIAgent> dest,int h){dest.AddRange(Members[h]);return ReportedCount.ContainsKey(h)?ReportedCount[h]:Members[h].Count;}
 public int GetMoveHandlerAgentCount(int h){return Members[h].Count;}public AIAgent GetHandlerLeaderAgent(int h){return Leaders.ContainsKey(h)?Leaders[h]:null;}
}
public static class AICF_VehicleNativeSubgroup{
 public static bool CapturedMember(AICF_TransportTrip trip,IEntity actor) CAPTURED
 public static bool Settled(IEntity actor,Vehicle vehicle) SETTLED
 public static bool ExactSettledHandler(AICF_TransportTrip trip,SCR_AIGroup group,Vehicle vehicle,AIGroupMovementComponent movement,int handler,List<AIAgent> groupAgents) HANDLERBODY
 public static bool CanReconcilePositiveHandles(AICF_TransportTrip trip,SCR_AIGroupUtilityComponent utility,SCR_AIGroup group,Vehicle vehicle,SCR_AIGroupVehicle registered,AIAgent pilot,AIGroupMovementComponent movement,out string reason) POSITIVE
}
public class Scene{
 public AICF_TransportTrip Trip=new AICF_TransportTrip();public SCR_AIGroupUtilityComponent Utility=new SCR_AIGroupUtilityComponent();public SCR_AIGroup Group=new SCR_AIGroup();public Vehicle Vehicle=new Vehicle();public SCR_AIGroupVehicle Record;public AIAgent Pilot;public AIGroupMovementComponent Movement=new AIGroupMovementComponent();
 public static Scene Fresh(){var s=new Scene();for(int i=0;i<3;i++){var actor=new ChimeraCharacter();actor.Access.Linked=s.Vehicle;actor.Access.Seat=new Compartment{Vehicle=s.Vehicle,Occupant=actor};var agent=new AIAgent{Actor=actor,Parent=s.Group,Handler=i==0?1:2};s.Group.Agents.Add(agent);s.Trip.Actors.Add(actor);if(i==0)s.Pilot=agent;}s.Record=new SCR_AIGroupVehicle{Vehicle=s.Vehicle};s.Utility.m_VehicleMgr.Records.Add(s.Record);s.Movement.Members[1]=new List<AIAgent>{s.Pilot};s.Movement.Members[2]=new List<AIAgent>{s.Group.Agents[1],s.Group.Agents[2]};s.Movement.Leaders[1]=s.Pilot;s.Movement.Leaders[2]=s.Group.Agents[1];return s;}
 public bool Actual(){string reason;return AICF_VehicleNativeSubgroup.CanReconcilePositiveHandles(Trip,Utility,Group,Vehicle,Record,Pilot,Movement,out reason);}
 public static int Run(){int n=0;Action<bool,string> check=(ok,label)=>{n++;if(!ok)throw new Exception(label);};check(Fresh().Actual(),"actual own split positive handlers");
 var negatives=new Action<Scene>[] {
  s=>s.Record.Handler=0,s=>s.Record.Handler=-1,s=>s.Pilot.Handler=0,s=>s.Pilot.Handler=-1,s=>s.Pilot.Handler=2,
  s=>s.Trip.Slot=null,s=>s.Trip.Slot.Desired=4,s=>s.Trip.Actors.RemoveAt(2),s=>s.Group.Agents[2].Actor.Current=false,s=>s.Group.Agents[2].Actor.Conscious=false,
  s=>s.Movement.Formations.Remove(1),s=>s.Movement.Formations.Remove(2),s=>s.Movement.ReportedCount[1]=2,s=>s.Movement.ReportedCount[2]=3,
  s=>s.Movement.Members[2].Add(s.Movement.Members[2][0]),s=>s.Movement.Members[2].Clear(),s=>s.Movement.Members[1].Clear(),
  s=>s.Movement.Members[2].Add(new AIAgent{Actor=new ChimeraCharacter(),Parent=s.Group,Handler=2}),
  s=>s.Group.Agents[1].Parent=new SCR_AIGroup(),s=>s.Group.Agents[1].Handler=3,s=>((ChimeraCharacter)s.Pilot.Actor).In=false,
  s=>((ChimeraCharacter)s.Pilot.Actor).Access.In=false,s=>((ChimeraCharacter)s.Pilot.Actor).Access.Entering=true,s=>((ChimeraCharacter)s.Pilot.Actor).Access.Exiting=true,s=>((ChimeraCharacter)s.Pilot.Actor).Access.Switching=true,
  s=>((ChimeraCharacter)s.Pilot.Actor).Access.Linked=new Vehicle(),s=>((ChimeraCharacter)s.Pilot.Actor).Access.Seat.Vehicle=new Vehicle(),s=>((ChimeraCharacter)s.Pilot.Actor).Access.Seat.Occupant=new IEntity(),
  s=>s.Movement.Leaders[2]=new AIAgent(),s=>s.Utility.m_VehicleMgr.Records.Clear(),s=>s.Utility.m_VehicleMgr.Records.Add(s.Record),
  s=>s.Utility.m_VehicleMgr.Records.Add(new SCR_AIGroupVehicle{Vehicle=s.Vehicle,Handler=3}),
  s=>s.Utility.m_VehicleMgr.Records.Add(new SCR_AIGroupVehicle{Vehicle=new Vehicle(),Handler=1}),
  s=>s.Utility.m_VehicleMgr.Records.Add(new SCR_AIGroupVehicle{Vehicle=new Vehicle(),Handler=2})
 };
 int index=0;foreach(var mutate in negatives){var s=Fresh();mutate(s);check(!s.Actual(),"actual scope rejects unsafe case="+index++);}
 var noLeader=Fresh();noLeader.Movement.Leaders.Clear();check(noLeader.Actual(),"native passenger handler may have no leader before pilot join");
 var foreignOther=Fresh();foreignOther.Utility.m_VehicleMgr.Records.Add(new SCR_AIGroupVehicle{Vehicle=new Vehicle(),Handler=3});check(foreignOther.Actual(),"unrelated vehicle with other handler remains untouched");
 var foot=Fresh();var actor=new ChimeraCharacter{In=false};var agent=new AIAgent{Actor=actor,Parent=foot.Group,Handler=0};foot.Group.Agents.Add(agent);foot.Trip.Actors.Add(actor);foot.Trip.Slot.Desired=4;check(!foot.Actual(),"third foot handler cannot enter full settled repair scope");
 actor.In=true;actor.Access.Linked=new Vehicle();actor.Access.Seat=new Compartment{Vehicle=actor.Access.Linked,Occupant=actor};check(!foot.Actual(),"third actor in other vehicle cannot enter repair scope");
 actor.Access.Linked=foot.Vehicle;actor.Access.Seat.Vehicle=foot.Vehicle;check(foot.Actual(),"third original settled actor may be joined by the existing exact passenger pass");
 return n;
 }
}
'@
$template=$code.Replace('CAPTURED',(Adapt (Body 'CapturedMember'))).Replace('SETTLED',(Adapt (Body 'Settled'))).Replace('HANDLERBODY',(Adapt (Body 'ExactSettledHandler'))).Replace('POSITIVE',(Adapt (Body 'CanReconcilePositiveHandles')))
Add-Type -TypeDefinition $template
$actual=[Scene]::Run()
$mutations=@(
 @('CAPTURED_ALWAYS_TRUE',''),
 @('CURRENT_IDENTITY_BYPASS',''),
 @('SETTLED_ALWAYS_TRUE',''),
 @('seen.Contains(agent)','false'),
 @('agent.GetParentGroup() != group','false'),
 @('!movement.GetFormationDefinition(handler)','false'),
 @('ownRecords != 1','false'),
 @('agents.Count != slot.GetDesiredSize()','false')
)
$caught=0
foreach($change in $mutations){
 $variant='AICF_PositiveSubgroupUnsafe'+$caught
 $mutated=$template.Replace($change[0],$change[1])
 if($change[0] -eq 'CAPTURED_ALWAYS_TRUE'){$mutated=$template.Replace((Adapt (Body 'CapturedMember')),'{ return true; }')}
 if($change[0] -eq 'SETTLED_ALWAYS_TRUE'){$mutated=$template.Replace((Adapt (Body 'Settled')),'{ return true; }')}
 if($change[0] -eq 'CURRENT_IDENTITY_BYPASS'){$mutated=$template.Replace('!AICF_VehicleShortRecovery.IsCurrentMember(trip, actor)','false').Replace('!AICF_VehicleShortRecovery.IsCurrentMember(trip, agent.GetControlledEntity())','false')}
 Add-Type -TypeDefinition ('namespace '+$variant+' {'+$mutated+'}')
 $failed=$false;try{([type]($variant+'.Scene')).GetMethod('Run').Invoke($null,@())|Out-Null}catch{$failed=$true}
 if(!$failed){throw "Actual unsafe positive-handler mutant escaped: $($change[0])"};$caught++
}
"PASS positive native subgroup: actual_scope_cases=$actual executable_unsafe_mutants=$caught native_join=NOT_RUN; no foreign/default/dead/foot handler adoption"

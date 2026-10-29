param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$s=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Vehicles/AICF_VehicleParking.c'))
$body=Get-AICFMethodBody ([pscustomobject]@{Source=$s;Code=(ConvertTo-AICFCodeText $s)}) HasNoForeignOrHumanUse
$body=$body.Replace('!GetGame().GetPlayerManager()','GetGame().GetPlayerManager() == null').Replace('!GetGame()','GetGame() == null')
$body=$body.Replace('array<int> playerIds = {};','List<int> playerIds = new List<int>();').Replace('array<IEntity> observed = {controlled, main};','List<IEntity> observed = new List<IEntity>{controlled,main};').Replace('array<BaseCompartmentSlot> seats = {};','List<BaseCompartmentSlot> seats = new List<BaseCompartmentSlot>();')
$body=$body.Replace('!controlled && !main','controlled == null && main == null').Replace('if (!entity)','if (entity == null)').Replace('if (!character)','if (character == null)').Replace('if (access &&','if (access != null &&')
$body=$body.Replace('BaseCompartmentManagerComponent.Cast(m_Vehicle.FindComponent(BaseCompartmentManagerComponent))','m_Vehicle.Manager').Replace('if (!manager)','if (manager == null)').Replace('if (!seat)','if (seat == null)').Replace('if (!occupant)','if (occupant == null)')
$body=$body.Replace('!original.GetAIControlComponent()','original.GetAIControlComponent() == null').Replace('!original ||','original == null ||').Replace('!agent ||','agent == null ||').Replace('!m_Trip.GetAssignment()','m_Trip.GetAssignment() == null')
$body=[regex]::Replace($body,'foreach \(([^:()]+) : ([^)]+)\)','foreach ($1 in $2)')
$code=@'
using System;using System.Collections.Generic;
public class IEntity{public bool Captured=true,Authority=true,Alive=true;public int PlayerId,MainId;public IEntity Linked;}
public class Group{public static Group Current=new Group();}
public class Assignment{public Group Group=Group.Current;public Group GetGroup(){return Group;}}
public class Trip{public Assignment Assignment=new Assignment();public Assignment GetAssignment(){return Assignment;}}
public class AIAgent{public IEntity Entity;public Group Group=Group.Current;public IEntity GetControlledEntity(){return Entity;}public Group GetParentGroup(){return Group;}}
public class AIControl{public AIAgent Agent;public AIAgent GetControlAIAgent(){return Agent;}}
public class CompartmentAccessComponent{public IEntity Target;public IEntity GetVehicleCompartmentManagerOwner(){return Target;}public static IEntity GetVehicleIn(IEntity entity){return entity.Linked;}}
public class ChimeraCharacter:IEntity{public CompartmentAccessComponent Access=new CompartmentAccessComponent();public AIControl Control=new AIControl();public ChimeraCharacter(){Control.Agent=new AIAgent{Entity=this};}public static ChimeraCharacter Cast(IEntity e){return e as ChimeraCharacter;}public CompartmentAccessComponent GetCompartmentAccessComponent(){return Access;}public AIControl GetAIControlComponent(){return Control;}}
public class BaseCompartmentSlot{public IEntity Occupant;public IEntity GetOccupant(){return Occupant;}}
public class BaseCompartmentManagerComponent{public List<BaseCompartmentSlot> Seats=new List<BaseCompartmentSlot>();public void GetCompartments(List<BaseCompartmentSlot> seats){seats.AddRange(Seats);}}
public class Vehicle:IEntity{public BaseCompartmentManagerComponent Manager=new BaseCompartmentManagerComponent();}
public class PlayerManager{public List<int> Ids=new List<int>();public Dictionary<int,IEntity> Controlled=new Dictionary<int,IEntity>();public Dictionary<int,IEntity> Main=new Dictionary<int,IEntity>();public void GetAllPlayers(List<int> ids){ids.AddRange(Ids);}public IEntity GetPlayerControlledEntity(int id){return Controlled.ContainsKey(id)?Controlled[id]:null;}public int GetPlayerIdFromControlledEntity(IEntity e){return e.PlayerId;}}
public class World{public PlayerManager Players=new PlayerManager();public PlayerManager GetPlayerManager(){return Players;}}
public static class SCR_PossessingManagerComponent{public static PlayerManager Players;public static IEntity GetPlayerMainEntity(int id){return Players.Main.ContainsKey(id)?Players.Main[id]:null;}public static int GetPlayerIdFromMainEntity(IEntity e){return e.MainId;}}
public static class AICF_GroupRuntime{public static bool IsAliveCharacter(IEntity e){return e!=null && e.Alive;}}
public static class AICF_VehicleBoardingMutationFence{public static bool IsAuthoritativeReplicatedEntity(IEntity e){return e.Authority;}public static bool IsAuthoritativeAIEntity(IEntity e){return e.Authority && e.PlayerId==0 && e.MainId==0;}}
public class ParkingFence{public World World=new World();public Vehicle m_Vehicle=new Vehicle();public Trip m_Trip=new Trip();public ParkingFence(){SCR_PossessingManagerComponent.Players=World.Players;}public World GetGame(){return World;}public bool HasCapturedMember(IEntity entity){return entity.Captured;}public bool Read() BODY}
'@
Add-Type -TypeDefinition ($code.Replace(' BODY',' '+$body))
$checks=0
function Good{New-Object ParkingFence}
function Refuse([ParkingFence]$f,[string]$name){if($f.Read()){throw "Actual parking human fence accepted $name"};$script:checks++}
$f=Good;if(!$f.Read()){throw 'Empty current AI car rejected'};$checks++
$f=Good;$f.World.Players.Ids.Add(1);$f.World.Players.Controlled[1]=$f.m_Vehicle;Refuse $f 'direct vehicle control'
$f=Good;$human=New-Object ChimeraCharacter;$human.Linked=$f.m_Vehicle;$f.World.Players.Ids.Add(1);$f.World.Players.Controlled[1]=$human;Refuse $f 'human linked vehicle'
$f=Good;$human=New-Object ChimeraCharacter;$human.Access.Target=$f.m_Vehicle;$f.World.Players.Ids.Add(1);$f.World.Players.Controlled[1]=$human;Refuse $f 'human target transition'
$f=Good;$human=New-Object ChimeraCharacter;$human.Linked=$f.m_Vehicle;$f.World.Players.Ids.Add(1);$f.World.Players.Main[1]=$human;Refuse $f 'player-main possession'
$f=Good;$f.World.Players.Ids.Add(1);Refuse $f 'unknown player identity'
$f=Good;$foreign=New-Object ChimeraCharacter;$foreign.Captured=$false;$seat=New-Object BaseCompartmentSlot;$seat.Occupant=$foreign;$f.m_Vehicle.Manager.Seats.Add($seat);Refuse $f 'foreign AI occupant'
$f=Good;$human=New-Object ChimeraCharacter;$human.PlayerId=1;$seat=New-Object BaseCompartmentSlot;$seat.Occupant=$human;$f.m_Vehicle.Manager.Seats.Add($seat);Refuse $f 'captured actor possessed since capture'
$f=Good;$original=New-Object ChimeraCharacter;$original.Control.Agent.Group=New-Object Group;$seat=New-Object BaseCompartmentSlot;$seat.Occupant=$original;$f.m_Vehicle.Manager.Seats.Add($seat);Refuse $f 'original moved to foreign group'
$f=Good;$human=New-Object ChimeraCharacter;$f.World.Players.Ids.Add(1);$f.World.Players.Controlled[1]=$human;if(!$f.Read()){throw 'Visible unlinked bystander incorrectly blocks parking'};$checks++
$f=Good;$original=New-Object ChimeraCharacter;$seat=New-Object BaseCompartmentSlot;$seat.Occupant=$original;$f.m_Vehicle.Manager.Seats.Add($seat);if(!$f.Read()){throw 'Exact original AI occupant rejected'};$checks++
"PASS Vehicle parking actual human/foreign fence controls=$checks extracted_player_and_occupant_scan=EXECUTED"

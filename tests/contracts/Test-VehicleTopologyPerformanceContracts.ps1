param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$source=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Vehicles/AICF_VehiclePreset.c'))
function Body($text,$method){Get-AICFMethodBody ([pscustomobject]@{Source=$text;Code=(ConvertTo-AICFCodeText $text)}) $method}
function Adapt($body){
 $body=$body.Replace('array<ref array<IEntity>> ownerTables = {};','AICFArray<AICFArray<IEntity>> ownerTables = new AICFArray<AICFArray<IEntity>>();')
 $body=[regex]::Replace($body,'array<([^>]+)> (\w+) = \{\};','AICFArray<$1> $2 = new AICFArray<$1>();')
 $body=$body.Replace('FindComponent(BaseCompartmentManagerComponent)','FindComponent(typeof(BaseCompartmentManagerComponent))').Replace('if (entity.FindComponent(typeof(BaseCompartmentManagerComponent)))','if (entity.FindComponent(typeof(BaseCompartmentManagerComponent)) != null)').Replace('string.Format','SF.Format')
 $body=[regex]::Replace($body,'foreach \((\w+) (\w+) : (\w+(?:\.\w+)*)\)','foreach ($1 $2 in $3)')
 $body=[regex]::Replace($body,'for \(int (\w+);','for (int $1 = 0;')
 return $body.Replace('array<','AICFArray<')
}
# Frozen pre-change bodies are an independent compatibility and traversal oracle.
$legacyMethods=(Adapt @'
static void CollectHierarchy(IEntity entity,array<IEntity> owners){
		if (!entity) return;
		if (entity.FindComponent(BaseCompartmentManagerComponent)) owners.Insert(entity);
		IEntity child = entity.GetChildren();
		while (child)
		{
			CollectHierarchy(child, owners);
			child = child.GetSibling();
		}
	}
public static int RelativeOwnerIndex(BaseCompartmentSlot seat){
		array<IEntity> owners = {};
		CollectHierarchy(seat.GetVehicle(), owners);
		return owners.Find(seat.GetOwner());
	}
public static string SeatKey(BaseCompartmentSlot seat){
		return string.Format("%1:%2:%3", RelativeOwnerIndex(seat), seat.GetCompartmentSlotID(), seat.GetType());
	}
public static void Collect(IEntity vehicle,array<BaseCompartmentSlot> seats){
		seats.Clear();
		if (!vehicle) return;
		BaseCompartmentManagerComponent manager = BaseCompartmentManagerComponent.Cast(vehicle.FindComponent(BaseCompartmentManagerComponent));
		if (!manager) return;
		array<BaseCompartmentSlot> found = {};
		manager.GetCompartments(found);
		foreach (BaseCompartmentSlot seat : found)
		{
			if (!seat || !seat.IsCompartmentAccessible() || seat.GetVehicle() != vehicle) continue;
			int index = seats.Count();
			for (int i; i < seats.Count(); i++)
				if (RelativeOwnerIndex(seat) < RelativeOwnerIndex(seats[i]) ||
					(RelativeOwnerIndex(seat) == RelativeOwnerIndex(seats[i]) && seat.GetCompartmentSlotID() < seats[i].GetCompartmentSlotID()))
				{ index = i; break; }
			seats.InsertAt(seat, index);
		}
	}
public bool MatchesSeats(array<BaseCompartmentSlot> seats){
		if (seats.Count() != m_aSeats.Count()) return false;
		for (int i; i < seats.Count(); i++)
			if (SeatKey(seats[i]) != m_aSeats[i]) return false;
		return true;
	}
public static bool CollectCaptured(IEntity vehicle,AICF_VehiclePreset preset,array<BaseCompartmentSlot> seats){
		seats.Clear();
		if (!vehicle || !preset || preset.m_aSeats.IsEmpty() || preset.m_aSeats.Count() > MAX_SEATS ||
			preset.m_aSeats.Count() != preset.m_aMembers.Count()) return false;
		BaseCompartmentManagerComponent manager = BaseCompartmentManagerComponent.Cast(vehicle.FindComponent(BaseCompartmentManagerComponent));
		if (!manager || manager.GetOwner() != vehicle) return false;
		array<BaseCompartmentSlot> raw = {};
		manager.GetCompartments(raw);
		if (raw.IsEmpty() || raw.Count() > MAX_SEATS) return false;
		array<string> keys = {};
		foreach (BaseCompartmentSlot seat : raw)
		{
			if (!seat || seat.GetVehicle() != vehicle || !seat.GetOwner() || !seat.GetManager() ||
				seat.GetManager().GetOwner() != seat.GetOwner() || RelativeOwnerIndex(seat) < 0) return false;
			string key = SeatKey(seat);
			if (keys.Contains(key)) return false;
			keys.Insert(key);
			// Catalog мог изначально исключить недоступные места. Новое доступное
			// место вне captured recipe означает настоящую несовместимость.
			if (!preset.m_aSeats.Contains(key) && seat.IsCompartmentAccessible()) return false;
		}
		array<BaseCompartmentSlot> resolved = {};
		foreach (string captured : preset.m_aSeats)
		{
			int index = keys.Find(captured);
			if (index < 0 || resolved.Contains(raw[index])) return false;
			resolved.Insert(raw[index]);
		}
		seats.Copy(resolved);
		return true;
	}
'@)
$signatures=[ordered]@{
 CollectHierarchy='static void CollectHierarchy(IEntity entity,AICFArray<IEntity> owners)'
 RelativeOwnerIndex='public static int RelativeOwnerIndex(BaseCompartmentSlot seat)'
 SeatKey='public static string SeatKey(BaseCompartmentSlot seat)'
 Collect='public static void Collect(IEntity vehicle,AICFArray<BaseCompartmentSlot> seats)'
 MatchesSeats='public bool MatchesSeats(AICFArray<BaseCompartmentSlot> seats)'
 CollectCaptured='public static bool CollectCaptured(IEntity vehicle,AICF_VehiclePreset preset,AICFArray<BaseCompartmentSlot> seats)'
}
$current=@(foreach($entry in $signatures.GetEnumerator()){$entry.Value+(Adapt (Body $source $entry.Key))}) -join "`n"
$current+='static string SeatKeyAtOwner(BaseCompartmentSlot seat,int ownerIndex)'+(Adapt (Body $source 'SeatKeyAtOwner'))
$scaffold=@'
using System;using System.Collections;using System.Collections.Generic;
public class AICFArray<T>:IEnumerable<T>{public List<T> Items=new List<T>();public int Count(){return Items.Count;}public bool IsEmpty(){return Items.Count==0;}public void Insert(T x){Items.Add(x);}public void InsertAt(T x,int i){Items.Insert(i,x);}public int Find(T x){return Items.IndexOf(x);}public bool Contains(T x){return Items.Contains(x);}public void Clear(){Items.Clear();}public void Copy(AICFArray<T> other){Items.Clear();Items.AddRange(other.Items);}public T this[int i]{get{return Items[i];}set{Items[i]=value;}}public IEnumerator<T> GetEnumerator(){return Items.GetEnumerator();}IEnumerator IEnumerable.GetEnumerator(){return GetEnumerator();}}
public class Node{public static implicit operator bool(Node x){return !Object.ReferenceEquals(x,null);}}
public class IEntity:Node{public static int Visits;public string Kind="Part";public IEntity Child,Sibling;public BaseCompartmentManagerComponent Manager;public IEntity GetChildren(){Visits++;return Child;}public IEntity GetSibling(){return Sibling;}public object FindComponent(Type t){return Manager;}}
public class BaseCompartmentManagerComponent:Node{public IEntity Owner;public AICFArray<BaseCompartmentSlot> Raw=new AICFArray<BaseCompartmentSlot>();public IEntity GetOwner(){return Owner;}public void GetCompartments(AICFArray<BaseCompartmentSlot> dst){dst.Copy(Raw);}public static BaseCompartmentManagerComponent Cast(object x){return x as BaseCompartmentManagerComponent;}}
public class BaseCompartmentSlot:Node{public IEntity Vehicle,Owner;public BaseCompartmentManagerComponent Manager;public bool Accessible=true;public int Id,Type;public IEntity GetVehicle(){return Vehicle;}public IEntity GetOwner(){return Owner;}public BaseCompartmentManagerComponent GetManager(){return Manager;}public bool IsCompartmentAccessible(){return Accessible;}public int GetCompartmentSlotID(){return Id;}public new int GetType(){return Type;}}
public static class SF{public static string Format(string text,params object[] values){for(int i=values.Length;i>0;i--)text=text.Replace("%"+i,Convert.ToString(values[i-1]));return text;}}
public class AICF_VehiclePreset:Node{public const int MAX_SEATS=64;public AICFArray<string> m_aSeats=new AICFArray<string>();public AICFArray<int> m_aMembers=new AICFArray<int>();CURRENT}
public class LegacyPreset:Node{public const int MAX_SEATS=64;public AICFArray<string> m_aSeats=new AICFArray<string>();LEGACY}
public class Scene{
 public IEntity Car,Rear,Character,InventoryOwner;public int Nodes;public AICFArray<BaseCompartmentSlot> Raw;
 static IEntity Part(bool manager){var e=new IEntity();if(manager)e.Manager=new BaseCompartmentManagerComponent{Owner=e};return e;}
 public static Scene Make(int seats,int gear){var s=new Scene();s.Car=Part(true);s.Character=Part(false);s.Character.Kind="Character";s.InventoryOwner=Part(true);s.Rear=Part(true);s.Car.Child=s.Character;s.Character.Sibling=s.Rear;s.Character.Child=s.InventoryOwner;s.Nodes=4;for(int i=0;i<gear;i++){var item=Part(false);item.Sibling=s.InventoryOwner.Child;s.InventoryOwner.Child=item;s.Nodes++;}s.Raw=s.Car.Manager.Raw;for(int i=0;i<seats;i++){var owner=i%3==0?s.Car:i%3==1?s.InventoryOwner:s.Rear;s.Raw.Insert(new BaseCompartmentSlot{Vehicle=s.Car,Owner=owner,Manager=owner.Manager,Id=i/3,Type=i%3});}return s;}
}
public static class TopologyChecks{
 static int n;static void Need(bool ok,string reason){n++;if(!ok)throw new Exception(reason);}
 static void Same(AICFArray<BaseCompartmentSlot> a,AICFArray<BaseCompartmentSlot> b,string reason){Need(a.Count()==b.Count(),reason+" count");for(int i=0;i<a.Count();i++)Need(a[i]==b[i],reason+" reference/order");}
 static AICF_VehiclePreset Recipe(AICFArray<BaseCompartmentSlot> seats){var p=new AICF_VehiclePreset();for(int i=0;i<seats.Count();i++){p.m_aSeats.Insert(LegacyPreset.SeatKey(seats[i]));p.m_aMembers.Insert(i);}return p;}
 public static int Run(){n=0;foreach(int count in new[]{1,5,15,32,64})foreach(int gear in new[]{0,12,200}){
  var s=Scene.Make(count,gear);var old=new AICFArray<BaseCompartmentSlot>();var current=new AICFArray<BaseCompartmentSlot>();
  IEntity.Visits=0;LegacyPreset.Collect(s.Car,old);int oldVisits=IEntity.Visits;IEntity.Visits=0;AICF_VehiclePreset.Collect(s.Car,current);int newVisits=IEntity.Visits;
  Same(old,current,"catalog legacy compatibility");Need(newVisits<=s.Nodes,"Collect visits hierarchy at most once including character inventory");if(count>=15)Need(oldVisits>newVisits*count,"legacy superlinear traversal reproduced");
  var recipe=Recipe(old);IEntity.Visits=0;Need(recipe.MatchesSeats(current),"matches exact legacy keys");Need(IEntity.Visits==s.Nodes,"MatchesSeats one hierarchy per common vehicle");
  var captured=new AICFArray<BaseCompartmentSlot>();var previous=new AICFArray<BaseCompartmentSlot>();IEntity.Visits=0;Need(AICF_VehiclePreset.CollectCaptured(s.Car,recipe,captured),"captured actual valid");Need(IEntity.Visits==s.Nodes,"CollectCaptured one hierarchy");Need(LegacyPreset.CollectCaptured(s.Car,recipe,previous),"legacy captured valid");Same(captured,previous,"captured legacy compatibility");
  s.Raw[0].Accessible=false;Need(AICF_VehiclePreset.CollectCaptured(s.Car,recipe,captured)&&LegacyPreset.CollectCaptured(s.Car,recipe,previous),"captured inaccessible retained");Same(captured,previous,"captured flag compatibility");AICF_VehiclePreset.Collect(s.Car,current);LegacyPreset.Collect(s.Car,old);Same(current,old,"catalog inaccessible excluded");s.Raw[0].Accessible=true;
  s.Raw.Items.Reverse();AICF_VehiclePreset.Collect(s.Car,current);LegacyPreset.Collect(s.Car,old);Same(current,old,"reversed native enumeration");
  var inserted=new IEntity{Manager=new BaseCompartmentManagerComponent()};inserted.Manager.Owner=inserted;inserted.Sibling=s.Car.Child;s.Car.Child=inserted;s.Nodes++;
  Need(recipe.MatchesSeats(current)==(count==1),"new call cannot reuse stale hierarchy ordinals");AICF_VehiclePreset.Collect(s.Car,current);LegacyPreset.Collect(s.Car,old);Same(current,old,"hierarchy change observed next call");
 }
 var first=Scene.Make(3,15);var second=Scene.Make(3,20);var mixed=new AICFArray<BaseCompartmentSlot>();mixed.Insert(first.Raw[0]);mixed.Insert(second.Raw[1]);mixed.Insert(first.Raw[2]);mixed.Insert(second.Raw[0]);var mixedRecipe=Recipe(mixed);IEntity.Visits=0;Need(mixedRecipe.MatchesSeats(mixed),"legacy mixed vehicle inputs preserved");Need(IEntity.Visits==first.Nodes+second.Nodes,"mixed roots each walked only once per call");
 var invalid=Scene.Make(5,9);var output=new AICFArray<BaseCompartmentSlot>();var invalidRecipe=Recipe(invalid.Raw);var outsider=new IEntity{Manager=new BaseCompartmentManagerComponent()};outsider.Manager.Owner=outsider;invalid.Raw[0].Owner=outsider;invalid.Raw[0].Manager=outsider.Manager;
 Need(!AICF_VehiclePreset.CollectCaptured(invalid.Car,invalidRecipe,output)&&output.IsEmpty(),"outside owner remains rejected atomically");AICF_VehiclePreset.Collect(invalid.Car,output);var legacyOutput=new AICFArray<BaseCompartmentSlot>();LegacyPreset.Collect(invalid.Car,legacyOutput);Same(output,legacyOutput,"legacy catalog negative ordinal order preserved");
 return n;}
}
'@
$code=$scaffold.Replace('CURRENT',$current).Replace('LEGACY',$legacyMethods)
Add-Type -TypeDefinition $code
$cases=[TopologyChecks]::Run()
$mutations=@(
 @('int ownerIndex = owners.Find(seat.GetOwner());','CollectHierarchy(vehicle, owners); int ownerIndex = owners.Find(seat.GetOwner());'),
 @('CollectHierarchy(child, owners);','if (child.Kind != "Character") CollectHierarchy(child, owners);'),
 @('ordinals.InsertAt(ownerIndex, index);','ordinals.InsertAt(ownerIndex, 0);')
)
$caught=0
foreach($change in $mutations){
 $changed=$current.Replace($change[0],$change[1]);if($changed -ceq $current){throw 'Missing performance mutation'}
 $ns='TopologyUnsafe'+$caught;Add-Type -TypeDefinition ('namespace '+$ns+' {'+$scaffold.Replace('CURRENT',$changed).Replace('LEGACY',$legacyMethods)+'}')
 $failed=$false;try{([type]($ns+'.TopologyChecks')).GetMethod('Run').Invoke($null,@())|Out-Null}catch{$failed=$true}
 if(!$failed){throw "Unsafe topology mutant escaped: $($change[0])"};$caught++
}
$fixture=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'tests/fixtures/AICF_VehicleTopologyPerformanceProbe.c'))
function FixtureRules($text){
 $update=Body $text 'Update';$capture=Body $text 'Capture';$bench=Body $text 'AICF_BenchmarkTopology';$repeat=Body $text 'Current'
 if(([regex]::Matches($update,'super.Update\(state, vehicles\);')).Count -ne 1 -or !$update.Contains('!System.IsCLIParam("aicfTopologyPerformanceProbe")') -or !$update.Contains('m_bTopologyPerformanceDone = true;')){'SINGLE_OPT_IN_AFTER_SUPER'}
 foreach($guard in @('agents.Count() != 10','AICF_GroupRuntime.CountAliveAgents(group) != 10','AICF_VehicleNativeSubgroup.Settled(member, vehicle)','occupants.Count() != 10','!scope.m_aMembers.Contains(occupant) || occupants.Contains(occupant)','inventory.GetItems(items);','if (items.IsEmpty()) return null;')){if(!$capture.Contains($guard)){'EXACT_TEN_EQUIPPED_OCCUPANTS';break}}
 foreach($guard in @('repeated.m_sGroupRpl != m_sGroupRpl','repeated.m_iTripGeneration != m_iTripGeneration','repeated.m_iLeaseGeneration != m_iLeaseGeneration','m_aMembers[index].GetID() != m_aMemberIds[index]')){if(!$repeat.Contains($guard)){'REPEATED_IMMUTABLE_SCOPE';break}}
 if(!$bench.Contains('Math.Clamp(value.ToInt(), 1, 3)') -or !$bench.Contains('bool before = scope.Current();') -or !$bench.Contains('bool after = scope.Current();') -or !$bench.Contains('ordinary_fps_measurement=0')){'BOUNDED_DIAGNOSTIC_BENCHMARK'}
 if((ConvertTo-AICFCodeText $text) -match 'SpawnEntity|SetOrigin|SetWorldTransform|GetInVehicle\(|GetOutVehicle\(|SetHealth|SetDamage|CallLater|RequestClose|DeleteEntity'){'NO_GAMEPLAY_OR_LIFECYCLE_MUTATION'}
}
$issues=@(FixtureRules $fixture);if($issues.Count){throw "Native benchmark fixture scope: $issues"}
$fixtureControls=0
foreach($mutation in @(
 @('!System.IsCLIParam("aicfTopologyPerformanceProbe")','false','SINGLE_OPT_IN_AFTER_SUPER'),
 @('occupants.Count() != 10','occupants.Count() < 1','EXACT_TEN_EQUIPPED_OCCUPANTS'),
 @('if (items.IsEmpty()) return null;','','EXACT_TEN_EQUIPPED_OCCUPANTS'),
 @('repeated.m_iLeaseGeneration != m_iLeaseGeneration','false','REPEATED_IMMUTABLE_SCOPE'),
 @('bool after = scope.Current();','bool after = true;','BOUNDED_DIAGNOSTIC_BENCHMARK'),
 @('super.Update(state, vehicles);','super.Update(state, vehicles); GetGame().RequestClose();','NO_GAMEPLAY_OR_LIFECYCLE_MUTATION')
)){
 $changed=$fixture.Replace($mutation[0],$mutation[1]);if($changed -ceq $fixture){throw 'Missing native fixture mutation'}
 if($mutation[2] -notin @(FixtureRules $changed)){throw "Native fixture guard escaped: $($mutation[2])"};$fixtureControls++
}
"PASS vehicle topology performance: actual_cases=$cases unsafe_mutants=$caught fixture_unsafe_controls=$fixtureControls old_new_order_keys=IDENTICAL calls_local=1 character_inventory_ordinal=PRESERVED native_timing=NOT_RUN"

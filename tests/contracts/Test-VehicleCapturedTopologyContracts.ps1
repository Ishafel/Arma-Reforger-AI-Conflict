param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$vehicles=Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Vehicles'
$preset=[IO.File]::ReadAllText((Join-Path $vehicles 'AICF_VehiclePreset.c'))
$flow=[IO.File]::ReadAllText((Join-Path $vehicles 'AICF_VehicleBehaviorFlow.c'))
$fixture=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'tests/fixtures/AICF_VehicleBehaviorProbe.c'))
function Body([string]$text,[string]$name){Get-AICFMethodBody ([pscustomobject]@{Source=$text;Code=(ConvertTo-AICFCodeText $text)}) $name}
function Rules([string]$p,[string]$f,[string]$probe){
 $resolve=Body $p 'CollectCaptured';$collect=Body $p 'Collect';$trace=Body $f 'ReportTopologyMismatch';$wrap=Body $f 'CollectBehaviorSeats'
 if(!$resolve -or !$trace -or !$wrap){'METHODS';return}
 foreach($guard in @('preset.m_aSeats.IsEmpty()', 'preset.m_aSeats.Count() > MAX_SEATS',
  'preset.m_aSeats.Count() != preset.m_aMembers.Count()', 'manager.GetOwner() != vehicle',
  'raw.IsEmpty() || raw.Count() > MAX_SEATS', 'seat.GetVehicle() != vehicle', '!seat.GetOwner() || !seat.GetManager()',
  'seat.GetManager().GetOwner() != seat.GetOwner() || RelativeOwnerIndex(seat) < 0',
  'if (keys.Contains(key)) return false;',
  '!preset.m_aSeats.Contains(key) && seat.IsCompartmentAccessible()',
  'index < 0 || resolved.Contains(raw[index])')){
  if(!$resolve.Contains($guard)){'FULL_RAW_STRUCTURE_AND_CAPTURED_KEYS';break}
 }
 if(!$resolve.Contains('manager.GetCompartments(raw);') -or !$resolve.Contains('seats.Copy(resolved);') -or
  !$resolve.Contains('foreach (string captured : preset.m_aSeats)') -or
  $resolve.IndexOf('seats.Copy(resolved);') -lt $resolve.IndexOf('index < 0 || resolved.Contains(raw[index])')){'ATOMIC_ORDERED_CAPTURED_RESOLUTION'}
 if((ConvertTo-AICFCodeText $resolve) -match 'SetCompartmentAccessible|SetReserved|GetInVehicle|GetOutVehicle|SetHealth|Delete|Spawn|\.Fail\(|\.Complete\(' -or
  $resolve -match '!seat.IsCompartmentAccessible\(\)'){'NO_ACCESSIBILITY_FILTER_OR_PERMISSION_MUTATION'}
 if(!$collect.Contains('!seat.IsCompartmentAccessible()')){'CATALOG_COLLECT_UNCHANGED'}
 foreach($method in @('InspectTransit','Begin','RestoreCrewAssignments','PollReboarding')){
  $body=Body $f $method
  if(!$body.Contains('CollectBehaviorSeats(trip, seats,') -or $body.Contains('AICF_VehiclePreset.Collect(')){'ALL_BEHAVIOR_CONSUMERS_USE_CAPTURED';break}
 }
 if(!$wrap.Contains('AICF_VehiclePreset.CollectCaptured(trip.GetLease().GetVehicle(), trip.GetVehiclePreset(), seats)') -or
  !$wrap.Contains('ReportTopologyMismatch(trip, context);') -or !$wrap.Contains('return false;')){'ACTUAL_MISMATCH_DIAGNOSTIC_BOUNDARY'}
 foreach($guard in @('if (!IsCurrent(trip)) return;', 'group.GetAllocatedCompartments(allocated);',
  'node < hierarchy.Count() && node < 256', 'hierarchy.Count() < 256', 'manager.GetCompartments(raw);',
  'emitted >= AICF_VehiclePreset.MAX_SEATS', 'allocated && allocated.Contains(seat)',
  'seat.IsCompartmentAccessible()', 'seat.IsReserved()', 'seat.IsGetInLocked()', 'current_get_in_vehicle=',
  'VEHICLE_BEHAVIOR_TOPOLOGY_MISMATCH','VEHICLE_BEHAVIOR_TOPOLOGY_MANAGER','VEHICLE_BEHAVIOR_TOPOLOGY_SEAT')){
  if(!$trace.Contains($guard)){'BOUNDED_RAW_ALLOCATION_DIAGNOSTICS';break}
 }
 if((ConvertTo-AICFCodeText $trace) -match 'allocated\.(?:Clear|Insert|Remove|Sort|Copy)|SetCompartmentAccessible|SetReserved|\.GetInVehicle\(|\.GetOutVehicle\(|SetHealth'){'ALLOCATION_ALIAS_READONLY'}
 $inject=Body $probe 'AICF_VBNativeAllocation'
 foreach($proof in @('selected.SetCompartmentAccessible(false);', 'AICF_VehiclePreset.Collect(m_VBVehicle, oldFiltered);',
  'AICF_VehiclePreset.CollectCaptured(m_VBVehicle, preset, captured)', 'bool flagPreserved = !selected.IsCompartmentAccessible();',
  'captured[index] != original[index]', 'selected.SetCompartmentAccessible(accessibleBefore);',
  'selected.GetOccupant() == occupant', 'oldFiltered.Count() == original.Count() - 1',
  'NATIVE_ALLOCATION_NOT_TOPOLOGY_CHANGE', 'organic_soak_cause_proven=0')){if(!$inject.Contains($proof)){'CAUSAL_NATIVE_FLAG_PROOF';break}}
 if($inject.IndexOf('selected.SetCompartmentAccessible(accessibleBefore);') -gt $inject.IndexOf('AICF_VBCheck(') -or
  $inject -match 'SetHealth|Teleport|SetOrigin|CallLater'){'FIXTURE_RESTORES_FLAG_BEFORE_VERDICT'}
}
$issues=@(Rules $preset $flow $fixture);if($issues.Count){throw "Captured topology: $issues"}
$controls=1
foreach($mutation in @(
 @('p','if (keys.Contains(key)) return false;','if (false) return false;','FULL_RAW_STRUCTURE_AND_CAPTURED_KEYS'),
 @('p','index < 0 || resolved.Contains(raw[index])','index < 0','FULL_RAW_STRUCTURE_AND_CAPTURED_KEYS'),
 @('p','!preset.m_aSeats.Contains(key) && seat.IsCompartmentAccessible()','false','FULL_RAW_STRUCTURE_AND_CAPTURED_KEYS'),
 @('p','seat.GetManager().GetOwner() != seat.GetOwner() || RelativeOwnerIndex(seat) < 0','false','FULL_RAW_STRUCTURE_AND_CAPTURED_KEYS'),
 @('p','seat.GetVehicle() != vehicle','false','FULL_RAW_STRUCTURE_AND_CAPTURED_KEYS'),
 @('p','seats.Copy(resolved);','seats.Copy(raw);','ATOMIC_ORDERED_CAPTURED_RESOLUTION'),
 @('f','group.GetAllocatedCompartments(allocated);','group.ReleaseCompartments();','BOUNDED_RAW_ALLOCATION_DIAGNOSTICS'),
 @('f','emitted >= AICF_VehiclePreset.MAX_SEATS','false','BOUNDED_RAW_ALLOCATION_DIAGNOSTICS'),
 @('probe','selected.SetCompartmentAccessible(accessibleBefore);','Print("omitted restore");','CAUSAL_NATIVE_FLAG_PROOF'),
 @('probe','captured[index] != original[index]','false','CAUSAL_NATIVE_FLAG_PROOF')
)){
 $p=$preset;$f=$flow;$probe=$fixture
 switch($mutation[0]){'p'{$p=$p.Replace($mutation[1],$mutation[2])}'f'{$f=$f.Replace($mutation[1],$mutation[2])}'probe'{$probe=$probe.Replace($mutation[1],$mutation[2])}}
 if($p -ceq $preset -and $f -ceq $flow -and $probe -ceq $fixture){throw "Missing mutation: $($mutation[3])"}
 if($mutation[3] -notin @(Rules $p $f $probe)){throw "Mutation survived: $($mutation[3])"};$controls++
}
$resolve=Body $preset 'CollectCaptured'
$unsafe=$resolve.Insert(1," seat.SetCompartmentAccessible(true);")
if('NO_ACCESSIBILITY_FILTER_OR_PERMISSION_MUTATION' -notin @(Rules ($preset.Replace($resolve,$unsafe)) $flow $fixture)){throw 'Permission mutation survived'};$controls++
$trace=Body $flow 'ReportTopologyMismatch'
$unsafe=$trace.Insert(1," allocated.Clear();")
if('ALLOCATION_ALIAS_READONLY' -notin @(Rules $preset ($flow.Replace($trace,$unsafe)) $fixture)){throw 'Native allocation alias mutation survived'};$controls++
# Исполняем actual production resolver body; adapter подменяет native objects/array syntax.
$body=$resolve.Replace('foreach (BaseCompartmentSlot seat : raw)','foreach (BaseCompartmentSlot seat in raw)').Replace('foreach (string captured : preset.m_aSeats)','foreach (string captured in preset.m_aSeats)').Replace('vehicle.FindComponent(BaseCompartmentManagerComponent)','vehicle.FindComponent(typeof(BaseCompartmentManagerComponent))')
$body=[regex]::Replace($body,'array<([^>]+)> (\w+) = \{\};',{param($m) 'AICFArray<'+$m.Groups[1].Value+'> '+$m.Groups[2].Value+' = new AICFArray<'+$m.Groups[1].Value+'>();'})
$scaffold=@'
using System; using System.Collections; using System.Collections.Generic;
public class AICFArray<T>:IEnumerable<T> {
 readonly List<T> values=new List<T>(); public int Count(){return values.Count;} public bool IsEmpty(){return values.Count==0;}
 public void Clear(){values.Clear();} public void Insert(T value){values.Add(value);} public bool Contains(T value){return values.Contains(value);}
 public int Find(T value){return values.IndexOf(value);} public T this[int i]{get{return values[i];}set{values[i]=value;}}
 public void Copy(AICFArray<T> other){values.Clear();values.AddRange(other.values);} public void Remove(int i){values.RemoveAt(i);}
 public IEnumerator<T> GetEnumerator(){return values.GetEnumerator();} IEnumerator IEnumerable.GetEnumerator(){return GetEnumerator();}
}
public class IEntity {public BaseCompartmentManagerComponent Manager;public object FindComponent(Type type){return Manager;} public static implicit operator bool(IEntity v){return !object.ReferenceEquals(v,null);}}
public class BaseCompartmentManagerComponent {public IEntity Owner;public AICFArray<BaseCompartmentSlot> Raw=new AICFArray<BaseCompartmentSlot>(); public static BaseCompartmentManagerComponent Cast(object o){return o as BaseCompartmentManagerComponent;}public IEntity GetOwner(){return Owner;} public void GetCompartments(AICFArray<BaseCompartmentSlot> dst){dst.Copy(Raw);}public static implicit operator bool(BaseCompartmentManagerComponent v){return !object.ReferenceEquals(v,null);}}
public class BaseCompartmentSlot {public string Key;public bool Accessible=true;public IEntity Vehicle;public IEntity Owner;public BaseCompartmentManagerComponent Manager;public int Ordinal=0;public IEntity GetVehicle(){return Vehicle;}public IEntity GetOwner(){return Owner;}public BaseCompartmentManagerComponent GetManager(){return Manager;}public bool IsCompartmentAccessible(){return Accessible;}public static implicit operator bool(BaseCompartmentSlot v){return !object.ReferenceEquals(v,null);}}
public class AICF_VehiclePreset {public AICFArray<string> m_aSeats=new AICFArray<string>();public AICFArray<int> m_aMembers=new AICFArray<int>();public static implicit operator bool(AICF_VehiclePreset v){return !object.ReferenceEquals(v,null);}}
public class CapturedTopologyHarness {
 const int MAX_SEATS=64; static int RelativeOwnerIndex(BaseCompartmentSlot seat){return seat.Ordinal;}static string SeatKey(BaseCompartmentSlot seat){return seat.Key;}
'@
$scaffold+='public static bool Resolve(IEntity vehicle,AICF_VehiclePreset preset,AICFArray<BaseCompartmentSlot> seats)'+$body
$scaffold+=@'
 static void Check(bool pass,string name){if(!pass)throw new Exception(name);}
 public static int Run(){int count=0;IEntity car=new IEntity();car.Manager=new BaseCompartmentManagerComponent{Owner=car};var recipe=new AICF_VehiclePreset();var output=new AICFArray<BaseCompartmentSlot>();
 for(int i=0;i<3;i++){string key="0:"+i+":2";recipe.m_aSeats.Insert(key);recipe.m_aMembers.Insert(i);car.Manager.Raw.Insert(new BaseCompartmentSlot{Key=key,Vehicle=car,Owner=car,Manager=car.Manager});}
 Check(Resolve(car,recipe,output)&&output.Count()==3,"baseline");count++;
 var chosen=car.Manager.Raw[1];chosen.Accessible=false;Check(Resolve(car,recipe,output)&&output[1]==chosen&&!chosen.Accessible,"mutable flag still captured and not restored");count++;chosen.Accessible=true;
 var first=car.Manager.Raw[0];car.Manager.Raw[0]=car.Manager.Raw[2];car.Manager.Raw[2]=first;Check(Resolve(car,recipe,output)&&output[0]==first,"captured order not native iteration order");count++;
 var extra=new BaseCompartmentSlot{Key="0:99:2",Vehicle=car,Owner=car,Manager=car.Manager};car.Manager.Raw.Insert(extra);Check(!Resolve(car,recipe,output)&&output.IsEmpty(),"unknown accessible extra");count++;extra.Accessible=false;Check(Resolve(car,recipe,output)&&output.Count()==3,"unknown inaccessible catalog exclusion");count++;
 extra.Vehicle=new IEntity();Check(!Resolve(car,recipe,output),"foreign inaccessible seat rejected");count++;extra.Vehicle=car;car.Manager.Raw.Remove(3);
 string expected=recipe.m_aSeats[0];recipe.m_aSeats[0]="0:100:2";Check(!Resolve(car,recipe,output)&&output.IsEmpty(),"missing key atomic failure");count++;recipe.m_aSeats[0]=expected;
 recipe.m_aSeats[0]=recipe.m_aSeats[1];Check(!Resolve(car,recipe,output),"duplicate captured key");count++;recipe.m_aSeats[0]=expected;
 string originalKey=first.Key;first.Key=chosen.Key;Check(!Resolve(car,recipe,output),"duplicate raw key");count++;first.Key=originalKey;
 first.Ordinal=-1;Check(!Resolve(car,recipe,output),"owner outside actual vehicle hierarchy");count++;first.Ordinal=0;
 var correctManager=first.Manager;first.Manager=new BaseCompartmentManagerComponent{Owner=new IEntity()};Check(!Resolve(car,recipe,output),"manager owner mismatch");count++;first.Manager=correctManager;
 car.Manager.Raw.Insert(null);Check(!Resolve(car,recipe,output),"null raw slot");count++;car.Manager.Raw.Remove(3);
 recipe.m_aMembers.Remove(2);Check(!Resolve(car,recipe,output),"parallel mapping shape");count++;recipe.m_aMembers.Insert(2);
 for(int i=3;i<65;i++)car.Manager.Raw.Insert(new BaseCompartmentSlot{Key="x"+i,Vehicle=car,Owner=car,Manager=car.Manager,Accessible=false});Check(!Resolve(car,recipe,output),"bounded raw cardinality");count++;
 Check(!Resolve(null,recipe,output),"missing vehicle");count++;return count;
 }
}
'@
Add-Type -TypeDefinition $scaffold
$nativeAdapterControls=[CapturedTopologyHarness]::Run()
"PASS Captured topology: exact bounded raw keys despite mutable native allocation flags; missing/duplicate/foreign/accessible extras rejected, original catalog and GetIn permissions retained; source_controls=$controls actual_resolver_cases=$nativeAdapterControls native_causal_fixture=REQUIRES_SEPARATE_RUNTIME"

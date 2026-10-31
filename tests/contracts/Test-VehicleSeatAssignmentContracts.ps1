param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
$path=Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Vehicles/AICF_VehicleSeatAssignment.c'
$source=[IO.File]::ReadAllText($path)
# Исполняем тело helper; adapter меняет только синтаксис Enforce коллекций/visibility.
function Translate([string]$text,[string]$name) {
 $text=$text.Replace('class AICF_VehicleSeatAssignment',"public class $name").Replace('static bool AutoAssign','public static bool AutoAssign')
 $text=$text.Replace('ValidateShape(reason)','ValidateShape(out reason)')
 $text=$text.Replace('array<int>','List<int>').Replace('= {};','= new List<int>();')
 $text=$text.Replace('!preset || !activeMembers || !mandatoryIndices','preset == null || activeMembers == null || mandatoryIndices == null')
 $text=$text.Replace('!preset.m_aSeats || !preset.m_aMembers','preset.m_aSeats == null || preset.m_aMembers == null')
 $text=[regex]::Replace($text,'foreach \(int (\w+) : (\w+)\)','foreach (int $1 in $2)')
 $text=[regex]::Replace($text,'\bint (position|seat|next|mandatory|cargo|assigned);','int $1 = 0;')
 return $text
}
$id='SeatAssignment'+[guid]::NewGuid().ToString('N')
$variants=@(
 @{Name=$id;Source=$source},
 @{Name=$id+'UnsafeCargo';Source=$source.Replace('if (required.Contains(mandatory)) assignments[mandatory] = ordered[next++];','if (required.Contains(-1)) assignments[mandatory] = ordered[next++];')},
 @{Name=$id+'UnsafeDuplicates';Source=$source.Replace(' || members.Contains(member)','')},
 @{Name=$id+'UnsafeShortCrew';Source=$source.Replace('if (members.Count() < required.Count()) return false;','')}
)
foreach($variant in $variants | Select-Object -Skip 1){if($variant.Source -eq $source){throw 'FAIL unsafe target absent'}}
$code=@'
using System;
using System.Collections.Generic;
using System.Linq;
public static class NativeArrayAdapter {
 public static bool IsEmpty<T>(this List<T> values) { return values.Count == 0; }
 public static void Insert<T>(this List<T> values,T value) { values.Add(value); }
}
public static class AICF_Stage1Config { public const int MAX_GROUP_SIZE=10; }
public class AICF_VehiclePreset {
 public List<int> m_aSeats=new List<int>();
 public List<int> m_aMembers=new List<int>();
 public bool Shape=true;
 public bool ValidateShape(out string reason) { reason="READY";return Shape && m_aSeats.Count>0 && m_aSeats.Count<=64 && m_aMembers.Count==m_aSeats.Count; }
}
'@
foreach($variant in $variants){$code += "`n"+(Translate $variant.Source $variant.Name)}
$code+=@'
public class AssignmentReplay {
 public delegate bool Assign(AICF_VehiclePreset preset,List<int> active,List<int> required);
 static int checks;
 static void Check(bool result,string label) { if(!result)throw new Exception("FAIL "+label);checks++; }
 static List<int> L(params int[] values) { return values.ToList(); }
 static AICF_VehiclePreset P(int count) { var p=new AICF_VehiclePreset();for(int i=0;i<count;i++){p.m_aSeats.Add(i);p.m_aMembers.Add(-1);}return p; }
 static void Refuse(Assign assign,AICF_VehiclePreset p,List<int> active,List<int> required,string name) {
  var before=p==null?null:p.m_aMembers.ToArray();bool accepted=assign(p,active,required);
  Check(!accepted,name);if(p!=null)Check(before.SequenceEqual(p.m_aMembers),name+"_atomic");
 }
 public static string Run(Assign assign,Assign cargo,Assign duplicates,Assign shortCrew) {
  checks=0;
  var p=P(5);p.m_aMembers=L(9,8,7,6,5);
  Check(assign(p,L(9,2,5),L(4,2)),"assign");Check(p.m_aMembers.SequenceEqual(L(9,-1,2,-1,5)),"mandatory_first_numeric");
  Check(assign(p,L(5,9,2),L(2,4)),"repeat");Check(p.m_aMembers.SequenceEqual(L(9,-1,2,-1,5)),"input_order_invariant");
  Check(assign(p,L(5,2),L(4,2)),"required_only");Check(p.m_aMembers.SequenceEqual(L(-1,-1,2,-1,5)),"stale_removed");
  p=P(2);Check(assign(p,L(9,0,4),L(1)),"overflow_roster");Check(p.m_aMembers.SequenceEqual(L(4,0)),"extra_members_unassigned");
  p=P(64);Check(assign(p,L(9,1,8,0,7,2,6,3,5,4),L(63)),"max_topology");Check(p.m_aMembers[63]==0 && p.m_aMembers.Take(9).SequenceEqual(Enumerable.Range(1,9)),"max_numeric");
  p=P(1);Check(assign(p,L(9),L(0)),"single");Check(p.m_aMembers[0]==9,"single_value");
  Refuse(assign,null,L(0),L(0),"null_preset");Refuse(assign,P(2),null,L(0),"null_active");Refuse(assign,P(2),L(0),null,"null_required");
  Refuse(assign,P(2),L(),L(0),"no_alive");Refuse(assign,P(2),L(0),L(),"unknown_topology");
  Refuse(assign,P(2),L(0),L(0,1),"insufficient_crew");Refuse(assign,P(2),L(0,0),L(0),"duplicate_active");
  Refuse(assign,P(2),L(0),L(0,0),"duplicate_required");Refuse(assign,P(2),L(-1),L(0),"negative_active");
  Refuse(assign,P(2),L(10),L(0),"outside_active");Refuse(assign,P(2),L(0),L(-1),"negative_seat");
  Refuse(assign,P(2),L(0),L(2),"outside_seat");Refuse(assign,P(2),L(0,1,2,3,4,5,6,7,8,9,10),L(0),"roster_bound");
  Refuse(assign,P(0),L(0),L(0),"empty_shape");Refuse(assign,P(65),L(0),L(0),"large_shape");
  p=P(2);p.Shape=false;Refuse(assign,p,L(0),L(0),"invalid_recipe");p=P(2);p.m_aMembers.RemoveAt(0);Refuse(assign,p,L(0),L(0),"malformed_members");
  p=P(5);Check(cargo(p,L(9,2,5),L(4,2)) && !p.m_aMembers.SequenceEqual(L(9,-1,2,-1,5)),"unsafe_cargo_detected");
  p=P(2);Check(duplicates(p,L(0,0),L(0)),"unsafe_duplicate_detected");
  bool threw=false;try {shortCrew(P(2),L(0),L(0,1));}catch(ArgumentOutOfRangeException){threw=true;}Check(threw,"unsafe_short_crew_detected");
  return "PASS Vehicle seat assignment actual="+checks+" unsafe=3 native_runtime=NOT_RUN";
 }
}
'@
# Isolated names for helper variants; the contract runs in its own process.
Add-Type -TypeDefinition $code -Language CSharp
$actual=[type]$id;$unsafeCargo=[type]($id+'UnsafeCargo');$unsafeDuplicates=[type]($id+'UnsafeDuplicates');$unsafeShort=[type]($id+'UnsafeShortCrew')
$method=[AssignmentReplay+Assign]
$delegates=@(foreach($type in @($actual,$unsafeCargo,$unsafeDuplicates,$unsafeShort)){[Delegate]::CreateDelegate($method,$type.GetMethod('AutoAssign'))})
[AssignmentReplay]::Run($delegates[0],$delegates[1],$delegates[2],$delegates[3])
$service=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/UI/AICF_VehiclePresetService.c'))
if($service -notmatch 'json.WriteValue\("mandatorySeats", viewer.m_aMandatorySeats\)' -or $service -notmatch 'GetAttachedTurret\(\)' -or $service -notmatch 'ECompartmentType.PILOT' -or $service -notmatch 'ECompartmentType.TURRET'){throw 'FAIL actual catalog mandatory metadata'}
$reset=[regex]::Match($service,'(?s)else if \(operation == AICF_EVehiclePresetOperation.RESET\)(.*?)else if').Groups[1].Value
if($reset -notmatch '(?s)if \(accepted\).*?viewer.m_Draft = null' -or $reset -notmatch 'viewer.m_aMandatorySeats.Clear\(\)' -or $reset -match 'SetUnitType|AutoAssign|Spawn'){throw 'FAIL reset clears draft without changing unit policy'}
Write-Output 'PASS Service metadata/reset source controls=2'

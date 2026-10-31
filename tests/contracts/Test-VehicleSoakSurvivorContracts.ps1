param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$source=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'tests/fixtures/AICF_VehicleSoakProbe.c'))
function Body([string]$text,[string]$name){Get-AICFMethodBody ([pscustomobject]@{Source=$text;Code=(ConvertTo-AICFCodeText $text)}) $name}
function Rules([string]$text){
 $current=Body $text 'AICF_VehicleSoakSurvivorsCurrent';$foot=Body $text 'AICF_VehicleSoakRetainedFoot';$far=Body $text 'AICF_VehicleSoakFarthest'
 foreach($guard in @('slot.GetGroup().GetID() != m_VehicleSoakGroupId','slot.GetSpawnGeneration() != m_iVehicleSoakGeneration',
  'trip != m_VehicleSoakObservedTrip','trip != m_VehicleCoordinator.GetTrip(slot)','!trip.IsVehiclePresetCurrent()',
  'trip.VehiclePresetMember(position)','actor.GetID() == m_aVehicleSoakMembers[position]','current && current != actor',
  'fitCount > 0','CountAliveAgents(slot.GetGroup()) == aliveOriginals')){if(!$current.Contains($guard)){'SURVIVOR_IDENTITY_AND_ACTUAL_GROUP';break}}
 if(!$foot.Contains('trip.VehiclePresetMember(position)') -or $foot.Contains('slot.GetRosterMember(position)') -or
  !$foot.Contains('actor.IsInVehicle()') -or !$foot.Contains('access.IsInCompartment()') -or !$foot.Contains('access.IsGettingIn()') -or
  !$foot.Contains('access.IsGettingOut()') -or !$foot.Contains('access.IsSwitchingSeatsAnim()') -or !$foot.Contains('if (inside) return false;')){'ALL_CAPTURED_ORIGINALS_PHYSICAL_DETACH'}
 if(!$far.Contains('AICF_GroupRuntime.IsAliveCharacter(actor) && SCR_AIDamageHandling.IsConscious(actor)')){'ONLY_ACTUAL_FIT_DISTANCE'}
 $exact=Body $text 'AICF_VehicleSoakRosterExact';$seats=Body $text 'AICF_VehicleSoakExactSeats'
 if(!$exact.Contains('CountAliveAgents(slot.GetGroup()) != 5') -or !$seats.Contains('!AICF_VehicleSoakRosterExact(slot)')){'STARTUP_FIVE_UNCHANGED'}
 if(!$text.Contains('casualty_policy=CAPTURED_ORIGINAL_SURVIVORS') -or !$text.Contains('"MEMBER_ARRIVAL"')){'EXPLICIT_FRESH_RECEIPTS'}
}
if(@(Rules $source).Count){throw "Current survivor fixture rejected: $(Rules $source)"}
$mutants=@(
 @('captured identity','actor.GetID() == m_aVehicleSoakMembers[position]','true'),
 @('replacement','current && current != actor','false'),
 @('group generation','slot.GetSpawnGeneration() != m_iVehicleSoakGeneration','false'),
 @('current trip','trip != m_VehicleCoordinator.GetTrip(slot)','false'),
 @('foreign extra group actor','CountAliveAgents(slot.GetGroup()) == aliveOriginals','true'),
 @('no fit survivor','fitCount > 0','true'),
 @('nonfit actor omitted','if (inside) return false;','if (inside) Print("inside");'),
 @('incap compartment','access.IsInCompartment()','false'),
 @('initial roster','CountAliveAgents(slot.GetGroup()) != 5','false'),
 @('unannounced policy','casualty_policy=CAPTURED_ORIGINAL_SURVIVORS','casualty_policy=ANY')
)
foreach($mutant in $mutants){if(@(Rules ($source.Replace($mutant[1],$mutant[2]))).Count -eq 0){throw "Escaped survivor source control: $($mutant[0])"}}
$body=Body $source 'AcceptOriginal'
$type='AICFSoakOriginal'+[guid]::NewGuid().ToString('N')
Add-Type "public static class $type { public static bool Check(bool capturedExact,bool alive,bool currentExact,bool foreignCurrent) {$body} }"
$method=([type]$type).GetMethod('Check')
for($mask=0;$mask -lt 16;$mask++){
 $values=@();for($bit=0;$bit -lt 4;$bit++){$values+=[bool]($mask -band (1 -shl $bit))}
 $expected=$values[0] -and !$values[3] -and (!$values[1] -or $values[2])
 if($method.Invoke($null,$values) -ne $expected){throw "Actual extracted identity predicate failed mask=$mask"}
}
'PASS VehicleSoak natural casualty: unchanged five startup, all captured bodies physically detached, all actual fit originals at destination; 10 source mutants +16 executed production predicate controls'

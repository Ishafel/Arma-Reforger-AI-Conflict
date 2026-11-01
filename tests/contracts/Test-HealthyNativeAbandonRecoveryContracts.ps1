param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$path=Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Vehicles/AICF_VehicleTransitFlow.c'
$source=[IO.File]::ReadAllText($path)
function Body($text,$name){Get-AICFMethodBody ([pscustomobject]@{Source=$text;Code=(ConvertTo-AICFCodeText $text)}) $name}
function Rules($text){
 $read=Body $text 'ReadHealthyNativeAbandonScope';$recover=Body $text 'RecoverHealthyNativeAbandon';$tick=Body $text 'Tick'
 foreach($guard in @('AICF_TransportNativeAbandon.Observe(trip, observed)',
  'AICF_VehiclePresetNativeSeatBinding.Capture(trip)','!binding.IsCurrent()',
  'usage.GetDamageState() == EDamageState.UNDAMAGED','damage.GetDefaultHitZone().GetHealthScaled() == 1.0',
  'damage.GetMovementDamage() == 0','SCR_AIVehicleUsability.VehicleCanMove(vehicle)',
  '!SCR_AIVehicleUsability.VehicleIsOnFire(vehicle)',
  'binding.m_aMembers.Count() == agents.Count()','agents.Count() == trip.GetVehiclePresetSlot().GetDesiredSize()',
  'm_Watchdog.AreAllAliveMembersSettledInVehicle(binding.m_Group, vehicle)',
  'AICF_VehicleBehaviorPolicy.HasLocalCombat(trip)','trip.IsDeadlineReached(System.GetTickCount())',
  'SCR_DamageManagerComponent.GetDamageManager(vehicle) == damage',
  'SCR_AIVehicleUsageComponent.Cast(vehicle.FindComponent(SCR_AIVehicleUsageComponent)) == usage')){
  if(!$read.Contains($guard)){'CURRENT_FULL_HEALTH_ORIGINAL_SETTLED_SCOPE';break}
 }
 if(([regex]::Matches($recover,'ReadHealthyNativeAbandonScope\(trip, eligibility\)')).Count -ne 2 -or
  !$recover.Contains('utility.GetCurrentAction() != action') -or !$recover.Contains('utility.GetExecutedAction() != action') -or
  !$recover.Contains('!state.BeginMobilityRecovery()') -or $recover.IndexOf('!state.BeginMobilityRecovery()') -gt $recover.IndexOf('action.Fail();') -or
  $recover.IndexOf('action.Fail();') -gt $recover.IndexOf('ValidateContext(trip, causationId, true, invalidReason)') -or
  !$recover.Contains('!binding.IsCurrent()') -or !$recover.Contains('trip.GetAssignment() != assignment') -or
  !$recover.Contains('trip.GetMovementState() != state') -or !$recover.Contains('state.GetRouteGeneration() != routeGeneration') -or
  !$recover.Contains('BeginMobilityRecovery(trip, causationId, "HEALTHY_NATIVE_ABANDON", false)')){'REPEATED_SCOPE_COUNTED_CANCEL_RECHECK_ROUTE_ONLY'}
 if($tick.IndexOf('RecoverHealthyNativeAbandon(trip, causationId)') -lt $tick.IndexOf('AICF_TransportNativeAbandon.Observe(trip, nativeAbandonDetails)') -or
  $tick.IndexOf('RecoverHealthyNativeAbandon(trip, causationId)') -gt $tick.IndexOf('"TRANSPORT_NATIVE_GROUP_ABANDONED"') -or
  !$tick.Contains('FallbackToFoot("NATIVE_GROUP_ABANDONED_VEHICLE", causationId)')){'REPAIR_BEFORE_BOUNDED_FALLBACK'}
 if((ConvertTo-AICFCodeText $recover) -match '\b(?:SetHealth|Heal|SetOrigin|SetWorldTransform|CommitTransition|CreditNativeRouteProgress|ObservePhysicalMotion|Reset|CallLater)\s*\('){'NO_HEALTH_POSE_PHASE_CLOCK_BYPASS'}
}
$failures=@(Rules $source)
if($failures.Count){throw "Healthy native abandon recovery: $failures"}
$body=Body $source 'CanRecoverNativeAbandon'
$adapter=@'
public static class AICF_HealthyAbandonActual {
 public static bool Read(bool healthy,bool fullRoster,bool settled,bool localCombat,int attempts,int maximumAttempts,bool expired) BODY
}
'@
if(!('AICF_HealthyAbandonActual' -as [type])){Add-Type -TypeDefinition $adapter.Replace('BODY',$body)}
$checks=0
foreach($bits in 0..31){
 $healthy=($bits -band 1)-ne 0;$full=($bits -band 2)-ne 0;$settled=($bits -band 4)-ne 0
 $combat=($bits -band 8)-ne 0;$expired=($bits -band 16)-ne 0
 foreach($budget in @(@(-1,2),@(0,0),@(0,2),@(1,2),@(2,2),@(3,2))){
  $actual=[AICF_HealthyAbandonActual]::Read($healthy,$full,$settled,$combat,$budget[0],$budget[1],$expired)
  $expected=$bits -eq 7 -and $budget[0] -in 0,1 -and $budget[1] -eq 2
  if($actual -ne $expected){throw "Actual admission mismatch bits=$bits budget=$budget"};$checks++
 }
}
$mutants=0
foreach($mutation in @(
 @('damage.GetDefaultHitZone().GetHealthScaled() == 1.0','true','CURRENT_FULL_HEALTH_ORIGINAL_SETTLED_SCOPE'),
 @('agents.Count() == trip.GetVehiclePresetSlot().GetDesiredSize()','true','CURRENT_FULL_HEALTH_ORIGINAL_SETTLED_SCOPE'),
 @('!state.BeginMobilityRecovery()','false','REPEATED_SCOPE_COUNTED_CANCEL_RECHECK_ROUTE_ONLY'),
 @('utility.GetExecutedAction() != action','false','REPEATED_SCOPE_COUNTED_CANCEL_RECHECK_ROUTE_ONLY'),
 @('trip.GetAssignment() != assignment','false','REPEATED_SCOPE_COUNTED_CANCEL_RECHECK_ROUTE_ONLY'),
 @('BeginMobilityRecovery(trip, causationId, "HEALTHY_NATIVE_ABANDON", false)','BeginMobilityRecovery(trip, causationId, "HEALTHY_NATIVE_ABANDON", true)','REPEATED_SCOPE_COUNTED_CANCEL_RECHECK_ROUTE_ONLY')
)){
 if(!$source.Contains($mutation[0])){throw "Mutation target absent: $($mutation[0])"}
 if($mutation[2] -notin @(Rules ($source.Replace($mutation[0],$mutation[1])))){throw "Mutation survived: $($mutation[2])"};$mutants++
}
"PASS Healthy native abandon recovery: actual admission $checks cases, $mutants unsafe source mutations rejected; full-health original settled crew, exact native action, counted route-only repair, unchanged absolute deadline and bounded fallback. Native cancellation/motion remain separate gates."

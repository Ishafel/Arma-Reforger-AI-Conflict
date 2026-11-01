param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$source=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'tests/fixtures/AICF_VehicleMultiSquadProbe.c'))
function Body([string]$text,[string]$name){Get-AICFMethodBody ([pscustomobject]@{Source=$text;Code=(ConvertTo-AICFCodeText $text)}) $name}
function Rules([string]$text){
 $update=Body $text 'Update';$start=Body $text 'Start';$init=Body $text 'AICF_MultiInitialize';$tick=Body $text 'AICF_MultiTick';$sample=Body $text 'AICF_MultiSample'
 $fund=Body $text 'AICF_MultiFund';$funding=Body $text 'AICF_MultiFunding';$protect=Body $text 'AICF_MultiProtect';$environment=Body $text 'AICF_MultiEnvironment';$intent=Body $text 'AICF_MultiCurrentFinalIntent';$abort=Body $text 'AICF_MultiAbort';$exact=Body $text 'AICF_MultiExact'
 if(!$update -or !$init -or !$tick -or !$fund -or !$sample -or !$intent -or !$abort){'MISSING_METHOD';return}
 if($text -match 'new AICF_FactionState|SpawnGroup\(|MarkReady\(|SetOrigin\(|Teleport\(|SetWorldTransform\(|CommitTransition\('){'NO_FAKE_DEPLOYMENT_OR_MOVEMENT'}
 if($start.IndexOf('AICF_MultiEnvironment()') -gt $start.IndexOf('super.Start(') -or $environment -notmatch 'm_sAICFMultiMode == "CAPACITY"' -or $environment -notmatch 'SetFactionsFriendly' -or $protect -notmatch 'm_sAICFMultiMode != "CAPACITY"' -or $funding -notmatch 'if \(m_sAICFMultiMode == "CAPACITY"\)'){'EXPLICIT_EARLY_CAPACITY_ONLY_ENVIRONMENT'}
 if($fund -notmatch 'mode != "CAPACITY"' -or $fund -notmatch '4000 - AICF_MultiGranted\(key\)' -or $fund -notmatch 'm_aAICFMultiGrantTokens.Contains\(token\)' -or $fund -notmatch 'IsOperationalOwnedBase\(base, faction\)' -or $fund -notmatch 'Math.Min\(quotedDemand, base.GetSuppliesMax\(\)\)' -or $fund -notmatch 'base.AddSupplies\(requested\)' -or $fund -notmatch 'owner=ECONOMY_SYSTEM test_only=1' -or @([regex]::Matches($text,'\.AddSupplies\(')).Count -ne 1){'FINITE_ECONOMY_OWNER_LEDGER'}
 $jit=Body $text 'AICF_MultiJitVehicleQuote';$quote=Body $text 'AICF_MultiOwnVehicleQuote';$purchase=Body $text 'PurchaseVehiclePreset';$preflight=Body $text 'InspectVehiclePresetPreflight'
 foreach($guard in @('mode != "CAPACITY"','!Replication.IsServer()','!m_Campaign.IsMaster()',
  '!m_Campaign.IsRunning()','!m_bGraphContextReady','base != faction.GetMainBase()',
  '!m_SupplyNetwork.IsOperationalOwnedBase(base, faction)','!m_ConflictAdapter.GetSpawnRejectionReason(base, faction).IsEmpty()',
  'controller.AICF_MultiOwnVehicleQuote(trip, faction, base, prefab, cost, started)',
  'LogisticsObligations(base)','cost + Math.Ceil(Math.Max(0, obligations))','AICF_MultiFund(faction, required, token, started)')){
  if(!$jit.Contains($guard)){'JIT_DEFICIT_ONLY_CURRENT_ECONOMY_OWNER'}
 }
 foreach($guard in @('m_sAICFMultiMode != "CAPACITY"','trip.GetSlotId() < 0 || trip.GetSlotId() >= 6',
  'trip.GetPhase() != AICF_ETransportTripPhase.SPAWN_COMMIT','trip.GetFactionKey() != faction.GetFactionKey()',
  'trip.IsDeadlineReached(System.GetTickCount())','!trip.IsVehiclePresetCurrent()','trip.IsVehiclePresetDebited()',
  'AICF_VehiclePreset preset = trip.GetVehiclePreset()','preset.m_sPrefab != prefab','lease && !lease.MatchesTripIdentity(',
  '!reservation.MatchesTrip(trip, request.GetRequestGeneration())','reservation.IsExpired(System.GetTickCount())',
  'AICF_VehicleSpawnSiteSelection selection = plan.GetSelection()','AICF_VehicleSpawnSiteReservation reservation = plan.GetReservation()',
  'selection.m_Base != base','reservation.GetBase() != base',
  'item.m_iPhase != 2','!item.m_bRecruitAccepted','item.m_aMembers.Count() != 5','item.m_Recipe.m_sPrefab != prefab',
  '!AICF_MultiRoster(item, slot)','trip.GetVehiclePresetSlot() != slot','m_VehicleCoordinator.GetTrip(slot) != trip',
  'trip.GetAssignment().GetGroup() != slot.GetGroup()','trip.GetGroupGeneration() != item.m_iGeneration','cost = catalog.Cost(prefab)')){
  if(!$quote.Contains($guard)){'JIT_SELECTED_REAL_SIX_RECIPE_TRIP_IDENTITY'}
 }
 if($jit -notmatch 'trip.GetRequestState\(\).GetRequestGeneration\(\), trip.GetRequestState\(\).GetAttemptCount\(\), stage' -or
  $jit -notmatch 'native_debit=PENDING' -or ([regex]::Matches($purchase,'super.PurchaseVehiclePreset\(')).Count -ne 1 -or
  $purchase -notmatch 'return super.PurchaseVehiclePreset\(trip, faction, base, prefab, paid\)' -or
  $purchase.IndexOf('AICF_MultiJitVehicleQuote(') -gt $purchase.IndexOf('super.PurchaseVehiclePreset(') -or
  ([regex]::Matches($preflight,'super.InspectVehiclePresetPreflight\(')).Count -ne 1 -or
  $preflight -notmatch 'return super.InspectVehiclePresetPreflight\(trip, faction, base, fleet\);' -or
  $preflight -notmatch '"PREFLIGHT", fleet, frame' -or
  $preflight.IndexOf('AICF_MultiJitVehicleQuote(') -gt $preflight.IndexOf('super.InspectVehiclePresetPreflight(') -or
  $text -match 'super.SpawnCandidate\(' -or $purchase -match 'return true|paid\s*='){'JIT_BEFORE_PREFLIGHT_AND_NATIVE_DEBIT_NO_SUCCESS_BYPASS'}
 if($init -notmatch 'slot.GetUnitType\(\) == AICF_EGroupUnitType.INFANTRY' -or $init -notmatch 'deploymentSize != 1' -or $init -notmatch 'AICF_EGroupUnitType.INFANTRY, 1' -or $tick -notmatch 'AICF_EGroupUnitType.INFANTRY, 5' -or $tick -notmatch 'RequestPlayerRecruitment\(slot, faction, reason\)' -or $sample -notmatch 'mustered == m_iAICFMultiCohort \* 2'){'REAL_INFANTRY_PAID_MUSTER_BEFORE_MOTORIZED'}
 $apply=Body $text 'AICF_MultiApply'
 if($tick -match 'm_bAICFMultiMustered\s*&&' -or $funding.Contains('m_bAICFMultiMustered') -or
  $tick -notmatch '(?s)item.m_iPhase = 1;\s*item.m_iNextCommand = now;' -or
  $tick -notmatch 'if \(item.m_iSlot < m_iAICFMultiCohort && now >= item.m_iNextCommand\)' -or
  $apply -notmatch 'item.m_iPhase != 1 \|\| !item.m_bRecruitAccepted \|\| item.m_aMembers.Count\(\) != 5 \|\| !AICF_MultiRoster\(item, slot\)' -or
  $funding -notmatch 'if \(item.m_iSlot >= m_iAICFMultiCohort\) continue;' -or
  $funding -notmatch 'item.m_iPhase >= 1 && item.m_iPhase <= 2' -or $funding -notmatch 'item.m_bRecruitAccepted && item.m_aMembers.Count\(\) == 5'){'PER_GROUP_PAID_MUSTER_WITHOUT_GLOBAL_IDLE_BARRIER'}
 if($text -notmatch 'muster_policy=PER_GROUP_BEFORE_VEHICLE'){'EXPLICIT_PER_GROUP_MUSTER_POLICY'}
 if($tick -notmatch 'if \(item.m_iSlot >= m_iAICFMultiCohort\) return;' -or $tick.IndexOf('item.m_iSlot >= m_iAICFMultiCohort') -gt $tick.IndexOf('RequestPlayerRecruitment') -or $sample -notmatch 'm_iAICFMultiCohort \+= 2;\s*m_bAICFMultiMustered = false;'){'STAGED_MUSTER_RELEASES_BARRACKS'}
 if($exact -notmatch 'lease.GetState\(\) != AICF_EVehicleLeaseState.ACTIVE' -or $exact -notmatch 'MatchesTripIdentity\(item.m_sFaction, item.m_iSlot, item.m_iGeneration, trip.GetTripGeneration\(\)\)' -or $exact -notmatch 'MatchesEntityIdentity' -or $exact -notmatch 'rpl.Id\(\).ToString\(\) != lease.GetRplId\(\)' -or $exact -notmatch 'seats\[index\].GetOccupant\(\) != actor' -or $exact -notmatch 'IsGettingIn\(\) \|\| access.IsGettingOut\(\) \|\| access.IsSwitchingSeatsAnim\(\)'){'REAL_CURRENT_LEASE_EXACT_OCCUPANTS'}
 if($sample -notmatch 'item.m_iLeg > 0 && item.m_fDisplacement >= 25 && item.m_iMoving > 0' -or $sample -notmatch 'vehicles.Contains\(vehicle\) \|\| rplIds.Contains\(item.m_sRpl\)' -or $sample -notmatch 'usExact >= m_iAICFMultiCohort && ussrExact >= m_iAICFMultiCohort' -or $sample -notmatch 'now - m_iAICFMultiStableAt >= 30000' -or $sample -notmatch 'else m_iAICFMultiStableAt = -1'){'SIMULTANEOUS_MOVING_COHORTS_STABLE_30S'}
 $mounted=Body $text 'AICF_MultiMountedWait'
 if($text -notmatch 'if \(!System.GetCLIParam\("aicfVehicleMultiConcurrencyMode", value\)\) return "MOVING";' -or $init -notmatch 'm_sAICFMultiConcurrency == "MOUNTED_STAGING" && m_sAICFMultiMode != "CAPACITY"' -or $text -notmatch 'classification = "CAPACITY_MOUNTED_ONLY"' -or $text -notmatch 'concurrency_mode=%1 classification=%2 notmovingproof=%3'){'SEPARATE_EXPLICIT_MOUNTED_ORACLE'}
 foreach($guard in @('item.m_iLeg == 0','!trip.IsDeadlineReached(System.GetTickCount())','trip.IsPresetWaitingForDestination()',
  'trip.GetMovementState().GetRouteGeneration() == 0','!trip.GetMovementState().GetRouteWaypoint()',
  '!trip.GetMovementState().GetSupersededRouteWaypoint()')){if(!$mounted.Contains($guard)){'NATIVE_INITIAL_MOUNTED_WAIT_PROOF'}}
 if($sample -notmatch 'if \(m_sAICFMultiConcurrency == "MOUNTED_STAGING"\) concurrencyProof = AICF_MultiMountedWait\(item, trip\);' -or $sample -notmatch 'item.m_iPhase == 3 && concurrencyProof && AICF_MultiExact\(item, slot, trip\)' -or $tick -notmatch '(?s)if \(m_sAICFMultiConcurrency == "MOUNTED_STAGING" && m_iAICFMultiCohort <= 6\)\s*\{\s*if \(item.m_iLeg != 0\) AICF_MultiFail\(item, "MOUNTED_STAGING_EARLY_TARGET"\);\s*return;' -or $tick.IndexOf('MOUNTED_STAGING_EARLY_TARGET') -gt $tick.IndexOf('if (!AICF_MultiOrder(')){'MOUNTED_NO_FAR_ORDER_BEFORE_SIX'}
 if((Body $text 'AICF_MultiLog') -notmatch 'AICF_MultiConcurrency.Metadata\(m_sAICFMultiMode\)' -or $fund -notmatch 'Print\(details \+ AICF_MultiConcurrency.Metadata\(mode\)\)' -or $environment -notmatch 'AICF_MultiConcurrency.Metadata\(m_sAICFMultiMode\)'){'ALL_MARKERS_LABEL_CONCURRENCY'}
 if($sample -notmatch 'now = System.GetTickCount\(\);\s*m_iAICFMultiSnapshotAt = now;' -or $sample -notmatch 'm_iAICFMultiSnapshotAt = -1;' -or (Body $text 'AICF_MultiLog') -notmatch 'if \(m_iAICFMultiSnapshotAt >= 0\) now = m_iAICFMultiSnapshotAt;'){'CAPTURED_NATIVE_SAMPLE_TICK'}
 if($intent -notmatch 'slot.HasPlayerStrategicIntent\(\)' -or $intent -notmatch 'PLAYER_COMMAND' -or $intent -notmatch 'slot.GetPlayerStrategicIntentTargetKind\(\) == AICF_EOrderTargetKind.POSITION' -or $intent -notmatch 'slot.GetTargetKind\(\) == AICF_EOrderTargetKind.POSITION' -or $intent -notmatch 'slot.GetStrategicIntentRevision\(\) == item.m_iRevision' -or $intent -notmatch 'slot.GetPlayerStrategicIntentTargetPosition\(\) == item.m_vGoal' -or $intent -notmatch 'slot.GetTargetPosition\(\) == item.m_vGoal' -or $tick -notmatch 'item.m_iPhase >= 4 && !AICF_MultiCurrentFinalIntent\(item, slot\)' -or $tick.IndexOf('!AICF_MultiCurrentFinalIntent') -gt $tick.IndexOf('"ARRIVAL"')){'LIVE_FINAL_PLAYER_POINT_INTENT'}
 if($tick -notmatch '"FOOT_ARRIVAL"[^;]*slot.GetTargetPosition\(\), slot.GetStrategicIntentRevision\(\)' -or $tick -notmatch '"DISMOUNTED"[^;]*slot.GetStrategicIntentRevision\(\), slot.GetTargetPosition\(\)'){'ACTUAL_FINAL_READBACK_NOT_CACHED'}
 $retained=Body $text 'AICF_MultiRetainedFoot'
 if($retained -notmatch '!AICF_MultiRoster\(item, slot\) \|\| !AICF_MultiCurrentFinalIntent\(item, slot\)' -or
  $retained -notmatch 'trip.GetPhase\(\) != AICF_ETransportTripPhase.TACTICAL_DEPLOYMENT' -or
  $retained -notmatch '!trip.GetHandoffState\(\).IsOrderRestored\(\)' -or
  $retained -notmatch 'vehicle.GetID\(\) != item.m_VehicleId' -or $retained -notmatch 'lease.GetLeaseGeneration\(\) != item.m_iLeaseGeneration' -or
  $retained -notmatch 'lease.MatchesEntityIdentity\(vehicle, vehicle.GetID\(\), AICF_VehicleBehaviorBinding.ComponentRpl\(vehicle\)\)' -or
  $retained -notmatch 'watchdog.InspectProtectedMemberDismountClearance\(' -or
  $retained -notmatch 'actor.GetID\(\) != item.m_aMembers\[position\]' -or
  $retained -notmatch 'if \(linked \|\| compartment \|\| gettingIn \|\| gettingOut \|\| characterVehicle \|\| inside\) return false' -or
  $retained -notmatch '"RETAINED_MEMBER"' -or $retained -notmatch '"RETAINED_CLEARANCE"' -or
  $tick -notmatch 'normal_complete=0 retained_manual=1 phase=TACTICAL_DEPLOYMENT order_restored=1' -or
  $tick -notmatch 'item.m_iPhase == 5 && AICF_MultiRetainedFoot\(item, slot, trip\)'){'TRUTHFUL_RETAINED_MANUAL_FOOT_ENDPOINT'}
 $temporary=Body $text 'AICF_MultiTemporaryDeployment'
 if($temporary -notmatch '!AICF_MultiRoster\(item, slot\)' -or $temporary -notmatch 'trip.GetOperationId\(\) != item.m_sOperation' -or
  $temporary -notmatch 'trip.GetPhase\(\) != AICF_ETransportTripPhase.TACTICAL_DEPLOYMENT' -or
  $temporary -notmatch 'vehicle.GetID\(\) != item.m_VehicleId' -or $temporary -notmatch 'lease.GetRplId\(\) != item.m_sRpl' -or
  $temporary -notmatch 'lease.GetLeaseGeneration\(\) != item.m_iLeaseGeneration' -or
  $temporary -notmatch 'trip.GetAssignment\(\).GetStrategicIntentRevision\(\) != slot.GetStrategicIntentRevision\(\)' -or
  $tick -notmatch 'if \(AICF_MultiTemporaryDeployment\(item, slot, trip\)\) return;' -or
  $tick.IndexOf('if (AICF_MultiTemporaryDeployment') -gt $tick.IndexOf('TRIP_ENDED_BEFORE_CAPACITY_COHORTS') -or
  $sample -notmatch 'item.m_iPhase == 3 && concurrencyProof && AICF_MultiExact\(item, slot, trip\)' -or
  $exact -notmatch 'trip.GetPhase\(\) != AICF_ETransportTripPhase.TRANSIT'){'TACTICAL_WAIT_IS_NOT_MOVING_CREDIT'}
 if($retained -notmatch 'GetLastTransitionReason\(\) == "LOCAL_THREAT" && vector.DistanceXZ\(actor.GetOrigin\(\), item.m_vGoal\) > 35' -or
  $tick -notmatch 'item.m_bRetainedFoot && !AICF_MultiRetainedFoot\(item, slot, trip, false\)' -or
  $tick -notmatch 'retained_manual=1 in_vehicle=0 phase=TACTICAL_DEPLOYMENT order_restored=1'){'LIVE_FOOT_CLEARANCE_AFTER_RETAINED_DISEMBARK'}
 $stopIndex=$update.IndexOf('m_bStopped || !m_Campaign')
 if($stopIndex -lt 0 -or $stopIndex -gt $update.IndexOf('AICF_MultiInitialize()') -or $stopIndex -gt $update.IndexOf('now - m_iAICFMultiStart >= m_iAICFMultiDuration')){'STOP_BEFORE_INITIALIZATION_AND_DONE'}
 if($update -notmatch 'if \(!m_Campaign.IsRunning\(\)\)\s*\{\s*if \(m_bCampaignWasRunning \|\| m_bAICFMultiStarted\)\s*AICF_MultiAbort\("MATCH_STOPPED_BEFORE_DURATION"\);\s*return;' -or $update.IndexOf('now - m_iAICFMultiSetupStarted >= 180000') -gt $update.IndexOf('if (!m_Campaign.IsRunning())')){'NATIVE_STARTING_WAIT_WITH_SETUP_DEADLINE'}
 if($update -notmatch 'now - m_iAICFMultiSetupStarted >= 180000' -or $update -notmatch 'AICF_MultiAbort\("SETUP_TIMEOUT"\)' -or $abort -notmatch '"ABORTED"' -or $abort -notmatch 'duration_satisfied=0' -or $abort -notmatch 'if \(!m_bStopped\) Stop\(true\)' -or $abort -notmatch 'RequestClose\(\)'){'BOUNDED_SETUP_ABORT_OWNS_CLEANUP'}
 if($update -notmatch 'now - m_iAICFMultiStart >= m_iAICFMultiDuration' -or $update -notmatch '"DONE"' -or $update -notmatch 'duration_satisfied=1' -or (Body $text 'AICF_MultiFail') -match 'RequestClose|Stop\('){'FAILURE_OBSERVATION_FULL_DURATION'}
}
$failures=@(Rules $source);if($failures.Count){throw "Multi-squad fixture: $failures"}
$controls=1
$runningGuard=[regex]::Match((Body $source 'Update'),'if \((m_bCampaignWasRunning \|\| m_bAICFMultiStarted)\)').Groups[1].Value
$runningGuard=$runningGuard.Replace('m_bCampaignWasRunning','$wasRunning').Replace('m_bAICFMultiStarted','$started').Replace('||','-or')
$evaluate=[scriptblock]::Create('param($wasRunning,$started) '+$runningGuard)
foreach($case in @(@($false,$false,$false),@($true,$false,$true),@($false,$true,$true),@($true,$true,$true))){if([bool](& $evaluate $case[0] $case[1]) -ne $case[2]){throw 'Native STARTING/after-RUNNING stop truth table mismatch'};$controls++}
foreach($mutation in @(
 @('!trip.GetHandoffState().IsOrderRestored()','false','TRUTHFUL_RETAINED_MANUAL_FOOT_ENDPOINT'),
 @('actor.GetID() != item.m_aMembers[position]','false','TRUTHFUL_RETAINED_MANUAL_FOOT_ENDPOINT'),
 @('linked || compartment || gettingIn || gettingOut || characterVehicle || inside','false','TRUTHFUL_RETAINED_MANUAL_FOOT_ENDPOINT'),
 @('normal_complete=0 retained_manual=1 phase=TACTICAL_DEPLOYMENT order_restored=1','normal_complete=1','TRUTHFUL_RETAINED_MANUAL_FOOT_ENDPOINT'),
 @('if (AICF_MultiTemporaryDeployment(item, slot, trip)) return;','','TACTICAL_WAIT_IS_NOT_MOVING_CREDIT'),
 @('trip.GetAssignment().GetStrategicIntentRevision() != slot.GetStrategicIntentRevision()','false','TACTICAL_WAIT_IS_NOT_MOVING_CREDIT'),
 @('item.m_bRetainedFoot && !AICF_MultiRetainedFoot(item, slot, trip, false)','false','LIVE_FOOT_CLEARANCE_AFTER_RETAINED_DISEMBARK'),
 @('m_sAICFMultiMode != "CAPACITY"','false','EXPLICIT_EARLY_CAPACITY_ONLY_ENVIRONMENT'),
 @('mode != "CAPACITY"','false','FINITE_ECONOMY_OWNER_LEDGER'),
 @('4000 - AICF_MultiGranted(key)','4000000','FINITE_ECONOMY_OWNER_LEDGER'),
 @('deploymentSize != 1','false','REAL_INFANTRY_PAID_MUSTER_BEFORE_MOTORIZED'),
 @('RequestPlayerRecruitment(slot, faction, reason)','FakeRecruit(slot, faction, reason)','REAL_INFANTRY_PAID_MUSTER_BEFORE_MOTORIZED'),
 @('if (item.m_iSlot < m_iAICFMultiCohort && now >= item.m_iNextCommand)','if (m_bAICFMultiMustered && item.m_iSlot < m_iAICFMultiCohort && now >= item.m_iNextCommand)','PER_GROUP_PAID_MUSTER_WITHOUT_GLOBAL_IDLE_BARRIER'),
 @('item.m_iNextCommand = now;','item.m_iNextCommand = now + 90000;','PER_GROUP_PAID_MUSTER_WITHOUT_GLOBAL_IDLE_BARRIER'),
 @('item.m_iPhase != 1 || !item.m_bRecruitAccepted || item.m_aMembers.Count() != 5 || !AICF_MultiRoster(item, slot)','false','PER_GROUP_PAID_MUSTER_WITHOUT_GLOBAL_IDLE_BARRIER'),
 @('muster_policy=PER_GROUP_BEFORE_VEHICLE','muster_policy=GLOBAL_BARRIER','EXPLICIT_PER_GROUP_MUSTER_POLICY'),
 @('if (item.m_iPhase >= 1 && item.m_iPhase <= 2 && item.m_bRecruitAccepted && item.m_aMembers.Count() == 5)','if (m_bAICFMultiMustered && item.m_iPhase >= 1 && item.m_iPhase <= 2 && item.m_bRecruitAccepted && item.m_aMembers.Count() == 5)','PER_GROUP_PAID_MUSTER_WITHOUT_GLOBAL_IDLE_BARRIER'),
 @('if (item.m_iSlot >= m_iAICFMultiCohort) continue;','if (false) continue;','PER_GROUP_PAID_MUSTER_WITHOUT_GLOBAL_IDLE_BARRIER'),
 @('item.m_iSlot >= m_iAICFMultiCohort','false','STAGED_MUSTER_RELEASES_BARRACKS'),
 @('mustered == m_iAICFMultiCohort * 2','mustered == 12','REAL_INFANTRY_PAID_MUSTER_BEFORE_MOTORIZED'),
 @('m_iAICFMultiSnapshotAt = now;','m_iAICFMultiSnapshotAt = 0;','CAPTURED_NATIVE_SAMPLE_TICK'),
 @('lease.GetState() != AICF_EVehicleLeaseState.ACTIVE','false','REAL_CURRENT_LEASE_EXACT_OCCUPANTS'),
 @('seats[index].GetOccupant() != actor','false','REAL_CURRENT_LEASE_EXACT_OCCUPANTS'),
 @('item.m_fDisplacement >= 25','true','SIMULTANEOUS_MOVING_COHORTS_STABLE_30S'),
 @('now - m_iAICFMultiStableAt >= 30000','true','SIMULTANEOUS_MOVING_COHORTS_STABLE_30S'),
 @('slot.GetStrategicIntentRevision() == item.m_iRevision','true','LIVE_FINAL_PLAYER_POINT_INTENT'),
 @('slot.GetPlayerStrategicIntentTargetPosition() == item.m_vGoal','true','LIVE_FINAL_PLAYER_POINT_INTENT'),
 @('slot.GetTargetPosition() == item.m_vGoal','true','LIVE_FINAL_PLAYER_POINT_INTENT'),
 @('item.m_iPhase >= 4 && !AICF_MultiCurrentFinalIntent(item, slot)','false','LIVE_FINAL_PLAYER_POINT_INTENT'),
 @('now - m_iAICFMultiSetupStarted >= 180000','false','BOUNDED_SETUP_ABORT_OWNS_CLEANUP'),
 @('duration_satisfied=0','duration_satisfied=1','BOUNDED_SETUP_ABORT_OWNS_CLEANUP'),
 @('m_bCampaignWasRunning || m_bAICFMultiStarted','true','NATIVE_STARTING_WAIT_WITH_SETUP_DEADLINE')
 ,@('trip.IsPresetWaitingForDestination()','true','NATIVE_INITIAL_MOUNTED_WAIT_PROOF')
 ,@('trip.GetMovementState().GetRouteGeneration() == 0','true','NATIVE_INITIAL_MOUNTED_WAIT_PROOF')
 ,@('m_sAICFMultiConcurrency == "MOUNTED_STAGING" && m_iAICFMultiCohort <= 6','false','MOUNTED_NO_FAR_ORDER_BEFORE_SIX')
 ,@('if (!System.GetCLIParam("aicfVehicleMultiConcurrencyMode", value)) return "MOVING";','return "MOUNTED_STAGING";','SEPARATE_EXPLICIT_MOUNTED_ORACLE')
 ,@('trip.GetSlotId() < 0 || trip.GetSlotId() >= 6','false','JIT_SELECTED_REAL_SIX_RECIPE_TRIP_IDENTITY')
 ,@('trip.GetPhase() != AICF_ETransportTripPhase.SPAWN_COMMIT','false','JIT_SELECTED_REAL_SIX_RECIPE_TRIP_IDENTITY')
 ,@('trip.GetGroupGeneration() != item.m_iGeneration','false','JIT_SELECTED_REAL_SIX_RECIPE_TRIP_IDENTITY')
 ,@('m_VehicleCoordinator.GetTrip(slot) != trip','false','JIT_SELECTED_REAL_SIX_RECIPE_TRIP_IDENTITY')
 ,@('item.m_Recipe.m_sPrefab != prefab','false','JIT_SELECTED_REAL_SIX_RECIPE_TRIP_IDENTITY')
 ,@('!item.m_bRecruitAccepted','false','JIT_SELECTED_REAL_SIX_RECIPE_TRIP_IDENTITY')
 ,@('cost = catalog.Cost(prefab)','cost = 1','JIT_SELECTED_REAL_SIX_RECIPE_TRIP_IDENTITY')
 ,@('!reservation.MatchesTrip(trip, request.GetRequestGeneration())','false','JIT_SELECTED_REAL_SIX_RECIPE_TRIP_IDENTITY')
 ,@('reservation.IsExpired(System.GetTickCount())','false','JIT_SELECTED_REAL_SIX_RECIPE_TRIP_IDENTITY')
 ,@('cost + Math.Ceil(Math.Max(0, obligations))','4000','JIT_DEFICIT_ONLY_CURRENT_ECONOMY_OWNER')
 ,@('return super.PurchaseVehiclePreset(trip, faction, base, prefab, paid);','paid = 475; return true;','JIT_BEFORE_PREFLIGHT_AND_NATIVE_DEBIT_NO_SUCCESS_BYPASS')
 ,@('return super.InspectVehiclePresetPreflight(trip, faction, base, fleet);','return null;','JIT_BEFORE_PREFLIGHT_AND_NATIVE_DEBIT_NO_SUCCESS_BYPASS')
)){
 if(!$source.Contains($mutation[0])){throw "Missing mutation target $($mutation[2])"}
 if($mutation[2] -notin @(Rules ($source.Replace($mutation[0],$mutation[1])))){throw "Mutation survived $($mutation[2])"}
 $controls++
}
$update=Body $source 'Update'
$stopFirst=$update.Replace('if (m_bStopped || !m_Campaign)','if (false)')
if('STOP_BEFORE_INITIALIZATION_AND_DONE' -notin @(Rules ($source.Replace($update,$stopFirst)))){throw 'Stop-at-deadline mutation survived'}
$controls++
$expression=[regex]::Match((Body $source 'AICF_MultiFund'),'int requested = (Math.Min\(remaining,.*?\));').Groups[1].Value
$expression=$expression.Replace('Math.Min','[Math]::Min').Replace('Math.Max','[Math]::Max').Replace('base.GetSuppliesMax()','$capacity').Replace('quotedDemand','$demand').Replace('remaining','$remaining').Replace('before','$before')
$fundCheck=[scriptblock]::Create('param($before,$demand,$capacity,$remaining) '+$expression)
foreach($case in @(@(465,475,1200,3775,10),@(475,475,1200,3765,0),@(455,475,1200,3765,20),@(0,475,1200,10,10),@(465,475,1200,0,0),@(390,475,400,3775,10),@(465,501,1200,3775,36))){
 if([int](& $fundCheck $case[0] $case[1] $case[2] $case[3]) -ne $case[4]){throw 'JIT deficit/budget/capacity formula failed'}
 $controls++
}
$leaseGuard=[regex]::Match((Body $source 'AICF_MultiOwnVehicleQuote'),'lease && !lease.MatchesTripIdentity\(').Value
if(!$leaseGuard){throw 'PRESPAWN must allow no physical lease while validating any existing lease'}
$leaseCheck=[scriptblock]::Create('param($hasLease,$matches) '+$leaseGuard.Replace('lease && !lease.MatchesTripIdentity(','$hasLease -and -not $matches'))
foreach($case in @(@($false,$false,$false),@($true,$false,$true),@($true,$true,$false))){
 if([bool](& $leaseCheck $case[0] $case[1]) -ne $case[2]){throw 'Optional reserved lease guard rejected legal PRESPAWN or accepted stale lease'}
 $controls++
}
$runner=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'tools/Invoke-AICFVehicleMultiSquad.ps1'))
$runnerTokens=$null;$runnerErrors=$null
[void][Management.Automation.Language.Parser]::ParseInput($runner,[ref]$runnerTokens,[ref]$runnerErrors)
if($runnerErrors.Count -or $runner -notmatch "\[string\]\`$ConcurrencyMode='MOVING'" -or
 $runner -notmatch "'-aicfVehicleMultiConcurrencyMode',\`$ConcurrencyMode" -or
 # ProtectionScope между прежними аргументами/полями не меняет их binding.
 $runner -notmatch '-ConcurrencyMode \$ConcurrencyMode' -or $runner -notmatch '-MinimumSeconds \$DurationSeconds' -or
 $runner -notmatch 'classification=\$taskClassification;' -or $runner -notmatch 'notmovingproof=\(\$ConcurrencyMode -eq ''MOUNTED_STAGING''\)' -or
 $runner -notmatch 'Compiled stage fixture does not support the separate MOUNTED_STAGING oracle' -or
 $runner -notmatch 'muster_policy=\$taskMusterPolicy' -or $runner -notmatch "\`$taskMusterPolicy='PER_GROUP_BEFORE_VEHICLE'" -or
 $runner -notmatch 'Compiled stage fixture does not support the PER_GROUP_BEFORE_VEHICLE muster policy'){
 throw 'Runner must preserve default MOVING and bind CLI/auditor/manifest to the explicit concurrency mode'
}
$reject=[regex]::Match($runner,'if\((\$ConcurrencyMode -eq ''MOUNTED_STAGING'' -and \$Mode -ne ''CAPACITY'')\)').Groups[1].Value
if(!$reject){throw 'Missing runner capacity-only guard'}
$runnerCheck=[scriptblock]::Create('param($ConcurrencyMode,$Mode) '+$reject)
foreach($case in @(@('MOVING','NORMAL',$false),@('MOVING','CAPACITY',$false),@('MOUNTED_STAGING','NORMAL',$true),@('MOUNTED_STAGING','CAPACITY',$false))){
 if([bool](& $runnerCheck $case[0] $case[1]) -ne $case[2]){throw 'Runner mixed mode guard failed'}
 $controls++
}
function AcqClass([string]$Text,[string]$Name){$m=[regex]::Match($Text,'(?m)^(?:modded )?class '+[regex]::Escape($Name)+'\b');if(!$m.Success){throw "Missing $Name"};$n=[regex]::Match($Text.Substring($m.Index+$m.Length),'(?m)^(?:modded )?class ');if($n.Success){return $Text.Substring($m.Index,$m.Length+$n.Index)};return $Text.Substring($m.Index)}
$taskAcqContext=AcqClass $source 'AICF_MultiAcquisitionContext'
$taskAcqFrame=AcqClass $source 'AICF_MultiAcquisitionFrame'
$taskAcqProbe=AcqClass $source 'AICF_MultiAcquisitionProbe'
$taskAcqFlow=AcqClass $source 'AICF_VehicleAcquisitionFlow'
$taskAcqUpdate=Body $taskAcqFlow 'Update'
$taskAcqDeadline=Body (AcqClass $source 'AICF_TransportTrip') 'AICF_MultiStoredDeadlineMs'
$taskAcqEnd=Body (AcqClass $source 'ArmaReforgerScripted') 'OnGameEnd'
$taskAcqBase=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Vehicles/AICF_VehicleAcquisitionFlow.c'))
if($taskAcqBase -notmatch '(?m)^\s*AICF_TripOutcome Update\(' -or [regex]::Matches($taskAcqUpdate,'super.Update\(').Count-ne1 -or $taskAcqUpdate -notmatch 'return actual;'){throw 'Acquisition wrapper must delegate one original-base Update and return same outcome'}
if($taskAcqDeadline.Trim() -ne '{ return m_iAbsoluteDeadlineMs; }'){throw 'Probe deadline accessor must read only raw stored script field'}
if(($taskAcqContext+$taskAcqProbe+$taskAcqUpdate) -match '\.(?:IsCurrent|Capture|GetID|GetWorld|FindComponent|Set\w*|Cancel\w*|CanAttempt|IsDeadlineReached|Commit\w*|GetAbsoluteDeadlineMs\(\)\s*\+)\(' -or ($taskAcqProbe+$taskAcqUpdate) -match 'GetGame\(|CallLater|GetTickCount|ObserveMeaningfulTaskLoss|ObserveUnexplainedMobIdle'){throw 'Probe repeats decisions/native reads or changes gameplay clocks'}
if($taskAcqContext -match 'ref\s+(?:AICF_|SCR_)' -or $taskAcqProbe.Replace('static ref AICF_MultiAcquisitionFrame s_Frame;','') -match 'static\s+(?:ref\s+)?(?:AICF_|SCR_)\w+\s+\w+\s*[;=]' -or $taskAcqFrame -match 'AICF_\w+\s+m_'){throw 'Observer retains domain refs'}
foreach($taskGetter in @('GetKind','GetReason','GetCausationId','GetRetryAtMs')){if([regex]::Matches((Body $taskAcqProbe 'Record'),'actual\.'+$taskGetter+'\(').Count-ne1){throw "Outcome getter repeated/missing $taskGetter"}}
if($taskAcqEnd -notmatch 'AICF_MultiAcquisitionProbe.Stop\(\);\s*super.OnGameEnd\(\);'){throw 'Probe game-end cache cleanup missing'}
$taskAcqBindings=@(
 @('State/Vehicles/AICF_TransportTrip.c','AICF_TransportTrip','GetAssignment,GetRequestState,GetLease,GetTripGeneration,GetOperationId,GetPhase,GetTransitionCount,GetPhaseStartedAtMs'),
 @('State/Vehicles/AICF_StrategicAssignmentSnapshot.c','AICF_StrategicAssignmentSnapshot','GetAssignmentRevision,GetGroup,GetGroupGeneration,GetStrategicIntentRevision,GetFactionKey,GetSlotId,GetBaseRevision'),
 @('State/Vehicles/AICF_VehiclePhaseStates.c','AICF_VehicleRequestState','GetRequestGeneration,GetSpawnPlan,GetAbsoluteDeadlineMs,GetNextAttemptAtMs,GetAttemptCount,GetTotalAttemptCount,GetMaximumAttempts,GetLastFailureReason,GetQuotaCreation'),
 @('Vehicles/AICF_VehicleSpawner.c','AICF_VehicleSpawnPlan','GetReservation,GetPlannedAtMs,GetApproachDeadlineMs,GetAllStagedSinceMs,GetStagedCount,GetAliveCount'),
 @('Vehicles/AICF_VehicleSpawner.c','AICF_VehicleSpawnSiteReservation','GetOwnerToken,GetRequestGeneration,GetReservedAtMs,GetExpiresAtMs'),
 @('State/Vehicles/AICF_VehicleLease.c','AICF_VehicleLease','GetLeaseGeneration,GetVehicleGeneration,GetVehicleLifecycleId,GetState'),
 @('State/Vehicles/AICF_TripOutcome.c','AICF_TripOutcome','GetKind,GetReason,GetCausationId,GetRetryAtMs'),
 @('Diagnostics/AICF_Stage1Diagnostics.c','AICF_Stage1Diagnostics','GetRunId')
)
$taskAcqBound=0
foreach($taskBinding in $taskAcqBindings){$taskOriginal=AcqClass ([IO.File]::ReadAllText((Join-Path $RepositoryRoot ('AIConflictCore/Scripts/Game/AIConflict/'+$taskBinding[0])))) $taskBinding[1];foreach($taskMethod in $taskBinding[2].Split(',')){if((Body $taskOriginal $taskMethod) -notmatch '^\s*\{\s*return [ms]_\w+;\s*\}\s*$'){throw "Observer getter ceased to be pure stored field: $($taskBinding[1]).$taskMethod"};$taskAcqBound++}}
function AcqReplay([string]$Text){
 $Text=$Text.Replace('modded class','public class').Replace('class AICF_MultiAcquisitionContext','public class AICF_MultiAcquisitionContext:Native').Replace('class AICF_MultiAcquisitionFrame','public class AICF_MultiAcquisitionFrame:Native').Replace('class AICF_MultiAcquisitionProbe','public class AICF_MultiAcquisitionProbe')
 $Text=$Text.Replace('ref AICF_MultiAcquisitionFrame','AICF_MultiAcquisitionFrame')
 $Text=$Text.Replace('static const int','public const int').Replace('static ref array<string>','public static EArray<string>').Replace('static ref array<int>','public static EArray<int>')
 $Text=$Text.Replace('= {};','= new EArray<string>();').Replace('EArray<int> s_At = new EArray<string>();','EArray<int> s_At = new EArray<int>();')
 $Text=$Text.Replace('s_Keys.Find(key)','s_Keys.IndexOf(key)').Replace('string.Format','Fmt.Format').Replace('Print(','Fmt.Print(').Replace('System.GetCLIParam("aicfVehicleAcquisitionOutcomeProbe", value)','Cli.GetCLIParam("aicfVehicleAcquisitionOutcomeProbe", out value)')
 $Text=[regex]::Replace($Text,'(?m)^(\s*)(static )?(AICF_\w+|SCR_\w+|bool|int|string) (m_\w+|s_\w+);','$1public $2$3 $4;')
 $Text=[regex]::Replace($Text,'(?m)^(\s*)static (bool|int|void|AICF_\w+) (\w+)\(','$1public static $2 $3(')
 $Text=$Text.Replace('bool SameSource(','public bool SameSource(').Replace('bool SameAcquisitionState(','public bool SameAcquisitionState(').Replace('super.','base.')
 $Text=[regex]::Replace($Text,'(?m)^(\s*)(string|int) (\w+);','$1$2 $3 = default($2);')
 $Text=$Text.Replace('AICF_MultiAcquisitionContext before;','AICF_MultiAcquisitionContext before = null;')
 $Text=$Text.Replace('AICF_MultiAcquisitionFrame frame;','AICF_MultiAcquisitionFrame frame = null;')
 return $Text
}
$taskAcqShim=@'
using System;using System.Collections.Generic;
namespace MultiAcquisitionReplay{
public class Native{public static implicit operator bool(Native n){return n!=null;}}
public class EArray<T>:List<T>{public new int Count(){return base.Count;}public void Insert(T v){Add(v);}}
public static class Fmt{public static List<string> Logs=new List<string>();public static string Format(string f,params object[] a){for(int i=a.Length;i>0;i--)f=f.Replace("%"+i,a[i-1]==null?"NONE":a[i-1].ToString());return f;}public static void Print(string s){Logs.Add(s);}}
public static class Cli{public static bool Enabled;public static bool GetCLIParam(string n,out string v){v=Enabled?"1":"";return Enabled;}}
public static class AICF_Stage1Diagnostics{public static string Run="RUN1";public static string GetRunId(){return Run;}}
public class SCR_AIGroup:Native{}public class AICF_FactionFleet:Native{}public class SCR_CampaignFaction:Native{}
public class AICF_StrategicAssignmentSnapshot:Native{public SCR_AIGroup Group=new SCR_AIGroup();public string Faction="US";public int Slot,Generation=1,Revision=2,Intent=3,BaseRevision=4;public Action ReadAction;public SCR_AIGroup GetGroup(){return Group;}public string GetFactionKey(){return Faction;}public int GetSlotId(){return Slot;}public int GetGroupGeneration(){return Generation;}public int GetAssignmentRevision(){if(ReadAction!=null)ReadAction();return Revision;}public int GetStrategicIntentRevision(){return Intent;}public int GetBaseRevision(){return BaseRevision;}}
public class AICF_VehicleSpawnSiteReservation:Native{public string Token="SITE1";public int Generation=1,Reserved=10,Expires=90000;public string GetOwnerToken(){return Token;}public int GetRequestGeneration(){return Generation;}public int GetReservedAtMs(){return Reserved;}public int GetExpiresAtMs(){return Expires;}}
public class AICF_VehicleSpawnPlan:Native{public AICF_VehicleSpawnSiteReservation Reservation=new AICF_VehicleSpawnSiteReservation();public int Planned=20,Deadline=80000,Since=30,Staged=5,Alive=5;public AICF_VehicleSpawnSiteReservation GetReservation(){return Reservation;}public int GetPlannedAtMs(){return Planned;}public int GetApproachDeadlineMs(){return Deadline;}public int GetAllStagedSinceMs(){return Since;}public int GetStagedCount(){return Staged;}public int GetAliveCount(){return Alive;}}
public class AICF_VehicleRequestState:Native{public AICF_VehicleSpawnPlan Plan=new AICF_VehicleSpawnPlan();public int Generation=1,Deadline=100000,Next=150,Attempts=1,Total=1,Maximum=4;public string Failure="NONE";public Native Guard;public int GetRequestGeneration(){return Generation;}public int GetAbsoluteDeadlineMs(){return Deadline;}public int GetNextAttemptAtMs(){return Next;}public int GetAttemptCount(){return Attempts;}public int GetTotalAttemptCount(){return Total;}public int GetMaximumAttempts(){return Maximum;}public string GetLastFailureReason(){return Failure;}public Native GetQuotaCreation(){return Guard;}public AICF_VehicleSpawnPlan GetSpawnPlan(){return Plan;}}
public class AICF_VehicleLease:Native{public int Generation=1,VehicleGeneration=1,State;public string Id="CAR1";public int GetLeaseGeneration(){return Generation;}public int GetVehicleGeneration(){return VehicleGeneration;}public string GetVehicleLifecycleId(){return Id;}public int GetState(){return State;}}
public class AICF_TransportTrip:Native{public AICF_StrategicAssignmentSnapshot Assignment=new AICF_StrategicAssignmentSnapshot();public AICF_VehicleRequestState Request=new AICF_VehicleRequestState();public AICF_VehicleLease Lease;public int Generation=1,Phase=5,Transitions=4,Started=50,m_iAbsoluteDeadlineMs=120000;public string Operation="TRIP1";public AICF_StrategicAssignmentSnapshot GetAssignment(){return Assignment;}public AICF_VehicleRequestState GetRequestState(){return Request;}public AICF_VehicleLease GetLease(){return Lease;}public int GetTripGeneration(){return Generation;}public string GetOperationId(){return Operation;}public int GetPhase(){return Phase;}public int GetTransitionCount(){return Transitions;}public int GetPhaseStartedAtMs(){return Started;}public int AICF_MultiStoredDeadlineMs()DEADLINE}
public class AICF_TripOutcome:Native{public int Kind,Retry=123,Reads;public string Reason="VEHICLE_QUOTA_PENDING",Causation="CAUSE1";public Action<string> Hook;public int GetKind(){Reads++;if(Hook!=null)Hook("kind");return Kind;}public string GetReason(){Reads++;if(Hook!=null)Hook("reason");return Reason;}public string GetCausationId(){Reads++;if(Hook!=null)Hook("causation");return Causation;}public int GetRetryAtMs(){Reads++;if(Hook!=null)Hook("retry");return Retry;}}
public class FlowBase:Native{public int Calls,Writes;public object[] Args;public int Now;public Action During;public Exception Error;public AICF_TripOutcome Result=new AICF_TripOutcome();public virtual AICF_TripOutcome Update(AICF_TransportTrip trip,AICF_StrategicAssignmentSnapshot assignment,AICF_FactionFleet fleet,SCR_CampaignFaction faction,int now){Calls++;Args=new object[]{trip,assignment,fleet,faction};Now=now;if(During!=null)During();if(Error!=null)throw Error;return Result;}public void DebugWrite(){Writes++;}}
public class AICF_VehicleAcquisitionFlow:FlowBase{public override AICF_TripOutcome Update(AICF_TransportTrip trip,AICF_StrategicAssignmentSnapshot currentAssignment,AICF_FactionFleet fleet,SCR_CampaignFaction faction,int nowMs)UPDATE}
public class GameBase{public int Calls;public virtual void OnGameEnd(){Calls++;}}
public class ArmaReforgerScripted:GameBase{public override void OnGameEnd()ENDGAME}
'@
$taskAcqCases=@'
public static class AcquisitionCases{
static AICF_TransportTrip Trip;static AICF_VehicleAcquisitionFlow Flow;static AICF_FactionFleet Fleet;static SCR_CampaignFaction Faction;
static void Setup(bool enabled=true){AICF_MultiAcquisitionProbe.Stop();Fmt.Logs.Clear();Cli.Enabled=enabled;AICF_Stage1Diagnostics.Run="RUN1";Trip=new AICF_TransportTrip();Flow=new AICF_VehicleAcquisitionFlow();Fleet=new AICF_FactionFleet();Faction=new SCR_CampaignFaction();}
static AICF_TripOutcome Call(int now=1000){return Flow.Update(Trip,Trip==null?null:Trip.Assignment,Fleet,Faction,now);}
public static int Run(){int n=0;foreach(bool enabled in new[]{false,true})foreach(bool absent in new[]{false,true}){Setup(enabled);var result=Flow.Result;if(absent)Flow.Result=null;int deadline=Trip.m_iAbsoluteDeadlineMs;if(Call()!=(absent?null:result)||Flow.Calls!=1||Flow.Writes!=0||Flow.Args[0]!=Trip||Flow.Args[1]!=Trip.Assignment||Flow.Args[2]!=Fleet||Flow.Args[3]!=Faction||Flow.Now!=1000||Trip.m_iAbsoluteDeadlineMs!=deadline)throw new Exception("original result/args/deadline altered");if(result.Reads!=(enabled&&!absent?4:0))throw new Exception("outcome getters not exactly once");if(!enabled&&(Fmt.Logs.Count!=0||AICF_MultiAcquisitionProbe.s_iRecords!=0))throw new Exception("disabled probe observed");if(enabled&&!absent&&!string.Join("\n",Fmt.Logs).Contains("actual_reason=VEHICLE_QUOTA_PENDING"))throw new Exception("actual reason replaced");n++;}
Setup(false);Trip.Assignment.ReadAction=()=>{throw new Exception("disabled snapshot observed");};if(Call()!=Flow.Result||Flow.Calls!=1)throw new Exception("disabled result");n++;
Setup();var error=new InvalidOperationException("ORIGINAL");Flow.Error=error;bool sameError=false;try{Call();}catch(Exception e){sameError=Object.ReferenceEquals(e,error);}if(!sameError||Flow.Calls!=1||AICF_MultiAcquisitionProbe.s_iRecords!=0)throw new Exception("original exception suppressed/replaced");n++;
foreach(string change in new[]{"group","generation","tripGeneration","assignment","inputRevision","request","requestGeneration","plan","reservation","token","lease","run","clearedRequest"}){
 Setup();bool foreign=change=="group"||change=="generation"||change=="tripGeneration"||change=="assignment"||change=="inputRevision"||change=="run";
 switch(change){case"group":Flow.During=()=>Trip.Assignment.Group=new SCR_AIGroup();break;case"generation":Flow.During=()=>Trip.Assignment.Generation++;break;case"tripGeneration":Flow.During=()=>Trip.Generation++;break;case"assignment":Flow.During=()=>Trip.Assignment=new AICF_StrategicAssignmentSnapshot();break;case"inputRevision":Flow.During=()=>Trip.Assignment.Revision++;break;case"request":Flow.During=()=>Trip.Request=new AICF_VehicleRequestState();break;case"requestGeneration":Flow.During=()=>Trip.Request.Generation++;break;case"plan":Flow.During=()=>Trip.Request.Plan=new AICF_VehicleSpawnPlan();break;case"reservation":Flow.During=()=>Trip.Request.Plan.Reservation=new AICF_VehicleSpawnSiteReservation();break;case"token":Flow.During=()=>Trip.Request.Plan.Reservation.Token="SITE2";break;case"lease":Flow.During=()=>Trip.Lease=new AICF_VehicleLease();break;case"run":Flow.During=()=>AICF_Stage1Diagnostics.Run="SUCCESSOR";break;case"clearedRequest":Flow.During=()=>Trip.Request=null;break;}
 var result=Flow.Result;if(Call()!=result||Flow.Calls!=1)throw new Exception("original changed "+change);
 if(foreign){if(!AICF_MultiAcquisitionProbe.s_bScopeLost||AICF_MultiAcquisitionProbe.s_Keys.Count()!=0||!string.Join("\n",Fmt.Logs).Contains("STABLE_IDENTITY_CHANGED"))throw new Exception("foreign cache write "+change);}
 else if(AICF_MultiAcquisitionProbe.s_bScopeLost||!string.Join("\n",Fmt.Logs).Contains("acquisition_context_changed=True"))throw new Exception("legitimate mutable state called foreign "+change);n++;
}
foreach(string seam in new[]{"super","readBefore","readAfter","kind","reason","causation","retry","stopNewBegin"}){
 Setup();var outer=Trip;var nested=new AICF_VehicleAcquisitionFlow();var next=new AICF_TransportTrip{Operation="NESTED"};int savedRecords=-1;string savedKeys=null,savedSignatures=null,savedAt=null;AICF_MultiAcquisitionFrame savedFrame=null;
 Action inner=()=>{if(seam=="stopNewBegin")AICF_MultiAcquisitionProbe.Stop();if(nested.Update(next,next.Assignment,Fleet,Faction,1001)!=nested.Result)throw new Exception("nested result");savedRecords=AICF_MultiAcquisitionProbe.s_iRecords;savedKeys=string.Join("|",AICF_MultiAcquisitionProbe.s_Keys);savedSignatures=string.Join("|",AICF_MultiAcquisitionProbe.s_Signatures);savedAt=string.Join("|",AICF_MultiAcquisitionProbe.s_At);savedFrame=AICF_MultiAcquisitionProbe.s_Frame;};
 if(seam=="super"||seam=="stopNewBegin")Flow.During=inner;
 else if(seam=="readBefore"||seam=="readAfter"){int reads=0;Trip.Assignment.ReadAction=()=>{reads++;if(reads==(seam=="readBefore"?1:3)){Trip.Assignment.ReadAction=null;inner();}};}
 else Flow.Result.Hook=key=>{if(key==seam){Flow.Result.Hook=null;inner();}};
 if(Call()!=Flow.Result||Flow.Calls!=1||nested.Calls!=1||Flow.Result.Reads!=4||savedRecords<1||savedRecords!=AICF_MultiAcquisitionProbe.s_iRecords||savedKeys!=string.Join("|",AICF_MultiAcquisitionProbe.s_Keys)||savedSignatures!=string.Join("|",AICF_MultiAcquisitionProbe.s_Signatures)||savedAt!=string.Join("|",AICF_MultiAcquisitionProbe.s_At)||savedFrame!=AICF_MultiAcquisitionProbe.s_Frame)throw new Exception("stale outer overwrote successor "+seam);n++;
}
Setup();Call(1000);Call(1001);if(AICF_MultiAcquisitionProbe.s_iRecords!=1)throw new Exception("duplicate unchanged WAIT printed");Call(11000);if(AICF_MultiAcquisitionProbe.s_iRecords!=2)throw new Exception("bounded observer heartbeat missing");Flow.Result.Reason="SPAWN_BACKOFF";Call(11001);if(AICF_MultiAcquisitionProbe.s_iRecords!=3)throw new Exception("changed actual WAIT hidden");n++;
Setup();for(int i=0;i<AICF_MultiAcquisitionProbe.MAX_CONTEXTS;i++){Trip=new AICF_TransportTrip{Operation="O"+i};Call(i);}Trip=new AICF_TransportTrip{Operation="OVER"};Call(999);if(!AICF_MultiAcquisitionProbe.s_bOverflow||AICF_MultiAcquisitionProbe.s_Keys.Count()!=AICF_MultiAcquisitionProbe.MAX_CONTEXTS)throw new Exception("context bound hidden");n++;
Setup();for(int i=0;i<AICF_MultiAcquisitionProbe.MAX_RECORDS;i++){Flow.Result.Reason="R"+i;Call(i);}Flow.Result.Reason="OVER";Call(999);if(!AICF_MultiAcquisitionProbe.s_bOverflow||AICF_MultiAcquisitionProbe.s_iRecords!=AICF_MultiAcquisitionProbe.MAX_RECORDS)throw new Exception("record bound hidden");n++;
Setup();Flow.Error=new InvalidOperationException("ORIGINAL");for(int i=0;i<AICF_MultiAcquisitionProbe.MAX_RECORDS+2;i++){try{Call(i);}catch(InvalidOperationException){}}if(!AICF_MultiAcquisitionProbe.s_bOverflow||!AICF_MultiAcquisitionProbe.s_bScopeLost||AICF_MultiAcquisitionProbe.s_iRecords!=AICF_MultiAcquisitionProbe.MAX_RECORDS||AICF_MultiAcquisitionProbe.s_Keys.Count()!=0)throw new Exception("unfinished-frame metadata escaped budget");n++;
Setup();Call();var game=new ArmaReforgerScripted();game.OnGameEnd();if(game.Calls!=1||AICF_MultiAcquisitionProbe.s_Keys.Count()!=0||AICF_MultiAcquisitionProbe.s_Signatures.Count()!=0||AICF_MultiAcquisitionProbe.s_At.Count()!=0||AICF_MultiAcquisitionProbe.s_iRecords!=0||AICF_MultiAcquisitionProbe.s_bOverflow||AICF_MultiAcquisitionProbe.s_bScopeLost||AICF_MultiAcquisitionProbe.s_Frame!=null)throw new Exception("unbalanced game-end clear");n++;
Setup();Trip=null;if(Call()!=Flow.Result||Flow.Calls!=1||!AICF_MultiAcquisitionProbe.s_bScopeLost)throw new Exception("null scope changed original delegate");n++;return n;}
}
}
'@
$taskAcqCode=$taskAcqShim.Replace('DEADLINE',$taskAcqDeadline).Replace('UPDATE',(AcqReplay $taskAcqUpdate)).Replace('ENDGAME',(AcqReplay $taskAcqEnd))+(AcqReplay $taskAcqContext)+(AcqReplay $taskAcqFrame)+(AcqReplay $taskAcqProbe)+$taskAcqCases
Add-Type -TypeDefinition $taskAcqCode -Language CSharp
$taskAcqCount=[MultiAcquisitionReplay.AcquisitionCases]::Run()
$taskAcqMutants=@(
 @('zero','AICF_TripOutcome actual = base.Update(trip, currentAssignment, fleet, faction, nowMs);','AICF_TripOutcome actual = Result;'),
 @('double','AICF_TripOutcome actual = base.Update(trip, currentAssignment, fleet, faction, nowMs);','AICF_TripOutcome actual = base.Update(trip, currentAssignment, fleet, faction, nowMs); actual = base.Update(trip, currentAssignment, fleet, faction, nowMs);'),
 @('return','return actual;','return null;'),
 @('reason','reason = actual.GetReason();','reason = "FAKE";'),
 @('getter','kind = actual.GetKind();','kind = actual.GetKind(); kind = actual.GetKind();'),
 @('stale','m_Group != other.m_Group','false'),
 @('frame','if (!frame || frame != s_Frame) return;','if (!frame) return;'),
 @('mutable','!before.SameAcquisitionState(after)','false'),
 @('cadence','nowMs - s_At[index] < 10000','nowMs - s_At[index] < 0'),
 @('stop','s_Keys.Clear();','')
)
$taskAcqCaught=0
foreach($taskMutation in $taskAcqMutants){if(!$taskAcqCode.Contains($taskMutation[1])){throw "Missing acquisition unsafe target $($taskMutation[0])"};$taskNS='MultiAcquisitionUnsafe'+$taskMutation[0];Add-Type -TypeDefinition ($taskAcqCode.Replace('MultiAcquisitionReplay',$taskNS).Replace($taskMutation[1],$taskMutation[2])) -Language CSharp;$taskBad=$false;try{[void]([type]($taskNS+'.AcquisitionCases')).GetMethod('Run').Invoke($null,@())}catch{$taskBad=$true};if(!$taskBad){throw "Acquisition unsafe survived $($taskMutation[0])"};$taskAcqCaught++}
"PASS Multi acquisition outcome observer actual_cases=$taskAcqCount executed_unsafe=$taskAcqCaught stored_field_bindings=$taskAcqBound original_result_reference=1 script_state_only=1 native=NOT_RUN"
"PASS Multi-squad fixture: real paid muster/leases/cohorts, NORMAL isolation, bounded CAPACITY funding, live final intent and startup/stop ordering; controls=$controls"
# Actual composed preflight flow. Native calls are deterministic stubs; bodies below are extracted.
$taskFundingCore=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Economy/AICF_EconomySystem.c'))
$taskFundingFlow=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Vehicles/AICF_VehicleAcquisitionFlow.c'))
$taskFundingDecision=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Economy/AICF_VehiclePresetEconomyDecision.c'))
function FundingAdapt([string]$s){
 $s=$s.Replace('super.','SUPER.');$s=[regex]::Replace($s,'\bbase\b','site');$s=$s.Replace('SUPER.','base.')
 $s=$s.Replace('GetGame()','Runtime.Game').Replace('string.Format(','Runtime.Format(').Replace('System.GetTickCount()','Runtime.Now').Replace('ResourceName','string').Replace('EntityID.INVALID','0').Replace('EntityID','int').Replace('AICF_EconomySystem','Economy')
 $s=$s.Replace('System.GetCLIParam("aicfVehicleMultiMode", mode)','Cli.Get(out mode)')
 $s=[regex]::Replace($s,'\bIEntity\b','Actor')
 $s=$s.Replace('protected ref ','protected ').Replace('override ','').Replace('protected ','public ')
 $s=$s.Replace('array<string>','L<string>').Replace('L<string> m_aAICFMultiGrantTokens = {};','L<string> m_aAICFMultiGrantTokens = new L<string>();')
 $s=[regex]::Replace($s,'foreach \(([^:]+) : ([^)]+)\)','foreach ($1 in $2)')
 $s=[regex]::Replace($s,'(?m)^(\s*)(int|bool|string|Economy|AICF_\w+)\s+(\w+);\s*$',{param($m)$v='null';switch($m.Groups[2].Value){int{$v='0'}bool{$v='false'}string{$v='""'}};$m.Groups[1].Value+$m.Groups[2].Value+' '+$m.Groups[3].Value+'='+$v+';'})
 $s=[regex]::Replace($s,'for \(int (\w+);','for (int $1 = 0;')
 $s=$s.Replace('int cost, started;','int cost=0, started=0;').Replace('out int','out int')
 $s=$s.Replace('prefab, cost, started)','prefab, out cost, out started)')
 $s=$s.Replace('faction, base, prefab, paid)','faction, site, prefab, out paid)')
 $s=[regex]::Replace($s,'(?m)^(\s*)(bool|void|int|AICF_\w+) (\w+)\(','$1public $2 $3(')
 return $s
}
function FundingMethod([string]$text,[string]$name){
 $head=[regex]::Match($text,'(?m)^\s*(?:(?:override|protected|static)\s+)*\w+\s+'+[regex]::Escape($name)+'\([^)]*\)').Value
 if(!$head){throw ('Funding method declaration missing '+$name)}
 $method=FundingAdapt ($head+(Body $text $name))
 if($name-eq'AICF_MultiOwnVehicleQuote'){$method=$method.Insert($method.IndexOf('{')+1,'cost=0; started=0;')}
 return $method
}
$taskFundingMethods=@('InspectVehiclePresetPreflight','AICF_MultiPreflightScopeCurrent','AICF_MultiJitVehicleQuote','AICF_MultiGranted','AICF_MultiFund')|ForEach-Object{FundingMethod $source $_}
$taskFundingOwner=@('AICF_MultiOwnVehicleQuote','AICF_MultiOwnPreflightFleet','AICF_MultiRoster')|ForEach-Object{FundingMethod $source $_}
$taskFundingOriginal=FundingAdapt (Body $taskFundingCore 'InspectVehiclePresetPreflight')
$taskFundingOriginal=$taskFundingOriginal.Insert(1,'Calls++;')
$taskFundingClassify=FundingAdapt (Body $taskFundingDecision 'ClassifyQuote')
$taskFundingLease=FundingAdapt (Body $taskFundingDecision 'IsOwnUnusedReservation')
$taskFundingFlowBody=FundingMethod $taskFundingFlow 'InspectPresetEconomyBeforeLease'
$taskFundingCode=@'
#pragma warning disable 0162,0168
using System;
namespace FundingReplay {
public static class S{public static bool IsEmpty(this string s){return string.IsNullOrEmpty(s);}}
public class N{public static implicit operator bool(N x){return x!=null;}public static void Print(string s){Runtime.Logs.Add(s);}}
public class L<T>:System.Collections.Generic.List<T>{public new int Count(){return base.Count;}public void Insert(T x){Add(x);}}
public static class Math{public static int Max(int a,int b){return System.Math.Max(a,b);}public static int Min(int a,int b){return System.Math.Min(a,b);}public static float Max(float a,float b){return System.Math.Max(a,b);}public static int Ceil(float x){return (int)System.Math.Ceiling(x);}}
public static class Cli{public static string Mode="CAPACITY";public static bool Get(out string m){m=Mode;return true;}}
public static class Replication{public static bool Server=true;public static bool IsServer(){return Server;}}
public static class Runtime{public static int Now=1000,Price=475;public static Action MetadataHook;public static int MetadataReads;public static System.Collections.Generic.List<string> Logs=new System.Collections.Generic.List<string>();public static Game Game=new Game();public static string Format(string f,params object[] a){for(int i=9;i>=1;i--)f=f.Replace("%"+i,"{"+(i-1)+"}");return string.Format(f,a);}}
public class Game:N{public bool IsServer(){return true;}}
public enum AICF_EVehiclePresetEconomyDecision{INVALID_CONTEXT,AFFORDABLE,INSUFFICIENT_AVAILABLE_SUPPLIES}
public enum AICF_ETransportTripPhase{SPAWN_COMMIT,OTHER}
public enum AICF_EVehicleLeaseState{RESERVED,ACTIVE}
public class AICF_VehiclePreset:N{public string m_sPrefab="CAR";}
public class Actor:N{public int Id;public bool Alive=true,Authority=true;public int GetID(){return Id;}}
public class SCR_AIGroup:N{public int Id=11;public int GetID(){return Id;}public bool Authority=true;public L<Actor> Actors=new L<Actor>();}
public class AICF_GroupSlot:N{public SCR_AIGroup Group;public int Generation=1;public bool IsCombatReady(){return true;}public int GetSpawnGeneration(){return Generation;}public SCR_AIGroup GetGroup(){return Group;}public Actor GetRosterMember(int i){return Group.Actors[i];}}
public class SCR_CampaignMilitaryBaseComponent:N{public int Stock=465,Capacity=1200,Adds;public Action OnAdd;public int GetSupplies(){return Stock;}public int GetSuppliesMax(){return Capacity;}public void AddSupplies(int n){Adds++;Stock=System.Math.Min(Capacity,Stock+n);if(OnAdd!=null)OnAdd();}}
public class SCR_CampaignFaction:N{public string Key="US";public SCR_CampaignMilitaryBaseComponent Site;public string GetFactionKey(){return Key;}public SCR_CampaignMilitaryBaseComponent GetMainBase(){return Site;}}
public class AICF_StrategicAssignmentSnapshot:N{public SCR_AIGroup Group;public int Revision=1;public SCR_AIGroup GetGroup(){return Group;}}
public class AICF_VehicleLease:N{public bool Match=true,Physical;public int Gen=1;public AICF_EVehicleLeaseState State=AICF_EVehicleLeaseState.RESERVED;public bool MatchesTripIdentity(string f,int s,int g,int t){return Match;}public int GetLeaseGeneration(){return Gen;}public AICF_EVehicleLeaseState GetState(){return State;}public bool HasPhysicalAsset(){return Physical;}public N GetVehicle(){return Physical?new N():null;}public int GetVehicleGeneration(){return Physical?1:0;}public int GetEntityId(){return Physical?1:0;}public N GetCleanupSnapshot(){return null;}public string GetEntityIdString(){return Physical?"1":"";}public string GetRplId(){return "";}public string GetPrefab(){return "";}public string GetVehicleLifecycleId(){return "";}}
public class AICF_FactionFleet:N{public string Key="US";public AICF_VehicleLease Lease;public string GetFactionKey(){return Key;}public bool OwnsVehicleLease(AICF_VehicleLease l){return Lease==l;}public AICF_VehicleLease FindLeaseForSlot(int i){return Lease;}}
public class AICF_FleetRegistry:N{public AICF_FactionFleet Fleet;public AICF_FactionFleet FindFleet(string f){return Fleet;}}
public class AICF_VehicleSpawnSiteSelection:N{public SCR_CampaignMilitaryBaseComponent m_Base;}
public class AICF_VehicleSpawnSiteReservation:N{public SCR_CampaignMilitaryBaseComponent Site;public string Token="SITE";public bool Current=true;public SCR_CampaignMilitaryBaseComponent GetBase(){return Site;}public bool IsExpired(int now){return !Current;}public bool MatchesTrip(AICF_TransportTrip t,int g){return Current;}public string GetOwnerToken(){return Token;}}
public class AICF_VehicleSpawnPlan:N{public AICF_VehicleSpawnSiteSelection Selection;public AICF_VehicleSpawnSiteReservation Reservation;public AICF_VehicleSpawnSiteSelection GetSelection(){return Selection;}public AICF_VehicleSpawnSiteReservation GetReservation(){return Reservation;}}
public class AICF_VehicleRequestState:N{public int Generation=1,Attempt;public AICF_VehicleSpawnPlan Plan;public int GetRequestGeneration(){return Generation;}public int GetAttemptCount(){return Attempt;}public AICF_VehicleSpawnPlan GetSpawnPlan(){return Plan;}}
public class AICF_TransportTrip:N{public bool Valid=true,Current=true,Debited,Expired;public string Operation="O";public int Slot,GroupGen=1,TripGen=1,PresetRevision=1;public AICF_ETransportTripPhase Phase;public AICF_VehiclePreset Preset=new AICF_VehiclePreset();public AICF_VehicleRequestState Request;public AICF_VehicleLease Lease;public AICF_StrategicAssignmentSnapshot Assignment;public AICF_GroupSlot PresetSlot;public bool IsValid(){return Valid;}public bool IsTerminal(){return false;}public bool IsDeadlineReached(int n){return Expired;}public bool IsVehiclePresetCurrent(){return Current;}public bool IsVehiclePresetDebited(){return Debited;}public AICF_ETransportTripPhase GetPhase(){return Phase;}public int GetSlotId(){return Slot;}public string GetFactionKey(){return "US";}public int GetGroupGeneration(){return GroupGen;}public int GetTripGeneration(){return TripGen;}public int GetVehiclePresetRevision(){return PresetRevision;}public AICF_VehiclePreset GetVehiclePreset(){return Preset;}public AICF_VehicleRequestState GetRequestState(){return Request;}public AICF_VehicleLease GetLease(){return Lease;}public bool HasLease(){return Lease!=null;}public AICF_StrategicAssignmentSnapshot GetAssignment(){return Assignment;}public string GetOperationId(){return Operation;}public AICF_GroupSlot GetVehiclePresetSlot(){return PresetSlot;}public bool RecordVehiclePresetEconomyDecline(AICF_VehiclePresetEconomyDecision d){return true;}}
public class AICF_MultiAcquisitionFrame:N{public bool m_bCompleted;public int m_iQuoteSequence;}
public class AICF_MultiAcquisitionContext:N{public AICF_TransportTrip Trip;public AICF_VehicleRequestState Request;public AICF_VehicleSpawnPlan Plan;public string Signature,m_sFields;public bool SameSource(AICF_MultiAcquisitionContext a){return a!=null&&a.Trip==Trip&&Signature==a.Signature;}public bool SameAcquisitionState(AICF_MultiAcquisitionContext a){return a!=null&&a.Request==Request&&a.Plan==Plan;}}
public static class AICF_MultiAcquisitionProbe{public static AICF_MultiAcquisitionContext Read(AICF_TransportTrip t,AICF_StrategicAssignmentSnapshot a){return new AICF_MultiAcquisitionContext{Trip=t,Request=t.Request,Plan=t.Request==null?null:t.Request.Plan,Signature=t.GroupGen+"/"+t.TripGen+"/"+t.PresetRevision+"/"+a.Revision,m_sFields=t.Operation+"/"+t.Phase+"/"+(t.Request==null?"NONE":t.Request.Generation+"/"+(t.Request.Plan==null?"NONE":t.Request.Plan.Reservation.Token))};}}
public static class AICF_GroupTrace{public static string Entity(SCR_AIGroup g){return g.Id.ToString();}}
public static class AICF_Stage1Diagnostics{public static string BaseKey(SCR_CampaignMilitaryBaseComponent b){return "BASE";}}
public static class AICF_Stage4Diagnostics{public static void Info(string e,string d){Runtime.Logs.Add(e);}}
public static class AICF_MultiConcurrency{public static string Metadata(string m){return " diagnostic";}}
public static class AICF_GroupRuntime{public static int CountAliveAgents(SCR_AIGroup g){return g.Actors.FindAll(a=>a.Alive).Count;}public static bool IsAliveCharacter(Actor a){return a.Alive;}}
public static class AICF_VehicleBoardingMutationFence{public static bool IsAuthoritativeReplicatedEntity(SCR_AIGroup g){return g.Authority;}public static bool IsAuthoritativeAIEntity(Actor a){return a.Authority;}}
public class AICF_VehiclePresetCatalog:N{int Price;public AICF_VehiclePresetCatalog(SCR_CampaignFaction f){Runtime.MetadataReads++;Price=Runtime.Price;if(Runtime.MetadataHook!=null)Runtime.MetadataHook();}public int Cost(string p){return Price;}public void Clear(){}}
public class Recipe:N{public string m_sPrefab="CAR";}
public class AICF_MultiSquadCase:N{public string m_sFaction="US";public int m_iSlot,m_iPhase=2,m_iGeneration=1,m_GroupId=11;public bool m_bRecruitAccepted=true;public L<int> m_aMembers=new L<int>();public Recipe m_Recipe=new Recipe();}
public class Coordinator:N{public AICF_FleetRegistry Registry;public AICF_TransportTrip Trip;public AICF_TransportTrip GetTrip(AICF_GroupSlot s){return Trip;}public AICF_FleetRegistry GetAIQuotaFleetRegistry(){return Registry;}}
public class Campaign:N{public bool Master=true,Running=true;public bool IsMaster(){return Master;}public bool IsRunning(){return Running;}}
public class Supply:N{public bool Operational=true;public bool IsOperationalOwnedBase(SCR_CampaignMilitaryBaseComponent b,SCR_CampaignFaction f){return Operational;}}
public class Adapter:N{public string Reject="";public string GetSpawnRejectionReason(SCR_CampaignMilitaryBaseComponent b,SCR_CampaignFaction f){return Reject;}}
public class AICF_MatchController:N{public static AICF_MatchController Active;public bool m_bStopped,m_bAICFMultiDone;public bool m_bAICFMultiStarted=true;public string m_sAICFMultiMode="CAPACITY";public int m_iAICFMultiCohort=2,m_iAICFMultiStart=1;public Coordinator m_VehicleCoordinator;public AICF_GroupSlot Slot;public L<AICF_MultiSquadCase> m_aAICFMultiCases=new L<AICF_MultiSquadCase>();public static AICF_MatchController GetActiveController(){return Active;}public AICF_GroupSlot AICF_MultiSlot(AICF_MultiSquadCase i){return Slot;} OWNER_METHODS }
public class AICF_VehiclePresetEconomyDecision:N{public static AICF_VehiclePresetEconomyDecision Last;public AICF_EVehiclePresetEconomyDecision Status;public int Cost,Stock,Obligations;public SCR_CampaignMilitaryBaseComponent Site;public AICF_FactionFleet Fleet;public AICF_VehiclePresetEconomyDecision(AICF_TransportTrip t,SCR_CampaignMilitaryBaseComponent b,int c,int s,int o,AICF_EVehiclePresetEconomyDecision st,AICF_FactionFleet f=null){Cost=c;Stock=s;Obligations=o;Status=st;Site=b;Fleet=f;Last=this;}public AICF_EVehiclePresetEconomyDecision GetStatus(){return Status;}public AICF_FactionFleet GetFleet(){return Fleet;}public string Details(){return "ACTUAL";}public static AICF_EVehiclePresetEconomyDecision ClassifyQuote(int cost,int stock,int obligations) CLASSIFY public static bool IsOwnUnusedReservation(AICF_TransportTrip trip,AICF_FactionFleet fleet,AICF_VehicleLease lease,int leaseGeneration) LEASE }
public class EconomyBase:N{public int Calls,Obligations;public bool Enabled=true,m_bGraphContextReady=true;public N m_Graph=new N();public Campaign m_Campaign=new Campaign();public Supply m_SupplyNetwork=new Supply();public Adapter m_ConflictAdapter=new Adapter();public bool IsEnabled(){return Enabled;}public int LogisticsObligations(SCR_CampaignMilitaryBaseComponent b){return Obligations;}public virtual AICF_VehiclePresetEconomyDecision InspectVehiclePresetPreflight(AICF_TransportTrip trip,SCR_CampaignFaction faction,SCR_CampaignMilitaryBaseComponent site,AICF_FactionFleet fleet) ORIGINAL }
public class Economy:EconomyBase{public int m_iAICFMultiUSGrant,m_iAICFMultiUSSRGrant,m_iAICFMultiPreflightSequence;public L<string> m_aAICFMultiGrantTokens=new L<string>();public AICF_MultiAcquisitionFrame m_AICFMultiPreflightFrame; FIXTURE_METHODS }
public class AICF_TripOutcome:N{public string Reason;public AICF_TripOutcome(string r){Reason=r;}}
public class Flow:N{public Economy Economy;public int EndCalls,FactoryCalls;public AICF_TripOutcome EndRequest(AICF_TransportTrip t,AICF_FactionFleet f,AICF_VehicleRequestState r,string why,int now,bool strict){EndCalls++;return new AICF_TripOutcome(why);}public AICF_TripOutcome FailClosed(AICF_TransportTrip t,string r,string c){EndCalls++;return new AICF_TripOutcome(r);} FLOW_BODY }
'@
# Bind only the original public return seam; all original method inputs are unchanged.
$taskFundingFlowBody=$taskFundingFlowBody.Replace('match.VehiclePresetEconomy()','match.FundingEconomy')
$taskFundingCode=$taskFundingCode.Replace('public static AICF_MatchController Active;','public static AICF_MatchController Active;public Economy FundingEconomy;')
$taskFundingCode=$taskFundingCode.Replace('public class AICF_MatchController:N','public class AICF_MatchController:N')
$taskFundingCode=$taskFundingCode.Replace(' OWNER_METHODS ',($taskFundingOwner -join "`n")).Replace(' CLASSIFY ',$taskFundingClassify).Replace(' LEASE ',$taskFundingLease).Replace(' ORIGINAL ',$taskFundingOriginal).Replace(' FIXTURE_METHODS ',($taskFundingMethods -join "`n")).Replace(' FLOW_BODY ',$taskFundingFlowBody)
$taskFundingCode=$taskFundingCode.Replace('public AICF_VehiclePresetEconomyDecision InspectVehiclePresetPreflight(','public override AICF_VehiclePresetEconomyDecision InspectVehiclePresetPreflight(')
$taskFundingCode=$taskFundingCode.Replace('public override AICF_VehiclePresetEconomyDecision InspectVehiclePresetPreflight(AICF_TransportTrip trip','public override AICF_VehiclePresetEconomyDecision InspectVehiclePresetPreflight(AICF_TransportTrip trip')
$taskFundingCode=$taskFundingCode.Replace('!economy.IsVehiclePresetPreflightCurrent(trip, faction, decision)','!economy.IsVehiclePresetPreflightCurrent(trip, faction, decision)')
$taskFundingCode=$taskFundingCode.Replace('public int Calls,Obligations;','public int Calls,Obligations;public bool IsVehiclePresetPreflightCurrent(AICF_TransportTrip t,SCR_CampaignFaction f,AICF_VehiclePresetEconomyDecision d){return d.Status!=AICF_EVehiclePresetEconomyDecision.INVALID_CONTEXT&&d.Stock==d.Site.Stock&&d.Cost==Runtime.Price;}')
$taskFundingCode+=@'
public static class Cases{
static int n;static Economy E;static Flow F;static AICF_TransportTrip T;static AICF_FactionFleet Fleet;static SCR_CampaignFaction Side;static SCR_CampaignMilitaryBaseComponent Site;static AICF_MatchController C;
static void Need(bool v,string x){n++;if(!v)throw new Exception(x);}
static void Setup(){Cli.Mode="CAPACITY";Replication.Server=true;Runtime.Price=475;Runtime.MetadataReads=0;Runtime.MetadataHook=null;Runtime.Logs.Clear();E=new Economy();F=new Flow{Economy=E};Site=new SCR_CampaignMilitaryBaseComponent();Side=new SCR_CampaignFaction{Site=Site};var g=new SCR_AIGroup();for(int i=0;i<5;i++)g.Actors.Add(new Actor{Id=100+i});var slot=new AICF_GroupSlot{Group=g};T=new AICF_TransportTrip{PresetSlot=slot,Assignment=new AICF_StrategicAssignmentSnapshot{Group=g},Request=new AICF_VehicleRequestState{Plan=new AICF_VehicleSpawnPlan{Selection=new AICF_VehicleSpawnSiteSelection{m_Base=Site},Reservation=new AICF_VehicleSpawnSiteReservation{Site=Site}}}};Fleet=new AICF_FactionFleet();C=new AICF_MatchController{Slot=slot,FundingEconomy=E,m_VehicleCoordinator=new Coordinator{Trip=T,Registry=new AICF_FleetRegistry{Fleet=Fleet}}};var item=new AICF_MultiSquadCase();for(int i=0;i<5;i++)item.m_aMembers.Add(100+i);C.m_aAICFMultiCases.Add(item);AICF_MatchController.Active=C;}
static AICF_TripOutcome Call(){return F.InspectPresetEconomyBeforeLease(T,Fleet,Side,T.Request,T.Request.Plan,Runtime.Now);}
public static int Run(){n=0;Setup();Site.Stock=485;Site.Stock-=20;Cli.Mode="NORMAL";var old=Call();Need(old!=null&&old.Reason=="VEHICLE_PRESET_ECONOMY_DECLINED"&&E.Calls==1&&Site.Adds==0,"actual old485-20 declines475 BEFORE factory");
Setup();var result=Call();Need(result==null&&E.Calls==1&&Site.Stock==475&&E.m_iAICFMultiUSGrant==10&&Site.Adds==1,"bounded JIT10 before single original classifier");Need(AICF_VehiclePresetEconomyDecision.Last.Status==AICF_EVehiclePresetEconomyDecision.AFFORDABLE,"original actual status");Need(Runtime.Logs.Exists(s=>s.Contains("BEFORE_METADATA_SNAPSHOT")),"honest non-atomic helper quote label");Need(Runtime.MetadataReads==2,"one helper metadata quote +one authoritative original quote");
Setup();var actual=E.InspectVehiclePresetPreflight(T,Side,Site,Fleet);Need(object.ReferenceEquals(actual,AICF_VehiclePresetEconomyDecision.Last)&&E.Calls==1,"same decision reference returned");
Setup();var sentinel=new InvalidOperationException("ORIGINAL_INSPECT");Runtime.MetadataHook=()=>{if(Runtime.MetadataReads==2)throw sentinel;};Exception caught=null;try{E.InspectVehiclePresetPreflight(T,Side,Site,Fleet);}catch(Exception ex){caught=ex;}Need(object.ReferenceEquals(caught,sentinel)&&E.Calls==1&&E.m_AICFMultiPreflightFrame==null,"same original exception propagated after helper scope cleanup");
Setup();E.m_iAICFMultiUSGrant=4000;actual=E.InspectVehiclePresetPreflight(T,Side,Site,Fleet);Need(actual.Status==AICF_EVehiclePresetEconomyDecision.INSUFFICIENT_AVAILABLE_SUPPLIES&&Site.Adds==0&&E.Calls==1,"budget exhausted original negative retained");
Setup();Site.OnAdd=()=>Runtime.Price=525;actual=E.InspectVehiclePresetPreflight(T,Side,Site,Fleet);Need(actual.Cost==525&&actual.Stock==475&&actual.Status==AICF_EVehiclePresetEconomyDecision.INSUFFICIENT_AVAILABLE_SUPPLIES&&E.Calls==1,"helper475 original525 change stays negative no rewritten status");
foreach(string bad in new[]{"NORMAL","cohort","member","fleet","physical","generation","reservation","preset","requestCallback"}){Setup();if(bad=="NORMAL")Cli.Mode="NORMAL";if(bad=="cohort")C.m_iAICFMultiCohort=0;if(bad=="member")C.Slot.Group.Actors[0].Id=999;if(bad=="fleet")C.m_VehicleCoordinator.Registry.Fleet=new AICF_FactionFleet();if(bad=="physical"){T.Lease=new AICF_VehicleLease{Physical=true};Fleet.Lease=T.Lease;}if(bad=="generation")T.GroupGen=2;if(bad=="reservation")T.Request.Plan.Reservation.Current=false;if(bad=="preset")T.Current=false;if(bad=="requestCallback")Runtime.MetadataHook=()=>{Runtime.MetadataHook=null;T.Request=new AICF_VehicleRequestState{Plan=T.Request.Plan,Generation=2};};actual=E.InspectVehiclePresetPreflight(T,Side,Site,Fleet);Need(Site.Adds==0&&E.Calls==1&&actual.Status!=AICF_EVehiclePresetEconomyDecision.AFFORDABLE,"invalid current scope no funding "+bad);}
Setup();Site.OnAdd=()=>{Site.OnAdd=null;T.Current=false;};actual=E.InspectVehiclePresetPreflight(T,Side,Site,Fleet);Need(E.Calls==1&&object.ReferenceEquals(actual,AICF_VehiclePresetEconomyDecision.Last)&&E.m_AICFMultiPreflightFrame==null&&actual.Status==AICF_EVehiclePresetEconomyDecision.INVALID_CONTEXT,"grant callback native context failure remains actual negative and scope released");
Setup();Site.Capacity=470;actual=E.InspectVehiclePresetPreflight(T,Side,Site,Fleet);Need(Site.Stock==470&&E.m_iAICFMultiUSGrant==5&&actual.Status==AICF_EVehiclePresetEconomyDecision.INSUFFICIENT_AVAILABLE_SUPPLIES,"native capacity partial grant remains decline");
Setup();E.m_iAICFMultiUSGrant=3995;actual=E.InspectVehiclePresetPreflight(T,Side,Site,Fleet);Need(E.m_iAICFMultiUSGrant==4000&&Site.Stock==470&&actual.Status==AICF_EVehiclePresetEconomyDecision.INSUFFICIENT_AVAILABLE_SUPPLIES,"remaining5 finite budget clamp");
Setup();E.Obligations=20;actual=E.InspectVehiclePresetPreflight(T,Side,Site,Fleet);Need(Site.Stock==495&&actual.Obligations==20&&actual.Status==AICF_EVehiclePresetEconomyDecision.AFFORDABLE,"actual obligations remain charged");
Setup();E.InspectVehiclePresetPreflight(T,Side,Site,Fleet);Site.Stock-=20;actual=E.InspectVehiclePresetPreflight(T,Side,Site,Fleet);Need(E.Calls==2&&Site.Stock==475&&E.m_iAICFMultiUSGrant==30&&actual.Status==AICF_EVehiclePresetEconomyDecision.AFFORDABLE,"distinct actual invocation deficit funded within same4000 no request replacement");
return n;}}
}
'@
Add-Type -TypeDefinition $taskFundingCode -Language CSharp
$taskFundingCount=[FundingReplay.Cases]::Run()
$taskFundingUnsafe=@(
 @('noJit','AICF_MultiJitVehicleQuote(trip, faction, site, trip.GetVehiclePreset().m_sPrefab, "PREFLIGHT", fleet, frame);',''),
 @('double','return base.InspectVehiclePresetPreflight(trip, faction, site, fleet);','base.InspectVehiclePresetPreflight(trip, faction, site, fleet); return base.InspectVehiclePresetPreflight(trip, faction, site, fleet);'),
 @('return','return base.InspectVehiclePresetPreflight(trip, faction, site, fleet);','base.InspectVehiclePresetPreflight(trip, faction, site, fleet); return null;'),
 @('budget','4000 - AICF_MultiGranted(key)','4000000'),
 @('fleet','registry.FindFleet(faction.GetFactionKey()) != fleet','false'),
 @('scope','!AICF_MultiPreflightScopeCurrent(trip, quoteContext, quoteFrame)','false'),
 @('physical','return AICF_VehiclePresetEconomyDecision.IsOwnUnusedReservation(trip, fleet, lease, lease.GetLeaseGeneration());','return true;')
)
$taskFundingCaught=0
foreach($mutation in $taskFundingUnsafe){if(!$taskFundingCode.Contains($mutation[1])){throw ('Missing funding unsafe target '+$mutation[0])};$ns='FundingUnsafe'+$mutation[0];Add-Type -TypeDefinition ($taskFundingCode.Replace('FundingReplay',$ns).Replace($mutation[1],$mutation[2])) -Language CSharp;$caught=$false;try{[void]([type]($ns+'.Cases')).GetMethod('Run').Invoke($null,@())}catch{$caught=$true};if(!$caught){throw ('Funding unsafe survived '+$mutation[0])};$taskFundingCaught++}
"PASS Multi preflight funding composed_actual_cases=$taskFundingCount executed_unsafe=$taskFundingCaught original_actual_decision_return=1 production=UNCHANGED native=NOT_RUN"

exit 0

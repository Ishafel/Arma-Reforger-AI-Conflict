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
 $jit=Body $text 'AICF_MultiJitVehicleQuote';$quote=Body $text 'AICF_MultiOwnVehicleQuote';$purchase=Body $text 'PurchaseVehiclePreset';$spawn=Body $text 'SpawnCandidate'
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
  ([regex]::Matches($spawn,'super.SpawnCandidate\(')).Count -ne 1 -or $spawn -notmatch 'AICF_MultiJitVehicleQuote\(trip, faction, selection.m_Base, candidate.m_Prefab, "PRESPAWN"\)' -or
  $spawn.IndexOf('AICF_MultiJitVehicleQuote(') -gt $spawn.IndexOf('super.SpawnCandidate(') -or $purchase -match 'return true|paid\s*='){'JIT_BEFORE_PREFLIGHT_AND_NATIVE_DEBIT_NO_SUCCESS_BYPASS'}
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
 ,@('return super.SpawnCandidate(trip, faction, kind, candidates, selection, candidate, identity);','return true;','JIT_BEFORE_PREFLIGHT_AND_NATIVE_DEBIT_NO_SUCCESS_BYPASS')
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
"PASS Multi-squad fixture: real paid muster/leases/cohorts, NORMAL isolation, bounded CAPACITY funding, live final intent and startup/stop ordering; controls=$controls"
exit 0

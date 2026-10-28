param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$fixture=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'tests/fixtures/AICF_VehicleScenarioProbe.c'))
$budget=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'tests/fixtures/AICF_DismountOperationBudgetProbe.c'))
$runner=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'tools/Invoke-AICFVehicleScenarios.ps1'))
$auditor=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'tests/log-audits/Test-VehicleScenarioLog.ps1'))
function Body($s,$m){Get-AICFMethodBody ([pscustomobject]@{Source=$s;Code=(ConvertTo-AICFCodeText $s)}) $m}
function Rules($f,$b,$r,$a){
 foreach($part in @("[string]`$ReceiptPolicy='CURRENT_RETAINED'","if(`$DismountBudgetProbe){`$ReceiptPolicy='LEGACY_COMPLETE'}",'aicfVehicleScenarioCurrentBehavior','-RetainedBehavior:$taskRetained -ReceiptPolicy $ReceiptPolicy','aicfVehicleScenarioDismountBudgetAdapter')){if(!$r.Contains($part)){'EXPLICIT_CURRENT_DEFAULT_OR_CONTROLLED_BUDGET_LEGACY_BRANCH';break}}
 foreach($part in @('m_bVSTacticalPolicy = m_bVSRetained || m_bVSCurrentBehavior','m_bVSCurrentBehavior && (m_bVSRetained || m_sVSCase == "repeat5")','version=3 policy=CURRENT_RETAINED','if (m_bVSTacticalPolicy)','else if (trip.GetPhase() != AICF_ETransportTripPhase.COMPLETE)','TACTICAL_DEPLOYMENT || !AICF_VSRetainedFoot(slot)','trip.GetLastTransitionReason() != "DESTINATION_REACHED" || distance > m_Stage3Config.GetDismountDistanceMeters() + 10','VEHICLE_SCENARIO_ROSTER','if (farthest > 35) return;','m_iVSMoving >= 5 && m_iVSRecoveries == 0')){if(!$f.Contains($part)){'CURRENT_SINGLE_PHYSICAL_GEOMETRY_WITH_HISTORICAL_ORACLE_SEPARATE';break}}
 $adapter=Body $b 'Tick'
 $truce=Body $f 'AICF_VSVerifyTruce'
 foreach($part in @('manager.GetFactionsList(factions)','factions.Count() != m_iVSTruceFactionCount','second = first + 1','!a.IsFactionFriendly(b) || !b.IsFactionFriendly(a)')){if(!$truce.Contains($part)){'CURRENT_NATIVE_ALL_FACTIONS_TRUCE_READ_ONLY';break}}
 if((ConvertTo-AICFCodeText $truce) -match 'SetFactionsFriendly|SetThreat|SetHealth|EnableDamageHandling'){ 'NO_LATE_TRUCE_OR_THREAT_MUTATION' }
 foreach($part in @('AICF_TripOutcome outcome = super.Tick(trip, causationId)','!AICF_VSDismountBudgetProbe.Enabled()','aicfVehicleScenarioDismountBudgetAdapter','adapter != "1"','!probe.Matches(trip)','trip.GetPhase() != AICF_ETransportTripPhase.TRANSIT','outcome.GetKind() != AICF_ETripOutcomeKind.START_TACTICAL_DEPLOYMENT','outcome.GetReason() != "DESTINATION_REACHED"','trip.GetMovementState().GetRouteGeneration() <= 0','distance > m_Config.GetDismountDistanceMeters()','classification=CONTROLLED_DISMOUNT_BRANCH_ONLY','AICF_TripOutcome.StartDismount("CONTROLLED_BUDGET_DESTINATION_REACHED", outcome.GetCausationId())')){if(!$adapter.Contains($part)){'EXACT_CONTROLLED_PROBE_REAL_SOURCE_ARRIVAL_ADAPTER';break}}
 if((ConvertTo-AICFCodeText $adapter) -match '\b(?:Teleport|SetOrigin|SetHealth|SetLifeState|CommitTransition|SetPhase|RecordForceClearanceAttempt|RecordPhysicalRelocationAttempt|Set.*Deadline)\s*\('){'ADAPTER_NO_MOVEMENT_HEALTH_PHASE_OR_BUDGET_MUTATION'}
 foreach($part in @("[string]`$ReceiptPolicy='LEGACY_COMPLETE'",'EXPLICIT_CURRENT_POLICY_V3','CURRENT_EXACT_NATIVE_CLI','LEGACY_RECEIPT_POLICY','CONTROLLED_BUDGET_ACTUAL_SOURCE_ARRIVAL','HISTORICAL_BUDGET_NO_ADAPTER_MARKER')){if(!$a.Contains($part)){'STRICT_DISTINCT_CURRENT_HISTORICAL_ADAPTER_RECEIPT_POLICIES';break}}
}
$bad=@(Rules $fixture $budget $runner $auditor);if($bad.Count){throw "Current scenario policy source: $bad"}
$mutants=@('!AICF_VSDismountBudgetProbe.Enabled()','adapter != "1"','!probe.Matches(trip)','trip.GetPhase() != AICF_ETransportTripPhase.TRANSIT','outcome.GetKind() != AICF_ETripOutcomeKind.START_TACTICAL_DEPLOYMENT','outcome.GetReason() != "DESTINATION_REACHED"','trip.GetMovementState().GetRouteGeneration() <= 0','distance > m_Config.GetDismountDistanceMeters()','classification=CONTROLLED_DISMOUNT_BRANCH_ONLY')
foreach($m in $mutants){if(!$budget.Contains($m)){throw "Stale adapter mutant $m"};if(!@(Rules $fixture ($budget.Replace($m,'false')) $runner $auditor).Count){throw "Escaped adapter mutant $m"}}
"PASS Current scenario policy: explicit current default, historical strict audit, physical single-trip gates, $($mutants.Count) exact native-arrival adapter mutants; native proof REQUIRED separately"

param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
$auditor=Join-Path $RepositoryRoot 'tests/log-audits/Test-TransportDamageFallbackLog.ps1'
$fixture=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'tests/fixtures/AICF_TransportDamageFallbackProbe.c'))
$base=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'tests/fixtures/AICF_VehicleScenarioProbe.c'))
$runner=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'tools/Invoke-AICFTransportDamageFallback.ps1'))
foreach($needle in @('System.GetCLIParam("aicfTransportDamageFallbackProbe", enabled)',
 'trip.GetPhase() != AICF_ETransportTripPhase.TRANSIT','!AICF_VSExact(m_VSState.GetSlot(0))',
 'trip.GetLease().GetVehicle() != m_VSVehicle','m_VSVehicle.GetID() != m_VMDOriginalVehicleId',
 'damage.GetAllHitZonesInHierarchy(zones)','!wheel.IsProxy()','wheel.GetWheelIndex() >= 0',
 'damagedWheel.SetHealthScaled(0)','damage.UpdateVehicleState()',
 'damage.GetMovementDamage() >= 1','!SCR_AIVehicleUsability.VehicleCanMove(m_VSVehicle, damage)',
 'now - m_iVMDHoldAt < 30000','trip != m_VMDTrip','member.GetID() != m_aVSMembers[position]',
 'm_VSVehicles.RequestExplicitVehicleAdmission(slot, m_USSRFaction, "DAMAGE_FALLBACK_EXPLICIT_RETRY")',
 '!m_bVMDRecoveryServiced || slot.HasPendingOrderRecovery()')){
 if(!$fixture.Contains($needle)){throw "Damage fixture contract missing: $needle"}
}
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$code=ConvertTo-AICFCodeText $fixture
$accessorBody=Get-AICFMethodBody ([pscustomobject]@{Source=$fixture;Code=$code}) 'AICF_VMDReadLifecycleFailure'
if($accessorBody -notmatch 'latched = m_Acceptance.HasFailureLatch\(\)' -or $accessorBody -notmatch 'count = m_Acceptance.GetFailureCount\(\)' -or $accessorBody -match 'm_Acceptance\.(?!(?:HasFailureLatch|GetFailureCount)\(\))\w+\s*[=(]'){throw 'Lifecycle accessor must read actual acceptance state without mutation'}
foreach($needle in @('m_VSVehicles.AICF_VMDReadLifecycleFailure(lifecycleLatched, lifecycleCount)',
 'bool lifecycleClean = lifecycleRead && !lifecycleLatched && lifecycleCount == 0;',
 'AICF_VSCheck("DAMAGE_NO_FALSE_LIFECYCLE_FAILURE", lifecycleClean)',
 'lifecycle_failure_latched=%6 lifecycle_failure_count=%7','lifecycleLatched, lifecycleCount)',
 'if (!fallback || !lifecycleClean)')){if(!$fixture.Contains($needle)){throw "Missing actual lifecycle readback/gate: $needle"}}
$readBody=$accessorBody.Replace('!m_Acceptance','m_Acceptance == null')
$readAdapter=@'
public class AICF_DamageAcceptanceReadback{
 public class Monitor{public bool Latch;public int Count;public bool HasFailureLatch(){return Latch;}public int GetFailureCount(){return Count;}}
 public static class Replication{public static bool Server=true;public static bool IsServer(){return Server;}}
 public Monitor m_Acceptance=new Monitor();
 public bool Read(out bool latched,out int count) ACCESSOR_BODY
 public static string Evaluate(int scenario){Replication.Server=true;var owner=new AICF_DamageAcceptanceReadback();if(scenario==1)owner.m_Acceptance=null;if(scenario==2)Replication.Server=false;if(scenario==3){owner.m_Acceptance.Latch=true;owner.m_Acceptance.Count=3;}bool latch;int count;bool read=owner.Read(out latch,out count);return read+","+latch+","+count;}
}
'@
if(!('AICF_DamageAcceptanceReadback' -as [type])){Add-Type -TypeDefinition ($readAdapter.Replace('ACCESSOR_BODY',$readBody))}
$expectedReadback=@('True,False,0','False,True,-1','False,True,-1','True,True,3')
for($readCase=0;$readCase -lt 4;$readCase++){if([AICF_DamageAcceptanceReadback]::Evaluate($readCase) -ne $expectedReadback[$readCase]){throw "Actual read-only lifecycle accessor case=$readCase"}}
$retryBody=Get-AICFMethodBody ([pscustomobject]@{Source=$fixture;Code=$code}) 'AICF_VMDIsFailedExplicitRetry'
if(!$retryBody -or !$fixture.Contains('AICF_VSCheck("DAMAGE_EXPLICIT_RETRY_TERMINAL_BEFORE_PURCHASE", false)') -or
 $fixture.IndexOf('if (AICF_VMDIsFailedExplicitRetry(slot, trip))') -gt $fixture.IndexOf('if (trip == m_VMDTrip || !trip.GetLease()')){throw 'Exact terminal explicit retry must fail and finish before waiting for a missing asset'}
if($retryBody -match '\.(?:Set\w*|Request\w*|Clear\w*|Record\w*)\('){throw 'Retry failure classification must remain read-only'}
foreach($reference in @('m_VMDTrip','trip','m_USSRFaction','m_VSVehicles')){$retryBody=[regex]::Replace($retryBody,'!'+$reference+'(?=\s*[|)])',($reference+' == null'))}
$retryBody=[regex]::Replace($retryBody,'!trip\.GetAssignment\(\)(?!\.)','trip.GetAssignment() == null')
$retryBody=[regex]::Replace($retryBody,'!trip\.GetLease\(\)(?!\.)','trip.GetLease() == null')
$retryAdapter=@'
public class AICF_DamageRetryFailure{
 public class Slot{public int Id,Gen=1,Intent=2;public int GetSlotId(){return Id;}public int GetSpawnGeneration(){return Gen;}public int GetStrategicIntentRevision(){return Intent;}}
 public class Faction{public string GetFactionKey(){return "USSR";}}
 public class Lease{public bool Physical;public bool HasPhysicalAsset(){return Physical;}}
 public class Assignment{public object Group;public object GetGroup(){return Group;}}
 public class Trip{public Slot Slot;public Lease Lease=new Lease();public Assignment Assignment=new Assignment();public int Id,GroupGen=1,TripGen=2;public string Op="O2",Faction="USSR";public bool Terminal=true,Debited;public Slot GetVehiclePresetSlot(){return Slot;}public Assignment GetAssignment(){return Assignment;}public string GetFactionKey(){return Faction;}public int GetSlotId(){return Id;}public int GetGroupGeneration(){return GroupGen;}public int GetTripGeneration(){return TripGen;}public string GetOperationId(){return Op;}public bool IsTerminal(){return Terminal;}public Lease GetLease(){return Lease;}public bool IsVehiclePresetDebited(){return Debited;}}
 public class Coordinator{public Trip Current;public Trip GetTrip(Slot slot){return Current;}}
 public int m_iVMDStep=5,m_iVMDIntent=2;public string m_sVMDOperation="O1";public Trip m_VMDTrip=new Trip(){TripGen=1,Op="O1"};public Faction m_USSRFaction=new Faction();public Coordinator m_VSVehicles=new Coordinator();public object m_VSGroup=new object();public bool Own=true;public bool AICF_VSOwnRecoveryRoster(Slot slot){return Own&&slot!=null;}
 public bool Failed(Slot slot,Trip trip) RETRY_FAILURE_BODY
 public static bool Evaluate(int scenario){var owner=new AICF_DamageRetryFailure();var slot=new Slot();var trip=new Trip(){Slot=slot};trip.Assignment.Group=owner.m_VSGroup;owner.m_VSVehicles.Current=trip;
 switch(scenario){case 1:trip.Terminal=false;break;case 2:trip.Lease.Physical=true;trip.Debited=true;break;case 3:trip.Lease.Physical=true;break;case 4:trip.Lease=null;break;case 5:trip=owner.m_VMDTrip;break;case 6:trip.TripGen=3;break;case 7:trip.Faction="US";break;case 8:trip.GroupGen=2;break;case 9:trip.Assignment.Group=new object();break;case 10:trip.Id=1;break;case 11:owner.m_VSVehicles.Current=new Trip();break;case 12:trip.Slot=new Slot();break;case 13:slot.Intent=3;break;case 14:owner.Own=false;break;case 15:owner.m_iVMDStep=4;break;case 16:trip.Op="";break;case 17:trip.Op="O1";break;case 18:owner.m_VMDTrip=null;break;case 19:trip=null;break;case 20:trip.Assignment=null;break;case 21:owner.m_USSRFaction=null;break;case 22:owner.m_VSVehicles=null;break;case 23:slot=null;break;case 24:trip.Debited=true;break;}
 return owner.Failed(slot,trip);}
}
'@
if(!('AICF_DamageRetryFailure' -as [type])){Add-Type -TypeDefinition ($retryAdapter.Replace('RETRY_FAILURE_BODY',$retryBody))}
for($retryCase=0;$retryCase -le 24;$retryCase++){if([AICF_DamageRetryFailure]::Evaluate($retryCase) -ne ($retryCase -in 0,3,4,24)){throw "Actual terminal explicit retry control case=$retryCase"}}
if($code -match '\.SetMovementDamage\(|\.BeginCrewRecovery\(|\.CommitTransition\(|\.Record\w*\(|\.ClearPendingOrderRecovery\(|\.SetOrigin\(|\.Teleport\(|\.AddAction\(|CallLater|m_iCrewRecovery\w*\s*='){throw 'Fixture primes production actions, counters, state or positions'}
if($code -match 'ProcessPendingOrderRecovery\('){throw 'Causal observer must not service the same recovery twice'}
$pathBody=Get-AICFMethodBody ([pscustomobject]@{Source=$fixture;Code=$code}) 'AICF_VMDFootPath'
foreach($needle in @('movement.GetCurrentPath(points)','nav.IsTileLoaded(ahead)','nav.IsTileValid(ahead)',
 'groupMovement.GetAgentMoveHandlerId(agent)','if (handler >= 0)',
 'agent.GetMovementComponent() != movement','slot.GetStrategicAssignmentRevision() != assignment')){
 if(!$pathBody.Contains($needle)){throw "Missing read-only foot diagnostic fence: $needle"}
}
if((ConvertTo-AICFCodeText $pathBody) -match '\.(?:LoadTileIn|RequestFollowPath\w*|Set\w*|AddAction|Clear\w*|Remove\w*|Teleport)\('){throw 'Foot diagnostic must not request navmesh or change actions/position'}
foreach($needle in @('ProcessPendingOrderRecovery(m_VSState, slot, m_VSFaction)',
 'slot.GetPendingOrderRecoveryCause() != "VEHICLE_HANDOFF"','slot.IsPendingOrderRecoveryContextCurrent()',
 'm_Stage2Config.GetReliabilityIntervalMs() * ORDER_RECOVERY_STABLE_INTERVALS',
 'AICF_VSOnRecoveryVerified(slot, expected, stableMs, stablePolls, requiredStableMs)')){
 if(!$base.Contains($needle)){throw "Base scenario recovery owner missing: $needle"}
}
if($base -notmatch '(?s)protected bool AICF_VSAlternativeScenario\([^)]*\)\s*\{\s*return false;\s*\}' -or
 $base.IndexOf('if (AICF_VSAlternativeScenario(slot, trip)) return;') -gt $base.IndexOf('if (trip.IsTerminal() && trip.GetPhase()')){throw 'Normal scenario is not isolated from opt-in controlled flow'}
$baseUpdate=Get-AICFMethodBody ([pscustomobject]@{Source=$base;Code=(ConvertTo-AICFCodeText $base)}) 'Update'
if($base -notmatch '(?s)protected void AICF_VSServiceRecovery\([^)]*\)\s*\{\s*\}' -or
 $baseUpdate.IndexOf('AICF_VSServiceRecovery(slot);') -gt $baseUpdate.IndexOf('AICF_VSUpdateVehicles();')){throw 'Default neutral recovery hook must run before coordinator, including missing trip'}
foreach($needle in @('tools/Start-AICFRuntime.ps1','ArmaReforgerWorkbenchSteamDiag.exe','-Role Server -Variant Stock',
 "'-aicfTransportDamageFallbackProbe','1'",'source-hashes.json','Stage changed during damage fallback probe',
 'Test-TransportDamageFallbackLog.ps1')){if(!$runner.Contains($needle)){throw "Canonical isolated runner missing: $needle"}}
$work=Join-Path ([IO.Path]::GetTempPath()) ('aicf-damage-log-'+[guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($work)
$argv=@('-gproj','source','-profile','profile','-addonsDir','addons','-addons','CORE,MAP','-aicfVehicleScenario','uaz5','-aicfTransportDamageFallbackProbe','1')
[IO.File]::WriteAllText((Join-Path $work 'launcher.txt'),'AICF_RUNTIME_MANIFEST_JSON='+(@{arguments=$argv}|ConvertTo-Json -Compress))
[IO.File]::WriteAllText((Join-Path $work 'exit.txt'),'0')
$lines=[Collections.Generic.List[string]]::new()
$lines.Add('CLI Params: '+($argv -join ' '));$lines.Add('[ROSTER_READY]')
$lines.Add('[VEHICLE_SCENARIO_ENV] scenario=uaz5 combat=0 combat_mode=HOLD_FIRE damage_protection=1 truce=1')
$rules=@('GROUP_CREATED','INITIAL_COHESION_POLICY','EXACT_RECIPE','INITIAL_ORDER','DAMAGE_EXACT_INITIAL_BOARDING','DAMAGE_MOUNTED_HOLD','DAMAGE_REMOTE_ORDER','DAMAGE_NATIVE_WHEEL_INJECTION','DAMAGE_TYPED_FALLBACK','DAMAGE_NO_FALSE_LIFECYCLE_FAILURE','DAMAGE_REAL_OWNER_RECOVERY_VERIFIED','DAMAGE_ORIGINAL_FOOT_ARRIVAL','DAMAGE_NO_REPURCHASE','DAMAGE_EXPLICIT_RETRY','DAMAGE_NEW_EXPLICIT_OPERATION_AND_DEBIT')
$lines.Add('[VEHICLE_SCENARIO_COHESION] scenario=uaz5 group=G formation=Column displacement=0 native_readback=1 identity_current=1 setup_parity_only=1 causal_foot_fix_proven=0')
foreach($rule in $rules){$lines.Add('[VEHICLE_SCENARIO_CHECK] scenario=uaz5 leg=1 rule='+$rule+' passed=1')}
$lines.Add('[VEHICLE_PRESET_PURCHASE] operation=O1 prefab=UAZ supplies=125')
for($i=0;$i -lt 5;$i++){$lines.Add("[VEHICLE_PRESET_OCCUPANT] operation=O1 revision=1 position=$i seat=0:$i`:0 member=M$i occupant=M$i")}
function Event([string]$name,[string]$details){'[TRANSPORT_DAMAGE_PROBE] event='+$name+' operation_id=O1 t_ms=1 intent_revision=2 classification=CONTROLLED_FAULT_INJECTION normal_arrival_proof=0 '+$details}
$lines.Add((Event 'BOARDED' 'vehicle=V1 group=G exact5=1 debited=1'))
for($i=0;$i -lt 5;$i++){$lines.Add("[TRANSPORT_DAMAGE_MEMBER] event=BOARD position=$i member=M$i")}
$lines.Add((Event 'MOVING' 'vehicle=V1 displacement_m=20 moving_polls=5 exact5=1'))
$lines.Add((Event 'INJECTED' 'vehicle=V1 wheels=4 destroyed_wheels=4 movement_damage=1 can_move=0 crew_recoveries=0 alive=5 exact5_before=1 exact5_after=1'))
$lines.Add('[VEHICLE_STATE_CHANGED] operation_id=O1 reason=VEHICLE_MOVEMENT_DESTROYED previous_state=TRANSIT state=FALLBACK')
$lines.Add((Event 'FALLBACK' 'reason=VEHICLE_MOVEMENT_DESTROYED phase=FALLBACK latency_ms=1000 crew_recoveries=0 order_restored=1 lifecycle_failure_latched=0 lifecycle_failure_count=0'))
$lines.Add('[ORDER_BINDING_STABLE] faction=USSR slot=0 cause=VEHICLE_HANDOFF target=NONE waypoint=WP stable_polls=12 stable_ms=11000')
$lines.Add((Event 'RECOVERY_VERIFIED' 'waypoint_id=WP stable_ms=11000 stable_polls=11 required_stable_ms=10000 pending_after=0 owner=ProcessPendingOrderRecovery direct_clear=0'))
$lines.Add((Event 'FOOT' 'held_ms=40000 outside5=1 farthest_m=20 alive=5 order_restored=1 same_operation=1'))
for($i=0;$i -lt 5;$i++){$lines.Add("[TRANSPORT_DAMAGE_MEMBER] event=FOOT position=$i member=M$i")}
$lines.Add((Event 'EXPLICIT' 'accepted=1 unchanged_intent=1'))
$lines.Add('[VEHICLE_PRESET_PURCHASE] operation=O2 prefab=UAZ supplies=125')
$lines.Add((Event 'RETRY_PURCHASED' 'new_operation=O2 vehicle=V2 different_vehicle=1 debited=1 original_roster5=1 unchanged_intent=1 injection_count=1'))
$lines.Add((Event 'DONE' ('checks='+$rules.Count+' failures=0 injected=1 step=6')))
$lines.Add('[VEHICLE_SCENARIO_DONE] scenario=uaz5 checks='+$rules.Count+' failures=0 legs=1')
$lines.Add('Game destroyed.')
$good=$lines -join "`n"
$controls=0
function Verify([string]$label,[string]$body,[bool]$passes){
 [IO.File]::WriteAllText((Join-Path $work 'console.log'),$body)
 $output=@(& powershell.exe -NoProfile -ExecutionPolicy Bypass -File $auditor -LogPath (Join-Path $work 'console.log') -LauncherPath (Join-Path $work 'launcher.txt') -NativeExitPath (Join-Path $work 'exit.txt') 2>&1)
 $code=$LASTEXITCODE
 if(($passes -and $code -ne 0) -or (!$passes -and $code -ne 1)){throw "Damage log control failed $label exit=$code output=$output"}
 $script:controls++
}
try{
 Verify 'positive exact native proof' $good $true
 Verify 'terminal explicit retry failure cannot pass' ($good.Replace((Event 'RETRY_PURCHASED' 'new_operation=O2 vehicle=V2 different_vehicle=1 debited=1 original_roster5=1 unchanged_intent=1 injection_count=1'),(Event 'RETRY_FAILED' 'new_operation=O2 reason=SPAWN_SITE_APPROACH_TIMEOUT phase=FALLBACK physical=0 debited=0 attempted=1'))+"`n[VEHICLE_SCENARIO_CHECK] scenario=uaz5 leg=1 rule=DAMAGE_EXPLICIT_RETRY_TERMINAL_BEFORE_PURCHASE passed=0") $false
 Verify 'missing lifecycle case' ($good.Replace('[VEHICLE_SCENARIO_CHECK] scenario=uaz5 leg=1 rule=DAMAGE_NO_FALSE_LIFECYCLE_FAILURE passed=1', '')) $false
 Verify 'missing lifecycle latch field' ($good.Replace(' lifecycle_failure_latched=0','')) $false
 Verify 'false lifecycle latch readback' ($good.Replace('lifecycle_failure_latched=0','lifecycle_failure_latched=1')) $false
 Verify 'missing lifecycle failure count' ($good.Replace(' lifecycle_failure_count=0','')) $false
 Verify 'nonzero lifecycle failure count' ($good.Replace('lifecycle_failure_count=0','lifecycle_failure_count=1')) $false
 Verify 'exact operation damage lifecycle latch' ($good+"`n[ACCEPTANCE_FAILURE_LATCHED] run=native t_ms=120 reason=VEHICLE_MOVEMENT_DESTROYED domain=RECOVERY operation_id=O1 count=1") $false
 Verify 'foreign operation damage lifecycle latch' ($good+"`n[ACCEPTANCE_FAILURE_LATCHED] run=native t_ms=120 reason=VEHICLE_MOVEMENT_DESTROYED domain=RECOVERY operation_id=O10 count=1") $true
 Verify 'different reason lifecycle latch' ($good+"`n[ACCEPTANCE_FAILURE_LATCHED] run=native t_ms=120 reason=OTHER domain=RECOVERY operation_id=O1 count=1") $true
 $nativePrefix=$good.Replace('[VEHICLE_PRESET_PURCHASE] operation=', '[VEHICLE_PRESET_PURCHASE] run=stage1-server-13805 t_ms=19177 operation=').Replace('[VEHICLE_PRESET_OCCUPANT] operation=', '[VEHICLE_PRESET_OCCUPANT] run=stage1-server-13805 t_ms=53416 operation=')
 Verify 'native logger run and tick prefix' $nativePrefix $true
 Verify 'native prefix foreign original operation' ($nativePrefix.Replace('operation=O1 revision=', 'operation=O10 revision=')) $false
 Verify 'native prefix foreign retry operation' ($nativePrefix.Replace('operation=O2 prefab=', 'operation=O20 prefab=')) $false
 Verify 'native prefix wrong occupant' ($nativePrefix.Replace('member=M1 occupant=M1', 'member=M1 occupant=M9')) $false
 Verify 'native prefix unchanged intent violation' ($nativePrefix.Replace('unchanged_intent=1', 'unchanged_intent=0')) $false
 Verify 'native prefix duplicate original purchase' ($nativePrefix+"`n[VEHICLE_PRESET_PURCHASE] run=stage1-server-13805 t_ms=19178 operation=O1 prefab=UAZ supplies=125") $false
 Verify 'native prefix duplicate retry purchase' ($nativePrefix+"`n[VEHICLE_PRESET_PURCHASE] run=stage1-server-13805 t_ms=450604 operation=O2 prefab=UAZ supplies=125") $false
 Verify 'native prefix duplicate occupant' ($nativePrefix.Replace('[TRANSPORT_DAMAGE_PROBE] event=BOARDED ', "[VEHICLE_PRESET_OCCUPANT] run=stage1-server-13805 t_ms=53417 operation=O1 revision=1 position=0 seat=0:0:0 member=M0 occupant=M0`n[TRANSPORT_DAMAGE_PROBE] event=BOARDED ")) $false
 Verify 'native prefix cannot cross event newline' ($nativePrefix.Replace('[VEHICLE_PRESET_PURCHASE] run=stage1-server-13805 t_ms=19177 operation=O2', "[VEHICLE_PRESET_PURCHASE] run=stage1-server-13805 t_ms=19177`noperation=O2")) $false
 foreach($change in @(
  @('Game destroyed.','stopped?'),@('passed=1','passed=0'),@('movement_damage=1','movement_damage=0.999'),
  @('can_move=0','can_move=1'),@('destroyed_wheels=4','destroyed_wheels=3'),@('exact5_before=1','exact5_before=0'),
  @('crew_recoveries=0','crew_recoveries=1'),@('held_ms=40000','held_ms=29999'),@('farthest_m=20','farthest_m=36'),
  @('outside5=1','outside5=0'),@('latency_ms=1000','latency_ms=10001'),@('original_roster5=1','original_roster5=0'),
  @('unchanged_intent=1','unchanged_intent=0'),@('new_operation=O2','new_operation=O1'),@('vehicle=V2','vehicle=V1'),
  @('member=M1 occupant=M1','member=M1 occupant=M9'),@('event=FOOT position=1 member=M1','event=FOOT position=1 member=M9'),
  @('operation=O2 prefab=UAZ supplies=125','operation=O2 prefab=OTHER supplies=125'),
  @('event=FOOT operation_id=O1','event=FOOT operation_id=O9'),@('event=FOOT operation_id=O1 t_ms=1 intent_revision=2','event=FOOT operation_id=O1 t_ms=1 intent_revision=3'),
  @('normal_arrival_proof=0','normal_arrival_proof=1'),@('classification=CONTROLLED_FAULT_INJECTION','classification=NORMAL'),
  @('step=6','step=5'),@('injection_count=1','injection_count=2'),@('displacement_m=20','displacement_m=14'),
  @('[VEHICLE_PRESET_PURCHASE] operation=O2 prefab=UAZ supplies=125','[REMOVED_PURCHASE] operation=O2 prefab=UAZ supplies=125'),
  @('direct_clear=0','direct_clear=1'),@('owner=ProcessPendingOrderRecovery','owner=Fixture'),
  @('stable_ms=11000','stable_ms=9999'),@('required_stable_ms=10000','required_stable_ms=1'),
  @('pending_after=0','pending_after=1'),@('cause=VEHICLE_HANDOFF','cause=OTHER'),
  @('waypoint_id=WP','waypoint_id=STALE'),@('[ORDER_BINDING_STABLE]','[OMITTED_BINDING]'),
  @('formation=Column','formation=Wedge'),@('native_readback=1','native_readback=0')
 )){Verify $change[0] ($good.Replace($change[0],$change[1])) $false}
 Verify 'crew side effect after damage' ($good.Replace((Event 'FALLBACK' 'reason=VEHICLE_MOVEMENT_DESTROYED phase=FALLBACK latency_ms=1000 crew_recoveries=0 order_restored=1 lifecycle_failure_latched=0 lifecycle_failure_count=0'),"[VEHICLE_RECOVERY_STARTED] operation_id=O1 role=PILOT`n"+(Event 'FALLBACK' 'reason=VEHICLE_MOVEMENT_DESTROYED phase=FALLBACK latency_ms=1000 crew_recoveries=0 order_restored=1 lifecycle_failure_latched=0 lifecycle_failure_count=0'))) $false
 Verify 'premature repurchase before explicit' ($good.Replace((Event 'FOOT' 'held_ms=40000 outside5=1 farthest_m=20 alive=5 order_restored=1 same_operation=1'),"[VEHICLE_SPAWNED] faction=USSR slot=A0 numeric_slot=0 operation_id=O3`n"+(Event 'FOOT' 'held_ms=40000 outside5=1 farthest_m=20 alive=5 order_restored=1 same_operation=1'))) $false
 Verify 'duplicate injection receipt' ($good.Replace((Event 'INJECTED' 'vehicle=V1 wheels=4 destroyed_wheels=4 movement_damage=1 can_move=0 crew_recoveries=0 alive=5 exact5_before=1 exact5_after=1'),((Event 'INJECTED' 'vehicle=V1 wheels=4 destroyed_wheels=4 movement_damage=1 can_move=0 crew_recoveries=0 alive=5 exact5_before=1 exact5_after=1')+"`n"+(Event 'INJECTED' 'vehicle=V1 wheels=4 destroyed_wheels=4 movement_damage=1 can_move=0 crew_recoveries=0 alive=5 exact5_before=1 exact5_after=1')))) $false
 Verify 'full sibling fatal' ($good+"`nSCRIPT (E): unrelated exception") $false
 [IO.File]::WriteAllText((Join-Path $work 'script.log'),'SCRIPT (E): sibling failure')
 Verify 'sibling fatal separate from console' $good $false
 Remove-Item -LiteralPath (Join-Path $work 'script.log')
}
finally{
 $resolved=[IO.Path]::GetFullPath($work)
 $expected=[IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\')+'\'
 if($resolved.StartsWith($expected,[StringComparison]::OrdinalIgnoreCase) -and (Split-Path -Leaf $resolved).StartsWith('aicf-damage-log-')){Remove-Item -LiteralPath $resolved -Recurse -Force}
}
"PASS Transport damage fallback: actual lifecycle readback controls=4, exact terminal retry controls=25, native wheel fixture isolation, full stopped-log controls=$controls; strict native resource gate remains separate"

param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$path=Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Vehicles/AICF_VehicleNativeRouteProgress.c'
$source=[IO.File]::ReadAllText($path)
$flow=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Vehicles/AICF_VehicleTransitFlow.c'))
$phase=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/State/Vehicles/AICF_VehiclePhaseStates.c'))
$movement=[regex]::Match($phase,'(?s)class AICF_VehicleMovementState\b.*?(?=\r?\nclass |\z)').Value
function Body([string]$text,[string]$name){Get-AICFMethodBody ([pscustomobject]@{Source=$text;Code=(ConvertTo-AICFCodeText $text)}) $name}
function Rules([string]$text){
 $observe=Body $text 'Observe';$read=Body $text 'ReadContext';$project=Body $text 'ProjectUnique';$segment=Body $text 'SegmentProjection';$validate=Body $text 'ValidatePath';$same=Body $text 'SamePath';$base=Body $text 'Baseline';$invalid=Body $text 'Invalidate';$plausible=Body $text 'PlausibleStep';$finite=Body $text 'FiniteNumber'
 if(!$observe -or !$read -or !$project -or !$segment -or !$validate -or !$same -or !$base -or !$invalid -or !$plausible -or !$finite){'METHODS';return}
 $code=ConvertTo-AICFCodeText $text
 if($text -match 'AICF_GroupTrace|AICF_Stage\w*Diagnostics|NATIVE_VEHICLE_PATH_TRACE' -or
  $code -match '\b(?:SetOrigin|Teleport|SetVelocity|SetAngularVelocity|SetCruiseSpeed|SetCurrentAction|AddAction|AddWaypoint|DeleteRplEntity|SpawnEntityPrefabEx|CallLater|ObserveRouteProgress|MarkRouteProgress|CommitTransition|CommitRetarget)\s*\(' -or
  $code -match '(?:trip|movement|vehicle|car|driver|slot|lease)\.\w+\s*=' -or
  $code -match '(?:protected|static)\s+(?:ref\s+)?(?:Vehicle|IEntity|Physics|SCR_AIGroup|AIAgent|AIWaypoint|AICarMovementComponent|AICF_TransportTrip)\s+m_'){'NO_EXTERNAL_WRITES_DIAGNOSTICS_OR_ENGINE_RETENTION'}
 foreach($call in [regex]::Matches($code,'\b(?:trip|movement|vehicle|car|driver|slot|lease)\.(\w+)\s*\(')){
  if($call.Groups[1].Value -notmatch '^(?:Get|Is|Has|Matches|Find|ToString)'){'NO_EXTERNAL_WRITES_DIAGNOSTICS_OR_ENGINE_RETENTION';break}
 }
 foreach($guard in @('!Replication.IsServer()','!trip.IsValid()','trip.IsTerminal()','trip.GetPhase() != AICF_ETransportTripPhase.TRANSIT','trip.GetVehiclePresetSlot()','!trip.IsVehiclePresetCurrent()',
  'slot.GetGroup() != group','assignment.GetGroupGeneration() != slot.GetSpawnGeneration()','assignment.GetAssignmentRevision() != slot.GetStrategicAssignmentRevision()','assignment.GetStrategicIntentRevision() != slot.GetStrategicIntentRevision()',
  'assignment.GetTargetPosition() != slot.GetTargetPosition()','assignment.GetTargetBase() != slot.GetTargetBase()','lease.GetState() != AICF_EVehicleLeaseState.ACTIVE',
  'lease.MatchesTripIdentity(trip.GetFactionKey(), trip.GetSlotId(), trip.GetGroupGeneration(), trip.GetTripGeneration())','lease.MatchesEntityIdentity(vehicle, vehicle.GetID(), vehicleRpl.Id().ToString())',
  '!movement.IsRouteWaypointBound()','movement.GetRouteGeneration() <= 0','movement.GetAssignmentRevision() != assignment.GetAssignmentRevision()',
  'movement.GetCrewRecoveryToken()','movement.IsCrewRecoveryRoutePending()',
  'group.GetCurrentWaypoint() != waypoint','!queue.Contains(waypoint)','utility.m_Owner != group','move.m_Utility != utility','move.m_RelatedWaypoint != waypoint','move.GetRemoveAction()','move.GetActionIsSuspended()')){
  if(!$read.Contains($guard)){'EXACT_CURRENT_TRIP_ROUTE_CONTEXT';break}
 }
 if($read.Contains('movement.IsRecoveryEvidencePending()')){'RECOVERY_WITNESS_REMAINS_OBSERVABLE'}
 if(!$read.Contains('reason = "CURRENT_WAYPOINT_AUTHORITY";') -or !$read.Contains('reason = "CURRENT_CAR_MOVEMENT";') -or
  !$observe.Contains('if (!contextCurrent) return Invalidate("NO_EXACT_CURRENT_NATIVE_CAR_ROUTE_" + contextReason)') -or
  !$observe.Contains('if (!repeatedCurrent) return Invalidate("CONTEXT_CHANGED_DURING_READ_" + contextReason)')){'CONTEXT_REJECTION_REMAINS_DIAGNOSABLE'}
 if($read.Contains('IsAuthoritativeReplicatedEntity(waypoint)') -or $read -match '!\s*waypointRpl\s*(?:\)|\|\|)' -or
  ([regex]::Matches($read,'if \(waypointRpl\)')).Count -ne 2 -or
  ([regex]::Matches($read,'if \(!waypointRpl.IsMaster\(\) \|\| waypointRpl.IsProxy\(\)\) return false;')).Count -ne 2 -or
  !$read.Contains('string waypointRplId = "NONE";') -or !$read.Contains('string waypointComponentId = "NONE";') -or
  !$read.Contains('waypointRplState = 0;') -or !$read.Contains('waypointRplState = 1;')){'OPTIONAL_NATIVE_WAYPOINT_RPL_AUTHORITY'}
 foreach($guard in @('!waypoint || group.GetCurrentWaypoint() != waypoint','EntityID waypointId = waypoint.GetID();',
  'movement.GetRouteWaypoint() != waypoint || waypoint.GetID() != waypointId',
  'RplComponent.Cast(waypoint.FindComponent(RplComponent)) != waypointRpl',
  'waypointRpl.Id().ToString() != waypointRplId','waypointComponentId = waypointRpl.ToString();',
  'waypointId, waypointRplId, movement.GetRouteGeneration(), endpoint, move.ToString(), waypointComponentId',
  'IsAuthoritativeReplicatedEntity(group)','IsAuthoritativeReplicatedEntity(vehicle)','!groupRpl || !driverRpl',
  'IsAuthoritativeAIEntity(driver)')){
  if(!$read.Contains($guard)){'NATIVE_WAYPOINT_VALUE_IDENTITY_WITH_ACTOR_RPL_UNCHANGED';break}
 }
 if($read -match '(?m)^\s*scope\s*(?:\+)?=[^;]*(?:waypoint|waypointRplState)' -or
  !(Body $text 'GetDetails').Contains('native_waypoint_has_rpl=%1') -or
  ([regex]::Matches($observe,'m_iWaypointRplState = waypointRplState;')).Count -ne 3){'WAYPOINT_RPL_METADATA_OUTSIDE_REPLAY_SCOPE'}
 $details=Body $text 'GetDetails'
 if(!$details.Contains('opaqueScope.Replace("=", ":");') -or !$details.Contains('opaqueScope.Replace(" ", "|");') -or
  !$details.Contains('" native_route_scope=" + opaqueScope')){'OPAQUE_DETAILS_WITHOUT_DUPLICATE_IDENTITY_KEYS'}
 foreach($guard in @('registered.GetEntity() != vehicle','registered.GetDriver()','IsAuthoritativeAIEntity(driver)','SCR_AIDamageHandling.IsConscious(driver)',
  'driver.GetAIControlComponent().GetControlAIAgent()','driverAgent.GetControlledEntity() != driver','driverAgent.GetParentGroup() != group','!agents.Contains(driverAgent)',
  '!driver.IsInVehicle()','!access.IsInCompartment()','access.IsGettingIn()','access.IsGettingOut()','access.IsSwitchingSeatsAnim()',
  'PilotCompartmentSlot.Cast(access.GetCompartment())','pilot.GetVehicle() != vehicle','pilot.GetOccupant() != driver','CompartmentAccessComponent.GetVehicleIn(driver) != vehicle',
  'AICarMovementComponent.Cast(driverAgent.GetMovementComponent())','car.GetAIAgent().GetControlledEntity() != vehicle')){
  if(!$read.Contains($guard)){'EXACT_CURRENT_SETTLED_AI_PILOT_AND_NATIVE_CAR';break}
 }
 if(([regex]::Matches($observe,'ReadContext\(trip,')).Count -ne 2 -or
  !$observe.Contains('car.GetCurrentPath(points);') -or !$observe.Contains('car.GetCurrentPath(repeated);') -or
  !$observe.Contains('car != repeatedCar || vehicle != repeatedVehicle || scope != repeatedScope || routeIdentity != repeatedRoute') -or
  !$observe.Contains('endpoint != repeatedEndpoint || vehicle.GetPhysics() != physics || vector.Distance(vehicle.GetOrigin(), position) > 0.1') -or
  !$observe.Contains('if (!SamePath(points, repeated))') -or $observe.IndexOf('ReadContext(trip, repeatedCar') -gt $observe.IndexOf('ProjectUnique(points, position)')){'REPEATED_CONTEXT_AND_FULL_PATH_READ'}
 if(!$finite.Contains('value == value') -or !$finite.Contains('Math.AbsFloat(value) < 1000000') -or
  !$validate.Contains('points.Count() < 2 || points.Count() > MAXIMUM_POINTS') -or !$validate.Contains('foreach (vector point : points)') -or !$validate.Contains('if (!FinitePoint(point)) return false;') -or
  !$validate.Contains('vector.DistanceXZ(last, endpoint) <= ENDPOINT_XZ_TOLERANCE_METERS') -or !$validate.Contains('Math.AbsFloat(last[1] - endpoint[1]) <= ENDPOINT_HEIGHT_TOLERANCE_METERS') -or
  $text -notmatch 'MAXIMUM_POINTS = 512;' -or $text -notmatch 'ENDPOINT_XZ_TOLERANCE_METERS = 3;' -or $text -notmatch 'ENDPOINT_HEIGHT_TOLERANCE_METERS = 2;' -or
  $observe.IndexOf('ValidatePath(points, endpoint)') -gt $observe.IndexOf('ProjectUnique(points, position)')){'COMPLETE_FINITE_ENDPOINT_MATCHED_PATH'}
 foreach($expression in @('float lengthSq = dx * dx + dz * dz;','Math.Clamp(numerator / lengthSq, 0, 1)','vector projected = first + (last - first) * fraction;',
  'gap = vector.DistanceXZ(position, projected);','height = Math.AbsFloat(position[1] - projected[1]);','arc = prefix + Math.Sqrt(lengthSq) * fraction;')){
  if(!$segment.Contains($expression)){'NATIVE_ORDER_XZ_SEGMENT_ARC';break}
 }
 foreach($guard in @('height <= MAXIMUM_HEIGHT_METERS && gap < result.m_fGap','result.m_fGap > MAXIMUM_OFF_PATH_METERS',
  'otherHeight <= MAXIMUM_HEIGHT_METERS','Math.AbsFloat(otherArc - result.m_fArc) > NONLOCAL_ARC_METERS','otherGap <= result.m_fGap + AMBIGUITY_GAP_METERS','AMBIGUOUS_NONLOCAL_SEGMENT')){
  if(!$project.Contains($guard)){'UNIQUE_LOCAL_PROJECTION_ONLY';break}
 }
 foreach($limit in @('MAXIMUM_OFF_PATH_METERS = 8;','MAXIMUM_HEIGHT_METERS = 2;','NONLOCAL_ARC_METERS = 20;','AMBIGUITY_GAP_METERS = 2;')){
  if(!$text.Contains($limit)){'UNIQUE_LOCAL_PROJECTION_ONLY';break}
 }
 if(!$same.Contains('first.Count() != second.Count()') -or !$same.Contains('first[index] != second[index]') -or
  !$observe.Contains('SamePath(points, m_aRecords[index].m_aPoints)') -or !$observe.Contains('foreach (vector point : points) created.m_aPoints.Insert(point);') -or
  !$observe.Contains('m_aRecords.Count() >= MAXIMUM_PATH_VARIANTS') -or $text -notmatch 'MAXIMUM_PATH_VARIANTS = 4;' -or $code -match '\bHash\b|m_aRecords.Remove'){'FULL_ARRAY_HISTORY_WITHOUT_EVICTION'}
 if(!$observe.Contains('if (scope != m_sScope)') -or !$observe.Contains('routeIdentity != m_sRouteIdentity') -or
  $observe -notmatch '(?s)Invalidate\("BASELINE_ONLY"\);.*?Baseline\(record, projection, position, speed, nowMs, routeIdentity\);\s*return false;' -or
  !$base.Contains('record.m_fHighWaterArc = Math.Max(record.m_fHighWaterArc, projection.m_fArc);') -or !$base.Contains('record.m_fCreditAnchorArc = record.m_fHighWaterArc;') -or
  $invalid.Contains('m_aRecords.Clear()') -or $invalid.Contains('m_fHighWaterArc') -or !$invalid.Contains('record.m_bHasSample = false;')){'BASELINE_ONLY_REPLAN_WITH_REPLAY_FRONTIER'}
 foreach($guard in @('elapsedMs <= 0 || elapsedMs > MAXIMUM_SAMPLE_INTERVAL_MS','speed > MAXIMUM_SPEED_MPS || previousSpeed > MAXIMUM_SPEED_MPS',
  'Math.Max(previousSpeed, speed) * seconds * 1.5 + 1','displacement > velocityBudget','displacement > MAXIMUM_SPEED_MPS * seconds + 1',
  'arcDelta <= displacement * 2 + 1','arcDelta <= MAXIMUM_SPEED_MPS * seconds + 1')){if(!$plausible.Contains($guard)){'PHYSICS_TIME_ARC_PLAUSIBILITY';break}}
 if(!$text.Contains('MAXIMUM_SAMPLE_INTERVAL_MS = 5000;') -or !$text.Contains('MAXIMUM_SPEED_MPS = 80;') -or !$text.Contains('MINIMUM_SPEED_MPS = 0.5;')){'PHYSICS_TIME_ARC_PLAUSIBILITY'}
 if(!$observe.Contains('arcDelta < -0.5') -or !$observe.Contains('displacement >= 0.1 && Math.Max(speed, record.m_fSpeed) >= MINIMUM_SPEED_MPS && arcDelta > 0') -or
  !$observe.Contains('record.m_fHighWaterArc = Math.Max(record.m_fHighWaterArc, projection.m_fArc);') -or
  !$observe.Contains('float advance = record.m_fHighWaterArc - record.m_fCreditAnchorArc;') -or
  !$observe.Contains('physical && advance >= minimumAdvanceMeters && record.m_fPhysicalTravel >= minimumAdvanceMeters * 0.5') -or
  !$observe.Contains('return credited;')){'PHYSICAL_FORWARD_THRESHOLD_ONLY'}
}
$errors=@(Rules $source);if($errors.Count){throw "Native route progress contracts: $errors"}

function IntegrationRules([string]$flowSource,[string]$stateSource){
 $observe=Body $flowSource 'ObserveProgressAndRecover';$abort=Body $flowSource 'AbortOwnedActions';$credit=Body $stateSource 'CreditNativeRouteProgress';$getter=Body $stateSource 'GetNativeRouteProgress';$reset=Body $stateSource 'Reset'
 if(!$observe -or !$abort -or !$credit -or !$getter -or !$reset){'INTEGRATION_METHODS';return}
 $report=Body $flowSource 'ReportProgress'
 if(!$report.Contains('physicalMotion && trip.GetMovementState().MarkMotionReportDue(') -or
  !$report.Contains('MOTION_REPORT_INTERVAL_MS') -or !$report.Contains('if (routeProgress)') -or
  !$report.Contains('FormatIdentity(trip, "ROUTE_PROGRESS_CONFIRMED")') -or
  ([regex]::Matches($report,'trip.GetMovementState\(\).GetNativeRouteProgress\(\).GetDetails\(\)')).Count -ne 2){'INTEGRATION_BOUNDED_REJECTION_TELEMETRY'}
 $gated=[regex]::Match($observe,'(?s)if \(nativeRoute.Observe\(trip, nowMs, m_Config.GetProgressMeters\(\)\)\)\s*\{(.*?)\}').Groups[1].Value
 if(!$gated.Contains('state.CreditNativeRouteProgress(nowMs);') -or !$gated.Contains('routeProgress = true;') -or
  ([regex]::Matches((ConvertTo-AICFCodeText $flowSource),'\.CreditNativeRouteProgress\(')).Count -ne 1 -or
  $observe.IndexOf('nativeRoute.Observe(') -gt $observe.IndexOf('bool objectiveStalled')){'INTEGRATION_PROOF_GATED_CREDIT_BEFORE_TIMEOUT'}
 $writes=@([regex]::Matches((ConvertTo-AICFCodeText $credit),'\b(m_\w+)\s*(?:=|\+=|-=|\+\+|--)')|ForEach-Object{$_.Groups[1].Value})
 if($writes.Count -ne 2 -or @($writes|Where-Object{$_ -notin @('m_iLastRouteProgressAtMs','m_bRecoveryRouteEvidence')}).Count -or
  !$credit.Contains('m_iLastRouteProgressAtMs = nowMs;') -or $credit -notmatch '(?s)if \(m_bRecoveryEvidencePending\)\s*m_bRecoveryRouteEvidence = true;'){'INTEGRATION_ONLY_PROGRESS_CLOCK_AND_PENDING_ROUTE_EVIDENCE'}
 if(!$getter.Contains('if (!m_NativeRouteProgress)') -or !$getter.Contains('m_NativeRouteProgress = new AICF_VehicleNativeRouteProgress();') -or
  !$reset.Contains('ClearNativeRouteProgress();') -or !$abort.Contains('state.ClearNativeRouteProgress();') -or
  ([regex]::Matches((ConvertTo-AICFCodeText $stateSource),'\bClearNativeRouteProgress\(')).Count -ne 2 -or
  ([regex]::Matches((ConvertTo-AICFCodeText $flowSource),'\.ClearNativeRouteProgress\(')).Count -ne 1){'INTEGRATION_MOVEMENT_LIFECYCLE_ONLY_CLEAR'}
 foreach($entry in @(@($stateSource,'StageRouteWaypoint'),@($flowSource,'PrepareRoute'),@($flowSource,'CreateAndStageRoute'))){
  $body=Body $entry[0] $entry[1]
  if(!$body -or $body -match 'ClearNativeRouteProgress|GetNativeRouteProgress\(\)\.Reset|m_NativeRouteProgress\s*='){'INTEGRATION_REPLAN_RETAINS_REPLAY_HISTORY'}
 }
 if(!$observe.Contains('bool routeProgress = state.ObserveRouteProgress(') -or !$observe.Contains('bool physicalMotion = state.ObservePhysicalMotion(') -or
  !$observe.Contains('m_Config.GetProgressMeters()') -or !$observe.Contains('m_Config.GetMotionMeters()') -or
  !$observe.Contains('m_Config.GetObjectiveProgressTimeoutMs()') -or !$observe.Contains('m_Config.GetStuckTimeoutMs()') -or
  $observe.IndexOf('ObserveRouteProgress(') -gt $observe.IndexOf('nativeRoute.Observe(')){'INTEGRATION_EXISTING_DIRECT_MOTION_AND_DEADLINES_REMAIN'}
}
$integrationErrors=@(IntegrationRules $flow $movement);if($integrationErrors.Count){throw "Native route integration: $integrationErrors"}

# Выполняем непосредственно скалярные production выражения ограничителя шага.
$plausible=Body $source 'PlausibleStep'
$plausible=$plausible.Trim()
if($plausible.StartsWith('{') -and $plausible.EndsWith('}')){$plausible=$plausible.Substring(1,$plausible.Length-2)}
$plausible=[regex]::Replace($plausible,'if \((.*?)\) return false;','if ($1) { return $false }')
$plausible=$plausible.Replace('float ','').Replace('Math.Max','[Math]::Max').Replace('&&','-and').Replace('||','-or')
$plausible=$plausible.Replace('<=','-le').Replace('>=','-ge').Replace('>','-gt').Replace('<','-lt')
foreach($name in @('arcDelta','displacement','previousSpeed','speed','elapsedMs','seconds','velocityBudget')){
 $plausible=[regex]::Replace($plausible,'\b'+$name+'\b',[Text.RegularExpressions.MatchEvaluator]{param($m)'$'+$m.Value})
}
$interval=[int]([regex]::Match($source,'MAXIMUM_SAMPLE_INTERVAL_MS = (\d+);').Groups[1].Value)
$maxSpeed=[double]([regex]::Match($source,'MAXIMUM_SPEED_MPS = ([0-9.]+);').Groups[1].Value)
$plausible=$plausible.Replace('MAXIMUM_SAMPLE_INTERVAL_MS',"$interval").Replace('MAXIMUM_SPEED_MPS',"$maxSpeed")
$stepCheck=[scriptblock]::Create('param($arcDelta,$displacement,$previousSpeed,$speed,$elapsedMs) '+$plausible)
$stepCases=@(@(25,25,20,20,1000,$true),@(25,1,20,20,1000,$false),@(100,50,10,10,1000,$false),@(25,25,100,100,1000,$false),@(0,0,0,0,0,$false),@(25,25,20,20,5001,$false),@(250,200,40,40,5000,$true))
foreach($case in $stepCases){if([bool](& $stepCheck $case[0] $case[1] $case[2] $case[3] $case[4]) -ne $case[5]){throw "Physics/time/arc boundary failed: $case"}}

# Геометрический oracle: полилиния в native порядке, XZ projection с Y filter.
function Project-Oracle([object[]]$points,[double[]]$position){
 $segments=@();$prefix=0.0
 foreach($index in 0..($points.Count-2)){
  $a=$points[$index];$b=$points[$index+1];$dx=$b[0]-$a[0];$dz=$b[2]-$a[2];$sq=$dx*$dx+$dz*$dz
  if($sq -gt 0.0001){
   $fraction=[Math]::Max(0.0,[Math]::Min(1.0,(($position[0]-$a[0])*$dx+($position[2]-$a[2])*$dz)/$sq))
   $x=$a[0]+($b[0]-$a[0])*$fraction;$z=$a[2]+($b[2]-$a[2])*$fraction;$y=$a[1]+($b[1]-$a[1])*$fraction
   $gap=[Math]::Sqrt(($position[0]-$x)*($position[0]-$x)+($position[2]-$z)*($position[2]-$z));$height=[Math]::Abs($position[1]-$y)
   if($height -le 2){$segments+=@([pscustomobject]@{arc=$prefix+[Math]::Sqrt($sq)*$fraction;gap=$gap;index=$index})}
  }
  $prefix+=[Math]::Sqrt($sq)
 }
 $best=$segments|Sort-Object gap,index|Select-Object -First 1
 if(!$best -or $best.gap -gt 8){return [pscustomobject]@{valid=$false;reason='OFF_PATH_OR_HEIGHT'}}
 if(@($segments|Where-Object{[Math]::Abs($_.arc-$best.arc) -gt 20 -and $_.gap -le $best.gap+2}).Count){return [pscustomobject]@{valid=$false;reason='AMBIGUOUS'}}
 return [pscustomobject]@{valid=$true;arc=$best.arc;gap=$best.gap}
}
$detour=@(@(0,0,0),@(0,0,100),@(100,0,100),@(100,0,0))
$atStart=Project-Oracle $detour @(0,0,0);$away=Project-Oracle $detour @(0,0,30)
if(!$atStart.valid -or !$away.valid -or [Math]::Abs($away.arc-$atStart.arc-30) -gt 0.000001 -or [Math]::Sqrt(10000+900) -le 100){throw 'Road detour must advance along route while direct target distance grows'}
$geometryCases=@(
 @{path=$detour;pos=@(0,0,30);valid=$true},
 @{path=@(@(0,0,0),@(0,0,30),@(30,0,30));pos=@(0,0,30);valid=$true},
 @{path=@(@(-20,0,-20),@(20,0,20),@(-20,0,20),@(20,0,-20));pos=@(0,0,0);valid=$false},
 @{path=@(@(0,0,0),@(0,0,60),@(3,0,60),@(3,0,0));pos=@(1.5,0,30);valid=$false},
 @{path=@(@(0,0,0),@(0,0,100));pos=@(20,0,50);valid=$false},
 @{path=@(@(0,0,0),@(0,0,100));pos=@(0,3,50);valid=$false},
 @{path=@(@(0,0,0),@(0,0,0));pos=@(0,0,0);valid=$false},
 @{path=@(@(-20,0,0),@(20,0,0),@(20,10,20),@(-20,10,20),@(-20,10,0),@(20,10,0));pos=@(0,10,0);valid=$true}
)
foreach($case in $geometryCases){if((Project-Oracle $case.path $case.pos).valid -ne $case.valid){throw 'Unique local projection/height/ambiguity geometry failed'}}

# Counterexamples независимы от direct distance: frontier не растёт от oscillation,
# переиздания одного пути или A->B->A. Без реального нового forward frontier PASS нет.
$high=0.0;$anchor=0.0;$credits=0
foreach($arc in @(0,10,0,10,0,10,0,10)){$high=[Math]::Max($high,$arc);if($high-$anchor -ge 25){$credits++;$anchor=$high}}
if($credits){throw 'Oscillation minted forward progress'}
$oldFrontier=100.0;$replayedArc=70.0;$baseline=[Math]::Max($oldFrontier,$replayedArc)
if([Math]::Max($baseline,100)-$baseline -ge 25 -or [Math]::Max($baseline,124.999)-$baseline -ge 25 -or [Math]::Max($baseline,125)-$baseline -lt 25){throw 'Replayed-path baseline/frontier boundary failed'}

$controls=1
# Выполняем production rejection predicate под optional component gate.
# Отсутствие Rpl разрешено, но существующий proxy/nonmaster всегда отклоняется.
$read=Body $source 'ReadContext'
$predicate=[regex]::Match($read,'if \((!waypointRpl.IsMaster\(\) \|\| waypointRpl.IsProxy\(\))\) return false;').Groups[1].Value
if(!$predicate){throw 'Missing optional waypoint authority predicate'}
$predicate=$predicate.Replace('!waypointRpl.IsMaster()','-not $master').Replace('waypointRpl.IsProxy()','$proxy').Replace('||','-or')
$authorityCheck=[scriptblock]::Create('param($hasRpl,$master,$proxy) if ($hasRpl) { return -not ('+$predicate+') }; return $true')
foreach($hasRpl in @($false,$true)){foreach($master in @($false,$true)){foreach($proxy in @($false,$true)){
 $expected=(!$hasRpl -or ($master -and !$proxy))
 if([bool](& $authorityCheck $hasRpl $master $proxy) -ne $expected){throw 'Optional native waypoint Rpl authority truth table failed'}
 $controls++
}}}
foreach($mutation in @(
 @('slot.GetGroup() != group','false','EXACT_CURRENT_TRIP_ROUTE_CONTEXT'),
 @('assignment.GetAssignmentRevision() != slot.GetStrategicAssignmentRevision()','false','EXACT_CURRENT_TRIP_ROUTE_CONTEXT'),
 @('move.m_RelatedWaypoint != waypoint','false','EXACT_CURRENT_TRIP_ROUTE_CONTEXT'),
 @('if (movement.GetCrewRecoveryToken()','if (movement.IsRecoveryEvidencePending()) return false; if (movement.GetCrewRecoveryToken()','RECOVERY_WITNESS_REMAINS_OBSERVABLE'),
 @('registered.GetEntity() != vehicle','false','EXACT_CURRENT_SETTLED_AI_PILOT_AND_NATIVE_CAR'),
 @('pilot.GetOccupant() != driver','false','EXACT_CURRENT_SETTLED_AI_PILOT_AND_NATIVE_CAR'),
 @('car.GetAIAgent().GetControlledEntity() != vehicle','false','EXACT_CURRENT_SETTLED_AI_PILOT_AND_NATIVE_CAR'),
 @('if (!SamePath(points, repeated))','if (false)','REPEATED_CONTEXT_AND_FULL_PATH_READ'),
 @('scope != repeatedScope','false','REPEATED_CONTEXT_AND_FULL_PATH_READ'),
 @('MAXIMUM_POINTS = 512;','MAXIMUM_POINTS = 4096;','COMPLETE_FINITE_ENDPOINT_MATCHED_PATH'),
 @('value == value','true','COMPLETE_FINITE_ENDPOINT_MATCHED_PATH'),
 @('if (!FinitePoint(point)) return false;','if (false) return false;','COMPLETE_FINITE_ENDPOINT_MATCHED_PATH'),
 @('vector.DistanceXZ(last, endpoint) <= ENDPOINT_XZ_TOLERANCE_METERS','true','COMPLETE_FINITE_ENDPOINT_MATCHED_PATH'),
 @('Math.Clamp(numerator / lengthSq, 0, 1)','Math.Clamp(numerator / lengthSq, -1, 2)','NATIVE_ORDER_XZ_SEGMENT_ARC'),
 @('otherHeight <= MAXIMUM_HEIGHT_METERS','true','UNIQUE_LOCAL_PROJECTION_ONLY'),
 @('MAXIMUM_OFF_PATH_METERS = 8;','MAXIMUM_OFF_PATH_METERS = 800;','UNIQUE_LOCAL_PROJECTION_ONLY'),
 @('Math.AbsFloat(otherArc - result.m_fArc) > NONLOCAL_ARC_METERS','false','UNIQUE_LOCAL_PROJECTION_ONLY'),
 @('first[index] != second[index]','false','FULL_ARRAY_HISTORY_WITHOUT_EVICTION'),
 @('m_aRecords.Count() >= MAXIMUM_PATH_VARIANTS','false','FULL_ARRAY_HISTORY_WITHOUT_EVICTION'),
 @('record.m_fHighWaterArc = Math.Max(record.m_fHighWaterArc, projection.m_fArc);','record.m_fHighWaterArc = projection.m_fArc;','BASELINE_ONLY_REPLAN_WITH_REPLAY_FRONTIER'),
 @('displacement > velocityBudget','false','PHYSICS_TIME_ARC_PLAUSIBILITY'),
 @('arcDelta <= displacement * 2 + 1','true','PHYSICS_TIME_ARC_PLAUSIBILITY'),
 @('MINIMUM_SPEED_MPS = 0.5;','MINIMUM_SPEED_MPS = 0;','PHYSICS_TIME_ARC_PLAUSIBILITY'),
 @('arcDelta < -0.5','false','PHYSICAL_FORWARD_THRESHOLD_ONLY'),
 @('record.m_fPhysicalTravel >= minimumAdvanceMeters * 0.5','true','PHYSICAL_FORWARD_THRESHOLD_ONLY')
 ,@('opaqueScope.Replace("=", ":");',';','OPAQUE_DETAILS_WITHOUT_DUPLICATE_IDENTITY_KEYS')
 ,@('reason = "CURRENT_WAYPOINT_AUTHORITY";',';','CONTEXT_REJECTION_REMAINS_DIAGNOSABLE')
 ,@('if (waypointRpl)','if (!waypointRpl) return false; if (waypointRpl)','OPTIONAL_NATIVE_WAYPOINT_RPL_AUTHORITY')
 ,@('!waypointRpl.IsMaster() || waypointRpl.IsProxy()','false','OPTIONAL_NATIVE_WAYPOINT_RPL_AUTHORITY')
 ,@('waypointRplState = 0;','waypointRplState = 1;','OPTIONAL_NATIVE_WAYPOINT_RPL_AUTHORITY')
 ,@('movement.GetRouteWaypoint() != waypoint || waypoint.GetID() != waypointId','false','NATIVE_WAYPOINT_VALUE_IDENTITY_WITH_ACTOR_RPL_UNCHANGED')
 ,@('RplComponent.Cast(waypoint.FindComponent(RplComponent)) != waypointRpl','false','NATIVE_WAYPOINT_VALUE_IDENTITY_WITH_ACTOR_RPL_UNCHANGED')
 ,@('waypointRpl.Id().ToString() != waypointRplId','false','NATIVE_WAYPOINT_VALUE_IDENTITY_WITH_ACTOR_RPL_UNCHANGED')
 ,@('waypointComponentId = waypointRpl.ToString();','waypointComponentId = "NONE";','NATIVE_WAYPOINT_VALUE_IDENTITY_WITH_ACTOR_RPL_UNCHANGED')
 ,@('IsAuthoritativeReplicatedEntity(group)','IsAuthoritativeReplicatedEntity(null)','NATIVE_WAYPOINT_VALUE_IDENTITY_WITH_ACTOR_RPL_UNCHANGED')
 ,@('IsAuthoritativeReplicatedEntity(vehicle)','IsAuthoritativeReplicatedEntity(null)','NATIVE_WAYPOINT_VALUE_IDENTITY_WITH_ACTOR_RPL_UNCHANGED')
 ,@('!groupRpl || !driverRpl','!driverRpl','NATIVE_WAYPOINT_VALUE_IDENTITY_WITH_ACTOR_RPL_UNCHANGED')
 ,@('scope = string.Format("operation=','scope = "waypoint=" + waypointRplId; scope = string.Format("operation=','WAYPOINT_RPL_METADATA_OUTSIDE_REPLAY_SCOPE')
)){
 if(!$source.Contains($mutation[0])){throw "Mutation target absent: $($mutation[0])"}
 if($mutation[2] -notin @(Rules ($source.Replace($mutation[0],$mutation[1])))){throw "Mutation survived: $($mutation[2])"};$controls++
}
$observe=Body $source 'Observe'
if('NO_EXTERNAL_WRITES_DIAGNOSTICS_OR_ENGINE_RETENTION' -notin @(Rules ($source.Replace($observe,$observe.Insert(1,'vehicle.SetOrigin(position);'))))){throw 'External mutation survived'};$controls++
$invalid=Body $source 'Invalidate'
if('BASELINE_ONLY_REPLAN_WITH_REPLAY_FRONTIER' -notin @(Rules ($source.Replace($invalid,$invalid.Insert(1,'m_aRecords.Clear();'))))){throw 'Replay frontier deletion survived'};$controls++
$opaque='operation=trip-USSR-0-1-1 group=123 faction=USSR slot=0'.Replace('=',':').Replace(' ','|')
$row='faction=USSR slot=A0 native_route_scope='+$opaque
if(([regex]::Matches($row,'\bfaction=')).Count -ne 1 -or ([regex]::Matches($row,'\bslot=')).Count -ne 1){throw 'Scope details overwrote caller faction/role slot identity'}
foreach($mutation in @(
 @('nativeRoute.Observe(trip, nowMs, m_Config.GetProgressMeters())','true','INTEGRATION_PROOF_GATED_CREDIT_BEFORE_TIMEOUT'),
 @('bool routeProgress = state.ObserveRouteProgress(','state.CreditNativeRouteProgress(nowMs); bool routeProgress = state.ObserveRouteProgress(','INTEGRATION_PROOF_GATED_CREDIT_BEFORE_TIMEOUT'),
 @('state.ClearNativeRouteProgress();',';','INTEGRATION_MOVEMENT_LIFECYCLE_ONLY_CLEAR'),
 @(' + trip.GetMovementState().GetNativeRouteProgress().GetDetails()','','INTEGRATION_BOUNDED_REJECTION_TELEMETRY'),
 @('m_Config.GetObjectiveProgressTimeoutMs()','999999999','INTEGRATION_EXISTING_DIRECT_MOTION_AND_DEADLINES_REMAIN')
)){
 if(!$flow.Contains($mutation[0])){throw "Flow integration target absent: $($mutation[0])"}
 if($mutation[2] -notin @(IntegrationRules ($flow.Replace($mutation[0],$mutation[1])) $movement)){throw "Flow integration mutation survived: $($mutation[2])"};$controls++
}
$credit=Body $movement 'CreditNativeRouteProgress'
foreach($write in @('m_fBestRouteDistanceMeters = 0;','m_iAbsoluteDeadlineMs += 120000;','m_iCrewRecoveryAttempts = 0;')){
 if('INTEGRATION_ONLY_PROGRESS_CLOCK_AND_PENDING_ROUTE_EVIDENCE' -notin @(IntegrationRules $flow ($movement.Replace($credit,$credit.Insert(1,$write))))){throw 'Native credit wrote direct distance, deadline or recovery counter'};$controls++
}
if('INTEGRATION_MOVEMENT_LIFECYCLE_ONLY_CLEAR' -notin @(IntegrationRules $flow ($movement.Replace('if (!m_NativeRouteProgress)','if (true)')))){throw 'Recreated observer discarded path replay history'};$controls++
$stage=Body $movement 'StageRouteWaypoint'
if('INTEGRATION_REPLAN_RETAINS_REPLAY_HISTORY' -notin @(IntegrationRules $flow ($movement.Replace($stage,$stage.Insert(1,'ClearNativeRouteProgress();'))))){throw 'Staged route discarded replay history'};$controls++
"PASS Native route progress: full repeated current car path, exact pilot/lease/route, finite endpoint match, unique local arc projection, proof-gated integration with unchanged direct/motion/deadline contracts, bounded forward credit and retained replay frontier; controls=$controls step_cases=$($stepCases.Count) geometry_cases=$($geometryCases.Count) replay_cases=3"

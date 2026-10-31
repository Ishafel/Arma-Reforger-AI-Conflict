param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$core=Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict'
$source=[IO.File]::ReadAllText((Join-Path $core 'Vehicles/AICF_TransportNativeGate.c'))
$state=[IO.File]::ReadAllText((Join-Path $core 'State/Vehicles/AICF_VehiclePhaseStates.c'))
$flow=[IO.File]::ReadAllText((Join-Path $core 'Vehicles/AICF_VehicleTransitFlow.c'))
function Body([string]$text,[string]$method){Get-AICFMethodBody ([pscustomobject]@{Source=$text;Code=(ConvertTo-AICFCodeText $text)}) $method}
function Rules([string]$text,[string]$phase,[string]$transit){
 $scan=Body $text 'TryBegin';$begin=Body $text 'Begin';$matches=Body $text 'Matches';$returned=Body $text 'IsReturned';$resumed=Body $text 'HasResumed';$expired=Body $text 'IsExpired';$capture=Body $phase 'CaptureNativeGateWindowStart';$observe=Body $transit 'ObserveNativeGate'
 if(!$scan -or !$begin -or !$matches -or !$returned -or !$resumed -or !$capture -or !$observe){'METHODS';return}
 $payload=$begin+(Body $text 'IsEligiblePerform')
 if($scan -match 'ECompartmentType.PILOT|ECompartmentType.TURRET' -or
  !$scan.Contains('int position = trip.GetVehiclePreset().m_aMembers[index];') -or
  !$scan.Contains('if (position < 0) continue;') -or !$scan.Contains('trip.VehiclePresetMember(position)') -or
  !$scan.Contains('IsAuthoritativeAIEntity(member)') -or !$scan.Contains('MatchesSeats(seats)')){'ALL_EXACT_ASSIGNED_SEATS'}
 foreach($guard in @('!Live(perform)','perform.GetPriority() != SCR_AIActionBase.PRIORITY_BEHAVIOR_OPEN_NAVLINK_DOOR',
  'activity.m_Utility != trip.GetAssignment().GetGroup().GetGroupUtilityComponent()',
  'activity != activity.m_Utility.GetCurrentAction()',
  'move.m_RelatedWaypoint != trip.GetMovementState().GetRouteWaypoint()',
  'vector.Distance(seat.GetVehicle().GetOrigin(), smart.GetOwner().GetOrigin()) > 50',
  'tags.Contains(SCR_AIGoalReaction_OpenNavlinkDoor.SMART_ACTION_TAG)',
  'sibling.GetRelatedGroupActivity() != activity','getIn.m_CompartmentToGetIn.m_Value == seat',
  'getIn.m_Vehicle.m_Value == seat.GetOwner()','getOut.m_Vehicle.m_Value == seat.GetOwner()',
  'getOut.GetPriority() == perform.GetPriority() + 100',
  'utility.GetCurrentBehavior() != perform','utility.GetCurrentBehavior() != exitAction',
  'if (!gate.Matches(trip)) continue;')){if(!$payload.Contains($guard)){'PROVEN_NATIVE_OPEN_GATE_CHAIN';break}}
 if(!$begin.Contains('if (performCandidates != 1)') -or !$begin.Contains('if (returnCandidates != 1 || exitCandidates > 1)') -or
  !$begin.Contains('performCandidates++;') -or !$begin.Contains('returnCandidates++;') -or !$begin.Contains('exitCandidates++;') -or
  ([regex]::Matches($begin,'!IsEligiblePerform\(trip, seat,')).Count -ne 2 -or
  $begin.IndexOf('if (returnCandidates != 1 || exitCandidates > 1)') -gt $begin.IndexOf('CaptureNativeGateWindowStart(')){'UNIQUE_NATIVE_CHAIN_OR_FAIL_CLOSED'}
 foreach($guard in @('m_Member.GetID() != m_Id','m_Group.GetID() != m_GroupId','m_Vehicle.GetID() != m_VehicleId',
  'Replication.FindItemId(m_Vehicle).ToString() != m_sVehicleRpl','GetRouteGeneration() != m_iRouteGeneration',
  'GetAssignmentRevision() != m_iAssignmentRevision','GetStrategicIntentRevision() != m_iIntentRevision',
  'm_SmartOwner.GetID() != m_SmartOwnerId','m_SmartAction.GetOwner() != m_SmartOwner',
  'm_Return.m_CompartmentToGetIn.m_Value != m_Seat','m_Return.GetRelatedGroupActivity() != m_Activity',
  'm_Return.GetActionState() == EAIActionState.FAILED')){if(!$matches.Contains($guard)){'IMMUTABLE_CONTEXT_AND_EXACT_RETURN';break}}
 if($returned -notmatch '!character.IsInVehicle\(\) && !access.IsInCompartment\(\) && !CompartmentAccessComponent.GetVehicleIn\(character\).*?m_bObservedExit = true;' -or
  !$returned.Contains('!m_bObservedExit || !m_Return || !m_bReturnCompleted') -or
  !$returned.Contains('!access.IsGettingIn() && !access.IsGettingOut()') -or !$returned.Contains('access.IsSwitchingSeatsAnim()') -or
  !$returned.Contains('access.GetCompartment() == m_Seat && m_Seat.GetOccupant() == m_Member') -or
  !$returned.Contains('CompartmentAccessComponent.GetVehicleIn(character) != m_Vehicle')){'OBSERVED_EXIT_THEN_COMPLETED_PHYSICAL_RETURN'}
 if(!$returned.Contains('if (m_iReturnedAtMs <= 0)') -or !$returned.Contains('m_vReturnPosition = m_Vehicle.GetOrigin();') -or
  !$resumed.Contains('m_iReturnedAtMs <= 0') -or !$resumed.Contains('physics.GetVelocity().Length() < 0.5') -or
  !$resumed.Contains('vector.DistanceXZ(m_Vehicle.GetOrigin(), m_vReturnPosition) >= minimumMotionMeters')){'POST_RETURN_REAL_MOTION_WITHOUT_CLOCK_WRITE'}
 $key=[regex]::Match($begin,'(?s)string windowKey =.*?(?=gate.m_iStarted =)').Value
 foreach($part in @('m_sOperation','m_GroupId','m_iAssignmentRevision','m_iIntentRevision','m_iRouteGeneration','GetLeaseGeneration()','GetVehicleGeneration()','m_SmartOwnerId')){
  if(!$key.Contains($part)){'SHARED_GATE_WINDOW_IDENTITY';break}
 }
 if($key -match 'm_Member|m_Id\b|m_sSeatKey' -or !$key){'SHARED_GATE_WINDOW_IDENTITY'}
 if(!$capture.Contains('int index = m_aNativeGateWindowKeys.Find(key);') -or
  !$capture.Contains('return m_aNativeGateWindowStarts[index];') -or
  $capture.IndexOf('return m_aNativeGateWindowStarts[index];') -gt $capture.IndexOf('m_aNativeGateWindowStarts.Insert(nowMs);') -or
  !$capture.Contains('m_aNativeGateWindowKeys.Count() >= 32') -or
  $capture -match '\.Clear\(|\.Remove\(|m_aNativeGateWindowStarts\[index\]\s*='){'FIRST_SHARED_DEADLINE_NO_EVICTION_OR_RENEWAL'}
 if(!$begin.Contains('CaptureNativeGateWindowStart(windowKey, System.GetTickCount())') -or
  !$begin.Contains('Math.Min(gate.m_iStarted + 60000, trip.GetAbsoluteDeadlineMs())') -or
  !$expired.Contains('now - m_iStarted >= 60000 || now >= m_iDeadline') -or
  $observe.IndexOf('gate.IsExpired(nowMs)') -gt $observe.IndexOf('gate.IsReturned()') -or
  $observe.IndexOf('gate.IsReturned()') -gt $observe.IndexOf('gate.HasResumed(m_Config.GetMotionMeters())') -or
  $observe.IndexOf('gate.HasResumed(m_Config.GetMotionMeters())') -gt $observe.IndexOf('gate.Log("TRANSPORT_NATIVE_GATE_RETURNED")')){'SAME_ABSOLUTE_WINDOW_THROUGH_RESUME'}
 $code=ConvertTo-AICFCodeText $text
 if($code -match '\b(?:SetOrigin|Teleport|AddAction|Fail|Complete|SetReserved|CallLater|Record\w*Progress|Mark\w*Progress|Mark\w*Motion)\s*\(' -or
  (ConvertTo-AICFCodeText $observe) -match 'CreditNativeRouteProgress|MarkRouteProgress|ObservePhysicalMotion|DeadlineMs\('){'READ_ONLY_NATIVE_AND_PROGRESS'}
 if(!$text.Contains('smart_tag=%3') -or !$text.Contains('window_deadline_ms=%2') -or !$text.Contains('observed_exit=%3')){'BOUNDED_NATIVE_PAYLOAD_EVIDENCE'}
 $report=Body $text 'ReportCandidate';$cadence=Body $phase 'MarkNativeGateCandidateReportDue'
 if(!$report.Contains('MarkNativeGateCandidateReportDue(System.GetTickCount())') -or !$report.Contains('sampled_candidate_only=1 accepted=0') -or
  !$cadence.Contains('nowMs - m_iNativeGateCandidateReportAtMs < 10000')){'BOUNDED_AMBIGUOUS_CANDIDATE_REJECTION'}
 $movement=[regex]::Match($phase,'(?s)class AICF_VehicleMovementState\b.*?(?=\r?\nclass |\z)').Value
 $reset=Body $movement 'Reset'
 foreach($history in @('m_aNativeGateWindowKeys','m_aNativeGateWindowStarts')){
  if(!$reset.Contains($history+'.Clear();') -or ([regex]::Matches($movement,('\b'+$history+'\.Clear\(\)'))).Count -ne 1){'RECEIPT_LIFECYCLE_RESET_ONLY'}
 }
}
$issues=@(Rules $source $state $flow);if($issues.Count){throw "Native cargo gate contracts: $issues"}
$controls=1
foreach($mutation in @(
 @('g','int position = trip.GetVehiclePreset().m_aMembers[index];','if (seats[index].GetType() != ECompartmentType.PILOT) continue; int position = trip.GetVehiclePreset().m_aMembers[index];','ALL_EXACT_ASSIGNED_SEATS'),
 @('g','tags.Contains(SCR_AIGoalReaction_OpenNavlinkDoor.SMART_ACTION_TAG)','true','PROVEN_NATIVE_OPEN_GATE_CHAIN'),
 @('g','utility.GetCurrentBehavior() != exitAction','true','PROVEN_NATIVE_OPEN_GATE_CHAIN'),
 @('g','activity != activity.m_Utility.GetCurrentAction()','false','PROVEN_NATIVE_OPEN_GATE_CHAIN'),
 @('g','if (performCandidates != 1)','if (false)','UNIQUE_NATIVE_CHAIN_OR_FAIL_CLOSED'),
 @('g','returnCandidates != 1 || exitCandidates > 1','false','UNIQUE_NATIVE_CHAIN_OR_FAIL_CLOSED'),
 @('s','nowMs - m_iNativeGateCandidateReportAtMs < 10000','false','BOUNDED_AMBIGUOUS_CANDIDATE_REJECTION'),
 @('g','m_Return.GetRelatedGroupActivity() != m_Activity','false','IMMUTABLE_CONTEXT_AND_EXACT_RETURN'),
 @('g','m_SmartOwner.GetID() != m_SmartOwnerId','false','IMMUTABLE_CONTEXT_AND_EXACT_RETURN'),
 @('g','!m_bObservedExit || !m_Return || !m_bReturnCompleted','false','OBSERVED_EXIT_THEN_COMPLETED_PHYSICAL_RETURN'),
 @('g','!character.IsInVehicle() && !access.IsInCompartment() && !CompartmentAccessComponent.GetVehicleIn(character)','access.IsGettingOut()','OBSERVED_EXIT_THEN_COMPLETED_PHYSICAL_RETURN'),
 @('g','!m_bReturnCompleted','false','OBSERVED_EXIT_THEN_COMPLETED_PHYSICAL_RETURN'),
 @('g','if (m_iReturnedAtMs <= 0)','if (true)','POST_RETURN_REAL_MOTION_WITHOUT_CLOCK_WRITE'),
 @('g','vector.DistanceXZ(m_Vehicle.GetOrigin(), m_vReturnPosition) >= minimumMotionMeters','true','POST_RETURN_REAL_MOTION_WITHOUT_CLOCK_WRITE'),
 @('g','gate.m_sOperation, gate.m_GroupId,','gate.m_sOperation, gate.m_Id,','SHARED_GATE_WINDOW_IDENTITY'),
 @('s','return m_aNativeGateWindowStarts[index];','return nowMs;','FIRST_SHARED_DEADLINE_NO_EVICTION_OR_RENEWAL'),
 @('s','m_aNativeGateWindowKeys.Count() >= 32','false','FIRST_SHARED_DEADLINE_NO_EVICTION_OR_RENEWAL'),
 @('s','m_aNativeGateWindowKeys.Clear();',';','RECEIPT_LIFECYCLE_RESET_ONLY'),
 @('g','gate.m_iStarted + 60000','System.GetTickCount() + 60000','SAME_ABSOLUTE_WINDOW_THROUGH_RESUME'),
 @('f','gate.HasResumed(m_Config.GetMotionMeters())','true','SAME_ABSOLUTE_WINDOW_THROUGH_RESUME')
)){
 $g=$source;$s=$state;$f=$flow
 if($mutation[0] -eq 'g'){$g=$g.Replace($mutation[1],$mutation[2])}elseif($mutation[0] -eq 's'){$s=$s.Replace($mutation[1],$mutation[2])}else{$f=$f.Replace($mutation[1],$mutation[2])}
 if($g -ceq $source -and $s -ceq $state -and $f -ceq $flow){throw "Absent mutation target $($mutation[3])"}
 if($mutation[3] -notin @(Rules $g $s $f)){throw "Mutation survived $($mutation[3])"};$controls++
}
# Выполняем actual production receipt body; adapter меняет только array API на List.
$capture=Body $state 'CaptureNativeGateWindowStart'
$capture=$capture.Replace('.Find(key)','.IndexOf(key)').Replace('key.IsEmpty()','string.IsNullOrEmpty(key)').Replace('.Count()','.Count').Replace('.Insert(key)','.Add(key)').Replace('.Insert(nowMs)','.Add(nowMs)')
Add-Type -TypeDefinition ('using System; using System.Collections.Generic; public class AICF_NativeGateReceiptContract { public List<string> m_aNativeGateWindowKeys=new List<string>(); public List<int> m_aNativeGateWindowStarts=new List<int>(); public int Capture(string key,int nowMs) '+$capture+' }')
$receipt=New-Object AICF_NativeGateReceiptContract
foreach($case in @(@('gate-A',1000,1000),@('gate-A',20000,1000),@('gate-A',70000,1000),@('gate-B',90000,90000),@('',100000,-1),@('gate-C',0,-1))){if($receipt.Capture($case[0],$case[1]) -ne $case[2]){throw 'Actual shared receipt body renewed or accepted invalid window'};$controls++}
for($i=2;$i -lt 32;$i++){[void]$receipt.Capture('capacity-'+$i,100000+$i)}
if($receipt.Capture('new-over-cap',200000) -ne -1 -or $receipt.Capture('gate-A',300000) -ne 1000){throw 'Bounded receipt evicted a frontier or rejected an existing window'};$controls++
$expired=Body $source 'IsExpired'
Add-Type -TypeDefinition ('public class AICF_NativeGateExpiryContract { public int m_iStarted=1000; public int m_iDeadline=61000; public bool Expired(int now) '+$expired+' }')
$expiry=New-Object AICF_NativeGateExpiryContract
if($expiry.Expired(60999) -or !$expiry.Expired(61000)){throw 'Native gate 60s boundary changed'};$controls++
$expiry.m_iDeadline=50000;if(!$expiry.Expired(50000)){throw 'Trip absolute deadline was extended'};$controls++
$begin=Body $source 'Begin'
$performReject=[regex]::Match($begin,'if \((performCandidates != 1)\)').Groups[1].Value
$siblingReject=[regex]::Match($begin,'if \((returnCandidates != 1 \|\| exitCandidates > 1)\)').Groups[1].Value
Add-Type -TypeDefinition ('public static class AICF_NativeGateUniqueContract { public static bool Eligible(int performCandidates,int returnCandidates,int exitCandidates){if ('+$performReject+') return false; if ('+$siblingReject+') return false; return true;} }')
foreach($case in @(@(2,2,2,$false),@(2,1,1,$false),@(1,2,1,$false),@(1,1,2,$false),@(1,0,1,$false),@(0,1,1,$false),@(1,1,0,$true),@(1,1,1,$true))){
 if([AICF_NativeGateUniqueContract]::Eligible($case[0],$case[1],$case[2]) -ne $case[3]){throw 'Two native chains were cross-paired or unique valid chain rejected'};$controls++
}
# Actual production boolean kernels: still seated before GetOut is not a return.
$returned=Body $source 'IsReturned'
$exit=[regex]::Match($returned,'if \((!character.IsInVehicle\(\) && !access.IsInCompartment\(\) && !CompartmentAccessComponent.GetVehicleIn\(character\))\)').Groups[1].Value
$reject=[regex]::Match($returned,'if \((!m_bObservedExit \|\| !m_Return \|\| !m_bReturnCompleted)\)').Groups[1].Value
$settled=[regex]::Match($returned,'bool settled = ([^;]+);').Groups[1].Value
$seat=[regex]::Match($returned,'bool exactSeat = ([^;]+);').Groups[1].Value
foreach($pair in @(@('access.IsGettingOut()','gettingOut'),@('access.IsGettingIn()','gettingIn'),@('character.IsInVehicle()','inVehicle'),@('access.IsInCompartment()','inCompartment'))){$exit=$exit.Replace($pair[0],$pair[1]);$settled=$settled.Replace($pair[0],$pair[1])}
$exit=$exit.Replace('CompartmentAccessComponent.GetVehicleIn(character)','linkedVehicle')
$reject=$reject.Replace('m_bObservedExit','observedExit').Replace('m_bReturnCompleted','returnComplete').Replace('!m_Return','!hasReturn')
$seat=$seat.Replace('access.GetCompartment() == m_Seat','seatExact').Replace('m_Seat.GetOccupant() == m_Member','occupantExact')
Add-Type -TypeDefinition ('public static class AICF_NativeGateProtocolContract { public static bool ObserveExit(bool gettingOut,bool inVehicle,bool inCompartment,bool linkedVehicle){return '+$exit+';} public static bool Returned(bool observedExit,bool hasReturn,bool returnComplete,bool inVehicle,bool inCompartment,bool gettingIn,bool gettingOut,bool seatExact,bool occupantExact){if ('+$reject+') return false; bool settled='+$settled+'; bool exactSeat='+$seat+'; return settled && exactSeat;} }')
if([AICF_NativeGateProtocolContract]::ObserveExit($false,$true,$true,$true) -or [AICF_NativeGateProtocolContract]::ObserveExit($true,$true,$true,$true) -or ![AICF_NativeGateProtocolContract]::ObserveExit($false,$false,$false,$false) -or [AICF_NativeGateProtocolContract]::ObserveExit($false,$false,$false,$true)){throw 'Canceled GetOut was mistaken for actual unlinked physical exit'};$controls++
foreach($case in @(
 @($false,$true,$true,$true,$true,$false,$false,$true,$true,$false),
 @($true,$true,$false,$true,$true,$false,$false,$true,$true,$false),
 @($true,$true,$true,$false,$false,$false,$false,$true,$true,$false),
 @($true,$true,$true,$true,$true,$true,$false,$true,$true,$false),
 @($true,$true,$true,$true,$true,$false,$true,$true,$true,$false),
 @($true,$true,$true,$true,$true,$false,$false,$false,$true,$false),
 @($true,$true,$true,$true,$true,$false,$false,$true,$true,$true)
)){
 if([AICF_NativeGateProtocolContract]::Returned($case[0],$case[1],$case[2],$case[3],$case[4],$case[5],$case[6],$case[7],$case[8]) -ne $case[9]){throw 'Physical exit/return protocol admitted a stale initial seat or wrong transition'};$controls++
}
"PASS Native cargo gate: all exact assigned seats, full native OpenGate paired chain, observed exit/completed exact return then actual motion, shared immutable60s window and no native/progress writes; controls=$controls"

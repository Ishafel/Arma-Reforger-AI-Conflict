param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $RepositoryRoot 'tests/log-audits/Test-RecruitmentCapacityLog.ps1') -EvidenceRoot unused
$prefix='faction=USSR slot=0 generation=1 token=10 base=B group=G service=S'
$raw=@"
[RECRUIT_CAPACITY_START] classification=CONTROLLED_NATIVE_CAPACITY_INJECTION no_heal=1 no_teleport=1 native_deadline_ms=30000
[VEHICLE_SOAK_START] initial_alive=1
[INFANTRY_RECRUITMENT_ARRIVED] $prefix physical_presence=1
[RECRUIT_CAPACITY_PRESSURE] $prefix classification=CONTROLLED_NATIVE_CAPACITY_INJECTION physical_presence=1 active=84 intended=84 hard_limit=84 original_limit=228 original_started_at_ms=100 paid=0
[INFANTRY_RECRUIT_NATIVE_CAPACITY_WAIT] $prefix active=84 intended=84 hard_limit=84 pending=0 requested=1 projected=85 global_can_activate=0 donor_created=0 request_issued=0 paid=0 token_unchanged=1 visit_deadline_reset=0
[RECRUIT_CAPACITY_HOLD] $prefix active=84 intended=84 hard_limit=84 pending=0 requested=1 projected=85 global_can_activate=0 donor_created=0 request_issued=0 paid=0 token_unchanged=1 visit_deadline_reset=0 held_ms=35000 started_at_ms=100
[RECRUIT_CAPACITY_RELEASE] $prefix elapsed_ms=65000 active=84 intended=84 hard_limit=228 mechanism=RESTORE_ORIGINAL_NATIVE_LIMIT started_at_ms=100 token_unchanged=1
[INFANTRY_RECRUIT_NATIVE_CAPACITY_RELEASED] $prefix active=84 intended=84 hard_limit=228 pending=0 requested=1 projected=85 global_can_activate=1 donor_created=0 request_issued=0 paid=0 token_unchanged=1 visit_deadline_reset=0
[INFANTRY_RECRUIT_SPAWN_REQUESTED] $prefix donor=D paid=0
[INFANTRY_RECRUIT_NATIVE_SPAWN] $prefix boundary=REQUEST donor=D expected=1 engine_queue_observable=0 active=84 hard_limit=228 spawn_boundary=NOT_DISPATCHED
[INFANTRY_RECRUIT_DEBITED] $prefix cost=10 supplies_before=100 supplies_after=90 ticket_cost=0
[INFANTRY_RECRUIT_JOINED] $prefix alive=2 character=N
[RECRUIT_CAPACITY_JOIN] $prefix member=N actual=2 physical_presence=1 donor_cleared=1 started_at_ms=100
[RECRUIT_CAPACITY_DONE] passed=1 reason=NATIVE_WAIT_RELEASE_PAID_JOIN token=10 held=1 released=1 elapsed_ms=67000
"@
$positive=@(Test-AICFRecruitCapacity $raw)
if($positive.Count){throw "Donor native pressure positive rejected: $positive"}
foreach($m in @(
 @('no pressure','active=84 intended=84 hard_limit=84 original_limit=228','active=80 intended=80 hard_limit=84 original_limit=228','ACTUAL_NATIVE_PRESSURE'),
 @('false WAIT math','projected=85 global_can_activate=0','projected=84 global_can_activate=1','REAL_CAPACITY_WAIT_NO_REQUEST_OR_PAYMENT'),
 @('early donor','donor_created=0','donor_created=1','REAL_CAPACITY_WAIT_NO_REQUEST_OR_PAYMENT'),
 @('early request','request_issued=0','request_issued=1','REAL_CAPACITY_WAIT_NO_REQUEST_OR_PAYMENT'),
 @('early payment','request_issued=0 paid=0','request_issued=0 paid=10','REAL_CAPACITY_WAIT_NO_REQUEST_OR_PAYMENT'),
 @('too short hold','held_ms=35000','held_ms=29000','UNCHANGED_TOTAL_VISIT_DEADLINE'),
 @('visit extended','held_ms=35000 started_at_ms=100','held_ms=35000 started_at_ms=200','UNCHANGED_TOTAL_VISIT_DEADLINE'),
 @('no actual native restore','hard_limit=228 mechanism=RESTORE','hard_limit=84 mechanism=RESTORE','ACTUAL_CAPACITY_RELEASE_ADMISSION'),
 @('false allowed budget','requested=1 projected=85 global_can_activate=1','requested=1 projected=229 global_can_activate=1','ACTUAL_CAPACITY_RELEASE_ADMISSION'),
 @('wrong token','RECRUIT_CAPACITY_JOIN] faction=USSR slot=0 generation=1 token=10','RECRUIT_CAPACITY_JOIN] faction=USSR slot=0 generation=1 token=11','EXACT_ORDER_GENERATION_SERVICE_TOKEN'),
 @('no real debit','supplies_after=90','supplies_after=100','EXACT_POST_RELEASE_PAID_PHYSICAL_JOIN'),
 @('outside barracks',"RECRUIT_CAPACITY_JOIN] $prefix member=N actual=2 physical_presence=1","RECRUIT_CAPACITY_JOIN] $prefix member=N actual=2 physical_presence=0",'ACTUAL_NEW_MEMBER_IN_ORIGINAL_GROUP'),
 @('wrong joined actor','member=N actual=2','member=FOREIGN actual=2','ACTUAL_NEW_MEMBER_IN_ORIGINAL_GROUP'),
 @('missing dispatch readback','spawn_boundary=NOT_DISPATCHED','unknown_boundary=NOT_DISPATCHED','ACTUAL_DONOR_DISPATCH_COUNTERS')
 ,@('wrong actual donor','boundary=REQUEST donor=D','boundary=REQUEST donor=FOREIGN','ACTUAL_DONOR_DISPATCH_COUNTERS')
)){
 if(!(@(Test-AICFRecruitCapacity ($raw.Replace($m[1],$m[2]))) -contains $m[3])){throw "Donor native pressure negative escaped: $($m[0])"}
}
$request='[INFANTRY_RECRUIT_SPAWN_REQUESTED] '+$prefix+' donor=D paid=0'
if(!(@(Test-AICFRecruitCapacity ($raw.Replace($request,'').Replace('[RECRUIT_CAPACITY_HOLD]',$request+"`n[RECRUIT_CAPACITY_HOLD]"))) -contains 'EXACT_POST_RELEASE_PAID_PHYSICAL_JOIN')){throw 'Request during held native capacity accepted'}
foreach($marker in @('[GROUP_SPAWN_TIMEOUT]','reason=SPAWN_INVALID_OR_TIMEOUT','[ACCEPTANCE_FAILURE_LATCHED]','SCRIPT    (E): failure')){if(!(@(Test-AICFRecruitCapacity ($raw+"`n"+$marker)) -contains 'NO_TIMEOUT_OR_NATIVE_FATAL')){throw 'Prior failure hidden by native release'}}
Write-Output 'PASS Donor capacity log: positive1/negative20; no request/debit during hold, exact native release/paid physical join, same token/visit deadline'

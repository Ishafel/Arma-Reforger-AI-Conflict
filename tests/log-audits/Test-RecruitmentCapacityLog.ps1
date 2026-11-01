[CmdletBinding()]
param([Parameter(Mandatory)][string]$EvidenceRoot)
$ErrorActionPreference='Stop'
$donorEvidenceRoot=$EvidenceRoot
. (Join-Path $PSScriptRoot 'Test-VehicleSoakObservationLog.ps1') -LogPath unused -EvidenceRoot unused -Faction USSR
$EvidenceRoot=$donorEvidenceRoot
function Test-AICFRecruitCapacity([string]$Raw){
 $issues=[Collections.Generic.List[string]]::new()
 function Need([bool]$condition,[string]$why){if(!$condition -and !$issues.Contains($why)){$issues.Add($why)}}
 $events=@([regex]::Matches($Raw,'\[(RECRUIT_CAPACITY_[A-Z_]+|INFANTRY_RECRUIT_NATIVE_CAPACITY_[A-Z_]+|INFANTRY_RECRUIT_SPAWN_REQUESTED|INFANTRY_RECRUIT_NATIVE_SPAWN|INFANTRY_RECRUIT_DEBITED|INFANTRY_RECRUIT_JOINED|INFANTRY_RECRUITMENT_ARRIVED|INFANTRY_RECRUITMENT_FINISHED|VEHICLE_SOAK_START)\]([^\r\n]*)')|ForEach-Object{[pscustomobject]@{name=$_.Groups[1].Value;index=$_.Index;fields=(Fields $_.Groups[2].Value)}})
 function One($name){$items=@($events|Where-Object name -eq $name);Need ($items.Count -eq 1) ('ONCE_'+$name);if($items.Count){return $items[0]};return [pscustomobject]@{index=-1;fields=@{}}}
 $start=One RECRUIT_CAPACITY_START;$pressure=One RECRUIT_CAPACITY_PRESSURE;$wait=One INFANTRY_RECRUIT_NATIVE_CAPACITY_WAIT;$hold=One RECRUIT_CAPACITY_HOLD;$release=One RECRUIT_CAPACITY_RELEASE;$admit=One INFANTRY_RECRUIT_NATIVE_CAPACITY_RELEASED;$join=One RECRUIT_CAPACITY_JOIN;$done=One RECRUIT_CAPACITY_DONE;$soak=One VEHICLE_SOAK_START
 Need ($start.fields.classification -eq 'CONTROLLED_NATIVE_CAPACITY_INJECTION' -and $start.fields.no_heal -eq '1' -and $start.fields.no_teleport -eq '1' -and $start.fields.native_deadline_ms -eq '30000') 'EXPLICIT_CONTROLLED_NATIVE_CASE'
 Need ($pressure.fields.classification -eq 'CONTROLLED_NATIVE_CAPACITY_INJECTION' -and $pressure.fields.physical_presence -eq '1' -and (Number $pressure.fields.hard_limit) -le [Math]::Max((Number $pressure.fields.active),(Number $pressure.fields.intended)) -and (Number $pressure.fields.original_limit) -gt (Number $pressure.fields.hard_limit)) 'ACTUAL_NATIVE_PRESSURE'
 Need ($start.index -lt $pressure.index -and $pressure.index -lt $wait.index -and $wait.index -lt $hold.index -and $hold.index -lt $release.index -and $release.index -lt $admit.index -and $admit.index -lt $join.index -and $join.index -lt $done.index) 'NATIVE_WAIT_RELEASE_CAUSAL_ORDER'
 foreach($e in @($wait,$hold,$release,$admit,$join)){
  foreach($key in @('faction','slot','generation','token','base','group','service')){Need ($e.fields[$key] -and (Entity $e.fields[$key]) -eq (Entity $pressure.fields[$key])) 'EXACT_ORDER_GENERATION_SERVICE_TOKEN'}
 }
 foreach($e in @($wait,$hold)){
  Need ($e.fields.requested -eq '1' -and (Number $e.fields.projected) -eq [Math]::Max((Number $e.fields.active),(Number $e.fields.intended))+(Number $e.fields.pending)+1 -and ((Number $e.fields.projected) -gt (Number $e.fields.hard_limit) -or $e.fields.global_can_activate -eq '0') -and $e.fields.donor_created -eq '0' -and $e.fields.request_issued -eq '0' -and $e.fields.paid -eq '0' -and $e.fields.token_unchanged -eq '1' -and $e.fields.visit_deadline_reset -eq '0') 'REAL_CAPACITY_WAIT_NO_REQUEST_OR_PAYMENT'
 }
 Need ((Number $hold.fields.held_ms) -ge 35000 -and (Number $release.fields.elapsed_ms) -ge 65000 -and $hold.fields.started_at_ms -eq $pressure.fields.original_started_at_ms -and $release.fields.started_at_ms -eq $pressure.fields.original_started_at_ms -and $join.fields.started_at_ms -eq $pressure.fields.original_started_at_ms) 'UNCHANGED_TOTAL_VISIT_DEADLINE'
 Need ($release.fields.mechanism -eq 'RESTORE_ORIGINAL_NATIVE_LIMIT' -and $release.fields.hard_limit -eq $pressure.fields.original_limit -and $admit.fields.global_can_activate -eq '1' -and (Number $admit.fields.projected) -le (Number $admit.fields.hard_limit) -and $admit.fields.requested -eq '1') 'ACTUAL_CAPACITY_RELEASE_ADMISSION'
 $requested=@($events|Where-Object{$_.name -eq 'INFANTRY_RECRUIT_SPAWN_REQUESTED' -and $_.fields.token -eq $pressure.fields.token})
 $debits=@($events|Where-Object{$_.name -eq 'INFANTRY_RECRUIT_DEBITED' -and $_.fields.token -eq $pressure.fields.token})
 $joined=@($events|Where-Object{$_.name -eq 'INFANTRY_RECRUIT_JOINED' -and $_.fields.token -eq $pressure.fields.token})
 $arrived=@($events|Where-Object{$_.name -eq 'INFANTRY_RECRUITMENT_ARRIVED' -and $_.fields.token -eq $pressure.fields.token -and $_.fields.physical_presence -eq '1' -and $_.index -lt $pressure.index})
 Need ($requested.Count -eq 1 -and $debits.Count -eq 1 -and $joined.Count -eq 1 -and $arrived.Count -eq 1 -and $requested[0].index -gt $admit.index -and $debits[0].index -gt $requested[0].index -and $joined[0].index -gt $debits[0].index -and $joined[0].index -lt $join.index -and (Number $debits[0].fields.cost) -gt 0 -and (Number $debits[0].fields.supplies_before)-(Number $debits[0].fields.supplies_after) -eq (Number $debits[0].fields.cost) -and $debits[0].fields.ticket_cost -eq '0') 'EXACT_POST_RELEASE_PAID_PHYSICAL_JOIN'
 if($joined.Count -eq 1){Need ((NativeEntityMatches $join.fields.member $joined[0].fields.character) -and $join.fields.actual -eq $joined[0].fields.alive -and (Number $join.fields.actual) -eq (Number $soak.fields.initial_alive)+1 -and $join.fields.donor_cleared -eq '1' -and $join.fields.physical_presence -eq '1') 'ACTUAL_NEW_MEMBER_IN_ORIGINAL_GROUP'}
 $native=@($events|Where-Object{$_.name -eq 'INFANTRY_RECRUIT_NATIVE_SPAWN' -and $_.fields.token -eq $pressure.fields.token -and $_.fields.boundary -eq 'REQUEST' -and $_.index -gt $requested[0].index})
 Need ($native.Count -eq 1 -and (NativeEntityMatches $native[0].fields.donor $requested[0].fields.donor) -and $native[0].fields.expected -eq '1' -and $native[0].fields.engine_queue_observable -eq '0' -and $native[0].fields.active -and $native[0].fields.hard_limit -and $native[0].fields.spawn_boundary) 'ACTUAL_DONOR_DISPATCH_COUNTERS'
 Need ($done.fields.passed -eq '1' -and $done.fields.reason -eq 'NATIVE_WAIT_RELEASE_PAID_JOIN' -and $done.fields.held -eq '1' -and $done.fields.released -eq '1' -and $done.fields.token -eq $pressure.fields.token) 'CONTROLLED_DONE_PROVEN'
 Need ($Raw -notmatch 'reason=SPAWN_INVALID_OR_TIMEOUT|\[GROUP_SPAWN_TIMEOUT\]|\[ACCEPTANCE_FAILURE_LATCHED\]|SCRIPT\s+\([EF]\)|Assertion failed|NULL pointer') 'NO_TIMEOUT_OR_NATIVE_FATAL'
 return $issues.ToArray()
}
if($MyInvocation.InvocationName -eq '.'){return}
$root=(Resolve-Path -LiteralPath $EvidenceRoot).Path
$console=@(Get-ChildItem -LiteralPath (Join-Path $root 'profile') -Recurse -File -Filter console.log)
if($console.Count -ne 1){throw 'Exactly one native console required'}
$raw=[IO.File]::ReadAllText($console[0].FullName)
$functional=@(Test-AICFRecruitCapacity $raw)
$fatals=@();$resources=@()
foreach($file in @(Get-ChildItem -LiteralPath $console[0].DirectoryName -File -Filter '*.log')){
 $n=0;foreach($line in [IO.File]::ReadLines($file.FullName)){$n++;$item=[pscustomobject]@{file=$file.FullName;line=$n;text=$line};if($line -match 'SCRIPT\s+\([EF]\)|ENGINE\s+\(F\)|NULL pointer|Virtual Machine Exception|Assertion failed|Access violation|memory leaks?'){$fatals+=$item}elseif($line -match '\([EF]\)\s*:'){$resources+=$item}}
}
$summary=Get-Content (Join-Path $root 'summary.json') -Raw|ConvertFrom-Json
$manifest=Get-Content (Join-Path $root 'source-manifest.json') -Raw|ConvertFrom-Json
$drift=@($manifest.files|Where-Object{!(Test-Path -LiteralPath $_.path) -or (Get-FileHash -LiteralPath $_.path).Hash -ne $_.sha256})
$stopped=$summary.native_exit -eq 0 -and $raw -match 'ENGINE\s*: Game destroyed\.'
$result=[ordered]@{functional=($functional.Count -eq 0);functional_issues=$functional;source_freeze=($drift.Count -eq 0 -and $summary.source_drift -eq 0);stopped_native=$stopped;fatal_count=$fatals.Count;resource_count=$resources.Count;fatal_errors=$fatals;resource_errors=$resources}
$result|ConvertTo-Json -Depth 7|Set-Content (Join-Path $root 'donor-audit.json')
$result|ConvertTo-Json -Depth 3
if($functional.Count -or $drift.Count -or !$stopped -or $fatals.Count -or $resources.Count){exit 1}

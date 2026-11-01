param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
$work=Join-Path ([IO.Path]::GetTempPath()) ('aicf-multi-protection-'+[guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($work)
$script:checks=0
# Используем существующий полный positive lifecycle oracle, не ослабляя его.
$tokens=$null;$errors=$null
$ast=[Management.Automation.Language.Parser]::ParseFile((Join-Path $PSScriptRoot 'Test-VehicleMultiSquadLogContracts.ps1'),[ref]$tokens,[ref]$errors)
if($errors.Count){throw 'Existing multi-squad oracle cannot be parsed'}
$names=@('Id','Marker','Snapshot','NativeJoin','Fixture')
$definitions=@($ast.FindAll({param($node) $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -in $names},$false))
if($definitions.Count -ne $names.Count){throw 'Existing multi-squad fixture functions missing'}
. ([scriptblock]::Create(($definitions | ForEach-Object{$_.Extent.Text}) -join "`n"))
function Fields([string]$line){$f=@{};foreach($v in [regex]::Matches($line,'(?:^|\s)([A-Za-z_][A-Za-z0-9_]*)=(.*?)(?=\s+[A-Za-z_][A-Za-z0-9_]*=|$)')){$f[$v.Groups[1].Value]=$v.Groups[2].Value.Trim()};return $f}
function HierarchyFixture([string]$concurrency='MOVING',[int]$seconds=1800){
 $body=Fixture CAPACITY 0 $concurrency $seconds
 $manifest=([IO.File]::ReadAllText("$work/launcher.txt").Replace('AICF_RUNTIME_MANIFEST_JSON=',''))|ConvertFrom-Json
 $manifest.arguments+=@('-aicfVehicleMultiProtectionScope','HIERARCHY')
 $manifest|ConvertTo-Json -Depth 5 -Compress|ForEach-Object{[IO.File]::WriteAllText("$work/launcher.txt",'AICF_RUNTIME_MANIFEST_JSON='+$_)}
 $body=[regex]::Replace($body,'(?m)^CLI Params: [^\r\n]+',('CLI Params: '+($manifest.arguments -join ' ')))
 $lines=[Collections.Generic.List[string]]::new()
 foreach($line in ($body -split "`n")){
  if($line -match '\[VEHICLE_MULTI_SLOT\]'){
   $f=Fields $line
   if($f.exact_active -eq '1'){
    $row=@($script:rows|Where-Object{$_.faction -eq $f.faction -and [string]$_.slot -eq $f.numeric_slot})[0]
    foreach($actor in $row.members){
     $receipt=Marker PROTECTION ([int]$f.elapsed_ms) $row.faction $row.slot "kind=MEMBER entity=$actor protection_errors=0 protection_zones=5 protection_zones_repeated=5 protection_managers_covered=2 protection_child_managers=1 protection_enabled_before=2 protection_enabled_after=0 protection_status=COVERED protection_reason=NONE"
     $lines.Add($receipt.TrimEnd()+ ' protection_scope=HIERARCHY test_only=1')
    }
    $receipt=Marker PROTECTION ([int]$f.elapsed_ms) $row.faction $row.slot "kind=VEHICLE entity=$($row.vehicle) protection_errors=0 protection_zones=44 protection_zones_repeated=44 protection_managers_covered=22 protection_child_managers=21 protection_enabled_before=22 protection_enabled_after=0 protection_status=COVERED protection_reason=NONE"
    $lines.Add($receipt.TrimEnd()+ ' protection_scope=HIERARCHY test_only=1')
   }
  }
  if($line -match '\[VEHICLE_MULTI_\w+\]'){$line+=' protection_scope=HIERARCHY test_only=1'}
  $lines.Add($line)
 }
 return $lines -join "`n"
}
function RunCase([string]$name,[string]$body,[int]$expected,[string]$failedGate='',
 [string]$scope='HIERARCHY',[string]$mode='CAPACITY',[string]$concurrency='MOVING',[int]$seconds=1800,
 [string]$sourceScope=$scope,[string]$timingScope=$scope,[string]$sibling=''){
 [IO.File]::WriteAllText("$work/console.log",$body);[IO.File]::WriteAllText("$work/script.log",$sibling);[IO.File]::WriteAllText("$work/native-exit.txt",'0')
 @{muster_policy='PER_GROUP_BEFORE_VEHICLE';protection_scope=$sourceScope}|ConvertTo-Json|Set-Content -LiteralPath "$work/source-manifest.json"
 $started=[DateTimeOffset]::Parse('2026-10-27T19:00:00Z')
 @{started_utc=$started.ToString('o');ended_utc=$started.AddSeconds($seconds+30).ToString('o');wall_seconds=$seconds+30;requested_seconds=$seconds;mode=$mode;concurrency_mode=$concurrency;protection_scope=$timingScope}|ConvertTo-Json|Set-Content -LiteralPath "$work/timing.json"
 $result=& (Join-Path $RepositoryRoot 'tests/log-audits/Test-VehicleMultiSquadLog.ps1') -LogPath "$work/console.log" -EvidenceRoot $work -Mode $mode -ConcurrencyMode $concurrency -ProtectionScope $scope -MinimumSeconds $seconds
 if($LASTEXITCODE -ne $expected){throw "$name unexpected verdict: $result"}
 $summary=Get-Content -LiteralPath "$work/audit-summary.json" -Raw|ConvertFrom-Json
 if($failedGate -and $summary.gates.$failedGate.verdict -ne 'FAIL'){throw "$name expected $failedGate FAIL: $result"}
 if($expected -eq 0 -and $scope -eq 'HIERARCHY' -and (!$summary.hierarchy_protection_proved -or $summary.normal_campaign_evidence)){throw 'Hierarchy proof misclassified'}
 if($summary.gates.protection.verdict -eq 'FAIL' -and ($summary.concurrent_moving_cohorts_proved -or $summary.mounted_staging_cohorts_proved -or $summary.hierarchy_protection_proved)){throw 'Missing protection proof cannot certify controlled cohorts'}
 $script:checks++
}
try{
 $good=HierarchyFixture
 RunCase hierarchy-current-all-sixty-actors-twelve-vehicles $good 0
 foreach($mutation in @(
  @('enabled-child','protection_enabled_after=0','protection_enabled_after=1','protection'),
  @('readback-error','protection_errors=0','protection_errors=1','protection'),
  @('readback-failed','protection_status=COVERED','protection_status=REJECTED','protection'),
  @('incomplete-native-count','protection_zones_repeated=44','protection_zones_repeated=43','protection'),
  @('missing-child-managers','protection_child_managers=21','protection_child_managers=0','protection'),
  @('invalid-covered-count','protection_managers_covered=22','protection_managers_covered=0','protection'),
  @('wrong-actor','kind=MEMBER entity=0x4000000000001004','kind=MEMBER entity=0x4000000000001003','protection'),
  @('wrong-vehicle','kind=VEHICLE entity=0x4000000000000600','kind=VEHICLE entity=0x4000000000000601','protection'),
  @('mixed-protection-scope','protection_scope=HIERARCHY','protection_scope=ROOT_ONLY','environment'),
  @('missing-test-classification','test_only=1','test_only=0','protection')
 )){
  if(!$good.Contains($mutation[1])){throw "Missing log mutation target $($mutation[0])"}
  RunCase $mutation[0] ($good.Replace($mutation[1],$mutation[2])) 1 $mutation[3]
 }
 $noReceipts=[regex]::Replace($good,'(?m)^.*\[VEHICLE_MULTI_PROTECTION\][^\r\n]*\r?\n','')
 RunCase missing-hierarchy-proof $noReceipts 1 protection
 foreach($omission in @(@('all-actor-proof','kind=MEMBER'),@('all-vehicle-proof','kind=VEHICLE'),@('one-actor-proof','kind=MEMBER entity=0x4000000000001004'),@('one-vehicle-proof','kind=VEHICLE entity=0x4000000000000600'))){
  $changed=[regex]::Replace($good,('(?m)^.*\[VEHICLE_MULTI_PROTECTION\][^\r\n]*'+[regex]::Escape($omission[1])+'[^\r\n]*\r?\n'),'')
  RunCase $omission[0] $changed 1 protection
 }
 $stale=[regex]::Replace($good,'(?m)^.*\[VEHICLE_MULTI_PROTECTION\][^\r\n]*\r?\n',{param($match) $f=Fields $match.Value;if([int]$f.elapsed_ms -gt 20000){return ''};return $match.Value})
 RunCase initial-proof-does-not-protect-thirty-second-window $stale 1 protection
 RunCase wrong-source-scope $good 1 provenance -sourceScope ROOT_ONLY
 RunCase wrong-timing-scope $good 1 provenance -timingScope ROOT_ONLY
 RunCase wrong-mode $good 1 environment -mode NORMAL
 RunCase sibling-fatal-remains-fatal $good 1 native_fatal -sibling 'SCRIPT (E): Leaked test instance'
 $withoutCli=$good.Replace('-aicfVehicleMultiProtectionScope HIERARCHY','')
 RunCase actual-CLI-scope-missing $withoutCli 1 provenance
 $manifest=([IO.File]::ReadAllText("$work/launcher.txt").Replace('AICF_RUNTIME_MANIFEST_JSON=',''))|ConvertFrom-Json
 $manifest.arguments=@($manifest.arguments|Where-Object{$_ -notin @('-aicfVehicleMultiProtectionScope','HIERARCHY')})
 [IO.File]::WriteAllText("$work/launcher.txt",'AICF_RUNTIME_MANIFEST_JSON='+($manifest|ConvertTo-Json -Depth 5 -Compress))
 RunCase manifest-scope-missing $good 1 provenance
 $mounted=HierarchyFixture MOUNTED_STAGING 900
 RunCase hierarchy-mounted-proof-separate-from-moving $mounted 0 -concurrency MOUNTED_STAGING -seconds 900
 $root=Fixture CAPACITY
 RunCase historical-root-log-without-new-metadata $root 0 -scope ROOT_ONLY
 $newRoot=[regex]::Replace($root,'(?m)^(.*\[VEHICLE_MULTI_\w+\][^\r\n]*)$', '$1 protection_scope=ROOT_ONLY test_only=1')
 RunCase root-with-explicit-new-metadata $newRoot 0 -scope ROOT_ONLY
}finally{
 $resolved=[IO.Path]::GetFullPath($work)
 if(!$resolved.StartsWith([IO.Path]::GetTempPath(),[StringComparison]::OrdinalIgnoreCase) -or (Split-Path -Leaf $resolved) -notlike 'aicf-multi-protection-*'){throw 'Unsafe cleanup'}
 Remove-Item -LiteralPath $resolved -Recurse -Force
}
"PASS Multi hierarchy log: strict exact current actor/vehicle coverage, repeated native counts, CLI/manifest scope, root regression and negative controls; controls=$script:checks"
exit 0

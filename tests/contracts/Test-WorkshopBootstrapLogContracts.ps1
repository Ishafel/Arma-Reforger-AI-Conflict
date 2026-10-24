param([string]$RepositoryRoot = (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference = 'Stop'
. (Join-Path $RepositoryRoot 'tools/AICF_Scenarios.ps1')
$catalog = Get-AICFScenarioCatalog -Path (Join-Path $RepositoryRoot 'tools/config/AICF_Scenarios.json')
$audit = Join-Path $RepositoryRoot 'tests/log-audits/Test-WorkshopBootstrapLog.ps1'
$tempRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\','/')
$work = Join-Path $tempRoot ('aicf-cold-log-' + [guid]::NewGuid().ToString('N'))
[void](New-Item -ItemType Directory -Path $work)
$count = 0
function Fixture([string]$variant = 'Stock', [int]$mode = 0, [int]$difficulty = 0, [switch]$HostFlow, [switch]$Explicit, [switch]$Cancel) {
    $plan = Resolve-AICFScenario -Catalog $catalog -Variant $variant -MatchMode @('Standard','CoopUS','CoopUSSR')[$mode] -Difficulty @('Easy','Medium','Hard')[$difficulty]
    $argv = @('-gproj','C:\source\addon.gproj','-profile','C:\fresh','-aicfColdBootstrapProbe','1')
    if ($HostFlow) { $argv += @('-aicfColdHostProbe','1') }
    if ($Explicit) { $argv += @('-aicfColdVariant',$variant,'-aicfColdMode',[string]$mode,'-aicfColdDifficulty',[string]$difficulty) }
    if ($Cancel) { $argv += @('-aicfColdCancelProbe','1') }
    $names = @('LITERAL_NAME','FRONTEND_GRAPH','NATIVE_ENTRY','SETUP_OPEN','BOOTSTRAP_CONTEXT')
    $done = 4
    if ($Cancel) { $names += @('CANCEL_RETURN','CANCEL_CLOSED'); $done = 2 }
    else {
        $names += @('DEFAULTS','SELECT','SELECT_ENVIRONMENT','CAN_START','LAUNCH','SELECTED_HEADER','NO_BOOTSTRAP','APPLIED_RULES','APPLIED_ENVIRONMENT')
        if ($HostFlow) { $names += @('HOST_REQUEST','HOST_CONTEXT','HOST_LISTEN'); $done = 5 }
        if ($Explicit) { $names += @('CONTENT_READY','TARGET_WORLD','TARGET_GRAPH'); $done += 2 }
    }
    $lines = @('CLI Params: ' + ($argv -join ' '), '[AICF][SCENARIO_BOOTSTRAP] phase=OPEN entry=SETUP')
    $lines += @($names | ForEach-Object { "[AICF][COLD_CASE] case=$_ pass=1" })
    if ($Cancel) { $lines += '[AICF][SCENARIO_BOOTSTRAP] phase=CANCEL' }
    else {
        $lines += "[AICF][SCENARIO_LAUNCH] variant=$variant mode=$mode difficulty=$difficulty resource=$($plan.scenario) addons=$($plan.addonIds)"
        if ($Explicit) {
            $lines += '[AICF][COLD_WORLD] actual=C:/game/worlds/MP/selected.ent expected=worlds/MP/selected.ent'
            $lines += @($plan.addonIds -split ',' | ForEach-Object { "[AICF][COLD_ADDON] id=$_ vanilla=0" })
            $lines += '[AICF][COLD_ADDON] id=58D0FB3206B6F859 vanilla=1'
        }
    }
    $lines += @("[AICF][COLD_DONE] checks=$done failed=0", 'Game destroyed.')
    [pscustomobject]@{ log = ($lines -join "`n"); manifest = (@{ profile = 'C:\fresh'; arguments = $argv } | ConvertTo-Json -Depth 4); plan = $plan }
}
function Check([string]$name, $fixture, [string]$log, [int]$native = 0, [string]$rule = '', [string]$sideLog = '', [switch]$DefaultRoot) {
    [IO.File]::WriteAllText("$work/console.log", $log)
    [IO.File]::WriteAllText("$work/manifest.json", $fixture.manifest)
    [IO.File]::WriteAllText("$work/exit.txt", [string]$native)
    [IO.File]::WriteAllText("$work/error.log", $sideLog)
    $arguments = @('-NoProfile','-ExecutionPolicy','Bypass','-File',$audit,'-LogPath',"$work/console.log",'-ManifestPath',"$work/manifest.json",'-NativeExitPath',"$work/exit.txt")
    if (!$DefaultRoot) { $arguments += @('-RepositoryRoot',$RepositoryRoot) }
    $output = & powershell.exe @arguments 2>&1
    $expected = 0
    if ($rule) { $expected = 1 }
    if ($LASTEXITCODE -ne $expected -or ($rule -and ($output -join "`n") -notmatch $rule)) { throw "Unexpected ${name}: $output" }
    $script:count++
    "PASS $name"
}
try {
    $play = Fixture
    $hosted = Fixture -HostFlow
    $cancel = Fixture -Cancel
    $stock = Fixture -HostFlow -Explicit
    $wcs = Fixture -variant EveronWCSRHS -mode 2 -difficulty 2 -HostFlow -Explicit
    foreach ($item in @($play,$hosted,$cancel,$stock,$wcs)) { Check positive $item $item.log }
    Check defaultroot $wcs $wcs.log -DefaultRoot
    $warmPlan = $wcs.manifest | ConvertFrom-Json
    $warmPlan.arguments += @('-aicfWarmBootstrapControl','1')
    $warm = [pscustomobject]@{ manifest = ($warmPlan | ConvertTo-Json -Depth 4) }
    $warmLog = $wcs.log.Replace('-aicfColdDifficulty 2','-aicfColdDifficulty 2 -aicfWarmBootstrapControl 1').Replace('[AICF][SCENARIO_BOOTSTRAP] phase=OPEN entry=SETUP','')
    Check warm $warm $warmLog
    Check unexpectedbootstrap $warm ($warmLog + "`n[AICF][SCENARIO_BOOTSTRAP] phase=OPEN entry=SETUP") -rule PHASE_OPEN
    Check mountedworld $wcs ($wcs.log.Replace('C:/game/worlds/MP/selected.ent','$ArmaReforger:worlds/MP/selected.ent'))
    Check wrongmountedworld $wcs ($wcs.log.Replace('C:/game/worlds/MP/selected.ent','$ArmaReforger:worlds/MP/Arland.ent')) -rule TARGET_WORLD
    $retryPlan = $wcs.manifest | ConvertFrom-Json
    $retryPlan.arguments += @('-aicfColdRetryHost','1')
    $retry = [pscustomobject]@{ manifest = ($retryPlan | ConvertTo-Json -Depth 4) }
    $retryLog = $wcs.log.Replace('-aicfColdDifficulty 2','-aicfColdDifficulty 2 -aicfColdRetryHost 1') + "`n[AICF][SCENARIO_BOOTSTRAP] phase=CANCEL`n[AICF][COLD_CASE] case=RETRY_CANCEL_CONTEXT pass=1`n[AICF][COLD_CASE] case=RETRY_RETURN pass=1"
    Check retry $retry $retryLog
    Check missingretry $retry ($retryLog.Replace('[AICF][COLD_CASE] case=RETRY_RETURN pass=1','')) -rule CASE_RETRY_RETURN
    Check missingcancel $retry ($retryLog.Replace('[AICF][SCENARIO_BOOTSTRAP] phase=CANCEL','')) -rule RETRY_FLOW
    Check missingheader $wcs ($wcs.log.Replace('[AICF][COLD_CASE] case=SELECTED_HEADER pass=1','')) -rule CASE_SELECTED_HEADER
    Check failedworld $wcs ($wcs.log.Replace('case=TARGET_WORLD pass=1','case=TARGET_WORLD pass=0')) -rule CASE_TARGET_WORLD
    Check wrongworld $wcs ($wcs.log.Replace('actual=C:/game/worlds/MP/selected.ent','actual=C:/game/worlds/MP/Arland.ent')) -rule TARGET_WORLD
    Check missingworld $wcs ($wcs.log.Replace('[AICF][COLD_WORLD]','[OTHER]')) -rule TARGET_WORLD
    Check extraaddon $wcs ($wcs.log + "`n[AICF][COLD_ADDON] id=AAAAAAAAAAAAAAAA vanilla=0") -rule TARGET_GRAPH
    Check missingaddon $wcs ($wcs.log.Replace('[AICF][COLD_ADDON] id=A1CF261006100001 vanilla=0','')) -rule TARGET_GRAPH
    Check duplicateaddon $wcs ($wcs.log + "`n[AICF][COLD_ADDON] id=A1CF261006100001 vanilla=0") -rule TARGET_GRAPH
    Check wrongvariant $wcs ($wcs.log.Replace('variant=EveronWCSRHS mode=2','variant=Stock mode=2')) -rule LAUNCH_PLAN
    Check wrongmode $wcs ($wcs.log.Replace('mode=2 difficulty=2','mode=0 difficulty=2')) -rule LAUNCH_PLAN
    Check wrongdifficulty $wcs ($wcs.log.Replace('mode=2 difficulty=2','mode=2 difficulty=0')) -rule LAUNCH_PLAN
    Check wrongresource $wcs ($wcs.log.Replace($wcs.plan.scenario,'OTHER')) -rule LAUNCH_PLAN
    Check wronggraph $wcs ($wcs.log.Replace('addons=' + $wcs.plan.addonIds,'addons=9178E5822AFE48EA')) -rule LAUNCH_PLAN
    Check nativestop $wcs $wcs.log -native -1 -rule NATIVE_EXIT
    Check hang $wcs ($wcs.log + "`nApplication hangs") -rule NATIVE_HANG
    Check lateerror $wcs ($wcs.log + "`nSCRIPT (E): late error") -rule SCRIPT_VM_FATAL
    Check sidelogs $wcs $wcs.log -rule SCRIPT_VM_FATAL -sideLog 'Access violation'
    Check duplicatecase $wcs ($wcs.log + "`n[AICF][COLD_CASE] case=LAUNCH pass=1") -rule CASE_LAUNCH
    Check duplicatedone $wcs ($wcs.log + "`n[AICF][COLD_DONE] checks=6 failed=0") -rule COMPLETION
    Check timeout $wcs ($wcs.log + "`n[AICF][COLD_TIMEOUT]") -rule COMPLETION
    Check intermediate $wcs ('[BOOTSTRAP_SERVER]' + "`n" + $wcs.log) -rule INTERMEDIATE_MATCH
    Check wrongcli $wcs ($wcs.log.Replace('-gproj C:\source\addon.gproj','-gproj C:\other\addon.gproj')) -rule CLI_MISMATCH
    $bad = [pscustomobject]@{ manifest = $wcs.manifest.Replace('EveronWCSRHS','Unknown') }
    Check unknown $bad $wcs.log -rule CLI_SELECTION
    $bad.manifest = $wcs.manifest.Replace('"-aicfColdMode",','"-aicfColdMode", "3", "-aicfColdMode",')
    Check duplicatecli $bad $wcs.log -rule CLI_SELECTION
    $acceptedOnly = ($wcs.log -split "`n" | Where-Object { $_ -notmatch 'case=(SELECTED_HEADER|NO_BOOTSTRAP|APPLIED_RULES|APPLIED_ENVIRONMENT|HOST_LISTEN|TARGET_WORLD|TARGET_GRAPH)|\[COLD_(DONE|WORLD|ADDON)\]' }) -join "`n"
    Check acceptedonly $wcs $acceptedOnly -rule COMPLETION
    $handoffPlan = $wcs.manifest | ConvertFrom-Json
    $handoffPlan.arguments += @('-aicfExpectHostHandoff','1','-aicfHostIntentContracts','1','-aicfColdReadyProbe','1')
    $handoff = [pscustomobject]@{ manifest = ($handoffPlan | ConvertTo-Json -Depth 4) }
    $handoffLog = $wcs.log.Replace('-aicfColdDifficulty 2','-aicfColdDifficulty 2 -aicfExpectHostHandoff 1 -aicfHostIntentContracts 1 -aicfColdReadyProbe 1')
    $handoffLog += "`n" + (@('RETURN','HOST','LEAF','READY') | ForEach-Object { "[AICF][HOST_HANDOFF] phase=$_" }) -join "`n"
    $handoffLog += "`n[AICF][COLD_READY] listen=1 address=100.98.58.185 pending=0`nStarting RPL server, listening on address 127.0.0.1:24073, fastValidation=true`n[ROSTER_READY]`n[ENVIRONMENT_APPLIED] time_minutes=577 weather_mode=4"
    $intentNames = @('ROUNDTRIP','INVALID_JSON','SCHEMA','PHASE','TTL','MAP','CONTENT','MODE','DIFFICULTY','MINUTES','WEATHER','ENTRY','GRAPH','HOST_ONLY')
    $handoffLog += "`n" + (($intentNames | ForEach-Object { "[AICF][HOST_INTENT_CASE] case=$_ pass=1" }) -join "`n")
    Check handoff $handoff $handoffLog
    Check missinghandoff $handoff ($handoffLog.Replace('[AICF][HOST_HANDOFF] phase=READY','')) -rule HOST_HANDOFF_PHASES
    Check duplicatehandoff $handoff ($handoffLog + "`n[AICF][HOST_HANDOFF] phase=READY") -rule HOST_HANDOFF_PHASES
    Check reorderedhandoff $handoff ($handoffLog.Replace('phase=HOST','phase=TEMP').Replace('phase=LEAF','phase=HOST').Replace('phase=TEMP','phase=LEAF')) -rule HOST_HANDOFF_PHASES
    Check failedhandoff $handoff ($handoffLog + "`n[AICF][HOST_HANDOFF][FAIL] reason=GRAPH") -rule HOST_HANDOFF_FAILED
    Check failedintent $handoff ($handoffLog.Replace('case=TTL pass=1','case=TTL pass=0')) -rule INTENT_TTL
    Check missingintent $handoff ($handoffLog.Replace('[AICF][HOST_INTENT_CASE] case=HOST_ONLY pass=1','')) -rule INTENT_HOST_ONLY
    Check duplicintent $handoff ($handoffLog + "`n[AICF][HOST_INTENT_CASE] case=ROUNDTRIP pass=1") -rule INTENT_CASE_COUNT
    Check wronglisten $handoff ($handoffLog.Replace('listen=1 address=100.98.58.185','listen=0 address=100.98.58.185')) -rule HOST_ROSTER_READY
    Check wrongport $handoff ($handoffLog.Replace('address 127.0.0.1:24073','address 127.0.0.1:2001')) -rule HOST_LISTENER
    Check pendingintent $handoff ($handoffLog.Replace('pending=0','pending=1')) -rule HOST_ROSTER_READY
    Check missingroster $handoff ($handoffLog.Replace('[ROSTER_READY]','')) -rule HOST_ROSTER_READY
    Check missingapplied $handoff ($handoffLog.Replace('time_minutes=577','time_minutes=480')) -rule HOST_ENVIRONMENT_APPLIED
    "Workshop bootstrap log contracts: PASS; cases=$count"
}
finally {
    if (Test-Path -LiteralPath $work) {
        $resolved = [IO.Path]::GetFullPath((Resolve-Path -LiteralPath $work).Path)
        if ([IO.Path]::GetDirectoryName($resolved) -ine $tempRoot -or [IO.Path]::GetFileName($resolved) -notmatch '^aicf-cold-log-[a-f0-9]{32}$') { throw 'Unsafe contract cleanup path' }
        Remove-Item -LiteralPath $resolved -Recurse -Force
    }
}

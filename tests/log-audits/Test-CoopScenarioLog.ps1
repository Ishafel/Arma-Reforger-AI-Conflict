param(
    [Parameter(Mandatory=$true)][string]$LogPath,
    [Parameter(Mandatory=$true)][string]$LauncherPath,
    [Parameter(Mandatory=$true)][string]$NativeExitPath,
    [switch]$Client,
    [switch]$RequirePlayer,
    [switch]$RequireRecruit,
    [switch]$RequireBarracks,
    [switch]$RequireSupportArea,
    [switch]$RequireHeaderMatrix,
    [switch]$RequireWelcome,
    [ValidateSet('US','USSR')][string]$BarracksFaction='USSR',
    [string]$ExpectedWinner,
    [int]$MinimumObservationMs=30000
)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../../tools/AICF_Scenarios.ps1')
$catalog=Get-AICFScenarioCatalog
$failures=[Collections.Generic.List[string]]::new()
if($RequirePlayer -and -not $Client) { $failures.Add('PLAYER_ROLE') }
if($RequireWelcome -and -not $Client) { $failures.Add('WELCOME_ROLE') }
if($RequireRecruit -and $RequireBarracks) { $failures.Add('RECRUIT_PROBE_CONFLICT') }
$log=[IO.File]::ReadAllText($LogPath)
$launcher=[IO.File]::ReadAllText($LauncherPath)
if((Get-Content -LiteralPath $NativeExitPath -Raw).Trim() -ne '0') { $failures.Add('NATIVE_EXIT') }
if($log -notmatch 'Game destroyed\.') { $failures.Add('STOPPED_LOG_REQUIRED') }
if($log -match 'SCRIPT\s+\([EF]\):|ENGINE\s+\(F\):|NULL pointer|Virtual Machine Exception|Unhandled exception') { $failures.Add('SCRIPT_FATAL') }
if($log -match '\[COOP_TIMEOUT\]|\[COOP_CASE\] case=\w+ pass=0') { $failures.Add('PROBE_FAILURE') }
if($RequireRecruit -and $log -match '\[JOIN_TIMEOUT\]|\[JOIN_(?:ADMISSION|RESULT|SERVER)\][^\r\n]*pass=0') { $failures.Add('RECRUIT_FAILED') }
if($RequireBarracks -and $log -match '\[BUY_TIMEOUT\]|\[BUY_(?:ADMISSION|RESULT|SERVER|LIMIT|UNPAID_GUARD|PAYMENT|FACTION)\][^\r\n]*pass=0') { $failures.Add('BARRACKS_FAILED') }
if($RequireSupportArea) {
    if($log -match '\[AREA_(?:CASE|SPAWN)\][^\r\n]*pass=0') { $failures.Add('SUPPORT_AREA_FAILED') }
    if($Client) {
        foreach($part in @('Ural4320_FIA_repair_box.et','Ural4320_repair_box.et','M923A1_repair_box.et')) {
            if($log -notmatch ('\[AREA_CASE\] pass=1 server=0 before_mismatch=[01] station_unchanged=1 center_error=[\d.eE+-]+ prefab=[^\r\n]*'+[regex]::Escape($part))) { $failures.Add("SUPPORT_AREA_$part") }
        }
        if($log -notmatch '\[AREA_CASE\] pass=1 server=0 before_mismatch=1') { $failures.Add('SUPPORT_AREA_REPRODUCTION') }
        foreach($area in [regex]::Matches($log,'\[AREA_CASE\][^\r\n]*center_error=([\d.eE+-]+)')) {
            $distance=[double]::Parse($area.Groups[1].Value,[Globalization.CultureInfo]::InvariantCulture)
            if($distance -lt 0 -or $distance -ge 0.002) { $failures.Add('SUPPORT_AREA_CENTER') }
        }
    } else {
        foreach($vehicle in @('Ural4320_repair_FIA.et','Ural4320_repair.et','M923A1_repair.et')) {
            if($log -notmatch ('\[AREA_SPAWN\] pass=1 prefab=[^\r\n]*'+[regex]::Escape($vehicle))) { $failures.Add("SUPPORT_AREA_$vehicle") }
        }
    }
}
$manifest=$null
if($launcher -notmatch 'AICF_RUNTIME_MANIFEST_JSON=({[^\r\n]+})') { $failures.Add('MANIFEST') }
else { $manifest=$matches[1] | ConvertFrom-Json }
if($manifest) {
    $expectedRole='Server'; if($Client) { $expectedRole='Client' }
    if($manifest.role -cne $expectedRole -or $manifest.variant -notin (@($catalog.variants.name)+@('StockCoopUSSR'))) { $failures.Add('LAUNCH_ROLE_VARIANT') }
    $matchMode='Standard'
    if($manifest.PSObject.Properties['matchMode']){$matchMode=$manifest.matchMode}
    elseif($manifest.variant -eq 'StockCoopUSSR'){$matchMode='CoopUSSR'}
    $difficulty='Easy'; if($manifest.PSObject.Properties['difficulty']){$difficulty=$manifest.difficulty}
    $plan=$null
    try { $plan=Resolve-AICFScenario -Catalog $catalog -Variant $manifest.variant -MatchMode $matchMode -Difficulty $difficulty }
    catch { $failures.Add('SCENARIO_MANIFEST') }
    if(!$plan){ $plan=Resolve-AICFScenario -Catalog $catalog -Variant Stock }
    $mode=$plan.playerSide; if(!$mode){$mode='STANDARD'}
    if($manifest.PSObject.Properties['scenario'] -and $manifest.scenario -cne $plan.scenario){$failures.Add('SCENARIO_MANIFEST')}
    if($RequireRecruit -and ($plan.baseVariant -ne 'Stock' -or $mode -ne 'USSR')) { $failures.Add('RECRUIT_VARIANT') }
    if($RequireBarracks -and ($plan.baseVariant -ne 'Stock' -or ($mode -ne 'STANDARD' -and $mode -ne $BarracksFaction))) { $failures.Add('BARRACKS_VARIANT') }
    $argsList=@($manifest.arguments)
    $indices=@(for($i=0;$i -lt $argsList.Count;$i++) { if($argsList[$i] -ceq '-aicfCoopProbe') {$i} })
    if($indices.Count -ne 1 -or $indices[0]+1 -ge $argsList.Count -or $argsList[$indices[0]+1] -cne $mode) { $failures.Add('PROBE_MODE') }
    if($RequireBarracks) {
        $factionIndices=@(for($i=0;$i -lt $argsList.Count;$i++) { if($argsList[$i] -ceq '-aicfBarracksFaction') {$i} })
        if(($factionIndices.Count -eq 0 -and $BarracksFaction -ne 'USSR') -or $factionIndices.Count -gt 1 -or
            ($factionIndices.Count -eq 1 -and ($factionIndices[0]+1 -ge $argsList.Count -or $argsList[$factionIndices[0]+1] -cne $BarracksFaction))) { $failures.Add('BARRACKS_FACTION_ARGUMENT') }
    }
    if($Client) {
        if($launcher -notmatch '\[SERVER_READY\] process_id=\d+ port=\d+ log=') { $failures.Add('CLIENT_READINESS') }
        if(!$RequireRecruit -and !$RequireBarracks -and [regex]::Matches($log,'\[COOP_CLIENT_DONE\] pass=1 samples=\d+').Count -ne 1) { $failures.Add('CLIENT_READBACK') }
        if($RequireBarracks) {
            if([regex]::Matches($log,'\[BUY_ENTER_REQUEST\] sent=1').Count -ne 1 -or [regex]::Matches($log,'\[BUY_SENT\] pass=1 leader=1 members=0 max=4').Count -ne 1) { $failures.Add('BARRACKS_ORDER') }
            foreach($count in 0..3) {
                if([regex]::Matches($log,"\[BUY_ADMISSION\] pass=1 leader=1 recruitable=1 members=$count max=4").Count -ne 1) { $failures.Add("BARRACKS_ADMISSION_$count") }
            }
            if([regex]::Matches($log,'\[BUY_ADMISSION\]').Count -ne 4) { $failures.Add('BARRACKS_ADMISSION_COUNT') }
            if([regex]::Matches($log,'\[BUY_RESULT\] pass=1 recruited=1 members=4').Count -ne 1) { $failures.Add('BARRACKS_READBACK') }
            if([regex]::Matches($log,'\[BUY_LIMIT\] pass=1 members=4 max=4').Count -ne 1) { $failures.Add('BARRACKS_LIMIT') }
            if($log -notmatch '\[COOP_CLIENT_PLAYER\] pass=1 entity=') { $failures.Add('PLAYER_SPAWN') }
        }
        if($RequireRecruit) {
            if([regex]::Matches($log,'\[JOIN_VOLUNTEER\] pass=1 leader=0').Count -ne 1) { $failures.Add('LEADER_VOTE_REQUEST') }
            if([regex]::Matches($log,'\[JOIN_ADMISSION\] pass=1 leader=1 menu=1 volunteer=1 recruitable=1').Count -ne 1) { $failures.Add('RECRUIT_ADMISSION') }
            if([regex]::Matches($log,'\[JOIN_RESULT\] pass=1 recruited=1 members=1\b').Count -ne 1) { $failures.Add('RECRUIT_READBACK') }
            foreach($faction in @('US','USSR','FIA')) {
                $visible=0; if($faction -eq 'USSR') {$visible=1}
                if([regex]::Matches($log,"\[JOIN_WELCOME\] faction=$faction visible=$visible\b").Count -ne 1) { $failures.Add("WELCOME_$faction") }
            }
            if([regex]::Matches($log,'\[JOIN_WELCOME\]').Count -ne 3) { $failures.Add('WELCOME_COUNT') }
            if($log -notmatch '\[COOP_CLIENT_PLAYER\] pass=1 entity=') { $failures.Add('PLAYER_SPAWN') }
        }
        if($RequirePlayer -and $log -notmatch '\[COOP_CLIENT_PLAYER\] pass=1 entity=') { $failures.Add('PLAYER_SPAWN') }
        if($RequirePlayer) {
            $opponent='US'; if($mode -eq 'US'){$opponent='USSR'}
            foreach($side in @($opponent,'FIA')) {
                if([regex]::Matches($log,("\[COOP_CLIENT_FACTION\] faction=$side accepted=0 pass=1")).Count -ne 1) { $failures.Add("FACTION_DENIED_$side") }
            }
            if($log -match '\[COOP_CLIENT_FACTION\][^\r\n]*pass=0') { $failures.Add('FACTION_ACCESS_FAILED') }
        }
        if($RequireWelcome) {
            $human=$plan.usFaction; $opponent=$plan.ussrFaction
            if($mode -eq 'USSR'){$human=$plan.ussrFaction; $opponent=$plan.usFaction}
            foreach($side in @($human,$opponent,'FIA')) {
                $visible=0; if($side -eq $human){$visible=1}
                if($log -notmatch ('\[COOP_WELCOME\] faction='+[regex]::Escape($side)+" visible=$visible\b")){$failures.Add("FACTION_WELCOME_$side")}
            }
            foreach($row in [regex]::Matches($log,'\[COOP_WELCOME\] faction=(\w+) visible=1')) {
                if($row.Groups[1].Value -cne $human){$failures.Add('FACTION_WELCOME_EXTRA')}
            }
        }
        if($RequirePlayer -or $RequireRecruit -or $RequireBarracks) {
            $playerIndices=@(for($i=0;$i -lt $argsList.Count;$i++) { if($argsList[$i] -ceq '-aicfCoopProbePlayer') {$i} })
            if($playerIndices.Count -ne 1 -or $playerIndices[0]+1 -ge $argsList.Count -or $argsList[$playerIndices[0]+1] -cne '1') { $failures.Add('PLAYER_MODE') }
        }
    } else {
        if($RequireBarracks) {
            $source='{6F72F05752ED62A8}Prefabs/Groups/OPFOR/Group_USSR_FireGroup_Guard.et'
            if($BarracksFaction -eq 'US') { $source='{0A8E20F50DA233E1}Prefabs/Groups/BLUFOR/Group_US_FireTeam_Guard.et' }
            if([regex]::Matches($log,('\[BUY_CREATED\] prefab='+[regex]::Escape($source))).Count -ne 1) { $failures.Add('BARRACKS_SOURCE') }
            if([regex]::Matches($log,"\[BUY_FACTION\] pass=1 faction=$BarracksFaction\b").Count -ne 1) { $failures.Add('BARRACKS_FACTION') }
            if([regex]::Matches($log,'\[BUY_ROSTER\] count=4 recruitable=4').Count -ne 1 -or [regex]::Matches($log,'\[BUY_SERVER\] pass=1 members=4').Count -ne 1) { $failures.Add('BARRACKS_AUTHORITY') }
            if([regex]::Matches($log,'\[BUY_UNPAID_GUARD\] pass=1 count=4').Count -ne 1) { $failures.Add('BARRACKS_UNPAID_GUARD') }
            $payment=[regex]::Matches($log,'\[BUY_PAYMENT\] pass=1 before=(\d+) after=(\d+) cost=(\d+) debited=(\d+) credited=(\d+)')
            if($payment.Count -ne 1 -or [int]$payment[0].Groups[3].Value -le 0 -or [int]$payment[0].Groups[4].Value -lt [int]$payment[0].Groups[3].Value -or [int]$payment[0].Groups[1].Value - [int]$payment[0].Groups[2].Value -ne [int]$payment[0].Groups[4].Value - [int]$payment[0].Groups[5].Value) { $failures.Add('BARRACKS_PAYMENT') }
        }
        if($RequireRecruit -and ([regex]::Matches($log,'\[JOIN_SERVER\] pass=1 player=[1-9]\d* recruited=1').Count -ne 1 -or $log -notmatch '\[JOIN_TARGET\] entity=-?\d+ recruitable=1 prefab=' -or $log -notmatch '\[JOIN_SOURCE\] kind=STOCK_GROUP_CATALOG prefab=')) { $failures.Add('RECRUIT_AUTHORITY') }
        if($RequireRecruit -and ([regex]::Matches($log,'\[JOIN_LEADER_SETUP\] removed=1').Count -ne 1 -or [regex]::Matches($log,'\[JOIN_LEADER_RECOVERED\] pass=1').Count -ne 1)) { $failures.Add('LEADER_VOTE_AUTHORITY') }
        $done=[regex]::Matches($log,'\[COOP_DONE\] mode=(USSR|US|STANDARD) checks=(\d+) failed=0 elapsed_ms=(\d+)')
        if($done.Count -ne 1 -or $done[0].Groups[1].Value -cne $mode -or [int]$done[0].Groups[3].Value -lt $MinimumObservationMs) { $failures.Add('COMPLETION') }
        $expected=@('US_ROSTER','USSR_ROSTER','QUEUE_SETTLED','US_TICKETS','USSR_TICKETS','COMMANDERS','HELPERS','BUDGET_FLOOR','STANDARD_POLICY','MIRROR_POLICY','STABLE_ROSTER','REPLICATED_MODE','FACTION_ACCESS','AUTO_CONSTRUCTION','CONTENT_FACTIONS','HEADER_MODE')
        if($mode -ne 'STANDARD') { $expected+=@('ZERO_PLAYER_TICKETS_NO_DEFEAT','CONFLICT_REJECTED','NO_GROUP_SUMMARY') }
        if($ExpectedWinner) { $expected+='TERRITORIAL_WINNER' }
        $cases=[regex]::Matches($log,'\[COOP_CASE\] case=(\w+) pass=1')
        if($cases.Count -ne $expected.Count -or ($done.Count -eq 1 -and [int]$done[0].Groups[2].Value -ne $expected.Count)) { $failures.Add('CASE_COUNT') }
        foreach($name in $expected) { if(@($cases | Where-Object {$_.Groups[1].Value -ceq $name}).Count -ne 1) { $failures.Add("CASE_$name") } }
        $ussr=10; if($mode -eq 'USSR') {$ussr=0}; $us=10; if($mode -eq 'US'){$us=0}
        if($log -notmatch ("\[ROSTER_READY\][^\r\n]*us_groups=$us ussr_groups=$ussr\b")) { $failures.Add('ROSTER') }
        if($mode -ne 'STANDARD') {
            $runtimeSide=$plan.usFaction; if($mode -eq 'USSR'){$runtimeSide=$plan.ussrFaction}
            if($log -match ('\[(?:SPAWN_ATTEMPT_STARTED|GROUP_ROSTER_READY|REPLACEMENT_SPAWNED)\][^\r\n]*faction=(?:'+[regex]::Escape($mode)+'|'+[regex]::Escape($runtimeSide)+')\b')) { $failures.Add('PLAYER_COMBAT_SPAWN') }
        }
        if($manifest.PSObject.Properties['matchMode']) {
            $context=[regex]::Matches($log,('\[COOP_CONTEXT\] mode='+$mode+' us_faction='+[regex]::Escape($plan.usFaction)+' ussr_faction='+[regex]::Escape($plan.ussrFaction)+' nodes=(\d+)'))
            if($context.Count -ne 1 -or [int]$context[0].Groups[1].Value -le 0){$failures.Add('CONTENT_CONTEXT')}
            elseif($plan.baseVariant -match 'North' -and [int]$context[0].Groups[1].Value -ne 7){$failures.Add('NORTH_SCOPE')}
        }
        # Stage1Diagnostics normalizes runtime content keys to stable US/USSR.
        if($ExpectedWinner -and $log -notmatch ("\[VICTORY\][^\r\n]*winner="+[regex]::Escape($ExpectedWinner)+"\b[^\r\n]*reason=ALL_OBJECTIVES_UNCONTESTED")) { $failures.Add('TERRITORIAL_RESULT') }
    }
}
if($RequireHeaderMatrix) {
    if($Client){$failures.Add('HEADER_MATRIX_ROLE')}
    $expectedHeaders=0
    foreach($variant in $catalog.variants){foreach($side in @('US','USSR')){foreach($difficulty in @('Easy','Medium','Hard')){
        $expectedHeaders++
        if([regex]::Matches($log,("\[COOP_HEADER_CASE\] variant=$($variant.name) side=$side difficulty=$difficulty pass=1\b")).Count -ne 1){$failures.Add("HEADER_MATRIX_$($variant.name)_${side}_$difficulty")}
    }}}
    if([regex]::Matches($log,'\[COOP_HEADER_CASE\]').Count -ne $expectedHeaders -or $log -notmatch "\[COOP_HEADERS_DONE\] count=$expectedHeaders failed=0\b"){$failures.Add('HEADER_MATRIX_COMPLETION')}
}
"Co-op log diagnostics: script_errors=$([regex]::Matches($log,'SCRIPT\s+\([EF]\):').Count) other_errors=$([regex]::Matches($log,'\(E\):').Count)"
if($failures.Count) { $failures | ForEach-Object {"FAIL $_"}; exit 1 }
'Co-op full stopped log: PASS'
exit 0

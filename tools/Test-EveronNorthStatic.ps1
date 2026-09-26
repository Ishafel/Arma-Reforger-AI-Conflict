param([string]$RepositoryRoot = (Split-Path -Parent $PSScriptRoot))
$ErrorActionPreference = 'Stop'
$failures = [Collections.Generic.List[string]]::new()
function Require([bool]$Condition, [string]$Rule) { if (-not $Condition) { $failures.Add("${variant}:$Rule") } }
foreach ($variant in @('EveronNorthRHS', 'EveronNorth')) {
    $relativePath = 'AIConflictEveronRHS/Missions/AICF_RHS_Conflict_Everon_North.conf'
    $parentResource = '{57FA3D0337BE47E5}Missions/AICF_RHS_Conflict_Everon.conf'
    $resource = '{A1CF190919100000}Missions/AICF_RHS_Conflict_Everon_North.conf'
    if ($variant -eq 'EveronNorth') {
        $relativePath = 'AIConflictEveron/Missions/AICF_Conflict_Everon_North.conf'
        $parentResource = '{4C5D73A5614F41D9}Missions/AICF_Conflict_Everon.conf'
        $resource = '{A1CF190919300000}Missions/AICF_Conflict_Everon_North.conf'
    }
    $headerPath = Join-Path $RepositoryRoot $relativePath
    $header = Get-Content -LiteralPath $headerPath -Raw -Encoding UTF8
    $expected = @('MilitaryBaseAirfield', 'MilitaryHospital', 'MainBaseNorth', 'SmallBaseMaidensBay',
        'TownBaseMeaux', 'TownBaseTyrone', 'TownBaseKermovan', 'StartingPos06', 'StartingPos12')
    Require ($header.Contains('SCR_MissionHeaderCampaign : ' + [char]34 + $parentResource + [char]34)) 'NORTH_PARENT'
    Require ($header -match 'm_bCustomBaseWhitelist\s+1\b') 'NORTH_WHITELIST'
    Require ($header -match 'm_bEstablishingBasesEnabled\s+0\b') 'NORTH_NO_NEW_BASES'
    Require ($header -notmatch '(?m)^\s*(World|SystemsConfig)\b') 'NORTH_INHERITED_WORLD'
    $blocks = @([regex]::Matches($header, '(?s)SCR_CampaignCustomBase\s+"\{[A-F0-9]{16}\}"\s*\{([^{}]*)\}'))
    Require ($blocks.Count -eq 9) 'NORTH_BASE_COUNT'
    $names = @()
    $hqNames = @()
    $captureNames = @()
    foreach ($block in $blocks) {
        $body = $block.Groups[1].Value
        $name = [regex]::Match($body, 'm_sBaseName\s+"([^"]+)"').Groups[1].Value
        $names += $name
        Require ($body -match 'm_fRadioRange\s+2000\b') "NORTH_RADIO_RANGE_$name"
        if ($body -match 'm_bCanBeHQ\s+1\b') { $hqNames += $name }
        if ($body -match 'm_bIsControlPoint\s+1\b') { $captureNames += $name }
    }
    Require ((($names | Sort-Object) -join ',') -ceq (($expected | Sort-Object) -join ',')) 'NORTH_BASE_IDENTITIES'
    Require ((($hqNames | Sort-Object) -join ',') -ceq 'MilitaryBaseAirfield,MilitaryHospital') 'NORTH_HQ_PAIR'
    Require ($captureNames.Count -eq 7 -and @($captureNames | Where-Object { $_ -in $hqNames }).Count -eq 0) 'NORTH_SEVEN_CAPTURE_POINTS'
    $meta = Get-Content -LiteralPath ($headerPath + '.meta') -Raw
    Require ($meta.Contains($resource)) 'NORTH_RESOURCE_ID'
    foreach ($platform in @('PC','HEADLESS','XBOX_ONE','XBOX_SERIES','PS4')) {
        Require ($meta -match ('CONFResourceClass\s+' + $platform + '\b')) "NORTH_PLATFORM_$platform"
    }
}
if ($failures.Count) { $failures | ForEach-Object { Write-Output "[AICF][NORTH_STATIC][FAIL] $_" }; exit 1 }
Write-Output '[AICF][NORTH_STATIC][PASS] variants=2 bases=9 hq=2 capture_points=7 inherited_world=PASS'
exit 0

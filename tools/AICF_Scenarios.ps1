# Общий каталог запуска и генерации тонких MissionHeader. Не запускает игру.
function Get-AICFScenarioCatalog {
    param([string]$Path=(Join-Path $PSScriptRoot 'config/AICF_Scenarios.json'))
    $catalog=[IO.File]::ReadAllText($Path) | ConvertFrom-Json
    if($catalog.schema -ne 1){throw 'Unsupported AICF scenario catalog schema'}
    foreach($project in $catalog.localProjects.PSObject.Properties) {
        if($project.Name -cnotmatch '^[A-F0-9]{16}$' -or $project.Value -notmatch '^[A-Za-z0-9_]+$'){throw 'Invalid local addon project'}
    }
    $names=@{}
    $resources=@{}
    $stringPrefixes=@{}
    foreach($variant in $catalog.variants) {
        if($variant.name -notmatch '^[A-Za-z][A-Za-z0-9]+$' -or $names.ContainsKey($variant.name)){throw 'Invalid or duplicate scenario variant'}
        $names[$variant.name]=$true
        $graph=$catalog.graphs.PSObject.Properties[$variant.graph]
        if(!$graph -or $graph.Value.project -notmatch '^[A-Za-z0-9_]+/addon\.gproj$' -or
            (Split-Path -Parent $graph.Value.project) -notin @($catalog.localProjects.PSObject.Properties.Value)){throw "Invalid scenario graph: $($variant.name)"}
        if($graph.Value.externalAddons -isnot [bool] -or !$graph.Value.addons.Count){throw 'Invalid addon graph'}
        foreach($id in $graph.Value.addons){if($id -cnotmatch '^[A-F0-9]{16}$'){throw 'Invalid addon ID'}}
        foreach($side in @('US','USSR')) {
            if($graph.Value.factions.$side -cnotmatch '^[A-Za-z0-9_]+$'){throw 'Invalid runtime faction key'}
        }
        if(@($graph.Value.addons | Select-Object -Unique).Count -ne $graph.Value.addons.Count){throw 'Duplicate addon ID'}
        if($variant.standard.Count -ne 3){throw 'Incomplete scenario variant'}
        if($variant.stringGuidPrefix -cnotmatch '^A1CF26101847[A-F0-9]{2}$' -or $stringPrefixes.ContainsKey($variant.stringGuidPrefix)){throw 'Invalid or duplicate generated string prefix'}
        $stringPrefixes[$variant.stringGuidPrefix]=$true
        foreach($resource in $variant.standard) {
            if($resource -cnotmatch '^\{[A-F0-9]{16}\}Missions/[A-Za-z0-9_]+\.conf$' -or $resources.ContainsKey($resource.Substring(1,16))){throw 'Invalid or duplicate standard header'}
            $resources[$resource.Substring(1,16)]=$true
        }
        foreach($side in @('US','USSR')) {
            if($variant.coop.$side.Count -ne 3){throw 'Incomplete Co-op header IDs'}
            foreach($id in $variant.coop.$side) {
                if($id -cnotmatch '^[A-F0-9]{16}$' -or $resources.ContainsKey($id)){throw 'Invalid or duplicate Co-op header ID'}
                $resources[$id]=$true
            }
            foreach($text in @($variant.sideRu.$side,$variant.sideEn.$side,$variant.titleRu,$variant.titleEn)) {
                if([string]::IsNullOrWhiteSpace($text) -or $text -match '["\r\n]'){throw 'Invalid scenario title'}
            }
        }
    }
    return $catalog
}

function Resolve-AICFScenario {
    param(
        [Parameter(Mandatory=$true)]$Catalog,
        [Parameter(Mandatory=$true)][string]$Variant,
        [ValidateSet('Standard','CoopUS','CoopUSSR')][string]$MatchMode='Standard',
        [ValidateSet('Easy','Medium','Hard')][string]$Difficulty='Easy'
    )
    $Difficulty=@('Easy','Medium','Hard') | Where-Object {$_ -ieq $Difficulty}
    $MatchMode=@('Standard','CoopUS','CoopUSSR') | Where-Object {$_ -ieq $MatchMode}
    $canonical=$Variant
    if($Variant -eq 'StockCoopUSSR') {
        if($MatchMode -eq 'CoopUS'){throw 'StockCoopUSSR conflicts with CoopUS'}
        $canonical='Stock'; $MatchMode='CoopUSSR'
    }
    $entry=@($Catalog.variants | Where-Object name -EQ $canonical)
    if($entry.Count -ne 1){throw "Unknown scenario variant: $Variant"}
    $entry=$entry[0]
    $graph=$Catalog.graphs.PSObject.Properties[$entry.graph].Value
    $index=@('Easy','Medium','Hard').IndexOf($Difficulty)
    $resource=$entry.standard[$index]
    $side=''
    $mode=0
    $commander='BOTH'
    $suffix=$entry.profileSuffix
    if($MatchMode -ne 'Standard') {
        $side='US'; $mode=1; $commander='USSR'
        if($MatchMode -eq 'CoopUSSR'){$side='USSR'; $mode=2; $commander='US'}
        $stem=[IO.Path]::GetFileNameWithoutExtension($entry.standard[0].Substring(18))
        $difficultySuffix=''; if($index -gt 0){$difficultySuffix='_'+@('Easy','Medium','Hard')[$index]}
        $resource='{'+$entry.coop.$side[$index]+'}Missions/'+$stem+'_Coop_'+$side+$difficultySuffix+'.conf'
        $suffix+='-'+$MatchMode
    }
    [pscustomobject]@{
        variant=$Variant; baseVariant=$entry.name; matchMode=$MatchMode; mode=$mode
        playerSide=$side; commander=$commander; difficulty=$Difficulty; difficultyIndex=$index
        scenario=$resource; missionHeader=$resource.Substring(18); profileSuffix=$suffix
        project=$graph.project; addonIds=($graph.addons -join ','); externalAddons=$graph.externalAddons
        usFaction=$graph.factions.US; ussrFaction=$graph.factions.USSR
        definition=$entry
    }
}

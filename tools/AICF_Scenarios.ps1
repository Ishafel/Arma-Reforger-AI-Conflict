# Logical launch plans; no per-mode/difficulty mission resources are generated.
function Get-AICFScenarioCatalog {
    param([string]$Path=(Join-Path $PSScriptRoot 'config/AICF_Scenarios.json'))
    $catalog=[IO.File]::ReadAllText($Path)|ConvertFrom-Json
    if($catalog.schema -ne 2){throw 'Unsupported AICF scenario catalog schema'}
    foreach($project in $catalog.localProjects.PSObject.Properties){
        if($project.Name -cnotmatch '^[A-F0-9]{16}$' -or $project.Value -notmatch '^[A-Za-z0-9_]+$'){throw 'Invalid local addon project'}
    }
    if(@($catalog.localProjects.PSObject.Properties).Count -ne 7){throw 'Expected seven local projects'}
    $targets=@{}
    foreach($mission in $catalog.nativeMissions.PSObject.Properties){
        $m=$mission.Value
        if($m.resource -cnotmatch '^\{[A-F0-9]{16}\}Missions/[A-Za-z0-9_]+\.conf$' -or $targets.ContainsKey($m.resource)){throw 'Invalid or duplicate native mission'}
        if($m.owner -and $m.owner -cnotmatch '^[A-F0-9]{16}$'){throw 'Invalid native mission owner'}
        $targets[$m.resource]=$true
    }
    if($targets.Count -ne 4){throw 'Expected four native mission targets'}
    $names=@{};$pairs=@{}
    foreach($variant in $catalog.variants){
        if($variant.name -notmatch '^[A-Za-z][A-Za-z0-9]+$' -or $names.ContainsKey($variant.name)){throw 'Invalid or duplicate scenario variant'}
        $names[$variant.name]=$true
        if($variant.PSObject.Properties['standard'] -or $variant.PSObject.Properties['coop']){throw 'Retired mission presets are not supported'}
        if($variant.mapIndex -notin @(0,1,2) -or $variant.contentIndex -notin @(0,1,2)){throw 'Invalid menu indices'}
        $pair="$($variant.mapIndex):$($variant.contentIndex)"
        if($pairs.ContainsKey($pair)){throw 'Duplicate menu combination'}
        $pairs[$pair]=$true
        $graph=$catalog.graphs.PSObject.Properties[$variant.graph]
        if(!$graph -or $graph.Value.project -notmatch '^[A-Za-z0-9_]+/addon\.gproj$' -or (Split-Path -Parent $graph.Value.project) -notin @($catalog.localProjects.PSObject.Properties.Value)){throw 'Invalid scenario graph'}
        $g=$graph.Value
        if($g.externalAddons -isnot [bool] -or !$g.addons.Count -or @($g.addons|Select-Object -Unique).Count -ne $g.addons.Count){throw 'Invalid addon graph'}
        foreach($id in $g.addons){if($id -cnotmatch '^[A-F0-9]{16}$'){throw 'Invalid addon ID'}}
        $mission=$catalog.nativeMissions.PSObject.Properties[$variant.mission]
        if(!$mission -or ($mission.Value.owner -and $mission.Value.owner -notin $g.addons)){throw 'Missing native mission dependency'}
        $expectedMission='StockArland'
        if($variant.mapIndex -gt 0){$expectedMission='StockEveron'}
        if($variant.contentIndex -gt 0){$expectedMission=$expectedMission.Replace('Stock','RHS')}
        if($variant.mission -cne $expectedMission){throw 'Native target does not match map/content'}
        foreach($side in @('US','USSR')){
            if($g.factions.$side -cnotmatch '^[A-Za-z0-9_]+$'){throw 'Invalid runtime faction key'}
            foreach($text in @($variant.sideRu.$side,$variant.sideEn.$side,$variant.titleRu,$variant.titleEn)){
                if([string]::IsNullOrWhiteSpace($text) -or $text -match '["\r\n]'){throw 'Invalid scenario title'}
            }
        }
    }
    if($pairs.Count -ne 9){throw 'Incomplete menu matrix'}
    return $catalog
}
function Resolve-AICFScenario {
    param(
        [Parameter(Mandatory=$true)]$Catalog,
        [Parameter(Mandatory=$true)][string]$Variant,
        [ValidateSet('Standard','CoopUS','CoopUSSR')][string]$MatchMode='Standard',
        [ValidateSet('Easy','Medium','Hard')][string]$Difficulty='Easy'
    )
    $entry=@($Catalog.variants|Where-Object name -EQ $Variant)
    if($entry.Count -ne 1){throw "Unknown scenario variant: $Variant"}
    $entry=$entry[0]
    $graph=$Catalog.graphs.PSObject.Properties[$entry.graph].Value
    $mission=$Catalog.nativeMissions.PSObject.Properties[$entry.mission].Value
    $difficultyName=@('Easy','Medium','Hard')|Where-Object {$_ -ieq $Difficulty}
    $modeName=@('Standard','CoopUS','CoopUSSR')|Where-Object {$_ -ieq $MatchMode}
    $index=@('Easy','Medium','Hard').IndexOf($difficultyName)
    $mode=@('Standard','CoopUS','CoopUSSR').IndexOf($modeName)
    $side='';$commander='BOTH';$suffix=$entry.profileSuffix
    if($mode -eq 1){$side='US';$commander='USSR'}
    if($mode -eq 2){$side='USSR';$commander='US'}
    if($mode){$suffix+='-'+$modeName}
    [pscustomobject]@{
        variant=$entry.name;baseVariant=$entry.name;matchMode=$modeName;mode=$mode
        playerSide=$side;commander=$commander;difficulty=$difficultyName;difficultyIndex=$index
        scenario=$mission.resource;missionHeader=$mission.resource;missionOwner=$mission.owner
        profileSuffix=$suffix;project=$graph.project;addonIds=($graph.addons -join ',')
        externalAddons=$graph.externalAddons;usFaction=$graph.factions.US;ussrFaction=$graph.factions.USSR
        selectionArguments=@('-aicfScenarioVariant',$entry.name,'-aicfMatchMode',[string]$mode,'-aicfDifficulty',[string]$index)
        definition=$entry
    }
}

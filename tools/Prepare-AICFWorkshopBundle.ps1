[CmdletBinding(DefaultParameterSetName='Prepare')]
param(
    [Parameter(Mandatory)][string]$BundlePath,
    [Parameter(Mandatory,ParameterSetName='Prepare')][string]$OutputPath,
    [Parameter(Mandatory,ParameterSetName='Check')][switch]$Check,
    [string]$RepositoryRoot=(Split-Path -Parent $PSScriptRoot)
)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'AICF_Scenarios.ps1')
$catalog=Get-AICFScenarioCatalog -Path (Join-Path $RepositoryRoot 'tools/config/AICF_Scenarios.json')
$source=(Resolve-Path -LiteralPath $BundlePath).ProviderPath.TrimEnd([char[]]'\/')
if(!(Test-Path -LiteralPath $source -PathType Container)){throw '[WORKSHOP_BUNDLE_PATH] Bundle must be a directory'}
$manifestPath=Join-Path $source 'manifest.json'
$manifest=[IO.File]::ReadAllText($manifestPath)|ConvertFrom-Json
if($manifest.version -ne 1 -or !$manifest.asset -or $manifest.asset.type -cne 'addon'){throw '[WORKSHOP_BUNDLE_SCHEMA] Expected native addon manifest version 1'}
$id=[string]$manifest.asset.id
$project=@($catalog.localProjects.PSObject.Properties|Where-Object Name -CEQ $id)
if($project.Count -ne 1){throw '[WORKSHOP_BUNDLE_OWNER] Unknown addon identity'}
if([string]$manifest.asset.version -cnotmatch '^(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)$'){throw '[WORKSHOP_BUNDLE_VERSION] Expected numeric major.minor.patch'}
$scenarioProperty=$manifest.asset.PSObject.Properties['scenarios']
if(($scenarioProperty -and $manifest.asset.scenarios -isnot [array]) -or $manifest.files -isnot [array] -or !$manifest.files.Count){throw '[WORKSHOP_BUNDLE_SCHEMA] Expected scenarios/files arrays'}
$scenarios=@()
if($scenarioProperty){$scenarios=$manifest.asset.scenarios}

# Native bundles are flat. Reject paths and links before copying any payload.
$files=@{}
foreach($file in $manifest.files){
    $name=[string]$file.name
    if($name -notmatch '^[^\\/:*?"<>|]+$' -or $name -in @('.','..','manifest.json') -or $files.ContainsKey($name)){throw '[WORKSHOP_BUNDLE_FILE_NAME] Unsafe or duplicate bundle filename'}
    $path=Join-Path $source $name
    $item=Get-Item -LiteralPath $path
    if($item.PSIsContainer -or ($item.Attributes -band [IO.FileAttributes]::ReparsePoint)){throw '[WORKSHOP_BUNDLE_FILE_LINK] Expected regular bundle file'}
    if([string]$file.sha256 -notmatch '^[A-Fa-f0-9]{64}$' -or $item.Length -ne [long]$file.size -or (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ine $file.sha256){throw "[WORKSHOP_BUNDLE_HASH] Payload differs from manifest: $name"}
    $files[$name]=$file
}
foreach($required in @('addon.gproj','data.pak','resourceDatabase.rdb')){if(!$files.ContainsKey($required)){throw "[WORKSHOP_BUNDLE_REQUIRED_FILE] Missing $required"}}
$gproj=[IO.File]::ReadAllText((Join-Path $source 'addon.gproj'))
if($gproj -cnotmatch ('GUID\s+"'+[regex]::Escape($id)+'"')){throw '[WORKSHOP_BUNDLE_PROJECT] Project GUID differs from asset identity'}

$setup='{A1CF261019500001}Missions/AICF_Conflict_Arland_Setup.conf'
$known=@{}
foreach($variant in $catalog.variants){
    if($variant.graph -cne $project[0].Value){continue}
    foreach($mode in @('Standard','CoopUS','CoopUSSR')){foreach($difficulty in @('Easy','Medium','Hard')){
        $preset=Resolve-AICFScenario -Catalog $catalog -Variant $variant.name -MatchMode $mode -Difficulty $difficulty
        $known[$preset.scenario]=$true
    }}
}
$isEntryOwner=$project[0].Value -ceq 'AIConflictArland'
if($isEntryOwner){$known[$setup]=$true}
$seen=@{}
$retained=[Collections.Generic.List[object]]::new()
foreach($scenario in $scenarios){
    $resource=[string]$scenario.gameId
    if(!$known.ContainsKey($resource) -or $seen.ContainsKey($resource)){throw '[WORKSHOP_CATALOG_IDENTITY] Unknown, foreign or duplicate scenario ID'}
    $seen[$resource]=$true
    if($isEntryOwner -and $resource -ceq $setup){
        if($scenario.name -cne 'AI Conflict'){throw '[WORKSHOP_CATALOG_ENTRY_NAME] Entry must have literal AI Conflict name'}
        $retained.Add($scenario)
    }
}
if($isEntryOwner -and $retained.Count -ne 1){throw '[WORKSHOP_CATALOG_ENTRY_MISSING] Universal entry missing from native manifest'}
$before=$scenarios.Count
$removed=$before-$retained.Count
if($Check){
    if($removed){throw "[WORKSHOP_CATALOG_EXPOSED_PRESETS] addon=$id old_scenarios=$removed"}
    "Workshop bundle catalog: PASS; addon=$id version=$($manifest.asset.version) scenarios=$before payload_files=$($files.Count)"
    return
}

$destination=$ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OutputPath).TrimEnd([char[]]'\/')
if(Test-Path -LiteralPath $destination){throw '[WORKSHOP_BUNDLE_OUTPUT_EXISTS] Output directory must be new'}
if($destination.StartsWith($source+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -or $source.StartsWith($destination+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw '[WORKSHOP_BUNDLE_OUTPUT_PATH] Output must be separate from the input bundle'}
[void](New-Item -ItemType Directory -Path $destination)
foreach($name in $files.Keys){
    $target=Join-Path $destination $name
    Copy-Item -LiteralPath (Join-Path $source $name) -Destination $target
    if((Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash -ine $files[$name].sha256){throw '[WORKSHOP_BUNDLE_COPY_HASH] Copied payload differs; incomplete output must not be published'}
}
if($scenarioProperty){$manifest.asset.scenarios=[object[]]$retained.ToArray()}
[IO.File]::WriteAllText((Join-Path $destination 'manifest.json'),($manifest|ConvertTo-Json -Depth 100),[Text.UTF8Encoding]::new($false))
"Workshop bundle prepared: addon=$id version=$($manifest.asset.version) scenarios_before=$before scenarios_after=$($retained.Count) removed=$removed payload_files=$($files.Count) output=$destination"

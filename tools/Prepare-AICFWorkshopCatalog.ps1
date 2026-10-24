[CmdletBinding(DefaultParameterSetName='Prepare')]
param(
    [Parameter(Mandatory,ParameterSetName='Prepare')][string]$PublishingRoot,
    [Parameter(Mandatory)][string]$OutputRoot,
    [Parameter(Mandatory)][ValidatePattern('^(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)$')][string]$ExpectedVersion,
    [Parameter(Mandatory,ParameterSetName='Check')][switch]$Check,
    [string]$RepositoryRoot=(Split-Path -Parent $PSScriptRoot)
)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'AICF_Scenarios.ps1')
$catalog=Get-AICFScenarioCatalog -Path (Join-Path $RepositoryRoot 'tools/config/AICF_Scenarios.json')
$projects=@($catalog.localProjects.PSObject.Properties|Sort-Object Name)
$bundleTool=Join-Path $PSScriptRoot 'Prepare-AICFWorkshopBundle.ps1'
$output=$ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OutputRoot).TrimEnd([char[]]'\/')

function Get-AICFWorkshopCatalogBundle([string]$Path,[string]$Id){
    $directory=Get-Item -LiteralPath $Path
    $manifestPath=Join-Path $Path 'manifest.json'
    $file=Get-Item -LiteralPath $manifestPath
    if(!$directory.PSIsContainer -or ($directory.Attributes -band [IO.FileAttributes]::ReparsePoint) -or ($file.Attributes -band [IO.FileAttributes]::ReparsePoint)){
        throw '[WORKSHOP_SET_PATH] Expected ordinary bundle directory and manifest'
    }
    $manifest=[IO.File]::ReadAllText($manifestPath)|ConvertFrom-Json
    if($manifest.asset.id -cne $Id){throw '[WORKSHOP_SET_OWNER] Bundle directory and asset identity differ'}
    if($manifest.asset.version -cne $ExpectedVersion){throw '[WORKSHOP_SET_VERSION] Every addon must match ExpectedVersion'}
    & $bundleTool -BundlePath $Path -Check -RepositoryRoot $RepositoryRoot|Out-Null
    return $manifest
}

function Test-AICFWorkshopCatalogSet {
    $reportPath=Join-Path $output 'catalog.json'
    if(!(Test-Path -LiteralPath $reportPath -PathType Leaf)){throw '[WORKSHOP_SET_REPORT] Missing completed catalog report'}
    try {$report=[IO.File]::ReadAllText($reportPath)|ConvertFrom-Json}
    catch {throw '[WORKSHOP_SET_REPORT] Invalid catalog report JSON'}
    if($report.schema -ne 1 -or $report.version -cne $ExpectedVersion -or $report.publication -cne 'NOT_RUN' -or $report.addons -isnot [array] -or $report.addons.Count -ne $projects.Count){
        throw '[WORKSHOP_SET_REPORT] Invalid schema, version or complete addon list'
    }
    $directories=@(Get-ChildItem -LiteralPath $output -Directory)
    if($directories.Count -ne $projects.Count -or @($directories|Where-Object Name -CNotIn $projects.Name).Count){throw '[WORKSHOP_SET_OWNERS] Expected exactly the seven catalog owners'}
    $total=0
    foreach($project in $projects){
        $rows=@($report.addons|Where-Object id -CEQ $project.Name)
        if($rows.Count -ne 1 -or $rows[0].project -cne $project.Value){throw '[WORKSHOP_SET_REPORT_OWNER] Missing, duplicate or foreign report owner'}
        $path=Join-Path $output $project.Name
        $manifest=Get-AICFWorkshopCatalogBundle $path $project.Name
        if($rows[0].manifest_sha256 -cne (Get-FileHash -LiteralPath (Join-Path $path 'manifest.json')).Hash){throw '[WORKSHOP_SET_MANIFEST_CHANGED] Prepared manifest differs from completed report'}
        $count=@($manifest.asset.scenarios|Where-Object {$null -ne $_}).Count
        if($rows[0].scenarios -ne $count){throw '[WORKSHOP_SET_REPORT_COUNT] Report and native scenario list differ'}
        $total+=$count
    }
    if($total -ne 1){throw '[WORKSHOP_SET_ENTRY] Exactly one common entry required'}
    "Workshop catalog: PASS; addons=$($projects.Count) scenarios=$total version=$ExpectedVersion publication=NOT_RUN"
}

if($Check){
    Test-AICFWorkshopCatalogSet
    return
}
if(Test-Path -LiteralPath $output){throw '[WORKSHOP_SET_OUTPUT_EXISTS] OutputRoot must be new'}
$source=(Resolve-Path -LiteralPath $PublishingRoot).ProviderPath.TrimEnd([char[]]'\/')
$rootItem=Get-Item -LiteralPath $source
if(!$rootItem.PSIsContainer -or ($rootItem.Attributes -band [IO.FileAttributes]::ReparsePoint)){throw '[WORKSHOP_SET_PATH] PublishingRoot must be an ordinary directory'}
if($output.StartsWith($source+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -or $source.StartsWith($output+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){
    throw '[WORKSHOP_SET_OUTPUT_PATH] OutputRoot must be separate from PublishingRoot'
}

# Validate every source before creating even the first output bundle.
$inputs=@{}
foreach($project in $projects){
    $path=Join-Path $source $project.Name
    if(!(Test-Path -LiteralPath $path -PathType Container)){throw ('[WORKSHOP_SET_MISSING] Missing addon '+$project.Name)}
    $manifest=Get-AICFWorkshopCatalogBundle $path $project.Name
    $inputs[$project.Name]=@{
        hash=(Get-FileHash -LiteralPath (Join-Path $path 'manifest.json')).Hash
        scenarios=@($manifest.asset.scenarios|Where-Object {$null -ne $_}).Count
    }
}
[void](New-Item -ItemType Directory -Path $output)
$rows=@()
foreach($project in $projects){
    $inputPath=Join-Path $source $project.Name
    $manifestPath=Join-Path $inputPath 'manifest.json'
    if((Get-FileHash -LiteralPath $manifestPath).Hash -cne $inputs[$project.Name].hash){throw '[WORKSHOP_SET_SOURCE_CHANGED] Source changed after preflight; incomplete output must not be used'}
    $destination=Join-Path $output $project.Name
    & $bundleTool -BundlePath $inputPath -OutputPath $destination -RepositoryRoot $RepositoryRoot|Out-Null
    if((Get-FileHash -LiteralPath $manifestPath).Hash -cne $inputs[$project.Name].hash){throw '[WORKSHOP_SET_SOURCE_CHANGED] Source changed while preparing; incomplete output must not be used'}
    $prepared=Get-AICFWorkshopCatalogBundle $destination $project.Name
    $rows+=@{
        id=$project.Name;project=$project.Value;scenarios_before=$inputs[$project.Name].scenarios
        scenarios=@($prepared.asset.scenarios|Where-Object {$null -ne $_}).Count
        source_manifest_sha256=$inputs[$project.Name].hash
        manifest_sha256=(Get-FileHash -LiteralPath (Join-Path $destination 'manifest.json')).Hash
    }
}
$report=@{schema=1;version=$ExpectedVersion;publication='NOT_RUN';addons=[object[]]$rows}
[IO.File]::WriteAllText((Join-Path $output 'catalog.json'),($report|ConvertTo-Json -Depth 10),[Text.UTF8Encoding]::new($false))
Test-AICFWorkshopCatalogSet

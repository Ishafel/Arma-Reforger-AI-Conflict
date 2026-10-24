param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $RepositoryRoot 'tools/AICF_Scenarios.ps1')
$catalog=Get-AICFScenarioCatalog -Path (Join-Path $RepositoryRoot 'tools/config/AICF_Scenarios.json')
$projects=@($catalog.localProjects.PSObject.Properties|Sort-Object Name)
$tool=Join-Path $RepositoryRoot 'tools/Prepare-AICFWorkshopCatalog.ps1'
$tempBase=[IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd([char[]]'\/')
$work=Join-Path $tempBase ('aicf-workshop-catalog-'+[guid]::NewGuid().ToString('N'))
[void](New-Item -ItemType Directory -Path $work)
$utf8=[Text.UTF8Encoding]::new($false)
$checks=0
function Save-Json([string]$Path,$Value){[IO.File]::WriteAllText($Path,($Value|ConvertTo-Json -Depth 100),$utf8)}
function New-SourceSet([string]$Name){
    $root=Join-Path $work $Name
    [void](New-Item -ItemType Directory -Path $root)
    foreach($project in $projects){
        $path=Join-Path $root $project.Name
        [void](New-Item -ItemType Directory -Path $path)
        $payload=@{'addon.gproj'=('GameProject { GUID "'+$project.Name+'" }');'data.pak'='opaque native payload';'resourceDatabase.rdb'='opaque resource index'}
        $files=@(foreach($name in $payload.Keys){
            [IO.File]::WriteAllText((Join-Path $path $name),$payload[$name],$utf8)
            $file=Get-Item -LiteralPath (Join-Path $path $name)
            @{name=$name;size=$file.Length;sha256=(Get-FileHash -LiteralPath $file.FullName).Hash}
        })
        $scenarios=@(foreach($variant in $catalog.variants){
            if($variant.graph -cne $project.Value){continue}
            foreach($mode in @('Standard','CoopUS','CoopUSSR')){foreach($difficulty in @('Easy','Medium','Hard')){
                $preset=Resolve-AICFScenario -Catalog $catalog -Variant $variant.name -MatchMode $mode -Difficulty $difficulty
                @{gameId=$preset.scenario;name='legacy'}
            }}
        })
        if($project.Value -ceq 'AIConflictArland'){$scenarios+=@{gameId='{A1CF261019500001}Missions/AICF_Conflict_Arland_Setup.conf';name='AI Conflict'}}
        $asset=@{id=$project.Name;version='0.2.3';type='addon';description='immutable'}
        if($project.Value -cne 'AIConflictCore'){$asset.scenarios=$scenarios}
        Save-Json "$path/manifest.json" @{version=1;asset=$asset;files=$files}
    }
    return $root
}
function Copy-Prepared([string]$Name){
    $destination=Join-Path $work $Name
    Copy-Item -LiteralPath $script:prepared -Destination $destination -Recurse
    return $destination
}
function Expect-Failure([string]$Name,[scriptblock]$Action,[string]$Rule){
    $failure=''
    try {& $Action|Out-Null}catch{$failure=$_.Exception.Message}
    if(!$failure.Contains($Rule)){throw "Unexpected $Name result: $failure"}
    $script:checks++
    "PASS $Name"
}
try{
    $source=New-SourceSet 'native'
    $sourceHashes=@{}
    foreach($file in Get-ChildItem -LiteralPath $source -File -Recurse){$sourceHashes[$file.FullName]=(Get-FileHash -LiteralPath $file.FullName).Hash}
    $prepared=Join-Path $work 'prepared'
    & $tool -PublishingRoot $source -OutputRoot $prepared -ExpectedVersion '0.2.3' -RepositoryRoot $RepositoryRoot|Out-Null
    $report=Get-Content "$prepared/catalog.json" -Raw|ConvertFrom-Json
    if($report.addons.Count -ne 7 -or ($report.addons|Measure-Object scenarios_before -Sum).Sum -ne 82 -or ($report.addons|Measure-Object scenarios -Sum).Sum -ne 1 -or $report.publication -cne 'NOT_RUN'){throw 'Incomplete catalog'}
    $checks++;'PASS complete-seven-addon-catalog'
    foreach($key in $sourceHashes.Keys){if((Get-FileHash -LiteralPath $key).Hash -cne $sourceHashes[$key]){throw 'Native source changed'}}
    foreach($project in $projects){foreach($name in @('addon.gproj','data.pak','resourceDatabase.rdb')){
        if((Get-FileHash "$source/$($project.Name)/$name").Hash -cne (Get-FileHash "$prepared/$($project.Name)/$name").Hash){throw 'Payload changed'}
    }}
    $checks++;'PASS immutable-input-and-all-payloads'
    & $tool -OutputRoot $prepared -ExpectedVersion '0.2.3' -Check -RepositoryRoot $RepositoryRoot|Out-Null
    $checks++;'PASS repeat-final-check'
    $last=$projects[-1].Name
    foreach($case in @('missing','version','hash','owner','entry')){
        $inputPath=New-SourceSet ('bad-'+$case)
        $out=Join-Path $work ('output-'+$case)
        $manifestPath="$inputPath/$last/manifest.json"
        $m=Get-Content $manifestPath -Raw|ConvertFrom-Json
        switch($case){
            'missing' {$inputPath=Join-Path $work 'absent';[void](New-Item -ItemType Directory -Path $inputPath);$rule='WORKSHOP_SET_MISSING'}
            'version' {$m.asset.version='0.2.2';Save-Json $manifestPath $m;$rule='WORKSHOP_SET_VERSION'}
            'hash' {[IO.File]::WriteAllText("$inputPath/$last/data.pak",'broken');$rule='WORKSHOP_BUNDLE_HASH'}
            'owner' {$m.asset.id='FFFFFFFFFFFFFFFF';Save-Json $manifestPath $m;$rule='WORKSHOP_SET_OWNER'}
            'entry' {
                $manifestPath="$inputPath/B52C5F6AEDBF423E/manifest.json"
                $m=Get-Content $manifestPath -Raw|ConvertFrom-Json
                $m.asset.scenarios[-1].name='incorrect'
                Save-Json $manifestPath $m
                $rule='WORKSHOP_CATALOG_ENTRY_NAME'
            }
        }
        Expect-Failure ('preflight-'+$case) {& $tool -PublishingRoot $inputPath -OutputRoot $out -ExpectedVersion '0.2.3' -RepositoryRoot $RepositoryRoot} $rule
        if(Test-Path -LiteralPath $out){throw 'Failed preflight created partial output'}
    }
    $existing=Join-Path $work 'existing'
    [void](New-Item -ItemType Directory -Path $existing)
    [IO.File]::WriteAllText("$existing/marker",'preserve')
    Expect-Failure 'existing-output' {& $tool -PublishingRoot $source -OutputRoot $existing -ExpectedVersion '0.2.3' -RepositoryRoot $RepositoryRoot} 'WORKSHOP_SET_OUTPUT_EXISTS'
    if([IO.File]::ReadAllText("$existing/marker") -cne 'preserve'){throw 'Existing output changed'}
    Expect-Failure 'nested-output' {& $tool -PublishingRoot $source -OutputRoot "$source/prepared" -ExpectedVersion '0.2.3' -RepositoryRoot $RepositoryRoot} 'WORKSHOP_SET_OUTPUT_PATH'
    Expect-Failure 'unprepared-native-root' {& $tool -OutputRoot $source -ExpectedVersion '0.2.3' -Check -RepositoryRoot $RepositoryRoot} 'WORKSHOP_SET_REPORT'
    Expect-Failure 'expected-version-mismatch' {& $tool -OutputRoot $prepared -ExpectedVersion '0.2.4' -Check -RepositoryRoot $RepositoryRoot} 'WORKSHOP_SET_REPORT'
    foreach($case in @('schema','duplicate','count','manifest','payload','legacy','extra-owner','json')){
        $out=Copy-Prepared ('changed-'+$case)
        $reportPath="$out/catalog.json"
        $r=Get-Content $reportPath -Raw|ConvertFrom-Json
        $arland="$out/B52C5F6AEDBF423E/manifest.json"
        switch($case){
            'schema' {$r.schema=2;Save-Json $reportPath $r;$rule='WORKSHOP_SET_REPORT'}
            'duplicate' {$r.addons[0].id=$r.addons[1].id;Save-Json $reportPath $r;$rule='WORKSHOP_SET_REPORT_OWNER'}
            'count' {$r.addons[0].scenarios=1;Save-Json $reportPath $r;$rule='WORKSHOP_SET_REPORT_COUNT'}
            'manifest' {[IO.File]::AppendAllText($arland,' ');$rule='WORKSHOP_SET_MANIFEST_CHANGED'}
            'payload' {[IO.File]::WriteAllText("$out/9178E5822AFE48EA/data.pak",'tampered');$rule='WORKSHOP_BUNDLE_HASH'}
            'legacy' {
                Copy-Item -LiteralPath "$source/B52C5F6AEDBF423E/manifest.json" -Destination $arland -Force
                $owner=@($r.addons|Where-Object id -CEQ 'B52C5F6AEDBF423E')[0]
                $owner.manifest_sha256=(Get-FileHash $arland).Hash
                $owner.scenarios=10
                Save-Json $reportPath $r
                $rule='WORKSHOP_CATALOG_EXPOSED_PRESETS'
            }
            'extra-owner' {[void](New-Item -ItemType Directory -Path "$out/FFFFFFFFFFFFFFFF");$rule='WORKSHOP_SET_OWNERS'}
            'json' {[IO.File]::WriteAllText($reportPath,'{broken');$rule='WORKSHOP_SET_REPORT'}
        }
        Expect-Failure ('changed-output-'+$case) {& $tool -OutputRoot $out -ExpectedVersion '0.2.3' -Check -RepositoryRoot $RepositoryRoot} $rule
    }
}finally{
    $target=[IO.Path]::GetFullPath($work)
    if(!$target.StartsWith($tempBase+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -or (Split-Path -Leaf $target) -notlike 'aicf-workshop-catalog-*'){throw 'Unsafe catalog fixture cleanup'}
    Remove-Item -LiteralPath $target -Recurse -Force
}
"Workshop catalog contracts: PASS; cases=$checks; seven owners; one entry; publication NOT_RUN"

param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $RepositoryRoot 'tools/AICF_Scenarios.ps1')
$catalog=Get-AICFScenarioCatalog -Path (Join-Path $RepositoryRoot 'tools/config/AICF_Scenarios.json')
$tool=Join-Path $RepositoryRoot 'tools/Prepare-AICFWorkshopBundle.ps1'
$tempBase=[IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd([char[]]'\/')
$work=Join-Path $tempBase ('aicf-workshop-bundle-'+[guid]::NewGuid().ToString('N'))
[void](New-Item -ItemType Directory -Path $work)
$utf8=[Text.UTF8Encoding]::new($false)
$setup='{A1CF261019500001}Missions/AICF_Conflict_Arland_Setup.conf'
$checks=0
function Save-Manifest($Path,$Manifest){[IO.File]::WriteAllText((Join-Path $Path 'manifest.json'),($Manifest|ConvertTo-Json -Depth 100),$utf8)}
function New-Bundle([string]$Name,[string]$Id){
 $path=Join-Path $work $Name
 [void](New-Item -ItemType Directory -Path $path)
 $project=@($catalog.localProjects.PSObject.Properties|Where-Object Name -CEQ $Id)[0].Value
 $payload=@{'addon.gproj'=('GameProject { GUID "'+$Id+'" }');'data.pak'='opaque fixture payload';'resourceDatabase.rdb'='opaque fixture resource index'}
 if($project -ceq 'AIConflictArland'){$payload['resourceDatabase.rdb']='Missions/AICF_Conflict_Arland_Setup.conf'}
 $files=@(foreach($name in $payload.Keys){[IO.File]::WriteAllText((Join-Path $path $name),$payload[$name],$utf8);$item=Get-Item -LiteralPath (Join-Path $path $name);@{name=$name;size=$item.Length;sha256=(Get-FileHash -LiteralPath $item.FullName).Hash}})
 $scenarios=@()
 if($project -ceq 'AIConflictArland'){$scenarios+=@{gameId=$setup;name='AI Conflict';description='fixture';playerCount=64}}
 $manifest=[pscustomobject]@{version=1;files=$files;asset=[pscustomobject]@{id=$Id;version='0.2.2';type='addon';description='must stay unchanged';scenarios=$scenarios}}
 Save-Manifest $path $manifest
 return $path
}
function Expect-Failure([string]$Name,[scriptblock]$Action,[string]$Rule){
 $failure=''
 try{& $Action|Out-Null}catch{$failure=$_.Exception.Message}
 if(!$failure.Contains($Rule)){throw "Unexpected $Name failure: $failure"}
 $script:checks++
 "PASS $Name"
}
try{
 foreach($project in $catalog.localProjects.PSObject.Properties){
  $inputPath=New-Bundle $project.Value $project.Name
  $output=Join-Path $work ($project.Value+'-prepared')
  $sourceManifestHash=(Get-FileHash -LiteralPath "$inputPath/manifest.json").Hash
  $before=Get-Content -LiteralPath "$inputPath/manifest.json" -Raw|ConvertFrom-Json
  & $tool -BundlePath $inputPath -OutputPath $output -RepositoryRoot $RepositoryRoot|Out-Null
  & $tool -BundlePath $output -Check -RepositoryRoot $RepositoryRoot|Out-Null
  $after=Get-Content -LiteralPath "$output/manifest.json" -Raw|ConvertFrom-Json
  $expected=0;if($project.Value -ceq 'AIConflictArland'){$expected=1}
  if($after.asset.scenarios.Count -ne $expected -or ($expected -eq 1 -and $after.asset.scenarios[0].gameId -cne $setup)){throw 'Wrong menu catalog'}
  if((Get-FileHash -LiteralPath "$inputPath/manifest.json").Hash -ne $sourceManifestHash){throw 'Input bundle mutated'}
  foreach($file in $before.files){if((Get-FileHash -LiteralPath (Join-Path $output $file.name)).Hash -ine $file.sha256){throw 'Payload changed'}}
  $before.asset.scenarios=@()
  $after.asset.scenarios=@()
  if(($before|ConvertTo-Json -Depth 100 -Compress) -cne ($after|ConvertTo-Json -Depth 100 -Compress)){throw 'Unrelated metadata changed'}
  $checks++;"PASS $($project.Value)-prepared"
 }
 $core=New-Bundle 'native-core-no-scenarios' '9178E5822AFE48EA'
 $coreManifest=Get-Content "$core/manifest.json" -Raw|ConvertFrom-Json
 $coreManifest.asset.PSObject.Properties.Remove('scenarios')
 Save-Manifest $core $coreManifest
 & $tool -BundlePath $core -Check -RepositoryRoot $RepositoryRoot|Out-Null
 & $tool -BundlePath $core -OutputPath "$core-prepared" -RepositoryRoot $RepositoryRoot|Out-Null
 $corePrepared=Get-Content "$core-prepared/manifest.json" -Raw|ConvertFrom-Json
 if($corePrepared.asset.scenarios -isnot [array] -or $corePrepared.asset.scenarios.Count){throw 'Core must explicitly clear obsolete scenarios'}
 $checks++;'PASS native-core-no-scenarios'
 $cases=@('hash','version','owner','foreign','duplicate-scenario','missing-entry','entry-name','unsafe-file','duplicate-file','schema','retired-index')
 foreach($case in $cases){
  $inputPath=New-Bundle ('bad-'+$case) 'B52C5F6AEDBF423E'
  $output=Join-Path $work ('bad-output-'+$case)
  $manifest=Get-Content -LiteralPath "$inputPath/manifest.json" -Raw|ConvertFrom-Json
  $rule=''
  switch($case){
   'hash' {[IO.File]::WriteAllText("$inputPath/data.pak",'tampered');$rule='WORKSHOP_BUNDLE_HASH'}
   'version' {$manifest.asset.version='0.2.2a';$rule='WORKSHOP_BUNDLE_VERSION'}
   'owner' {$manifest.asset.id='FFFFFFFFFFFFFFFF';$rule='WORKSHOP_BUNDLE_OWNER'}
   'foreign' {$manifest.asset.scenarios[0].gameId='{4C5D73A5614F41D9}Missions/AICF_Conflict_Everon.conf';$rule='WORKSHOP_CATALOG_IDENTITY'}
   'duplicate-scenario' {$manifest.asset.scenarios+=@($manifest.asset.scenarios[-1]);$rule='WORKSHOP_CATALOG_IDENTITY'}
   'missing-entry' {$manifest.asset.scenarios=@($manifest.asset.scenarios|Where-Object gameId -CNE $setup);$rule='WORKSHOP_CATALOG_ENTRY_MISSING'}
   'entry-name' {$manifest.asset.scenarios[-1].name='#AICF_Setup_Arland_Name';$rule='WORKSHOP_CATALOG_ENTRY_NAME'}
   'unsafe-file' {$manifest.files[0].name='../outside';$rule='WORKSHOP_BUNDLE_FILE_NAME'}
   'duplicate-file' {$manifest.files+=@($manifest.files[0]);$rule='WORKSHOP_BUNDLE_FILE_NAME'}
   'schema' {$manifest.asset.scenarios=$null;$rule='WORKSHOP_BUNDLE_SCHEMA'}
   'retired-index' {
    [IO.File]::AppendAllText("$inputPath/resourceDatabase.rdb","`nMissions/AICF_Conflict_Arland.conf")
    $row=@($manifest.files|Where-Object name -CEQ 'resourceDatabase.rdb')[0]
    $row.size=(Get-Item "$inputPath/resourceDatabase.rdb").Length
    $row.sha256=(Get-FileHash "$inputPath/resourceDatabase.rdb").Hash
    $rule='WORKSHOP_CATALOG_RESOURCE_INDEX'
   }
  }
  Save-Manifest $inputPath $manifest
  Expect-Failure $case {& $tool -BundlePath $inputPath -OutputPath $output -RepositoryRoot $RepositoryRoot} $rule
  if(Test-Path -LiteralPath $output){throw 'Invalid input created an output bundle'}
 }
 $inputPath=New-Bundle 'path-guards' 'B52C5F6AEDBF423E'
 $existing=Join-Path $work 'existing'
 [void](New-Item -ItemType Directory -Path $existing)
 [IO.File]::WriteAllText("$existing/marker",'preserve')
 Expect-Failure 'existing-output' {& $tool -BundlePath $inputPath -OutputPath $existing -RepositoryRoot $RepositoryRoot} 'WORKSHOP_BUNDLE_OUTPUT_EXISTS'
 if([IO.File]::ReadAllText("$existing/marker") -cne 'preserve'){throw 'Existing output modified'}
 Expect-Failure 'nested-output' {& $tool -BundlePath $inputPath -OutputPath "$inputPath/nested" -RepositoryRoot $RepositoryRoot} 'WORKSHOP_BUNDLE_OUTPUT_PATH'
}finally{
 $target=[IO.Path]::GetFullPath($work)
 if(!$target.StartsWith($tempBase+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -or (Split-Path -Leaf $target) -notlike 'aicf-workshop-bundle-*'){throw 'Unsafe fixture cleanup'}
 Remove-Item -LiteralPath $target -Recurse -Force
}
"Workshop bundle contracts: PASS; cases=$checks; immutable payload; one menu entry across seven addons"

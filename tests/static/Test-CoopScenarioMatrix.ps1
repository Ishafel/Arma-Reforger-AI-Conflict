param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $RepositoryRoot 'tools/AICF_Scenarios.ps1')
. (Join-Path $RepositoryRoot 'tools/lib/AICFLocalization.Common.ps1')
$catalog=Get-AICFScenarioCatalog
$entries=Read-AICFLocalization $RepositoryRoot
$tempBase=[IO.Path]::GetFullPath([IO.Path]::GetTempPath())
$work=Join-Path $tempBase ('aicf-coop-matrix-'+[guid]::NewGuid().ToString('N'))
[void](New-Item -ItemType Directory -Path $work)
$count=0; $launches=0; $negative=0
try {
    foreach($variant in $catalog.variants){foreach($mode in @('Standard','CoopUS','CoopUSSR')){foreach($difficulty in @('Easy','Medium','Hard')){
        $resolved=Resolve-AICFScenario -Catalog $catalog -Variant $variant.name -MatchMode $mode -Difficulty $difficulty
        if($resolved.scenario -cne $catalog.nativeMissions.PSObject.Properties[$variant.mission].Value.resource -or $resolved.missionHeader -cne $resolved.scenario){throw 'Native target mismatch'}
        $wanted=@('-aicfScenarioVariant',$variant.name,'-aicfMatchMode',[string]$resolved.mode,'-aicfDifficulty',[string]$resolved.difficultyIndex)
        if(($resolved.selectionArguments -join '|') -cne ($wanted -join '|')){throw 'Managed runtime selection mismatch'}
        $count++
    }}}
    $fakeRepo=Join-Path $work 'source space'; $fakeGame=Join-Path $work 'game'; $fakeServer=Join-Path $work 'server'; $fakeMods=Join-Path $work 'mods'
    foreach($folder in @($fakeRepo,$fakeGame,$fakeServer,$fakeMods,"$fakeGame/addons","$fakeServer/addons")){[void](New-Item -ItemType Directory $folder -Force)}
    foreach($project in $catalog.localProjects.PSObject.Properties.Value){[void](New-Item -ItemType Directory "$fakeRepo/$project" -Force);[IO.File]::WriteAllText("$fakeRepo/$project/addon.gproj",'fixture')}
    foreach($binary in @("$fakeGame/ArmaReforgerSteamDiag.exe","$fakeServer/ArmaReforgerServerDiag.exe")){[IO.File]::WriteAllText($binary,'fixture')}
    foreach($variant in $catalog.variants){foreach($side in @('US','USSR')){foreach($role in @('Server','Client')){
        $expected=Resolve-AICFScenario -Catalog $catalog -Variant $variant.name -MatchMode ('Coop'+$side) -Difficulty Hard
        $output=@(& powershell.exe -NoProfile -ExecutionPolicy Bypass -File "$RepositoryRoot/tools/Start-AICFRuntime.ps1" -Role $role -Variant $variant.name -MatchMode ('Coop'+$side) -Difficulty Hard -RepositoryRoot $fakeRepo -GameRoot $fakeGame -ServerRoot $fakeServer -RhsAddonsRoot $fakeMods -ProfileRoot "$work/profile-$launches" -DryRun 2>&1)
        if($LASTEXITCODE -ne 0){throw "Launcher failed: $output"}
        $line=@($output | Where-Object {$_ -like 'AICF_RUNTIME_MANIFEST_JSON=*'})
        if($line.Count -ne 1){throw 'Manifest missing'}
        $manifest=$line[0].Substring('AICF_RUNTIME_MANIFEST_JSON='.Length)|ConvertFrom-Json
        if($manifest.matchMode -cne $expected.matchMode -or $manifest.baseVariant -cne $variant.name -or $manifest.scenario -cne $expected.scenario){throw 'Manifest mode/header mismatch'}
        if($role -eq 'Server') {
            $argsList=@($manifest.arguments)
            foreach($pair in @(@('-server',$expected.scenario),@('-MissionHeader',$expected.missionHeader),@('-aicfAICommanderMode',$expected.commander))){$i=[array]::IndexOf($argsList,$pair[0]);if($i -lt 0 -or $argsList[$i+1] -cne $pair[1]){throw "Wrong launcher argument $($pair[0])"}}
        }
        $launches++
    }}}
    foreach($mutation in @('duplicate-variant','duplicate-native','bad-project','missing-mission','missing-faction','duplicate-addon','duplicate-pair','retired-preset','bad-owner')) {
        $copy=([IO.File]::ReadAllText("$RepositoryRoot/tools/config/AICF_Scenarios.json")|ConvertFrom-Json)
        switch($mutation) {
            'duplicate-variant' {$copy.variants[1].name=$copy.variants[0].name}
            'duplicate-native' {$copy.nativeMissions.StockEveron.resource=$copy.nativeMissions.StockArland.resource}
            'bad-project' {$copy.graphs.AIConflictArland.project='../addon.gproj'}
            'missing-mission' {$copy.variants[0].mission='Absent'}
            'missing-faction' {$copy.graphs.AIConflictArland.factions.US=''}
            'duplicate-addon' {$copy.graphs.AIConflictArland.addons+=@($copy.graphs.AIConflictArland.addons[0])}
            'duplicate-pair' {$copy.variants[1].mapIndex=$copy.variants[0].mapIndex;$copy.variants[1].contentIndex=$copy.variants[0].contentIndex}
            'retired-preset' {$copy.variants[0]|Add-Member -NotePropertyName standard -NotePropertyValue @('retired')}
            'bad-owner' {$copy.nativeMissions.RHSArland.owner='FFFFFFFFFFFFFFFF'}
        }
        [IO.File]::WriteAllText("$work/mutant.json",($copy|ConvertTo-Json -Depth 12))
        $rejected=$false; try{Get-AICFScenarioCatalog -Path "$work/mutant.json"|Out-Null}catch{$rejected=$true}
        if(!$rejected){throw "Mutation escaped: $mutation"};$negative++
    }
    $rejected=$false;try{Resolve-AICFScenario -Catalog $catalog -Variant StockCoopUSSR|Out-Null}catch{$rejected=$true}
    if(!$rejected){throw 'Retired alias accepted'}
    $lower=Resolve-AICFScenario -Catalog $catalog -Variant Stock -MatchMode coopus -Difficulty hard
    if($lower.matchMode -cne 'CoopUS' -or $lower.difficultyIndex -ne 2){throw 'Case normalization failed'}
} finally {
    $target=[IO.Path]::GetFullPath($work)
    if(!$target.StartsWith($tempBase,[StringComparison]::OrdinalIgnoreCase) -or (Split-Path -Leaf $target) -notlike 'aicf-coop-matrix-*'){throw 'Unsafe cleanup path'}
    Remove-Item -LiteralPath $target -Recurse -Force
}
"Co-op matrix: PASS; logical_plans=$count launcher_manifests=$launches negative_catalog=$negative retired_alias=REJECTED"

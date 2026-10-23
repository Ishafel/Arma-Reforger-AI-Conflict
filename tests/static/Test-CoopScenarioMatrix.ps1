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
    $generated=@(& powershell.exe -NoProfile -ExecutionPolicy Bypass -File "$RepositoryRoot/tools/Update-AICFCoopScenarios.ps1" -RepositoryRoot $RepositoryRoot -Check 2>&1)
    if($LASTEXITCODE -ne 0){throw "Stale scenarios: $generated"}
    foreach($variant in $catalog.variants){foreach($side in @('US','USSR')){foreach($difficulty in @('Easy','Medium','Hard')){
        $resolved=Resolve-AICFScenario -Catalog $catalog -Variant $variant.name -MatchMode ('Coop'+$side) -Difficulty $difficulty
        $path=Join-Path $RepositoryRoot ((Split-Path -Parent $resolved.project)+'/'+$resolved.missionHeader)
        $header=[IO.File]::ReadAllText($path); $meta=[IO.File]::ReadAllText($path+'.meta')
        $legacy=$variant.name -eq 'Stock' -and $side -eq 'USSR' -and $difficulty -ne 'Easy'
        $parent=$variant.standard[$resolved.difficultyIndex]
        if($legacy){$parent='{A1CF261018460001}Missions/AICF_Conflict_Arland_Coop_USSR.conf'}
        if($header -notmatch ('^SCR_MissionHeaderCampaign\s*:\s*"'+[regex]::Escape($parent)+'"')){throw "Parent mismatch: $path"}
        if(!$legacy -and $header -notmatch ("m_eAICFMatchMode\s+"+$resolved.mode+'\b')){throw "Mode mismatch: $path"}
        if($header -match '(?m)^\s*(World|SystemsConfig|m_aCampaignCustomBaseList|m_bCustomBaseWhitelist)\b'){throw "Header owns topology: $path"}
        if(!$meta.Contains($resolved.scenario)){throw "Resource identity mismatch: $path"}
        foreach($platform in @('PC','HEADLESS','XBOX_ONE','XBOX_SERIES','PS4')){if($meta -notmatch "CONFResourceClass $platform\b"){throw "Missing platform: $path"}}
        foreach($key in [regex]::Matches($header,'"#(AICF_[A-Za-z0-9_]+)"')){if(!$entries.Contains($key.Groups[1].Value)){throw "Missing localization: $path"}}
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
    foreach($mutation in @('duplicate-variant','duplicate-resource','bad-project','incomplete-sides','missing-faction','duplicate-addon','duplicate-string-prefix','unmanaged-string-prefix')) {
        $copy=([IO.File]::ReadAllText("$RepositoryRoot/tools/config/AICF_Scenarios.json")|ConvertFrom-Json)
        switch($mutation) {
            'duplicate-variant' {$copy.variants[1].name=$copy.variants[0].name}
            'duplicate-resource' {$copy.variants[1].coop.US[0]=$copy.variants[0].coop.US[0]}
            'bad-project' {$copy.graphs.AIConflictArland.project='../addon.gproj'}
            'incomplete-sides' {$copy.variants[0].coop.US=@('0000000000000001')}
            'missing-faction' {$copy.graphs.AIConflictArland.factions.US=''}
            'duplicate-addon' {$copy.graphs.AIConflictArland.addons+=@($copy.graphs.AIConflictArland.addons[0])}
            'duplicate-string-prefix' {$copy.variants[1].stringGuidPrefix=$copy.variants[0].stringGuidPrefix}
            'unmanaged-string-prefix' {$copy.variants[0].stringGuidPrefix='F0000000000000'}
        }
        [IO.File]::WriteAllText("$work/mutant.json",($copy|ConvertTo-Json -Depth 12))
        $rejected=$false; try{Get-AICFScenarioCatalog -Path "$work/mutant.json"|Out-Null}catch{$rejected=$true}
        if(!$rejected){throw "Mutation escaped: $mutation"};$negative++
    }
    $lower=Resolve-AICFScenario -Catalog $catalog -Variant Stock -MatchMode coopus -Difficulty hard
    if($lower.matchMode -cne 'CoopUS' -or $lower.difficultyIndex -ne 2){throw 'Case normalization failed'}
    $future=([IO.File]::ReadAllText("$RepositoryRoot/tools/config/AICF_Scenarios.json")|ConvertFrom-Json)
    $new=($future.variants[0]|ConvertTo-Json -Depth 8|ConvertFrom-Json)
    $new.name='FutureMap'; $new.standard=@('{F000000000000001}Missions/FutureMap.conf','{F000000000000002}Missions/FutureMap_Medium.conf','{F000000000000003}Missions/FutureMap_Hard.conf')
    $new.stringGuidPrefix='A1CF26101847FF'
    $new.coop.US=@('F000000000000011','F000000000000012','F000000000000013'); $new.coop.USSR=@('F000000000000021','F000000000000022','F000000000000023')
    $future.variants+=@($new); [IO.File]::WriteAllText("$work/future.json",($future|ConvertTo-Json -Depth 12))
    $future=Get-AICFScenarioCatalog -Path "$work/future.json"
    $resolved=Resolve-AICFScenario -Catalog $future -Variant FutureMap -MatchMode CoopUS -Difficulty Hard
    if($resolved.scenario -cne '{F000000000000013}Missions/FutureMap_Coop_US_Hard.conf'){throw 'Future map needs launcher special-case'}
} finally {
    $target=[IO.Path]::GetFullPath($work)
    if(!$target.StartsWith($tempBase,[StringComparison]::OrdinalIgnoreCase) -or (Split-Path -Leaf $target) -notlike 'aicf-coop-matrix-*'){throw 'Unsafe cleanup path'}
    Remove-Item -LiteralPath $target -Recurse -Force
}
"Co-op matrix: PASS; headers=$count launcher_manifests=$launches negative_catalog=$negative future_map=PASS"

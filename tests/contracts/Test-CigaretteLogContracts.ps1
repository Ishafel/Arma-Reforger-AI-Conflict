param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $RepositoryRoot 'tools/AICF_Scenarios.ps1')
$plan=Resolve-AICFScenario -Catalog (Get-AICFScenarioCatalog) -Variant Stock -MatchMode CoopUSSR -Difficulty Medium
$argv=@('-gproj','C:\source\addon.gproj','-profile','C:\fresh','-addonsDir','C:\source','-addons',$plan.addonIds,'-aicfTransitionProbe','1','-aicfExpectedReadyVariants','9','-aicfProbeMap','0','-aicfProbeContent','0','-aicfProbeMode','2','-aicfProbeDifficulty','1','-aicfCigaretteProbe','1')
$manifest=@{arguments=$argv}|ConvertTo-Json -Compress
$lines=@('CLI Params: '+($argv -join ' '),"[SCENARIO_LAUNCH] variant=Stock mode=2 difficulty=1 resource=$($plan.scenario) addons=$($plan.addonIds)")
$lines+=@('PREFLIGHT','ACCEPTED','ACTIVE_HEADER','EXACT_GRAPH')|ForEach-Object {"[TRANSITION_CASE] case=$_ pass=1"}
$lines+=$plan.addonIds.Split(',')|ForEach-Object {"[TRANSITION_LOADED] id=$_ vanilla=0"}
$lines+=@('[ROSTER_READY]','[TRANSITION_DONE] header=1 graph=1 roster=1')
for($cycle=0;$cycle -lt 6;$cycle++){
 $lines+=@('PARENT','PROP','COMPONENT','FRAME_MASK','PLAYBACK','DETACH','DELETE')|ForEach-Object {"[CIGARETTE_CASE] case=$_ cycle=$cycle pass=1"}
}
$lines+=@('[CIGARETTE_DONE] checks=42 failed=0 cycles=6','Game destroyed.')
$good=$lines -join "`n"
$base=[IO.Path]::GetFullPath([IO.Path]::GetTempPath())
$work=Join-Path $base ('aicf-cigarette-log-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $work|Out-Null
$count=0
function Check([string]$Name,[string]$Log,[int]$Expected,[string]$Rule,[int]$Native=0){
 [IO.File]::WriteAllText("$work/console.log",$Log)
 [IO.File]::WriteAllText("$work/manifest.json",$manifest)
 [IO.File]::WriteAllText("$work/exit.txt","$Native")
 $output=& powershell.exe -NoProfile -ExecutionPolicy Bypass -File (Join-Path $RepositoryRoot 'tests/log-audits/Test-CigaretteLog.ps1') -LogPath "$work/console.log" -ManifestPath "$work/manifest.json" -NativeExitPath "$work/exit.txt" 2>&1
 if($LASTEXITCODE -ne $Expected -or ($Rule -and ($output -join "`n") -notmatch $Rule)){throw "$Name rejected incorrectly: $output"}
 $script:count++
}
try{
 Check positive $good 0 ''
 foreach($name in @('PARENT','PROP','COMPONENT','FRAME_MASK','PLAYBACK','DETACH','DELETE')){
  Check $name ($good.Replace("case=$name cycle=3 pass=1","case=$name cycle=3 pass=0")) 1 "CASE_$($name)_3"
 }
 Check missing ($good.Replace('[CIGARETTE_CASE] case=PLAYBACK cycle=0 pass=1','')) 1 CASE_COUNT
 Check duplicate ($good.Replace('[CIGARETTE_CASE] case=PLAYBACK cycle=0 pass=1',"[CIGARETTE_CASE] case=PLAYBACK cycle=0 pass=1`n[CIGARETTE_CASE] case=PLAYBACK cycle=0 pass=1")) 1 CASE_COUNT
 Check done ($good.Replace('checks=42 failed=0 cycles=6','checks=42 failed=0 cycles=5')) 1 COMPLETION
 Check thread ($good+"`nAttempt to modify locked entity from a different thread") 1 NATIVE_ASSERT
 Check assertion ($good+"`nAssertion failed") 1 NATIVE_ASSERT
 Check posevelocity ($good+"`nDEFAULT (E): ParticleEffectComponent: Suspicious parent velocity (1095480.62 km/h) for particle effect 'Particles/Enviroment/Cigarette_smoke.ptc'.") 1 PARTICLE_POSE_VELOCITY
 [IO.File]::WriteAllText("$work/smoke.log",'ParticleEffectComponent: Suspicious parent velocity')
 try{Check posevelocitysibling $good 1 PARTICLE_POSE_VELOCITY}finally{Remove-Item -LiteralPath "$work/smoke.log"}
 [IO.File]::WriteAllText("$work/crash.log",'Attempt to modify locked entity from a different thread')
 try{Check crashfile $good 1 NATIVE_ASSERT}finally{Remove-Item -LiteralPath "$work/crash.log"}
 Check stopped ($good.Replace('Game destroyed.','')) 1 STOPPED_LOG
 Check native $good 1 NATIVE_EXIT 1
 Check graph ($good.Replace('[TRANSITION_CASE] case=EXACT_GRAPH pass=1','[TRANSITION_CASE] case=EXACT_GRAPH pass=0')) 1 CASE_EXACT_GRAPH
}
finally{
 $target=[IO.Path]::GetFullPath($work)
 if(!$target.StartsWith($base,[StringComparison]::OrdinalIgnoreCase) -or (Split-Path -Leaf $target) -notlike 'aicf-cigarette-log-*'){throw 'Unsafe cleanup'}
 Remove-Item -LiteralPath $target -Recurse -Force
}
"Cigarette log contracts: PASS; cases=$count"

param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
$fixtureRoot=Join-Path $RepositoryRoot ('.codex-runtime/deferred-log-contract-'+[guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($fixtureRoot)
$auditor=Join-Path $RepositoryRoot 'tests/log-audits/Test-DeferredVehiclePresetLog.ps1'
$server="CLI Params: -gproj Core -profile Profile -addonsDir Addons -addons CoreId -aicfDeferredVehiclePresetProbe 1`n[ROSTER_READY]`naccepted=1 reason=WAITING_FOR_REINFORCEMENTS`n"
foreach($rule in @('BOUND_BEFORE_MUSTER_COMPLETE','NO_VEHICLE_BEFORE_MUSTER','DEFERRED_CREW_REDUCED','POST_MUSTER_POINT_ORDER','PURCHASED_EXACT_DRIVER')){$server+="[DEFERRED_CHECK] rule=$rule passed=1`n"}
for($i=1;$i -le 9;$i++){$server+="[INFANTRY_RECRUIT_JOINED] faction=USSR slot=0 character=Entity$i`n"}
$server+="[DEFERRED_CREW_COMMIT] retained=1 kit_unchanged=1 generation=2`n[VEHICLE_PRESET_PURCHASE] UAZ469_uncovered_CIV_blue.et supplies=50`n[DEFERRED_DONE] checks=5 failures=0`nGame destroyed.`n"
$client="CLI Params: -gproj Core -profile Profile -addonsDir Addons -addons CoreId -aicfDeferredVehiclePresetProbe 1`n"
foreach($rule in @('PARTIAL_ROSTER_FUTURE_POSITIONS','CATALOG_NATIVE_UAZ','SELECT_NATIVE_TOPOLOGY','CONFIRM_BEFORE_RECRUITMENT','BOUND_WHILE_RECRUITING','DEFERRED_CREW_OWNER_READBACK')){$client+="[DEFERRED_CLIENT_CHECK] rule=$rule passed=1`n"}
$client+="[DEFERRED_CLIENT_DONE] checks=6 failures=0`nGame destroyed.`n"
$manifest=@{arguments=@('-gproj','Core','-profile','Profile','-addonsDir','Addons','-addons','CoreId','-aicfDeferredVehiclePresetProbe','1')}|ConvertTo-Json -Compress
foreach($role in @('Server','Client')){
 [void][IO.Directory]::CreateDirectory((Join-Path $fixtureRoot $role))
 "AICF_RUNTIME_MANIFEST_JSON=$manifest`n[SERVER_READY]"|Set-Content (Join-Path $fixtureRoot "$role-launcher.txt")
 '0'|Set-Content (Join-Path $fixtureRoot "$role-exit.txt")
}
$arguments=@('-NoProfile','-ExecutionPolicy','Bypass','-File',$auditor,'-ServerLog',(Join-Path $fixtureRoot 'Server/console.log'),'-ClientLog',(Join-Path $fixtureRoot 'Client/console.log'),'-ServerLauncher',(Join-Path $fixtureRoot 'Server-launcher.txt'),'-ClientLauncher',(Join-Path $fixtureRoot 'Client-launcher.txt'),'-ServerExit',(Join-Path $fixtureRoot 'Server-exit.txt'),'-ClientExit',(Join-Path $fixtureRoot 'Client-exit.txt'))
$shell=(Get-Command powershell.exe).Source
$cases=@(
 @{name='positive';server=$server;client=$client;exit=0},
 @{name='unpaid';server=$server.Replace('supplies=50','supplies=0');client=$client;exit=1},
 @{name='missing_recruit';server=$server.Replace('[INFANTRY_RECRUIT_JOINED] faction=USSR slot=0 character=Entity9','');client=$client;exit=1},
 @{name='wrong_kit';server=$server.Replace('kit_unchanged=1','kit_unchanged=0');client=$client;exit=1},
 @{name='missing_driver';server=$server.Replace('rule=PURCHASED_EXACT_DRIVER passed=1','rule=PURCHASED_EXACT_DRIVER passed=0');client=$client;exit=1},
 @{name='native_oom';server=$server;client=$client+"`nMEMORY (E): Out of memory";exit=1},
 @{name='native_vm';server=$server+"`nSCRIPT (E): Virtual Machine Exception";client=$client;exit=1},
 @{name='early_closed';server=$server;client=$client.Replace('Game destroyed.','');exit=1}
)
try{
 foreach($case in $cases){
  [IO.File]::WriteAllText((Join-Path $fixtureRoot 'Server/console.log'),$case.server)
  [IO.File]::WriteAllText((Join-Path $fixtureRoot 'Client/console.log'),$case.client)
  & $shell @arguments *> (Join-Path $fixtureRoot ($case.name+'.txt'))
  if($LASTEXITCODE -ne $case.exit){throw "Deferred log control $($case.name) actual=$LASTEXITCODE expected=$($case.exit)"}
 }
 'PASS Deferred log contracts: positive and seven unsafe/missing native evidence controls'
}finally{
 # Delete only this exact, locally created fixture root after checking containment.
 $resolved=[IO.Path]::GetFullPath($fixtureRoot)
 $expected=[IO.Path]::GetFullPath((Join-Path $RepositoryRoot '.codex-runtime'))+[IO.Path]::DirectorySeparatorChar
 if(!$resolved.StartsWith($expected,[StringComparison]::OrdinalIgnoreCase)){throw 'Fixture cleanup outside evidence root'}
 Remove-Item -LiteralPath $resolved -Recurse -Force
}

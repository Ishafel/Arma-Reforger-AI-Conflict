param([Parameter(Mandatory)][string]$LogPath,[Parameter(Mandatory)][string]$NativeExitPath,[switch]$RegistrationOnly)
$ErrorActionPreference='Stop'
$text=Get-Content -LiteralPath $LogPath -Raw
if((Get-Content $NativeExitPath -Raw).Trim() -ne '0' -or !$text.Contains('Game destroyed.')){throw 'Native run not cleanly stopped'}
if($text -notmatch '\[BOARDING_REGISTRATION_CHECK\].*registered_before_actions=1'){throw 'No positive actual boarding boundary'}
if($text -match '\[BOARDING_REGISTRATION_CHECK\].*registered_before_actions=0'){throw 'GetIn dispatched without native registration'}
if($text -notmatch '\[BOARDING_REGISTRATION_OCCUPANTS\] exact_production_readback=1'){throw 'No exact production occupant readback'}
$m=[regex]::Match($text,'\[BOARDING_REGISTRATION_DONE\] physical_distance_m=([\d.]+)')
$movement=$m.Success -and [double]::Parse($m.Groups[1].Value,[Globalization.CultureInfo]::InvariantCulture) -ge 10
if(!$movement -and !$RegistrationOnly){throw 'No physical movement after boarding'}
$raw=''
foreach($f in Get-ChildItem (Split-Path $LogPath -Parent) -Filter '*.log'){$raw+=[IO.File]::ReadAllText($f.FullName)+"`n"}
if($raw -match 'SCRIPT\s+\([EF]\)|ENGINE\s+\(F\)|Virtual Machine Exception|Out of memory|vehicle provided is not registered as used inside group!'){throw 'Fatal/script/registration error in complete sibling logs'}
$errors=[regex]::Matches($raw,'\([EF]\)').Count
"PASS native registration/exact boarding/stopped log"
if($movement){'MOVEMENT_GATE=PASS'}else{'MOVEMENT_GATE=NOT_PROVEN registration_only_scope=1'}
if($errors){"RESOURCE_GATE=FAIL raw_sibling_errors=$errors"}else{'RESOURCE_GATE=PASS'}

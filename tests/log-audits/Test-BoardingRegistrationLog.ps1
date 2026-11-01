param([Parameter(Mandatory)][string]$LogPath,[Parameter(Mandatory)][string]$NativeExitPath,[switch]$RegistrationOnly,[ValidateRange(10,1000)][double]$MinimumMovementMeters=10,[switch]$RequireAssignmentScope,[switch]$RequireTacticalRecovery)
$ErrorActionPreference='Stop'
$text=Get-Content -LiteralPath $LogPath -Raw
if((Get-Content $NativeExitPath -Raw).Trim() -ne '0' -or !$text.Contains('Game destroyed.')){throw 'Native run not cleanly stopped'}
if($text -notmatch '\[BOARDING_REGISTRATION_CHECK\].*registered_before_actions=1'){throw 'No positive actual boarding boundary'}
if($text -match '\[BOARDING_REGISTRATION_CHECK\].*registered_before_actions=0'){throw 'GetIn dispatched without native registration'}
if($text -notmatch '\[BOARDING_REGISTRATION_OCCUPANTS\] exact_production_readback=1'){throw 'No exact production occupant readback'}
$m=[regex]::Match($text,'\[BOARDING_REGISTRATION_DONE\] physical_distance_m=([\d.]+)')
$movement=$m.Success -and [double]::Parse($m.Groups[1].Value,[Globalization.CultureInfo]::InvariantCulture) -ge $MinimumMovementMeters
if(!$movement -and !$RegistrationOnly){throw 'No physical movement after boarding'}
$raw=''
foreach($f in Get-ChildItem (Split-Path $LogPath -Parent) -Filter '*.log'){$raw+=[IO.File]::ReadAllText($f.FullName)+"`n"}
if($raw -match 'SCRIPT\s+\([EF]\)|ENGINE\s+\(F\)|Virtual Machine Exception|Out of memory|vehicle provided is not registered as used inside group!'){throw 'Fatal/script/registration error in complete sibling logs'}
if($RequireAssignmentScope -and $raw -match '\[VEHICLE_NATIVE_SUBGROUP_REJECTED\][^\r\n]*reason=ASSIGNMENT_SCOPE'){throw 'ASSIGNMENT_SCOPE rejection remains in full logs'}
if($RequireTacticalRecovery){
 $done=[regex]::Match($text,'\[BOARDING_REGISTRATION_DONE\][^\r\n]*\boperation=(\S+)')
 if(!$done.Success){throw 'Missing recovery operation'}
 $operation=[regex]::Escape($done.Groups[1].Value)
 if($text -notmatch "\[TACTICAL_REGISTRATION_CHECK\] operation=$operation registered=1" -or
    $text -notmatch "\[TACTICAL_REGISTRATION_RESUMED\] same_vehicle=1 operation=$operation(?:\s|$)"){throw 'No tactical registration and resume for the moved operation'}
 $beforeDone=$text.Substring(0,$done.Index)
 $resumes=[regex]::Matches($beforeDone,"\[TACTICAL_REGISTRATION_RESUMED\] same_vehicle=1 operation=$operation(?:\s|$)")
 $afterResume=$beforeDone.Substring($resumes[$resumes.Count-1].Index)
 if($afterResume -notmatch "\[VEHICLE_PROGRESS\][^\r\n]*operation_id=$operation\s[^\r\n]*native_route_progress=OBSERVED"){throw 'No production native route progress between resume and movement proof'}
 $entries=[regex]::Matches($beforeDone,"(?m)^.*\[VEHICLE_STATE_CHANGED\][^\r\n]*operation_id=$operation\s[^\r\n]*previous_state=TRANSIT state=TACTICAL_DEPLOYMENT[^\r\n]*")
 $returns=[regex]::Matches($beforeDone,"(?m)^.*\[VEHICLE_STATE_CHANGED\][^\r\n]*operation_id=$operation\s[^\r\n]*previous_state=TACTICAL_DEPLOYMENT state=TRANSIT[^\r\n]*")
 if(!$entries.Count -or !$returns.Count){throw 'No complete production tactical cycle'}
 $entry=$entries[$entries.Count-1].Value;$return=$returns[$returns.Count-1].Value
 foreach($field in @('entity_id','rpl_id','lease_generation','vehicle_generation','vehicle_lifecycle_id')){
  $pattern='\b'+$field+'=(\S+)'
  $first=[regex]::Match($entry,$pattern);$last=[regex]::Match($return,$pattern)
  if(!$first.Success -or !$last.Success -or $first.Groups[1].Value -ne $last.Groups[1].Value){throw "Retained identity changed: $field"}
 }
 if($raw -match '\[NATIVE_REGISTRATION_LOST\]'){throw 'Missing native record observed in GetIn BT'}
}
$errors=[regex]::Matches($raw,'\([EF]\)').Count
"PASS native registration/exact boarding/stopped log"
if($movement){'MOVEMENT_GATE=PASS'}else{'MOVEMENT_GATE=NOT_PROVEN registration_only_scope=1'}
if($RequireAssignmentScope){'ASSIGNMENT_GATE=PASS'}
if($RequireTacticalRecovery){'TACTICAL_RECOVERY_GATE=PASS'}
if($errors){"RESOURCE_GATE=FAIL raw_sibling_errors=$errors"}else{'RESOURCE_GATE=PASS'}

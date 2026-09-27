param(
 [Parameter(Mandatory=$true)][string]$LogPath,
 [switch]$RequireProbe,
 [switch]$RequireRecruitment,
 [ValidateSet('US','USSR')][string[]]$ExpectedForcedSides=@('US','USSR')
)
$ErrorActionPreference='Stop'
$lines=Get-Content -LiteralPath $LogPath
$raw=$lines -join "`n"
$failures=[Collections.Generic.List[string]]::new()
$orders=@{}; $completed=@{}; $recruited=@{}
if($raw -notmatch 'Game destroyed\.') {$failures.Add('LOG_NOT_STOPPED')}
if($raw -notmatch '\[ROSTER_READY\]') {$failures.Add('ROSTER_NOT_READY')}
if($raw -match 'SCRIPT\s+\([EF]\)|ENGINE\s+\(F\)|Virtual Machine Exception|NULL pointer') {$failures.Add('ENGINE_SCRIPT_ERROR')}
if($RequireProbe -and $raw -notmatch '\[FORCED_SMALL_CONTRACTS\] passed=7 total=7') {$failures.Add('PROBE_CONTRACTS')}
foreach($line in $lines) {
 if($line -match '\[INFANTRY_RECRUIT_JOINED\].*?faction=(US|USSR)\s') {$recruited[$Matches[1]]=$true}
 if($line -notmatch '\[(CONSTRUCTION_[A-Z_]+)\]') {continue}
 $event=$Matches[1]; $f=@{}
 if($event -notin @('CONSTRUCTION_DECISION','CONSTRUCTION_FORCED_SMALL_STARTED','CONSTRUCTION_PAYMENT','CONSTRUCTION_PLACED','CONSTRUCTION_FORCED_SMALL_COMPLETION','CONSTRUCTION_COMPLETED')) {continue}
 foreach($m in [regex]::Matches($line,'(\w+)=([^\s]+)')) {$f[$m.Groups[1].Value]=$m.Groups[2].Value}
 if(!$f.token) {continue}
 if($event -eq 'CONSTRUCTION_DECISION') {$orders[$f.token]=@{Faction=$f.faction;Base=$f.base;Provider=$f.provider;Type=$f.type;Start=([long]$f.t_ms-[long]$f.duration_ms);Forced=$false;Paid=0;Placed=0;Requested=$false;Layout='';Completed=0}}
 if(!$orders.ContainsKey($f.token)) {continue}
 $o=$orders[$f.token]
 if($f.faction -ne $o.Faction -or $f.base -ne $o.Base -or $f.provider -ne $o.Provider -or $f.type -ne $o.Type) {$failures.Add('ORDER_IDENTITY')}
 switch($event) {
  'CONSTRUCTION_FORCED_SMALL_STARTED' {
   if($o.Forced -or $o.Type -ne 'SMALL_BARRACKS' -or ([long]$f.t_ms-$o.Start) -lt 120000 -or $o.Paid -or $o.Placed) {$failures.Add('FORCE_ADMISSION')}
   $o.Forced=$true
  }
  'CONSTRUCTION_PAYMENT' {$o.Paid++; if($o.Paid -ne 1 -or $f.debit_count -ne '1' -or $f.reservation_released -ne '1') {$failures.Add('DUPLICATE_OR_INVALID_PAYMENT')}}
  'CONSTRUCTION_PLACED' {$o.Placed++; $o.Layout=$f.layout; if($o.Placed -ne 1 -or $o.Paid -ne 1) {$failures.Add('PLACEMENT_PAYMENT')}}
  'CONSTRUCTION_FORCED_SMALL_COMPLETION' {
   if(!$o.Forced -or $o.Placed -ne 1 -or $o.Paid -ne 1 -or $o.Layout -ne $f.layout) {$failures.Add('FORCE_COMPLETION_ADMISSION')}
   $o.Requested=$true
  }
  'CONSTRUCTION_COMPLETED' {
   if(!$o.Forced) {break}
   $o.Completed++
   if(!$o.Requested -or $o.Completed -ne 1 -or $f.service_online -ne '1' -or $o.Layout -ne $f.layout) {$failures.Add('FORCE_COMPLETION_POSTCONDITION')}
   $completed[$o.Faction]=$true
  }
 }
}
foreach($side in $ExpectedForcedSides) {
 if(!$completed.ContainsKey($side)) {$failures.Add("NO_FORCED_ONLINE_$side")}
}
foreach($side in @('US','USSR')) {
 if($RequireRecruitment -and !$recruited.ContainsKey($side)) {$failures.Add("NO_RECRUITMENT_$side")}
}
if($failures.Count) { $failures | ForEach-Object {"FAIL $_"}; exit 1 }
"Forced small barracks: PASS (deadline, identity, one payment/layout, online=$($ExpectedForcedSides -join ','))"

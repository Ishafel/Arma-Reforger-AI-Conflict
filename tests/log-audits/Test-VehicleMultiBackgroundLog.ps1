[CmdletBinding()]
param([Parameter(Mandatory)][string]$LogPath,[Parameter(Mandatory)][string]$EvidenceRoot)
$ErrorActionPreference='Stop'
function Fields([string]$line){$f=@{};foreach($v in [regex]::Matches($line,'(?:^|\s)([A-Za-z_][A-Za-z0-9_]*)=(.*?)(?=\s+[A-Za-z_][A-Za-z0-9_]*=|$)')){$f[$v.Groups[1].Value]=$v.Groups[2].Value.Trim()};return $f}
$errors=New-Object 'System.Collections.Generic.List[string]'
function Require([bool]$ok,[string]$reason){if(!$ok -and !$errors.Contains($reason)){$errors.Add($reason)}}
$originals=@{};$actors=@{};$setup=@{};$final=@{};$readbacks=0;$setupReadback=0;$finalReadback=0;$census=0
foreach($line in [IO.File]::ReadLines((Resolve-Path -LiteralPath $LogPath).Path)){
 if($line -notmatch '\[MULTI_BACKGROUND_([A-Z_]+)\]'){continue};$event=$Matches[1];$f=Fields $line
 Require ($f.background_mode -eq 'MINIMUM_BACKGROUND_CAPACITY' -and $f.preparation_only -eq '1' -and $f.original_stress_covered -eq '0' -and $f.normal_campaign -eq '0') 'EXPLICIT_ADDITIONAL_PREPARATION_ONLY'
 if($event -eq 'REJECTED'){Require $false 'SETUP_REJECTED'}
 if($event -eq 'MEMBER'){
  $key=[string]$f.faction+'/'+[string]$f.numeric_slot
  Require ($f.faction -in @('US','USSR') -and $f.numeric_slot -in @('6','7','8','9')) 'ONLY_EIGHT_BACKGROUND_SLOTS'
  Require ($f.same_original_alive_one -eq '1' -and $f.desired -eq '1' -and $f.physical_agents -eq '1' -and $f.alive_agents -eq '1' -and $f.recruiting -eq '0' -and $f.queued_order -eq '0') 'ACTUAL_SINGLE_LIVE_UNRECRUITED_ORIGINAL'
  Require (![string]::IsNullOrWhiteSpace($f.actor) -and $f.actor -notin @('NONE','0','-1') -and ![string]::IsNullOrWhiteSpace($f.actor_rpl) -and $f.actor_rpl -ne 'NONE') 'REAL_ORIGINAL_IDENTITY'
  $identity=[string]$f.group+'|'+$f.actor+'|'+$f.actor_rpl+'|'+$f.generation
  if($f.stage -eq 'SETUP'){Require (!$setup.ContainsKey($key)) 'SETUP_EACH_ONCE';$setup[$key]=1;$originals[$key]=$identity;$actors[$key]=$f.actor}
  else{Require ($originals.ContainsKey($key) -and $originals[$key] -ceq $identity) 'SAME_ORIGINAL_GROUP_ACTOR_GENERATION'}
  if($f.stage -eq 'FINAL'){Require (!$final.ContainsKey($key)) 'FINAL_EACH_ONCE';$final[$key]=1}
 }
 if($event -eq 'READBACK'){
  $readbacks++;Require ($f.background_originals -eq '8' -and $f.missing -eq '0' -and $f.total_managed_slots -eq '20' -and $f.selected_groups -eq '12' -and $f.selected_crew -eq '5') 'UNCHANGED_TWENTY_SLOTS_AND_SELECTED_TWELVE_BY_FIVE'
  Require (![string]::IsNullOrWhiteSpace($f.background_originals_id)) 'EXPLICIT_EIGHT_ORIGINALS_LEDGER'
  $ids=@{};foreach($id in [regex]::Matches([string]$f.background_originals_id,'(US|USSR)/([6-9]):([^;]+);')){$key=$id.Groups[1].Value+'/'+$id.Groups[2].Value;Require (!$ids.ContainsKey($key)) 'UNIQUE_ORIGINALS_LEDGER';$ids[$key]=$id.Groups[3].Value}
  Require ($ids.Count -eq 8) 'EIGHT_LEDGER_IDENTITIES';foreach($key in $actors.Keys){Require ($ids.ContainsKey($key) -and $ids[$key] -ceq $actors[$key]) 'LEDGER_MATCHES_ACTUAL_ORIGINALS'}
  if($f.stage -eq 'SETUP'){$setupReadback++};if($f.stage -eq 'FINAL'){$finalReadback++}
 }
 if($event -eq 'CENSUS'){
  $census++;Require ($f.readonly -eq '1' -and $f.actor_rows_emitted -eq '0' -and $f.maximum_agents_examined -eq '4096' -and $f.maximum_groups_examined -eq '4096') 'BOUNDED_AGGREGATE_CENSUS_ONLY'
  if($f.snapshot_complete -eq '1'){
   Require ([int]$f.agents_returned -le 4096 -and [int]$f.agents_examined -eq [int]$f.agents_returned -and $f.missing -eq '0' -and $f.groups_complete -eq '1' -and $f.native_active_before -eq $f.native_active_after -and $f.native_intended_before -eq $f.native_intended_after) 'NO_FALSE_COMPLETE_CENSUS'
  }
  if($f.native_count_reconciled -eq '1'){Require ($f.snapshot_complete -eq '1' -and $f.counted_activated -eq $f.native_active_after) 'NO_UNPROVEN_NATIVE_COUNT_RECONCILIATION'}
  Require (([int]$f.active_characters+[int]$f.active_vehicles+[int]$f.active_group_controllers+[int]$f.active_null+[int]$f.active_other) -eq [int]$f.counted_activated) 'COUNTED_TYPE_SUM_RECONCILED'
 }
}
Require ($setup.Count -eq 8 -and $final.Count -eq 8 -and $setupReadback -eq 1 -and $finalReadback -eq 1 -and $readbacks -ge 2) 'SETUP_AND_FINAL_ALL_EIGHT_ORIGINALS'
Require ($census -ge 2) 'SETUP_AND_COHORT_CENSUS_PRESENT'
$verdict='PASS';if($errors.Count){$verdict='FAIL'}
[pscustomobject]@{verdict=$verdict;background_mode='MINIMUM_BACKGROUND_CAPACITY';original_production_background_covered=$false;setup_originals=$setup.Count;final_originals=$final.Count;readbacks=$readbacks;census_snapshots=$census;failures=@($errors)}|ConvertTo-Json -Depth 4|Set-Content (Join-Path $EvidenceRoot 'background-audit-summary.json')
"MULTI_BACKGROUND_AUDIT=$verdict original_production_background_covered=0 setup=$($setup.Count) final=$($final.Count) reasons=$($errors -join ',')"
if($errors.Count){exit 1};exit 0

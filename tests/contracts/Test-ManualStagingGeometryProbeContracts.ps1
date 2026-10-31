param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
$source=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'tests/fixtures/AICF_ManualStagingGeometryProbe.c'))
function Rules($s){
 foreach($guard in @('Replication.IsServer()','!m_bRosterReady','aicfManualStagingGeometryProbe','flag != "1"',
  'now - m_iMSGStarted >= 15000','m_bMSGDone = true;','GetGame().RequestClose();','m_iMSGNext = now + 3000;',
  'AICF_InfantrySpawnPlacement.IsUsable(group, query, destination)',
  'shift >= 2 && shift <= 8 && vertical <= 1 && projection <= 0.25 && pad >= 10 && pad <= 90 && progress > 1',
  '(phase == 0 && !old) || (phase == 1 && old)','historical_dynamic_occupancy_reconstructed=0','production_mutations=0')){
  if(!$s.Contains($guard)){'READONLY_BOUNDED_PRESENT_WORLD';break}
 }
 if($s -match '\b(?:Teleport|SetOrigin|SetWorldTransform|SpawnEntityPrefabEx|SetHealth|Kill|KillGM|DeleteEntity|SetDisableMovementControls|SetLimitOfActiveAIs)\s*\('){'NO_GAMEPLAY_MUTATION'}
}
$bad=@(Rules $source);if($bad.Count){throw "Geometry fixture: $bad"}
$mutants=@('Replication.IsServer()','!m_bRosterReady','flag != "1"','now - m_iMSGStarted >= 15000',
 'm_bMSGDone = true;','m_iMSGNext = now + 3000;','AICF_InfantrySpawnPlacement.IsUsable(group, query, destination)',
 'shift >= 2 && shift <= 8 && vertical <= 1 && projection <= 0.25 && pad >= 10 && pad <= 90 && progress > 1',
 '(phase == 0 && !old) || (phase == 1 && old)','historical_dynamic_occupancy_reconstructed=0')
foreach($m in $mutants){if(!@(Rules $source.Replace($m,'false')).Count){throw "Escaped geometry fixture mutant $m"}}
foreach($mutation in @('actor.Teleport(t);','world.SpawnEntityPrefabEx(r,p);','actor.SetHealth(1);','world.SetLimitOfActiveAIs(999);')){if(!@(Rules ($source+"`n"+$mutation)).Count){throw 'Mutable geometry probe escaped'}}
$rings=@([regex]::Match($source,'array<float> rings = \{([^}]+)\}').Groups[1].Value.Split(',')|ForEach-Object{[double]$_.Trim()})
$angles=@([regex]::Match($source,'array<float> angles = \{([^}]+)\}').Groups[1].Value.Split(',')|ForEach-Object{[double]$_.Trim()})
if(($rings.Count -ne 7 -or $rings[0] -ne 3 -or $rings[1] -ne 5 -or $rings[2] -ne 7 -or $rings[3] -ne 2 -or $rings[4] -ne 4 -or $rings[5] -ne 6 -or $rings[6] -ne 7.9) -or ($angles[0..2] -join ',') -ne '0,45,-45'){throw 'Old9 prefix or bounded rings changed'}
$points=@(foreach($phase in 0..1){for($r=0;$r -lt $rings.Count;$r++){for($a=0;$a -lt $angles.Count;$a++){$old=$r -lt 3 -and $a -lt 3;if(($phase -eq 0 -and !$old) -or ($phase -eq 1 -and $old)){continue};'{0}:{1}' -f $rings[$r],$angles[$a]}}})
if($points.Count -ne 91 -or @($points|Sort-Object -Unique).Count -ne 91 -or @($rings|Where-Object{$_ -lt 2 -or $_ -ge 8}).Count){throw 'Duplicate/unbounded geometry candidate grid'}
"PASS Manual staging geometry fixture: $($mutants.Count) source negatives +4 mutation vetoes;91 unique bounded candidates/old9prefix; present-world read-only only"

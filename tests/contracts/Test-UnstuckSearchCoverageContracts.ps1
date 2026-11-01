param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$source=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Vehicles/AICF_VehicleTransitFlow.c'))
function Body([string]$name){Get-AICFMethodBody ([pscustomobject]@{Source=$source;Code=(ConvertTo-AICFCodeText $source)}) $name}
$body=Body 'BuildUnstuckSearchDirections'
$count=[int]([regex]::Match($source,'UNSTUCK_DIRECTION_COUNT = (\d+)').Groups[1].Value)
$translated=$body.Replace('array<vector> directions = {};','var directions = new List<Vec>();').Replace('vector rightDirection = Vector(','Vec rightDirection = new Vec(').Replace('directions.Insert(','directions.Add(').Replace('float angle =','double angle =').Replace('Math.PI2','(Math.PI * 2)').Replace('for (int bearing;','for (int bearing = 0;')
$shim=@"
using System;
using System.Collections.Generic;
public struct Vec {
 public double X,Y,Z;
 public Vec(double x,double y,double z){X=x;Y=y;Z=z;}
 public double this[int i]{get{return i==0?X:i==1?Y:Z;}}
 public static Vec operator+(Vec a,Vec b){return new Vec(a.X+b.X,a.Y+b.Y,a.Z+b.Z);}
 public static Vec operator-(Vec a){return new Vec(-a.X,-a.Y,-a.Z);}
 public static Vec operator-(Vec a,Vec b){return a+-b;}
 public static Vec operator*(Vec a,double b){return new Vec(a.X*b,a.Y*b,a.Z*b);}
 public Vec Normalized(){return this*(1/Math.Sqrt(X*X+Y*Y+Z*Z));}
}
public static class Coverage {
 const int UNSTUCK_DIRECTION_COUNT=$count;
 public static List<Vec> Build(Vec targetDirection) $translated
}
"@
Add-Type -TypeDefinition $shim
$checks=0
function Check([bool]$ok,[string]$rule){if(!$ok){throw "Unstuck search coverage: $rule"};$script:checks++}
function Near($a,$b){[Math]::Abs($a.X-$b.X)+[Math]::Abs($a.Y-$b.Y)+[Math]::Abs($a.Z-$b.Z) -lt 0.000001}
$directions=[Coverage]::Build([Vec]::new(0,0,1))
Check ($directions.Count -eq 32) 'EXACT_BOUNDED_32'
$legacy=@([Vec]::new(0,0,1),[Vec]::new(0,0,-1),[Vec]::new(1,0,0),[Vec]::new(-1,0,0),[Vec]::new(1,0,1).Normalized(),[Vec]::new(-1,0,1).Normalized(),[Vec]::new(1,0,-1).Normalized(),[Vec]::new(-1,0,-1).Normalized())
for($i=0;$i -lt 8;$i++){Check (Near $directions[$i] $legacy[$i]) "LEGACY_PREFIX_$i"}
foreach($direction in $directions){Check ([Math]::Abs($direction.X*$direction.X+$direction.Z*$direction.Z-1) -lt 0.000001 -and $direction.Y -eq 0) 'NORMALIZED_XZ'}
for($i=0;$i -lt $directions.Count;$i++){for($j=$i+1;$j -lt $directions.Count;$j++){if(Near $directions[$i] $directions[$j]){throw "Duplicate bearing $i/$j"}}}
$checks++
$again=[Coverage]::Build([Vec]::new(0,0,1))
Check (@(0..31|Where-Object{!(Near $directions[$_] $again[$_])}).Count -eq 0) 'DETERMINISTIC_ORDER'
$rotated=[Coverage]::Build([Vec]::new(1,0,0))
Check (@(0..31|Where-Object{!(Near $rotated[$_] ([Vec]::new($directions[$_].Z,0,-$directions[$_].X)))}).Count -eq 0) 'ROTATION_EQUIVARIANT'
# Native failing-pose evidence: the old eight miss a clear patch at bearing9/32,
# while both old terrain hits are occupied. The predicate is external geometry.
$patch=[Vec]::new([Math]::Sin(9*[Math]::PI/16),0,[Math]::Cos(9*[Math]::PI/16))
$oldHits=@($legacy|Where-Object{Near $_ $patch})
$newHits=@($directions|Where-Object{Near $_ $patch})
Check ($oldHits.Count -eq 0 -and $newHits.Count -eq 1) 'NATIVE_MISSED_PATCH_COVERED'
$offsetNames=[regex]::Match((Body 'TryRelocateVehicleForUnstuck'),'searchOffsets = \{(?<items>[^}]+)\}').Groups['items'].Value.Split(',')|ForEach-Object{$_.Trim()}
$offsets=@($offsetNames|ForEach-Object{[double]([regex]::Match($source,([regex]::Escape($_)+' = ([0-9.]+)')).Groups[1].Value)})
Check ($offsets.Count -eq 4 -and $offsets[0] -eq 8 -and $offsets[1] -eq 13 -and $offsets[2] -eq 6 -and $offsets[3] -eq 10.5) 'OLD_RINGS_FIRST_NEW_BOUNDED_RINGS'
Check (($offsets|Measure-Object -Minimum).Minimum -ge 4 -and ($offsets|Measure-Object -Maximum).Maximum -le 15 -and $directions.Count*$offsets.Count -eq 128) 'BOUNDED_CANDIDATE_SET'
$sample=Body 'TryRelocateVehicleForUnstuck'
foreach($required in @('BuildUnstuckSearchDirections(targetDirection)','displacementMeters < UNSTUCK_MIN_DISPLACEMENT_METERS','displacementMeters > UNSTUCK_MAX_DISPLACEMENT_METERS','IsUnstuckCandidateHazardClear(','m_Watchdog.CanSafelyRelocateVehicle(','m_Watchdog.CanApplyHiddenRecovery(','vehicle.SetWorldTransform(relocatedTransform)')){Check ($sample.Contains($required)) "ACTUAL_SAMPLER_$required"}
Check ($source.Contains('UNSTUCK_MIN_DISPLACEMENT_METERS = 4.0') -and $source.Contains('UNSTUCK_MAX_DISPLACEMENT_METERS = 15.0') -and $source.Contains('UNSTUCK_HAZARD_CLEARANCE_METERS = 6.0') -and $source.Contains('HARD_MAX_MOBILITY_RECOVERIES = 2')) 'UNCHANGED_LIMITS'
Write-Output "PASS: Unstuck search coverage ($checks executable/source checks, actual extracted direction builder)"

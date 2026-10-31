param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$path=Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Vehicles/AICF_PresetEntrySeparation.c'
$source=[IO.File]::ReadAllText($path)
$flow=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Vehicles/AICF_VehicleBoardingFlow.c'))
function Body([string]$text,[string]$name){Get-AICFMethodBody ([pscustomobject]@{Source=$text;Code=(ConvertTo-AICFCodeText $text)}) $name}
function Rules([string]$text){
 $capture=Body $text 'Capture';$pair=Body $text 'CanPair';$geometry=Body $text 'AreIndependent'
 if(!$capture -or !$pair -or !$geometry){'MISSING_METHOD';return}
 foreach($guard in @('!Replication.IsServer()','!trip.IsVehiclePresetCurrent()','!token.Matches(trip, lease, group)',
  '!token.IsPhysicalMutationOwnerSafe()','!token.IsOwnerValid()','seat.GetVehicle() != vehicle',
  '!AICF_VehicleBoardingMutationFence.IsAuthoritativeAIEntity(member)',
  'seat.GetType() == ECompartmentType.TURRET','token.GetCompartmentType() == EAICompartmentType.Turret',
  'seat.GetOccupant() && seat.GetOccupant() != member','!seat.IsCompartmentAccessible()',
  'seat.IsGetInLockedFor(member)','seat.IsReserved() && !seat.IsReservedBy(member)',
  'access.IsGettingOut() || access.IsSwitchingSeatsAnim()','linkedVehicle && linkedVehicle != vehicle')){
  if(!$capture.Contains($guard)){'EXACT_AI_ROLE_SEAT_AND_RESERVATION';break}
 }
 foreach($guard in @('owner != vehicle && owner.GetRootParent() != vehicle','AICF_VehiclePreset.RelativeOwnerIndex(seat)',
  'ordinal < 0','doors.IsEmpty() || doors.Count() > 16','!seat.IsAvailableFromDoor(door)',
  'manager.GetDoorInfo(door)','info.GetAIEntryPointInfo()','info.GetEntryPointInfo()',
  '!entry || !entry.GetOwner()','pointOwner != vehicle && pointOwner.GetRootParent() != vehicle',
  'entry.GetWorldTransform(transform)','string.Format("%1:%2:%3", ordinal, seat.GetCompartmentMgrID(), door)',
  'result.m_aKeys.Contains(key)','result.m_aKeys.Insert(key)','result.m_aPoints.Insert(transform[3])')){
  if(!$capture.Contains($guard)){'COMPLETE_ROOT_RELATIVE_DOOR_METADATA';break}
 }
 if(([regex]::Matches($capture,'!token.Matches\(trip, lease, group\)')).Count -ne 2 -or
  ([regex]::Matches($capture,'!token.IsPhysicalMutationOwnerSafe\(\)')).Count -ne 2 -or
  $capture -notmatch 'seat.GetManager\(\) != manager \|\| manager.GetOwner\(\) != owner'){'REPEATED_EXACT_CAPTURE_IDENTITY'}
 if(([regex]::Matches($capture,'access.IsGettingIn\(\) && access.GetCompartment\(\) != seat')).Count -ne 2 -or
  $capture -notmatch 'character.GetCompartmentAccessComponent\(\) != access'){'ACTUAL_TRANSITION_EXACT_SEAT_OR_SERIAL'}
 if($pair -notmatch '!candidate \|\| !active \|\| candidate == active' -or
  $pair -notmatch 'Capture\(trip, lease, group, candidate\)' -or $pair -notmatch 'Capture\(trip, lease, group, active\)' -or
  $pair -notmatch '!AreIndependent\(first, second\)' -or
  $pair -notmatch '!candidate.Matches\(trip, lease, group\) \|\| !active.Matches\(trip, lease, group\)' -or
  $pair -notmatch '!candidate.IsPhysicalMutationOwnerSafe\(\) \|\| !active.IsPhysicalMutationOwnerSafe\(\)' -or
  $pair -notmatch 'vector.Distance\(vehicle.GetOrigin\(\), origin\) > 0.1' -or
  $pair -notmatch 'vector.Distance\(vehicle.GetYawPitchRoll\(\), angles\) > 0.1'){'REPEATED_PAIR_AND_POSE_FENCE'}
 if($geometry -notmatch '!first \|\| !second \|\| first.m_aKeys.IsEmpty\(\) \|\| second.m_aKeys.IsEmpty\(\)' -or
  $geometry -notmatch 'first.m_aKeys.Count\(\) != first.m_aPoints.Count\(\) \|\| second.m_aKeys.Count\(\) != second.m_aPoints.Count\(\)' -or
  $geometry -notmatch '(?s)foreach \(int left, string key : first.m_aKeys\).*?second.m_aKeys.Contains\(key\).*?foreach \(vector other : second.m_aPoints\).*?vector.DistanceXZ\(first.m_aPoints\[left\], other\) < 2.5'){'ALL_CROSS_DOORS_AND_POINTS'}
 $code=(ConvertTo-AICFCodeText $text) -replace 'result.m_a(?:Keys|Points).Insert\([^;]+;',''
 if($code -match '\.(?:Set\w*|AddAction|Fail|Complete|Cancel\w*|Interrupt\w*|GetInVehicle|GetOutVehicle|Begin\w*|Observe\w*|Record\w*|Insert|Remove)\(|CallLater|Teleport|SetOrigin|FindPath|RequestFollowPath|m_[ifb]\w*\s*[+]?='){'READONLY_PAIR_PERMISSION'}
}
$failures=@(Rules $source)
if($failures.Count){throw "Preset parallel entries: $failures"}
$controls=1
foreach($mutation in @(
 @('!Replication.IsServer()','false','EXACT_AI_ROLE_SEAT_AND_RESERVATION'),
 @('!token.IsOwnerValid()','false','EXACT_AI_ROLE_SEAT_AND_RESERVATION'),
 @('!AICF_VehicleBoardingMutationFence.IsAuthoritativeAIEntity(member)','false','EXACT_AI_ROLE_SEAT_AND_RESERVATION'),
 @('seat.GetType() == ECompartmentType.TURRET','false','EXACT_AI_ROLE_SEAT_AND_RESERVATION'),
 @('seat.IsReserved() && !seat.IsReservedBy(member)','false','EXACT_AI_ROLE_SEAT_AND_RESERVATION'),
 @('access.IsGettingOut() || access.IsSwitchingSeatsAnim()','false','EXACT_AI_ROLE_SEAT_AND_RESERVATION'),
 @('owner != vehicle && owner.GetRootParent() != vehicle','false','COMPLETE_ROOT_RELATIVE_DOOR_METADATA'),
 @('doors.IsEmpty() || doors.Count() > 16','false','COMPLETE_ROOT_RELATIVE_DOOR_METADATA'),
 @('!seat.IsAvailableFromDoor(door)','false','COMPLETE_ROOT_RELATIVE_DOOR_METADATA'),
 @('!entry || !entry.GetOwner()','false','COMPLETE_ROOT_RELATIVE_DOOR_METADATA'),
 @('seat.GetCompartmentMgrID(), door','0, door','COMPLETE_ROOT_RELATIVE_DOOR_METADATA'),
 @('seat.GetManager() != manager || manager.GetOwner() != owner','false','REPEATED_EXACT_CAPTURE_IDENTITY'),
 @('access.IsGettingIn() && access.GetCompartment() != seat','false','ACTUAL_TRANSITION_EXACT_SEAT_OR_SERIAL'),
 @('!active.Matches(trip, lease, group)','false','REPEATED_PAIR_AND_POSE_FENCE'),
 @('vector.Distance(vehicle.GetOrigin(), origin) > 0.1','false','REPEATED_PAIR_AND_POSE_FENCE'),
 @('second.m_aKeys.Contains(key)','false','ALL_CROSS_DOORS_AND_POINTS'),
 @('foreach (vector other : second.m_aPoints)','foreach (vector other : {})','ALL_CROSS_DOORS_AND_POINTS'),
 @('vector.DistanceXZ(first.m_aPoints[left], other) < 2.5','vector.DistanceXZ(first.m_aPoints[left], other) < 0.5','ALL_CROSS_DOORS_AND_POINTS')
)){
 if(!$source.Contains($mutation[0])){throw "Mutation target absent: $($mutation[2])"}
 if($mutation[2] -notin @(Rules ($source.Replace($mutation[0],$mutation[1])))){throw "Mutation survived: $($mutation[2])"}
 $controls++
}
$capture=Body $source 'Capture'
foreach($sideEffect in @('seat.SetReserved(member);','token.CancelOwnerSafe();','vehicle.SetOrigin("0 0 0");')){
 if('READONLY_PAIR_PERMISSION' -notin @(Rules ($source.Replace($capture,$capture.Insert(1,$sideEffect))))){throw "Side effect survived: $sideEffect"}
 $controls++
}
# Выполняем геометрический predicate из production body. Адаптер меняет лишь
# Enforce foreach/array/null syntax; все guards, loop nesting и threshold исходные.
$body=Body $source 'AreIndependent'
$body=$body.Replace('!first || !second','first == null || second == null')
$body=$body.Replace('foreach (int left, string key : first.m_aKeys)' + "`r`n`t`t{", 'for (int left=0; left<first.m_aKeys.Count(); left++) { string key=first.m_aKeys[left];')
$body=[regex]::Replace($body,'foreach \(int left, string key : first.m_aKeys\)\s*\{','for (int left=0; left<first.m_aKeys.Count(); left++) { string key=first.m_aKeys[left];')
$body=$body.Replace('foreach (vector other : second.m_aPoints)','foreach (EntryVector other in second.m_aPoints)').Replace('vector.DistanceXZ','EntryVector.DistanceXZ')
$adapter=@'
using System;
using System.Collections.Generic;
public class EntryArray<T> : List<T> { public bool IsEmpty(){return base.Count==0;} public new int Count(){return base.Count;} }
public struct EntryVector { public double x,z; public EntryVector(double px,double pz){x=px;z=pz;} public static double DistanceXZ(EntryVector a,EntryVector b){return Math.Sqrt((a.x-b.x)*(a.x-b.x)+(a.z-b.z)*(a.z-b.z));} }
public class AICF_PresetEntryDoorSet { public EntryArray<string> m_aKeys=new EntryArray<string>(); public EntryArray<EntryVector> m_aPoints=new EntryArray<EntryVector>(); }
public static class AICF_ParallelEntryGeometry { public static bool AreIndependent(AICF_PresetEntryDoorSet first,AICF_PresetEntryDoorSet second) BODY public static bool MayQueueParallel(int busyCount){return ADMISSION;} }
'@
$admission=[regex]::Match((Body $flow 'TickPreset'),'if \((busyTokens.Count\(\) == 1)\)').Groups[1].Value
if(!$admission){throw 'Bounded parallel admission missing'}
$admission=$admission.Replace('busyTokens.Count()','busyCount')
if(!('AICF_ParallelEntryGeometry' -as [type])){Add-Type -TypeDefinition $adapter.Replace('BODY',$body).Replace('ADMISSION',$admission)}
function DoorSet($keys,$points){
 $set=New-Object AICF_PresetEntryDoorSet
 foreach($key in $keys){$set.m_aKeys.Add($key)}
 $coordinates=@($points | ForEach-Object { $_ })
 if(($coordinates.Count % 2) -ne 0){throw 'Invalid geometry case coordinates'}
 for($coordinate=0;$coordinate -lt $coordinates.Count;$coordinate+=2){$set.m_aPoints.Add([EntryVector]::new($coordinates[$coordinate],$coordinates[$coordinate+1]))}
 return $set
}
$left=DoorSet @('0:11:0') @(@(0,0))
foreach($case in @(
 @{Name='disjoint opposite doors';Right=(DoorSet @('0:11:1') @(@(3,0)));Expected=$true},
 @{Name='exact 2.5m boundary';Right=(DoorSet @('0:11:1') @(@(2.5,0)));Expected=$true},
 @{Name='shared door identity';Right=(DoorSet @('0:11:0') @(@(3,0)));Expected=$false},
 @{Name='different identity overlapping entry';Right=(DoorSet @('1:12:0') @(@(0,0)));Expected=$false},
 @{Name='one unsafe among multiple possible doors';Right=(DoorSet @('0:11:1','0:11:2') @(@(4,0),@(2.49,0)));Expected=$false},
 @{Name='unknown metadata';Right=$null;Expected=$false},
 @{Name='empty door set';Right=(DoorSet @() @());Expected=$false},
 @{Name='missing one door point';Right=(DoorSet @('0:11:1','0:11:2') @(@(4,0)));Expected=$false}
)){
 if([AICF_ParallelEntryGeometry]::AreIndependent($left,$case.Right) -ne $case.Expected){throw "Native door geometry case failed: $($case.Name)"}
 $controls++
}
$multiple=DoorSet @('0:11:0','0:11:3') @(@(0,0),@(6,0))
if([AICF_ParallelEntryGeometry]::AreIndependent($multiple,(DoorSet @('0:11:1') @(@(6,0))))){throw 'Only first candidate point was considered'}
$controls++
# Реальные M2 US5 Pilot/front-passenger AI entry points из stopped native log.
# Проверка геометрии доказывает применимость пары, но не заменяет native boarding.
if(![AICF_ParallelEntryGeometry]::AreIndependent((DoorSet @('0:M2MAIN:0') @(@(1591.96,918.694))),(DoorSet @('0:M2MAIN:1') @(@(1595.92,918.285))))){throw 'M2 opposite-door geometry was rejected'}
$controls++
foreach($busyCount in @(0,1,2,3,8)){
 if([AICF_ParallelEntryGeometry]::MayQueueParallel($busyCount) -ne ($busyCount -eq 1)){throw "Third-entry/empty-pair admission changed: busy=$busyCount"}
 $controls++
}
"PASS Vehicle preset parallel entries: complete exact root-relative live-door sets, all cross entry points >=2.5m, unknown/shared/turret/human/stale rejected, read-only repeated pair proof; controls=$controls"

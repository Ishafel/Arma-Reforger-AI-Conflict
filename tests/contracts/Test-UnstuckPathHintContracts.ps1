param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$hint=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/State/Vehicles/AICF_UnstuckPathHint.c'))
$flow=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Vehicles/AICF_VehicleTransitFlow.c'))
$progress=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Vehicles/AICF_VehicleNativeRouteProgress.c'))
function Body($s,$n){Get-AICFMethodBody ([pscustomobject]@{Source=$s;Code=(ConvertTo-AICFCodeText $s)}) $n}
function Rules($h,$f){
 $capture=Body $h 'Capture';$current=Body $h 'IsCurrent';$read=Body $h 'ReadInitial';$move=Body $h 'MoveCurrent';$health=Body $h 'HealthyActiveStationary';$sample=Body $f 'TryRelocateVehicleForUnstuck'
 foreach($g in @('ValidatePath(hint.m_aPoints, hint.m_vEndpoint)','!LocalFragment(hint.m_aPoints, origin)','ProjectUnique(hint.m_aPoints, origin)','if (!hint.IsCurrent()) return null;','hint_only=1 path_progress=0','MAXIMUM_LOCAL_POINTS = 128;','MAXIMUM_AGE_MS = 3000;')){if(!$h.Contains($g)){'BOUNDED_HINT_NOT_ROUTE_CREDIT';break}}
 foreach($g in @('AICF_VehicleNativeSubgroup.Read(','FindManagedInfantrySlot(m_Group) != m_Trip.GetVehiclePresetSlot()','vehicle.GetOrigin() != m_vOrigin','IsCurrentVehicleHandler(m_Trip, m_Utility, m_iHandler)','state.IsRouteWaypointBound()','m_Trip.IsDeadlineReached(')){if(!$h.Contains($g)){'EXACT_OWNER_HANDLER_ORIGIN_DEADLINE';break}}
 foreach($g in @('m_Utility.GetExecutedAction() != m_Move','m_Move.GetActionState() != EAIActionState.RUNNING','queue.Contains(m_Waypoint)')){if(!$move.Contains($g)){'CURRENT_EXECUTED_OWNED_NATIVE_MOVE';break}}
 foreach($g in @('m_CarAgent.IsAIActivated()','m_CarAgent.CountTowardsAIWorldLimit()','m_CarAgent.GetLOD() != 0','m_Pilot.IsAIActivated()','physics.GetVelocity().Length() > 1.0','GetDamageState() != EDamageState.UNDAMAGED','GetHealthScaled() == 1.0','GetMovementDamage() == 0','VehicleIsOnFire(vehicle)')){if(!$health.Contains($g)){'HEALTHY_ACTIVE_SLOW_ONLY';break}}
 foreach($g in @('now - m_iCapturedAtMs > MAXIMUM_AGE_MS','Scope() != m_sScope','m_Waypoint.GetID() != m_WaypointId','m_WaypointRpl.Id().ToString() != m_sWaypointRplId','state.GetRouteGeneration() != m_iRouteGeneration','state.GetRouteEndpoint() != m_vEndpoint','m_Assignment.GetTargetPosition() != m_vGoal','m_Pilot.GetMovementComponent() != m_Car','m_Car.GetAIAgent() != m_CarAgent','m_Car.GetCurrentPath(repeated)','SamePath(m_aPoints, repeated)')){if(!$current.Contains($g)){'FRESH_REPEAT_IDENTITY_AND_FULL_RAW_PATH';break}}
 if((ConvertTo-AICFCodeText $h)-match '\b(?:SetOrigin|SetWorldTransform|SetVelocity|SetHealth|ActivateAI|DeactivateAI|SetLOD|CallLater|CreditNativeRouteProgress|BeginMobilityRecovery|CommitTransition)\s*\('){'NO_EXTERNAL_MUTATION_OR_PROGRESS_WRITES'}
 foreach($g in @('pathHint = AICF_UnstuckPathHint.Capture(trip, originalPosition)','if (pathHint && !pathHint.Prefers(candidate))','if (!hasFirstSafe)','selectedCandidate = candidate','bool currentHint = !pathHint || pathHint.IsCurrent()','vehicle.GetOrigin() == originalPosition','STALE_NO_MUTATION','FINAL_RECHECK_REJECTED','IsUnstuckCandidateHazardClear(selectedCandidate','relocatedTransform[3] = selectedCandidate')){if(!$sample.Contains($g)){'PREFERENCE_WITH_STALE_FAILCLOSED_AND_FINAL_PHYSICAL_FENCES';break}}
 if($sample.IndexOf('selectedCandidate = candidate')-gt$sample.IndexOf('vehicle.SetWorldTransform(')-or$sample.IndexOf('FINAL_RECHECK_REJECTED')-gt$sample.IndexOf('vehicle.SetWorldTransform(')){'SELECTION_BEFORE_ONLY_PHYSICAL_MUTATION'}
 if($f-notmatch'UNSTUCK_DIRECTION_COUNT = 32;'-or$f-notmatch'UNSTUCK_MAX_DISPLACEMENT_METERS = 15.0;'-or$f-notmatch'HARD_MAX_MOBILITY_RECOVERIES = 2;'){ 'OLD_BOUNDS_AND_TWO_ATTEMPTS' }
}
$fail=@(Rules $hint $flow);if($fail.Count){throw "Hint source: $fail"};$checks=1
foreach($guard in @('MAXIMUM_LOCAL_POINTS = 128;','MAXIMUM_AGE_MS = 3000;','SamePath(m_aPoints, repeated)','m_CarAgent.IsAIActivated()','m_CarAgent.GetLOD() != 0','Scope() != m_sScope','m_Waypoint.GetID() != m_WaypointId','m_Utility.GetExecutedAction() != m_Move','GetHealthScaled() == 1.0','vehicle.GetOrigin() != m_vOrigin','hint_only=1 path_progress=0')){if(!@(Rules $hint.Replace($guard,'false') $flow).Count){throw "Hint mutant escaped $guard"};$checks++}
foreach($guard in @('if (pathHint && !pathHint.Prefers(candidate))','bool currentHint = !pathHint || pathHint.IsCurrent()','IsUnstuckCandidateHazardClear(selectedCandidate','vehicle.GetOrigin() == originalPosition','FINAL_RECHECK_REJECTED')){if(!@(Rules $hint $flow.Replace($guard,'false')).Count){throw "Sampler mutant escaped $guard"};$checks++}
function Translate($b){
 $b=$b.Replace('array<vector>','List<Vec>').Replace('vector.','Vec.').Replace('vector ','Vec ').Replace('float ','double ').Replace('Math.AbsFloat','Math.Abs').Replace('.Count()','.Count')
 $b=$b.Replace('foreach (Vec point : points)','foreach (Vec point in points)').Replace('!points','points == null').Replace('!projected','projected == null').Replace('return projected &&','return projected != null &&').Replace('projected.m_sReason.IsEmpty()','String.IsNullOrEmpty(projected.m_sReason)').Replace('for (int index;','for (int index=0;').Replace('for (int other;','for (int other=0;')
 return $b.Replace('double prefix;','double prefix=0;').Replace('prefix, arc, gap, height)','prefix, out arc, out gap, out height)').Replace('prefix, otherArc, otherGap, otherHeight)','prefix, out otherArc, out otherGap, out otherHeight)')
}
$cs=@'
#pragma warning disable 1718
using System;using System.Collections.Generic;
public struct Vec {
 public double X,Y,Z;public Vec(double x,double y,double z){X=x;Y=y;Z=z;}public double this[int i]{get{return i==0?X:i==1?Y:Z;}}
 public static Vec operator+(Vec a,Vec b){return new Vec(a.X+b.X,a.Y+b.Y,a.Z+b.Z);}public static Vec operator-(Vec a,Vec b){return new Vec(a.X-b.X,a.Y-b.Y,a.Z-b.Z);}public static Vec operator*(Vec a,double k){return new Vec(a.X*k,a.Y*k,a.Z*k);}
 public static double DistanceXZ(Vec a,Vec b){var d=a-b;return System.Math.Sqrt(d.X*d.X+d.Z*d.Z);}public static double Distance(Vec a,Vec b){var d=a-b;return System.Math.Sqrt(d.X*d.X+d.Y*d.Y+d.Z*d.Z);}}
public static class Math {public static double Abs(double x){return System.Math.Abs(x);}public static double Sqrt(double x){return System.Math.Sqrt(x);}public static double Clamp(double x,double a,double b){return System.Math.Max(a,System.Math.Min(b,x));}}
public class AICF_VehicleNativeRouteProjection {public string m_sReason="NO_PROJECTED_SEGMENT";public double m_fArc,m_fGap=double.MaxValue,m_fHeight;public int m_iSegment=-1;}
public static class AICF_VehicleNativeRouteProgress {
 public const int MAXIMUM_POINTS=512;public const double MAXIMUM_OFF_PATH_METERS=8,MAXIMUM_HEIGHT_METERS=2,ENDPOINT_XZ_TOLERANCE_METERS=3,ENDPOINT_HEIGHT_TOLERANCE_METERS=2,NONLOCAL_ARC_METERS=20,AMBIGUITY_GAP_METERS=2;
 public static bool FiniteNumber(double value) __FINITE__
 public static bool FinitePoint(Vec value) __POINT__
 public static bool SegmentProjection(Vec position,Vec first,Vec last,double prefix,out double arc,out double gap,out double height){arc=0;gap=0;height=0;__SEGMENT_INNER__}
 public static AICF_VehicleNativeRouteProjection ProjectUnique(List<Vec> points,Vec position) __PROJECT__
 public static bool ValidatePath(List<Vec> points,Vec endpoint) __VALIDATE__
}
public static class Hint {
 public const int MAXIMUM_LOCAL_POINTS=128;
 public static bool LocalFragment(List<Vec> points,Vec origin) __LOCAL__
 public static bool ForwardCandidate(List<Vec> points,double originArc,Vec candidate) __FORWARD__
 public static int Run(){int n=0;Action<bool,string>c=(b,s)=>{if(!b)throw new Exception(s);n++;};
 var origin=new Vec(1580.21,11.062,2009.42);var path=new List<Vec>{new Vec(1580.12,10.961,2008.93),new Vec(1583.6,11.8,2015.2),new Vec(1584,11.8,2015.6)};
 c(LocalFragment(path,origin),"actual three node anchored fragment");var p=AICF_VehicleNativeRouteProgress.ProjectUnique(path,origin);c(p.m_sReason=="","origin uniquely projected");
 c(!ForwardCandidate(path,p.m_fArc,new Vec(1578.45,12.6505,2017.22)),"actual sideways correction rejected");
 c(ForwardCandidate(path,p.m_fArc,new Vec(1584.4,11.8,2016.2)),"existing diagonal forward patch accepted");
 c(!AICF_VehicleNativeRouteProgress.ValidatePath(path,new Vec(1475.78,37.2685,2473.24)),"local fragment never full route proof");
 c(!LocalFragment(path,new Vec(1600,11,2009)),"foreign origin");c(!LocalFragment(path,new Vec(1580,15,2009)),"height origin mismatch");c(!LocalFragment(null,origin),"missing path");c(!LocalFragment(new List<Vec>(),origin),"empty path");c(!LocalFragment(new List<Vec>{origin},origin),"one node");
 var many=new List<Vec>();for(int i=0;i<129;i++)many.Add(origin);c(!LocalFragment(many,origin),"fragment bounded128");c(!LocalFragment(new List<Vec>{origin,new Vec(double.NaN,0,1)},origin),"nonfinite path");
 var straight=new List<Vec>{new Vec(0,0,0),new Vec(0,0,20)};
 foreach(double arc in new[]{-1.0,0,3.999})c(!ForwardCandidate(straight,0,new Vec(0,0,arc)),"no forward advance");c(ForwardCandidate(straight,0,new Vec(0,0,4)),"four meter boundary");
 c(!ForwardCandidate(straight,0,new Vec(2.001,0,8)),"sideways gap");c(ForwardCandidate(straight,0,new Vec(2,0,8)),"gap boundary");c(!ForwardCandidate(straight,0,new Vec(0,2.001,8)),"height mismatch");c(ForwardCandidate(straight,0,new Vec(0,2,8)),"height boundary");
 var loop=new List<Vec>{new Vec(0,0,0),new Vec(0,0,30),new Vec(2,0,30),new Vec(2,0,0)};c(!ForwardCandidate(loop,0,new Vec(1,0,10)),"ambiguous nonlocal projection");
 c(!ForwardCandidate(straight,0,new Vec(double.NaN,0,8)),"nonfinite candidate");c(!ForwardCandidate(new List<Vec>{origin,origin},0,origin),"degenerate fragment");return n;
 }
}
'@
$segment=(Translate (Body $progress 'SegmentProjection')).Trim();$segment=$segment.Substring(1,$segment.Length-2)
$cs=$cs.Replace('__FINITE__',(Translate (Body $progress 'FiniteNumber'))).Replace('__POINT__',(Translate (Body $progress 'FinitePoint'))).Replace('__SEGMENT_INNER__',$segment).Replace('__PROJECT__',(Translate (Body $progress 'ProjectUnique'))).Replace('__VALIDATE__',(Translate (Body $progress 'ValidatePath'))).Replace('__LOCAL__',(Translate (Body $hint 'LocalFragment'))).Replace('__FORWARD__',(Translate (Body $hint 'ForwardCandidate')))
Add-Type -TypeDefinition $cs
$actual=[Hint]::Run();$checks+=$actual
"PASS Unstuck path hint: controls=$checks actual_geometry=$actual native=NOT_RUN"

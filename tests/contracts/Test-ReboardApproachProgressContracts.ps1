param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$root=Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict'
$token=[IO.File]::ReadAllText((Join-Path $root 'State/Vehicles/AICF_VehicleBehaviorAction.c'))
$flow=[IO.File]::ReadAllText((Join-Path $root 'Vehicles/AICF_VehicleBehaviorFlow.c'))
function Body([string]$text,[string]$name){Get-AICFMethodBody ([pscustomobject]@{Source=$text;Code=(ConvertTo-AICFCodeText $text)}) $name}
$observe=Body $token 'ObserveEntryProgress'
$blocked=Body $token 'EntryBlocked'
$trace=Body $flow 'ReportReboarding'
$poll=Body $flow 'PollReboarding'
if(!$observe -or !$blocked.Contains('ObserveEntryProgress(m_Actor.GetOrigin(), m_Seat.GetPosition(), System.GetTickCount());') -or
 !$blocked.Contains('m_Seat.GetVehicle() != m_Vehicle') -or !$blocked.Contains('System.GetTickCount() - m_iLastProgressAt >= 2000')){throw 'Exact seat approach must drive existing bounded stall clock'}
if($observe -match 'CallLater|m_iRetries|m_iShortAttempts|Deadline|GetInVehicle|SetOrigin|SetWorldTransform'){throw 'Progress observation changes recovery policy'}
foreach($field in @('can_short_seat=','distance_m=','height_m=','speed_mps=','remaining_ms=','vehicle_rpl=','member_rpl=','lease_generation=','preset_revision=','accessible=','locked=','reserved_self=','occupant=','getting_in=','getting_out=','switching=','actual_seat=','native_target_accessible=','native_can_get_in=','short_attempts=','retries=','progress_age_ms=','selected_action=','executed_action=')){
 if(!$trace.Contains($field)){throw "Missing reboard diagnostic $field"}
}
if($trace.Contains('EntryBlocked(') -or $trace -match 'Retire\(|IssueEntry\(|TrySeat\(|m_iLastProgressAt\s*=|m_iShortAttempts\+\+'){throw 'Diagnostic must remain read-only'}
if(!$poll.Contains('ReportReboarding(trip, "DEADLINE");') -or !$poll.Contains('state.m_iNextReboardTraceAt = System.GetTickCount() + 5000;')){throw 'Missing bounded cadence or timeout snapshot'}
# Выполняется извлечённый production predicate: одинаковая текущая seat position
# для обеих actor positions исключает credit от одного движения машины.
$body=$observe.Replace('vector.Distance','Vec.Distance').Replace('vector ','Vec ').Replace('float ','double ')
$source=@'
using System;
public struct Vec { public double X,Y,Z; public Vec(double x,double y,double z){X=x;Y=y;Z=z;} public static double Distance(Vec a,Vec b){return Math.Sqrt((a.X-b.X)*(a.X-b.X)+(a.Y-b.Y)*(a.Y-b.Y)+(a.Z-b.Z)*(a.Z-b.Z));}}
public class Approach { public Vec m_vLastProgressPosition;public int m_iLastProgressAt=100;public bool Observe(Vec actorPosition,Vec seatPosition,int nowMs) BODY }
'@
$oldRule=$body.Replace('previousDistance - currentDistance','Vec.Distance(m_vLastProgressPosition, actorPosition)')
if($oldRule -ceq $body){throw 'Mutation target absent'}
$mutantSource='public class JitterMutant { public Vec m_vLastProgressPosition;public int m_iLastProgressAt=100;public bool Observe(Vec actorPosition,Vec seatPosition,int nowMs) '+$oldRule+' }'
Add-Type ($source.Replace('BODY',$body)+$mutantSource)
function V([double]$x,[double]$y=0,[double]$z=0){New-Object Vec($x,$y,$z)}
$checks=0
$p=New-Object Approach;$p.m_vLastProgressPosition=V 6
foreach($a in @((V 0 0 6),(V -6),(V 0 0 -6),(V 6),(V 6.5),(V 6))){if($p.Observe($a,(V 0),1000+$checks)){throw 'Circular jitter/retreat reset production progress clock'};$checks++}
if($p.m_iLastProgressAt -ne 100){throw 'Stall clock changed without approach'};$checks++
if($p.Observe((V 5.8),(V 0),2000)){throw 'Subthreshold progress credited'};$checks++
if(!$p.Observe((V 5.7),(V 0),2100) -or $p.m_iLastProgressAt -ne 2100){throw 'Cumulative physical approach not credited'};$checks++
foreach($seat in @((V 2),(V -8),(V 5.7 0 20))){if($p.Observe((V 5.7),$seat,2200)){throw 'Moving seat credited a stationary actor'};$checks++}
$p=New-Object Approach;$p.m_vLastProgressPosition=V 0 2
if(!$p.Observe((V 0 1.5),(V 0),3000)){throw 'Real vertical 3D approach ignored'};$checks++
# Мутация прежнего правила любой подвижки действительно ломает эту проверку.
$mutant=New-Object JitterMutant;$mutant.m_vLastProgressPosition=V 6
if(!$mutant.Observe((V 0 0 6),(V 0),4000)){throw 'Former jitter regression mutant unexpectedly survived'};$checks++
"PASS Reboard approach progress: extracted production predicate rejects circles, retreat and moving vehicle credit; real 3D approach retained; read-only bounded diagnostics; controls=$checks"

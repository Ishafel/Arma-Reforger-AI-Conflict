param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)),[string]$FixturePath)
$ErrorActionPreference='Stop'
. (Join-Path $RepositoryRoot 'tests/lib/Stage3StaticAudit.Common.ps1')
if(!$FixturePath){$FixturePath=Join-Path $RepositoryRoot 'tests/fixtures/AICF_PresetQueuedApproachProbe.c'}
$source=[IO.File]::ReadAllText($FixturePath)
function Body([string]$name){Get-AICFMethodBody ([pscustomobject]@{Source=$source;Code=(ConvertTo-AICFCodeText $source)}) $name}
$tick=Body 'TickPreset'
if(!$tick-or@([regex]::Matches($tick,'ObserveQueueProbe\(trip, state\)')).Count-ne2){throw 'Every native tick must observe walking before and after production, not only QueuePresetSeat'}
$latch=Body 'CanLatchQueueWalk';$poll=Body 'CanCountQueuePoll'
if(!$latch-or!$poll){throw 'Missing extracted completed-action/poll predicates'}
$name='QueueFixtureOracle'+[guid]::NewGuid().ToString('N')
$code=@'
using System;
namespace CLASSNAME { public enum EAIActionState {RUNNING,COMPLETED,FAILED,EVALUATED}
public static class Oracle {
 public static bool Latch(bool exact,bool observed,EAIActionState actionState,bool runningCurrent,float motion,float beforeDistance,float afterDistance,int polls) LATCHBODY
 public static bool Poll(bool runningCurrent,int nowMs,int lastMs) POLLBODY
} }
'@
Add-Type ($code.Replace('CLASSNAME',$name).Replace('LATCHBODY',$latch).Replace('POLLBODY',$poll))
$type=[type]($name+'.Oracle');$enum=[type]($name+'.EAIActionState');$cases=0
foreach($kind in @('running','completedNoCurrent','completedCurrent','foreignIdentity','unobservedComplete','failed','evaluated','replacedRunning','shortMotion','noTowards','twoPolls','nanMotion','nanDistance','nanBefore')){
 $exact=$true;$observed=$true;$state=[Enum]::Parse($enum,'RUNNING');$current=$true;$motion=[single]12;$before=[single]23;$after=[single]7;$polls=3
 switch($kind){completedNoCurrent{$state=[Enum]::Parse($enum,'COMPLETED');$current=$false}completedCurrent{$state=[Enum]::Parse($enum,'COMPLETED')}foreignIdentity{$exact=$false}unobservedComplete{$observed=$false;$state=[Enum]::Parse($enum,'COMPLETED')}failed{$state=[Enum]::Parse($enum,'FAILED')}evaluated{$state=[Enum]::Parse($enum,'EVALUATED')}replacedRunning{$current=$false}shortMotion{$motion=9.9}noTowards{$after=18}twoPolls{$polls=2}nanMotion{$motion=[single]::NaN}nanDistance{$after=[single]::NaN}nanBefore{$before=[single]::NaN}}
 $actual=$type.GetMethod('Latch').Invoke($null,@($exact,$observed,$state,$current,[single]$motion,[single]$before,[single]$after,$polls))
 if($actual-ne($kind-in@('running','completedNoCurrent','completedCurrent'))){throw "Observed native walk oracle escaped $kind"};$cases++
}
$pollCases=0
foreach($row in @(@($true,1500,1000,$true),@($true,1499,1000,$false),@($true,1000,1000,$false),@($true,900,1000,$false),@($false,2000,1000,$false),@($true,2000,1000,$true))){
 if($type.GetMethod('Poll').Invoke($null,@([bool]$row[0],[int]$row[1],[int]$row[2]))-ne$row[3]){throw 'Distinct500ms native polls regressed'};$pollCases++
}
$observe=Body 'ObserveQueueProbe';$currentBody=Body 'QueueProbeCurrent';$queue=Body 'QueuePresetSeat'
foreach($guard in @('m_QueueProbeApproach = current','utility.GetExecutedAction() == current','IsActionOwnedByUtility(current)','current.m_Entity.m_Value == m_QueueProbeVehicle','m_QueueProbeApproach.m_Utility != utility')){if(!$observe.Contains($guard)){throw "Missing native action capture/binding $guard"}}
foreach($guard in @('ComponentRpl(actor) != m_sQueueProbeActorRpl','ComponentRpl(m_QueueProbeVehicle) != m_sQueueProbeVehicleRpl','Math.AbsFloat(damage.GetHealthScaled() - m_fQueueProbeHealth) <= 0.001','m_QueueProbeToken.Matches(trip, trip.GetLease(), trip.GetAssignment().GetGroup())','m_QueueProbeToken.WasPresetQueued()')){if(!$currentBody.Contains($guard)){throw "Missing fresh exact identity/health $guard"}}
if(!$queue.Contains('if (!m_bQueueProbeWalkProven || !QueueProbeCurrent(trip, state)) return false;')){throw 'Queue release must require latched walk and fresh identity/health'}
if($observe-match'\.Teleport\(|SetWorldTransform|SetOrigin\(|SetHealth|SetPermanentLOD|ActivateAI|m_iQueueProbeDeadline\s*=(?!=)'){throw 'Observer mutates physical state or clock'}
"PASS Preset queue observer: extracted latch=$cases, distinct-poll=$pollCases; per-tick native capture, natural same-action completion, fresh identity/health before queue, no physical/clock mutation"

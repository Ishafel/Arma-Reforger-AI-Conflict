param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$source=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Integration/AICF_CigaretteAnimation.c'))
$fixture=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'tests/fixtures/AICF_CigaretteStreamingProbe.c'))
function Body([string]$text,[string]$name){Get-AICFMethodBody ([pscustomobject]@{Source=$text;Code=(ConvertTo-AICFCodeText $text)}) $name}
function Rules([string]$s){
 $frame=Body $s 'EOnFrame';$fixed=Body $s 'EOnPostFixedFrame';$resync=Body $s 'AICF_ResyncParticlePose';$added=Body $s 'OnAddedToParent'
 if(!$frame.Contains('m_bAICFParticlePose && AICF_IsParticlePoseDiscontinuous(distance, timeSlice)') -or
  $frame.IndexOf('AICF_ResyncParticlePose(owner, timeSlice, distance);') -gt $frame.IndexOf('super.EOnPostFixedFrame(owner, timeSlice);')){'RESYNC_BEFORE_NATIVE_PARTICLE_UPDATE'}
 foreach($guard in @('!ChimeraCharacter.Cast(owner.GetParent())','!m_CigarSmokeEmitter || !m_MouthSmokeEmitter','m_vAICFParticlePose = position;')){if(!$frame.Contains($guard)){'LIVE_PARENT_AND_EMITTERS';break}}
 if((ConvertTo-AICFCodeText $fixed).Trim('{} '+"`r`n`t") -ne ''){'NO_FIXED_DISPATCH'}
 if(!$added.Contains('m_vAICFParticlePose = child.GetOrigin();') -or !$added.Contains('m_bAICFParticlePose = true;')){'INITIAL_POSE_SAMPLE'}
 foreach($guard in @('delete m_CigarSmokeEmitter;','delete m_MouthSmokeEmitter;','super.OnAddedToParent(owner, owner.GetParent());',
  'ClearEventMask(owner, EntityEvent.POSTFIXEDFRAME);','SetEventMask(owner, EntityEvent.FRAME);',
  'if (cigarPlaying && m_CigarSmokeEmitter) m_CigarSmokeEmitter.Play();','if (mouthPlaying && m_MouthSmokeEmitter) m_MouthSmokeEmitter.Play();')){if(!$resync.Contains($guard)){'OWNED_EMITTER_RECREATE_RESTORE';break}}
 if($resync.IndexOf('delete m_MouthSmokeEmitter;') -gt $resync.IndexOf('super.OnAddedToParent(owner, owner.GetParent());')){'DELETE_BEFORE_NATIVE_RECREATE'}
 if($resync -match 'm_fEmissiveMultiplier\s*=|m_fMinDistanceSqFromCharacterFace\s*=|SetUseFrameEvent|SetOrigin|SetWorldTransform|SetHealth|SetDamage|CallLater|Rpc\('){'COSMETIC_ONLY_NATIVE_HEAT_VISIBILITY'}
 if(!(Body $s 'OnRemovedFromParent').Contains('m_bAICFParticlePose = false;') -or !(Body $s 'OnDelete').Contains('m_bAICFParticlePose = false;')){'POSE_LIFECYCLE_RESET'}
}
$issues=@(Rules $source);if($issues.Count){throw "Cigarette pose: $issues"}
$checks=1
foreach($mutation in @(
 @('m_bAICFParticlePose && AICF_IsParticlePoseDiscontinuous(distance, timeSlice)','false','RESYNC_BEFORE_NATIVE_PARTICLE_UPDATE'),
 @('delete m_CigarSmokeEmitter;','Print("skip");','OWNED_EMITTER_RECREATE_RESTORE'),
 @('ClearEventMask(owner, EntityEvent.POSTFIXEDFRAME);','Print("skip");','OWNED_EMITTER_RECREATE_RESTORE'),
 @('if (mouthPlaying && m_MouthSmokeEmitter) m_MouthSmokeEmitter.Play();','m_MouthSmokeEmitter.Play();','OWNED_EMITTER_RECREATE_RESTORE'),
 @('m_vAICFParticlePose = child.GetOrigin();','','INITIAL_POSE_SAMPLE'),
 @('m_bAICFParticlePose = false;','','POSE_LIFECYCLE_RESET')
)){
 $changed=$source.Replace($mutation[0],$mutation[1]);if($changed -ceq $source -or $mutation[2] -notin @(Rules $changed)){throw "Surviving pose mutant $($mutation[2])"};$checks++
}
$predicate=Body $source 'AICF_IsParticlePoseDiscontinuous'
$jumpValue=[regex]::Match($source,'AICF_PARTICLE_POSE_JUMP_METERS\s*=\s*([0-9.]+)').Groups[1].Value
$speedValue=[regex]::Match($source,'AICF_PARTICLE_SMOOTH_SPEED_MPS\s*=\s*([0-9.]+)').Groups[1].Value
if(!$jumpValue -or !$speedValue){throw 'Production pose thresholds missing'}
$body=Body $source 'AICF_ResyncParticlePose'
$body=$body.Substring(0,$body.IndexOf('m_iAICFParticlePoseResyncs++;'))+'}'
$body=$body.Replace('m_CigarSmokeEmitter &&','m_CigarSmokeEmitter != null &&').Replace('m_MouthSmokeEmitter &&','m_MouthSmokeEmitter != null &&').Replace('&& m_CigarSmokeEmitter)','&& m_CigarSmokeEmitter != null)').Replace('&& m_MouthSmokeEmitter)','&& m_MouthSmokeEmitter != null)').Replace('delete m_CigarSmokeEmitter;','m_CigarSmokeEmitter.Deleted = true;').Replace('delete m_MouthSmokeEmitter;','m_MouthSmokeEmitter.Deleted = true;').Replace('super.OnAddedToParent(owner, owner.GetParent());','SpawnNative(owner, owner.GetParent());')
$type=@'
using System;
public enum EParticleEffectState { STOPPED,PLAYING }
public class ParticleEffectEntity { public EParticleEffectState State; public bool Deleted;public int Plays;public EParticleEffectState GetState(){return State;}public void Play(){State=EParticleEffectState.PLAYING;Plays++;}}
public class Owner { public Owner GetParent(){return this;} }
public static class EntityEvent {public const int POSTFIXEDFRAME=1,FRAME=2;}
public class CigarettePose {const double AICF_PARTICLE_POSE_JUMP_METERS=JUMP_VALUE,AICF_PARTICLE_SMOOTH_SPEED_MPS=SPEED_VALUE;public ParticleEffectEntity m_CigarSmokeEmitter=new ParticleEffectEntity(),m_MouthSmokeEmitter=new ParticleEffectEntity();public int Mask=1;public float Heat=-1234;public bool MissingCigar,MissingMouth;public void SpawnNative(Owner a,Owner b){m_CigarSmokeEmitter=MissingCigar?null:new ParticleEffectEntity();m_MouthSmokeEmitter=MissingMouth?null:new ParticleEffectEntity();Mask|=1;}public void ClearEventMask(Owner a,int m){Mask&=~m;}public void SetEventMask(Owner a,int m){Mask|=m;} public bool Discontinuous(double distanceMeters,double timeSlice) PREDICATE public void Resync(Owner owner,double timeSlice,double distance) RESYNC }
'@
Add-Type ($type.Replace('PREDICATE',$predicate).Replace('RESYNC',$body).Replace('JUMP_VALUE',$jumpValue).Replace('SPEED_VALUE',$speedValue))
$probe=New-Object CigarettePose
foreach($case in @(@(0,0,$false),@(5,0.016,$false),@(5.001,0.016,$true),@(10,0.1,$false),@(10.01,0.1,$true),@(90,1,$false),@(101,1,$true),@(1625,0.017,$true),@(5,-1,$false))){if($probe.Discontinuous($case[0],$case[1]) -ne $case[2]){throw "Production pose threshold failed: $case"};$checks++}
foreach($cigar in @($false,$true)){foreach($mouth in @($false,$true)){foreach($missing in @(0,1,2)){
 $p=New-Object CigarettePose
 if($cigar){$p.m_CigarSmokeEmitter.State=[EParticleEffectState]::PLAYING};if($mouth){$p.m_MouthSmokeEmitter.State=[EParticleEffectState]::PLAYING}
 $oldCigar=$p.m_CigarSmokeEmitter;$oldMouth=$p.m_MouthSmokeEmitter;$p.MissingCigar=$missing -eq 1;$p.MissingMouth=$missing -eq 2
 $p.Resync((New-Object Owner),0.016,1625)
 if(!$oldCigar.Deleted -or !$oldMouth.Deleted -or $p.Mask -ne 2 -or $p.Heat -ne -1234){throw 'Production recreate changed heat or failed ownership/masks'}
 if($p.m_CigarSmokeEmitter -and ($p.m_CigarSmokeEmitter.Plays -ne [int]$cigar)){throw 'Cigar playing state lost'}
 if($p.m_MouthSmokeEmitter -and ($p.m_MouthSmokeEmitter.Plays -ne [int]$mouth)){throw 'Mouth playing state lost'}
 $checks++
}}}
foreach($guard in @('AICF_StreamingPose(m_AICFSmokeProp)','AICF_StreamingEmission()','m_CigarSmokeEmitter.GetLeadingParent() == owner','m_MouthSmokeEmitter.HasActiveParticles()',
 'm_AICFSmokeCigar = m_AICFSmokeAnimation.AICF_StreamingCigar();','m_AICFSmokeMouth = m_AICFSmokeAnimation.AICF_StreamingMouth();')){if(!$fixture.Contains($guard)){throw "Missing native proof $guard"}}
if($fixture -match 'CigaretteBindCandidate|CigaretteResyncCandidate|CIGARETTE_RESYNC_CANDIDATE'){throw 'Final fixture must call production, no candidate implementation'}
"PASS Cigarette pose contracts: extracted thresholds/recreate state matrix, preserved heat/native visibility, FRAME masks and current emitter deletion proof; controls=$checks"

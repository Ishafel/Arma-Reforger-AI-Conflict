param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$fixture=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'tests/fixtures/AICF_TransportCombatResumeProbe.c'))
function Body([string]$source,[string]$name){Get-AICFMethodBody ([pscustomobject]@{Source=$source;Code=(ConvertTo-AICFCodeText $source)}) $name}
function Rules([string]$source){
 $trace=[regex]::Match($source,'(?sm)^class AICF_TCRDamageTrace\s*\{.*?^\}').Value
 $scope=Body $trace 'ScopeCurrent';$identity=Body $trace 'SourceCurrent';$damage=Body $trace 'Damage';$collision=Body $trace 'Collision';$clear=Body $trace 'Clear';$bind=Body $trace 'Bind'
 $nativeDamage=Body $source 'OnDamage';$nativeCollision=Body $source 'ProcessCollision';$arm=Body $source 'AICF_TCRArm';$finish=Body $source 'AICF_TCRFinish';$stop=Body $source 'Stop';$update=Body $source 'Update'
 if(!$trace -or !$scope -or !$identity -or !$damage -or !$collision -or !$nativeDamage -or !$nativeCollision){'MISSING_CAUSAL_OBSERVER';return}
 $code=ConvertTo-AICFCodeText $trace
 if($code -match '\b(?:HandleDamage|SetDamage|SetHealth|SetInstigator|CreateInstigator|EnableDamageHandling|SetMovementDamage|SetOrigin|SetVelocity|SetCruiseSpeed|CallLater|Insert)\s*\('){'READ_ONLY_CONTEXT_WITHOUT_INJECTION'}
 if($trace -notmatch 'protected static AICF_TCRGate s_Observed;' -or $trace -match 'static ref AICF_TCRGate s_Observed' -or $bind -notmatch 'ScopeCurrent\(gate\)' -or $clear -notmatch 's_Observed = null' -or $arm -notmatch 'AICF_TCRDamageTrace.Bind\(gate\)' -or $finish -notmatch 'AICF_TCRDamageTrace.Clear\(\)' -or $stop -notmatch 'AICF_TCRDamageTrace.Clear\(\)' -or $update -match 'AICF_TCRDamageTrace.Clear\(\)'){'WEAK_LIFETIME_THROUGH_RELEASE_CLEARED_ON_STOP'}
 foreach($fence in @('!Replication.IsServer()','System.GetCLIParam("aicfTransportCombatResume", enabled)','enabled != "1"','gate.m_Trip.IsTerminal()','gate.m_Trip.GetPhase() != AICF_ETransportTripPhase.TRANSIT','gate.m_Trip.GetOperationId() != gate.m_sOperation','gate.m_Trip.GetTripGeneration() != gate.m_iTripGeneration','gate.m_Trip.GetGroupGeneration() != gate.m_iGroupGeneration','!gate.m_Trip.IsVehiclePresetCurrent()','gate.m_Trip.GetAbsoluteDeadlineMs() != gate.m_iDeadline','gate.m_Slot.GetGroup() != gate.m_Group','gate.m_Slot.GetSpawnGeneration() != gate.m_iGroupGeneration','gate.m_Slot.GetStrategicIntentRevision() != gate.m_iIntent','gate.m_Slot.GetVehiclePresetRevision() != gate.m_iPresetRevision','gate.m_Trip.GetAssignment().GetAssignmentRevision() != gate.m_iAssignment','gate.m_Trip.GetAssignment().GetTargetPosition() != gate.m_vTarget','gate.m_Group.GetID() != gate.m_GroupId','AICF_TCRGate.ComponentRpl(gate.m_Group) != gate.m_sGroupRpl','gate.m_Trip.GetLease() != gate.m_Lease','gate.m_Lease.GetLeaseGeneration() != gate.m_iLeaseGeneration','gate.m_Lease.GetVehicleGeneration() != gate.m_iVehicleGeneration','gate.m_Lease.MatchesTripIdentity(','gate.m_Vehicle.GetID() != gate.m_VehicleId','AICF_TCRGate.ComponentRpl(gate.m_Vehicle) != gate.m_sVehicleRpl','gate.m_Lease.MatchesEntityIdentity(')){
  if(!$scope.Contains($fence)){'FULL_EXACT_GENERATIONAL_SCOPE';break}
 }
 if($scope -match '\.Matches\(\)|RejectionReason|VehicleCanMove|VehicleIsOnFire|GetEngineFunctional|GetCurrentAction'){'OBSERVE_DAMAGE_WITHOUT_HEALTH_OR_ACTIVITY_VETO'}
 foreach($fence in @('!ScopeCurrent(gate)','source.GetOwner() != owner','owner.GetID() != ownerId','owner != gate.m_Vehicle && owner.GetRootParent() != gate.m_Vehicle')){if(!$identity.Contains($fence)){'EXACT_SOURCE_OWNER_HIERARCHY';break}}
 foreach($body in @($damage,$collision)){
  if(([regex]::Matches($body,'SourceCurrent\(gate, source, owner, ownerId\)')).Count -ne 2 -or $body.LastIndexOf('SourceCurrent(gate, source, owner, ownerId)') -gt $body.IndexOf('Print(details)')){'REPEAT_SOURCE_FENCE_BEFORE_PUBLISH'}
 }
 if(([regex]::Matches($nativeDamage,'super.OnDamage\(damageContext\)')).Count -ne 1 -or $nativeDamage -notmatch 'AICF_TCRDamageTrace.Damage\(this, damageContext\)' -or $nativeDamage.IndexOf('super.OnDamage') -lt $nativeDamage.IndexOf('AICF_TCRDamageTrace.Damage') -or $nativeDamage -match '\breturn\b'){'EXACTLY_ONE_NATIVE_DAMAGE_SUPER'}
 if(([regex]::Matches($nativeCollision,'super.ProcessCollision\(collision\)')).Count -ne 1 -or $nativeCollision -notmatch 'AICF_TCRDamageTrace.Collision\(this, collision\)' -or $nativeCollision.IndexOf('super.ProcessCollision') -lt $nativeCollision.IndexOf('AICF_TCRDamageTrace.Collision') -or $nativeCollision -match '\breturn\b'){'EXACTLY_ONE_NATIVE_COLLISION_SUPER'}
 foreach($read in @('context.damageType','context.damageValue','context.hitEntity','context.damageSource','context.instigator.GetInstigatorEntity()','context.instigator.GetInstigatorPlayerID()','context.struckHitZone.GetName()','context.struckHitZone.Type()','context.colliderID','context.hitPosition','context.hitDirection','context.hitNormal','context.impactVelocity','source.IsDamageHandlingEnabled()')){if(!$damage.Contains($read)){'ORIGINAL_NATIVE_DAMAGE_CONTEXT';break}}
 foreach($read in @('collision.m_Other','collision.m_OtherPilot','collision.m_fImpulse','collision.m_fTotalMomentumOwner','collision.m_fTotalMomentumOther','collision.m_fDamageShare','collision.GetAverageImpactPosition()','collision.GetOwnerVelocityBefore()','collision.GetOtherVelocityBefore()','collision.m_vOwnerOriginAtThePointOfImpact','collision.m_vOtherOriginAtThePointOfImpact','collision.m_bOtherIsDynamic','collision.m_bOtherIsDestructible','collision.m_iOtherResponseIndex')){if(!$collision.Contains($read)){'ORIGINAL_NATIVE_COLLISION_CONTEXT';break}}
 if($damage -notmatch 's_iDamageEvents >= 2048' -or $collision -notmatch 's_iCollisionEvents >= 2048' -or $damage -notmatch '\[TCR_DAMAGE\].*step=-1' -or $collision -notmatch '\[TCR_COLLISION\].*step=-1'){'BOUNDED_CALLBACK_RECEIPTS_WITHOUT_FAKE_STEP'}
}
$issues=@(Rules $fixture)
if($issues.Count){throw "Transport damage trace contracts: $issues"}
$controls=1
foreach($mutation in @(
 @('protected static AICF_TCRGate s_Observed;','protected static ref AICF_TCRGate s_Observed;','WEAK_LIFETIME_THROUGH_RELEASE_CLEARED_ON_STOP'),
 @('AICF_TCRDamageTrace.Bind(gate);','Print("not bound");','WEAK_LIFETIME_THROUGH_RELEASE_CLEARED_ON_STOP'),
 @('AICF_TCRDamageTrace.Clear();','Print("not cleared");','WEAK_LIFETIME_THROUGH_RELEASE_CLEARED_ON_STOP'),
 @('gate.m_Trip.GetGroupGeneration() != gate.m_iGroupGeneration','false','FULL_EXACT_GENERATIONAL_SCOPE'),
 @('gate.m_Slot.GetStrategicIntentRevision() != gate.m_iIntent','false','FULL_EXACT_GENERATIONAL_SCOPE'),
 @('gate.m_Lease.GetVehicleGeneration() != gate.m_iVehicleGeneration','false','FULL_EXACT_GENERATIONAL_SCOPE'),
 @('AICF_TCRGate.ComponentRpl(gate.m_Vehicle) != gate.m_sVehicleRpl','false','FULL_EXACT_GENERATIONAL_SCOPE'),
 @('owner != gate.m_Vehicle && owner.GetRootParent() != gate.m_Vehicle','false','EXACT_SOURCE_OWNER_HIERARCHY'),
 @('source.GetOwner() != owner','false','EXACT_SOURCE_OWNER_HIERARCHY'),
 @('owner.GetID() != ownerId','false','EXACT_SOURCE_OWNER_HIERARCHY'),
 @('if (!SourceCurrent(gate, source, owner, ownerId)) return;','if (false) return;','REPEAT_SOURCE_FENCE_BEFORE_PUBLISH'),
 @('super.OnDamage(damageContext);','Print("skipped native damage callback");','EXACTLY_ONE_NATIVE_DAMAGE_SUPER'),
 @('super.ProcessCollision(collision);','Print("skipped native collision");','EXACTLY_ONE_NATIVE_COLLISION_SUPER'),
 @('context.damageSource','context.hitEntity','ORIGINAL_NATIVE_DAMAGE_CONTEXT'),
 @('context.instigator.GetInstigatorEntity()','null','ORIGINAL_NATIVE_DAMAGE_CONTEXT'),
 @('collision.GetOwnerVelocityBefore()','vector.Zero','ORIGINAL_NATIVE_COLLISION_CONTEXT')
)){
 if(!$fixture.Contains($mutation[0])){throw "Mutation target absent: $($mutation[0])"}
 if($mutation[2] -notin @(Rules ($fixture.Replace($mutation[0],$mutation[1])))){throw "Mutation survived: $($mutation[2])"}
 $controls++
}
$scope=Body $fixture 'ScopeCurrent'
if('OBSERVE_DAMAGE_WITHOUT_HEALTH_OR_ACTIVITY_VETO' -notin @(Rules ($fixture.Replace($scope,$scope.Insert(1,'if (!gate.Matches()) return false;'))))){throw 'Health-gated damage observation survived'}
$controls++
$damage=Body $fixture 'Damage'
if('READ_ONLY_CONTEXT_WITHOUT_INJECTION' -notin @(Rules ($fixture.Replace($damage,$damage.Insert(1,'source.SetMovementDamage(0);'))))){throw 'Damage injection survived'}
$controls++
"PASS Transport damage trace: exact weak hierarchy scope/repeated identities, untouched native callbacks once, original damage/collision context, lifecycle through release; controls=$controls"

param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$fixture=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'tests/fixtures/AICF_VehicleMultiSquadProbe.c'))
$runner=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'tools/Invoke-AICFVehicleMultiSquad.ps1'))
function Body([string]$text,[string]$name){Get-AICFMethodBody ([pscustomobject]@{Source=$text;Code=(ConvertTo-AICFCodeText $text)}) $name}
function Rules([string]$text){
 $selected=Body $text 'SelectedScope';$allowed=Body $text 'ScopeAllowed';$collect=Body $text 'Collect';$apply=Body $text 'Apply';$under=Body $text 'UnderRoot'
 $protect=Body $text 'AICF_MultiProtect';$observe=Body $text 'AICF_MultiProtectHierarchy';$update=Body $text 'Update';$environment=Body $text 'AICF_MultiEnvironment'
 if($selected -notmatch 'if \(!System.GetCLIParam\("aicfVehicleMultiProtectionScope", scope\)\) return "ROOT_ONLY";'){'HISTORICAL_ROOT_DEFAULT'}
 if($allowed -notmatch 'if \(scope == "ROOT_ONLY"\) return true;' -or $allowed -notmatch 'return mode == "CAPACITY" && scope == "HIERARCHY";'){'CAPACITY_ONLY_OPT_IN'}
 $check='!AICF_MultiHierarchyProtection.ScopeAllowed(m_sAICFMultiMode, AICF_MultiHierarchyProtection.SelectedScope())'
 if(!$environment.Contains($check) -or !$update.Contains($check) -or $update.IndexOf($check) -gt $update.IndexOf('AICF_MultiEnvironment()') -or $update -notmatch 'AICF_MultiAbort\("PROTECTION_SCOPE_INVALID"\)'){'INVALID_SCOPE_ABORT_BEFORE_ENVIRONMENT'}
 foreach($guard in @('!Replication.IsServer()','mode != "CAPACITY"','SelectedScope() != "HIERARCHY"','entity.GetWorld() != GetGame().GetWorld()','!rpl.IsMaster()','rpl.IsProxy()','root.GetOwner() != entity')){
  if(!$apply.Contains($guard) -or $apply.IndexOf($guard) -gt $apply.IndexOf('manager.EnableDamageHandling(false)')){'AUTHORITATIVE_CONTROLLED_CONTEXT'}
 }
 foreach($guard in @('nativeCount = root.GetAllHitZonesInHierarchy(zones)','nativeCount <= 0 || nativeCount != zones.Count() || zones.Count() > 512',
  'managers.Insert(root)','owners.Insert(entity)','zone.GetHitZoneContainer()','manager.GetOwner()',
  'owner.GetWorld() != entity.GetWorld()','owner.FindComponent(DamageManagerComponent) != manager','!UnderRoot(root, manager)',
  'if (managers.Contains(manager)) continue;','owners.Insert(owner)')){if(!$collect.Contains($guard)){'FULL_DEDUPLICATED_NATIVE_HIERARCHY'}}
 if($under -notmatch 'depth < 64' -or $under -notmatch 'current == root' -or $under -notmatch 'current.GetParentHitZoneContainer\(\)'){'BOUNDED_REGISTERED_PARENT_PROOF'}
 if($apply -notmatch 'if \(!Collect\(entity, root, zones, managers, owners, nativeCount, reason\)\)' -or $apply.IndexOf('Collect(entity, root, zones') -gt $apply.IndexOf('manager.EnableDamageHandling(false)') -or
  $apply -notmatch 'if \(!Collect\(entity, root, repeatedZones, repeatedManagers, repeatedOwners, repeatedCount, reason\)\)' -or $apply.IndexOf('Collect(entity, root, repeatedZones') -lt $apply.IndexOf('manager.EnableDamageHandling(false)')){'FULL_COLLECTION_BEFORE_MUTATION_AND_REPEATED_READBACK'}
 foreach($guard in @('nativeCount != repeatedCount','zones.Count() != repeatedZones.Count()','managers.Count() != repeatedManagers.Count()',
  '!repeatedZones.Contains(originalZone)','repeatedManagers.Find(originalManager)','repeatedIndex < 0',
  'repeatedOwners[repeatedIndex] != owners[managerIndex]','originalManager.GetOwner() != owners[managerIndex]',
  'originalManager.IsDamageHandlingEnabled()','entity.GetID() != identity','entity.FindComponent(RplComponent) != rpl',
  'entity.FindComponent(DamageManagerComponent) != root','errors == 0 && enabledAfter == 0')){if(!$apply.Contains($guard)){'EXACT_COMPLETE_DISABLED_READBACK'}}
 $helper=$text.Substring(0,$text.IndexOf('class AICF_MultiConcurrency'))
 if($helper -match 'SetHealth|SetMovementDamage|GetPhysics|SetOrigin|Teleport|SetWorldTransform|GetCallqueue|CallLater|Complete\(|\.Fail\(|SetPhase|CommitTransition|m_i.*Deadline\s*=|m_i.*Attempts\s*='){'NO_HEAL_PHYSICS_ACTION_OR_BUDGET_SIDE_EFFECTS'}
 if(@([regex]::Matches($helper,'\.EnableDamageHandling\(false\)')).Count -ne 1 -or $helper -match '\.EnableDamageHandling\(true\)'){'ONLY_DISABLE_EXACT_NATIVE_MANAGERS'}
 if($protect -notmatch 'm_sAICFMultiMode != "CAPACITY"' -or $protect -notmatch '!Replication.IsServer\(\)' -or $protect -notmatch 'SelectedScope\(\) == "HIERARCHY"' -or
  $protect -notmatch 'AICF_MultiProtectHierarchy\(member, item, "MEMBER", emit\)' -or $protect -notmatch 'AICF_MultiProtectHierarchy\(trip.GetLease\(\).GetVehicle\(\), item, "VEHICLE", emit\)' -or
  $protect -notmatch 'member.GetDamageManager\(\).EnableDamageHandling\(false\)' -or $protect -notmatch 'damage.EnableDamageHandling\(false\)'){'EXPLICIT_BRANCH_WITH_HISTORICAL_ROOT_BEHAVIOR'}
 if($observe -notmatch 'AICF_MultiHierarchyProtection.Apply\(entity, details\)' -or $observe -notmatch 'AICF_MultiLog\("PROTECTION"' -or $observe -notmatch 'if \(!covered\) AICF_MultiFail\(item, "HIERARCHY_PROTECTION_FAILED"\)' -or
  $text -notmatch 'result \+= " protection_scope=" \+ scope \+ " test_only=1"' -or $text -notmatch 'protection_enabled_after=%7' -or $text -notmatch 'protection_managers_covered=%4' -or $text -notmatch 'protection_zones_repeated=%3'){'CLASSIFIED_COVERAGE_AND_FAILURE_EVIDENCE'}
 $metadata=Body $text 'Metadata';$log=Body $text 'AICF_MultiLog'
 if(@([regex]::Matches($metadata,'string.Format\(')).Count -gt 1 -or $metadata -match 'return result \+ string.Format' -or
  $log -notmatch 'string line = string.Format' -or $log -notmatch 'string metadata = AICF_MultiConcurrency.Metadata' -or $log -notmatch 'Print\(line \+ metadata\)' -or
  $environment -notmatch 'string line = string.Format' -or $environment -notmatch 'string metadata = AICF_MultiConcurrency.Metadata' -or $environment -notmatch 'Print\(line \+ metadata\)' -or
  $log -match 'Print\(string.Format' -or $environment -match 'Print\(string.Format'){'MATERIALIZED_PREFIXES_BEFORE_METADATA'}
}
$failures=@(Rules $fixture)
if($failures.Count){throw "Vehicle multi protection contract: $($failures -join ',')"}
$controls=1
foreach($mutation in @(
 @('return "ROOT_ONLY"','return "HIERARCHY"','HISTORICAL_ROOT_DEFAULT'),
 @('return mode == "CAPACITY" && scope == "HIERARCHY";','return scope == "HIERARCHY";','CAPACITY_ONLY_OPT_IN'),
 @('AICF_MultiAbort("PROTECTION_SCOPE_INVALID")','AICF_MultiFail(null, "INVALID")','INVALID_SCOPE_ABORT_BEFORE_ENVIRONMENT'),
 @('SelectedScope() != "HIERARCHY"','false','AUTHORITATIVE_CONTROLLED_CONTEXT'),
 @('!rpl.IsMaster()','false','AUTHORITATIVE_CONTROLLED_CONTEXT'),
 @('owner.FindComponent(DamageManagerComponent) != manager','false','FULL_DEDUPLICATED_NATIVE_HIERARCHY'),
 @('if (managers.Contains(manager)) continue;','if (false) continue;','FULL_DEDUPLICATED_NATIVE_HIERARCHY'),
 @('nativeCount != zones.Count()','false','FULL_DEDUPLICATED_NATIVE_HIERARCHY'),
 @('depth < 64','true','BOUNDED_REGISTERED_PARENT_PROOF'),
 @('Collect(entity, root, repeatedZones, repeatedManagers, repeatedOwners, repeatedCount, reason)','Collect(entity, root, zones, managers, owners, nativeCount, reason)','FULL_COLLECTION_BEFORE_MUTATION_AND_REPEATED_READBACK'),
 @('!repeatedZones.Contains(originalZone)','false','EXACT_COMPLETE_DISABLED_READBACK'),
 @('repeatedOwners[repeatedIndex] != owners[managerIndex]','false','EXACT_COMPLETE_DISABLED_READBACK'),
 @('entity.GetID() != identity','false','EXACT_COMPLETE_DISABLED_READBACK'),
 @('errors == 0 && enabledAfter == 0','errors == 0','EXACT_COMPLETE_DISABLED_READBACK'),
 @('manager.EnableDamageHandling(false)','manager.EnableDamageHandling(false); manager.SetHealthScaled(1)','NO_HEAL_PHYSICS_ACTION_OR_BUDGET_SIDE_EFFECTS'),
 @('manager.EnableDamageHandling(false)','manager.EnableDamageHandling(true)','ONLY_DISABLE_EXACT_NATIVE_MANAGERS'),
 @('if (!covered) AICF_MultiFail(item, "HIERARCHY_PROTECTION_FAILED")','if (false) AICF_MultiFail(item, "HIERARCHY_PROTECTION_FAILED")','CLASSIFIED_COVERAGE_AND_FAILURE_EVIDENCE'),
 @('result += " protection_scope=" + scope + " test_only=1"','result += " scope=" + scope','CLASSIFIED_COVERAGE_AND_FAILURE_EVIDENCE'),
 @('Print(line + metadata)','Print(string.Format("lost"))','MATERIALIZED_PREFIXES_BEFORE_METADATA'),
 @('return result;','return result + string.Format(" protection_scope=%1 test_only=1", scope);','MATERIALIZED_PREFIXES_BEFORE_METADATA')
)){
 if(!$fixture.Contains($mutation[0])){throw "Missing mutation target: $($mutation[0])"}
 $changed=$fixture.Replace($mutation[0],$mutation[1]);$rejected=@(Rules $changed)
 if($mutation[2] -notin $rejected){throw "Negative control accepted: $($mutation[2])"}
 $controls++
}
$allowedBody=Body $fixture 'ScopeAllowed'
$rootExpression=[regex]::Match($allowedBody,'if \(([^\r\n]+)\) return true;').Groups[1].Value
$hierarchyExpression=[regex]::Match($allowedBody,'return (mode[^;]+);').Groups[1].Value
function To-PS([string]$expression){$expression=[regex]::Replace($expression,'\b(mode|scope)\b', '$$$1');return $expression.Replace('==','-eq').Replace('&&','-and')}
$scopePredicate=[scriptblock]::Create('param($mode,$scope) if ('+(To-PS $rootExpression)+'){ return $true }; return ('+(To-PS $hierarchyExpression)+')')
foreach($case in @(@('NORMAL','ROOT_ONLY',$true),@('CAPACITY','ROOT_ONLY',$true),@('NORMAL','HIERARCHY',$false),@('CAPACITY','HIERARCHY',$true),@('CAPACITY','unknown',$false),@('NORMAL','unknown',$false),@('CAPACITY','',$false))){
 if([bool](& $scopePredicate $case[0] $case[1]) -ne $case[2]){throw 'Actual fixture scope predicate mismatch'}
 $controls++
}
if($runner -notmatch "\[ValidateSet\('ROOT_ONLY','HIERARCHY'\)\]\[string\]\`$ProtectionScope='ROOT_ONLY'" -or
 !$runner.Contains("if(`$ProtectionScope -eq 'HIERARCHY' -and `$Mode -ne 'CAPACITY'){throw") -or
 !$runner.Contains("'-aicfVehicleMultiProtectionScope',`$ProtectionScope") -or !$runner.Contains('protection_scope=$ProtectionScope')){throw 'Canonical runner must explicitly bind opt-in scope and reject NORMAL hierarchy'}
$controls++
"PASS Multi hierarchy protection: controlled opt-in only, full native manager/owner readback, no heal/physics/action changes; controls=$controls; native runtime separate"
exit 0

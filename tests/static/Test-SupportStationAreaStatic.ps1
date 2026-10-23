param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$source=ConvertTo-AICFCodeText ([IO.File]::ReadAllText((Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Integration/AICF_SupportStationArea.c')))
function Test-AreaContracts([string]$s) {
    if($s -match 'SetOrigin\(|SetWorldTransform\(|SetOffset\(|BumpMe\(|LogLevel|override void OnDelete|CallLater|\.Insert\(') { 'VISUAL_BOUNDARY' }
    foreach($rule in @(
        @('Init\(IEntity owner\)\s*\{\s*AICF_AlignSupportArea\(owner\);\s*super.Init\(owner\);','NATIVE_INIT'),
        @('GenerateAreaMesh\(\)\s*\{\s*AICF_AlignSupportArea\(GetOwner\(\)\);\s*super.GenerateAreaMesh\(\);','NATIVE_GENERATION'),
        @('if \(!owner\) return;.*?if \(!m_SupportStationComponent\) m_SupportStationComponent = GetSupportStation\(owner\);.*?!m_SupportStationComponent.UsesRange\(\)\) return;','STATION_READINESS'),
        @('if \(owner.GetParent\(\) != m_SupportStationComponent.GetOwner\(\)\) return;','DIRECT_CHILD'),
        @('vector offset = m_SupportStationComponent.GetOffset\(\);\s*if \(offset == vector.Zero\) return;','STATION_OFFSET'),
        @('owner.GetLocalTransform\(localTransform\);\s*if \(localTransform\[3\] == offset\) return;\s*localTransform\[3\] = offset;\s*owner.SetLocalTransform\(localTransform\);','POSITION_ONLY')
    )) { if($s -notmatch ('(?s)'+$rule[0])) { $rule[1] } }
}
$failures=@(Test-AreaContracts $source)
$mutations=@(
    @('super.Init(owner);','','NATIVE_INIT'),
    @('super.GenerateAreaMesh();','','NATIVE_GENERATION'),
    @('!m_SupportStationComponent.UsesRange()','false','STATION_READINESS'),
    @('owner.GetParent() != m_SupportStationComponent.GetOwner()','false','DIRECT_CHILD'),
    @('offset == vector.Zero','false','STATION_OFFSET'),
    @('if (localTransform[3] == offset) return;','','POSITION_ONLY'),
    @('localTransform[3] = offset;','localTransform[3] = vector.Zero;','POSITION_ONLY'),
    @('owner.SetLocalTransform(localTransform);','m_SupportStationComponent.GetOwner().SetLocalTransform(localTransform);','POSITION_ONLY'),
    @('owner.SetLocalTransform(localTransform);','owner.SetWorldTransform(localTransform);','VISUAL_BOUNDARY')
)
foreach($mutation in $mutations) {
    if(!$source.Contains($mutation[0])) { throw "Missing mutation: $($mutation[2])" }
    if($mutation[2] -notin @(Test-AreaContracts ($source.Replace($mutation[0],$mutation[1])))) { $failures+="MUTATION_ESCAPED $($mutation[2])" }
}
if($failures.Count) { $failures; exit 1 }
"Support station area static: PASS; negative mutations=$($mutations.Count)"

param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$source=ConvertTo-AICFCodeText ([IO.File]::ReadAllText((Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Integration/AICF_InstigatorContext.c')))
function Test-InstigatorContracts([string]$s) {
    if($s -match 'Print\(|LogLevel|SetInstigator\(|SetInstigatorByPlayerID|\.Kill\(|SetHealth|EnableDamageHandling|BumpMe|CallLater|\.Insert\('){'CONTEXT_ONLY'}
    foreach($rule in @(
        @('modded class SCR_InstigatorContextData.*?override void CreateInstigatorData','NATIVE_BOUNDARY'),
        @('if \(instigator.GetInstigatorType\(\) != InstigatorType.INSTIGATOR_OTHER \|\| isDeleted \|\| !ChimeraCharacter.Cast\(victimEntity\)\).*?super.CreateInstigatorData\(victimPlayerID, victimEntity, killerEntity, instigator, isDeleted\);\s*return;','NATIVE_PASSTHROUGH'),
        @('IEntity source = killerEntity;\s*if \(!source\) source = instigator.GetInstigatorEntity\(\);','CALLER_SOURCE'),
        @('if \(ChimeraCharacter.Cast\(source\)\).*?Instigator characterInstigator = Instigator.CreateInstigator\(source\);.*?type == InstigatorType.INSTIGATOR_AI \|\| type == InstigatorType.INSTIGATOR_PLAYER','CHARACTER_RESOLUTION'),
        @('super.CreateInstigatorData\(victimPlayerID, victimEntity, source, characterInstigator, false\);\s*m_Instigator = instigator;\s*return;','NATIVE_CHARACTER_RULES'),
        @('m_iVictimPlayerID = victimPlayerID;\s*if \(m_iVictimPlayerID <= 0\)\s*m_iVictimPlayerID = SCR_PossessingManagerComponent.GetPlayerIdFromControlledEntity\(victimEntity\);','VICTIM_ID'),
        @('m_VictimEntity = victimEntity;\s*m_KillerEntity = source;\s*m_Instigator = instigator;\s*m_iKillerPlayerID = instigator.GetInstigatorPlayerID\(\);','ORIGINAL_METADATA'),
        @('m_eKillerControlType = SCR_ECharacterControlType.UNKNOWN;','UNOWNED_SOURCE'),
        @('m_eVictimControlType = SCR_ECharacterControlType.AI;\s*if \(m_iVictimPlayerID > 0\)\s*m_eVictimControlType = SCR_CharacterHelper.GetCharacterControlType\(victimEntity\);','VICTIM_CONTROL'),
        @('m_eKillerDisguiseType = SCR_ECharacterDisguiseType.DEFAULT_FACTION;\s*m_eVictimDisguiseType = SCR_ECharacterDisguiseType.DEFAULT_FACTION;\s*SetPerceivedFaction\(m_iVictimPlayerID, victimEntity, source, instigator, false\);','PERCEIVED_FACTION'),
        @('m_eVictimKillerRelation = SCR_ECharacterDeathStatusRelations.OTHER_DEATH;','ENVIRONMENT_RELATION')
    )){if($s -notmatch ('(?s)'+$rule[0])){$rule[1]}}
}
$failures=@(Test-InstigatorContracts $source)
$mutations=@(
    @('!= InstigatorType.INSTIGATOR_OTHER','== InstigatorType.INSTIGATOR_OTHER','NATIVE_PASSTHROUGH'),
    @('|| isDeleted','|| false','NATIVE_PASSTHROUGH'),
    @('|| !ChimeraCharacter.Cast(victimEntity)','|| false','NATIVE_PASSTHROUGH'),
    @('IEntity source = killerEntity;','IEntity source;','CALLER_SOURCE'),
    @('type == InstigatorType.INSTIGATOR_AI || type == InstigatorType.INSTIGATOR_PLAYER','true','CHARACTER_RESOLUTION'),
    @('m_Instigator = instigator;','m_Instigator = characterInstigator;','NATIVE_CHARACTER_RULES'),
    @('m_iVictimPlayerID <= 0','false','VICTIM_ID'),
    @('m_KillerEntity = source;','m_KillerEntity = null;','ORIGINAL_METADATA'),
    @('m_eKillerControlType = SCR_ECharacterControlType.UNKNOWN;','m_eKillerControlType = SCR_ECharacterControlType.AI;','UNOWNED_SOURCE'),
    @('if (m_iVictimPlayerID > 0)','if (false)','VICTIM_CONTROL'),
    @('m_eKillerDisguiseType = SCR_ECharacterDisguiseType.DEFAULT_FACTION;','','PERCEIVED_FACTION'),
    @('SetPerceivedFaction(m_iVictimPlayerID, victimEntity, source, instigator, false);','','PERCEIVED_FACTION'),
    @('SCR_ECharacterDeathStatusRelations.OTHER_DEATH','SCR_ECharacterDeathStatusRelations.SUICIDE','ENVIRONMENT_RELATION'),
    @('m_eVictimKillerRelation = SCR_ECharacterDeathStatusRelations.OTHER_DEATH;','instigator.SetInstigator(null);','CONTEXT_ONLY')
)
foreach($mutation in $mutations){
    if(!$source.Contains($mutation[0])){throw "Missing mutation $($mutation[2])"}
    if($mutation[2] -notin @(Test-InstigatorContracts ($source.Replace($mutation[0],$mutation[1])))){$failures+="MUTATION_ESCAPED $($mutation[2])"}
}
if($failures.Count){$failures;exit 1}
"Instigator context static: PASS; negative mutations=$($mutations.Count)"

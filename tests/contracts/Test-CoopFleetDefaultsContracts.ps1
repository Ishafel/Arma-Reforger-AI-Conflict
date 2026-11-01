param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$core=Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict'
$source=[IO.File]::ReadAllText((Join-Path $core 'Vehicles/AICF_CoopFleetDefaults.c'))
$body=Get-AICFMethodBody ([pscustomobject]@{Source=$source;Code=(ConvertTo-AICFCodeText $source)}) 'Role'
$checks=0
function Require([bool]$Value,[string]$Name){if(!$Value){throw "FAIL $Name"};$script:checks++}
$adapter=@"
public enum AICF_ECoopVehicleRole { FOOT, TRANSPORT, ARMED_LIGHT, APC, TANK }
public static class AICF_Stage1Config { public const int GROUP_SLOTS_PER_FACTION=10; }
public static class AICF_CoopFleetPolicyAdapter {
 public static AICF_ECoopVehicleRole Role(int slotId,bool tanks) { $body }
}
"@
$rules=[IO.File]::ReadAllText((Join-Path $core 'Config/AICF_MatchRules.c'))
$methods=@()
foreach($name in @('GetMode','IsValid','GetPlayerOnlyFaction','GetAICommanderMode','GetManagedSlotCount','UsesTicketDefeat','AllowsAutomaticConstruction','AllowsPlayerFaction')){
    $record=[pscustomobject]@{Source=$rules;Code=(ConvertTo-AICFCodeText $rules)}
    $method=Get-AICFMethodBody $record $name
    $signature='public bool '+$name+'(string factionKey)'
    if($name -eq 'GetMode'){$signature='public int GetMode()'}
    elseif($name -eq 'IsValid'){$signature='public bool IsValid()'}
    elseif($name -in @('GetPlayerOnlyFaction','GetAICommanderMode')){$signature='public string '+$name+'()'}
    elseif($name -eq 'GetManagedSlotCount'){$signature='public int GetManagedSlotCount(string factionKey)'}
    $methods+=($signature+' '+$method)
}
$rulesAdapter=@"
public static class AICF_EMatchMode { public const int STANDARD=0, COOP_US=1, COOP_USSR=2; }
public class AICF_ContentProfile {
 public static AICF_ContentProfile GetActive() { return new AICF_ContentProfile(); }
 public string GetStableFactionKey(string key) { if(key=="RHS_USAF") return "US"; if(key=="RHS_AFRF") return "USSR"; return key; }
}
public class AICF_CoopRulesAdapter {
 private int m_eMode; public AICF_CoopRulesAdapter(int mode) { m_eMode=mode; }
 $($methods -join "`n")
}
"@
Add-Type -TypeDefinition ($adapter+$rulesAdapter)
foreach($mode in 0..2){
    $policy=New-Object AICF_CoopRulesAdapter($mode)
    $commander=@('BOTH','USSR','US')[$mode]
    Require ($policy.GetAICommanderMode() -ceq $commander) "ENEMY_COMMANDER_$mode"
    foreach($side in @('US','USSR','RHS_USAF','RHS_AFRF')){
        $stable=$side; if($side -eq 'RHS_USAF'){$stable='US'};if($side -eq 'RHS_AFRF'){$stable='USSR'}
        $human=($mode -eq 1 -and $stable -eq 'US') -or ($mode -eq 2 -and $stable -eq 'USSR')
        $slots=10; if($human){$slots=0}
        Require ($policy.GetManagedSlotCount($side) -eq $slots) "MANAGED_ROSTER_${mode}_$side"
        Require ($policy.UsesTicketDefeat($side) -eq !$human) "TICKET_DEFEAT_${mode}_$side"
        Require ($policy.AllowsAutomaticConstruction($side) -eq !$human) "HUMAN_CONSTRUCTION_${mode}_$side"
        Require ($policy.AllowsPlayerFaction($side) -eq ($mode -eq 0 -or $human)) "PLAYER_ACCESS_${mode}_$side"
    }
    Require ($policy.GetManagedSlotCount('FIA') -eq 0) "NO_MANAGED_FIA_$mode"
}
foreach($tanks in @($false,$true)){
    $expected=@('TRANSPORT','TRANSPORT','TRANSPORT','TRANSPORT','ARMED_LIGHT','APC','APC','FOOT','FOOT','FOOT')
    if($tanks){$expected[7]='TANK';$expected[8]='TANK'}
    foreach($side in @('US','USSR')){
        foreach($index in 0..9){Require ([AICF_CoopFleetPolicyAdapter]::Role($index,$tanks).ToString() -ceq $expected[$index]) "ROLE_${side}_${tanks}_$index"}
        foreach($index in @(-1,10,2147483647)){Require ([AICF_CoopFleetPolicyAdapter]::Role($index,$tanks).ToString() -ceq 'FOOT') "INVALID_${side}_${tanks}_$index"}
    }
}
Require ($source -match '!Replication.IsServer\(\)' -and $source -match 'rules.GetMode\(\) == AICF_EMatchMode.STANDARD' -and $source -match '!rules.IsValid\(\)') 'AUTHORITY_COOP_VALID_MODE_ONLY'
$apply=Get-AICFMethodBody ([pscustomobject]@{Source=$source;Code=(ConvertTo-AICFCodeText $source)}) 'Apply'
Require ($apply.Contains('rules.GetManagedSlotCount(state.GetFactionKey()) <= 0') -and $apply.IndexOf('rules.GetManagedSlotCount(') -lt $apply.IndexOf('new AICF_VehiclePresetCatalog')) 'NO_HUMAN_DEFAULT_OR_MODEL_INSPECTION'
Require ($source -match 'slot.GetState\(\) != AICF_EGroupSlotState.EMPTY' -and $source -match 'slot.HasVehiclePreset\(\)' -and $source -match 'slot.GetVehiclePresetRevision\(\) != 0') 'INITIAL_ONLY_PRESERVES_MANUAL_RESET_AND_BINDING'
Require ($source -match 'AICF_VehicleSeatAssignment.AutoAssign' -and $source -match 'profile.IsVehiclePresetCrewSeat\(prefab, stableKey, seat\)' -and $source -match 'size = Math.Min') 'LIVE_TOPOLOGY_AND_MANDATORY_FIRST'
Require ($source -match 'TRANSPORT_TOO_SMALL' -and $source -match 'role != AICF_ECoopVehicleRole.TRANSPORT.*?recipe.m_iBehavior = AICF_EVehicleBehavior.COMBAT' -and $source -match 'catalog.Clear\(\)') 'FULL_TRANSPORT_COMBAT_CREW_AND_WORLD_RELEASE'
Require ($source -notmatch 'SpawnEntity|AddSupplies|TryPurchase|SetWorldTransform|SetHealth|SetOrigin|CallLater') 'INTENT_ONLY_NO_FREE_ASSET_OR_RECOVERY'
$catalog=[IO.File]::ReadAllText((Join-Path $core 'Vehicles/AICF_VehiclePresetCatalog.c'))
$own=Get-AICFMethodBody ([pscustomobject]@{Source=$catalog;Code=(ConvertTo-AICFCodeText $catalog)}) 'OwnCandidates'
Require ($own -match 'AICF_EVehicleCatalogSource.FACTION' -and $own -match 'matches.Sort\(\)' -and $own -match '!candidates.Contains') 'OWN_FACTION_DETERMINISTIC_DEDUPLICATION'
$profile=[IO.File]::ReadAllText((Join-Path $core 'Content/AICF_ContentProfile.c'))
$tracked=Get-AICFMethodBody ([pscustomobject]@{Source=$profile;Code=(ConvertTo-AICFCodeText $profile)}) 'AllowsTrackedVehiclePreset'
Require ($tracked -match '^\{\s*return false;\s*\}$') 'STOCK_TRACKED_DEFAULT_DENY'
$wcs=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'AIConflictArlandWCSRHS/Scripts/Game/AIConflictArlandWCSRHS/AICF_WCSRHSContentProfile.c'))
$tank=Get-AICFMethodBody ([pscustomobject]@{Source=$wcs;Code=(ConvertTo-AICFCodeText $wcs)}) 'AllowsTrackedVehiclePreset'
Require ($tank -match '(?s)stableKey == "US".*?M1A1' -and $tank -match '(?s)stableKey == "USSR".*?T72' -and $tank -match 'return false;') 'WCS_FACTION_TANK_ALLOWLIST'
"Co-op fleet defaults: PASS checks=$checks; actual production Role matrix; runtime purchase/boarding NOT RUN"

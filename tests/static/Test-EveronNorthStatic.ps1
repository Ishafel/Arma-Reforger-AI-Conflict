param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
$source=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'AIConflictEveron/Scripts/Game/AIConflictEveron/Integration/AICF_EveronRuntimeScenario.c'))
$expected=@('MilitaryBaseAirfield','MilitaryHospital','MainBaseNorth','SmallBaseMaidensBay','TownBaseMeaux','TownBaseTyrone','SmallBaseHornbeamValley')
$rows=@([regex]::Matches($source,'new AICF_EveronRuntimeBase\("([^"]+)", (true|false)\)'))
if($rows.Count -ne 7 -or ((@($rows|ForEach-Object {$_.Groups[1].Value})|Sort-Object)-join ',') -cne (($expected|Sort-Object)-join ',')){throw 'North base identities'}
$hqs=@($rows|Where-Object {$_.Groups[2].Value -ceq 'true'}|ForEach-Object {$_.Groups[1].Value})
if(($hqs -join ',') -cne 'MilitaryBaseAirfield,MilitaryHospital' -or @($rows|Where-Object {$_.Groups[2].Value -ceq 'false'}).Count -ne 5){throw 'North HQ/control policy'}
foreach($rule in @('header.m_bCustomBaseWhitelist = true;','header.m_bEstablishingBasesEnabled = false;','m_fRadioRange = 2000;','m_bDisableWhenUnusedAsHQ = true;')){if(!$source.Contains($rule)){throw "North policy: $rule"}}
if($source -match 'SpawnEntity|DeleteEntity|SetFaction|CallLater'){throw 'North topology boundary'}
. (Join-Path $RepositoryRoot 'tools/AICF_Scenarios.ps1')
$catalog=Get-AICFScenarioCatalog
foreach($variant in @('EveronNorth','EveronNorthRHS','EveronNorthWCSRHS')){
 $plan=Resolve-AICFScenario -Catalog $catalog -Variant $variant
 if($plan.definition.mapIndex -ne 2 -or $plan.missionHeader -notmatch 'Missions/(23_Campaign|RHS_Conflict)\.conf$'){throw 'Native North target'}
}
'North static: PASS; runtime whitelist=7 HQ=2 control=5; stock/RHS/WCS use native Everon'

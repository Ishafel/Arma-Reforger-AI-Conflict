[CmdletBinding()]
param([string]$RepositoryRoot,[switch]$Check)
$ErrorActionPreference='Stop'
if(!$RepositoryRoot){$RepositoryRoot=Split-Path -Parent $PSScriptRoot}
. (Join-Path $PSScriptRoot 'AICF_Scenarios.ps1')
$catalog=Get-AICFScenarioCatalog
$changes=[Collections.Generic.List[string]]::new()
$newline="`r`n"
function Write-AICFGenerated([string]$Path,[string]$Text) {
    $existing=''; if(Test-Path -LiteralPath $Path){$existing=[IO.File]::ReadAllText($Path)}
    if($existing.Replace("`r`n","`n") -ceq $Text.Replace("`r`n","`n")){return}
    $changes.Add($Path)
    if(!$Check){[IO.File]::WriteAllText($Path,$Text,[Text.UTF8Encoding]::new($false))}
}
$items=[Collections.Generic.List[string]]::new()
function Add-AICFGeneratedString([string]$Guid,[string]$Id,[string]$English,[string]$Russian) {
    $items.Add((@("`t`tCustomStringTableItem `"{$Guid}`"","`t`t{","`t`t`tId `"$Id`"","`t`t`tTarget_en_us `"$English`"","`t`t`tTarget_ru_ru `"$Russian`"","`t`t}") -join $newline)+$newline)
}
Add-AICFGeneratedString 'A1CF261018470001' 'AICF_Scenario_Coop_Description' 'Human players on one side against the opposing AI commander and FIA garrisons.' 'Игроки на одной стороне против ИИ-командира другой стороны и гарнизонов FIA.'
Add-AICFGeneratedString 'A1CF261018470002' 'AICF_Scenario_Coop_Details' 'Your side has no managed AI combat squads or automatic construction. Builders, requested supply deliveries and barracks orders remain available. Your side loses by territory; the opposing AI can also lose its tickets. Only your side is available to players.' 'У стороны игроков нет управляемых модом боевых отрядов ИИ и автоматического строительства. Строители, заказанная доставка припасов и заказы в казарме доступны. Игроки проигрывают по территории; противник также может проиграть по билетам. Для игроков доступна только выбранная сторона.'
$count=0
$headerCalls=[Collections.Generic.List[string]]::new()
foreach($variant in $catalog.variants) {
    foreach($side in @('US','USSR')) {
        foreach($difficulty in @('Easy','Medium','Hard')) {
            $mode='Coop'+$side
            $resolved=Resolve-AICFScenario -Catalog $catalog -Variant $variant.name -MatchMode $mode -Difficulty $difficulty
            $index=$resolved.difficultyIndex
            $addon=Split-Path -Parent $resolved.project
            $name='AICF_Scenario_'+$variant.name+'Coop'+$side
            $difficultySuffix=''; if($index -gt 0){$difficultySuffix="_$difficulty"}
            $legacy=$variant.name -eq 'Stock' -and $side -eq 'USSR'
            if($legacy){$name='AICF_Scenario_ArlandCoopUSSR'}
            $name+=$difficultySuffix+'_Name'
            $parent=$variant.standard[$index]
            $headerCalls.Add("`t`tAICF_HeaderCase(`"$($resolved.scenario)`", `"$parent`", `"$($variant.name)`", `"$side`", $index);")
            $lines=@("SCR_MissionHeaderCampaign : `"$parent`"",'{',"`tm_sName `"#$name`"")
            if($legacy) {
                if($index -eq 0){$lines+=@("`tm_sDescription `"#AICF_Scenario_ArlandCoopUSSR_Description`"","`tm_sDetails `"#AICF_Scenario_ArlandCoopUSSR_Details`"","`tm_eAICFMatchMode 2")}
                else {
                    $lines[0]='SCR_MissionHeaderCampaign : "{A1CF261018460001}Missions/AICF_Conflict_Arland_Coop_USSR.conf"'
                    $lines+=@("`tm_eAICFDifficulty $index","`tm_bAICFFIATank 0","`tm_bAICFFIAPassengers 0")
                }
            } else {
                $lines+=@("`tm_sDescription `"#AICF_Scenario_Coop_Description`"","`tm_sDetails `"#AICF_Scenario_Coop_Details`"","`tm_eAICFMatchMode $($resolved.mode)","`tm_eAICFDifficulty $index")
                $sideId=1; if($side -eq 'USSR'){$sideId=2}
                $titleEn='AI Conflict - '+$variant.titleEn+' Co-op '+$variant.sideEn.$side
                $titleRu='AI Conflict — '+$variant.titleRu+' Co-op '+$variant.sideRu.$side
                if($index -gt 0){$titleEn+=' ('+$difficulty+')'; $titleRu+=' ('+@('лёгкий','средний','сложный')[$index]+')'}
                Add-AICFGeneratedString ($variant.stringGuidPrefix+$sideId+($index+1)) $name $titleEn $titleRu
            }
            $lines+='}'
            $path=Join-Path (Join-Path $RepositoryRoot $addon) $resolved.missionHeader
            Write-AICFGenerated $path (($lines -join $newline)+$newline)
            $meta=@('MetaFileClass','{',"`tName `"$($resolved.scenario)`"","`tConfigurations","`t{","`t`tCONFResourceClass PC {}","`t`tCONFResourceClass HEADLESS : PC {}","`t`tCONFResourceClass XBOX_ONE : PC {}","`t`tCONFResourceClass XBOX_SERIES : PC {}","`t`tCONFResourceClass PS4 : PC {}","`t}",'}')
            Write-AICFGenerated ($path+'.meta') (($meta -join $newline)+$newline)
            $count++
        }
    }
}
$languagePath=Join-Path $RepositoryRoot 'AIConflictCore/Language/AICF_Localization.st'
$language=[IO.File]::ReadAllText($languagePath)
$language=[regex]::Replace($language,'(?ms)^\t\tCustomStringTableItem "\{A1CF26101847[A-F0-9]{4}\}"\r?\n\t\t\{.*?^\t\t\}\r?\n','')
$insert=[regex]::Match($language,'Items\s*\{\r?\n')
if(!$insert.Success){throw 'StringTable Items missing'}
$language=$language.Insert($insert.Index+$insert.Length,($items -join ''))
Write-AICFGenerated $languagePath $language
$probePath=Join-Path $RepositoryRoot 'tests/fixtures/AICF_CoopHeadersProbe.c'
$probe=[IO.File]::ReadAllText($probePath)
$pattern='(?s)\t\t// AICF HEADER MATRIX BEGIN\r?\n.*?\t\t// AICF HEADER MATRIX END'
if(![regex]::IsMatch($probe,$pattern)){throw 'Header probe generation markers missing'}
$probe=[regex]::Replace($probe,$pattern,("`t`t// AICF HEADER MATRIX BEGIN"+$newline+($headerCalls -join $newline)+$newline+"`t`t// AICF HEADER MATRIX END"))
Write-AICFGenerated $probePath $probe
if($Check -and $changes.Count){$changes; throw 'Generated Co-op scenarios are stale: run tools/Update-AICFCoopScenarios.ps1'}
& (Join-Path $PSScriptRoot 'Build-AICFLocalization.ps1') -RepositoryRoot $RepositoryRoot -Check:$Check
"Co-op scenarios: $count headers; changed=$($changes.Count); check=$Check"

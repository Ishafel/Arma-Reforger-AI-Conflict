param([string]$RepositoryRoot = (Split-Path -Parent $PSScriptRoot))
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Stage3StaticAudit.Common.ps1')
$records = @(Get-AICFSourceRecords $RepositoryRoot)
$data = Find-AICFClassRecord $records 'AICF_SupplyMapData'
$entry = Find-AICFClassRecord $records 'AICF_SupplyMapBase'
$view = Find-AICFClassRecord $records 'AICF_SupplyMapUI'
$inputGate = Find-AICFClassRecord $records 'SCR_MapCursorModule'
$controller = Find-AICFClassRecord $records 'AICF_StrategicUIController'
$failures = [System.Collections.Generic.List[string]]::new()
foreach ($record in @($data, $entry, $view, $inputGate, $controller)) {
    if (-not $record) { throw 'Missing unique supply UI component' }
}
function Method($Record, [string]$Name) {
    (ConvertTo-AICFCodeText (Get-AICFMethodBody $Record $Name)) -replace '\s+', ' '
}
function Require([string]$Rule, [string]$Code, [string]$Pattern) {
    Assert-AICFContains $failures $Rule $Code $Pattern 'Supply map UI contract'
}
$collect = Method $data 'Collect'
$read = Method $data 'ReadSupplies'
$owned = Method $entry 'IsOwnedBy'
$refresh = Method $view 'Refresh'
$amount = Method $view 'RefreshAmount'
$detach = Method $view 'Detach'
$close = Method $view 'Close'
$open = Method $view 'Open'
$cursorRegistrationPattern = '\[BaseContainerProps\(\)\]\s*modded\s+class\s+SCR_MapCursorModule\b'
Require 'SUPPLY_CURSOR_CONFIG_REGISTRATION' $inputGate.Code $cursorRegistrationPattern
$comboResourcePattern = 'COMBO_LAYOUT\s*=\s*"\{4B5AE6E64037FFB4\}UI/layouts/WidgetLibrary/ComboBox/WLib_ComboBox.layout"'
$sliderResourcePattern = 'SLIDER_LAYOUT\s*=\s*"\{4A41296C0E9A889F\}UI/layouts/WidgetLibrary/WLib_Slider.layout"'
Require 'SUPPLY_COMBO_RESOURCE_IDENTITY' $view.Source $comboResourcePattern
Require 'SUPPLY_SLIDER_RESOURCE_IDENTITY' $view.Source $sliderResourcePattern
Require 'SUPPLY_DISCOVERY' $collect 'SCR_MilitaryBaseSystem.GetInstance\(\)[\s\S]*system.GetBases\(bases\)'
Require 'SUPPLY_OWNERSHIP' $collect '!base.IsInitialized\(\) \|\| base.GetFaction\(\) != faction'
Require 'SUPPLY_IDENTITY' $owned 'GetOwner\(\).GetID\(\) == m_EntityId[\s\S]*GetFaction\(\) == faction'
Require 'SUPPLY_ORDERING' $collect 'm_sSortKey.Compare\([\s\S]*SwapItems'
Require 'SUPPLY_STOCK_RESOURCE' $read 'entry.IsOwnedBy\(faction\)[\s\S]*GetResourceConsumer\(\)[\s\S]*entry.m_Base.GetSupplies\(\)'
Require 'SUPPLY_SYNC_SENTINEL' $read 'supplies < 0 \|\| supplies != supplies[\s\S]*return false'
Require 'SUPPLY_INTEGER_STOCK' $read 'Math.Floor\(supplies\)'
Assert-AICFNotContains $failures 'SUPPLY_READ_ONLY' ($data.Code + $entry.Code + $view.Code) `
    'Replication.BumpMe|\bRpc\(|AddSupplies\(|SpawnEntity|DeleteRplEntity|\.Transfer\(|\.Reserve\(' `
    'The form cannot mutate gameplay resources; transport intent goes through the player controller facade'
Require 'SUPPLY_NATIVE_WIDGETS' (Method $view 'CreatePanel') 'SCR_ComboBoxComponent.Cast[\s\S]*SCR_SliderComponent.Cast'
$nativeLabel = Method $view 'HideNativeLabel'
$nativeLabelPattern = 'component.GetLabelWidget\(\)[\s\S]*label.SetVisible\(false\)'
Require 'SUPPLY_NATIVE_LABEL_LIFETIME' $nativeLabel $nativeLabelPattern
Require 'SUPPLY_NATIVE_LABEL_CONTROLS' (Method $view 'CreatePanel') 'HideNativeLabel\(m_Combo\)[\s\S]*HideNativeLabel\(m_Slider\)'
Assert-AICFNotContains $failures 'SUPPLY_NO_NATIVE_LABEL_DELETE' ($view.Code) '\.UseLabel\(false\)|label.RemoveFromHierarchy\(' 'Stock AutomaticScroll retains the label animation component after HandlerAttached'
Require 'SUPPLY_DYNAMIC_FACTION' $refresh 'SGetLocalPlayerFaction\(\)[\s\S]*faction != m_Faction[\s\S]*m_Selected = null; m_iAmount = 0;'
Require 'SUPPLY_RECONCILE' $refresh 'AICF_SupplyMapData.Collect[\s\S]*m_EntityId != m_aBases\[index\].m_EntityId[\s\S]*RebuildList\(current\)'
Require 'SUPPLY_NO_POPUP_CHURN' $refresh 'if \(changed \|\| m_Combo.GetNumItems\(\) == 0 \|\| \(!m_Selected && !current.IsEmpty\(\)\)\) RebuildList'
$rebuild = Method $view 'RebuildList'
Require 'SUPPLY_POPUP_IDENTITY' $rebuild 'm_Combo.CloseList\(\); m_Combo.ClearAll\(\)[\s\S]*entry.m_Base == m_Selected.m_Base && entry.m_EntityId == m_Selected.m_EntityId'
Require 'SUPPLY_EMPTY_BASES' $rebuild 'm_Combo.SetEnabled\(!m_aBases.IsEmpty\(\)\)'
Require 'SUPPLY_CLAMP' $amount 'Math.ClampInt\(m_iAmount, 0, m_iMaximum\)'
Require 'SUPPLY_RANGE' $amount 'SetEnabled\(ready && m_iMaximum > 0\)[\s\S]*SetSliderSettings\(0, m_iMaximum, 1'
Require 'SUPPLY_REENTRANCY' $amount 'm_bRendering = true[\s\S]*SetValue\(m_iAmount\)[\s\S]*m_bRendering = false'
Require 'SUPPLY_SELECTION_RECHECK' (Method $view 'OnBaseChanged') 'IsOwnedBy\(SCR_FactionManager.SGetLocalPlayerFaction\(\)\)[\s\S]*Refresh\(\)'
Require 'SUPPLY_DRAG_RECHECK' (Method $view 'OnAmountChanged') 'm_bRendering \|\| !m_bOpen[\s\S]*Refresh\(\)'
Require 'SUPPLY_TICK_BEFORE_FACTION' (Method $controller 'Update') 'm_SupplyMapUI.Refresh\(\)[\s\S]*SGetLocalPlayerFaction\(\)'
Require 'SUPPLY_FULLSCREEN' (Method $controller 'OnMapOpen') 'config.MapEntityMode == EMapEntityMode.FULLSCREEN[\s\S]*m_SupplyMapUI.Attach\(mapRoot\)'
Require 'SUPPLY_COMMAND_EXCLUSION' (Method $controller 'SetCommandOpen') 'if \(open && m_SupplyMapUI\) m_SupplyMapUI.Close\(\)'
Require 'SUPPLY_CURSOR_CANCEL' (Method $controller 'CloseCommandForSupplies') 'CancelMapPointSelection\(false\); SetCommandOpen\(false\)'
Require 'SUPPLY_MAP_CLEANUP' (Method $controller 'RemoveMapUI') 'm_SupplyMapUI.Detach\(\)'
Require 'SUPPLY_MODAL_ACQUIRE' $open 'CS_DIALOG[\s\S]*CloseCommandForSupplies\(\)[\s\S]*m_bInputCaptured = true; m_Cursor.AICF_SetSupplyDialog\(this\)'
Require 'SUPPLY_NATIVE_BUTTON_INPUT' (Get-AICFMethodBody $view 'BindRect') 'FindAnyWidget\("AICF_RectInput"\)[\s\S]*input.AddHandler\(this\)[\s\S]*input.RemoveHandler\(this\)'
Require 'SUPPLY_CLOSE_INPUT_FRAME' $close 'm_Combo.CloseList\(\)[\s\S]*SetFocusedWidget\(null\)[\s\S]*CallLater\(ReleaseInput, 0, false\)'
Require 'SUPPLY_RELEASE_IDENTITY' (Method $inputGate 'AICF_ClearSupplyDialog') 'm_AICFSupplyDialog != dialog\) return;[\s\S]*HandleDialog\(false\)'
Require 'SUPPLY_RELEASE_CALLBACK' (Method $view 'ReleaseInput') 'Remove\(ReleaseInput\)[\s\S]*AICF_ClearSupplyDialog\(this\)'
Require 'SUPPLY_INPUT_LIFETIME' $detach 'Close\(\); ReleaseInput\(\)'
foreach ($pair in @(@('m_Combo', 'OnBaseChanged'), @('m_Slider', 'OnAmountChanged'))) {
    Require 'SUPPLY_UNSUBSCRIBE' $detach ([regex]::Escape("$($pair[0]).m_OnChanged.Remove($($pair[1]))"))
}
Require 'SUPPLY_BACK_UNSUBSCRIBE' $close 'RemoveActionListener\(UIConstants.MENU_ACTION_BACK, EActionTrigger.DOWN, OnBack\)'
foreach ($method in @('HandleSelect', 'HandleMultiSelect', 'HandleDrag', 'HandleRotateTool',
        'OnInputZoomIn', 'OnInputZoomOut', 'OnInputZoomWheelUp', 'OnInputZoomWheelDown', 'OnInputModifClick')) {
    Require 'SUPPLY_INPUT_ISOLATION' (Method $inputGate $method) ('if \(!AICF_HasSupplyDialog\(\)\) super\.' + $method)
}
Require 'SUPPLY_RADIAL_ISOLATION' (Method $inputGate 'HandleContextualMenu') 'AICF_HasSupplyDialog\(\) && !doClose\) return false'

# Negative inputs prove that central invariants fail if guards are removed.
$negativeCases = @(
    @{ Code = $owned.Replace('GetOwner().GetID() == m_EntityId', 'true'); Pattern = 'GetOwner\(\).GetID\(\) == m_EntityId' },
    @{ Code = $collect.Replace('base.GetFaction() != faction', 'false'); Pattern = 'base.GetFaction\(\) != faction' },
    @{ Code = $amount.Replace('Math.ClampInt(m_iAmount, 0, m_iMaximum)', 'm_iAmount'); Pattern = 'Math.ClampInt\(m_iAmount, 0, m_iMaximum\)' },
    @{ Code = $detach.Replace('m_Combo.m_OnChanged.Remove(OnBaseChanged)', ''); Pattern = 'm_Combo.m_OnChanged.Remove\(OnBaseChanged\)' },
    @{ Code = $inputGate.Code.Replace('[BaseContainerProps()]', ''); Pattern = $cursorRegistrationPattern },
    @{ Code = $nativeLabel.Replace('label.SetVisible(false)', 'label.RemoveFromHierarchy()'); Pattern = $nativeLabelPattern },
    @{ Code = $view.Source.Replace('{4B5AE6E64037FFB4}', ''); Pattern = $comboResourcePattern },
    @{ Code = $view.Source.Replace('{4A41296C0E9A889F}', ''); Pattern = $sliderResourcePattern }
)
foreach ($case in $negativeCases) {
    if ($case.Code -match $case.Pattern) { $failures.Add('[SUPPLY_NEGATIVE_FIXTURE] Mutation was not rejected') }
}
if ($failures.Count) { $failures; exit 1 }
Write-Output 'PASS Supply map UI: stock data, ownership, identity, clamp, input isolation, cleanup, cursor config registration, native label lifetime, resource GUIDs; 8 negative inputs. Runtime NOT RUN.'
exit 0

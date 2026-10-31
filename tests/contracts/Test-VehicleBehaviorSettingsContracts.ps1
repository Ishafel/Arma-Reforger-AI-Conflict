param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$root=Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict'
$model=[IO.File]::ReadAllText((Join-Path $root 'Vehicles/AICF_VehiclePreset.c'))
$editor=[IO.File]::ReadAllText((Join-Path $root 'UI/AICF_VehiclePresetEditor.c'))
$service=[IO.File]::ReadAllText((Join-Path $root 'UI/AICF_VehiclePresetService.c'))
$fixture=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'tests/fixtures/AICF_VehicleBehaviorSettingsProbe.c'))
function Body([string]$source,[string]$name){Get-AICFMethodBody ([pscustomobject]@{Source=$source;Code=(ConvertTo-AICFCodeText $source)}) $name}
function Require([bool]$condition,[string]$rule){if(!$condition){throw "FAIL $rule"};$script:checks++}
$checks=0
$valid=Body $model 'ValidBehavior'
$minimum=[int][regex]::Match($model,'MIN_RETURN_DISTANCE\s*=\s*(\d+)').Groups[1].Value
$maximum=[int][regex]::Match($model,'MAX_RETURN_DISTANCE\s*=\s*(\d+)').Groups[1].Value
# Выполняем извлечённый production predicate, включая обе стороны диапазона.
$expression=$valid.Trim().TrimStart('{').TrimEnd('}').Trim() -replace '^return\s+','' -replace ';\s*$',''
$expression=$expression.Replace('AICF_EVehicleBehavior.TRANSPORT','0').Replace('AICF_EVehicleBehavior.COMBAT','1').Replace('MIN_RETURN_DISTANCE',$minimum).Replace('MAX_RETURN_DISTANCE',$maximum)
$expression=$expression -replace '\bbehavior\b','$behavior' -replace '\breturnDistance\b','$distance'
$expression=$expression.Replace('==','-eq').Replace('>=','-ge').Replace('<=','-le').Replace('&&','-and').Replace('||','-or')
$evaluate=[scriptblock]::Create('param($behavior,$distance) '+$expression)
foreach($case in @(@(0,300,$true),@(1,437,$true),@(0,50,$true),@(1,1000,$true),@(-1,300,$false),@(2,300,$false),@(0,49,$false),@(1,1001,$false),@(0,0,$false),@(1,-10,$false))){
 Require ([bool](& $evaluate $case[0] $case[1]) -eq $case[2]) "BEHAVIOR_AND_DISTANCE_$($case[0])_$($case[1])"
}
Require ($minimum -eq 50 -and $maximum -eq 1000 -and $model -match 'm_iReturnDistance = 300') 'EXPLICIT_USER_DEFAULT_AND_BOUNDS'
$decode=Body $model 'Decode'
Require ($decode -match 'schema != 1 && schema != 2' -and $decode -match 'schema == 2 && \(!json.ReadValue\("behavior", preset.m_iBehavior\) \|\|' -and $decode -match '!json.ReadValue\("returnDistance", preset.m_iReturnDistance\)' -and $decode -match '!preset.ValidateShape\(reason\)') 'LEGACY_DEFAULTS_NEW_SCHEMA_REQUIRES_FIELDS_AND_VALIDATION'
$encode=Body $model 'Encode'
Require ($encode -match 'WriteValue\("schemaVersion", 2\)' -and $encode -match 'WriteValue\("behavior", m_iBehavior\)' -and $encode -match 'WriteValue\("returnDistance", m_iReturnDistance\)') 'PERSISTED_BEHAVIOR_AND_DISTANCE'
$same=Body $model 'SameAssignment'
Require ($same -match 'm_iBehavior != other.m_iBehavior' -and $same -match 'm_iReturnDistance != other.m_iReturnDistance') 'CONFIGURATION_CHANGE_INVALIDATES_ASSIGNMENT'
Require ((Body $model 'ValidateShape') -match '!ValidBehavior\(m_iBehavior, m_iReturnDistance\)' -and $service -match 'AICF_VehiclePreset.Decode\(candidateData\)') 'SERVER_REQUEST_REVALIDATES_BEHAVIOR'
$read=Body $editor 'ReadBehavior'
Require ($read -match 'returnDistance.ToString\(\) != value' -and $read -match '!AICF_VehiclePreset.ValidBehavior\(behavior, returnDistance\)') 'UI_REJECTS_NONCANONICAL_AND_OUT_OF_RANGE_VALUES'
$click=Body $editor 'OnClick'
Require ($click -match '!ReadBehavior\(\)' -and (Body $editor 'SendSelection') -match '!ReadBehavior\(\)' -and (Body $editor 'SendSelection') -match 'm_bPreserveBehaviorOnSelect = true') 'SAVE_APPLY_AND_MODEL_SELECTION_READ_CURRENT_INPUT'
$refresh=Body $editor 'Refresh'
Require ($refresh -match 'm_bPreserveBehaviorOnSelect && m_Player.AICF_VehiclePresetAccepted\(\)' -and $refresh -match 'm_Draft.m_iBehavior = m_iSelectedBehavior' -and $refresh -match 'm_Draft.m_iReturnDistance = m_iSelectedReturnDistance') 'SELECT_PRESERVES_UNSAVED_BEHAVIOR_AFTER_ACCEPTANCE'
Require ((Body $editor 'RenderSeats') -notmatch 'm_wReturnDistance.SetText|RenderBehavior\(') 'PERIODIC_REFRESH_DOES_NOT_OVERWRITE_TYPED_DISTANCE'
Require ((Body $editor 'Close') -match 'ClearCombo\(m_Behavior\)' -and (Body $editor 'Close') -match 'm_wReturnDistance = null') 'BEHAVIOR_WIDGET_LIFECYCLE'
foreach($name in @('LEGACY_TRANSPORT300','SCHEMA2_COMBAT_CUSTOM_DISTANCE','SCHEMA2_MISSING_BEHAVIOR_REJECTED','SCHEMA2_MISSING_DISTANCE_REJECTED','FUTURE_SCHEMA_REJECTED','NEGATIVE_BEHAVIOR_REJECTED','UNKNOWN_BEHAVIOR_REJECTED','DISTANCE49_REJECTED','DISTANCE1001_REJECTED','DISTANCE50_ACCEPTED','DISTANCE1000_ACCEPTED','MODE_CHANGE_IS_NEW_ASSIGNMENT','DISTANCE_CHANGE_IS_NEW_ASSIGNMENT','NAME_ONLY_ASSIGNMENT_IDEMPOTENT','MUTATED_OBJECT_VALIDATION_REJECTED','LIBRARY_SAVE_COMBAT','LIBRARY_REPEAT_SAVE_IDEMPOTENT','LIBRARY_FRESH_STORE_READBACK','LIBRARY_UPDATE_TRANSPORT','LIBRARY_UPDATE_PERSISTS_MODE','LIBRARY_INVALID_UPDATE_REJECTED','LIBRARY_DELETE','LIBRARY_TOMBSTONE_SURVIVES_RELOAD')){Require ($fixture.Contains('"'+$name+'"')) "NATIVE_CASE_$name"}
Write-Output "PASS Vehicle behavior settings contracts checks=$checks native_runtime=NOT_RUN"

param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
$source=Get-Content (Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Vehicles/AICF_VehicleNativeSubgroup.c') -Raw
$guard=[regex]::Match($source,'if \(assignment.GetTargetKind\(\) != slot.GetTargetKind\(\)[\s\S]*?return false;').Value
if(!$guard){throw 'Missing actual destination guard'}
$code=@'
public enum AICF_EOrderTargetKind {BASE,POSITION}
public class Destination {public AICF_EOrderTargetKind Kind;public object Base;public int Position;public AICF_EOrderTargetKind GetTargetKind(){return Kind;}public object GetTargetBase(){return Base;}public int GetTargetPosition(){return Position;}}
public class Actual {public static bool Check(Destination assignment,Destination slot){
'@ + $guard + 'return true;}}'
Add-Type -TypeDefinition $code
$cases=0
foreach($kind in @([AICF_EOrderTargetKind]::BASE,[AICF_EOrderTargetKind]::POSITION)){
 foreach($sameKind in @($false,$true)){foreach($sameBase in @($false,$true)){foreach($samePosition in @($false,$true)){
  $a=[Destination]::new();$s=[Destination]::new();$a.Kind=$kind;$s.Kind=$kind
  if(!$sameKind){if($kind -eq [AICF_EOrderTargetKind]::BASE){$s.Kind=[AICF_EOrderTargetKind]::POSITION}else{$s.Kind=[AICF_EOrderTargetKind]::BASE}}
  $a.Base=[object]::new();$s.Base=$a.Base;if(!$sameBase){$s.Base=[object]::new()}
  $a.Position=10;$s.Position=10;if(!$samePosition){$s.Position=20}
  $expected=$sameKind -and $sameBase -and ($kind -eq [AICF_EOrderTargetKind]::BASE -or $samePosition)
  if([Actual]::Check($a,$s) -ne $expected){throw "Actual guard mismatch kind=$kind same_kind=$sameKind same_base=$sameBase same_position=$samePosition"};$cases++
 }}}
}
"PASS actual subgroup destination guard $cases cases: BASE identity and exact POSITION"

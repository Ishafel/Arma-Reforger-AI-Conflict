param([string]$RepositoryRoot = (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
$openingUi=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/UI/AICF_StrategicUI.c'))
$openingLoadout=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/UI/AICF_LoadoutEditor.c'))
$openingVehicle=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/UI/AICF_VehiclePresetEditor.c'))
foreach($openingPair in @(@($openingLoadout,'INFANTRY'),@($openingVehicle,'VEHICLE'))){
 if($openingPair[0] -notmatch 'bool Open\(Widget' -or $openingPair[0] -notmatch 'return OpenRejected\("CURSOR_DIALOG_BUSY"\)' -or $openingPair[0] -notmatch 'return OpenRejected\("MAP_CLOSED"\)'){throw ('Missing explicit opening result '+$openingPair[1])}
 if(!$openingPair[0].Contains('GetGame().GetWorkspace().AddModal(m_wRoot,') -or !$openingPair[0].Contains('GetGame().GetWorkspace().RemoveModal(m_wRoot)')){throw ('Missing paired native modal ownership '+$openingPair[1])}
 if(!$openingPair[0].Contains('[AICF][PRESET_EDITOR_OPEN] editor='+$openingPair[1])){throw 'Missing opening diagnostic'}
}
function OpeningCase([string]$action){
 $openingMatch=[regex]::Match($openingUi,'case AICF_EStrategicUIButtonAction\.'+$action+':(?<body>[\s\S]*?)\bbreak;')
 if(!$openingMatch.Success){throw ('Missing actual case '+$action)}
 $openingMatch.Groups['body'].Value.Replace('m_LoadoutEditor &&','m_LoadoutEditor != null &&').Replace('m_VehiclePresetEditor &&','m_VehiclePresetEditor != null &&').Replace('&& m_wCommandScrim)','&& m_wCommandScrim != null)').Replace('&& m_wLoadoutShortcut &&','&& m_wLoadoutShortcut != null &&')
}
$openingInfantry=OpeningCase 'EDIT_LOADOUT'
$openingTransport=OpeningCase 'EDIT_VEHICLE'
function ReplayOpening([string]$infantry,[string]$transport,[string]$suffix){
 $openingNs='Opening'+[guid]::NewGuid().ToString('N')+$suffix
 $openingCode=@'
using System;
namespace __NS__ {
class Widget { public Widget Parent; public Widget GetParent(){return Parent;} }
static class SCR_Global {public static bool Admin; public static bool IsAdmin(){return Admin;}}
class Editor {
 public bool Result; public int Calls, Slot; public bool Opponent, PanelWasOpen; public Widget Parent; public Model Model;
 public bool Open(Widget parent,int slot,bool personal=false,bool opponent=false){Calls++;Parent=parent;Slot=slot;Opponent=opponent;PanelWasOpen=Model.CommandOpen;return Result;}
}
class Model {
 public Editor m_LoadoutEditor,m_VehiclePresetEditor;
 public Widget m_wCommandScrim,m_wLoadoutShortcut;
 public int m_iSelectedSlot=7, Closed;
 public bool CommandOpen=true;
 public void SetCommandOpen(bool value){CommandOpen=value;Closed++;}
 public void Infantry(){__INFANTRY__}
 public void Vehicle(){__VEHICLE__}
}
public static class Suite {
 static int checks;
 static void Require(bool value){checks++;if(!value)throw new Exception("OPENING_ASSERT");}
 static Model New(bool result){var m=new Model();m.m_wCommandScrim=new Widget{Parent=new Widget()};m.m_LoadoutEditor=new Editor{Model=m,Result=result};m.m_VehiclePresetEditor=new Editor{Model=m,Result=result};return m;}
 public static int Run(){
  foreach(bool success in new[]{false,true}) foreach(bool vehicle in new[]{false,true}){
   var m=New(success);if(vehicle)m.Vehicle();else m.Infantry();var e=vehicle?m.m_VehiclePresetEditor:m.m_LoadoutEditor;
   Require(e.Calls==1 && e.Slot==7 && e.Parent==m.m_wCommandScrim.Parent);
   Require(e.PanelWasOpen);Require(m.CommandOpen==!success);Require(m.Closed==(success?1:0));
  }
  var missing=New(true);missing.m_wCommandScrim=null;missing.Infantry();missing.Vehicle();Require(missing.CommandOpen && missing.Closed==0 && missing.m_LoadoutEditor.Calls==0 && missing.m_VehiclePresetEditor.Calls==0);
  SCR_Global.Admin=true;missing.m_wLoadoutShortcut=new Widget{Parent=new Widget()};missing.Infantry();Require(missing.m_LoadoutEditor.Calls==1 && missing.m_LoadoutEditor.Slot==0 && missing.m_LoadoutEditor.Opponent);
  return checks;
 }
}}
'@
 $openingCode=$openingCode.Replace('__NS__',$openingNs).Replace('__INFANTRY__',$infantry).Replace('__VEHICLE__',$transport)
 Add-Type -TypeDefinition $openingCode -Language CSharp
 $openingType=([System.Management.Automation.PSTypeName]($openingNs+'.Suite')).Type
 $openingType.GetMethod('Run').Invoke($null,@())
}
$openingChecks=ReplayOpening $openingInfantry $openingTransport 'Actual'
foreach($openingUnsafe in @('INFANTRY','VEHICLE')){
 $openingBadInfantry=$openingInfantry;$openingBadVehicle=$openingTransport
 if($openingUnsafe -eq 'INFANTRY'){$openingBadInfantry=$openingBadInfantry.Replace('if (m_LoadoutEditor.Open(m_wCommandScrim.GetParent(), m_iSelectedSlot))','m_LoadoutEditor.Open(m_wCommandScrim.GetParent(), m_iSelectedSlot);')}
 else{$openingBadVehicle=$openingBadVehicle.Replace('if (m_VehiclePresetEditor.Open(m_wCommandScrim.GetParent(), m_iSelectedSlot))','m_VehiclePresetEditor.Open(m_wCommandScrim.GetParent(), m_iSelectedSlot);')}
 try{[void](ReplayOpening $openingBadInfantry $openingBadVehicle $openingUnsafe)}catch{if($_.Exception.ToString().Contains('OPENING_ASSERT')){continue};throw}
 throw ('Unsafe closing-on-rejection accepted '+$openingUnsafe)
}
Write-Output ('PresetEditorOpening actual-cases PASS checks='+$openingChecks+' unsafe_rejected=2 native_visual=NOT_RUN')

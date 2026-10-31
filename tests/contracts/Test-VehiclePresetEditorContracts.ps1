param([string]$RepositoryRoot = (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$path = Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/UI/AICF_VehiclePresetEditor.c'
$source = Get-Content -LiteralPath $path -Raw
$record = [pscustomobject]@{Source=$source;Code=(ConvertTo-AICFCodeText $source)}
$methods = @{}
foreach ($name in @('Request','SendQueued','SendSelection','OnChanged','OnClick','OnMouseWheel')) {
 $body = Get-AICFMethodBody $record $name
 if (!$body) { throw "Missing actual editor method $name" }
 $methods[$name] = $body.Replace('ResourceName selected','string selected').Replace('System.GetTickCount()','Clock.Now').Replace('GetSliderPosPixels(scrollX, scrollY)','GetSliderPosPixels(out scrollX, out scrollY)')
}
# Native widget/layout boundary is source-bound; this is not rendered GUI proof.
$create = Get-AICFMethodBody $record 'Create'
$close = Get-AICFMethodBody $record 'Close'
$render = Get-AICFMethodBody $record 'RenderSeats'
foreach ($required in @('ScrollLayoutWidgetTypeID','WidgetFlags.CLIPCHILDREN','SizeLayoutWidgetTypeID','FrameWidgetTypeID','m_wSeatScroll.AddHandler(this)','ScrollLayoutSlot.SetVerticalAlign(m_wSeatSize, LayoutVerticalAlign.Top)')) {
 if (!$create.Contains($required)) { throw "Missing scroll boundary $required" }
}
$previewCreate = [regex]::Match($create,'CreateWidget\(WidgetType.RenderTargetWidgetTypeID,[\s\S]*?m_wPanel\)')
if (!$previewCreate.Success -or $previewCreate.Value.Contains('IGNORE_CURSOR')) { throw 'Interactive vehicle preview ignores cursor' }
foreach ($required in @('ClearCombo(oldCombo)','oldRow.RemoveFromHierarchy()','m_wSeatSize.SetHeightOverride','m_aSeats.Clear()','m_aSeatLabels.Clear()','m_aSeatRows.Clear()','FrameSlot.SetSizeToContent(row, false)','FrameSlot.SetPosY(row, seat * 58)','FrameSlot.SetSizeY(row, 52)','CreateText(row, 0, 0, 0.42, 1','Combo(0.43, 0, 0.97, 1, row)')) {
 if (!$render.Contains($required)) { throw "Missing seat rebuild lifecycle $required" }
}
if ($render -match 'FrameSlot.SetOffsets\(label|FrameSlot.SetOffsets\(combo.GetRootWidget') { throw 'Seat child geometry must use its fixed parent row' }
foreach ($required in @('ClearCombo(m_Catalog)','ClearCombo(m_SourceFilter)','ClearCombo(m_CategoryFilter)','m_wSeatScroll.RemoveHandler(this)','m_sQueuedSelection = ResourceName.Empty','m_wSeatScroll = null','m_Preview.Clear()')) {
 if (!$close.Contains($required)) { throw "Missing editor Close cleanup $required" }
}
$refresh = Get-AICFMethodBody $record 'Refresh'
if ($refresh.IndexOf('m_iGeneration = generation') -gt $refresh.LastIndexOf('SendSelection()')) { throw 'Queued selection dispatched before reply context observed' }
$shim = @'
using System;
using System.Collections.Generic;
namespace __NS__ {
public class Native { public static implicit operator bool(Native n){return n!=null;} }
public static class ResourceName {public const string Empty="";}
public static class Clock {public static int Now=2000;}
public static class Ext {public static bool IsEmpty(this string s){return string.IsNullOrEmpty(s);} }
public class Arr<T>:List<T>{public int Find(T x){return IndexOf(x);}public bool IsIndexValid(int i){return i>=0&&i<Count;}}
public class Widget:Native {public string Name="";public Widget Parent;public string Text="";public string GetName(){return Name;}public Widget GetParent(){return Parent;}public void SetText(string s){Text=s;}public string GetText(){return Text;}}
public class Scroll:Widget {public float X,Y;public int Sets;public void GetSliderPosPixels(out float x,out float y){x=X;y=Y;}public void SetSliderPosPixels(float x,float y){X=x;Y=y;Sets++;}}
public class SCR_ComboBoxComponent:Native {public int Item;public int GetCurrentIndex(){return Item;}}
public class AICF_VehiclePreset:Native {public int m_iBehavior,AssignedSeat=-1,AssignedMember=-1;public string m_sName;public void Assign(int s,int m){AssignedSeat=s;AssignedMember=m;}public string Encode(){return "draft";}}
public static class AICF_VehicleSeatAssignment {public static int Calls;public static bool AutoAssign(AICF_VehiclePreset p,Arr<int>a,Arr<int>b){Calls++;return true;}}
public static class AICF_Localization {public static string Resolve(string s){return s;}}
public static class AICF_EVehiclePresetOperation {public const int READ=0,SELECT=1,LOAD=2,DELETE=3,RESET=4,APPLY=5,SAVE=6,CONFIRM_CREW=7,CANCEL_CREW=8;}
public class Player:Native {public int Calls,Operation,Revision,Generation,Library,Slot;public string Payload;public int AICF_RequestVehiclePreset(int s,int r,int g,int l,int o,string p){Calls++;Slot=s;Revision=r;Generation=g;Library=l;Operation=o;Payload=p;return Calls;}}
public class Editor {
public Arr<Widget> m_aButtons=new Arr<Widget>(); public Arr<SCR_ComboBoxComponent> m_aSeats=new Arr<SCR_ComboBoxComponent>();public Arr<int> m_aLibraryIds=new Arr<int>(),m_aActiveMembers=new Arr<int>(),m_aMandatorySeats=new Arr<int>();
public Widget m_wStatus=new Widget(),m_wName=new Widget();public Scroll m_wSeatScroll=new Scroll();public Player m_Player=new Player();public AICF_VehiclePreset m_Draft=new AICF_VehiclePreset();
public SCR_ComboBoxComponent m_Catalog=new SCR_ComboBoxComponent(),m_SourceFilter=new SCR_ComboBoxComponent(),m_CategoryFilter=new SCR_ComboBoxComponent(),m_Behavior=new SCR_ComboBoxComponent(),m_Library=new SCR_ComboBoxComponent();
public string m_sQueuedSelection="",m_sQueuedPayload="",Selected="A";public bool m_bPending,m_bQueued,m_bRendering,m_bPreserveBehaviorOnSelect,BehaviorValid=true,Closed;public int m_iConfirmation,m_iRevision=3,m_iGeneration=7,m_iSelectionRevision=3,m_iSelectionGeneration=7,m_iNextRequestAt,m_iToken,m_iOperation,m_iQueuedOperation,m_iQueuedRevision,m_iQueuedGeneration,m_iQueuedLibraryRevision,m_iLibraryRevision=4,m_iSlot=2,Reveals,Catalogs,Previews,SeatRenders;
public string SelectedPrefab(){return Selected;}public bool ReadBehavior(){return BehaviorValid;}public void RevealDraft(){Reveals++;}public void RenderCatalog(string s=""){Catalogs++;}public void RefreshPreview(){Previews++;}public void RenderSeats(){SeatRenders++;}public string Status(string s){return s;}public void Close(){Closed=true;}
public Widget Button(string s){var w=new Widget{Name=s};m_aButtons.Add(w);return w;}
__METHODS__
}
public static class Suite {
static int N;static void Need(bool b,string s){if(!b)throw new Exception("EDITOR_ASSERT:"+s);N++;}
static Editor E(){Clock.Now=2000;AICF_VehicleSeatAssignment.Calls=0;return new Editor();}
public static int Run(){N=0;Editor e;Widget b;bool handled;
e=E();e.OnChanged(e.m_Catalog,0);Need(e.m_Player.Calls==1&&e.m_Player.Operation==1&&e.m_Player.Payload=="A","catalog immediate SELECT");Need(e.m_Player.Generation==7&&e.m_Player.Revision==3&&e.m_Player.Library==4&&e.m_Player.Slot==2,"captured RPC identity");Need(e.Previews==1&&e.m_bPending&&e.m_sQueuedSelection=="","preview plus one in-flight request");
e=E();e.Selected="";e.OnChanged(e.m_Catalog,0);Need(e.m_Player.Calls==0&&e.Reveals==1,"placeholder preserves draft");
e=E();e.m_bRendering=true;e.OnChanged(e.m_Catalog,0);Need(e.m_Player.Calls==0&&e.Previews==0,"render callback suppressed");
e=E();e.m_iConfirmation=3;e.OnChanged(e.m_Catalog,0);Need(e.m_sQueuedSelection==""&&e.m_Player.Calls==0,"modal selection suppressed");
e=E();e.m_bPending=true;e.Selected="B";e.OnChanged(e.m_Catalog,0);e.Selected="C";e.OnChanged(e.m_Catalog,0);Need(e.m_sQueuedSelection=="C"&&e.m_Player.Calls==0,"pending choice latest retained");e.m_bPending=false;e.SendSelection();Need(e.m_Player.Calls==1&&e.m_Player.Payload=="C","latest dispatched after completion");
e=E();e.m_bQueued=true;e.Selected="B";e.OnChanged(e.m_Catalog,0);Need(e.m_sQueuedSelection=="B"&&e.m_Player.Calls==0,"queued RPC not overwritten");
e=E();e.m_sQueuedSelection="B";e.m_iConfirmation=4;e.SendSelection();Need(e.m_Player.Calls==0&&e.m_sQueuedSelection=="B","selection waits for confirmation");b=e.Button("confirmCrew");e.OnClick(b,0,0,0);Need(e.m_Player.Calls==1&&e.m_Player.Operation==7&&e.m_Player.Payload=="4","confirmation not deadlocked by later selection");
e=E();e.m_sQueuedSelection="B";e.m_iConfirmation=4;b=e.Button("cancelCrew");e.OnClick(b,0,0,0);Need(e.m_Player.Calls==1&&e.m_Player.Operation==8,"cancel not deadlocked by later selection");
e=E();e.m_sQueuedSelection="B";e.m_iGeneration++;e.SendSelection();Need(e.m_Player.Calls==0&&e.m_sQueuedSelection==""&&e.m_wStatus.Text=="REVISION_CONFLICT","generation changed selection cancelled");
e=E();e.m_sQueuedSelection="B";e.m_iRevision++;e.SendSelection();Need(e.m_Player.Calls==0&&e.m_sQueuedSelection=="","revision changed selection cancelled");
e=E();e.BehaviorValid=false;e.m_sQueuedSelection="B";e.SendSelection();Need(e.m_Player.Calls==0&&e.m_sQueuedSelection==""&&e.Reveals==1,"invalid N clears selection and restores draft");
e=E();e.m_iRevision=-1;e.m_sQueuedSelection="B";e.SendSelection();Need(e.m_Player.Calls==0&&e.m_sQueuedSelection=="B","no dispatch before initial context");
e=E();handled=e.OnClick(new Widget{Name="reset"},0,0,0);Need(!handled&&e.m_Player.Calls==0,"foreign button ignored");
e=E();e.m_bPending=true;e.m_sQueuedSelection="B";b=e.Button("close");e.OnClick(b,0,0,0);Need(e.Closed,"close always available");
e=E();e.m_bPending=true;b=e.Button("reset");e.OnClick(b,0,0,0);Need(e.m_Player.Calls==0,"pending RPC blocks reset");
e=E();e.m_sQueuedSelection="B";b=e.Button("reset");e.OnClick(b,0,0,0);Need(e.m_Player.Calls==0,"later model resolves before reset");
e=E();b=e.Button("reset");e.OnClick(b,0,0,0);Need(e.m_Player.Calls==1&&e.m_Player.Operation==4,"reset keeps original operation");
e=E();e.BehaviorValid=false;b=e.Button("save");e.OnClick(b,0,0,0);Need(e.m_Player.Calls==0,"invalid behavior blocks save");
e=E();b=e.Button("autoSeats");e.OnClick(b,0,0,0);Need(AICF_VehicleSeatAssignment.Calls==1&&e.SeatRenders==1&&e.m_Player.Calls==0,"auto seats local draft only");
e=E();var seat=new SCR_ComboBoxComponent();e.m_aSeats.Add(seat);e.OnChanged(seat,3);Need(e.m_Draft.AssignedSeat==0&&e.m_Draft.AssignedMember==2,"seat edits use unpaged index");
e=E();e.m_bPending=true;seat=new SCR_ComboBoxComponent();e.m_aSeats.Add(seat);e.OnChanged(seat,3);Need(e.m_Draft.AssignedSeat==-1,"pending draft not edited");
e=E();e.m_wSeatScroll.Y=200;handled=e.OnMouseWheel(e.m_wSeatScroll,0,0,1);Need(handled&&e.m_wSeatScroll.Y==142,"scroll viewport wheel");
e=E();e.m_wSeatScroll.Y=200;var child=new Widget{Parent=new Widget{Parent=e.m_wSeatScroll}};handled=e.OnMouseWheel(child,0,0,1);Need(handled&&e.m_wSeatScroll.Y==142,"scroll descendants route to viewport");
e=E();handled=e.OnMouseWheel(new Widget(),0,0,1);Need(!handled&&e.m_wSeatScroll.Sets==0,"foreign wheel not consumed");
e=E();e.m_wSeatScroll=null;handled=e.OnMouseWheel(null,0,0,1);Need(!handled,"closed viewport wheel safe");
e=E();Clock.Now=0;e.m_iNextRequestAt=1000;e.Request(1,"A");Need(e.m_bQueued&&e.m_Player.Calls==0,"throttle queues request");Clock.Now=1000;e.SendQueued();Need(e.m_Player.Calls==1&&e.m_Player.Payload=="A"&&e.m_bPending,"throttle dispatch exact request");
return N;
}
}
}
'@
function Execute([hashtable]$Bodies,[string]$Suffix) {
 $definitions = 'public void Request(int operation,string payload)'+$Bodies.Request + 'public void SendQueued()'+$Bodies.SendQueued + 'public void SendSelection()'+$Bodies.SendSelection + 'public void OnChanged(SCR_ComboBoxComponent combo,int item)'+$Bodies.OnChanged + 'public bool OnClick(Widget w,int x,int y,int button)'+$Bodies.OnClick + 'public bool OnMouseWheel(Widget w,int x,int y,int wheel)'+$Bodies.OnMouseWheel
 $ns = 'AICFEditor'+$Suffix+[guid]::NewGuid().ToString('N')
 $code = $shim.Replace('__NS__',$ns).Replace('__METHODS__',$definitions)
 $script:lastVariantCompiled = $false
 Add-Type -TypeDefinition $code -Language CSharp
 $script:lastVariantCompiled = $true
 $type = ([System.Management.Automation.PSTypeName]($ns+'.Suite')).Type
 return $type.GetMethod('Run').Invoke($null,@())
}
$passed = Execute $methods 'Actual'
$mutations = @(
 @('SendSelection','m_iSelectionGeneration != m_iGeneration','false','GENERATION_FENCE'),
 @('SendSelection','m_iSelectionRevision != m_iRevision','false','REVISION_FENCE'),
 @('SendSelection','m_bPending || ','','PENDING_CHOICE_RETAINED'),
 @('SendSelection','m_iConfirmation > 0 || ','','CONFIRMATION_SERIALIZATION'),
 @('SendSelection','if (!ReadBehavior())','if (m_iRevision == int.MinValue)','BEHAVIOR_VALIDATION'),
 @('OnChanged','m_bRendering || ','','PROGRAMMATIC_CALLBACK'),
 @('Request','m_iQueuedGeneration = m_iGeneration;','m_iQueuedGeneration = 0;','RPC_EPOCH_CAPTURE'),
 @('OnClick','if (m_bPending || m_bQueued) return true;','if (m_bPending || m_bQueued || !m_sQueuedSelection.IsEmpty()) return true;','CONFIRMATION_DEADLOCK'),
 @('OnMouseWheel','if (!m_wSeatScroll || current != m_wSeatScroll)','if (!m_wSeatScroll || w != m_wSeatScroll)','DESCENDANT_SCROLL')
)
$rejected = 0
foreach ($mutation in $mutations) {
 $copy = @{}; foreach($key in $methods.Keys){$copy[$key]=$methods[$key]}
 if (!$copy[$mutation[0]].Contains($mutation[1])) { throw ('Mutation source missing '+$mutation[3]) }
 $copy[$mutation[0]] = $copy[$mutation[0]].Replace($mutation[1],$mutation[2])
 try { [void](Execute $copy ('Unsafe'+$rejected)) }
 catch {
  if (!$script:lastVariantCompiled -or !$_.Exception.ToString().Contains('EDITOR_ASSERT:')) { throw }
  $rejected++; continue
 }
 throw ('Unsafe actual-source variant accepted: '+$mutation[3])
}
Write-Output ('VehiclePresetEditor actual-source: PASS assertions='+$passed+' executed_unsafe_rejected='+$rejected+' scroll/lifecycle/source=PASS native_GUI=NOT_RUN')

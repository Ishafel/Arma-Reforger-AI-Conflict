param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)),[string]$SourcePath,[switch]$ReproduceOld)
$ErrorActionPreference='Stop'
if(!$SourcePath){$SourcePath=Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/UI/AICF_VehiclePreviewInput.c'}
$source=[IO.File]::ReadAllText($SourcePath)
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
function Body([string]$s,[string]$name){Get-AICFMethodBody ([pscustomobject]@{Source=$s;Code=(ConvertTo-AICFCodeText $s)}) $name}
if(!$ReproduceOld){
 if($source -notmatch '(?m)^\s*void UpdateDrag\(int generation\)'){throw 'ByName callback must be public (native rejects protected methods)'}
 $stop=Body $source 'StopDrag'
 if(!$stop.Contains('if (!m_bDragScheduled)')-or!$stop.Contains('queue.RemoveByName(this, "UpdateDrag")')){throw 'Missing own scheduled cancellation'}
 if($stop.IndexOf('m_bDragScheduled = false')-gt$stop.IndexOf('RemoveByName')-or$stop.IndexOf('m_DragQueue = null', $stop.IndexOf('ScriptCallQueue queue'))-gt$stop.IndexOf('RemoveByName')){throw 'Cancellation ownership revoked too late'}
 if($source-match'\.Remove\(UpdateDrag\)|\.CallLater\(UpdateDrag'){throw 'Dying handler creates bound callback reference'}
}
function Replay([string]$s){
 $s=$s.Replace('void ~AICF_VehiclePreviewInput()','void DestroyForTest()').Replace('protected ','public ').Replace('override ','public override ').Replace('class AICF_VehiclePreviewInput : ScriptedWidgetEventHandler','public class AICF_VehiclePreviewInput : ScriptedWidgetEventHandler').Replace('GetGame()','Runtime.Game')
 $s=[regex]::Replace($s,'(?m)^\t(void|bool) (\w+)\(','public $1 $2(')
 $s=$s.Replace('WidgetManager.GetMousePos(x, y)','WidgetManager.GetMousePos(out x, out y)').Replace('GetScreenSize(width, height)','GetScreenSize(out width, out height)').Replace(' * 0.5)', ' * 0.5f)')
 return $s
}
$shim=@'
using System;
namespace __NS__ {
public class Native { public bool Gone; public static implicit operator bool(Native n){return n!=null&&!n.Gone;} }
public static class Runtime { public static Game Game; public static bool Dying; public static void Reset(){Game=new Game();Dying=false;} }
public class Game:Native {public ScriptCallQueue Queue=new ScriptCallQueue();public int Reads;public ScriptCallQueue GetCallqueue(){Reads++;return Queue;}}
public class ScriptCallQueue:Native {
 public int Adds,Removes,BoundRemoves,Delay,Generation;public bool Repeat;public object Owner;public string Name;public Action OnRemove;
 public void CallLater(Action<int> fn,int delay,bool repeat,int generation){Adds++;Delay=delay;Repeat=repeat;Generation=generation;Owner=fn.Target;}
 public void Remove(Action<int> fn){BoundRemoves++;if(Runtime.Dying)throw new NullReferenceException("simulated native bound-reference teardown");Owner=null;}
 public void CallLaterByName(object owner,string name,int delay,bool repeat,int generation){Adds++;Owner=owner;Name=name;Delay=delay;Repeat=repeat;Generation=generation;}
 public void RemoveByName(object owner,string name){if(Owner!=owner||Name!=name)throw new Exception("foreign callback cancellation");Removes++;Owner=null;if(OnRemove!=null){var action=OnRemove;OnRemove=null;action();}}
}
public class Widget:Native {public bool Visible=true,Enabled=true;public int Added,Removed;public bool IsVisibleInHierarchy(){return Visible;}public bool IsEnabledInHierarchy(){return Enabled;}public void AddHandler(ScriptedWidgetEventHandler h){Added++;}public void RemoveHandler(ScriptedWidgetEventHandler h){Removed++;h.HandlerDeattached(this);}public void GetScreenSize(out float w,out float h){w=600;h=300;}}
public class RenderTargetWidget:Widget {}
public class AICF_VehiclePreview:Native {public bool Ready=true;public float Yaw,Fov,PanY;public bool IsReady(){return Ready;}public void Rotate(float x){Yaw+=x;}public void Zoom(float x){Fov+=x;}public void Pan(float x){PanY+=x;}}
public static class SCR_EMouseButtons {public const int LEFT=0,MIDDLE=2;}
public static class WidgetManager {public static int X,Y;public static void GetMousePos(out int x,out int y){x=X;y=Y;}}
public class ScriptedWidgetEventHandler:Native {
 public virtual void HandlerDeattached(Widget w){} public virtual bool OnMouseButtonDown(Widget w,int x,int y,int b){return false;}public virtual bool OnMouseButtonUp(Widget w,int x,int y,int b){return false;}public virtual bool OnMouseLeave(Widget w,Widget next,int x,int y){return false;}public virtual bool OnHide(Widget w){return false;}public virtual bool OnDisable(Widget w){return false;}public virtual bool OnMouseWheel(Widget w,int x,int y,int wheel){return false;}
}
__SOURCE__
public static class Suite {
static int N;static void Need(bool b,string name){if(!b)throw new Exception("INPUT_ASSERT:"+name);N++;}
static AICF_VehiclePreviewInput Bind(out RenderTargetWidget w,out AICF_VehiclePreview p){Runtime.Reset();w=new RenderTargetWidget();p=new AICF_VehiclePreview();var i=new AICF_VehiclePreviewInput();i.Attach(w,p);return i;}
public static int Run(){N=0;RenderTargetWidget w;AICF_VehiclePreview p;AICF_VehiclePreviewInput i;
__CASES__
return N;}
}}
'@
$cases=@'
i=Bind(out w,out p);Need(Runtime.Game.Reads==0&&Runtime.Game.Queue.Removes==0&&Runtime.Game.Queue.BoundRemoves==0,"attach without timer never touches queue");i.Detach();Runtime.Dying=true;i.DestroyForTest();Need(w.Removed==1&&Runtime.Game.Reads==0&&Runtime.Game.Queue.BoundRemoves==0,"observed Close then dying dtor has no native queue access");i.Detach();Need(w.Removed==1,"repeated detach idempotent");
i=Bind(out w,out p);Need(i.OnMouseButtonDown(w,10,20,0),"start drag");var q=Runtime.Game.Queue;Need(q.Adds==1&&q.Delay==33&&q.Repeat&&q.Name=="UpdateDrag"&&q.Owner==i,"named exact callback registration");Need(i.m_bDragScheduled&&i.m_DragQueue==q,"capture queue ownership");int first=q.Generation;i.OnMouseButtonUp(w,10,20,0);Need(q.Removes==1&&!i.m_bDragScheduled&&i.m_DragQueue==null,"mouseup cancels exact own timer");i.Detach();Runtime.Dying=true;i.DestroyForTest();Need(q.Removes==1&&q.BoundRemoves==0,"active drag explicit cleanup then destructor once");
i=Bind(out w,out p);i.OnMouseButtonDown(w,0,0,0);q=Runtime.Game.Queue;Runtime.Game.Queue=new ScriptCallQueue();i.Detach();Need(q.Removes==1&&Runtime.Game.Queue.Removes==0,"replacement game queue untouched");
i=Bind(out w,out p);i.OnMouseButtonDown(w,0,0,0);q=Runtime.Game.Queue;Runtime.Game=null;i.Detach();Need(q.Removes==1&&!i.m_bDragScheduled,"cancel cached queue after Game disappears");
i=Bind(out w,out p);i.OnMouseButtonDown(w,0,0,0);q=Runtime.Game.Queue;q.Gone=true;i.Detach();Need(q.Removes==0&&!i.m_bDragScheduled&&i.m_DragQueue==null,"destroyed queue failclosed no native cancellation");
i=Bind(out w,out p);Runtime.Game.Queue=null;Need(!i.OnMouseButtonDown(w,0,0,0)&&!i.m_bDragScheduled,"missing queue no schedule");
i=Bind(out w,out p);w.Visible=false;Need(!i.OnMouseButtonDown(w,0,0,0)&&Runtime.Game.Reads==0,"hidden target no schedule");w.Visible=true;Need(!i.OnMouseButtonDown(new Widget(),0,0,0)&&Runtime.Game.Reads==0,"foreign target ignored");Need(!i.OnMouseButtonDown(w,0,0,1),"unsupported button ignored");
i=Bind(out w,out p);i.OnMouseButtonDown(w,0,0,0);q=Runtime.Game.Queue;first=q.Generation;i.OnMouseButtonDown(w,0,0,0);Need(q.Adds==2&&q.Removes==1,"restart cancels previous timer");int current=q.Generation;float beforeYaw=p.Yaw;WidgetManager.X=20;i.UpdateDrag(first);Need(i.m_bDragScheduled&&q.Removes==1&&q.Owner==i&&p.Yaw==beforeYaw,"stale callback cannot cancel successor");WidgetManager.X=10;WidgetManager.Y=5;i.UpdateDrag(current);Need(p.Yaw==5,"current callback rotates");i.OnMouseLeave(w,null,0,0);Need(q.Removes==2&&!i.m_bRotating,"mouseleave cancels");
i=Bind(out w,out p);i.OnMouseButtonDown(w,0,0,2);q=Runtime.Game.Queue;WidgetManager.Y=30;i.UpdateDrag(q.Generation);Need(p.PanY==.1f,"middle drag normalized pan");i.OnHide(w);Need(q.Removes==1&&!i.m_bPanning,"hide cancels");
i=Bind(out w,out p);i.OnMouseButtonDown(w,0,0,0);q=Runtime.Game.Queue;i.OnDisable(w);Need(q.Removes==1,"disable cancels");
i=Bind(out w,out p);i.OnMouseButtonDown(w,0,0,0);q=Runtime.Game.Queue;i.HandlerDeattached(new Widget());Need(i.m_bDragScheduled&&q.Removes==0,"foreign detached callback ignored");i.HandlerDeattached(w);Need(q.Removes==1&&i.m_wTarget==null&&i.m_Preview==null,"native widget teardown cancels binding");
i=Bind(out w,out p);i.OnMouseButtonDown(w,0,0,0);q=Runtime.Game.Queue;q.OnRemove=()=>i.Detach();i.Detach();Need(q.Removes==1&&w.Removed==1,"reentrant repeated detach cancels once");
i=Bind(out w,out p);i.OnMouseButtonDown(w,0,0,0);q=Runtime.Game.Queue;Runtime.Dying=true;i.DestroyForTest();Need(q.Removes==1&&q.BoundRemoves==0&&w.Removed==1,"fallback destructor uses owned named cancellation");
'@
if($ReproduceOld){$cases=@'
i=Bind(out w,out p);i.Detach();Runtime.Dying=true;bool crashed=false;try{i.DestroyForTest();}catch(NullReferenceException){crashed=true;}Need(crashed,"old actual destructor unconditionally reaches bound Remove");
'@}
function Execute([string]$s,[string]$label){
 $ns='PreviewInput_'+[guid]::NewGuid().ToString('N')
 $code=$shim.Replace('__NS__',$ns).Replace('__SOURCE__',(Replay $s)).Replace('__CASES__',$cases)
 Add-Type -TypeDefinition $code -Language CSharp
 $type=($ns+'.Suite')-as[type]
 return $type::Run()
}
$count=Execute $source 'actual'
if($ReproduceOld){"REPRODUCED old native-boundary counterexample controls=$count; engine VM is separately preserved";exit 0}
$mutants=@(
 @('unowned-bound-cancel',$source.Replace('if (!m_bDragScheduled) { m_DragQueue = null; return; }','if (!m_bDragScheduled) { GetGame().GetCallqueue().Remove(UpdateDrag); m_DragQueue = null; return; }')),
 @('wrong-queue',$source.Replace('ScriptCallQueue queue = m_DragQueue;','ScriptCallQueue queue = GetGame().GetCallqueue();')),
 @('stale-cancel',$source.Replace('if (generation != m_iGeneration) return;','').Replace('generation != m_iGeneration || ','')),
 @('no-cancel',$source.Replace('if (queue) queue.RemoveByName(this, "UpdateDrag");',''))
)
foreach($mutant in $mutants){$failed=$false;try{[void](Execute $mutant[1] $mutant[0])}catch{$failed=$true;if($_.ToString()-notmatch'INPUT_ASSERT:|foreign callback cancellation|NullReferenceException|Object reference'){throw}};if(!$failed){throw "Unsafe mutant escaped: $($mutant[0])"}}
"PASS Vehicle preview input actual lifecycle assertions=$count; executed unsafe mutants=$($mutants.Count); native teardown=NOT_RUN"

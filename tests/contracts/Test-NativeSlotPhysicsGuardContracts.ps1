param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)),[string]$Mutation='')
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$path=Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Integration/AICF_NativeSlotPhysicsGuard.c'
$source=[IO.File]::ReadAllText($path).Replace("`r`n","`n")
function Body($s,$n){Get-AICFMethodBody ([pscustomobject]@{Source=$s;Code=(ConvertTo-AICFCodeText $s)}) $n}
# Literal primary 1.8.0.13 body. Whole source-file SHA:
# 4E0FCC958A1DD15951689F4038D7AA0692352846E7BE5B05F83F19EAC2CAB24F.
# The canonical checkout does not require the ignored API cache.
$old=@'
{
		// Return false if entity is not in desired simulation state, contains cloth component (prevent detection of pouches) or is a weapon
		if (ent.IsLoaded() || ent.GetPhysics().GetSimulationState() == 0 || ent.FindComponent(BaseLoadoutClothComponent) || ent.FindComponent(WeaponComponent))
			return false;

		//Ignore proxy entities
		EntityFlags entityFlags = ent.GetFlags();
		if (entityFlags & EntityFlags.PROXY)
			return false;

		if (ent.IsInherited(ChimeraCharacter))
				return false;

		// Filter out dead bodies and wrecks
		if (IsEntityDestroyed(ent))
		{
			if (ent.IsInherited(Vehicle) && m_RplComponent)
			{
				if (!m_RplComponent.IsProxy())
					SCR_EntityHelper.DeleteEntityAndChildren(ent);

				return false;
			}
		}

		return true;
	}
'@
function Norm($s){(ConvertTo-AICFCodeText $s)-replace '\s+',''}
function ProjectStock($s){
 $b=Body $s 'TraceCallback'
 $b=$b.Replace('AICF_NativeSlotOccupancyFrame frame = m_AICF_OccupancyFrame;','')
 $b=$b.Replace('AICF_ObserveUnknownOccupancyHit(ent, "NULL_ENTITY");','').Replace('AICF_ObserveUnknownOccupancyHit(ent, "NULL_PHYSICS");','')
 $b=[regex]::Replace($b,'if \(!ent\)\s*\{\s*if \(frame\)\s*frame.m_bUnknown = true;\s*return true;\s*\}','')
 $b=[regex]::Replace($b,'if \(!AICF_IsOccupancyFrameCurrent\(frame\)\)\s*return true;','')
 $b=[regex]::Replace($b,'Physics physics = ent.GetPhysics\(\);\s*if \(!physics\)\s*\{\s*if \(frame\)\s*frame.m_bUnknown = true;\s*return true;\s*\}','')
 $b=[regex]::Replace($b,'if \(ent.IsLoaded\(\)\)\s*\{\s*return false;\s*\}\s*if \(physics.GetSimulationState\(\) == 0','if (ent.IsLoaded() || ent.GetPhysics().GetSimulationState() == 0')
 # Remove only braces surrounding a single retained native return/delete.
 $b=[regex]::Replace($b,'\{\s*(return false;|SCR_EntityHelper.DeleteEntityAndChildren\(ent\);)\s*\}','$1')
 return Norm $b
}
function Rules($s){
 $code=ConvertTo-AICFCodeText $s;$occupied=Body $s 'IsOccupied';$trace=Body $s 'TraceCallback'
 if(@([regex]::Matches($occupied,'super\.IsOccupied\(\)')).Count-ne1){'ONE_NATIVE_OCCUPANCY'}
 if(!$occupied.Contains('bool current = m_AICF_OccupancyFrame == frame;') -or !$occupied.Contains("if (current)`n`t`t`tm_AICF_OccupancyFrame = previous;") -or !$occupied.Contains('return occupied || frame.m_bUnknown;')){'OWN_RESTORE_AND_UNKNOWN_RESULT'}
 if(@([regex]::Matches($trace,'ent\.GetPhysics\(\)')).Count-ne1 -or $trace.IndexOf('ent.IsLoaded()')-gt$trace.IndexOf('ent.GetPhysics()')){'CACHED_SINGLE_PHYSICS_AFTER_LOADED'}
 if((ProjectStock $s)-ne(Norm $old)){'EXACT_PINNED_NONNULL_POLICY_PROJECTION'}
 if((Norm (Body $s 'AICF_ObserveUnknownOccupancyHit'))-ne'{}' -or @([regex]::Matches($trace,'frame.m_bUnknown = true;\s*AICF_ObserveUnknownOccupancyHit\(ent, "(?:NULL_ENTITY|NULL_PHYSICS)"\);')).Count-ne2){'EMPTY_AFTER_UNKNOWN_HOOK_ONLY'}
 if($code-match 'CallLater|ScriptInvoker|\bstatic\b|TracePosition|Print\(|GetWorld\(|RegisterSlot|InitiateSpawn|Heal|Kill\('){'NO_NEW_GLOBAL_ASYNC_PHYSICAL_QUOTA_ACTIONS'}
 if(@([regex]::Matches($trace,'DeleteEntityAndChildren\(ent\)')).Count-ne1 -or !$trace.Contains("if (!AICF_IsOccupancyFrameCurrent(frame))`n`t`t`t`t`t`treturn true;`n`t`t`t`t`tSCR_EntityHelper.DeleteEntityAndChildren(ent);")){'SINGLE_ORIGINAL_DELETE_AFTER_PURE_SCOPE_FENCE'}
}
$rules=@(Rules $source);if($rules.Count){throw ($rules-join ',')}
$cache=Join-Path $RepositoryRoot '.cache/reforger-api/Arma-Reforger-Script-Diff-1.8.0.13/scripts/Game/Components/Spawner/SCR_EntitySpawnerSlotComponent.c'
if(Test-Path $cache){
 if((Get-FileHash $cache).Hash-ne'4E0FCC958A1DD15951689F4038D7AA0692352846E7BE5B05F83F19EAC2CAB24F'){throw 'Pinned native source changed'}
 if((Norm (Body ([IO.File]::ReadAllText($cache)) 'TraceCallback'))-ne(Norm $old)){throw 'Literal primary method drift'}
}
$sourceMutants=@(
 @('double-native',$source.Replace('bool occupied = super.IsOccupied();','bool occupied = super.IsOccupied(); super.IsOccupied();')),
 @('drop-unknown',$source.Replace('return occupied || frame.m_bUnknown;','return occupied;')),
 @('unconditional-restore',$source.Replace("if (current)`n`t`t`tm_AICF_OccupancyFrame = previous;",'m_AICF_OccupancyFrame = previous;')),
 @('second-physics',$source.Replace('Physics physics = ent.GetPhysics();','Physics physics = ent.GetPhysics(); ent.GetPhysics();')),
 @('allow-proxy',$source.Replace('if (entityFlags & EntityFlags.PROXY)','if (false)')),
 @('delete-cloth',$source.Replace('ent.FindComponent(BaseLoadoutClothComponent) || ','')),
 @('native-delete-unfenced',$source.Replace("if (!AICF_IsOccupancyFrameCurrent(frame))`n`t`t`t`t`t`treturn true;`n`t`t`t`t`tSCR_EntityHelper.DeleteEntityAndChildren(ent);",'SCR_EntityHelper.DeleteEntityAndChildren(ent);')),
 @('extra-trace',$source.Replace('bool occupied = super.IsOccupied();','TracePosition(); bool occupied = super.IsOccupied();')),
 @('hook-before-unknown',$source.Replace("frame.m_bUnknown = true;`n`t`t`tAICF_ObserveUnknownOccupancyHit(ent, `"NULL_ENTITY`");", "AICF_ObserveUnknownOccupancyHit(ent, `"NULL_ENTITY`");`n`t`t`tframe.m_bUnknown = true;")),
 @('hook-body-action',$source.Replace("protected void AICF_ObserveUnknownOccupancyHit(IEntity ent, string reason)`n`t{`n`t}","protected void AICF_ObserveUnknownOccupancyHit(IEntity ent, string reason)`n`t{ ent.GetPhysics(); }") )
)
foreach($m in $sourceMutants){if($m[1]-eq$source-or!@(Rules $m[1]).Count){throw "Escaped source mutation $($m[0])"}}
function Translate($s){
 $s=$s-replace '\bref\s+','' -replace '\bprotected\b','public' -replace '\bsuper\.','base.'
 $s=$s-replace '(?m)^(\s*)override bool IsOccupied\(\)','$1public override bool IsOccupied()'
 $s=$s-replace 'public void AICF_ObserveUnknownOccupancyHit','public virtual void AICF_ObserveUnknownOccupancyHit'
 $s=$s-replace 'class AICF_NativeSlotOccupancyFrame','public class AICF_NativeSlotOccupancyFrame : EngineObject'
 $s=$s-replace 'modded class SCR_EntitySpawnerSlotComponent','public class SCR_EntitySpawnerSlotComponent : NativeSlot'
 $s=$s-replace '(?m)^\tbool m_bUnknown;',"`tpublic bool m_bUnknown;"
 $s=$s-replace 'if \(entityFlags & EntityFlags.PROXY\)','if ((entityFlags & EntityFlags.PROXY) != 0)'
 $s=$s-replace 'FindComponent\((BaseLoadoutClothComponent|WeaponComponent)\)','FindComponent(typeof($1))'
 $s=$s-replace 'IsInherited\((ChimeraCharacter|Vehicle)\)','IsInherited(typeof($1))'
 return $s
}
$actual=Translate $source
switch($Mutation){
 'drop-unknown' {$actual=$actual.Replace('return occupied || frame.m_bUnknown;','return occupied;')}
 'restore-successor' {$actual=$actual.Replace("if (current)`n`t`t`tm_AICF_OccupancyFrame = previous;",'m_AICF_OccupancyFrame = previous;')}
 'skip-delete-fence' {$actual=$actual.Replace("if (!AICF_IsOccupancyFrameCurrent(frame))`n`t`t`t`t`t`treturn true;`n`t`t`t`t`tSCR_EntityHelper.DeleteEntityAndChildren(ent);",'SCR_EntityHelper.DeleteEntityAndChildren(ent);')}
 '' {}
 default {throw 'Unknown executed mutation'}
}
$oldCs=Translate $old
$shim=@'
using System;
using System.Collections.Generic;
public class EngineObject { public static implicit operator bool(EngineObject v){return !Object.ReferenceEquals(v,null);} }
[Flags] public enum EntityFlags { NONE=0, PROXY=1 }
public class BaseLoadoutClothComponent : EngineObject {}
public class WeaponComponent : EngineObject {}
public class ChimeraCharacter {}
public class Vehicle {}
public class Physics : EngineObject {
 public int State=1; public Action Read=delegate{};
 public int GetSimulationState(){Read();return State;}
}
public class IEntity : EngineObject {
 public bool Loaded,Cloth,Weapon,Character,Vehicle,Destroyed;
 public Physics Physics=new Physics(); public EntityFlags Flags; public int PhysicsReads;
 public List<string> Reads=new List<string>(); public Action<string> OnRead=delegate{};
 public void Read(string n){Reads.Add(n);OnRead(n);}
 public bool IsLoaded(){Read("loaded");return Loaded;}
 public Physics GetPhysics(){Read("physics");PhysicsReads++;return Physics;}
 public EngineObject FindComponent(Type t){Read(t.Name);if(t==typeof(BaseLoadoutClothComponent))return Cloth?new EngineObject():null;return Weapon?new EngineObject():null;}
 public EntityFlags GetFlags(){Read("flags");return Flags;}
 public bool IsInherited(Type t){Read(t.Name);return t==typeof(ChimeraCharacter)?Character:Vehicle;}
}
public class RplComponent : EngineObject {public bool Proxy; public Action Read=delegate{}; public bool IsProxy(){Read();return Proxy;}}
public static class SCR_EntityHelper {
 public static int Deletes; public static Action OnDelete=delegate{};
 public static void DeleteEntityAndChildren(IEntity ent){Deletes++;OnDelete();}
}
public class NativeSlot : EngineObject {
 public IEntity Hit; public RplComponent m_RplComponent; public int NativeCalls;
 public Func<NativeSlot,bool> Operation; public Action DestroyedRead=delegate{};
 public virtual bool IsOccupied(){NativeCalls++;if(Operation!=null)return Operation(this);bool accepted=TraceCallback(Hit);return Hit!=null&&accepted;}
 public virtual bool TraceCallback(IEntity ent){throw new Exception("abstract native callback");}
 public bool IsEntityDestroyed(IEntity ent){DestroyedRead();return ent.Destroyed;}
}
'@
$tests=@'
public class ObservedSlot : SCR_EntitySpawnerSlotComponent {
 public int Hooks; public string Reason; public IEntity Observed; public bool UnknownBeforeHook; public Action Hook=delegate{};
 public override void AICF_ObserveUnknownOccupancyHit(IEntity ent,string reason){Hooks++;Reason=reason;Observed=ent;UnknownBeforeHook=m_AICF_OccupancyFrame!=null&&m_AICF_OccupancyFrame.m_bUnknown;Hook();}
}
public static class RunSlotGuard {
 static int checks;
 static void Check(bool x,string n){checks++;if(!x)throw new Exception("CASE "+n);}
 static IEntity Entity(int mask){return new IEntity{Loaded=(mask&1)!=0,Cloth=(mask&2)!=0,Weapon=(mask&4)!=0,Flags=(mask&8)!=0?EntityFlags.PROXY:EntityFlags.NONE,Character=(mask&16)!=0,Destroyed=(mask&32)!=0,Vehicle=(mask&64)!=0,Physics=new Physics{State=(mask&128)!=0?0:1}};}
 static RplComponent Rpl(int mask){return (mask&256)!=0?new RplComponent{Proxy=(mask&512)!=0}:null;}
 public static string Run(){
  // Literal old function throws for the actual native failure boundary.
  OldSlot old=new OldSlot{Hit=new IEntity{Physics=null}};bool threw=false;
  try{old.IsOccupied();}catch(NullReferenceException){threw=true;}
  Check(threw&&old.NativeCalls==1,"OLD_NULL_PHYSICS_REPRO");
  SCR_EntitySpawnerSlotComponent safe=new SCR_EntitySpawnerSlotComponent{Hit=new IEntity{Physics=null}};
  Check(safe.IsOccupied()&&safe.NativeCalls==1&&safe.m_AICF_OccupancyFrame==null,"NEW_NULL_PHYSICS_OCCUPIED");
  safe=new SCR_EntitySpawnerSlotComponent();
  Check(safe.IsOccupied()&&safe.NativeCalls==1,"NULL_ENTITY_NATIVE_TRACEENT_NULL_OCCUPIED");
  safe=new SCR_EntitySpawnerSlotComponent{Hit=new IEntity{Loaded=true,Physics=null}};
  Check(!safe.IsOccupied()&&safe.Hit.PhysicsReads==0,"LOADED_SHORT_CIRCUITS_PHYSICS");
  ObservedSlot observed=new ObservedSlot{Hit=new IEntity{Physics=null}};
  Check(observed.IsOccupied()&&observed.Hooks==1&&observed.Reason=="NULL_PHYSICS"&&observed.Observed==observed.Hit&&observed.UnknownBeforeHook,"ONCE_NULL_PHYSICS_HOOK_AFTER_UNKNOWN");
  observed=new ObservedSlot();
  Check(observed.IsOccupied()&&observed.Hooks==1&&observed.Reason=="NULL_ENTITY"&&observed.Observed==null&&observed.UnknownBeforeHook,"ONCE_NULL_ENTITY_HOOK_AFTER_UNKNOWN");
  observed=new ObservedSlot{Hit=new IEntity{Loaded=true,Physics=null}};
  Check(!observed.IsOccupied()&&observed.Hooks==0,"LOADED_NO_UNKNOWN_HOOK");
  observed=new ObservedSlot{Hit=new IEntity()};
  Check(observed.IsOccupied()&&observed.Hooks==0,"KNOWN_PHYSICS_NO_UNKNOWN_HOOK");
  observed=new ObservedSlot{Hit=new IEntity{Physics=null}};AICF_NativeSlotOccupancyFrame hookSuccessor=new AICF_NativeSlotOccupancyFrame();
  observed.Hook=delegate{observed.m_AICF_OccupancyFrame=hookSuccessor;};
  Check(observed.IsOccupied()&&observed.Hooks==1&&observed.m_AICF_OccupancyFrame==hookSuccessor&&!hookSuccessor.m_bUnknown,"HOOK_CANNOT_FREE_OR_CLOBBER_SUCCESSOR");
  // All combinations of original native filters/authority; identical read order.
  for(int mask=0;mask<1024;mask++){
   IEntity a=Entity(mask),b=Entity(mask);OldSlot o=new OldSlot{Hit=a,m_RplComponent=Rpl(mask)};
   SCR_EntitySpawnerSlotComponent n=new SCR_EntitySpawnerSlotComponent{Hit=b,m_RplComponent=Rpl(mask)};
   SCR_EntityHelper.Deletes=0;bool ov=o.IsOccupied();int od=SCR_EntityHelper.Deletes;
   SCR_EntityHelper.Deletes=0;bool nv=n.IsOccupied();int nd=SCR_EntityHelper.Deletes;
   Check(ov==nv&&od==nd&&String.Join(",",a.Reads.ToArray())==String.Join(",",b.Reads.ToArray())&&n.NativeCalls==1&&n.m_AICF_OccupancyFrame==null,"NATIVE_POLICY_"+mask);
   a=Entity(mask);b=Entity(mask);o=new OldSlot{m_RplComponent=Rpl(mask)};n=new SCR_EntitySpawnerSlotComponent{m_RplComponent=Rpl(mask)};
   SCR_EntityHelper.Deletes=0;ov=o.TraceCallback(a);od=SCR_EntityHelper.Deletes;
   SCR_EntityHelper.Deletes=0;nv=n.TraceCallback(b);nd=SCR_EntityHelper.Deletes;
   Check(ov==nv&&od==nd&&String.Join(",",a.Reads.ToArray())==String.Join(",",b.Reads.ToArray()),"UNFRAMED_NATIVE_POLICY_"+mask);
  }
  // Nested calls own separate frames; null-hit evidence belongs only to that call.
  safe=new SCR_EntitySpawnerSlotComponent();int depth=0;AICF_NativeSlotOccupancyFrame outer=null;
  safe.Operation=delegate(NativeSlot s){depth++;if(depth==1){outer=safe.m_AICF_OccupancyFrame;bool inner=safe.IsOccupied();Check(inner,"INNER_NULL_OCCUPIED");Check(safe.m_AICF_OccupancyFrame==outer&&!outer.m_bUnknown,"NESTED_RESTORE_OWN_FRAME");depth--;return false;}safe.TraceCallback(null);depth--;return false;};
  Check(!safe.IsOccupied()&&safe.NativeCalls==2&&safe.m_AICF_OccupancyFrame==null,"INNER_UNKNOWN_DOES_NOT_CONTAMINATE_OUTER");
  safe=new SCR_EntitySpawnerSlotComponent();
  AICF_NativeSlotOccupancyFrame successor=new AICF_NativeSlotOccupancyFrame();
  safe.Operation=delegate(NativeSlot s){safe.m_AICF_OccupancyFrame=successor;return false;};
  Check(safe.IsOccupied()&&safe.m_AICF_OccupancyFrame==successor&&!successor.m_bUnknown,"LOST_OWNER_OCCUPIED_NO_SUCCESSOR_CLOBBER");
  safe=new SCR_EntitySpawnerSlotComponent();safe.Operation=delegate(NativeSlot s){safe.m_AICF_OccupancyFrame=null;return false;};
  Check(safe.IsOccupied()&&safe.m_AICF_OccupancyFrame==null,"LOST_OWNER_NULL_OCCUPIED");
  // Every native callback seam can invalidate the frame. No stale false or delete.
  string[] seams={"loaded","physics","simulation","BaseLoadoutClothComponent","WeaponComponent","flags","ChimeraCharacter","Vehicle","destroyed","rpl"};
  foreach(string seam in seams){
   safe=new SCR_EntitySpawnerSlotComponent{Hit=new IEntity{Destroyed=true,Vehicle=true},m_RplComponent=new RplComponent()};
   successor=new AICF_NativeSlotOccupancyFrame();bool fired=false;
   Action lose=delegate{fired=true;safe.m_AICF_OccupancyFrame=successor;};
   safe.Hit.OnRead=delegate(string n){if(n==seam)lose();};
   safe.Hit.Physics.Read=delegate{if(seam=="simulation")lose();};
   safe.DestroyedRead=delegate{if(seam=="destroyed")lose();};safe.m_RplComponent.Read=delegate{if(seam=="rpl")lose();};
   SCR_EntityHelper.Deletes=0;bool value=safe.IsOccupied();
   Check(fired&&value&&SCR_EntityHelper.Deletes==0&&safe.m_AICF_OccupancyFrame==successor&&!successor.m_bUnknown,"STALE_BEFORE_DELETE_"+seam);
  }
  for(int branch=0;branch<6;branch++){
   int[] masks={1,128,2,4,8,16};safe=new SCR_EntitySpawnerSlotComponent{Hit=Entity(masks[branch])};
   successor=new AICF_NativeSlotOccupancyFrame();
   safe.Hit.OnRead=delegate(string n){safe.m_AICF_OccupancyFrame=successor;};
   Check(safe.IsOccupied()&&safe.m_AICF_OccupancyFrame==successor&&!successor.m_bUnknown,"STALE_NATIVE_FALSE_BRANCH_"+branch);
  }
  safe=new SCR_EntitySpawnerSlotComponent{Hit=new IEntity{Physics=null}};AICF_NativeSlotOccupancyFrame captured=null;
  successor=new AICF_NativeSlotOccupancyFrame();safe.Hit.OnRead=delegate(string n){if(n=="physics"){captured=safe.m_AICF_OccupancyFrame;safe.m_AICF_OccupancyFrame=successor;}};
  Check(safe.IsOccupied()&&captured.m_bUnknown&&!successor.m_bUnknown&&safe.m_AICF_OccupancyFrame==successor,"UNKNOWN_MARKS_CAPTURED_NOT_SUCCESSOR");
  safe=new SCR_EntitySpawnerSlotComponent{Hit=new IEntity{Destroyed=true,Vehicle=true},m_RplComponent=new RplComponent()};
  successor=new AICF_NativeSlotOccupancyFrame();SCR_EntityHelper.Deletes=0;
  SCR_EntityHelper.OnDelete=delegate{safe.m_AICF_OccupancyFrame=successor;};
  Check(safe.IsOccupied()&&SCR_EntityHelper.Deletes==1&&safe.m_AICF_OccupancyFrame==successor,"DELETE_CALLBACK_LOSS_NO_SECOND_DELETE");SCR_EntityHelper.OnDelete=delegate{};
  safe=new SCR_EntitySpawnerSlotComponent();AICF_NativeSlotOccupancyFrame saved=new AICF_NativeSlotOccupancyFrame();safe.m_AICF_OccupancyFrame=saved;
  safe.Operation=delegate(NativeSlot s){return false;};
  Check(!safe.IsOccupied()&&safe.m_AICF_OccupancyFrame==saved,"PREVIOUS_FRAME_RESTORED");
  safe.Operation=delegate(NativeSlot s){s.TraceCallback(null);return false;};
  Check(safe.IsOccupied()&&safe.m_AICF_OccupancyFrame==saved&&!saved.m_bUnknown,"UNKNOWN_CAPTURED_FRAME_ONLY");
  safe.Operation=delegate(NativeSlot s){return true;};
  Check(safe.IsOccupied(),"NATIVE_TRUE_RETAINED");
  return "Native slot guard actual-source replay: PASS; checks="+checks+"; native trace/entity/physics boundaries SIMULATED";
 }
}
'@
$program=$shim+"`n"+$actual+"`npublic class OldSlot : NativeSlot { public override bool TraceCallback(IEntity ent)"+$oldCs+"}`n"+$tests
Add-Type -TypeDefinition $program -Language CSharp
[RunSlotGuard]::Run()
"Native slot guard source contracts: PASS; unsafe=$($sourceMutants.Count); pinned-projection=PASS; executed_mutation=$Mutation"

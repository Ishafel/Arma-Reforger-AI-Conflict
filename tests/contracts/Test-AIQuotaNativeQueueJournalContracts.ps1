param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
$source=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Forces/AICF_AIQuotaNativeQueueJournal.c'))
$source=$source.Substring(0,$source.IndexOf('modded class SCR_AIWorld'))
if($source-match'\b(SetLimitOfActiveAIs|DeleteRplEntity|ActivateAI|DeactivateAI|RequestSpawn|SpawnEntityPrefab|SetDormantCounts)\s*\('){throw 'Read-only native journal changes gameplay'}
function Adapt([string]$text){
 $text=$text.Replace('ResourceName','string').Replace('protected ref ','public ').Replace('protected ','public ').Replace('ref ','').Replace('int.MAX','int.MaxValue').Replace('GetGame()','Runtime.Game')
 $text=[regex]::Replace($text,'array<(\w+)>','Arr<$1>')
 $text=[regex]::Replace($text,'(Arr<\w+>) (m_\w+) = \{\};','$1 $2 = new $1();')
 $text=[regex]::Replace($text,'foreach \((\w+) (\w+) : (\w+)\)','foreach ($1 $2 in $3)')
 $text=[regex]::Replace($text,'(?m)^class (AICF_\w+)','public class $1:N')
 $text=$text.Replace('void AICF_AIQuotaNativeQueueJournal(','public AICF_AIQuotaNativeQueueJournal(')
 $text=[regex]::Replace($text,'(?m)^(\s*)(AICF_\w+|SCR_AIWorld|BaseWorld|int|float|bool|Arr<\w+>) (m_\w+)([;= ])','$1public $2 $3$4')
 $text=[regex]::Replace($text,'(?m)^(\s*)(bool|void|AICF_AIQuotaNativeQueueRecord|AICF_AIQuotaNativeQueueJournal) (\w+)\(','$1public $2 $3(')
 $text=$text.Replace('for (int index;', 'for (int index=0;').Replace('AICF_AIQuotaNativeQueueRecord record;','AICF_AIQuotaNativeQueueRecord record=null;').Replace('m_aRecords.Remove(index)','m_aRecords.RemoveAt(index)')
 return $text
}
$preamble=@'
#pragma warning disable 0162
using System;using System.Collections.Generic;
public class N {public static implicit operator bool(N value){return value!=null;}}
public class Arr<T>:List<T>{public static implicit operator bool(Arr<T> value){return value!=null;}public new int Count(){return base.Count;}public void Insert(T value){Add(value);}public void Copy(Arr<T> values){Clear();AddRange(values);}}
public static class Replication {public static bool Server=true;public static bool IsServer(){return Server;}}
public static class AICF_AIQuotaMath {public const int MAX_TOTAL_UNITS=4096;}
public class BaseWorld:N {public Dictionary<int,SCR_AIGroup> Groups=new Dictionary<int,SCR_AIGroup>();public SCR_AIGroup FindEntityByID(int id){SCR_AIGroup g;return Groups.TryGetValue(id,out g)?g:null;}}
public class SCR_AIWorld:N {public BaseWorld Main;public BaseWorld GetWorld(){return Main;}}
public class Game:N {public SCR_AIWorld World;public BaseWorld GetWorld(){return World.Main;}public SCR_AIWorld GetAIWorld(){return World;}}
public static class Runtime {public static Game Game;}
public class SCR_AIGroup:N {public BaseWorld World;public int Id,Actual,Dead,Alive=-1,Rpl=1;public object Faction=new object();public Arr<string> m_aUnitPrefabSlots=new Arr<string>();public bool Current=true;public BaseWorld GetWorld(){return World;}public bool IsExpandComplete(){return Actual>=m_aUnitPrefabSlots.Count()-Dead;}}
public class AICF_AIQuotaGroupQueue:N {public SCR_AIGroup m_Group;public BaseWorld m_World;public int m_GroupId,Serial,Rpl;public string m_sRpl;public object Faction;public static AICF_AIQuotaGroupQueue Capture(SCR_AIGroup g,object owner,int serial){if(g==null||!g.Current)return null;return new AICF_AIQuotaGroupQueue{m_Group=g,m_GroupId=g.Id,m_World=g.World,Serial=serial,Rpl=g.Rpl,Faction=g.Faction,m_sRpl=g.Rpl.ToString()};}public bool IsCurrent(){return Replication.Server&&m_Group!=null&&m_Group.Current&&m_Group.World==m_World&&Runtime.Game.GetWorld()==m_World&&m_Group.Id==m_GroupId&&m_World.FindEntityByID(m_GroupId)==m_Group&&m_Group.Rpl==Rpl&&m_Group.Faction==Faction;}public static int RequestedUnits(SCR_AIGroup g,int wanted){if(g==null||wanted< -1||wanted>4096||g.Actual<0||g.Dead<0||g.Actual>g.m_aUnitPrefabSlots.Count()-g.Dead)return -1;if(wanted==0||(wanted== -1&&g.Alive==0))return 0;return g.m_aUnitPrefabSlots.Count()-g.Dead-g.Actual;}}
'@
$cases=@'
public static class Cases {
 static SCR_AIWorld World(){Replication.Server=true;var w=new SCR_AIWorld{Main=new BaseWorld()};Runtime.Game=new Game{World=w};return w;}
 static SCR_AIGroup Group(SCR_AIWorld w,int id=2){var g=new SCR_AIGroup{World=w.Main,Id=id};for(int n=0;n<10;n++)g.m_aUnitPrefabSlots.Add("stock");w.Main.Groups.Add(id,g);return g;}
 public static int Execute(){int n=0;Action<bool,string> need=(ok,label)=>{if(!ok)throw new Exception(label);n++;};var w=World();var g=Group(w);var j=new AICF_AIQuotaNativeQueueJournal(w);var output=new Arr<AICF_AIQuotaNativeQueueRecord>();need(j.IsCurrent(),"exact world server journal");var r=j.BeforeRequest(g,10,100);need(r!=null&&!j.Snapshot(output),"request before native return is not certified");j.AfterRequest(r);need(j.Snapshot(output)&&output.Count()==1&&r.m_iSlotsWanted==10&&r.m_fObserverRange==100,"only observed native script request journalled");g.Actual=1;g.Alive=10;var again=j.BeforeRequest(g,9,0);j.AfterRequest(again);need(again==r&&j.Snapshot(output)&&output.Count()==1&&r.m_iSequence==1&&r.m_iSlotsWanted==9,"repeat request same identity not duplicate physical units");
 j.Cancel(new SCR_AIGroup{World=w.Main,Id=g.Id});need(j.Snapshot(output)&&output.Count()==1,"foreign pointer cannot cancel own metadata");j.Cancel(g);need(j.Snapshot(output)&&output.Count()==0,"exact native cancellation retires metadata only");
 r=j.BeforeRequest(g,9,0);j.AfterRequest(r);g.Actual=10;need(j.Snapshot(output)&&output.Count()==0,"completed group retires records");g.Actual=1;r=j.BeforeRequest(g,9,0);j.AfterRequest(r);w.Main.Groups.Remove(g.Id);r.m_Identity.m_Group=null;need(j.Snapshot(output)&&output.Count()==0,"proven native deletion prunes without lifetime cap");
 w=World();g=Group(w);j=new AICF_AIQuotaNativeQueueJournal(w);g.Alive=0;need(j.BeforeRequest(g,-1,0)==null&&j.Snapshot(output)&&output.Count()==0,"eliminated implicit request is not fabricated");g.Alive=-1;r=j.BeforeRequest(g,10,0);j.AfterRequest(r);g.m_aUnitPrefabSlots[0]="changed";need(!j.Snapshot(output),"authored shape mutation is unknown, not absent pending");
 w=World();g=Group(w);j=new AICF_AIQuotaNativeQueueJournal(w);r=j.BeforeRequest(g,10,0);g.Rpl=2;j.AfterRequest(r);need(!j.Snapshot(output),"changed Rpl during native call cannot certify request");
 w=World();g=Group(w);j=new AICF_AIQuotaNativeQueueJournal(w);r=j.BeforeRequest(g,10,0);j.AfterRequest(r);Runtime.Game.World=new SCR_AIWorld{Main=w.Main};need(!j.Snapshot(output),"world replacement rejects old snapshot");
 w=World();g=Group(w);j=new AICF_AIQuotaNativeQueueJournal(w);g.Current=false;need(j.BeforeRequest(g,10,0)==null,"unprovable birth header not adopted");g.Current=true;need(!j.Snapshot(output),"missed native request tracking cannot silently become complete");
 w=World();g=Group(w);j=new AICF_AIQuotaNativeQueueJournal(w);for(int k=0;k<5000;k++){r=j.BeforeRequest(g,10,0);need(r!=null,"bounded journal repeated request");j.AfterRequest(r);j.Cancel(g);}need(j.Snapshot(output)&&output.Count()==0,"retired journal records do not create lifetime4096 refusal");
 w=World();g=Group(w);j=new AICF_AIQuotaNativeQueueJournal(w);Replication.Server=false;need(!j.IsCurrent()&&!j.Snapshot(output),"client cannot supply authoritative pending snapshot");return n;}
}
'@
$code=$preamble+(Adapt $source)+$cases
Add-Type $code
$count=[Cases]::Execute()
$unsafe=0
foreach($mutation in @(
 @('unreturned-request',' || !record.m_bEnqueueReturned',''),
 @('untracked-capture-gap','if (!identity) { m_bUnknown = true; return null; }','if (!identity) { return null; }'),
 @('forget-authored-snapshot','m_Identity.m_Group.m_aUnitPrefabSlots[index] != m_aPrefabs[index]','false'),
 @('skip-completed-prune','if (record.m_Identity.m_Group.IsExpandComplete()) m_aRecords.Remove(index);','')
)){
 if(!$source.Contains($mutation[1])){throw "Missing journal mutation $($mutation[0])"}
 $name='JournalUnsafe'+[guid]::NewGuid().ToString('N')
 $mutated=($preamble+(Adapt $source.Replace($mutation[1],$mutation[2]))+$cases).Replace('public class N {',"namespace $name {public class N {")+'}'
 Add-Type $mutated
 $failed=$false;try{$null=([type]("$name.Cases")).GetMethod('Execute').Invoke($null,@())}catch{$failed=$true}
 if(!$failed){throw "Unsafe native journal survived $($mutation[0])"};$unsafe++
}
"PASS AI quota native request journal actual=$count executed_unsafe=$unsafe native_queue_membership=UNPROVEN runtime=NOT_RUN"

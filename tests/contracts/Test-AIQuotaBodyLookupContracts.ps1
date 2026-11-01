param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
$root=Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Forces'
$source=[IO.File]::ReadAllText((Join-Path $root 'AICF_AIQuotaBodyLookup.c'))
$service=[IO.File]::ReadAllText((Join-Path $root 'AICF_AIWorldQuotaService.c'))
if($source-match'\b(RefreshCensus|CallLater|SpawnEntity|RegisterExistingBinding|SetLimitOfActiveAIs|SetHealth)\s*\('){throw 'Lookup grants gameplay permissions'}
if($service-notmatch'(?s)m_bQuotaCensusBusy = true;\s*m_BodyLookup = new AICF_AIQuotaBodyLookup\(\);\s*m_BodyLookup.Capture\(m_aBodies\);\s*bool refreshed = RefreshCurrentCensus\(\);\s*m_BodyLookup = null;\s*m_bQuotaCensusBusy = false;'){throw 'Index must exist only during the original guarded synchronous refresh'}
# Bodies are append-only until released/removed; no reorder or revival can
# change the first live duplicate behind the local lookup.
if($service-match'm_aBodies\.(InsertAt|Swap|Reverse|Sort)\('){throw 'Body order contract changed'}
$fallback='foreach (AICF_AIQuotaBody body : bodies)'
$fallbackAt=$source.LastIndexOf($fallback)
if($fallbackAt -lt 0){throw 'Actual fallback loop absent'}
$withoutFallback=$source.Substring(0,$fallbackAt)+$source.Substring($fallbackAt).Replace($fallback,'foreach (AICF_AIQuotaBody body : new array<ref AICF_AIQuotaBody>())')
$variants=@(
 @{name='Actual';source=$source},
 @{name='Released';source=$source.Replace('!found.m_bReleased && ','')},
 @{name='Removed';source=$source.Replace(' && bodies.Contains(found)','')},
 @{name='ChangedEntity';source=$source.Replace('found.m_Entity == entity && ','')},
 @{name='MissingFallback';source=$withoutFallback},
 @{name='LastDuplicate';source=$source.Replace(' || m_mBodies.Contains(body.m_Entity)','')}
)
$code=@'
using System;
using System.Collections.Generic;
public class Native {public static implicit operator bool(Native x){return x!=null;}}
public class IEntity:Native {public int Id;}
public class AICF_AIQuotaBody:Native {public IEntity m_Entity;public bool m_bReleased;}
public class EArray<T>:List<T>{}
public class EMap<K,V> where V:class {readonly Dictionary<K,V> Vals=new Dictionary<K,V>();public void Clear(){Vals.Clear();}public bool Contains(K key){return Vals.ContainsKey(key);}public void Set(K k,V v){Vals[k]=v;}public V Get(K k){V v;return Vals.TryGetValue(k,out v)?v:null;}}
'@
foreach($v in $variants){
 $s=$v.source.Replace('AICF_AIQuotaBodyLookup','Lookup_'+$v.name).Replace('protected ref ','protected ').Replace('array<ref AICF_AIQuotaBody>','EArray<AICF_AIQuotaBody>').Replace('map<IEntity, AICF_AIQuotaBody>','EMap<IEntity, AICF_AIQuotaBody>')
 $s=$s.Replace('class Lookup_'+$v.name,'public class Lookup_'+$v.name+' : Native').Replace('void Capture(','public void Capture(').Replace('AICF_AIQuotaBody Find(','public AICF_AIQuotaBody Find(')
 $s=[regex]::Replace($s,'foreach \(([^:]+) : ([^)]+)\)','foreach ($1 in $2)')
 $code+=$s
}
$code+=@'
public static class LookupCases
{
 static int N;
 static void Need(bool b,string label){N++;if(!b)throw new Exception(label);}
 static AICF_AIQuotaBody Find(object x,IEntity e,EArray<AICF_AIQuotaBody> a){return (AICF_AIQuotaBody)x.GetType().GetMethod("Find").Invoke(x,new object[]{e,a});}
 static void Capture(object x,EArray<AICF_AIQuotaBody> a){x.GetType().GetMethod("Capture").Invoke(x,new object[]{a});}
 static AICF_AIQuotaBody Linear(IEntity e,EArray<AICF_AIQuotaBody> a){if(e==null)return null;foreach(var b in a)if(b!=null&&!b.m_bReleased&&b.m_Entity==e)return b;return null;}
 public static int Run(Type type)
 {
  N=0;var x=Activator.CreateInstance(type);var e=new IEntity{Id=1};var b=new AICF_AIQuotaBody{m_Entity=e};var a=new EArray<AICF_AIQuotaBody>{null,b};Capture(x,a);
  Need(Find(x,e,a)==b,"same exact body");Need(Find(x,null,a)==null,"null entity");
  b.m_bReleased=true;Need(Find(x,e,a)==null,"released cached body rejected");
  var c=new AICF_AIQuotaBody{m_Entity=e};a.Add(c);Need(Find(x,e,a)==c,"new binding after capture visible");
  a.Remove(c);Need(Find(x,e,a)==null,"removed live body rejected");
  a.Clear();b=new AICF_AIQuotaBody{m_Entity=e};c=new AICF_AIQuotaBody{m_Entity=e};a.Add(b);a.Add(c);Capture(x,a);Need(Find(x,e,a)==b,"first live duplicate preserves old lookup order");
  b.m_Entity=null;Need(Find(x,e,a)==c,"weak entity disappearing invalidates cached record");
  var other=new IEntity{Id=1};Need(Find(x,other,a)==null,"reused ID is not same entity");
  var empty=new EArray<AICF_AIQuotaBody>();Capture(x,empty);Need(Find(x,e,empty)==null,"next refresh forgets old entries");
  var rng=new Random(78);a.Clear();var entities=new EArray<IEntity>();for(int i=0;i<64;i++){entities.Add(new IEntity{Id=i});a.Add(new AICF_AIQuotaBody{m_Entity=entities[i]});}Capture(x,a);
  for(int step=0;step<512;step++)
  {
   if(step%17==0)Capture(x,a);
   int op=rng.Next(4);if(op==0&&a.Count>0)a[rng.Next(a.Count)].m_bReleased=true;
   if(op==1&&a.Count>0)a.RemoveAt(rng.Next(a.Count));
   if(op==2){var fresh=new IEntity{Id=1000+step};entities.Add(fresh);a.Add(new AICF_AIQuotaBody{m_Entity=fresh});}
   var query=entities[rng.Next(entities.Count)];Need(Find(x,query,a)==Linear(query,a),"seeded append/release/remove matches original linear decision");
  }
  return N;
 }
}
'@
Add-Type -TypeDefinition $code -Language CSharp
$cases=[LookupCases]::Run(('Lookup_Actual'-as[type]))
$killed=0
foreach($v in ($variants|Select-Object -Skip 1)){try{[void][LookupCases]::Run(('Lookup_'+$v.name-as[type]))}catch{$killed++;continue};throw ('Unsafe lookup survived '+$v.name)}
Write-Output "PASS quota local body lookup actual_cases=$cases unsafe_controls=$killed permissions_unchanged=1 native=NOT_RUN"

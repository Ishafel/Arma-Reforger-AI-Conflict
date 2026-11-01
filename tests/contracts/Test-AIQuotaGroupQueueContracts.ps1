param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$path=Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Forces/AICF_AIQuotaGroupQueue.c'
$source=[IO.File]::ReadAllText($path)
function Extract([string]$name){Get-AICFMethodBody ([pscustomobject]@{Source=$source;Code=(ConvertTo-AICFCodeText $source)}) $name}
$methods='public static int RequestedUnits(SCR_AIGroup group,int slotsWanted)'+(Extract 'RequestedUnits')
foreach($name in @('Available','EnsureAvailable','BeginDispatch','CancelAvailable')){
 $prefix='public int Available()'
 if($name-eq'EnsureAvailable'){$prefix='public bool EnsureAvailable(AICF_AIQuotaLedger ledger,int required)'}
 if($name-eq'BeginDispatch'){$prefix='public AICF_AIQuotaDispatch BeginDispatch(AICF_AIQuotaLedger ledger)'}
 if($name-eq'CancelAvailable'){$prefix='public void CancelAvailable(AICF_AIQuotaLedger ledger)'}
 $methods+=$prefix+(Extract $name)
}
$methods=[regex]::Replace($methods,'foreach \((\w+) (\w+) : (\w+)\)','foreach ($1 $2 in $3)')
$methods=$methods.Replace('int available;','int available=0;').Replace('int sum;','int sum=0;').Replace('MAX_TOTAL_UNITS, sum)','MAX_TOTAL_UNITS, out sum)')
$methods=$methods.Replace('m_aBaskets.Remove(index)','m_aBaskets.RemoveAt(index)')
$code=@'
using System;using System.Collections.Generic;
public class N {public static implicit operator bool(N value){return value!=null;}}
public class Arr<T>:List<T>{public static implicit operator bool(Arr<T> value){return value!=null;}public new int Count(){return base.Count;}public bool IsEmpty(){return base.Count==0;}public void Insert(T value){Add(value);}}
public static class AICF_AIQuotaMath{public const int MAX_TOTAL_UNITS=4096;public static bool AddBounded(int a,int b,int max,out int result){result=0;if(a<0||b<0||a>max||b>max-a)return false;result=a+b;return true;}}
public class SCR_AIGroup:N {public Arr<string> m_aUnitPrefabSlots=new Arr<string>();public int Actual,Alive=-1,Dead;public int GetAgentsCount(){return Actual;}public int GetDormantAliveCount(){return Alive;}public int GetDormantDeadCount(){return Dead;}}
public class AICF_AIQuotaReservation:N {public int m_iOwnerSerial,m_iAvailable;public bool m_bCancelled;}
public class AICF_AIQuotaDispatch:N {public AICF_AIQuotaReservation Reservation;}
public class AICF_AIQuotaLedger:N {public int Free=128,Held,Calls,Refunded;public Action AfterReserve;public AICF_AIQuotaReservation Reserve(int bucket,int n,int serial){Calls++;if(n<=0||n>Free)return null;Free-=n;Held+=n;var result=new AICF_AIQuotaReservation{m_iOwnerSerial=serial,m_iAvailable=n};if(AfterReserve!=null)AfterReserve();return result;}public bool CancelAvailable(AICF_AIQuotaReservation r,int serial){if(r.m_iOwnerSerial!=serial)return false;Free+=r.m_iAvailable;Held-=r.m_iAvailable;Refunded+=r.m_iAvailable;r.m_iAvailable=0;r.m_bCancelled=true;return true;}public AICF_AIQuotaDispatch BeginDispatch(AICF_AIQuotaReservation r,int serial){if(r.m_bCancelled||r.m_iOwnerSerial!=serial||r.m_iAvailable<=0)return null;r.m_iAvailable--;return new AICF_AIQuotaDispatch{Reservation=r};}}
public class GroupQueue:N {public bool Current=true;public int m_iSerial=1;public Arr<AICF_AIQuotaReservation> m_aBaskets=new Arr<AICF_AIQuotaReservation>();public bool IsCurrent(){return Current;}public int GetBucket(){return 0;}
__METHODS__
}
public static class Cases{public static int Execute(){int count=0;Action<bool,string> need=(ok,label)=>{if(!ok)throw new Exception(label);count++;};
var g=new SCR_AIGroup();for(int i=0;i<10;i++)g.m_aUnitPrefabSlots.Add("actor");
need(GroupQueue.RequestedUnits(g,-1)==10,"first request1 reserves full authored10");g.Actual=1;need(GroupQueue.RequestedUnits(g,-1)==9,"partial native1 retains9 not duplicate10");g.Alive=10;need(GroupQueue.RequestedUnits(g,-1)==9,"recorded10 and actual1 reserves9");g.Dead=2;need(GroupQueue.RequestedUnits(g,-1)==7,"native dead slots never resurrected");g.Alive=0;need(GroupQueue.RequestedUnits(g,-1)==0,"eliminated dormant group no resurrection");need(GroupQueue.RequestedUnits(g,0)==0,"zero request remains no-op");g.Actual=9;need(GroupQueue.RequestedUnits(g,1)==-1,"actual exceeds live capacity unknown");need(GroupQueue.RequestedUnits(null,1)==-1,"null group unknown");need(GroupQueue.RequestedUnits(g,-2)==-1,"invalid request negative unknown");
g=new SCR_AIGroup();need(GroupQueue.RequestedUnits(g,-1)==0,"empty authored roster never fabricates a first member");for(int i=0;i<10;i++)g.m_aUnitPrefabSlots.Add("actor");g.Actual=10;need(GroupQueue.RequestedUnits(g,5)==0,"fulfilled native group request is no-op");g.Actual=3;need(GroupQueue.RequestedUnits(g,10)==7,"requested count cannot exceed remaining authored slots");var q=new GroupQueue();var l=new AICF_AIQuotaLedger();need(q.EnsureAvailable(l,10)&&l.Held==10&&l.Calls==1,"one full basket before queue");need(q.EnsureAvailable(l,10)&&l.Calls==1,"repeat admission no double reservation");var dispatch=q.BeginDispatch(l);need(dispatch!=null&&q.Available()==9&&l.Held==10,"native inflight remains held separate from available");need(q.EnsureAvailable(l,9)&&l.Calls==1,"actual materialization remaining9 no duplicate reserve");q.CancelAvailable(l);need(l.Held==1&&l.Refunded==9&&q.Available()==0,"purge refunds only available retains inflight");q.CancelAvailable(l);need(l.Held==1&&l.Refunded==9,"repeat cancel no second refund");
q=new GroupQueue();l=new AICF_AIQuotaLedger{Free=9};need(!q.EnsureAvailable(l,10)&&l.Held==0&&q.Available()==0,"full basket veto no partial9");q.Current=false;l.Free=128;need(!q.EnsureAvailable(l,1)&&l.Held==0,"stale owner no reservation");q=new GroupQueue();l.AfterReserve=()=>q.Current=false;need(!q.EnsureAvailable(l,10)&&l.Held==0&&l.Free==128,"callback owner changed refunds exact basket");
q=new GroupQueue();l=new AICF_AIQuotaLedger();for(int i=0;i<5000;i++){q.Current=true;need(q.EnsureAvailable(l,1),"bounded repeated reservation");q.CancelAvailable(l);}need(q.m_aBaskets.Count()<=1&&l.Held==0,"retired baskets do not impose lifetime4096 cap");return count;}}
'@
$code=$code.Replace('__METHODS__',$methods)
Add-Type $code
$count=[Cases]::Execute()
$unsafe=0
foreach($mutation in @(
 @('remaining-double-count','int remaining = capacity - actual;','int remaining = capacity;'),
 @('first-member-only','required - available, m_iSerial','1, m_iSerial'),
 @('stale-after-reserve','!extra || !IsCurrent()','!extra'),
 @('cancel-keeps-available','if (basket) ledger.CancelAvailable(basket, m_iSerial);','{}'),
 @('lifetime-cap','m_aBaskets.RemoveAt(index);','{}')
)){
 if(!$code.Contains($mutation[1])){throw "Missing actual mutation $($mutation[0])"}
 $name='GroupUnsafe'+[guid]::NewGuid().ToString('N')
 $mutated=$code.Replace($mutation[1],$mutation[2]).Replace('public class N {',"namespace $name {public class N {")+'}'
 Add-Type $mutated
 $rejected=$false;try{$null=([type]("$name.Cases")).GetMethod('Execute').Invoke($null,@())}catch{$rejected=$true}
 if(!$rejected){throw "Unsafe group queue survived $($mutation[0])"};$unsafe++
}
"PASS AI quota actual group queue cases=$count executed_unsafe=$unsafe partial_materialized_basket=1 runtime_native_intended=NOT_RUN"

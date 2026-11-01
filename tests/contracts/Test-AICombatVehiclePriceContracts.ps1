param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$root=Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict'
function Read([string]$p){[IO.File]::ReadAllText((Join-Path $root $p))}
function Body([string]$s,[string]$m){Get-AICFMethodBody ([pscustomobject]@{Source=$s;Code=(ConvertTo-AICFCodeText $s)}) $m}
$policy=Read 'Economy/AICF_AIVehiclePricePolicy.c';$catalog=Read 'Vehicles/AICF_VehiclePresetCatalog.c';$economy=Read 'Economy/AICF_EconomySystem.c'
$combat=(Body $policy 'IsCombat').Replace('return entry &&','return entry != null &&')
$quote=Body $policy 'Quote'
$purchase=(Body $economy 'PurchaseVehiclePreset') -replace '\bbase\b','baseEntity'
$pay=(Body $economy 'CanPayVehiclePreset') -replace '\bbase\b','baseEntity'
$list=Body $catalog 'List'
$cost=Body $catalog 'Cost'
$rollback=(Body $economy 'RollbackVehiclePreset') -replace '\bbase\b','baseEntity'
$shim=@'
using System;using System.Collections.Generic;
namespace __NS__ {
public static class EEditableEntityLabel {public const int TRAIT_ARMED=21,TRAIT_ARMOR=20,VEHICLE_APC=53,TRAIT_UNARMED=22;}
public class SCR_EntityCatalogEntry {public HashSet<int> Labels=new HashSet<int>();public bool HasEditableEntityLabel(int label){return Labels.Contains(label);}}
public static class AICF_AIVehiclePricePolicy {public static bool IsCombat(SCR_EntityCatalogEntry entry){__COMBAT__}public static int Quote(int nativeCost,bool combat){__QUOTE__}}
public class Native {public static implicit operator bool(Native v){return v!=null;}}
public static class Text {public static bool IsEmpty(this string s){return string.IsNullOrEmpty(s);}}
public static class Replication {public static bool Server=true;public static bool IsServer(){return Server;}}
public class SCR_CampaignFaction:Native{}
public class SCR_CampaignMilitaryBaseComponent:Native {public int Stock;public int Fault;public int Calls;public int GetSupplies(){return Stock;}public void AddSupplies(int delta){Calls++;Stock+=delta;if(delta<0)Stock+=Fault;}}
public class Preset:Native {public string m_sPrefab="combat";}
public class AICF_TransportTrip:Native {public bool Current=true,Debited;public Preset Preset=new Preset();public bool IsVehiclePresetCurrent(){return Current;}public Preset GetVehiclePreset(){return Preset;}public bool IsVehiclePresetDebited(){return Debited;}}
public class Arr<T>:List<T>{public int Find(T value){return IndexOf(value);}} public class JsonSaveContext {public static Dictionary<string,object> Values;public JsonSaveContext(){Values=new Dictionary<string,object>();}public void WriteValue(string key,object value){Values[key]=value;}public string SaveToString(){return "encoded";}} public class AICF_VehiclePresetCatalog {public static int Raw=751;public static bool Combat=true;public Arr<string> m_aPrefabs=new Arr<string>{"combat"},m_aNames=new Arr<string>{"combat"};public Arr<int> m_aCosts=new Arr<int>(),m_aSources=new Arr<int>{1},m_aCategories=new Arr<int>{1};public AICF_VehiclePresetCatalog(SCR_CampaignFaction f){m_aCosts.Add(AICF_AIVehiclePricePolicy.Quote(Raw,Combat));}public int Cost(string prefab){__COST__}public string List(){__LIST__}}
public class Network {public bool Owned=true;public bool IsOperationalOwnedBase(SCR_CampaignMilitaryBaseComponent b,SCR_CampaignFaction f){return Owned;}}
public class Adapter {public string Rejection="";public string GetSpawnRejectionReason(SCR_CampaignMilitaryBaseComponent b,SCR_CampaignFaction f){return Rejection;}}
public class Economy {
 public bool Enabled=true,m_bGraphContextReady=true;public Native m_Graph=new Native();public Network m_SupplyNetwork=new Network();public Adapter m_ConflictAdapter=new Adapter();public int Obligations;
 public bool IsEnabled(){return Enabled;}public int LogisticsObligations(SCR_CampaignMilitaryBaseComponent b){return Obligations;}
 public bool CanPayVehiclePreset(SCR_CampaignFaction faction,SCR_CampaignMilitaryBaseComponent baseEntity,int cost){__PAY__}
 public bool PurchaseVehiclePreset(AICF_TransportTrip trip,SCR_CampaignFaction faction,SCR_CampaignMilitaryBaseComponent baseEntity,string prefab,out int paid){__PURCHASE__}
 public void RollbackVehiclePreset(SCR_CampaignMilitaryBaseComponent baseEntity,int paid){__ROLLBACK__}
}
public static class Suite {
 static int N;static void Need(bool ok,string rule){N++;if(!ok)throw new Exception(rule);}
 public static int Run(){
 int[] values={-1,0,1,2,3,4,5,6,7,8,50,125,250,3500,3750,int.MaxValue};
 foreach(int value in values){int expected=value;if(value>0)expected=(int)Math.Max(1L,((long)value+1L)/3L);Need(AICF_AIVehiclePricePolicy.Quote(value,true)==expected,"exact nearest rounding and positive minimum");Need(AICF_AIVehiclePricePolicy.Quote(value,false)==value,"unarmed prices unchanged");}
 foreach(int label in new[]{21,20,53}){var e=new SCR_EntityCatalogEntry();e.Labels.Add(label);Need(AICF_AIVehiclePricePolicy.IsCombat(e),"native combat label admitted");}
 var unarmed=new SCR_EntityCatalogEntry();unarmed.Labels.Add(22);Need(!AICF_AIVehiclePricePolicy.IsCombat(unarmed)&&!AICF_AIVehiclePricePolicy.IsCombat(null),"unarmed and unknown labels not discounted");
 var uiCatalog=new AICF_VehiclePresetCatalog(new SCR_CampaignFaction());uiCatalog.List();Need(((Arr<int>)JsonSaveContext.Values["costs"])[0]==uiCatalog.Cost("combat")&&uiCatalog.Cost("combat")==250,"actual UI List serializes same AI price array as payment");
 var f=new SCR_CampaignFaction();var b=new SCR_CampaignMilitaryBaseComponent{Stock=300};var t=new AICF_TransportTrip();var ecn=new Economy{Obligations=50};int paid;
 Need(ecn.CanPayVehiclePreset(f,b,AICF_AIVehiclePricePolicy.Quote(751,true)),"rounded discounted quote fits after obligations");
 Need(ecn.PurchaseVehiclePreset(t,f,b,"combat",out paid)&&paid==250&&b.Stock==50&&b.Calls==1,"actual purchase debits quoted AI price exactly once");
 Need(AICF_VehiclePresetCatalog.Raw==751,"native human quote remains unchanged");ecn.RollbackVehiclePreset(b,paid);Need(b.Stock==300,"rollback restores actual discounted debit");
 b.Stock=299;Need(!ecn.PurchaseVehiclePreset(t,f,b,"combat",out paid)&&paid==0&&b.Stock==299,"budget floor still respects obligations");b.Stock=300;
 t.Debited=true;Need(!ecn.PurchaseVehiclePreset(t,f,b,"combat",out paid)&&b.Stock==300,"already paid identity rejected");t.Debited=false;
 t.Current=false;Need(!ecn.PurchaseVehiclePreset(t,f,b,"combat",out paid),"stale assignment rejected");t.Current=true;
 Need(!ecn.PurchaseVehiclePreset(t,f,b,"foreign",out paid),"foreign prefab rejected");
 b.Fault=1;Need(!ecn.PurchaseVehiclePreset(t,f,b,"combat",out paid)&&paid==0&&b.Stock==300,"failed native debit restores original stock");b.Fault=0;
 Replication.Server=false;Need(!ecn.PurchaseVehiclePreset(t,f,b,"combat",out paid)&&b.Stock==300,"client cannot pay");Replication.Server=true;
 AICF_VehiclePresetCatalog.Combat=false;Need(!ecn.PurchaseVehiclePreset(t,f,b,"combat",out paid)&&b.Stock==300,"unarmed original price not made affordable");
 return N;
 }
}
}
'@
$shim=$shim.Replace('__COMBAT__',$combat).Replace('__QUOTE__',$quote).Replace('__PAY__',$pay).Replace('__PURCHASE__',$purchase).Replace('__ROLLBACK__',$rollback).Replace('__COST__',$cost).Replace('__LIST__',$list)
$ns='AIPrices'+[guid]::NewGuid().ToString('N');Add-Type -TypeDefinition $shim.Replace('__NS__',$ns)
$count=([type]($ns+'.Suite'))::Run()
$append=Body $catalog 'Append'
if(!$append.Contains('AICF_AIVehiclePricePolicy.Quote(EntryCost(entry, resource), AICF_AIVehiclePricePolicy.IsCombat(entry))')){throw 'Shared AI catalog transformation missing'}
if(!(Body $catalog 'Cost').Contains('return m_aCosts[index]') -or !(Body $catalog 'List').Contains('json.WriteValue("costs", m_aCosts)')){throw 'UI quote and payment catalog must share transformed array'}
foreach($m in @('InspectVehiclePresetPreflight','IsVehiclePresetPreflightCurrent','CanAffordVehiclePreset','PurchaseVehiclePreset')){if(!(Body $economy $m).Contains('catalog.Cost(')){throw "AI economy path bypasses catalog: $m"}}
if(!(Body (Read 'Command/AICF_AICommander.c') 'GetMaximumVehiclePresetCost').Contains('catalog.Cost(preset.m_sPrefab)')){throw 'Supply capacity demand bypasses AI quote'}
$flow=Read 'Vehicles/AICF_VehicleAcquisitionFlow.c'
if(!$flow.Contains('catalog.Cost(candidate.m_Prefab)') -or !$flow.Contains('PurchaseVehiclePreset(trip, faction, selection.m_Base, candidate.m_Prefab, paid)')){throw 'Acquisition preflight/commit bypasses shared AI quote'}
$human=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'AIConflictArlandRHS/Scripts/Game/AIConflictArlandRHS/Integration/AICF_RHSMenuPricePolicy.c'))+[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'AIConflictArlandRHS/Scripts/Game/AIConflictArlandRHS/Integration/AICF_RHSMenuPriceIntegration.c'))
if($human.Contains('AICF_AIVehiclePricePolicy')){throw 'Native player price must not consume AI discount'}
$unsafe=@(
 @{source=$shim.Replace('nativeCost % 3 == 2','nativeCost % 3 == 1');rule='exact nearest rounding and positive minimum'},
 @{source=$shim.Replace('nativeCost <= 0 || !combat','nativeCost <= 0');rule='unarmed prices unchanged'},
 @{source=$shim.Replace('Math.Max(1, reduced)','reduced');rule='exact nearest rounding and positive minimum'},
 @{source=$shim.Replace('baseEntity.AddSupplies(-cost);','baseEntity.AddSupplies(-AICF_VehiclePresetCatalog.Raw);');rule='actual purchase debits quoted AI price exactly once'}
)
foreach($v in $unsafe){$bad='AIPricesUnsafe'+[guid]::NewGuid().ToString('N');Add-Type -TypeDefinition $v.source.Replace('__NS__',$bad);$caught=$false;try{[void]([type]($bad+'.Suite'))::Run()}catch{if($_.Exception.ToString().Contains($v.rule)){$caught=$true}else{throw}};if(!$caught){throw "Unsafe variant accepted: $($v.rule)"}}
"PASS AI combat vehicle prices actual cases=$count unsafe=$($unsafe.Count); shared paid AI paths, UI, supply demand and human boundary retained"

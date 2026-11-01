param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$source=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Vehicles/AICF_VehicleTaskHandoff.c'))
$record=[pscustomobject]@{Source=$source;Code=(ConvertTo-AICFCodeText $source)}
$format=Get-AICFMethodBody $record 'FormatIdentity'
$report=Get-AICFMethodBody $record 'ReportOrderRestoreResult'
if(!$report.Contains('FormatIdentity(trip, restoreReason)')){throw 'ORDER_RESTORE_RESULT no longer uses tested formatter'}
if($format-match'\.(?:Set\w+|Delete\w*|Remove\w*|Activate\w*|GetVehicle)\s*\('){throw 'Identity log performs gameplay or weak vehicle access'}
function ExecuteFormatter([string]$body){
 $body=[regex]::Replace($body,'%([1-9])',{param($m) '{'+([int]$m.Groups[1].Value-1)+'}'})
 $body=$body.Replace('typename.EnumToString(AICF_EVehicleKind,','Native.EnumToString(typeof(AICF_EVehicleKind),').Replace('typename.EnumToString(AICF_ETransportTripPhase,','Native.EnumToString(typeof(AICF_ETransportTripPhase),')
 $code=@'
using System;
public class N {public static implicit operator bool(N value){return !Object.ReferenceEquals(value,null);}}
public static class Native {public static string EnumToString(Type type,object value){return Enum.GetName(type,value);}}
public static class Text {public static bool IsEmpty(this string value){return String.IsNullOrEmpty(value);}}
public enum AICF_EVehicleKind {TRANSPORT,ARMED_LIGHT} public enum AICF_ETransportTripPhase {TRANSIT,FALLBACK}
public class AICF_VehicleLease:N {public int Generation=3,VehicleGeneration=4;public string Rpl="R1",Entity="0xA";public int GetLeaseGeneration(){return Generation;}public int GetVehicleGeneration(){return VehicleGeneration;}public string GetVehicleLifecycleId(){return "life";}public string GetEntityIdString(){return Entity;}public string GetRplId(){return Rpl;}public AICF_EVehicleKind GetKind(){return AICF_EVehicleKind.ARMED_LIGHT;}public object GetVehicle(){throw new Exception("formatter must use captured lease IDs after physical deletion");}}
public class AICF_TransportTrip:N {public AICF_VehicleLease Lease=new AICF_VehicleLease();public string GetFactionKey(){return "US";}public string GetSlotKey(){return "A0";}public int GetSlotId(){return 0;}public int GetGroupGeneration(){return 1;}public int GetTripGeneration(){return 2;}public string GetOperationId(){return "trip";}public string GetCausationId(){return "cause";}public AICF_VehicleLease GetLease(){return Lease;}public AICF_ETransportTripPhase GetPhase(){return AICF_ETransportTripPhase.FALLBACK;}}
public static class Actual {public static string Format(AICF_TransportTrip trip,string reason) __BODY__
public static int Run(){int count=0;Action<bool,string> check=(ok,label)=>{if(!ok)throw new Exception(label);count++;};var trip=new AICF_TransportTrip();var line=Format(trip,"FIRE");foreach(var token in new[]{"faction=US","numeric_slot=0","group_generation=1","trip_generation=2","operation_id=trip","causation_id=cause","lease_generation=3","vehicle_generation=4","vehicle_lifecycle_id=life","reason=FIRE","vehicle=0xA","rpl_id=R1","kind=ARMED_LIGHT","state=FALLBACK"})check(line.Contains(token),"missing actual field "+token);check(line.IndexOf("rpl_id=")==line.LastIndexOf("rpl_id="),"one exact Rpl field");trip.Lease.Rpl="R2";check(Format(trip,"FIRE").Contains("rpl_id=R2"),"current captured lease Rpl");trip.Lease.Rpl="";check(Format(trip,"FIRE").Contains("rpl_id=NONE"),"unknown Rpl explicit");trip.Lease.Entity="";check(Format(trip,"FIRE").Contains("vehicle=NONE"),"unknown entity explicit");trip.Lease=null;check(Format(trip,"FIRE").Contains("rpl_id=NONE"),"no lease no invented Rpl");check(Format(null,"FIRE").Contains("rpl_id=NONE"),"no trip no invented Rpl");return count;}}
'@
 $namespace='AICFHandoffLog'+[guid]::NewGuid().ToString('N')
 $code=$code.Replace('__BODY__',$body).Replace('public class N {',"namespace $namespace {public class N {")+'}'
 Add-Type $code
 return [type]("$namespace.Actual")
}
$type=ExecuteFormatter $format
$cases=$type.GetMethod('Run').Invoke($null,@())
$oldGetter=$format.Replace('vehicleRplId = lease.GetRplId();','vehicleRplId = "NONE";')
$unsafeType=ExecuteFormatter $oldGetter
$rejected=$false;try{$null=$unsafeType.GetMethod('Run').Invoke($null,@())}catch{$rejected=$true}
if(!$rejected){throw 'Missing live lease Rpl did not fail actual formatter controls'}
"PASS vehicle handoff identity log actual_fields=$cases unsafe=1 readonly_lease_snapshot=1 current_native=NOT_RUN"

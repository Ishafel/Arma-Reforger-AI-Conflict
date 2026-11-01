param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$core=Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict'
$ui=[IO.File]::ReadAllText((Join-Path $core 'UI/AICF_StrategicUI.c'))
$controller=[IO.File]::ReadAllText((Join-Path $core 'Bootstrap/AICF_MatchController.c'))
function Body($text,$name){Get-AICFMethodBody ([pscustomobject]@{Source=$text;Code=(ConvertTo-AICFCodeText $text)}) $name}
function Adapt($body){
 $body=$body.Replace('array<string> fields = {};','List<string> fields = new List<string>();').Replace('summary.Split("|", fields, false);','fields.AddRange(summary.Split(new[]{"|"},StringSplitOptions.None));').Replace('fields.Count()','fields.Count')
 $body=$body.Replace('!fields[14].IsEmpty()','fields[14].Length != 0').Replace('!slot ||','slot == null ||').Replace('!preset)','preset == null)')
 $body=$body.Replace('string.Format','SF.Format')
 $body=[regex]::Replace($body,'(?m)^(\s*)name.Replace\(', '$1name = name.Replace(')
 return $body
}
$producer=Body $controller 'BuildGroupSummary'
if(!$producer.Contains('summary += "|" + AICF_VehiclePresetSummary.Field(slot);')){throw 'Missing optional field14 from actual assigned slot'}
if($ui -notmatch 'SetText\(FormatGroupSummary\(summary\)\)' -or $ui -match 'SetText\(AICF_Localization.Resolve\(FormatGroupSummary\(summary\)\)\)'){throw 'Group row applies another Resolve after literal name'}
$field=Body $ui 'Field'
if($field -notmatch '!slot \|\| !slot.HasVehiclePreset\(\)' -or $field -notmatch 'Decode\(slot.GetVehiclePresetData\(\)\)' -or $field -match 'Draft|Store|SetVehiclePreset|SetOrigin|BumpMe|Revision\+\+'){throw 'Field must read assigned valid recipe only'}
$template=@'
using System;using System.Collections.Generic;
public static class SF{public static string Format(string text,params object[] values){for(int i=values.Length;i>0;i--)text=text.Replace("%"+i,Convert.ToString(values[i-1]));return text;}}
public class AICF_GroupSlot{public string Data="";public bool HasVehiclePreset(){return Data.Length!=0;}public string GetVehiclePresetData(){return Data;}}
public class AICF_VehiclePreset{public string m_sName;public static Dictionary<string,AICF_VehiclePreset> Valid=new Dictionary<string,AICF_VehiclePreset>();public static AICF_VehiclePreset Decode(string data){return Valid.ContainsKey(data)?Valid[data]:null;}}
public static class AICF_Localization{
 public static string Code(string text){return text;}
 public static string Format(string key,params string[] values){return "\n"+String.Join(" / ",values);}
 public static string Resolve(string text){return text.Replace("{AICF:AICF_UI_CommandVehiclePreset}","Vehicle preset").Replace("{AICF:literal}","WRONGLY_TRANSLATED");}
}
public static class AICF_VehiclePresetSummary{
 public static string Field(AICF_GroupSlot slot) FIELD
 public static string EncodeName(string name) ENCODE
 public static string DecodeName(string name) DECODE
}
public static class SummaryUI{
 public static string FormatGroupSummary(string summary) FORMAT
}
public static class ActualTests{
 public static int Run(){int n=0;Action<bool,string> need=(ok,label)=>{n++;if(!ok)throw new Exception(label);};
 string legacy="A0|ATTACK|WAITING|0|NONE|HEALTHY|On foot|-|INFANTRY|10|NONE|0|0|7";
 string baseText=SummaryUI.FormatGroupSummary(legacy);
 need(!baseText.Contains("Vehicle preset"),"legacy14 has no preset label");
 need(SummaryUI.FormatGroupSummary(legacy+"|")==baseText,"empty15 has no preset label");
 need(SummaryUI.FormatGroupSummary("bad")=="UNAVAILABLE","short legacy summary is unavailable");
 need(AICF_VehiclePresetSummary.Field(null)=="","no slot");
 var slot=new AICF_GroupSlot();need(AICF_VehiclePresetSummary.Field(slot)=="","unassigned slot");
 slot.Data="malformed";need(AICF_VehiclePresetSummary.Field(slot)=="","invalid stored recipe does not display");
 var names=new[]{"Ural transport","\u0411\u0440\u043E\u043D\u044F \u043E\u0442\u0440\u044F\u0434\u0430","%|~;{}","%7C %25 %7B","{AICF:literal}","#AICF_UI_CommandVehiclePreset","name:1,2 & < > \\n","",new String('\u0416',80),"|\u043D\u0430\u0447\u0430\u043B\u043E;{AICF:literal}~\u043A\u043E\u043D\u0435\u0446%"};
 foreach(string name in names){
  string encoded=AICF_VehiclePresetSummary.EncodeName(name);
  need(AICF_VehiclePresetSummary.DecodeName(encoded)==name,"codec roundtrip: "+name);
  need(!encoded.Contains("|")&&!encoded.Contains("~")&&!encoded.Contains(";")&&!encoded.Contains("{")&&!encoded.Contains("}"),"wire delimiters protected");
  string wire=legacy+"|"+encoded;need(wire.Split('|').Length==15,"wire stays15fields");
  string rendered=SummaryUI.FormatGroupSummary(wire);
  if(name.Length>0)need(rendered==baseText+"\nVehicle preset: "+name,"assigned name remains literal after one base Resolve");
  else need(rendered==baseText,"empty name has no label");
  AICF_VehiclePreset.Valid["confirmed"]=new AICF_VehiclePreset{m_sName=name};slot.Data="confirmed";
  need(AICF_VehiclePresetSummary.Field(slot)==encoded,"field uses confirmed assigned recipe");
  need(slot.Data=="confirmed","field read does not change binding");
  slot.Data="";need(AICF_VehiclePresetSummary.Field(slot)=="","reset removes field");
  need(SummaryUI.FormatGroupSummary(legacy+"|"+AICF_VehiclePresetSummary.Field(slot))==baseText,"no stale name after reset");
 }
 need(AICF_VehiclePresetSummary.EncodeName("%|~;{}")=="%25%7C%7E%3B%7B%7D","exact reserved encoding");
 return n;
 }
}
'@
$template=$template.Replace('FIELD',(Adapt $field)).Replace('ENCODE',(Adapt (Body $ui 'EncodeName'))).Replace('DECODE',(Adapt (Body $ui 'DecodeName'))).Replace('FORMAT',(Adapt (Body $ui 'FormatGroupSummary')))
Add-Type -TypeDefinition $template
$actual=[ActualTests]::Run()
$mutants=@(
 @('name = name.Replace("%", "%25");',''),
 @('name = name.Replace("|", "%7C");',''),
 @('name = name.Replace("{", "%7B");',''),
 @('name = name.Replace("%7C", "|");','name = name.Replace("%25", "%"); name = name.Replace("%7C", "|");'),
 @('return result;','return AICF_Localization.Resolve(result);'),
 @('if (fields.Count > 14 && fields[14].Length != 0)','if (fields.Count > 99999)'),
 @('AICF_VehiclePresetSummary.DecodeName(fields[14])','fields[14]')
)
$caught=0
foreach($change in $mutants){
 $code=$template.Replace($change[0],$change[1]);if($code -ceq $template){throw "Mutation missing $($change[0])"}
 $namespace='PresetSummaryUnsafe'+$caught
 Add-Type -TypeDefinition ('namespace '+$namespace+' {'+$code+'}')
 $rejected=$false;try{([type]($namespace+'.ActualTests')).GetMethod('Run').Invoke($null,@())|Out-Null}catch{$rejected=$true}
 if(!$rejected){throw "Actual unsafe summary mutant escaped: $($change[0])"};$caught++
}
"PASS vehicle preset summary: actual_codec_field_formatter_cases=$actual unsafe_mutants=$caught producer_guards=3 legacy14=preserved assigned_only=1 native=NOT_RUN"

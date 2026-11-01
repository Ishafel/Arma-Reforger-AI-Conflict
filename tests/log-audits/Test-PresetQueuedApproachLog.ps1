param([string]$LogPath,[string]$LauncherPath,[string]$NativeExitPath,[string]$ReportPath,[switch]$RequireHealth)
$ErrorActionPreference='Stop'
function Get-AICFPresetQueuedApproachVerdict([string]$Log,[string]$All,[string]$Launcher,[string]$NativeExit,[bool]$RequireHealth)
{
	$failures=[Collections.Generic.List[string]]::new()
	function Need([bool]$ok,[string]$rule){if(!$ok){$failures.Add($rule)}}
	function Value([string]$line,[string]$key){$m=[regex]::Match($line,'(?:^|\s)'+[regex]::Escape($key)+'=(\S+)');if($m.Success){return $m.Groups[1].Value};return ''}
	Need ($NativeExit.Trim() -eq '0') 'NATIVE_EXIT'
	Need ($Log.Contains('Game destroyed.') -and $Log.Contains('[ROSTER_READY]')) 'STOPPED_ROSTER'
	$manifests=[regex]::Matches($Launcher,'(?m)^AICF_RUNTIME_MANIFEST_JSON=([^\r\n]+)')
	Need ($manifests.Count -eq 1) 'MANIFEST'
	if($manifests.Count -eq 1)
	{
		try{
			$argv=@(($manifests[0].Groups[1].Value|ConvertFrom-Json).arguments)
			$cli=[regex]::Matches($Log,'(?m)CLI Params:\s*([^\r\n]+)')
			Need ($cli.Count -eq 1 -and $cli[0].Groups[1].Value.Trim() -ceq ($argv -join ' ')) 'EXACT_CLI'
			foreach($flag in @('-gproj','-profile','-addonsDir','-addons','-aicfVehiclePresetProbe','-aicfVehiclePresetCrew','-aicfPresetQueuedApproachProbe'))
			{
				$indices=@(for($i=0;$i -lt $argv.Count;$i++){if($argv[$i] -eq $flag){$i}})
				Need ($indices.Count -eq 1 -and $indices[0]+1 -lt $argv.Count -and ![string]::IsNullOrWhiteSpace($argv[$indices[0]+1])) ('CLI_'+$flag)
			}
			foreach($flag in @('-aicfVehiclePresetProbe','-aicfPresetQueuedApproachProbe')){$i=[array]::IndexOf($argv,$flag);Need ($i -ge 0 -and $i+1 -lt $argv.Count -and $argv[$i+1] -eq '1') ('VALUE_'+$flag)}
			$i=[array]::IndexOf($argv,'-aicfVehiclePresetCrew');Need ($i -ge 0 -and $i+1 -lt $argv.Count -and $argv[$i+1] -eq 'unarmed') 'UNARMED_FOUR'
		}catch{$failures.Add('MALFORMED_MANIFEST')}
	}
	$start=@([regex]::Matches($Log,'\[PREQUEUE_PROBE_START\] ([^\r\n]+)'))
	$walk=@([regex]::Matches($Log,'\[PREQUEUE_PROBE_WALK\] ([^\r\n]+)'))
	$entry=@([regex]::Matches($Log,'\[PREQUEUE_PROBE_ENTRY\] ([^\r\n]+)'))
	Need ($start.Count -eq 1 -and $walk.Count -eq 1 -and $entry.Count -eq 1) 'UNIQUE_COMPLETE_CAUSAL_CHAIN'
	if($start.Count -eq 1 -and $walk.Count -eq 1 -and $entry.Count -eq 1)
	{
		$s=$start[0].Groups[1].Value;$w=$walk[0].Groups[1].Value;$e=$entry[0].Groups[1].Value
		foreach($key in @('operation_id','member')){Need ((Value $s $key) -ne '' -and (Value $s $key) -eq (Value $w $key) -and (Value $s $key) -eq (Value $e $key)) ('IDENTITY_'+$key)}
		Need ($start[0].Index -lt $walk[0].Index -and $walk[0].Index -lt $entry[0].Index) 'CAUSAL_ORDER'
		try{$motion=[double](Value $w 'displacement_m');$polls=[int](Value $w 'native_polls');Need (![double]::IsNaN($motion) -and ![double]::IsInfinity($motion) -and $motion -ge 10 -and $polls -ge 3) 'ACTUAL_NATIVE_WALK'}catch{$failures.Add('INVALID_MOTION')}
		foreach($pair in @(@('entry_queued','0'),@('deadline_unchanged','1'),@('original_member','1'),@('pose_writes','0'))){Need ((Value $w $pair[0]) -eq $pair[1]) ('WALK_'+$pair[0])}
		if($RequireHealth){Need ((Value $w 'health_unchanged') -eq '1') 'HEALTH_UNCHANGED'}
		foreach($pair in @(@('accepted','1'),@('deadline_unchanged','1'),@('native_walk_before_entry','1'))){Need ((Value $e $pair[0]) -eq $pair[1]) ('ENTRY_'+$pair[0])}
		$member=[regex]::Escape((Value $s 'member'));$operation=[regex]::Escape((Value $s 'operation_id'))
		Need ($Log -match ('\[VEHICLE_PRESET_OCCUPANT\][^\r\n]*operation='+$operation+'\s[^\r\n]*member='+$member+' occupant='+$member+'(?:\s|$)')) 'SAME_MEMBER_EXACT_OCCUPANT'
	}
	Need (@([regex]::Matches($Log,'\[VEHICLE_PRESET_CHECK\] rule=EXACT_OCCUPANT_\d+ passed=1')).Count -eq 4) 'FOUR_EXACT_OCCUPANTS'
	Need ($Log -match '\[VEHICLE_PRESET_DONE\] checks=\d+ failures=0') 'BASE_PROBE_DONE'
	Need ($Log -notmatch '\[VEHICLE_PRESET_CHECK\][^\r\n]*passed=0') 'NO_BASE_FAILURE'
	$fatal=[bool]($All -match 'SCRIPT\s+\([EF]\)|ENGINE\s+\(F\)|NULL pointer|Virtual Machine Exception|Assertion failed|Attempt to modify locked entity|Access violation')
	$resources=@([regex]::Matches($All,'(?m)^.*\(E\).*$')).Count
	$functional=$failures.Count -eq 0
	$verdict='FAIL';if($functional -and !$fatal -and $resources -eq 0){$verdict='PASS'}
	[pscustomobject]@{verdict=$verdict;functional_pass=$functional;functional_failures=@($failures);fatal_pass=!$fatal;all_native_error_lines=$resources;strict_resources_pass=($resources -eq 0)}
}
if($MyInvocation.InvocationName -eq '.'){return}
if(!$LogPath -or !$LauncherPath -or !$NativeExitPath){throw 'Complete stopped logs, launcher and native exit are required'}
$log=[IO.File]::ReadAllText($LogPath)
$siblings=@(Get-ChildItem (Split-Path -Parent $LogPath) -File -Filter '*.log')
$all=($siblings|ForEach-Object{[IO.File]::ReadAllText($_.FullName)}) -join "`n"
$result=Get-AICFPresetQueuedApproachVerdict $log $all ([IO.File]::ReadAllText($LauncherPath)) ([IO.File]::ReadAllText($NativeExitPath)) ([bool]$RequireHealth)
if($ReportPath){$result|ConvertTo-Json -Depth 6|Set-Content -LiteralPath $ReportPath}
$result|ConvertTo-Json -Depth 6
if($result.verdict -ne 'PASS'){exit 1}
exit 0

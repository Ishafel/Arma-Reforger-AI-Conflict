function Test-AICFUnstuckBrakeLog {
    param([Parameter(Mandatory)][AllowEmptyCollection()][AllowEmptyString()][string[]]$Lines)
    function Field([string]$line,[string]$name) {
        $match=[regex]::Match($line,'(?:^|\s)'+[regex]::Escape($name)+'=([^\s]+)')
        if($match.Success){return $match.Groups[1].Value};return ''
    }
    $applied=@($Lines|Where-Object{$_-match'\[UNSTUCK_BRAKE_APPLIED\]'})
    $released=@($Lines|Where-Object{$_-match'\[UNSTUCK_BRAKE_RELEASED\]'})
    $motion=@($Lines|Where-Object{$_-match'\[UNSTUCK_BRAKE_NATIVE_MOTION\]'})
    $done=@($Lines|Where-Object{$_-match'\[UNSTUCK_PROBE_FINISHED\]'})
    if($applied.Count-ne1-or$released.Count-ne1-or$motion.Count-ne1-or$done.Count-ne1){return $false}
    $operation=Field $applied[0] 'operation';$car=Field $applied[0] 'car';$deadline=Field $applied[0] 'deadline'
    if(!$operation-or!$car-or$car-eq'NONE'-or(Field $applied[0] 'rpl')-in@('','NONE')-or$deadline-notmatch'^\d+$'){return $false}
    $preNative=@($Lines|Where-Object{$_-match'\[UNSTUCK_BRAKE_PRE_NATIVE_MOTION\]'})
    if($preNative.Count-ne3){return $false}
    $lastObserved=0
    for($i=0;$i-lt3;$i++){
        $line=$preNative[$i]
        if((Field $line 'operation')-ne$operation-or(Field $line 'car')-ne$car-or(Field $line 'distinct_polls')-ne[string]($i+1)-or(Field $line 'current_executed_move')-ne'1'-or(Field $line 'native_brake')-ne'0'-or(Field $line 'attempts')-ne'0'){return $false}
        [double]$nativeDistance=0;[int]$observed=0
        if(![double]::TryParse((Field $line 'native_distance'),[Globalization.NumberStyles]::Float,[Globalization.CultureInfo]::InvariantCulture,[ref]$nativeDistance)-or[double]::IsNaN($nativeDistance)-or[double]::IsInfinity($nativeDistance)-or$nativeDistance-lt10){return $false}
        if(![int]::TryParse((Field $line 'observed_at'),[ref]$observed)-or$observed-le0-or($i-gt0-and$observed-$lastObserved-lt500)){return $false}
        if([array]::IndexOf($Lines,$line)-ge[array]::IndexOf($Lines,$applied[0])){return $false}
        $lastObserved=$observed
    }
    foreach($pair in @(@('initial_flag','0'),@('own_write','1'),@('attempts','0'),@('fake_stuck','0'))){if((Field $applied[0] $pair[0])-ne$pair[1]){return $false}}
    foreach($line in @($released[0],$motion[0])){if((Field $line 'operation')-ne$operation-or(Field $line 'car')-ne$car-or(Field $line 'deadline')-ne$deadline-or(Field $line 'attempts')-ne'2'-or(Field $line 'cycle_case_proven')-ne'0'){return $false}}
    foreach($pair in @(@('failed_sites','2'),@('persistent_brake','0'),@('physical_relocations','2'))){if((Field $released[0] $pair[0])-ne$pair[1]){return $false}}
    if((Field $motion[0] 'health_unchanged')-ne'1'-or(Field $motion[0] 'purchases')-ne'1'){return $false}
    [double]$distance=0;[int]$polls=0
    if(![double]::TryParse((Field $motion[0] 'actual_distance'),[Globalization.NumberStyles]::Float,[Globalization.CultureInfo]::InvariantCulture,[ref]$distance)-or[double]::IsNaN($distance)-or[double]::IsInfinity($distance)-or$distance-lt10){return $false}
    if(![int]::TryParse((Field $motion[0] 'distinct_polls'),[ref]$polls)-or$polls-lt3){return $false}
    if((Field $done[0] 'passed')-ne'1'-or(Field $done[0] 'reason')-ne'CONTROLLED_BRAKE_TWO_REAL_CORRECTIONS_AND_NATIVE_10M'){return $false}
    $corrections=@($Lines|Where-Object{$_-match'\[UNSTUCK_PROBE_CORRECTION\]' -and (Field $_ 'operation')-eq$operation -and (Field $_ 'car')-eq$car})
    if($corrections.Count-ne2){return $false}
    for($i=0;$i-lt2;$i++){
        if((Field $corrections[$i] 'attempt')-ne[string]($i+1)){return $false}
        [double]$moved=0
        if(![double]::TryParse((Field $corrections[$i] 'actual_distance'),[Globalization.NumberStyles]::Float,[Globalization.CultureInfo]::InvariantCulture,[ref]$moved)-or[double]::IsNaN($moved)-or[double]::IsInfinity($moved)-or$moved-lt4-or$moved-gt15){return $false}
    }
    $noMotion=@($Lines|Where-Object{$_-match'\[VEHICLE_UNSTUCK_ATTEMPT\]' -and (Field $_ 'operation_id')-eq$operation -and (Field $_ 'reason')-eq'NO_PHYSICAL_MOVEMENT' -and (Field $_ 'relocated')-eq'1'})
    if($noMotion.Count-ne2-or(Field $noMotion[0] 'attempt')-ne'1'-or(Field $noMotion[1] 'attempt')-ne'2'){return $false}
    $positions=@([array]::IndexOf($Lines,$applied[0]),[array]::IndexOf($Lines,$noMotion[0]),[array]::IndexOf($Lines,$corrections[0]),[array]::IndexOf($Lines,$noMotion[1]),[array]::IndexOf($Lines,$corrections[1]),[array]::IndexOf($Lines,$released[0]))
    for($i=1;$i-lt$positions.Count;$i++){if($positions[$i]-le$positions[$i-1]){return $false}}
    if(@($Lines|Where-Object{$_-match'\[UNSTUCK_BRAKE_RELEASE_RECEIPT\]' -and (Field $_ 'released')-ne'1'}).Count){return $false}
    if(@($Lines|Where-Object{$_-match'\[UNSTUCK_BRAKE_RELEASE_RECEIPT\]' -and (Field $_ 'native_preempted')-ne'0'}).Count){return $false}
    $sequence=@('[UNSTUCK_BRAKE_APPLIED]','[UNSTUCK_BRAKE_RELEASED]','[UNSTUCK_BRAKE_NATIVE_MOTION]','[UNSTUCK_PROBE_FINISHED]')
    $previous=-1
    foreach($tag in $sequence){$found=-1;for($i=0;$i-lt$Lines.Count;$i++){if($Lines[$i].Contains($tag)){$found=$i;break}};if($found-le$previous){return $false};$previous=$found}
    return $true
}

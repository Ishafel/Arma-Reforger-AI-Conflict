function Test-AICFUnstuckCorridorLog {
    param([Parameter(Mandatory)][AllowEmptyCollection()][AllowEmptyString()][string[]]$Lines,[switch]$RequireOriginalRetarget)
    function Field([string]$line,[string]$name){$m=[regex]::Match($line,'(?:^|\s)'+[regex]::Escape($name)+'=([^\s]+)');if($m.Success){return $m.Groups[1].Value};return ''}
    function Number([string]$s,[double]$minimum,[double]$maximum){[double]$n=0;return [double]::TryParse($s,[Globalization.NumberStyles]::Float,[Globalization.CultureInfo]::InvariantCulture,[ref]$n)-and![double]::IsNaN($n)-and![double]::IsInfinity($n)-and$n-ge$minimum-and$n-le$maximum}
    $scope=@($Lines|Where-Object{$_-match'\[UNSTUCK_CORRIDOR_SCOPE\]'})
    $fault=@($Lines|Where-Object{$_-match'\[UNSTUCK_PROBE_POSE_FAULT\]'})
    $finish=@($Lines|Where-Object{$_-match'\[UNSTUCK_PROBE_FINISHED\]'})
    if($scope.Count-ne1-or$fault.Count-ne1-or$finish.Count-ne1){return $false}
    $operation=Field $scope[0] 'operation';$car=Field $scope[0] 'car';$deadline=Field $scope[0] 'deadline';$rpl=Field $fault[0] 'rpl'
    if(!$operation-or!$car-or$car-eq'NONE'-or$rpl-in@('','NONE')-or$deadline-notmatch'^\d+$'){return $false}
    if((Field $scope[0] 'historical_source')-ne'MIN3_FIRST_STUCK_ORIGIN'-or(Field $scope[0] 'native_drivable_proven')-ne'0'){return $false}
    foreach($pair in @(@('operation',$operation),@('car',$car),@('deadline',$deadline),@('applied','1'),@('attempts','0'),@('maximum_attempts','2'),@('setup_fault_only','1'),@('native_progress','0'),@('orientation_preserved','1'),@('health_writes','0'))){if((Field $fault[0] $pair[0])-ne$pair[1]){return $false}}
    if(!(Number (Field $fault[0] 'distance_3d') 0.00001 15)){return $false}
    if((Field $finish[0] 'passed')-ne'1'-or(Field $finish[0] 'reason')-ne'CORRIDOR_AFTER_ACTUAL_FORWARD_CORRECTION_AND_NATIVE_10M'){return $false}
    $far=@($Lines|Where-Object{$_-match'\[UNSTUCK_PROBE_FAR_SCOPE\]' -and (Field $_ 'far_operation')-eq$operation})
    if($far.Count-ne1){return $false}
    foreach($pair in @(@('deadline',$deadline),@('purchases','1'),@('maximum_attempts','2'),@('same_trip','1'),@('same_lease','1'))){if((Field $far[0] $pair[0])-ne$pair[1]){return $false}}
    $corrections=@($Lines|Where-Object{$_-match'\[UNSTUCK_PROBE_CORRECTION\]' -and (Field $_ 'operation')-eq$operation -and (Field $_ 'car')-eq$car})
    if($corrections.Count-lt1-or$corrections.Count-gt2){return $false}
    $receipts=@();$previousAt=[array]::IndexOf($Lines,$fault[0])
    for($i=0;$i-lt$corrections.Count;$i++){
        $row=$corrections[$i];$number=Field $row 'attempt'
        if($number-ne[string]($i+1)-or!(Number (Field $row 'actual_distance') 4 15)){return $false}
        $receipt=@($Lines|Where-Object{$_-match'\[VEHICLE_UNSTUCK_ATTEMPT\]' -and (Field $_ 'operation_id')-eq$operation -and (Field $_ 'attempt')-eq$number})
        if($receipt.Count-ne1){return $false}
        foreach($pair in @(@('relocated','1'),@('mode','SAFE_TERRAIN_REPOSITION'),@('rpl_id',$rpl))){if((Field $receipt[0] $pair[0])-ne$pair[1]){return $false}}
        $receiptAt=[array]::IndexOf($Lines,$receipt[0]);$rowAt=[array]::IndexOf($Lines,$row)
        if($receiptAt-le$previousAt-or$rowAt-le$receiptAt){return $false}
        $previousAt=$rowAt;$receipts+=$receipt[0]
    }
    $correction=$corrections[-1];$attempt=Field $correction 'attempt'
    if($attempt-notin@('1','2')-or!(Number (Field $correction 'actual_distance') 4 15)){return $false}
    if((Field $correction 'forward')-ne'1'-or(Field $correction 'pre_hint')-ne'1'-or(Field $correction 'pre_scope')-ne'PROVEN'){return $false}
    $kind=Field $correction 'hint_kind'
    if($kind-ne'FULL_NATIVE_PATH'-and!($kind-eq'LOCAL_FRAGMENT'-and(Field $correction 'hint_points')-eq'3')){return $false}
    $actual=@($receipts[-1])
    foreach($pair in @(@('relocated','1'),@('mode','SAFE_TERRAIN_REPOSITION'),@('rpl_id',$rpl),@('hint_only','1'),@('path_progress','0'),@('hint_kind',$kind))){if((Field $actual[0] $pair[0])-ne$pair[1]){return $false}}
    $correctionAt=[array]::IndexOf($Lines,$correction);$faultAt=[array]::IndexOf($Lines,$fault[0]);$scopeAt=[array]::IndexOf($Lines,$scope[0]);$actualAt=[array]::IndexOf($Lines,$actual[0]);$finishAt=[array]::IndexOf($Lines,$finish[0])
    if($scopeAt-ge$faultAt-or$faultAt-ge$actualAt-or$actualAt-ge$correctionAt-or$correctionAt-ge$finishAt){return $false}
    if($RequireOriginalRetarget){
        $request=@($Lines|Where-Object{$_-match'\[UNSTUCK_CORRIDOR_RETARGET_REQUEST\]'})
        $bound=@($Lines|Where-Object{$_-match'\[UNSTUCK_CORRIDOR_ORIGINAL_BOUND\]'})
        if($request.Count-ne1-or$bound.Count-ne1){return $false}
        foreach($row in @($request[0],$bound[0])){foreach($pair in @(@('operation',$operation),@('car',$car),@('deadline',$deadline),@('purchases','1'),@('budget_before','0'))){if((Field $row $pair[0])-ne$pair[1]){return $false}}}
        foreach($pair in @(@('normal_adoption','1'),@('same_trip','1'),@('same_lease','1'))){if((Field $bound[0] $pair[0])-ne$pair[1]){return $false}}
        if((Field $request[0] 'normal_owner_command')-ne'1'-or!(Number (Field $request[0] 'approach_3d') 0 30)){return $false}
        foreach($name in @('assignment','intent')){if((Field $request[0] $name)-ne(Field $bound[0] $name)-or!(Number (Field $bound[0] $name) 1 2147483647)){return $false}}
        $old=Field $request[0] 'old_wp';$new=Field $bound[0] 'new_wp'
        if($old-in@('','NONE','0')-or$new-in@('','NONE','0')-or$old-eq$new-or!(Number (Field $bound[0] 'native_points') 2 512)){return $false}
        $requestAt=[array]::IndexOf($Lines,$request[0]);$boundAt=[array]::IndexOf($Lines,$bound[0]);$farAt=[array]::IndexOf($Lines,$far[0])
        if($farAt-ge$requestAt-or$requestAt-ge$boundAt-or$boundAt-ge$faultAt){return $false}
    }
    $motion=@($Lines|Where-Object{$_-match'\[UNSTUCK_CORRIDOR_NATIVE_MOTION\]' -and [array]::IndexOf($Lines,$_)-gt$correctionAt})
    if($motion.Count-lt3){return $false}
    $lastAt=0
    for($i=0;$i-lt3;$i++){
        $row=$motion[$i]
        foreach($pair in @(@('operation',$operation),@('car',$car),@('rpl',$rpl),@('deadline',$deadline),@('attempts',$attempt),@('maximum_attempts','2'),@('health_unchanged','1'),@('purchases','1'),@('pose_fault','1'),@('hint_only','1'),@('path_progress','0'))){if((Field $row $pair[0])-ne$pair[1]){return $false}}
        if((Field $row 'distinct_polls')-ne[string]($i+1)-or!(Number (Field $row 'actual_distance') 10 1000000)){return $false}
        [int]$at=0;if(![int]::TryParse((Field $row 'observed_at'),[ref]$at)-or$at-le0-or($i-gt0-and$at-$lastAt-lt500)){return $false}
        if([array]::IndexOf($Lines,$row)-ge$finishAt){return $false};$lastAt=$at
    }
    if(@($Lines|Where-Object{$_-match'\[UNSTUCK_BRAKE_APPLIED\]'}).Count){return $false}
    return $true
}

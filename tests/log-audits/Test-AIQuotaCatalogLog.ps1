param([string]$LogPath,[ValidateSet('character','group','vehicle')][string]$Mode='group',[int]$NativeExitCode=[int]::MinValue,[string]$EvidenceRoot,[ValidateSet('Server','Client')][string]$LogRole='Server',[switch]$LibraryOnly)
$ErrorActionPreference='Stop'
function ConvertFrom-AICFQuotaCatalogFields([string]$line){$r=@{};foreach($m in [regex]::Matches($line,'([a-zA-Z_][a-zA-Z0-9_]*)=([^\s]+)')){$r[$m.Groups[1].Value]=$m.Groups[2].Value};return $r}
function Test-AICFQuotaCatalogProof([string[]]$Lines,[string]$Case){
 $fail=@();$rows=@($Lines|Where-Object{$_-match'\[QUOTA_CATALOG_PROBE\]'});$done=@($rows|Where-Object{$_-match'\[DONE\]'});$selected=@($rows|Where-Object{$_-match'\[SELECTED\]'})
 if($done.Count-ne1-or$selected.Count-ne1){return @('EXACT_ONE_SELECTED_DONE')}
 $d=ConvertFrom-AICFQuotaCatalogFields $done[0];$s=ConvertFrom-AICFQuotaCatalogFields $selected[0]
 foreach($row in $rows){$scope=ConvertFrom-AICFQuotaCatalogFields $row;if($scope.mode-ne$Case){$fail+='CATALOG_ROW_MODE';break}}
 if($d.mode-ne$Case-or$s.mode-ne$Case-or$d.passed-ne'1'-or$d.reason-ne'ACTUAL_NATIVE_CATALOG_TAIL'){$fail+='NATIVE_DONE_SCOPE'}
 $n=0;if(![int]::TryParse($d.expected,[ref]$n)-or$n-lt1-or$n-gt20){$fail+='AUTHORED_COUNT';$n=1}
 if($d.factories-ne[string]$n-or$d.debits-ne'1'){$fail+='EXACT_NATIVE_EFFECTS'}
 $headers=0;if($Case-eq'group'){$headers=1};if($d.headers-ne[string]$headers-or@($rows|Where-Object{$_-match'\[NATIVE_HEADER\]'}).Count-ne$headers){$fail+='EXACT_NATIVE_HEADERS'}
 if($s.actual_avatar-ne'1'-or$s.actual_group_leader-ne'1'-or$s.faction-eq'NONE'-or!$s.prefab){$fail+='ACTUAL_USER_CATALOG'}
 $price=0.0;$paid=0.0
 if(![double]::TryParse($s.cost,[Globalization.NumberStyles]::Float,[Globalization.CultureInfo]::InvariantCulture,[ref]$price)-or! [double]::TryParse($d.debited,[Globalization.NumberStyles]::Float,[Globalization.CultureInfo]::InvariantCulture,[ref]$paid)-or$price-le0-or[double]::IsNaN($price)-or[double]::IsInfinity($price)-or[double]::IsNaN($paid)-or[double]::IsInfinity($paid)-or[Math]::Abs($price-$paid)-gt0.01){$fail+='ACTUAL_PAYMENT'}
 $payments=@($rows|Where-Object{$_-match'\[PAYMENT\]'});$paymentValue=0.0;$paymentDelta=0.0
 if($payments.Count-ne1){$fail+='NATIVE_CONSUMER_PAYMENT'}else{$p=ConvertFrom-AICFQuotaCatalogFields $payments[0];if($p.actual_native_debit-ne'1'-or$p.native_consumer-ne'1'-or![double]::TryParse($p.value,[Globalization.NumberStyles]::Float,[Globalization.CultureInfo]::InvariantCulture,[ref]$paymentValue)-or![double]::TryParse($p.actual_delta,[Globalization.NumberStyles]::Float,[Globalization.CultureInfo]::InvariantCulture,[ref]$paymentDelta)-or[double]::IsNaN($paymentValue)-or[double]::IsInfinity($paymentValue)-or[double]::IsNaN($paymentDelta)-or[double]::IsInfinity($paymentDelta)-or[Math]::Abs($paymentValue+$price)-gt0.01-or[Math]::Abs($paymentDelta-$price)-gt0.01){$fail+='NATIVE_CONSUMER_PAYMENT'}}
 $funding=@($rows|Where-Object{$_-match'\[CONTROLLED_RESOURCE_FUNDING\]'})
 if($funding.Count-gt1){$fail+='CONTROLLED_FUNDING_ONCE'}elseif($funding.Count-eq1){
  $f=ConvertFrom-AICFQuotaCatalogFields $funding[0];$v=@{};$valid=$true
  foreach($field in @('before','amount','after','effective_cost')){$number=0.0;if(![double]::TryParse($f[$field],[Globalization.NumberStyles]::Float,[Globalization.CultureInfo]::InvariantCulture,[ref]$number)-or[double]::IsNaN($number)-or[double]::IsInfinity($number)){$valid=$false};$v[$field]=$number}
  if(!$valid-or$f['native_generator']-ne'1'-or$f['fixture_only']-ne'1'-or!$f['generator_type']-or!$f['producer']-or$v.before-lt0-or$v.amount-le0-or$v.after-lt$price-or[Math]::Abs($v.effective_cost-$price)-gt0.01-or[Math]::Abs($v.after-$v.before-$v.amount)-gt0.01-or[array]::IndexOf($rows,$funding[0])-ge[array]::IndexOf($rows,$selected[0])){$fail+='CONTROLLED_NATIVE_FUNDING'}
 }
 $ids=@($rows|Where-Object{$_-match'\[BODY_IDENTITY\]'}|ForEach-Object{ConvertFrom-AICFQuotaCatalogFields $_})
 if($ids.Count-ne$n-or@($ids.entity|Sort-Object -Unique).Count-ne$n-or@($ids.rpl|Sort-Object -Unique).Count-ne$n-or@($ids|Where-Object{!$_.entity-or$_.entity-in@('NONE','0','-1')-or!$_.rpl-or$_.rpl-in@('NONE','0','-1')}).Count){$fail+='EXACT_PHYSICAL_IDENTITIES'}
 $tail=@($rows|Where-Object{$_-match'\[TAIL\]'});if($tail.Count-ne1){$fail+='NATIVE_TAIL'}else{$t=ConvertFrom-AICFQuotaCatalogFields $tail[0];$ms=0;if(![int]::TryParse($t.actual_tail_ms,[ref]$ms)-or$ms-lt25000-or$ms-ge300000-or$t.retained_original_body_count-ne[string]$n){$fail+='NATIVE_TAIL'}}
 if($Case-ne'vehicle'){$callbacks=@($rows|Where-Object{$_-match'\[NATIVE_CALLBACK\]'}|ForEach-Object{ConvertFrom-AICFQuotaCatalogFields $_});if($d.callbacks-ne[string]$n-or$callbacks.Count-ne$n-or@($callbacks|Where-Object{$_.bound_receipt_before_native_activation-ne'1'}).Count){$fail+='BOUND_BEFORE_NATIVE_ACTIVATION'};for($i=0;$i-lt$callbacks.Count;$i++){if($callbacks[$i].index-ne[string]($i+1)){$fail+='CALLBACK_SEQUENCE';break}}}
 if($Case-eq'group'){$negative=@($rows|Where-Object{$_-match'\[FULL_BASKET_NEGATIVE\]'});if($negative.Count-ne1){$fail+='FULL_BASKET_NEGATIVE'}else{$v=ConvertFrom-AICFQuotaCatalogFields $negative[0];if($v.requested-ne[string]$n-or$v.remaining-ne[string]($n-1)-or$v.native_factories-ne'0'-or$v.headers-ne'0'-or$v.callbacks-ne'0'-or$v.debits-ne'0'){$fail+='FULL_BASKET_NEGATIVE'};$negIndex=[array]::IndexOf($rows,$negative[0]);$firstEffect=@($rows|Where-Object{$_-match'\[(PAYMENT|NATIVE_HEADER|BODY_IDENTITY)\]'}|Select-Object -First 1);if($firstEffect.Count-ne1-or[array]::IndexOf($rows,$firstEffect[0])-le$negIndex){$fail+='NEGATIVE_BEFORE_NATIVE_EFFECTS'}}}
 if($Case-eq'vehicle'){$empty=@($rows|Where-Object{$_-match'\[EMPTY_VEHICLE\]'});if($n-ne1-or$empty.Count-ne1){$fail+='EMPTY_HUMAN_VEHICLE'}else{$v=ConvertFrom-AICFQuotaCatalogFields $empty[0];if($v.actual_compartments_read-ne'1'-or$v.occupants-ne'0'-or$v.reservations-ne'0'-or$v.counted_vehicle_agent-ne'0'-or$v.character_bucket_full-ne'1'){$fail+='EMPTY_HUMAN_VEHICLE'}};$factories=@($rows|Where-Object{$_-match'\[NATIVE_VEHICLE_FACTORY\]'});if($factories.Count-ne1-or$d.callbacks-ne'0'-or@($rows|Where-Object{$_-match'\[NATIVE_CALLBACK\]'}).Count){$fail+='ACTUAL_NATIVE_VEHICLE_FACTORY'}else{$v=ConvertFrom-AICFQuotaCatalogFields $factories[0];if($v.actual_protected_factory_return-ne'1'-or$v.actual_vehicle-ne'1'-or$v.retained_physical_identity-ne'1'-or$v.character_basket-ne'0'){$fail+='ACTUAL_NATIVE_VEHICLE_FACTORY'}}}
 return $fail
}
if($LibraryOnly){return}
if(!$LogPath-or!(Test-Path -LiteralPath $LogPath -PathType Leaf)){throw 'Full stopped native console log required'}
$lines=@(Get-Content -LiteralPath $LogPath);$fail=@()
if($LogRole-eq'Server'){$fail=@(Test-AICFQuotaCatalogProof $lines $Mode)}else{
 $avatar=@($lines|Where-Object{$_-match'\[QUOTA_CLIENT_BOOTSTRAP\].*phase=4\b'}|ForEach-Object{ConvertFrom-AICFQuotaCatalogFields $_})
 $clientPlayer=0
 if($avatar.Count-ne1-or$avatar[0].actual_actor-ne'1'-or!$avatar[0].actual_faction-or$avatar[0].actual_faction-eq'NONE'-or![int]::TryParse($avatar[0].player,[ref]$clientPlayer)-or$clientPlayer-le0){$fail+='ACTUAL_CLIENT_AVATAR'}
 if(@($lines|Where-Object{$_-match'\[QUOTA_CLIENT_BOOTSTRAP\].*failed=1\b'}).Count){$fail+='CLIENT_BOOTSTRAP_FAILED'}
}
if($NativeExitCode-ne0){$fail+='NATIVE_EXIT'}
$stops=@($lines|Where-Object{$_-match'ENGINE\s*:\s*Game destroyed\.'})
$proofEvents=@($lines|Where-Object{if($LogRole-eq'Server'){$_-match'\[QUOTA_CATALOG_PROBE\]\[DONE\]'}else{$_-match'\[QUOTA_CLIENT_BOOTSTRAP\].*phase=4\b'}})
if($stops.Count-ne1-or$proofEvents.Count-ne1-or[array]::IndexOf($lines,$stops[0])-le[array]::IndexOf($lines,$proofEvents[0])){$fail+='NATIVE_STOP_NOT_PROVEN'}
$siblings=@(Get-ChildItem -LiteralPath (Split-Path -Parent $LogPath) -File -Filter '*.log');$fatal=@();$resources=@();$other=@()
$fatalPattern='SCRIPT\s+\([EF]\)|ENGINE\s+\(F\)|\b(?:Unhandled|Uncaught)\s+exception\b|\b(?:VM|Virtual\s*Machine)\b.*\b(?:exception|error)\b|\bApplication\s+crashed\b|\bAccess violation\b|\bNull (?:pointer|instance)\b|\bNullPointerException\b|Assertion failed|Entity out of world bounds|Fatal error'
foreach($file in $siblings){
 $fatal+=@(Select-String -LiteralPath $file.FullName -Pattern $fatalPattern)
 $resources+=@(Select-String -LiteralPath $file.FullName -Pattern 'RESOURCES?\s+\([EF]\)')
 $other+=@(Select-String -LiteralPath $file.FullName -Pattern '\([EF]\)'|Where-Object{$_.Line-notmatch'RESOURCES?\s+\([EF]\)'-and$_.Line-notmatch$fatalPattern})
}
$result=[ordered]@{log_role=$LogRole;functional_pass=($fail.Count-eq0);functional_failures=$fail;native_exit=$NativeExitCode;full_sibling_fatal_count=$fatal.Count;resource_error_count=$resources.Count;other_error_count=$other.Count;strict_pass=($fail.Count-eq0-and$fatal.Count-eq0-and$resources.Count-eq0-and$other.Count-eq0);full_logs=@($siblings.FullName)}
if($EvidenceRoot){New-Item -ItemType Directory -Path $EvidenceRoot -Force|Out-Null;$result|ConvertTo-Json -Depth 6|Set-Content -LiteralPath (Join-Path $EvidenceRoot 'catalog-audit.json') -Encoding UTF8}
$result|ConvertTo-Json -Depth 6
if(!$result.strict_pass){exit 1}

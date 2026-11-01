param([string]$LogPath,[string]$OutputPath)
$ErrorActionPreference='Stop'
function Field([string]$s,[string]$key){$m=[regex]::Match($s,'(?:^|\s)'+[regex]::Escape($key)+'=([^\s]+)');if(!$m.Success){return '?'};return $m.Groups[1].Value}
function Number([string]$s,[string]$key){$v=Field $s $key;$n=0L;if($v-notmatch'^\d+$'-or![long]::TryParse($v,[ref]$n)){return -1L};return $n}
function Test-StockPressureText([string]$raw){
 $events=@([regex]::Matches($raw,'\[QUOTA_STOCK_PRESSURE\]\[(\w+)\] ([^\r\n]+)'))
 $veto=@($events|Where-Object{$_.Groups[1].Value-eq'FULL_STOCK_VETO'});$intent=@($events|Where-Object{$_.Groups[1].Value-eq'MANAGED_FIVE_INTENT'});$proof=@($events|Where-Object{$_.Groups[1].Value-eq'PROOF'});$done=@($events|Where-Object{$_.Groups[1].Value-eq'DONE'});$hold=@($events|Where-Object{$_.Groups[1].Value-eq'TARGET_HOLD_ONE'});$members=@($events|Where-Object{$_.Groups[1].Value-eq'MANAGED_MEMBER'})
 $fail=@();if($veto.Count-ne1-or$intent.Count-ne1-or$proof.Count-ne1-or$done.Count-ne1-or$hold.Count-ne1-or$members.Count-ne5){return @('EXACT_EVENT_SET')}
 $v=$veto[0];$i=$intent[0];$p=$proof[0];$d=$done[0];$h=$hold[0]
 if(!($h.Index-lt$v.Index-and$v.Index-lt$i.Index-and$i.Index-lt$p.Index-and$p.Index-lt$d.Index)){$fail+='CAUSAL_EVENT_ORDER'}
 if((Field $d.Value 'passed')-ne'1'-or(Field $d.Value 'reason')-ne'STOCK_FULL_MANAGED_RECRUIT_FIVE'){$fail+='DONE_FAILURE'}
 $limit=Number $v.Value 'stock_limit';if($limit-lt1-or$limit-gt4096-or(Number $v.Value 'stock_physical')-ne$limit-or(Number $v.Value 'stock_reserved')-ne0-or(Field $v.Value 'admitted')-ne'0'-or(Number $v.Value 'actual_agents')-ne0){$fail+='ACTUAL_STOCK_FULL_VETO'}
 $before=Number $v.Value 'native_intended_before';if($before-lt0-or$before-ne(Number $v.Value 'native_intended_after')){$fail+='NATIVE_INTENDED_CHANGED'}
 if((Number $p.Value 'stock_physical')-ne$limit-or(Number $p.Value 'stock_limit')-ne$limit-or(Number $p.Value 'stock_reserved')-ne0-or(Number $p.Value 'actual_managed')-ne5-or(Number $p.Value 'extra_actual')-ne0-or(Number $p.Value 'veto_hold_ms')-lt35000-or(Number $p.Value 'veto_hold_ms')-gt600000){$fail+='PROOF_BUDGET_OR_DURATION'}
 if((Number $p.Value 'elapsed_ms')-(Number $v.Value 'elapsed_ms')-lt35000){$fail+='REAL_ELAPSED_WINDOW'}
 $group=Field $h.Value 'group';$generation=Number $h.Value 'generation';$original=Field $h.Value 'original_actor'
 if($group-notmatch'^0x[0-9A-Fa-f]{16}$'-or$original-notmatch'^0x[0-9A-Fa-f]{16}$'-or$generation-lt1){$fail+='ORIGINAL_NATIVE_IDENTITY'}
 foreach($event in @($i,$p)+$members){if((Field $event.Value 'group')-cne$group-or(Number $event.Value 'generation')-ne$generation){$fail+='GROUP_GENERATION_MISMATCH'}}
 $ids=@($members|ForEach-Object{Field $_.Value 'actor'});$rpls=@($members|ForEach-Object{Field $_.Value 'rpl'})
 if(@($ids|Sort-Object -Unique).Count-ne5-or@($rpls|Sort-Object -Unique).Count-ne5-or$ids-notcontains$original-or@($ids|Where-Object{$_-notmatch'^0x[0-9A-Fa-f]{16}$'}).Count-or@($rpls|Where-Object{$_-notmatch'^-?\d+$'-or$_-eq'0'}).Count){$fail+='EXACT_FIVE_NATIVE_ACTORS'}
 $joins=@([regex]::Matches($raw,'\[INFANTRY_RECRUIT_JOINED\] ([^\r\n]+)')|Where-Object{$_.Index-gt$i.Index-and$_.Index-lt$p.Index-and(Field $_.Value 'faction')-eq'US'-and(Field $_.Value 'slot')-eq'0'-and(Field $_.Value 'group')-ceq$group-and(Number $_.Value 'generation')-eq$generation})
 $joined=@($joins|ForEach-Object{Field $_.Value 'character'});$expected=@($ids|Where-Object{$_-cne$original})
 if($joins.Count-ne4-or@($joined|Sort-Object -Unique).Count-ne4-or@(Compare-Object @($joined|Sort-Object) @($expected|Sort-Object)).Count){$fail+='ACTUAL_FOUR_JOINED_RECEIPTS'}
 $debits=@([regex]::Matches($raw,'\[INFANTRY_RECRUIT_DEBITED\] ([^\r\n]+)')|Where-Object{$_.Index-gt$i.Index-and$_.Index-lt$p.Index-and(Field $_.Value 'faction')-eq'US'-and(Field $_.Value 'slot')-eq'0'-and(Field $_.Value 'group')-ceq$group-and(Number $_.Value 'generation')-eq$generation})
 if($debits.Count-ne4){$fail+='EXACT_FOUR_NATIVE_DEBITS'}
 foreach($debit in $debits){$cost=Number $debit.Value 'cost';$beforeStock=Number $debit.Value 'supplies_before';$afterStock=Number $debit.Value 'supplies_after';if($cost-lt1-or$beforeStock-lt0-or$afterStock-lt0-or$beforeStock-$afterStock-ne$cost){$fail+='ACTUAL_SUPPLY_DELTA'}}
 if(!@($events|Where-Object{$_.Groups[1].Value-eq'RECRUIT_REQUEST'-and$_.Index-gt$i.Index-and$_.Index-lt$p.Index-and(Field $_.Value 'accepted')-eq'1'}).Count){$fail+='NATIVE_RECRUITMENT_NOT_ACCEPTED'}
 return $fail
}
if($LogPath){$raw=[IO.File]::ReadAllText($LogPath);$fail=@(Test-StockPressureText $raw);$result=[ordered]@{scope='STOCK_CHARACTER_MANAGED_RECRUIT_ONLY';pass=($fail.Count-eq0);failures=$fail;original_route_pass=$false;universal_quota_pass=$false};if($OutputPath){$result|ConvertTo-Json -Depth 4|Set-Content -LiteralPath $OutputPath -Encoding utf8};$result|ConvertTo-Json -Compress;if($fail.Count){exit 1}}

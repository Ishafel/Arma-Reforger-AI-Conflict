# Область проверки string.Format

`Invoke-AICFLanguageAudit` проверяет неподдерживаемые `%10` и выше только
в строковых литералах первого аргумента настоящего `string.Format`.
Литералы данных, например `name.Replace("%", "%25")` и
`string.Format("%1", "%25")`, не являются format template.
Запрет более9substitution arguments сохранён независимо от template.

Ложный baseline обнаружен полным offline запуском WCS high-load проверки
01.11.2026: `Test-Stage35Static.ps1` отклонял две операции percent encoding
в `AICF_StrategicUI.c`. Производственное кодирование/декодирование не менялось.
Старый audit воспроизводимо отвергнут новым actual-helper contract на первом
случае `percent-encoding` (expected regression FAIL1).

`Get-AICFStringFormatCallInfo` выделяет первый аргумент тем же lexer, который
считает arity: учитывает comments, escaped quotes, nested calls и скобки.
Поиск native calls идёт по stripped code; комментарий с псевдовызовом
не создаёт format call. Динамическое вычисление template не анализируется
как полный data flow.

После исправления `Test-LanguageAuditContracts.ps1`17casesPASS и
`Test-Stage35Static.ps1`PASS0. Проверены percent encoding, строковые данные
substitution arguments, `%10`/`%99`, девять/десять аргументов, nested/wrapped/
concatenated format, запятая/escaped quote внутри литерала, comments и
незакрытый вызов. Негативные случаи по-прежнему требуют соответствующий
`STRING_FORMAT_PLACEHOLDER` или `STRING_FORMAT_ARITY`.

Evidence: `.codex-runtime/wcs-highload-soak-20261101/language-before/`,
`language-after/`, исходный полный FAIL в `shard-2/Test-Stage35Static.ps1.txt`.
Это test-only исправление; оно не снимает FPS/runtime FAIL и не требует
новой Enforce компиляции, production `.c` bytes не менялись.

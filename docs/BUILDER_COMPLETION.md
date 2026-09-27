# Ожидание заблокированного завершения строительства

Строитель мог бесконечно работать инструментом на последнем этапе казарм:
`AddBuildingValue` отклонял завершение из-за препятствия, а служба строителя
не проверяла результат вызова. `BUILDER_PROGRESS` ошибочно описывал попытку
как прогресс. В зарегистрированном случае RHS AFRF казарма остановилась на
40/50, физическую проверку блокировал UH-1H. После освобождения площадки
тот же проект завершился штатно.

`AICF_BaseBuilderService` теперь проверяет результат через ту же composition
identity и повторно полученный живой layout: stock может синхронно удалить
старый layout при успешном завершении. Событие `BUILDER_PROGRESS` сохраняет
поля `value`, `before`, `total`, но появляется только после фактического
увеличения прогресса или завершения composition.

Если прогресс не изменился, служба убирает инструмент, снимает item-use
подписки, сбрасывает рабочий интервал и пишет `BUILDER_WORK_BLOCKED` с
`reason`, `progress`, `total`, `retry_ms=15000`. Оплаченный layout и назначение
сохраняются. Каждые 15 секунд выполняется существующий `AICF_CompletionClear`.
При успехе появляется `BUILDER_WORK_RESUMED`, строитель снова достаёт
инструмент и отрабатывает полный трёхсекундный интервал. Перед фактическим
завершением stock adapter заново проверяет площадку.

Пауза сбрасывается в `ClearTarget`, включая смену владельца, потерю identity,
завершение игроком, смерть строителя и остановку службы. Authority, readiness,
положение за footprint, инструмент и native clearance сохраняют свои guards.
Препятствия автоматически не удаляются; занятый участок останется в ожидании,
пока не будет освобождён. Повторная оплата и повторное создание layout не нужны.

## Проверки

До и после изменения запускаются PowerShell-аудиты:

```powershell
./tools/Test-BaseBuildersStatic.ps1
./tools/Test-BaseBuilderDangerContracts.ps1
./tools/Test-ConstructionStatic.ps1
./tools/Test-ConstructionContracts.ps1
./tools/Test-Stage3Static.ps1
./tools/Test-Stage35Static.ps1
./tools/Test-Stage4Static.ps1
```

`tools/fixtures/AICF_BaseBuilderCompletionProbe.c` копируется только в
`Construction` отдельной копии исходников. Production его не загружает.
После терминального Workbench Validate запускается canonical launcher:

```powershell
./tools/Start-AICFRuntime.ps1 -Role Server -Variant Stock `
  -RepositoryRoot <isolated-stage> -ProfileRoot <fresh-profile> `
  -AdditionalArguments @('-addr','127.0.0.1:22215','-aicfBuilderCompletionProbe','1')
```

Для RHS используется `-Variant EveronNorthRHS -RhsAddonsRoot <installed-addons>`.
Fixture блокирует completion первых малых казарм каждой стороны на 45 секунд,
не обходя production поиск, оплату, подход, анимацию и native clearance после
снятия отказа. Проверяет частоту повторов, отсутствие item-use во время паузы,
неизменность прогресса и identity, возобновление и online service. На успехе
или после семи минут вызывает `RequestClose`; verdict берётся из полного
остановленного server log и `BUILDER_COMPLETION_PROBE_FINISHED`.

Дополнительно `Test-ConstructionLog.ps1 -RequireCompletion -ExpectedMode BOTH`
проверяет цепочку reservation/payment/layout/service и отсутствие повторного
списания; `Test-BaseBuildersLog.ps1 -MinimumCompleted 2 -RequireToolUse` проверяет
native инструмент, два завершения и хотя бы один idle retirement. Эти широкие
анализаторы могут не пройти при остановке короткого fixture с другими
проектами в очереди или до ухода строителя на отдых; это отдельное покрытие.
Ручное наблюдение анимации в клиенте — отдельный gate, его не заменяют логи.

Проверка 26.09.2026: семь статических аудитов до/после — PASS без исходных
failures; production и fixture Workbench Validate для Stock и полного RHS —
PASS. Целевой runtime RHS Север — обе стороны завершили те же казармы после
45 секунд блокировки, `completed=2 failures=0`, оплата однократная. На Stock
прошёл US; USSR не нашёл площадку за время fixture, покрытие обеих сторон
не выполнено. Широкие runtime-аудиты сохранили failures покрытия:
`CONSTRUCTION_PENDING_AT_STOP` и `BUILDERS_IDLE_COVERAGE` на RHS,
`BUILDERS_COMPLETION_COVERAGE` на Stock. Ошибок SCRIPT E/F, ENGINE F, VM/NULL
pointer нет; ошибки ресурсов и других подсистем в полных логах присутствуют.
Ручной клиентский осмотр и полный lifecycle smoke — NOT RUN. Локальные
evidence и точные команды: `.codex-runtime/builder-completion-20260926/result.md`.

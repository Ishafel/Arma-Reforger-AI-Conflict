# Последний захват и восстановление приказов — 2026-10-04

Исходник до изменения: `ba101f2da541fa5e6cdce2a03437428b9e0e3146`.
Диагностический отчёт VM302 подтверждает владение шестью objective-базами,
но не устанавливает конкретный blocker победы. Этот вывод не подменяется
результатами локальной fixture. Текущая война VM302 не изменялась.

## Причина тупика и поведение

`SelectAttackTarget` возвращал `null`, когда доступных вражеских целей больше
нет. `AssignAICommanderOrder` и `ReconcileAICommanderOrder` возвращали отказ;
generic reliability могла только восстановить прежний intent. После двух
отказов `WAIT_STRATEGIC_REPLAN` не получал нового назначения даже на следующих
циклах командира.

Теперь faction commander после отсутствия attack-кандидата выбирает союзную
базу через существующий детерминированный `SelectDefendTarget` и выдаёт
`AREA_SECURITY`: обычный исполняемый Defend waypoint. Приоритет имеет база
под угрозой, затем передовая база и HQ. Numeric slot, role, group и generation
сохраняются. Это не player `SYSTEM_HOLD` и не ожидание команды игрока.
Оборона HQ имеет отдельный `allowed_idle_reason=HQ_AREA_SECURITY`, только
при связанном waypoint. Наличие задачи само по себе не доказывает движение.

Командир пересматривает security-назначение каждый свой цикл (default 15 секунд).
Новая вражеская цель немедленно заменяет его атакой, без ожидания dwell обороны.
При двух неудачных repairs и недействительной цели fallback вызывает тот же
authority boundary с `REPAIR_BUDGET_TARGET_INVALID`. Бюджет repairs не снят.
На готовом графе отсутствующая attack-цель больше не является причиной
бессрочного ожидания; при rebuild/replan сохраняется fail-closed gate.
Неготовый граф или невозможность создать waypoint не объявляются успешной задачей.

Правила новой player BASE-кандидатуры не расширены: ATTACK по-прежнему не может
атаковать союзную базу. Отдельная проверка assignment принимает `AREA_SECURITY`
только с `AI_COMMANDER`. Durable intent, vehicle suspension/restoration и
runtime endpoint сохраняют выбранную оборонительную семантику.

## Persistent field hold

`ReliabilityTick` пересматривает hold через 5, 10, 20 и далее 30 минут.
Счётчик задержек принадлежит воплощению slot и очищается при lifecycle reset,
а не при очередном hold. Проверяются group, generation, target и assignment
revision через `IsPersistentStuckContextCurrent`. Новых callbacks нет.
Rebuild/replan и существующие vehicle/recruitment/recovery gates сохраняются.

Повтор использует commander boundary; player intent восстанавливается согласно
прежним правилам. При отказе planner восстанавливается физический field hold и
пишется `GROUP_STUCK_REVIEW_DEFERRED held=...`. Пересмотр не является
подтверждением движения: существующая проверка displacement/route reduction
и бюджеты каждого recovery episode продолжают действовать. Teleport, replacement
roster и расход тикетов этим переходом не выполняются.

События field hold сохранили имена и поля, изменены значения:
`resume/trigger=CONTEXT_OR_BOUNDED_REVIEW`, `auto_retry=BOUNDED`,
`next_action=WAIT_CONTEXT_OR_BOUNDED_REVIEW`. Соответствующий запрет timed retry
в `Test-Stage35RecoveryPolicy` заменён проверками identity и backoff по требованию
этой задачи. Контракт `SYSTEM_HOLD` допускает совместную ветку выбора Defend
для security, сохраняя обязательность Defend у `SYSTEM_HOLD`.

## Диагностика победы

`AICF_VictoryDiagnostics` публикует `VICTORY_CHECK` отдельно для каждой runtime
фракции при изменении диагностического снимка и раз в 60 секунд без изменений.
Поля: `candidate`, `revision`, `graph_ready`, `rebuild`, `replan`, `objectives`,
`status`, `reason`, `blocker`, `objective`, `hq`, `initialized`, `owner`,
`capture_state`, `capturing`, `enemies_present`. `blocker` — stock callsign.

`status=NOT_READY` отличается от `UNMET` и `MET`. Причины включают
`GRAPH_NOT_READY`, `NODE_MISSING`, `BASE_MISSING`, `BASE_UNINITIALIZED`,
`OWNER_MISMATCH`, `CAPTURE_STATE`, `CAPTURING`, `ENEMIES_PRESENT`, `NO_OBJECTIVES`.
Записывается первая блокирующая база в порядке графа; количество целей считается
по всему графу. Проверка повторяется в обычном `Update`, даже без owner events.
Сам predicate победы и stock `EndGameMode` не изменены. HQ/RELAY не становятся
дополнительными целями победы.

## Строительство: проверено существующее ограничение

SHA-256 полного VM302 log совпал с отчётом:
`98a6c04af00f6c56d58ab532cccf1d5ed48e4b0a843a1335d7909483f5bf91ac`.
В полном файле найдено 579 `CONSTRUCTION_SEARCH_BACKOFF` и 599
`CONSTRUCTION_DECISION`. Для одинаковых faction/base/provider/type не найдено
ни одного решения до сохранённого `t_ms + retry_ms`; максимальный backoff —
480000 ms. Cursor поиска сохраняется, owner/provider identity сбрасывает историю,
успешная постройка сбрасывает историю типа. Существующие construction audits PASS.

Повторяющийся `BASE_OR_PROVIDER_UNSAFE` — предварительный отказ quote примерно
раз в минуту, до поиска площадки (`candidates=0`, `queries=1` в наблюдаемой серии).
Он не доказывает наличие противника. Требуемая история безрезультатного поиска
уже работает; Construction production не изменён. Снижение частоты предварительных
отказов и измерение их FPS/CPU влияния не заявляются выполненными.

## Команды и evidence

Все артефакты: `.codex-runtime/endgame-20261004/`, вне Git.
`before-*.log` — baseline, `final-*.log` и `complete-*.log` — проверки после правок.

```powershell
$audits = @('Stage3Static','Stage35Static','Stage35RecoveryPolicy','Stage4Static',
  'AICommanderModeStatic','VictoryRespawnStatic','ConstructionStatic',
  'ConstructionContracts','MapPointOrdersStatic','EndgameContracts',
  'RHSIntegrationStatic','WCSIntegrationStatic','RecoveryEpisodeContracts',
  'MovementRecoveryContracts','Issue12RecoveryContracts')
foreach ($audit in $audits) {
  powershell.exe -NoProfile -ExecutionPolicy Bypass -File "tools/Test-$audit.ps1"
}
```

14 PASS / 0; Stage4Static — сохранённый FAIL / 1, только `STAGE4_ATTACKED_BASES`.
Новый `Test-EndgameContracts` проверяет девять отрицательных мутаций.
Повтор применимых проверок после последних guards сохранил эти verdict.

Terminal Workbench запускается сохранённым `compile.ps1` по команде из
`docs/DEVELOPMENT.md`: `ArmaReforgerWorkbenchSteamDiag.exe -noThrow -wbsilent
-gproj ... -addonsDir ... -addons ... -logsDir ... -wbModule=ScriptEditor -run -validate`.
Exact аргументы находятся в `wb-*-args.json`, полные логи — в `wb-*`.
Окончательные production и fixture graphs: `production-complete` и
`fixture-complete`, каждый Arland и ArlandWCSRHS — PASS, native exit 0,
`Game successfully created`, `Script validation successful`, без SCRIPT E/F,
ENGINE F и VM exceptions. Версия Tools/Game/Server — 1.8.0.13.
Первый sandbox-run `wb-production-Arland` остановлен `SteamAPI_Init`, exit -1:
он не засчитан и не удалён. Последующие вызовы использовали пользовательскую
Steam-сессию через терминал, без GUI automation.

Fixture `tools/fixtures/AICF_EndgameProbe.c` копируется только в изолированный
`stage/AIConflictCore/Scripts/Game/AIConflict/Victory/`. Четыре source addons
копируются в stage; production-копии fixture нет. Запуск каждого сценария:

```powershell
./tools/Start-AICFRuntime.ps1 -Role Server -Variant Stock `
  -RepositoryRoot '<absolute stage>' -ProfileRoot '<fresh stock profile>' `
  -ServerPort 2013 -AdditionalArguments @('-aicfEndgameProbe','1','-aicfRequirePlayerForResult','0')
./tools/Start-AICFRuntime.ps1 -Role Server -Variant ArlandWCSRHS `
  -RepositoryRoot '<absolute stage>' -ProfileRoot '<fresh WCS profile>' `
  -ServerPort 2013 -AdditionalArguments @('-aicfEndgameProbe','1','-aicfRequirePlayerForResult','0')
```

Запуски последовательны; каждый закрывается fixture через `RequestClose`.
Полные server profiles и logs сохраняются, вместе с native exit и
`AICF_RUNTIME_MANIFEST_JSON` в `runtime-*-launcher.log`.
`audit-runtime.ps1` читает полные остановленные logs, сохраняет SHA-256,
функциональные результаты и отдельный список всех error lines.
Окончательные profiles: `server-stock-complete` и `server-wcs-complete`;
вывод: `runtime-stock-complete-launcher.log`, `runtime-wcs-complete-launcher.log`.
`source-stage-hashes.txt` подтверждает равенство всех пяти изменённых production
`.c` между рабочим деревом и stage. Предыдущие прогоны сохранены отдельно.

Stock и WCS+RHS: 12/12 функциональных cases PASS, по одному `VICTORY`
и `MATCH_END`, обе стороны имеют положительные тикеты. После последнего
захвата `ENEMIES_PRESENT` блокирует победу; изменение только presence даёт
`MET` и победу с той же revision графа. В WCS проверен реально загруженный
`WCS_RHS_ARLAND`; runtime faction keys этого запуска — `US`/`USSR`.
Два repair failures моделируются через production counter, старый атакующий
waypoint создаётся fixture; fallback проходит production authority path.
Время field hold вводится через test-only override timer predicate: это проверка
перехода и backoff, не многочасовой soak.

Общий runtime **не PASS**. `Test-Stage4Log.ps1 -LogPath <full stopped console.log>`
возвращает FAIL / 1 в обоих сценариях: production `STAGE1 RESULT FAIL` требует
ещё retarget-deadline, reinforcement и ticket-debit evidence, которых короткая
endgame fixture не предоставляет. Этот SCRIPT E не исключён из отчёта.
Кроме него присутствуют engine/resource diagnostics: stock GUID/unknown keyword/
duplicate Hierarchy/resource leaks; WCS дополнительно GUID материалов, текстур,
localization и другие content warnings/errors. Они сохранены полностью и не
объявлены доказанным pre-change runtime baseline этой правки.

`git diff --check` по всем файлам: FAIL / 2 из-за исходной пользовательской
пустой строки в конце `tools/Start-AICFRuntime.ps1`; этот файл не редактировался.
Проверка с исключением launcher: PASS / 0.

## Файлы и границы проверки

- `Orders/AICF_OrderPlanner.c`: security selection, текущая/durable validity,
  оборонительный waypoint и endpoint, возобновление атаки.
- `Bootstrap/AICF_MatchController.c`: orchestration fallback/review/diagnostics.
- `State/AICF_GroupSlot.c`: generation-local exponential review delay.
- `Victory/AICF_VictorySystem.c`, новый `AICF_VictoryDiagnostics.c`: observation.
- `tools/Test-EndgameContracts.ps1`, `tools/fixtures/AICF_EndgameProbe.c`;
  обновлены `Test-AICommanderModeStatic.ps1`, `Test-Stage35RecoveryPolicy.ps1`.
- `.gitignore`, `README.md`, `docs/VICTORY_RESPAWN.md`, этот документ.

Client/JIP и экран победы — NOT RUN. Многочасовое реальное застревание,
физическая зачистка базы от живых врагов, транспорт с security intent,
повторное появление бойцов в таком приказе, полный естественный бой и runtime
других карт — NOT RUN. Presence и owner в fixture контролируемые; доказательства
естественного захвата или физического движения от одного waypoint не выводятся.
Точная историческая причина блокировки победы на VM302 остаётся неизвестной
до нового evidence с `VICTORY_CHECK`. Работа не объявляется ACCEPTED.

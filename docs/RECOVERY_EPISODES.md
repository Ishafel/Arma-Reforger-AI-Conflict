# Восстановление пехоты — issues #11 и #12

Актуальная совместная реализация и её проверки описаны в разделе
«Совместное исправление #11 и #12 — 03.10.2026». Предшествующие разделы
сохраняют историю кандидатов и их ограничения на момент проверки;
их `NOT RUN` и промежуточные результаты не заменяют итоговые gates.

Исправление опубликовано в draft [PR #13](https://github.com/Ishafel/Arma-Reforger-AI-Conflict/pull/13).
Целевые focused tests — PASS; полный runtime matrix имеет 4 PASS и 3 FAIL
из-за дополнительных находок [#14](https://github.com/Ishafel/Arma-Reforger-AI-Conflict/issues/14).
Общий результат не считается полностью зелёным; issues #11/#12 не закрыты,
PR не merged, статус `ACCEPTED` не присвоен.

## Исходная реализация #11

`AICF_InfantryRecruitmentService` проверяет подход к казарме до создания donor
и оплаты. Waypoint должен присутствовать в queue и быть текущим. Отсутствие
waypoint либо 45 секунд без сокращения расстояния минимум на 5 м вызывает
ремонт через `AICF_OrderPlanner`. На визит разрешены два ремонта; исходный
approach timeout, token, group/generation и intent сохраняются. Grace после
ремонта не является физическим прогрессом. Покупка по-прежнему возможна
только после `IsPhysicallyPresent()` в пределах 35 м.

`RefreshInfantryMuster` не заменяет приказ активного визита приказом ожидания.
После отказа подхода служба сохраняет identity казармы и stable slot, generation,
позицию группы и graph revision. Обычный cooldown не разрешает тот же подход
повторно: выбирается другая допустимая казарма либо восстанавливается прежнее
назначение. Для ещё не собранного seed остаётся muster hold, без отправки
одиночного бойца в бой. Повторная оценка разрешается при изменении graph,
замене/отключении службы, её перемещении либо перемещении живого лидера минимум
на 35 м. Изменение group generation удаляет старую запись. Одно лишь ожидание
не снимает блокировку; исчезновение физического препятствия без наблюдаемого
изменения этих условий само по себе не обнаруживается.

`AICF_RouteRecoveryEpisode` хранится в stable slot отдельно от локальных
false-completion counters. Он начинается при false completion либо temporary
route hold и ограничивает отсутствие движения 180 секундами. Замена waypoint,
assignment, очередной hold и `ResetFalseCompletionRecovery()` не продлевают
его. Прогресс требует того же живого лидера, текущего waypoint, смещения минимум
15 м и сокращения расстояния до текущего endpoint минимум на 5 м. Смена лидера
меняет точку наблюдения, но не deadline. Группа в temporary/persistent hold
не подтверждает восстановление маршрута своей фоновой активностью.

По исчерпании срока controller один раз запрашивает persistent field hold через
planner. Entity, roster и tickets сохраняются; автоматические replan и
recruitment не обходят исчерпанный episode. `hold_committed=0` означает отказ
установки hold, а не успешное восстановление. Новый graph context либо явное
изменение player intent разрешают повторную оценку. Новая group generation
имеет собственный episode. Teleport, spawn/delete и новые callbacks не добавлены.

Диагностика различает:

- `INFANTRY_RECRUITMENT_APPROACH_REPAIR`: приказ выдан, движение ещё не доказано;
- `INFANTRY_RECRUITMENT_PROGRESS/ARRIVED`: физический прогресс/прибытие;
- `INFANTRY_RECRUITMENT_APPROACH_BLOCKED/REARMED`: блокировка конкретной службы
  и снятие после изменения контекста; новые события содержат `service`;
- `ORDER_RESTORE_RESULT`: прежние поля сохранены, добавлены `order_issued`,
  `safe_hold`, `movement_confirmation`;
- `ROUTE_RECOVERY_EXHAUSTED`: terminal hold и его фактический результат;
- `ROUTE_RECOVERY_EPISODE_FINISHED`: `PHYSICAL_PROGRESS` или `CONTEXT_CHANGED`.

## Проверки

Evidence: `.codex-runtime/issue-11/`. Исходный commit записан в `commit.txt`.
`before-*`/`after-*` содержат полный вывод статических проверок. Исходный
`STAGE4_ATTACKED_BASES` не относится к этой правке и не исправляется ради PASS.

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Test-InfantryRecruitmentStatic.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Test-RecoveryEpisodeContracts.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Test-Stage2Log.ps1 -LogPath '<полный остановленный console.log>'
```

Также выполняются `Test-Stage3Static`, `Test-Stage35Static`,
`Test-Stage35RecoveryPolicy`, `Test-Stage4Static`, `Test-AICommanderModeStatic`
и `Test-MapPointOrdersStatic`. Новый contract suite проверяет девять позитивных
и негативных логов: healthy, повторный подход к одной службе, replan с разными
assignment и `replanned`, hold без движения, реальный progress, новая generation
и прибытие, снятие блокировки при изменении контекста и отказ terminal hold.
Номера слотов анализатором больше не ограничены `0..3`.

Terminal runtime fixture состоит из
`tools/fixtures/AICF_InfantryRecruitmentRuntimeProbe.c` и
`tools/fixtures/AICF_RecoveryEpisodeProbe.c`; обе копируются только в отдельный
stage Core/Forces. Первая готовит настоящие казармы; при новом флаге размещает
их дальше радиуса немедленного прибытия. Вторая удаляет waypoint из queue,
ускоряет исчерпание ремонта и elapsed time episode, проверяет блокировку
повторного подхода и изменение graph context. После инъекций движение,
прибытие, набор и оплата выполняются production-кодом. Это не воспроизведение
точной геометрии исходной пользовательской сессии.

```powershell
& ./tools/Start-AICFRuntime.ps1 -Role Server -Variant RHS `
  -RepositoryRoot '<stage>' -ProfileRoot '<fresh-profile>' `
  -AdditionalArguments @('-addr','127.0.0.1:2011',
    '-aicfRecruitProbe','1','-aicfRecoveryEpisodeProbe','1',
    '-aicfRequirePlayerForResult','0')
```

Для direct `-server` проверенный проектом параметр адреса — `-addr`.
Документированный [bindPort](https://community.bistudio.com/wiki/Arma_Reforger:Startup_Parameters#bindPort)
переопределяет server config, но в выполненном direct запуске не изменил порт
2001; существующий пользовательский сервер не останавливался.

Manifest сохраняется в полном `runtime-*-launch.txt`; `wb-*-args.json`
содержат exact Workbench CLI, соседние каталоги — полные логи.
Первые sandbox/port-conflict запуски не являются runtime PASS.
Ручной визуальный gate, client/JIP, длительный soak и исходная пользовательская
сессия после обновления — NOT RUN.

## Состав изменения и baseline

Production:

- `Forces/AICF_InfantryRecruitmentService.c`: контроль подхода и история отказов;
- `Forces/AICF_InfantryRecruitmentOrder.c`: состояние подхода, `service` в логах;
- `Forces/AICF_RecruitmentApproachFailure.c`: identity и условия снятия блокировки;
- `Orders/AICF_OrderPlanner.c`: ремонт waypoint и защита активного визита от muster;
- `State/AICF_GroupSlot.c`: связь с episode и проверка активного визита;
- `State/AICF_RouteRecoveryEpisode.c`: общий deadline и физическое подтверждение;
- `Bootstrap/AICF_MatchController.c`: orchestration terminal hold и диагностика.

Все пути выше относятся к `AIConflictCore/Scripts/Game/AIConflict/`.
Также изменены `tools/Test-Stage2Log.ps1`, добавлен
`tools/Test-RecoveryEpisodeContracts.ps1`, расширена
`tools/fixtures/AICF_InfantryRecruitmentRuntimeProbe.c`, добавлена
`tools/fixtures/AICF_RecoveryEpisodeProbe.c`. `.gitignore` разрешает новые
тесты и этот документ. Чужое изменение `README.md`, появившееся во время
работы, не редактировалось.

Исходный HEAD: `4d6c5e03dd05733b3b47ef59387d4800f49501a8`.
Game/Server/Tools: `1.8.0.13`. Статические команды запускаются как
`powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/<имя>.ps1`.

| Имя команды | До | После |
|---|---|---|
| `Test-InfantryRecruitmentStatic` | PASS / 0 | PASS / 0 |
| `Test-Stage3Static` | PASS / 0 | PASS / 0 |
| `Test-Stage35Static` | PASS / 0 | PASS / 0 |
| `Test-Stage35RecoveryPolicy` | PASS / 0 | PASS / 0 |
| `Test-Stage4Static` | FAIL / 1: `STAGE4_ATTACKED_BASES` | Тот же FAIL / 1 |
| `Test-AICommanderModeStatic` | PASS / 0 | PASS / 0 |
| `Test-MapPointOrdersStatic` | PASS / 0 | PASS / 0 |
| `Test-RecoveryEpisodeContracts` | Новый | PASS / 0 |

Terminal Workbench: production stock, production RHS и stage fixture —
PASS / 0 (`wb-reviewed-stock`, `wb-reviewed-rhs`, `wb-reviewed-stage`),
везде `Game successfully created`, без `SCRIPT (E/F)` и `ENGINE (F)`.
Внешние resource warnings/errors сохраняются в полных логах. Sandbox
Workbench ранее завершался с platform initialization errors; он не подменяет
финальные успешные запуски в пользовательском окружении.

Промежуточные runtime попытки сохранены: №1 заблокирован TLS до мира,
№2–3 отказали на занятом порту 2001, №4 выявил недостатки постановки fixture,
№5 прерван после потери weak reference тестовой записи, №6 проверил четыре
fault cases, но изменение graph прервало набор до полного roster.
Эти попытки не объявляются полным runtime PASS; отрицательная проверка
`Test-RecoveryEpisodeContracts -RuntimeLogPath <лог №4>` действительно
отклоняет неполную fixture с exit 1 даже при успешном наборе.

## Финальный runtime — 2026-10-01

`runtime-rhs-7`, адрес `127.0.0.1:2012`: сервер штатно завершился
22:19:36 МСК, native exit 0. Полный остановленный `console.log` содержит
1721 строку. SHA-256 семи затронутых production-файлов совпадают между
рабочей копией и stage (`source-stage-hashes.json`). Manifest вынесен также
в `runtime-rhs-7-manifest.json`.

| Команда / проверка | Verdict |
|---|---|
| `Start-AICFRuntime.ps1 -Role Server -Variant RHS` с указанными выше fixture flags и `-addr 127.0.0.1:2012` | Exit 0, штатные `Replication finished` и `Game destroyed` |
| `Test-RecoveryEpisodeContracts.ps1 -RuntimeLogPath <полный console.log>` | PASS / 0: 4 fault cases, 9 synthetic analyzer cases |
| `Test-Stage2Log.ps1 -LogPath <полный console.log>` | PASS / 0: 20 bindings, 2 heartbeats, 1 recovery |
| `Test-InfantryRecruitmentLog.ps1 -LogPath <полный console.log> -RequireFullRosters` | PASS / 0: 3 визита, 18 recruits, две стороны 10/10 |
| `git diff --check` | PASS / 0 |

Восстановление потерянного waypoint сохранило token, срок визита и group identity,
не создало donor и не сняло supplies. После инъекции исчерпания подход завершился
с блокировкой службы. Проверена отмена этой блокировки при новом graph context;
после снятия тестовой инъекции действительное движение и прибытие подтвердились
production events, затем прошёл полный набор с `stable_groups=1`.
Общий route episode пережил 16 повторений смены assignment/hold и при
инъекционном elapsed time 180001 мс установил hold с `hold_committed=1`.
Это проверка deadline policy, а не длительный soak с естественными
endpoint failures исходной игровой сессии.

Старый `Test-Stage2Log.ps1` из исходного HEAD возвращал exit 0 на обоих
синтетических циклах; новая версия возвращает exit 1. Снимок старого анализатора
и полные before/after результаты находятся в evidence.

В финальном runtime нет `SCRIPT (E/F)`, `ENGINE (F)`, VM/null exceptions
или `[AICF][STAGE...][ERROR]`. Остаются 68 native resource/world/entity
diagnostics: RHS localization/GUID, `m_fAILimitThreshold`, `SCR_BaseTaskManager`,
duplicate `Hierarchy`, `Parent`, `SlidingTrackMaterial` и font resource leak
при shutdown. Они перечислены в `runtime-rhs-7-errors-index.txt`; индекс не
заменяет полный лог. Поэтому PASS относится к целевым контрактам, а не к
утверждению об отсутствии любых ошибок установленного content graph.

Исходная пользовательская сессия не перезапускалась; работа не объявляется
`ACCEPTED`, issue автоматически не закрывается. Для живого матча после
обновления, client/JIP, ручной проверки и soak сохраняется NOT RUN.

## Дополнение: локальное скрытое восстановление подхода

По запросу пользователя подключён bounded hidden recovery для recruitment.
После первого ремонта waypoint и следующих 45 секунд без прогресса выполняется
одна попытка локального переноса живых AI-бойцов. Используются существующие
`AICF_VehicleWatchdog.CanApplyHiddenRecovery` (игрок/possession, радиус и LOS),
`IsHiddenRecoveryCombatSafe`, `AICF_InfantrySpawnPlacement.IsUsable` (navmesh,
вода, свободный объём, связность) и общий с isolated-navmesh recovery лимит
`HasUsedIsolatedNavmeshRecovery` на generation. Выключенный
`aicfHiddenRecoveryEnabled` запрещает и этот путь.

Новый `Forces/AICF_InfantryRecruitmentRecovery.c` перебирает локальные позиции
в детерминированном порядке; горизонтальное смещение каждого бойца не превышает
8 м, перепад высоты — 1,5 м. Точка остаётся минимум на 5 м за пределами радиуса
прибытия к казарме: отряду ещё нужно пройти остаток пути самостоятельно.
Identity, roster, authority, транспорт, combat и player/LOS fences проверяются
до переноса и непосредственно перед каждой мутацией. Если повторная проверка
останавливает частичный перенос, generation budget всё равно израсходован.
Новые callback или lifecycle subscriptions не создаются.

`InfantryRecruitmentService` вызывает helper только на втором ремонте за визит;
`OrderPlanner` сохраняет владение waypoint. `InfantryRecruitmentOrder` хранит
pending-флаг и время подачи асинхронного `Teleport`. На следующем наблюдении
после минимум 1 секунды служба переустанавливает distance baseline и пропускает
закупку. Смещение от Teleport не выдаётся за физический прогресс; абсолютный
срок подхода не продлевается. `MatchController` передаёт существующие watchdog
и policy через `SetRecoveryPolicy`.

Изменения дополнения: новый helper, `InfantryRecruitmentOrder`,
`InfantryRecruitmentService`, одна composition-строка в `MatchController`,
`tools/Test-RecoveryEpisodeContracts.ps1`,
`tools/fixtures/AICF_RecoveryEpisodeProbe.c` и этот документ.

Evidence дополнения: `.codex-runtime/issue-11-recovery/`; `before-*.txt` и
`after-*.txt` сохраняют результаты девяти аудитов. Восемь PASS / 0; единственный
FAIL / 1 до и после — прежний `STAGE4_ATTACKED_BASES` в `Test-Stage4Static.ps1`.
Проверки: `Test-InfantryRecruitmentStatic`, `Test-Stage3Static`,
`Test-Stage35Static`, `Test-Stage35RecoveryPolicy`, `Test-Stage4Static`,
`Test-AICommanderModeStatic`, `Test-MapPointOrdersStatic`,
`Test-InfantrySpawnPlacementStatic`, `Test-RecoveryEpisodeContracts`.
Запуск каждого: `powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/<имя>.ps1`.

Терминальный Workbench `-noThrow -wbsilent -wbModule=ScriptEditor -run -validate`
для stock и RHS: PASS / 0, `Script validation successful`, `Game successfully created`.
Полные логи: `wb-stock`, `wb-rhs`; stage с fixture: `wb-stage-2`, PASS / 0.
Первая stage-попытка выявила несовпадение имён аргументов override только в
fixture; исправлена, не скрывается (`wb-stage`, FAIL / -1).

Первый дополнительный runtime (`runtime-1`) штатно завершился с exit 0.
Все 6 fault cases PASS: прежние 4, отказ player fence/устаревшая generation,
подача переноса и наблюдение результата с generation budget. Для бойца USSR
выбран шаг 3,00004 м; через секунду наблюдалось смещение 2,87708 м, `paid=0`.
Однако полный интеграционный gate вернул FAIL / 1 (`RUNTIME_INCOMPLETE_OR_UNSTOPPED`):
после искусственного исчерпания подхода и штатного retry изменился radio graph,
набор остановился с `GRAPH_CHANGED` на US=10, USSR=8. Shutdown нормальный;
причина FAIL — неполный roster, а не незавершённый процесс. Полные логи и
`runtime-1-contracts.txt` сохранены. Это не повод менять production graph gate.

Для прямой проверки продолжения того же визита fixture получила явный режим
`-aicfRecruitHiddenContinue 1`: после проверки переноса она прекращает инъекции,
оставляя production-подход и покупки. Его анализатор вызывается с
`-RequireHiddenRecovery -HiddenContinuationOnly`; он требует 2 общих и 2 hidden
cases, физический прогресс, полный набор и штатный shutdown. Проверки исчерпания
и context rearm остаются в исходном режиме и подтверждены первым runtime.
Промежуточная fixture скомпилирована в `wb-stage-3`: PASS / 0.

В `runtime-2` обе стороны набрали 10/10 и процесс штатно завершился, но hidden
gate вернул FAIL / 1: рандомизация HQ дала исходную дистанцию USSR 39,77 м,
и `NO_SAFE_LOCAL_POSITION` корректно запретил перенос. Положительная fixture
не выполнила предусловие расстояния. Поэтому только в режиме
`aicfRecruitHiddenContinue` казарма теперь размещается со смещением `100 0 60`
от штатного spawn point (без абсолютных координат карты), а инъекции начинаются
при расстоянии не меньше 55 м. Production-ограничение не ослаблялось.
Изменён также `tools/fixtures/AICF_InfantryRecruitmentRuntimeProbe.c`.
Промежуточная fixture скомпилирована в `wb-stage-4`: PASS / 0.

В `runtime-3` прямой путь USSR подтвердился: перенос 2,99995 м, наблюдаемое
смещение через секунду 3,08529 м с учётом последующего движения, `paid=0`,
прибытие по тому же token=12 и набор 10/10. Но US начал длинный подход позже,
и новый `GRAPH_CHANGED` оставил его на 6/10 к лимиту fixture; полный gate
FAIL / 1, shutdown нормальный. Увеличенная дистанция теперь применяется только
к проверяемой стороне USSR, а US использует штатное размещение этой fixture
`35 0 25`. Инъекции focused mode явно выбирают USSR. Финальная fixture:
`wb-stage-5`, PASS / 0. Production-файлы между runtime-прогонами не менялись.

### Итог дополнения

Финальный `runtime-4` завершился 01.10.2026 в 22:59:16 MSK, exit 0,
`Game destroyed`. Команда:

```powershell
./tools/Start-AICFRuntime.ps1 -Role Server -Variant RHS `
  -RepositoryRoot .codex-runtime/issue-11-recovery/stage `
  -ProfileRoot .codex-runtime/issue-11-recovery/runtime-4 `
  -AdditionalArguments @('-addr','127.0.0.1:2011',
    '-aicfRecruitProbe','1','-aicfRecoveryEpisodeProbe','1',
    '-aicfRecruitHiddenProbe','1','-aicfRecruitHiddenContinue','1',
    '-aicfRequirePlayerForResult','0')
```

Manifest сохранён в `runtime-4-manifest.json`; свежий полный лог содержит
точный `CLI Params`, `[ROSTER_READY]`, штатную остановку. Хеши production-файлов
в stage и checkout совпадают (`production-parity.txt`).

Подтверждены 4 focused cases: waypoint loss, общий deadline episode, отказ
player fence/устаревшая identity/успешная подача и наблюдение переноса/лимит
generation. USSR перенесён на 3 м, фактическое наблюдаемое смещение через секунду
3,30851 м включает последующее движение. `paid=0`; тот же token=12 дошёл до
казармы и набрал 10/10 без замены группы. Принудительное исчерпание подхода и
context rearm дополнительно подтверждены в `runtime-1`.

| Команда по полному остановленному `runtime-4` console.log | Verdict |
|---|---|
| `Test-Stage2Log.ps1 -LogPath <log>` | PASS / 0: bindings=20, heartbeats=4 |
| `Test-InfantryRecruitmentLog.ps1 -LogPath <log>` | PASS / 0: visits=3, joined=13, транзакции корректны |
| `Test-InfantryRecruitmentLog.ps1 -LogPath <log> -RequireFullRosters` | FAIL / 1: US=5/10, нет общего full-roster evidence |
| `Test-RecoveryEpisodeContracts.ps1 -RuntimeLogPath <log> -RequireHiddenRecovery -HiddenContinuationOnly` | FAIL / 1: `RUNTIME_INCOMPLETE_OR_UNSTOPPED`, конкретно неполный US roster; shutdown нормальный |
| `Test-RecoveryEpisodeContracts.ps1` | PASS / 0: structural contracts, 9 route и 2 rearm analyzer cases |
| `git diff --check` | PASS / 0 |

US не участвовал в hidden recovery. Его обычный подход дважды завершился
`APPROACH_INTERRUPTED_OR_TIMEOUT` после прибытия; смена graph штатно сняла
первую блокировку, повторный визит набрал до 5/10. Это остаётся ограничением
полной интеграционной проверки; весь сценарий не объявляется PASS.

В ходе проверки обновлён также `tools/Test-InfantryRecruitmentLog.ps1`:
`INFANTRY_RECRUITMENT_APPROACH_REARMED` относится к истории службы между
визитами и не содержит visit token. Анализатор раньше ошибочно требовал
открытый визит; теперь проверяет stable identity и `reason=CONTEXT_CHANGED`
отдельно. Положительный и отрицательный тесты сохранены в
`Test-RecoveryEpisodeContracts.ps1`; искусственный `reason=TIMER_ONLY`
возвращает FAIL / 1. Проверка полного roster не ослаблялась.

В полном логе нет script/VM/AICF ошибок; присутствуют те же 68 native
resource/world/entity diagnostics перечисленных выше категорий
(`runtime-4-errors-index.txt`). Это не утверждение об отсутствии любых ошибок.

NOT RUN: реальный подключённый игрок рядом/в LOS (проверен отказ через
watchdog fixture), client/JIP, перенос одновременно нескольких бойцов,
исходная пользовательская сессия и длительный soak. Не выполнено полное
интеграционное принятие; работа не помечается `ACCEPTED`.

## Совместное исправление #11 и #12 — 03.10.2026

Следующие результаты относятся к продолжению исправления поверх `ac98e88`.
Предыдущие неуспешные прогоны сохранены выше как история, их verdict не изменён.
Evidence нового этапа: `.codex-runtime/issues-11-12-20261003/`.

### Причины и изменения

- `AICF_AINodeLifecycle` передаёт `UNKNOWN` актуального recruitment waypoint
  в запись визита через slot. Проверяются текущий waypoint, group, generation,
  assignment и intent. BT уведомляет текущую activity об отказе и завершает
  только захваченный action, если callback не заменил его. Сам waypoint
  заменяет planner на следующем service tick. Два ремонта и абсолютный срок
  визита сохраняются; повторный callback не создаёт дополнительной попытки.
- Пустой `WaypointEntityIn` у управляемой группы завершает устаревшую ветку
  `SCR_AIGetSmartActionsState`. Перед `FAIL` вызывается штатный `OnAbort`,
  снимающий smart-action callbacks через `ResetVariables`. Непустые входы
  и неуправляемые группы продолжают штатную валидацию.
- В старом логе перед неудачами MOB egress уже зафиксирован
  `ROUTE_RECOVERY_EXHAUSTED hold_committed=1`. Общий task audit и MOB egress
  могли снова выдать маршрут поверх terminal hold; затем reliability
  прекращала проверку pending recovery из-за исчерпанного episode. Это
  объясняет сочетание отказа rebuild и последующего taskless deadline.
  Теперь task recovery и rebuild уважают владельца hold, а MOB relocation
  до мутации проверяет возможность восстановления текущей цели, включая
  deferred target. Исполняемый persistent hold учитывается как разрешённое
  ожидание только при наличии waypoint в очереди группы.
- Hidden recruitment recovery сообщает конкретную причину повторного отказа:
  изменение order, barracks, roster, group authority, member identity,
  source position, combat/player/LOS fence, usability или проекции destination.
  Проверки и общий budget generation не ослаблялись.

### Static и Workbench

Baseline сохранён до изменений: `baseline.json`, `baseline-*.txt`,
`baseline.diff`. Повторный результат — `final-static.json` и `final-*.txt`.

Команды запускались через `powershell.exe -NoProfile -ExecutionPolicy Bypass -File`:

| Script | Baseline → после исправления |
|---|---|
| `Test-InfantryRecruitmentStatic.ps1` | PASS / 0 → PASS / 0 |
| `Test-MovementRecoveryContracts.ps1` | PASS / 0 → PASS / 0 |
| `Test-RecoveryEpisodeContracts.ps1` | PASS / 0 → PASS / 0; 9 route и 2 rearm analyzer cases |
| `Test-Stage3Static.ps1` | PASS / 0 → PASS / 0 |
| `Test-Stage35Static.ps1` | PASS / 0 → PASS / 0 |
| `Test-Stage35RecoveryPolicy.ps1` | PASS / 0 → PASS / 0 |
| `Test-AICommanderModeStatic.ps1` | PASS / 0 → PASS / 0 |
| `Test-MapPointOrdersStatic.ps1` | PASS / 0 → PASS / 0 |
| `Test-Stage4Static.ps1` | FAIL / 1 → FAIL / 1: прежний `STAGE4_ATTACKED_BASES` |
| `Test-Issue12RecoveryContracts.ps1` | Новая проверка: PASS / 0; первоначально 9, окончательно 10 отрицательных мутаций |

Терминальная команда Workbench: `ArmaReforgerWorkbenchSteamDiag.exe -noThrow
-wbsilent -gproj <root project> -addonsDir <exact graph roots> -addons <exact IDs>
-logsDir <fresh evidence directory> -wbModule=ScriptEditor -run -validate`.
Массивы аргументов сохранены в `wb-*-args.json`; native output пропущен через
`Out-File`, чтобы дождаться процесса и получить настоящий exit code.
Stock, Everon RHS, WCS RHS и stage: PASS / 0, `Script validation successful`,
нет `SCRIPT (E/F)`/`ENGINE (F)`. Последняя production версия также проверена
в `wb-soak-stock`, `wb-soak-everon-rhs`, `wb-soak-wcs-rhs` (PASS / 0).
Все 166 production `.c` в soak stage совпадают с checkout по SHA-256;
отчёт — `production-parity.json`.

### Focused runtime без клиента

Каждый сервер запускался отдельной foreground-сессией через
`Start-AICFRuntime.ps1 -Role Server -Variant RHS -RepositoryRoot <stage>`.
Свежие profiles находятся вне репозитория; manifests и все три log-файла
каждого остановленного процесса скопированы в evidence. Общие CLI flags:
`-aicfRecruitProbe 1 -aicfRecoveryEpisodeProbe 1 -aicfRequirePlayerForResult 0`.
Искусственные казармы/таймеры и инъекция результата используются только fixture;
движение, waypoint, покупка и transfer — production.

| Run | Дополнительные flags | Результат |
|---|---|---|
| `runtime-focused-2` | `-aicfRecruitHiddenProbe 1 -aicfRecruitHiddenContinue 1` | 7 cases PASS, обе стороны 10/10, 18 recruits |
| `runtime-multi` | Те же flags и `-aicfRecruitMultiMemberProbe 1` | 7 cases PASS, `relocated=2 expected=2`, обе стороны 10/10, 16 recruits |
| `runtime-exhaust` | Без hidden flags | 7 cases PASS, исчерпание подхода, запрет повтора и context rearm, обе стороны 10/10 |

Проверки включают потерю waypoint, реальный вызов production BT handler
на существующей activity с инъекцией `UNKNOWN`, stale generation, пустой
waypoint, чужой handler, unrelated failure, сохранение token/start time,
отсутствие ранней оплаты, общий deadline через 16 сбросов assignment,
защиту terminal hold от task audit/rebuild/MOB recovery и отмену пустого
Smart Action без изменения текущего waypoint.

Во всех трёх полных остановленных логах:

- `Test-Stage2Log.ps1 -LogPath <log>` — PASS / 0;
- `Test-InfantryRecruitmentLog.ps1 -LogPath <log> -RequireFullRosters` — PASS / 0;
- `Test-Issue12RecoveryContracts.ps1 -RuntimeLogPath <log>` — PASS / 0;
- `Test-RecoveryEpisodeContracts.ps1 -RuntimeLogPath <log>` — PASS / 0
  (для hidden runs добавлены `-RequireHiddenRecovery -HiddenContinuationOnly`);
- native exit 0 и `Game destroyed`; script/VM/AICF ошибок нет.

Первый `runtime-focused` выявил неверный приоритет recruitment no-op перед
проверкой terminal hold. Он помечен FAIL; порядок исправлен и повторно проверен.
Первый вызов полного analyzer на `runtime-focused-2` завершился ошибкой
PowerShell при печати `SwitchParameter` как `int`; исправлено преобразование
через `IsPresent`, затем тот же неизменённый лог прошёл анализатор.

В каждом focused run остаются 68 native resource/world diagnostics прежних
категорий: неизвестные keywords/classes, дублирование Hierarchy, несовпадение
GUID/name и font resource leak при остановке. Они не выдаются за script PASS
и не удалены из evidence.

### Продолжительная проверка

`AICF_RecoverySoakProbe.c` не инъецирует ошибки и не меняет gameplay: проверяет
отсутствие pending repair у исчерпанного episode и вызывает `RequestClose`
по сроку. Первая одновременная партия семи вариантов исчерпала 32 ГБ RAM;
EveronNorthRHS записал `Out of memory` до `ROSTER_READY`. Все семь процессов
остановлены по проверенным manifest/PID; результат партии — NOT PASS,
штатная остановка не заявляется. Полные логи сохранены в `oom-batch/`.
Повторные прогоны выполняются с ограниченным количеством одновременных серверов.

Первый WCS soak обнаружил ещё один путь обхода ownership: callback смены
владельца базы вызвал `DEFEND_POSTURE_CHANGED waypoint_replaced=0`. Вызов
`RecordStrategicAssignment` снял temporary hold, хотя группа продолжала
исполнять его waypoint. После этого MOB audit зафиксировал
`MOB_EGRESS_DEADLINE_MISSED`. Этот кандидат остановлен и помечен FAIL;
полный лог — `soak-ArlandWCSRHS-logs/`.

В окончательной версии все шесть автоматических входов planner
(`AssignOrder`, `AssignAICommanderOrder`, `ReconcileStrategicOrder`,
`ReconcileAICommanderOrder`, `AssignLossResponseOrder`,
`AssignAICommanderLossResponseOrder`) сохраняют exclusive hold ownership.
Выход из hold по-прежнему выполняет его владелец после проверки контекста.
Новая runtime case `POSTURE_AND_QRF_HOLD_OWNER` проверяет отсутствие изменения
waypoint и assignment при прямом вызове этих callback-путей.

Релевантные static checks повторены после этой правки (`v3-static.json`):
PASS / 0. Окончательная Workbench-компиляция `wb-v3-stock`,
`wb-v3-everon-rhs`, `wb-v3-wcs-rhs`, `wb-v3-stage` — PASS / 0.
Промежуточный `wb-v2-stock` успешно проверил скрипты, но завис при завершении;
он остановлен с exit -1, не считается PASS и сохранён отдельно.
Все 166 production файлов окончательного `soak-stage-v2` совпадают с checkout
(`final-production-parity.json`). `RunFinal.ps1` запускает каждый окончательный
вариант после освобождения места предыдущим процессом: WCS — 30 минут,
остальные — 15 минут; каждый запуск получает отдельный свежий manifest.

Изменённые production файлы относительно исходной задачи: `AICF_MatchController.c`,
`AICF_AINodeLifecycle.c`, `AICF_InfantryRecruitmentOrder.c`,
`AICF_InfantryRecruitmentService.c`, `AICF_OrderPlanner.c`, `AICF_GroupSlot.c`;
добавлены `AICF_InfantryRecruitmentRecovery.c`, `AICF_RecruitmentApproachFailure.c`
и `AICF_RouteRecoveryEpisode.c`. Проверки и evidence contract:
`Test-Stage2Log.ps1`, `Test-InfantryRecruitmentLog.ps1`,
`Test-RecoveryEpisodeContracts.ps1`, `Test-Issue12RecoveryContracts.ps1`,
`AICF_InfantryRecruitmentRuntimeProbe.c`, `AICF_RecoveryEpisodeProbe.c`,
`AICF_RecoverySoakProbe.c`, `.gitignore` и этот документ. Fixture-файлы
не входят в production addon; native logs, caches и RDB не включаются в Git.

Реальный игрок рядом/в LOS и client/JIP — NOT RUN по указанию не подключать
клиент. Отказ player fence проверен инъекцией watchdog; это не заменяет
ручную проверку с игроком. Статус `ACCEPTED` не присваивается.

Окончательный `runtime-final-focused` выполнен уже со всеми шестью guards:
8 runtime cases PASS, `relocated=2 expected=2`, фактический локальный перенос
наблюдён до оплаты, затем обе стороны пополнились до 10/10 в тех же группах.
Все четыре анализатора выше завершились с exit 0; native exit 0,
`Game destroyed`, script/VM ошибок нет. Команды и полные результаты сохранены
в `runtime-final-focused-*`; предыдущие focused-прогоны остаются evidence
для отдельной ветки исчерпания ремонта и context rearm.

### Новые дефекты, найденные в окончательных прогонах

Они записаны в [issue #14](https://github.com/Ishafel/Arma-Reforger-AI-Conflict/issues/14).
Результаты соответствующих общих runtime gates остаются FAIL; анализаторы
и их пороги не ослаблялись.

- RHS: пять `ORDER_RECOVERED` к одной базе при лимите три. Отряд DEFEND
  физически прибыл (около 1,6 м от цели), но новый `DEFEND_ACTION` завершался
  за 4–10 секунд; после `AT_OBJECTIVE` task audit повторял rebuild. Это отдельный
  цикл после прибытия, а не false completion далёкого route leg из #11.
- EveronRHS: два failed-move VM exception. Сопоставление по полному логу
  `BUILDER_SPAWN_REQUESTED/READY` установило строительную группу; её `slot=NULL`
  означает отсутствие владельца managed infantry, а не потерю recruitment slot.
  Позднее строитель вернулся домой и был retired. Оба анализатора FAIL именно
  из-за двух SCRIPT(E), несмотря на штатное завершение сервера с exit 0.
- WCS: Stage 2 обнаружил шесть пар «фракция/слот/цель» с 4, 7, 6, 4, 17 и 6
  подтверждёнными recovery (лимит три). В наиболее частом случае DEFEND снова
  завершался за 4–10 секунд. Причина каждой агрегированной пары требует
  отдельного разбора; все они не объявляются автоматически одним дефектом.
  За 30 минут нет ошибок MOB rebuild/deadline и meaningful-task deadline;
  есть 18 физических подтверждений MOB egress. Recruitment analyzer PASS,
  но общий runtime gate остаётся FAIL.

Дополнительная изолированная fixture `defend-stage` переопределяет только
`SCR_TimedWaypoint.SetHoldingTime` для печати requested/actual. Production
не изменён. Workbench `wb-defend-probe` — exit 0. Runtime подтвердил
`requested=3600 actual=3600 parameters=1`: гипотеза отсутствующего параметра
не подтверждена. После получения диагностики этот дополнительный процесс
остановлен по точному manifest/PID; его лог **не считается полным runtime PASS**.
Fixture остаётся только в локальном evidence и не входит в Git.

На EveronNorthRHS десять `Incorrect tile position` и десять `Failed to load ''`,
а также ошибка material присутствовали и в сохранённом прогоне 30.09.2026,
и в промежуточном кандидате. Эти native diagnostics не исправлялись изменением
vendor/world; они сохраняются в полных логах и не объявляются устранёнными.

### Итоговая матрица окончательной версии

Все семь серверов запускались через `tools/Start-AICFRuntime.ps1 -Role Server`
с соответствующим `-Variant`, отдельным profile и `-RepositoryRoot <soak-stage-v2>`.
Fixture задавала `-aicfRecoverySoakMs 900000` (WCS: `1800000`),
`-aicfRequirePlayerForResult 0`; gameplay fault injections отсутствовали.
В каждом полном остановленном логе подтверждены точные `CLI Params` из manifest,
`ROSTER_READY`, `RECOVERY_SOAK_FINISHED owner_failures=0`, `Game destroyed`
и native exit 0. Длительность измерена от готового roster.

| Variant | Минуты | Script/VM ошибки | Stage 2 | Recruitment | Runtime gate |
|---|---:|---:|---|---|---|
| Stock | 15 | 0 | PASS / 0 | PASS / 0 | PASS |
| RHS | 15 | 0 | FAIL / 1: churn | PASS / 0 | FAIL |
| ArlandWCSRHS | 30 | 0 | FAIL / 1: churn | PASS / 0 | FAIL |
| Everon | 15 | 0 | PASS / 0 | PASS / 0 | PASS |
| EveronNorth | 15 | 0 | PASS / 0 | PASS / 0 | PASS |
| EveronRHS | 15 | 2 | FAIL / 1: builder VM | FAIL / 1: builder VM | FAIL |
| EveronNorthRHS | 15 | 0 | PASS / 0 | PASS / 0 | PASS |

Для каждого варианта выполнены:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Test-Stage2Log.ps1 -LogPath '<полный остановленный console.log>'
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Test-InfantryRecruitmentLog.ps1 -LogPath '<полный остановленный console.log>'
```

`final-*-manifest.json`, `final-*-logs`, `final-*-exit.txt`,
`final-*-Test-*.txt`, `final-matrix.json` и `final-target-metrics.json`
хранят полное локальное evidence. Все 166 production `.c` сохранили SHA256
тестовой версии после коммита (`final-commit-parity.json`, mismatches=0).
Общий объём окончательной матрицы — 120 серверо-минут, без клиента.

Native `(E)` строки отдельно сохранены: Stock 14, RHS 68, WCS 102,
Everon 79, EveronNorth 47, EveronRHS 242 (включая две SCRIPT(E)),
EveronNorthRHS 200. Это resource/world/navmesh diagnostics, а не заявление
о чистом engine log; PASS таблицы относится к указанным project analyzers
и завершению прогона. Существующий static failure `STAGE4_ATTACKED_BASES`
также сохранён. Реальная пользовательская сессия на исходной геометрии,
ручная визуальная приёмка, реальный player/LOS и client/JIP — NOT RUN.

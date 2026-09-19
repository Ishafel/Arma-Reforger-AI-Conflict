# Проверка остальных зданий на Arland — 2026-09-19

Запрос: проверить малые/большие казармы и остальные поддержанные здания
после исправления поиска Light Factory. Проверяется текущая production версия
на Stock Arland отдельно для `US` и `USSR`. Исходный HEAD:
`8cdc67a93e2611ad498a3ae02f696c1bf8e8f034`; рабочее дерево содержит ранее
выполненные исправления поиска. Пользовательские документы не изменялись.

## Условия

Game, Server и Tools — `1.8.0.13`. Полные manifests, исходный dirty status,
source hashes, baseline и logs находятся в
`.codex-runtime/construction-matrix-20260919/`.
Все 128 production `.c` Core совпадают с изолированной runtime копией.
Production addon не содержит test fixture.

Fixture назначает обычные базы, кроме HQ и control points, ближайшей стороне
и пополняет supplies раз в минуту. По очереди запрашиваются
`SMALL_BARRACKS`, `ARMORY`, `LIGHT_DEPOT`, `LARGE_BARRACKS`, `HEAVY_DEPOT`.
Одновременно проверяется один order на сторону. Этап длится до completion
обеих сторон или пяти минут. Поиск, safety/coverage guards, оплата, создание
unfinished layout и работа строителя выполняются production кодом.
Combat и смена владельца остаются активными и могут прерывать строительство.
Это обеспеченный припасами диагностический прогон, не измерение обычной
экономики или частоты строительства.

```powershell
$evidence = Join-Path $PWD '.codex-runtime/construction-matrix-20260919'
$runtime = Join-Path $evidence 'runtime-source'
# runtime-source содержит копии Core/Arland и fixture в Core/Construction.
& .\tools\Start-AICFRuntime.ps1 -Role Server -Variant Stock `
  -RepositoryRoot $runtime -AICommanderMode BOTH `
  -AdditionalArguments @(
    '-aicfConstructionProbe','1', '-aicfConstructionProbeMatrix','1',
    '-aicfConstructionProbeRefill','1', '-aicfConstructionProbeMs','1800000',
    '-aicfConstructionProbeTrace','1', '-aicfRequirePlayerForResult','0')
```

Server profile: `C:\Users\retar\AppData\Local\AICF\Server-20260919-114316-008`.
Launcher output: `matrix-launch.txt`; exact CLI — `runtime-manifest.json`.
Запуск содержит `ROSTER_READY`. Новый client не запускался.

## Изменения проверки

| Файл | Изменение |
|---|---|
| `tools/fixtures/AICF_ConstructionRuntimeProbe.c` | Последовательная matrix, completion каждой стороны, отдельный повтор типа и диагностика coverage |
| `tools/fixtures/AICF_ConstructionManualProbe.c` | Read-only запись HQ и точного transform принятого player проекта |
| `tools/Test-ConstructionLog.ps1` | `-RequireAllFactions`, проверка faction/type исходного order и completion |
| `tools/Test-ConstructionContracts.ps1` | Полная synthetic matrix, пропущенная пара, подмена faction |
| `tools/Test-ConstructionStatic.ps1` | Запрет новой manual fixture в production Core |
| `docs/TESTING.md`, этот отчёт | Команды, условия и границы evidence |

## Проверки

До и после изменений: `Test-ConstructionStatic.ps1`,
`Test-ConstructionContracts.ps1`, `Test-BaseBuildersStatic.ps1`,
`Test-Stage4Static.ps1` — **PASS / 0**. Contracts после изменения:
16 log inputs, positive и шесть negative static inputs.
`Test-AICommanderModeStatic.ps1` сохраняет **FAIL / 1** с единственным
`AI_COMMANDER_UI_STATE`. `git diff --check` — **PASS / 0**.

Терминальный Workbench `-noThrow -wbsilent -wbModule=ScriptEditor -run
-validate` для изолированного Stock graph — **PASS / 0**:
`wb-matrix/console.log`, `wb-coverage-final/console.log` и `wb-manual/console.log` содержат
`Script validation successful`. Production `.c` в этой проверке не менялись;
четыре production compile gates предшествующего исправления описаны в
[отчёте terrain](LIGHT_FACTORY_TERRAIN_20260919.md).

## Runtime

Процесс штатно завершился через `RequestClose`, native exit **0**, полный
остановленный log — `matrix-console.log` (оригинал в server profile).
1466 ticks за 1506161 мс, max window 96 queries. Max tick 277 мс включает
синхронные операции; это не подтверждение wall-clock лимита 8 мс.

| Тип | США | СССР |
|---|---|---|
| Малые казармы | 4 orders, 0 placement; 2 NO_SAFE_SITE и 2 lifecycle cancellations | Построены: `construction-1`, 7 кандидатов, 1000 → 750 supplies |
| Оружейная | 0 orders; причина отсутствия нового заказа в этом прогоне отдельно не доказана | 0 orders; новая постройка не проверена |
| Light Depot | 2 orders, 0 placement; NO_SAFE_SITE | Построен: `construction-59`, 49 кандидатов, 1200 → 1050 |
| Большие казармы | 1 order, 220 кандидатов, NO_SAFE_SITE | Построены на следующей базе: `construction-92`, 79 кандидатов, 1000 → 525 |
| Heavy Depot | 2 orders, 0 placement | 3 orders, 0 placement |

Для трёх готовых зданий подтверждены единственная оплата, builder progress
с активным инструментом, builder completion и `STOCK_SERVICE_ONLINE`.
Большие казармы завершились уже после перехода к этапу Heavy Depot:
`CONSTRUCTION_MATRIX_PHASE end` — промежуточный marker, итог следует из
полного журнала. Metadata обеих больших казарм valid: 400 meshes US и
346 USSR. Metadata Heavy Depot также valid: 124 US и 123 USSR.

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Test-ConstructionLog.ps1 `
  -LogPath '.codex-runtime/construction-matrix-20260919/matrix-console.log' `
  -ExpectedMode BOTH -RequireCompletion -RequireAllTypes -RequireAllFactions
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Test-BaseBuildersLog.ps1 `
  -LogPath '.codex-runtime/construction-matrix-20260919/matrix-console.log' `
  -MinimumCompleted 3 -RequireToolUse
```

Полная matrix audit — **FAIL / 1**: семь отсутствующих faction/type pairs,
`CONSTRUCTION_MISSING_TYPES`, `CONSTRUCTION_ENGINE_ERROR`.
Без требований полноты типов completion audit также **FAIL / 1**, только
`CONSTRUCTION_ENGINE_ERROR`. Builder audit — **FAIL / 1**, только
`BUILDERS_RUNTIME_SCRIPT_ERROR`. Оплата/worker/online цепочки трёх построек
прошли проверки, но не заменяют общий FAIL.

Полный log содержит 30 error lines: 4 ENTITY, 8 WORLD, 3 RESOURCES и 15 SCRIPT.
Помимо известных shutdown resupply ошибок зарегистрированы `Wrong class of
provided Waypoint!`, `ORDER_REPAIR_ACCOUNTING_INVARIANT_FAILED` и
`MEANINGFUL_TASK_DEADLINE_MISSED` с bridge events. Они не исправлялись в рамках
диагностики строительства и не объявляются сохранённым baseline без отдельного
воспроизведения. Индексы: `matrix-errors.txt`, `matrix-coverage.json`;
verdict опирается на полный log.

## Ручное сравнение по запросу пользователя

После matrix пользователь предложил самостоятельно найти площадку.
Запущена отдельная копия `manual-source`, без supply preparation и таймера
остановки: `aicfConstructionProbe 0`, `aicfConstructionProbeType 3`,
`aicfConstructionProbeTrace 1`. Изменён только начальный приоритет на большие
казармы и добавлены test-only наблюдения; gameplay placement/оплата сохранены.

Server profile: `Server-20260919-120836-806`, client:
`Client-20260919-120908-049`, оба под `C:\Users\retar\AppData\Local\AICF\`.
Оба запущены canonical launcher в отдельных терминальных сессиях.
Client launcher подтвердил exact server CLI, живой process и `ROSTER_READY`;
server подтвердил `Player connected`, client дошёл до deploy menus.
Полные команды — `manual-server-manifest.json`, `manual-client-manifest.json`.

Фракции поменялись стартовыми HQ: US на юге, USSR на севере. Пользователю
предложено поставить **Large Barracks за СССР на северной главной базе**
(base 250, origin `<1157.67,1.156,3282.46>`), соответствующей проблемной
площадке предыдущего запуска. Пользователь разместил два проекта
`{D44C687485600B72}...E_LivingArea_L_Conflict_USSR_01.et`:

| Entity | Position | Angles yaw/pitch/roll | Worker completion |
|---|---|---|---|
| `4519` / `0x40000000000011A7` | `<1281.96,9.58667,3268.84>` | `<-4.99619e-06,1.78991,-4.46499>` | `t_ms=154693`, tool/item active |
| `4973` / `0x400000000000136D` | `<1222.1,0.939056,3268.57>` | `<-5.00834e-06,-7.82554e-08,-0.895174>` | `t_ms=196881`, tool/item active |

Provider `3074`, position `<1158.09,1.34825,3279.28>`. Оба проекта достроены
одним worker generation; это подтверждает player admission и достижимость
их рабочих endpoints. Автономный `construction-2` был отменён с
`reason=PLAYER_PLACEMENT` ещё на подготовке metadata, candidates=0.
Это не доказательство, что текущий поиск рассмотрел и отверг именно эти
transforms. Затем выполнено отдельное повторение поисковых проверок:
[4 cases с точными position и записанными наклонами](CONSTRUCTION_MANUAL_REPLAY_20260919.md).
Другой prefab стороны US в предыдущем matrix также не эквивалентен точному
повторению USSR prefab на том же HQ.

Сохранены `manual-placements.txt`, `manual-completions.txt` и
`manual-server-live-snapshot.log`. Позже сессия остановлена для replay;
полные logs и способ остановки сохранены в `.codex-runtime/construction-replay-20260919/`
как `play-server-stopped.log`, `play-client-stopped.log`, `play-stop.json`.
Принудительная остановка не является runtime PASS. После replay запущена
новая игровая сессия; её manifests перечислены в replay report.
Отдельный planned coverage follow-up уступил место ручной проверке и **NOT RUN**.

## Границы

Everon/RHS runtime, construction client/JIP assertions, ручная проверка входов/фундаментов,
проезд техники, заказ personnel/vehicles из новых services и длительный
soak — **NOT RUN**. Работа не объявляется `ACCEPTED`.

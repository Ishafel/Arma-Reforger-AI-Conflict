# Поиск площадок строительства — 2026-09-19

Исходный HEAD: `8cdc67a93e2611ad498a3ae02f696c1bf8e8f034`.
Game, Server и Tools: `1.8.0.13`. Проверки выполняются только из терминала.
Evidence сохраняется в `.codex-runtime/construction-search-20260918/`;
каталог создан до полуночи. Два пользовательских prompt-файла не изменены.

## Исправления

- Planner продвигает несколько pending orders в общей квоте, вместо остановки
  после первой базы. Начало обхода сдвигается после первой active base.
  Сохраняются четыре новых кандидата, один metadata batch и одна попытка
  placement за tick. Неудачный placement также расходует эту попытку.
- Проверка доступа к существующим services/spawn выполняется до дорогого
  terrain/navmesh поиска. Если перед commit участок заняли, снимается только
  site reservation; тот же order продолжает поиск. Identity, ownership,
  inventory и economy revalidation остаются обязательными.
- Кандидаты привязаны к геометрическому центру footprint, а не prefab pivot.
  Поиск не исключает целое кольцо шириной half-diagonal вокруг provider.
  Каждые 16 центров проверяются с четырьмя поворотами за 64 попытки, затем
  исследуются новые центры. Cursor каждой базы/типа сохраняется между orders.
- Границы provider/world проверяются по углам повёрнутого footprint/выезда,
  вместо пустых углов охватывающего world AABB.
- Default attempts: `96 → 256`, CLI `aicfConstructionAttempts`: `4..512`.
  Deadline каждой фазы — 120 секунд, decision/cooldown — 60 секунд, общая квота —
  96 queries/window. Дополнительные попытки не увеличивают квоту одного tick.
- Холодная metadata и последующий поиск имеют отдельные deadline: после
  успешной подготовки geometry поиск получает свой полный срок. Ни одна
  фаза не может продлевать себя бесконечно; суммарный предел — два deadline.
  Событие `CONSTRUCTION_SEARCH_READY` сохраняет `search_deadline_ms`.
- Лимит 256 входных collision volumes исключал штатные большие казармы ещё
  до поиска. Теперь cap равен 2048, как у ограниченного preview tree.
  Объединение работает с максимум 48 объёмами одновременно и сохраняет cursor
  оставшихся входов. Все исходные объёмы покрыты итоговыми, не более 24 OBB.
  Полная исходная геометрия для проверки дорог/выездов также сохраняется.

Terrain/water, физические препятствия, service/spawn clearance, дороги,
navmesh и completion clearance не отключены. Проверка не гарантирует
нахождение всех мест, доступных при ручном placement.

## Команды

До и после правки:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Test-ConstructionStatic.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Test-ConstructionContracts.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Test-BaseBuildersStatic.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Test-AICommanderModeStatic.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Test-Stage4Static.ps1
git diff --check
```

Workbench запускается командами `DEVELOPMENT.md` и `RHS_EVERON.md` с
`-noThrow -wbsilent -wbModule=ScriptEditor -run -validate`, отдельным
`-logsDir` и `| Out-Null` для ожидания процесса. Native exit сохраняется
отдельно. Перед runtime fixture временно копируется из
`tools/fixtures/AICF_ConstructionRuntimeProbe.c` в Core `Construction/`.
После её удаления production graphs валидируются повторно.

Контрольный и промежуточный прогоны:

```powershell
& .\tools\Start-AICFRuntime.ps1 -Role Server -Variant Stock `
  -AdditionalArguments @('-aicfConstructionProbe','1',
    '-aicfConstructionProbeRefill','1','-aicfConstructionProbeMs','300000',
    '-aicfRequirePlayerForResult','0')
```

Прогон крупных построек дополнительно задаёт `-aicfConstructionProbeType 3` и
`-aicfConstructionProbeMs 480000`: большие казармы проверяются первыми,
затем применяется штатная последовательность следующих типов. Fixture
обеспечивает supplies; поиск, placement, оплата и completion идут production
path. Каждый server получает fresh profile и завершается через
`RequestClose()` самой fixture. Полные CLI и `AICF_RUNTIME_MANIFEST_JSON`
сохранены в `runtime-*-launch.txt`.

Завершающий focused run `runtime-delivery-launch.txt` использует те же flags,
но `aicfConstructionProbeMs=240000`. Его цель — раздельные deadline и
квоты подготовки/поиска больших казарм на окончательной версии planner.

Анализируется полный остановленный server log:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Test-ConstructionLog.ps1 `
  -LogPath '<полный console.log из manifest>' -ExpectedMode BOTH -RequireCompletion
```

## Контрольный baseline и промежуточный результат

Static baseline: ConstructionStatic, ConstructionContracts, BaseBuildersStatic
и Stage4Static — PASS / 0. AICommanderModeStatic — FAIL / 1,
`AI_COMMANDER_UI_STATE`; этот несвязанный отказ сохраняется.

Stock baseline profile `Server-20260918-202141-626`: две placement и две
completion малых казарм, по одной на сторону. Большие казармы US имеют
`meshes=400 valid=0`, затем `UNSUPPORTED_GEOMETRY`, `candidates=0`.
Native exit 0. `Test-ConstructionLog -RequireCompletion` — FAIL / 1,
`CONSTRUCTION_ENGINE_ERROR`: две shutdown ошибки
`SCR_BaseResupplySupportStationComponent needs a entity catalog manager!`.
Полный лог: `C:\Users\retar\AppData\Local\AICF\Server-20260918-202141-626\logs\logs_2026-09-18_20-21-41\console.log`.

Промежуточный profile `Server-20260919-020825-407`: две placement, одна
completion, log audit PASS / 0, native exit 0. Большие казармы USSR с
346 meshes уже дают `valid=1`. Это версия до финального уточнения fairness,
ограничения неудачных placement и увеличения attempts. Её результаты не
подменяют финальный runtime. Полный лог скопирован в `intermediate-console.log`.

Сессии имеют случайное распределение стартовых баз: faction/base pairs
отличаются. Эти прогоны не являются количественным A/B benchmark частоты
строительства. Max tick промежуточной fixture включает синхронный native
regression test с 400 объёмами; его нельзя считать production frame cost.

Fixture проверяет production helpers: endpoint/exit — 6 случаев,
candidate coverage — 8, oriented bounds — 6, compaction — 4, включая покрытие
всех 400 исходных объёмов. Дополнительно во время runtime проверяется общий
candidate budget. Static contracts включают отрицательные inputs с нарушенной
provider identity и тремя снятыми scheduler budget guards.

Первый Workbench в sandbox завершился exit -1 из-за Steam API; повторный
терминальный запуск с доступом к Steam прошёл Validate. В промежуточной
test-only fixture исправлена ошибка `%` внутри float-контекста `Vector()`;
повторный compile успешен. Эти неудачные логи сохранены в evidence.

## Достройка крупных зданий

Profile `Server-20260919-021427-937`, `runtime-final-launch.txt`, 480 секунд:
native exit 0, `Test-ConstructionLog -RequireCompletion` — PASS / 0,
10 orders, 3 placement, 2 completion. Большие казармы обеих сторон прошли
metadata (`US: 400 meshes`, `USSR: 346 meshes`, `valid=1`).

На захваченной базе `0x200000000000010E` найдена площадка больших казарм USSR
за 82 кандидата / 52 секунды. Supplies `1000 → 525`, props `49 → 338`;
строитель физически подошёл, использовал инструмент, затем появились
`BUILDER_COMPLETED` и `CONSTRUCTION_COMPLETED service_online=1`. Также
достроены малые казармы USSR. Полный остановленный лог —
`large-completion-console.log`; точный исходный путь —
`large-completion-logpath.txt`.

Этот run выполнен до последних двух изменений planner: отдельного search
deadline после metadata и повторения ещё не принятого решения при исчерпании
query budget. Геометрия, распределение candidates, attempts=256, stock
placement и builder completion соответствуют окончательной версии.
На стартовых HQ large/heavy получали безопасные отказы; наличие свободного
места на каждой базе этим тестом не доказано.

## Финальная проверка deadline

Profile `Server-20260919-022305-408`, `runtime-delivery-launch.txt`:
native exit 0, `Test-ConstructionLog` — PASS / 0, два orders без placement.
`-RequireCompletion` здесь не задан: это focused проверка поиска и его срока,
а не доказательство ещё одной достройки. Полный лог — `delivery-console.log`,
исходный путь сохранён в `delivery-logpath.txt`.

Metadata USSR/US заняла 92.6/108.0 секунды. После `CONSTRUCTION_SEARCH_READY`
каждый поиск получил ещё 120 секунд и завершился через 120.6 секунды
с учётом следующего scheduler tick. US рассмотрел 190 кандидатов, USSR — 230;
в предыдущем холодном прогоне — 24 и 71 соответственно. Оба окончательных
отказа — `NO_SAFE_SITE` с фактическими препятствиями; доступность больших казарм
на стартовых HQ не заявляется. Max window — 60 queries при лимите 96,
нарушений candidate budget нет; все 24 native helper checks прошли.

Полные console/launcher logs совпадают по числу диагностик: у baseline
17 error lines (включая две SCRIPT на shutdown), у каждого из трёх следующих
прогонов — 15 (4 ENTITY, 3 RESOURCES, 8 WORLD), SCRIPT/VM/fatal — 0.
Это сохранённые resource/world diagnostics, а не полностью бесшумный engine run.
Индексы: `*-all-engine-errors.txt`, сводка — `runtime-summary.json`.

После остановки удалена только временная Core-копия runtime fixture.
ConstructionStatic снова PASS / 0, AICommanderModeStatic сохраняет только
исходный `AI_COMMANDER_UI_STATE` / 1. Временный дополнительный отказ
`AI_COMMANDER_COMPONENT` во время runtime был вызван тестовым `modded
SCR_GameModeCampaign` из fixture; соответствующий вывод сохранён отдельно.
ConstructionContracts выполнялись на отдельной копии без fixture: 13 log inputs,
один positive и четыре negative static inputs — PASS / 0. Все проверяемые
изменённые production файлы и три аудитора совпали по SHA-256 с рабочими.
BaseBuildersStatic и Stage4Static — PASS / 0 до и после.

Заключительный Workbench без fixture: Arland, Everon, ArlandRHS и EveronRHS —
PASS / native exit 0, `Script validation successful`, SCRIPT E/F, ENGINE F,
VM/null errors отсутствуют. Воспроизводимая terminal команда —
`& .\.codex-runtime\construction-search-20260918\Validate-Production.ps1`;
она выполняет четыре Diag-команды из `DEVELOPMENT.md`/`RHS_EVERON.md`.
Точные arguments, exit и полные logs: `wb-production-*/`; сводка —
`production-workbench.json`. `git diff --check` — PASS / 0.

## Изменённые файлы

Все пути относительно корня репозитория:

- `AIConflictCore/Scripts/Game/AIConflict/Config/AICF_ConstructionConfig.c` — attempts.
- `AIConflictCore/Scripts/Game/AIConflict/Construction/AICF_ConstructionPlanner.c` — shared budgets, fairness, retry, deadline.
- `AIConflictCore/Scripts/Game/AIConflict/Construction/AICF_ConstructionSiteSearch.c` — кандидаты, центр footprint, oriented bounds.
- `AIConflictCore/Scripts/Game/AIConflict/Construction/AICF_ConstructionMetadata.c` — bounded compaction крупных построек.
- `tools/Test-ConstructionStatic.ps1` — новые scheduler/geometry guards.
- `tools/Test-ConstructionContracts.ps1` — отрицательные scheduler inputs.
- `tools/fixtures/AICF_ConstructionRuntimeProbe.c` — native geometry contracts и контроль candidate budget.
- `docs/ARCHITECTURE.md`, `docs/CONSTRUCTION_VALIDATION.md`, `docs/CONSTRUCTION_SITE_SEARCH_VALIDATION.md`, `docs/TESTING.md`, этот отчёт — контракты, команды и evidence.

## Ограничения проверки

Client/JIP, ручная проверка входов, проезда и анимации, полная матрица пяти
типов × фракции × карты, prepared moving-blocker/capture/player-placement
сценарии, runtime Everon/RHS после этой правки и длительный soak — **NOT RUN**.
Они не заменяются static PASS,
native helper contracts или успешным одиночным строительством.

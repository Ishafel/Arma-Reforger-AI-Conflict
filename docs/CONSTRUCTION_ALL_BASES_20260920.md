# Строительство на всех точках полного stock Everon — 2026-09-20

## Результат

За **900,194 с** завершены **32 новые постройки на 24 из 39 точек**. Все 24
успешные точки находятся вне HQ. На остальных **15 точках**, включая оба HQ,
за 15 минут не размещено ни одной новой композиции. Все 39 получили решения
на строительство, имели US owner, provider и полный запас supplies на старте.

Первая готовая постройка на успешной точке появилась через **54,192–619,060 с**
от подготовки острова. Самая быстрая — Durras (54,192 с), самая поздняя первая
постройка — Military Base Levie (10 мин 19 с). Завершены 23 `SMALL_BARRACKS`,
5 `LIGHT_DEPOT`, 3 `LARGE_BARRACKS`, 1 `HEAVY_DEPOT`. Новая `ARMORY` в этом
прогоне не завершалась; fixture не заставляет дублировать stock coverage.

Всего было 207 решений: 32 завершены, **145 отменены с
`SEARCH_BUDGET_EXHAUSTED`**, 30 оставались в работе и сняты с `STOP` на границе
измерения. `SEARCH_AREA_EXHAUSTED` и `NO_SAFE_SITE` — 0. Поэтому отсутствие
результата на 15 точках не доказывает отсутствия безопасных площадок.
Основные зарегистрированные отказы кандидатов — препятствия, дороги и
базовые проходы; подробные счётчики находятся в `metrics/bases.csv`.

Для 32 завершённых заказов средние времена: решение → размещение **81,51 с**,
подход строителя **8,84 с**, работа → доступная служба **21,09 с**. Эти средние
не включают неудачные предыдущие попытки; полное ожидание точки отражает
колонка первой готовой постройки. Общий scheduler накопил 4371 мс измеренного
script wall time за 898 updates, максимум одного update — 60 мс. Средний
server FPS по 90 samples — 59,78, максимальный engine frame — 677,3 мс.
Это не измерение общего OS CPU и не доказательство соблюдения 8 мс каждым tick.

**Вывод:** строительство вне HQ подтверждено; быстрое строительство на
каждой точке полного острова не подтверждено. Общий бюджет поиска и время
жизни попыток требуют дальнейшей работы. Production алгоритм в этом замере
не менялся.

Сервер работал 14:03:51–14:19:20 МСК и штатно остановлен, native exit **0**.
Полный остановленный лог содержит две ошибки stock
`SCR_BaseResupplySupportStationComponent` при teardown. Строгие runtime gates
остаются **FAIL**, результат не объявляется `ACCEPTED`.

## Условия замера

Пользователь выбрал полный stock Everon без RHS. Изолированный authoritative
server использует неизменённый production snapshot: 171 исходный файл совпадает
по SHA-256 с рабочим репозиторием. Дополнительно загружена только
`tools/fixtures/AICF_ConstructionAllBasesProbe.c`. Старые fixtures матрицы,
принудительного выбора типа и последовательного строительства не загружены.

После настоящего `ROSTER_READY` fixture получает все активные базы через stock
`GetBaseManager().GetBases()`: 39 initialized баз, включая оба HQ и control
points. Все передаются US, supplies заполняются до собственного stock capacity
каждой базы и пополняются раз в 5 с. Все 39 имеют building provider радиусом
150 м. Неактивные стартовые HQ, удалённые штатной инициализацией миссии,
не создаются заново; координаты и world не меняются.

Для мирного измерения US получает дружеские отношения с остальными фракциями
через `SCR_FactionManager.SetFactionsFriendly`. Оба механизма окончания матча
приостановлены только тестовой fixture. Это не проверка строительства в бою.
Режим commander — `US`; production проверки identity, navmesh, footprint,
terrain, препятствий, оплаты, незавершённой композиции и физической работы
строителя остаются включены. Приоритеты типов и существующее coverage
сохраняются. Уже имеющиеся службы не удаляются ради новых построек.

Все базы конкурируют за штатный общий бюджет: 4 candidate transforms, 96
query units и 8 мс на tick. Decisions распределены по штатному интервалу 60 с.
Отдельный заказ имеет окно поиска 120 с с предусмотренными production
продолжениями; ресурсы не отменяют вычислительные ограничения.

## Запуск и воспроизведение

Evidence root: `.codex-runtime/everon-all-us-20260920/`.

- `source/` — неизменяемая копия production и одна fixture.
- `production-before.csv`, `production-after.csv`, `hash-parity.json` —
  соответствие 171 production файла до/после.
- `run-01/source-hashes.csv` — окончательные SHA-256 всех 172 файлов до запуска.
  Корневой `source-hashes.csv` относится к первой попытке компиляции; для
  runtime использовать именно индекс внутри `run-01`.
- `wb-all-us-fixed-Everon-command.json` — точные Workbench executable/arguments.
- `run-01/command.json`, `launch.txt`, `startup-evidence.json`, `exit.json` —
  запуск, `AICF_RUNTIME_MANIFEST_JSON`, native CLI, PID/readiness и exit.
- `run-01/profile/logs/logs_2026-09-20_14-03-51/` — полные native
  `console.log`, `script.log`, `error.log`.

Точная команда внутри сохранённого `run.ps1` (выполнять из корня репозитория;
для повторения нужен новый, ещё не существующий profile):

```powershell
$test = Join-Path (Get-Location) '.codex-runtime/everon-all-us-20260920'
& ./tools/Start-AICFRuntime.ps1 -Role Server -Variant Everon `
  -RepositoryRoot "$test/source" -ProfileRoot "$test/run-02/profile" `
  -ServerPort 22430 -AICommanderMode US `
  -AdditionalArguments @('-aicfAllBasesProbe','1','-aicfAllBasesProbeMs','900000',
    '-aicfRequirePlayerForResult','0','-addr','127.0.0.1:22430','-noThrow')
```

Обычный production запуск не загружает fixture из `tools/fixtures`. Для новой
версии snapshot нужно заново скопировать addons и поместить только эту fixture
в `source/AIConflictCore/Scripts/Game/AIConflict/Fixtures/`, затем выполнить
терминальный Workbench Validate/Compile. Нельзя подкладывать её в production
`AIConflictCore/Scripts` или менять snapshot уже запущенного сервера.

Fixture останавливает planner и вызывает `GetGame().RequestClose()` через
900000 мс от подготовки. Это конечный замер, а не постоянно доступный сервер.
Client в этом прогоне не запускался.

## Как считаются времена

`tools/Measure-ConstructionAllBases.ps1` берёт один снимок полного лога,
проверяет полноту inventory и сопоставляет базы с реальными
`CONSTRUCTION_DECISION`. События coverage без решения не считаются заказами.

Начало отсчёта — `CONSTRUCTION_ALL_BASES_PREPARED`, `t_ms=4623`. Это исключает
загрузку мира, но включает штатную задержку первого решения и конкуренцию
всех баз. Первая площадка — `CONSTRUCTION_PLACED`; первая готовая постройка —
`CONSTRUCTION_COMPLETED` / `service_online=1`. Подход и работа сопоставляются
по layout entity ID с `BUILDER_WORK_STARTED` и `BUILDER_COMPLETED`.

`BUILT` означает хотя бы одну **новую** завершённую постройку. Это не означает
наличие всех пяти типов. Пустое время — этап не завершён за окно наблюдения,
а не ноль. `STOP` — остановка остаточного заказа на границе замера, а не
геометрический отказ. `coverage-first-observed.csv` содержит первое наблюдение
каждого типа; оно может произойти позже старта, после предыдущих построек.

```powershell
$log = "$test/run-01/profile/logs/logs_2026-09-20_14-03-51/console.log"
pwsh -NoProfile -File tools/Measure-ConstructionAllBases.ps1 `
  -LogPath $log -OutputDirectory "$test/metrics"
```

Результат: `bases.csv/json/md` по всем 39 точкам, `orders.json` с отдельными
заказами и фазами, `coverage-first-observed.csv`, `summary.json`; подкаталог
`search/` содержит общие search/frame counters. Отсутствующие этапы не
подменяются нулями, базам без новых построек отведены отдельные строки.

## Изменённые файлы в этой задаче

- `tools/fixtures/AICF_ConstructionAllBasesProbe.c` — изолированный стенд.
- `tools/Measure-ConstructionAllBases.ps1` — полный учёт точек и времени.
- `tools/Measure-ConstructionSearch.ps1` — тип фиксируется по реальному
  `CONSTRUCTION_DECISION`, поскольку ранняя диагностика coverage ещё может
  содержать тип по умолчанию.
- Этот отчёт и ссылка из `docs/TESTING.md`.

Production `.c` и ранее существовавшие изменения пользователя сохранены.
Результаты предыдущей оптимизации поиска описаны отдельно в
[CONSTRUCTION_SEARCH_20260920.md](CONSTRUCTION_SEARCH_20260920.md).

## Все точки

Время в секундах от передачи всех точек US. Завершение означает CONSTRUCTION_COMPLETED с доступной службой; пустое значение — этап не завершён.

| Точка | HQ | Первая площадка, с | Первая готовая постройка, с | Тип | Всего готово | Статус |
|---|---:|---:|---:|---|---:|---|
| Airbase Saint-Philippe | 0 |  |  |  | 0 | SEARCH_NOT_COMPLETED |
| André's Beacon | 0 |  |  |  | 0 | SEARCH_NOT_COMPLETED |
| Black Lake | 0 |  |  |  | 0 | SEARCH_NOT_COMPLETED |
| Calvary Hill | 0 | 144.412 | 180.492 | SMALL_BARRACKS | 2 | BUILT |
| Camurac | 0 |  |  |  | 0 | SEARCH_NOT_COMPLETED |
| Chotain | 0 | 208.531 | 240.659 | SMALL_BARRACKS | 1 | BUILT |
| Coastal Base Chotain | 0 |  |  |  | 0 | SEARCH_NOT_COMPLETED |
| Coastal Base Lamentin | 0 |  |  |  | 0 | SEARCH_NOT_COMPLETED |
| Coastal Base Morton | 0 |  |  |  | 0 | SEARCH_NOT_COMPLETED |
| Durras | 0 | 29.1 | 54.192 | SMALL_BARRACKS | 1 | BUILT |
| Entre-Deux | 0 | 416.214 | 444.309 | SMALL_BARRACKS | 1 | BUILT |
| Figari | 0 | 93.247 | 117.297 | SMALL_BARRACKS | 2 | BUILT |
| Gravette | 0 | 113.298 | 169.442 | SMALL_BARRACKS | 1 | BUILT |
| Hornbeam Valley | 0 | 139.378 | 173.478 | SMALL_BARRACKS | 1 | BUILT |
| Kermovan | 0 | 261.748 | 291.776 | SMALL_BARRACKS | 2 | BUILT |
| Laruns | 0 | 100.263 | 127.342 | SMALL_BARRACKS | 2 | BUILT |
| Le Moule | 0 | 99.266 | 131.362 | SMALL_BARRACKS | 1 | BUILT |
| Levie | 0 | 221.599 | 251.676 | SMALL_BARRACKS | 1 | BUILT |
| Main Operating Base (StartingPos05) | 1 |  |  |  | 0 | SEARCH_NOT_COMPLETED |
| Main Operating Base (StartingPos01) | 1 |  |  |  | 0 | SEARCH_NOT_COMPLETED |
| Meaux | 0 | 300.829 | 325.86 | SMALL_BARRACKS | 1 | BUILT |
| Military Base Levie | 0 | 591.935 | 619.06 | LIGHT_DEPOT | 1 | BUILT |
| Military Depot | 0 | 63.199 | 93.243 | SMALL_BARRACKS | 1 | BUILT |
| Military Hospital | 0 |  |  |  | 0 | SEARCH_NOT_COMPLETED |
| Montignac | 0 |  |  |  | 0 | SEARCH_NOT_COMPLETED |
| Morton Valley | 0 | 164.446 | 191.51 | SMALL_BARRACKS | 1 | BUILT |
| Old Wood | 0 | 135.386 | 163.444 | SMALL_BARRACKS | 1 | BUILT |
| Pennants Pass | 0 |  |  |  | 0 | SEARCH_NOT_COMPLETED |
| Pinewood Lake | 0 | 163.451 | 191.511 | SMALL_BARRACKS | 3 | BUILT |
| Provins | 0 | 456.345 | 482.483 | SMALL_BARRACKS | 1 | BUILT |
| Quarry | 0 |  |  |  | 0 | SEARCH_NOT_COMPLETED |
| Régina | 0 | 154.43 | 178.477 | SMALL_BARRACKS | 1 | BUILT |
| Saint-Philippe | 0 |  |  |  | 0 | SEARCH_NOT_COMPLETED |
| Saint-Pierre | 0 |  |  |  | 0 | SEARCH_NOT_COMPLETED |
| Simon's Wood | 0 | 133.381 | 168.443 | SMALL_BARRACKS | 1 | BUILT |
| Tiller's Find | 0 | 262.752 | 291.776 | SMALL_BARRACKS | 1 | BUILT |
| Tyrone | 0 | 127.349 | 159.425 | SMALL_BARRACKS | 1 | BUILT |
| Vernon | 0 | 40.112 | 68.192 | SMALL_BARRACKS | 2 | BUILT |
| Villeneuve | 0 | 109.282 | 135.378 | SMALL_BARRACKS | 2 | BUILT |

## Проверки и ограничения

Команды выполнены из корня репозитория; `$test` и `$log` определены выше.

| Команда / gate | Verdict |
|---|---|
| `pwsh -NoProfile -File tools/Test-ConstructionStatic.ps1` | PASS / 0 до и после |
| `pwsh -NoProfile -File tools/Test-RuntimeLauncherStatic.ps1` | PASS / 0 до и после |
| `pwsh -NoProfile -File "$test/compile.ps1" -Source source -Name all-us-fixed -Graph Everon` | PASS / 0, `Script validation successful.` |
| `pwsh -NoProfile -File "$test/run.ps1"` | Native exit 0; `CONSTRUCTION_ALL_BASES_DONE elapsed_ms=900194`, `Game destroyed.` |
| Manifest ↔ native `CLI Params`, живой PID 26596 и настоящий `ROSTER_READY` | PASS; сохранено в `run-01/startup-evidence.json` |
| `pwsh -NoProfile -File tools/Test-ConstructionLog.ps1 -LogPath $log -ExpectedMode US -RequireCompletion` | FAIL / 1: только `CONSTRUCTION_ENGINE_ERROR`; placed=32, completed=32 |
| `pwsh -NoProfile -File tools/Test-BaseBuildersLog.ps1 -LogPath $log -MinimumCompleted 1 -RequireToolUse` | FAIL / 1: только `BUILDERS_RUNTIME_SCRIPT_ERROR` |
| `pwsh -NoProfile -File tools/Test-ConstructionSearchLog.ps1 -LogPath $log` | FAIL / 1: только `NO_ENGINE_SCRIPT_ERROR`; stopped/roster/no cooldown PASS |
| `pwsh -NoProfile -File tools/Measure-ConstructionAllBases.ps1 -LogPath $log -OutputDirectory "$test/metrics"` | PASS / 0; независимая сверка 207 decisions, 32 completions, 39 bases; несовпадений identity/type 0 |
| SHA-256 production и source snapshot | 171/171 production неизменны; 172/172 snapshot неизменны во время runtime |
| `git diff --check` | PASS / 0 |

Первая компиляция fixture (`all-us`) завершилась FAIL: имя локальной переменной
`owned` оказалось зарезервированным Enforce keyword. Оно заменено на
`ownedUSCount`, после чего повторный terminal Validate/Compile успешен.
Оба полных compile logs сохранены; первый FAIL не скрыт и не является baseline
failure production. Два релевантных static baseline были зелёными и остались
зелёными. Известные по предыдущей задаче два `AI_COMMANDER_UI_STATE` здесь
не перепроверялись и не исправлялись.

Полный server `console.log` содержит 9327 строк. Две script errors появляются
в 14:19:19, после `CONSTRUCTION_ALL_BASES_DONE` в 14:19:16, непосредственно
перед `Game destroyed.`: `SCR_BaseResupplySupportStationComponent needs a
entity catalog manager!`. Во время самого измерения script errors не было.
Всего сохранено 50 native `(E)` строк, включая world/resource/material,
Hierarchy, pathfinding tile diagnostics и resource leak. `ENGINE (F)`,
`Virtual Machine Exception`, `NULL pointer` и `[AICF]...[ERROR]` не обнаружены.
Эти сообщения не фильтруются ради PASS. `native-errors.txt` и
`native-error-groups.json` — индекс; исходные полные logs сохранены рядом.

NOT RUN: direct-connect client, JIP, ручная визуальная проверка, строительство
в бою, повторные seeds и длительный soak после 15 минут. Это один одновременный
прогон полного stock острова, не статистическая гарантия для всех запусков.
Сервер с PID 26596 остановлен; `analysis-validation.json` подтверждает его
отсутствие. Никаких GUI, screenshots или изменений установленных game files
не выполнялось.

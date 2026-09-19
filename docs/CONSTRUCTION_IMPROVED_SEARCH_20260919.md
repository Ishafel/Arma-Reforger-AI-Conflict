# Более плотный поиск площадок и маршрут строителя

Stock Arland, Reforger `1.8.0.13`, 2026-09-19. Основание —
[повтор двух пользовательских площадок](CONSTRUCTION_MANUAL_REPLAY_20260919.md).
Evidence: `.codex-runtime/construction-improved-20260919/`.

## Изменения

- Вместо 64 центров с четырьмя поворотами проверяются 256 различных центров.
  Детерминированная последовательность Halton покрывает площадь доступного
  диска; смещение prefab pivot и восемь направлений сохранены. Следующий order
  продолжает последовательность. Координаты карт в Core не добавляются.
- Metadata отдельно сохраняет bounds stock outline. До 12 рабочих точек
  выбираются вокруг него, ещё четыре — снаружи общего footprint. Они
  проверяются по collision volumes completion с запасом 2.5 м и выбранным
  выездам. Общий пустой AABB композиции не становится стеной для маршрута.
- `AICF_ConstructionPath` хранит ограниченный поиск по navmesh одного
  кандидата: сетка 2 м, шаги 2/4 м, weighted heuristic 1.4, до 512 узлов,
  до 64 переходов за вызов, существующая
  общая квота 96 queries/window и ограничение slice. Новых callbacks или
  gameplay entities нет. Исчерпание ограниченного поиска отдельно отмечается
  `WORKER_PATH_SEARCH_EXHAUSTED`.
- Все старты одного кандидата делят лимит 4096 path queries. При исчерпании
  `WORKER_PATH_QUERY_LIMIT` переводит поиск к следующему кандидату. Это
  устраняет наблюдавшийся в первом matrix-run повтор дорогого кандидата
  после общего deadline (USSR SMALL, 15 candidates, 10487 queries, 132316 мс
  вместе с metadata, следующий order продолжал бы тот же незавершённый сайт).
- Stock spawn API вызывается один раз на кандидата. До восьми дополнительных
  стартов в 6 м от исходного проверяются в устойчивом порядке. Проверенный
  старт передаётся `AICF_BaseBuilderService` для AI receipt с проверкой
  acceptance, identity, faction и неизменности transform. Player layouts
  сохраняют прежний stock spawn. Само создание character остаётся асинхронным
  stock roster с прежними readiness/generation gates.
- Bounds, physics, water, terrain slope, vehicle exits, свежие проверки
  commit/completion, оплата и lifecycle guards сохраняются.

## Проверки

Baseline до правок: `Test-ConstructionStatic.ps1`,
`Test-ConstructionContracts.ps1`, `Test-BaseBuildersStatic.ps1`,
`Test-Stage4Static.ps1` — PASS / 0; `Test-AICommanderModeStatic.ps1` —
FAIL / 1, прежний `AI_COMMANDER_UI_STATE`. Сохранены `before.patch`,
`before-*.txt`, `before-summary.json`.

Contracts: 16 log inputs, positive и девять negative static inputs. Новые
негативные входы запрещают убирать native navmesh edge, списание общей квоты
и общий лимит пути кандидата. Coverage fixture проверяет 256 уникальных
центров, восемь направлений, детерминизм, продолжение sequence, малый радиус,
близкие к provider точки и покрытие контрольной сетки в диске.

На окончательном production code все четыре Workbench graphs (Arland,
Everon, ArlandRHS, EveronRHS): **PASS / 0**, `Script validation successful`,
без SCRIPT E/F, ENGINE F и VM exception. Итоговые артефакты —
`wb-budget-release-*`. Isolated replay/matrix fixtures также скомпилированы,
exit 0. Все 129 production `.c` совпадают с обеими test-копиями по SHA-256
(`budget-source-hashes.json`). Resource-leak diagnostics Workbench сохранены
и не скрываются. Итоговая статика `budget-final-static-summary.json` сохраняет
четыре PASS и прежний `AI_COMMANDER_UI_STATE` FAIL.

Первый runtime `replay-*` завершился native exit 0, но поиск с лимитом
24 перехода за вызов не решил задачу: cases 0/1/3 остались pending до
диагностического timeout, case 2 исчерпал поиск. Это **не PASS исправления**.
Следующая итерация `replay-v2-*` подтвердила путь для горизонтального второго
места. Калибровка на navmesh показала `self=1 near=1 far=0`; bool-семантика
`RayTrace` использовалась верно. Сетка только 2 м в `replay-v3-*` также
оказалась недостаточной: завершённого пути для ручного второго transform
не было до timeout. Прогон v3 остановлен принудительно после обнаружения
ошибки fixture: успешный `LiveClear` сохранял прежний reason `QUERY_BUDGET`,
и диагностический phase не продвигался. Исправлено очищение reason в fixture.
Полный log и `replay-v3-stop.json` сохранены; этот прогон не PASS.

Промежуточный `replay-final-*` использует смешанные шаги и передачу выбранного
старта строителю. Для двух transforms первой площадки с заведомым terrain
отказом повтор пути явно пропущен (`PATH_SKIPPED`, `aicfConstructionReplayValidOnly=1`);
это не путь PASS. Для второй площадки с ручным наклоном все geometry guards
и путь прошли: `SITE_VALIDATED`, endpoint `<1202.95,0.993622,3278.93>`,
60 раскрытых узлов; прежний алгоритм возвращал `WORKER_ENDPOINT_UNREACHABLE`.
Оба варианта второй площадки получили путь PASS; горизонтальный дополнительно
отклонён итоговой physics проверкой на `GraniteSurfaceStone_01.et`.
Replay закрылся штатно, native exit 0, 4 cases. Полный log сохранил 17 строк
E/F, включая две shutdown SCRIPT ошибки stock resupply; VM exceptions — 0.
Первый matrix-run подтвердил оплаченные малые казармы US и реальный
`BUILDER_COMPLETED` с tool/item active, но был остановлен для добавления
лимита queries кандидата. Его логи не являются финальным runtime PASS.

Окончательный `replay-budget-*`, уже с лимитом 4096 path queries, завершился
штатно: native exit 0, 4 cases, SCRIPT E/F и VM exceptions — 0. Сохранены
15 других E/F строк (WORLD 8, ENTITY 4, RESOURCES 3). Вторая ручная площадка
больших казарм СССР снова прошла bounds, access, physics, terrain, path и
итоговый physics: `SITE_VALIDATED`, endpoint `<1202.95,0.993622,3280.09>`,
60 nodes / 16 expanded, 3434 queries всего с geometry, 40133 мс проверки пути.
Прежний тест пути для неё возвращал `WORKER_ENDPOINT_UNREACHABLE`.
Горизонтальный вариант в этом прогоне достиг `WORKER_PATH_QUERY_LIMIT` и
сохранил отдельный physics отказ на камне; он не считается PASS. Первая
площадка в обоих transforms сохранила terrain отказ, путь явно пропущен.
Fresh-world replay не восстанавливает побитно прежнюю игровую сессию:
stock spawn и начальные объекты могут отличаться.

Ближайший новый центр к первой ручной площадке — 7.54 м вместо 34.77 м;
ко второй — 8.61 м вместо 8.39 м. Улучшение распределения не означает, что
каждая конкретная точка стала ближе к выборке. Общая проверка подтверждает
256 различных центров вместо повторения 64 центров.

Финальная matrix `matrix-budget-*` завершилась штатно: **native exit 0**,
`Game destroyed.`, 907278 мс, 904 ticks, max window 96 queries. Native helpers
— **30/30**. Максимальный измеренный полный tick — 290 мс; это не доказательство
соблюдения 8 мс для целого planner tick (slice относится к части поиска).
Полный остановленный log сохранён вместе с `error.log` и `script.log`.

| Тип | US | USSR |
| --- | --- | --- |
| SMALL_BARRACKS | оплачено и достроено, 2 кандидата | оплачено и достроено, 56 кандидатов |
| ARMORY | stock `covered=1`, нового order нет | stock `covered=1`, нового order нет |
| LIGHT_DEPOT | оплачено и достроено, 144 кандидата | оплачено и достроено, 65 кандидатов |
| LARGE_BARRACKS | не размещено, 21 кандидат на проверенной базе | не размещено, 256 + 226 кандидатов на двух базах |
| HEAVY_DEPOT | 195 кандидатов, поиск прерван штатным завершением теста | 49 кандидатов, поиск прерван штатным завершением теста |

Все четыре placements имеют один debit (казармы 250, depot 150), совпадающий
`BUILDER_PROGRESS`/`BUILDER_COMPLETED` с `tool_active=1 item_using=1` и
`CONSTRUCTION_COMPLETED service_online=1`. US SMALL/LIGHT завершились уже
после переключения phase; итог считается по событиям completion, а не по
промежуточным `MATRIX_PHASE end`.

Для больших казарм СССР первая база отклонила 171/58/17/10 кандидатов по
physics/road/water/occupied соответственно; вторая — 143/54/16/8/5 по
physics/road/water/occupied/slope. US LARGE достиг общего deadline, включая
один `WORKER_PATH_QUERY_LIMIT`. Heavy ни у одной стороны не дошёл до
подтверждения площадки: из прерванного поиска нельзя заключать, что свободных
мест вообще нет. Самостоятельная достройка LARGE/HEAVY окончательной версией
в этой matrix **не подтверждена**; точная ручная площадка LARGE проверена
отдельным replay выше.

Полный log содержит 17 E/F: 15 WORLD/ENTITY/RESOURCES diagnostics и две
прежние shutdown ошибки `SCR_BaseResupplySupportStationComponent` («needs a
entity catalog manager!»). VM/ENGINE F — 0. Аудиты не ослаблялись:

- `Test-ConstructionLog -RequireCompletion`: **FAIL / 1**, только
  `CONSTRUCTION_ENGINE_ERROR`; placed=4, completed=4.
- `Test-ConstructionLog -RequireCompletion -RequireAllTypes -RequireAllFactions`:
  **FAIL / 1**, восемь issues — engine error, шесть недостающих faction/type
  pairs ARMORY/LARGE/HEAVY и общий `CONSTRUCTION_MISSING_TYPES`.
- `Test-BaseBuildersLog -MinimumCompleted 4 -RequireToolUse`: **FAIL / 1**,
  только `BUILDERS_RUNTIME_SCRIPT_ERROR`.
- `git diff --check`: **PASS / 0**.

Matrix fixture назначает обычные неконтрольные базы ближайшей стороне,
пополняет supplies и последовательно задаёт потребность по пяти типам,
по 180000 мс на тип. На сторону допускается один незавершённый заказ.
Coverage, authority, выбор площадки, расход supplies и реальный builder
сохраняют production поведение. Арсеналы уже покрыты stock службами;
`covered=1` не обходится ради искусственного placement. В окончательной
matrix СССР получил южный HQ, США — северный. Это функциональная проверка,
а не сравнение скорости строительства до/после в одинаковом матче.

Сигнатуры взяты из pinned Script Diff `1.8.0.13`. Официальный
[AIPathfindingComponent reference](https://community.bistudio.com/wikidata/external-data/arma-reforger/ArmaReforgerScriptAPIPublic/interfaceAIPathfindingComponent.html)
также не поясняет bool-семантику `RayTrace`; поведение проверяется runtime.

## Команды

```powershell
pwsh -NoProfile -File tools/Test-ConstructionStatic.ps1
pwsh -NoProfile -File tools/Test-ConstructionContracts.ps1 -EvidenceRoot .codex-runtime/construction-improved-20260919/contracts-final
pwsh -NoProfile -File tools/Test-BaseBuildersStatic.ps1
pwsh -NoProfile -File tools/Test-Stage4Static.ps1
pwsh -NoProfile -File tools/Test-AICommanderModeStatic.ps1
```

Workbench — только терминальная команда из `docs/DEVELOPMENT.md`, с
`-noThrow -wbsilent -wbModule=ScriptEditor -run -validate | Out-Null`.
Dedicated runtime — только `tools/Start-AICFRuntime.ps1 -Role Server -Variant Stock`.
Полные argv сохраняются в `*-launch.txt` и `*-manifest.json`.

Ниже сокращённые схемы runtime запуска; точные аргументы, включая координаты
и transforms replay, сохранены в соответствующем manifest.

```powershell
# Isolated replay source; production .c сверены по SHA-256.
tools/Start-AICFRuntime.ps1 -Role Server -Variant Stock -RepositoryRoot <replay-final-source> -AICommanderMode BOTH -AdditionalArguments '-aicfConstructionReplay','1','-aicfConstructionReplayValidOnly','1'
# Isolated matrix source.
tools/Start-AICFRuntime.ps1 -Role Server -Variant Stock -RepositoryRoot <matrix-final-source> -AICommanderMode BOTH -AdditionalArguments '-aicfConstructionProbe','1','-aicfConstructionProbeMatrix','1','-aicfConstructionProbeMatrixPhaseMs','180000','-aicfConstructionProbeMs','1000000','-aicfConstructionProbeTrace','1'

pwsh -NoProfile -File tools/Test-ConstructionLog.ps1 -LogPath .codex-runtime/construction-improved-20260919/matrix-budget-console.log -RequireCompletion
pwsh -NoProfile -File tools/Test-ConstructionLog.ps1 -LogPath .codex-runtime/construction-improved-20260919/matrix-budget-console.log -RequireCompletion -RequireAllTypes -RequireAllFactions
pwsh -NoProfile -File tools/Test-BaseBuildersLog.ps1 -LogPath .codex-runtime/construction-improved-20260919/matrix-budget-console.log -MinimumCompleted 4 -RequireToolUse
```

## Обычная сессия после проверки

Из production repository без fixtures запущены отдельные server/client
через `Start-AICFRuntime.ps1`, Stock Arland, `AICommanderMode BOTH`:

- Server PID `13380`, profile `C:\Users\retar\AppData\Local\AICF\Server-20260919-134730-438`.
- Client PID `16668`, profile `C:\Users\retar\AppData\Local\AICF\Client-20260919-134810-195`.

Перед direct-connect launcher подтвердил exact server CLI, живой процесс,
порт 2001 и `ROSTER_READY`. Сохранены `play-server-manifest.json`,
`play-client-manifest.json`, оба `*-launch.txt` и `active-manual-session.json`.
Server log содержит `Player connected` и `Players connected: 1 / 1`, client
log — `WelcomeScreenMenu` и `STATE_REPLICATED`. На snapshot 13:50:06 SCRIPT/VM
errors — 0 у обоих. Сессия оставлена запущенной; это подтверждение подключения,
а не verdict по полным остановленным logs или проверка construction JIP.

```powershell
tools/Start-AICFRuntime.ps1 -Role Server -Variant Stock -RepositoryRoot $PWD.Path -AICommanderMode BOTH
tools/Start-AICFRuntime.ps1 -Role Client -Variant Stock -RepositoryRoot $PWD.Path -ServerProfileRoot 'C:\Users\retar\AppData\Local\AICF\Server-20260919-134730-438' -AICommanderMode BOTH
```

## Файлы этого шага

Production: `AICF_ConstructionPath.c`, `AICF_ConstructionSiteSearch.c`,
`AICF_ConstructionOrder.c`, `AICF_ConstructionMetadata.c`,
`AICF_BaseBuilderService.c` в Core/Construction.
Проверки: `Test-ConstructionStatic.ps1`, `Test-ConstructionContracts.ps1`,
`fixtures/AICF_ConstructionRuntimeProbe.c`, `fixtures/AICF_ConstructionReplayProbe.c`.
Документация: этот отчёт, `ARCHITECTURE.md`, актуальные ссылки в validation/testing.
Предыдущие изменения dirty tree и пользовательские prompt-документы сохраняются.

Визуальные критерии, Everon/RHS runtime и новый client/JIP construction test —
NOT RUN. Статус `ACCEPTED` не присваивается.

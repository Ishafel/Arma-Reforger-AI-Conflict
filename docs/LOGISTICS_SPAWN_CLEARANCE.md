# Проверка места появления AI-логистики

Admission по непаддированному кузову сохранён. Последующий выезд через road
waypoint, ограниченное восстановление той же машины и cooldown доказанно
неудачного exact slot описаны в [LOGISTICS_RECOVERY.md](LOGISTICS_RECOVERY.md).
Свободный прямой коридор по-прежнему не является условием spawn.

Дата: 2026-09-06. Исходный commit: `507b984c97cd47f540edd8e43963d5555a727bd4`.

## Причина и граница изменения

В пользовательском `logs_2026-09-06_19-05-27/script.log`, строки 2683–2688
и 6684–6688, `US slot=1000000 depot=0x400000000000581A` проходит проверку
кузова, затем получает `EXIT_OBSTACLE` на заборе, мастерской и навесах.
После исчерпания двух штатных кандидатов появляется `NO_SAFE_CATALOG_SLOT`.
`slot=1000003` в том же запуске успешно создаёт машину. Это исходное evidence,
а не runtime результат новой версии.

Удалены только logistics admission gates:

- два дополнительных OBB длиной 16 м, смещённые на 12 м вперёд/назад;
- широкий stock spawn-slot box с `Projectile` mask и exclusions;
- расширение prefab bounds на 0,1 м и сдвиг этих bounds вверх;
- обязательный reachable road endpoint до spawn. Он остаётся необязательной
  подсказкой `m_vParking`; при отсутствии используется exact stock slot.

Старая проверка принимала свободный коридор хотя бы с одной стороны.
Теперь свободный коридор ни с одной стороны не является условием spawn.
Маршруты, объезды и открытие ворот остаются штатному AI.

`AICF_LogisticsVehicleFootprint.IsClear` строит ориентированный OBB по mesh
bounds конкретного prefab, без дополнительного отступа. `TracePosition` с
`ENTS` и `EPhysicsLayerPresets.Vehicle` проверяет его против collision geometry
мира; отрицательная penetration запрещает spawn. `QueryEntitiesBy*`, broadphase
overlap, bounds соседнего здания и само наличие объекта поблизости не являются
окончательным veto. Props depot не исключаются: пересекающий кузов навес также
блокирует spawn. OBB остаётся консервативным приближением формы машины,
а не точным тестом каждой полости её collision mesh.

Сохранены water/terrain checks, детерминированный порядок разрешённых child/near
slots конкретного production, exact depot/provider/faction identity, generation
и readiness gates, общие reservations, faction fleet cap и authority checks.
`LogisticsSpawnClear` вызывается при reserve и снова перед `SpawnSelectedPrefab`.
Создание vehicle остаётся в `AICF_VehicleSpawner`; `AICF_LogisticsAcquisitionFlow`
и соседние combat/construction/cleanup paths не изменены. Нового CLI нет.

## Static и compile evidence

Локальный корень evidence:
`C:\Users\retar\IdeaProjects\Arma-Reforger-AI-Conflict\.codex-runtime\logistics-spawn-clearance-20260906`.

Каждая команда запускалась до и после production-правки:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Test-Stage3Static.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Test-Stage35Static.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Test-Stage3StaticContracts.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Test-Stage35RecoveryPolicy.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Test-Stage4Static.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Test-LogisticsStatic.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Test-LogisticsContracts.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Test-ConstructionStatic.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Test-ConstructionContracts.ps1
```

Baseline и итог: **9/9 PASS, все exit 0**, baseline failures нет.
`LogisticsContracts`: 56 → 70 positive/negative cases. Добавленные mutations
проверяют запрет обхода collision, surface, reserve/commit recheck, потерю shared
reservation, неверные mask/rotation, расширенные bounds и возврат exit veto.
Прежние отрицательные проверки сохранены. Outputs: `baseline-results.json`,
`final-results.json`, `baseline-*.txt`, `final-*.txt`.

Terminal Workbench `ArmaReforgerWorkbenchSteamDiag.exe -noThrow -wbsilent ...
-wbModule=ScriptEditor -run -validate` выполнен по `DEVELOPMENT.md` для Arland,
Everon и RHS: **PASS / exit 0**, `Game successfully created`,
`Script validation successful`, без `SCRIPT (E/F)`, `ENGINE (F)` и VM exceptions.
Версия Tools `1.8.0.13`. Exact arguments: `workbench-*-arguments.json`;
полные logs: `final-workbench-arland/`, `final-workbench-everon/`,
`final-workbench-rhs/`. Stock obsolete/up-cast warnings и shutdown resource leaks
учитываются отдельно: 24 shutdown resource leaks на graph, а RHS также пишет
`Wrong GUID/name ... Language/rhs_localization.st`; те же сообщения есть в
прежних `logistics-20260906/final-workbench-*/console.log`.
Первая sandbox-попытка остановилась на `SteamAPI_Init`
до validation; финальные gates выполнены в пользовательском Steam context.

## Runtime probe

`tools/fixtures/AICF_LogisticsSpawnClearanceProbe.c` используется вместе с
`AICF_LogisticsRuntimeProbe.c` только в отдельной копии source addon graph.
В рабочие production directories fixtures не копировались. Probe синхронно
создаёт временные физические препятствия у реально выбранного stock slot,
проверяет production `LogisticsSpawnClear` и удаляет только свои test entities.
Проверяется настоящий vehicle, созданный единственным production spawner.

Cases: свободный кузов; оба перекрытых выезда; забор рядом; навес над кузовом;
твёрдое препятствие внутри; занятый catalog slot; освобождение после удаления
test obstacle; второй worker на зарезервированном slot; production spawn;
пересечение с только что созданной настоящей машиной.

Запуск из терминала через `tools/Start-AICFRuntime.ps1`, `-Role Server
-Variant Everon -AICommanderMode BOTH`, свежий profile и `-RepositoryRoot`
изолированной source-копии. Test flags: `aicfLogisticsProbe=1`,
`aicfLogisticsProbePrepare=1`, `aicfLogisticsProbePeace=1`,
`aicfLogisticsProbeDurationMs=300000`, `aicfLogisticsWorkersPerDepot=2`,
`aicfRequirePlayerForResult=0`. Exact launcher manifest и полный terminal output
сохранены в `runtime-launch.txt`. Оценка выполняется после штатного завершения
probe по полным server logs, а не по одному filtered index.

`ServerPort=2017` был передан launcher, но этот параметр применяется только к
client readiness check: фактический server log подтверждает `0.0.0.0:2001`.
Текущие пользовательские процессы не останавливались; GUI не использовался.

Первый probe (`20:26:26–20:32:52`, launcher exit 0) подтвердил 10/10 cases,
но `Test-LogisticsLog` дал FAIL: новые test events ещё не имели префикса
`LOGISTICS_PROBE_*` и попадали под production identity contract. Кроме того,
пример `material/default` дал пять resource errors при создании test boxes.
Исправлены только fixture prefix и материал на `Common/Materials/Game/stone.gamemat`
из pinned `Physics.c`; production-анализатор и collision checks не ослаблялись.
Первый полный лог и FAIL сохранены в `runtime-logs/`, `runtime-log-audit.txt`.

Повторный запуск `20:33:55–20:40:21` завершился через fixture `RequestClose`,
launcher **exit 0**. `ROSTER_READY` получен до logistics acquisition.
`LOGISTICS_PROBE_SPAWN_CLEARANCE_CONTRACT passed=10 total=10` подтверждён
в полном остановленном log; каждый case наблюдался ровно один раз с `passed=1`.
Реальный второй worker `slot=1000001` того же depot также получил
`SHARED_SITE_RESERVED`; первая машина создана production spawner и получила
`LOGISTICS_DRIVER_READY`. Новых `EXIT_OBSTACLE`/road exit veto нет.

**Общий runtime gate: FAIL**, несмотря на подтверждённые 10/10 spatial cases.
Команды `test-spawn-runtime.ps1 -LogPath <console.log>` из evidence-каталога и
`tools/Test-LogisticsLog.ps1 -LogPath <console.log>` дали **exit 1** из-за двух
teardown `SCRIPT (E): 'SCR_BaseResupplySupportStationComponent' needs a entity
catalog manager!`. Они появились после `LOGISTICS_PROBE_FINISHED` и совпадают
с прежним Everon-1 evidence из `LOGISTICS_DRIVER_INTERACTION.md`.
VM exceptions и `ENGINE (F)` — 0. Stock resource/world/Hierarchy/pathfinding
messages и shutdown leak также сохранены; ошибка материала test boxes устранена.
Проверки не менялись ради PASS этого запуска.

Исходные полные logs второго запуска:
`C:\Users\retar\AppData\Local\AICF\LogisticsSpawnClearance-20260906-203355\logs\logs_2026-09-06_20-33-55`.
Копия всех трёх logs: `runtime-2-logs/logs_2026-09-06_20-33-55/` в evidence root.
Manifest/CLI/exit: `runtime-2-manifest.json`, `runtime-2-launch.txt`.
Verdicts: `runtime-2-results.json`, `runtime-2-case-observations.json`,
`runtime-2-log-audit.txt`, `runtime-2-spawn-audit.txt`.
Все 105 production `.c` из runtime-копии совпадают с рабочими исходниками
(`runtime-source-comparison.json`); fixtures остались только в test/source-копии.

Ручная проверка исходного depot, визуальной геометрии/выезда и открытия ворот,
client/JIP, RHS runtime, остальные vehicle prefab, отдельные live water/terrain
negative cases, доставка и длительный soak — **NOT RUN**. Collision fixture
использует физические boxes для стены/забора/навеса и настоящий stock
`UAZ452_cargo.et` для занятого vehicle volume.

## Изменённые файлы

В `AIConflictCore/Scripts/Game/AIConflict/`:

- `Vehicles/AICF_LogisticsVehicleFootprint.c` — единый physics test по prefab OBB;
- `Economy/AICF_LogisticsDepotRegistry.c` — read-only slot check по prefab;
- `Vehicles/AICF_VehicleSpawner.c` — удаление corridor veto, необязательная parking
  hint, сохранённые reserve/commit geometry gates.

Проверки: `tools/Test-LogisticsStatic.ps1`, `tools/Test-LogisticsContracts.ps1`,
новая `tools/fixtures/AICF_LogisticsSpawnClearanceProbe.c`.
Документация: этот файл, `docs/ARCHITECTURE.md`, `docs/LOGISTICS_VALIDATION.md`,
`docs/TESTING.md`. Пользовательский `docs/AI_LOGISTICS_IMPLEMENTATION_PROMPT.md`
не изменялся.

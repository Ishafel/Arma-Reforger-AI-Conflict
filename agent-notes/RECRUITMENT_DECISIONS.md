# Диагностика отказа recruitment short recovery

`tests/fixtures/AICF_RecruitmentDecisionProbe.c` — изолированный test-only
observer. Его не копируют в production/player build. В отдельный frozen stage
fixture добавляется как `AIConflictCore/Scripts/Game/AIConflict/AICF_ZRecruitmentDecisionProbe.c`;
native запуск требует terminal Workbench и отдельного согласования ресурсов.
Опция `-aicfRecruitmentDecisionProbe 1` включает authoritative diagnostics.
При отсутствии опции исходные методы продолжают выполняться один раз.
Публичный `Invoke-AICFVehicleSoak.ps1 -RecruitmentDecisionObserver` копирует
fixture только в новый isolated stage; с `-StageRoot` требует уже подготовленную
и hash-verified копию. `-QuotaCensusDecisionObserver` независимо подключает
quota census observer, если его fixture существует. Опции не меняют исходный
1800s duration, Proof4 auditor или поведение Soak; shared compiled stage
проверяется по исходному manifest, а не дополняется после компиляции.

Fixture наблюдает instance методы, не переопределяя static `Capture`:

- native-declared `SCR_AIProcessFailedMovementResult.EOnTaskSimulate`;
- `AICF_RecruitmentMoveFailure.IsCurrent`, `TryRecover`, `FindDestination`, `SafeDestination`.

Каждая обёртка вызывает ровно один исходный `super`, сохраняет аргументы,
возвращаемый `ENodeResult`/bool и out-position. PRE snapshot собирается до delegate,
печатный event — только после его результата. POST snapshot читается заново;
смена immutable owner/identity или неполное чтение помечаются как unknown,
никогда не разрешают recovery. Diagnostic context живёт только в stack scope.
В statics остаются лишь счётчик событий и overflow marker.

`[AICF][RECRUITMENT_DECISION]` содержит исходные handler/result/related/location,
current/executed activity и их состояния, related/current/queued waypoint,
group/actor entity IDs и Rpl, world/slot/generation/token/assignment/intent/graph/repair,
receipt-present, captured age и результат исходной проверки. В геометрических
events output допустим только при `result=1`. Auxiliary actor flags — отдельный
snapshot, не atomic authorization proof. Максимум256events, затем OVERFLOW
с `completeness=UNPROVEN`.
Для node event записывается полный исходный `node_result`; bool `result` означает
только равенство `ENodeResult.FAIL`. `internal_handler_result=UNKNOWN` явно
запрещает принимать общий node outcome за результат внутреннего handler.

Observer не добавляет физические/navmesh queries, `TryRecover`, таймеры,
action lifecycle вызовы, teleport, spawn/delete, heal или изменения приказов.
Это подготовка к отдельному native разбору. Отсутствие SHORT_SUBMITTED в старом
логе не устанавливает конкретный отказ Capture/current/placement.

Базовая проверка:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tests/contracts/Test-RecruitmentDecisionProbeContracts.ps1
```

Контракт исполняет86cases настоящих wrapper bodies,22case чистого immutable
owner comparison,8source и8executed unsafe controls. Все7 pinned `ENodeResult`
сохраняются; отключённая опция и каждый отсутствующий port проверяются отдельно.
Константы `ENodeResult.SKIP` в pinned API нет. Native boundaries в replay явно
simulated; это не Workbench или native proof. Первый Stock WB stage v3 завершился
**FAIL** из-за unsupported override метода, добавленного другим modded layer.
Fixture исправлен на native-declared entrypoint; fresh combined Stock WB v4
**PASS** (exit0/validation/fullfatal0, raw384+5fixtures/source SHA0,
manifest48B8B8003628887A8513AC5FFD39743AE2F476292E9CEBC89EF5D0F21CE718F0).
Полные три Workbench logs и provenance — ignored
`.codex-runtime/issue78-finalization-20261029/ordinary-idle-diagnostic-source-v4/compile-verdict.json`.
Native для observers **NOT RUN**. Старый v3 FAIL сохранён; frozen ordinary383
stage и полные US/USSR1800 runs не менялись этой подготовкой.

Runner отдельно проверяется командой
`powershell.exe -NoProfile -ExecutionPolicy Bypass -File tests/contracts/Test-VehicleSoakObserverRunnerContracts.ps1`.
Его10cases запускают настоящий runner в отдельных WinPS-процессах с явно
simulated git/compiler/native/auditor boundaries: off/каждый observer/оба,
missing fixture каждого вида, SHA drift, неподтверждённое добавление после
компиляции, реальные own-stage copies и missing source. При этом1800s/Proof4/BOTH
сохраняются. Это CLI gate, а не запуск игры и не успешный runtime audit.

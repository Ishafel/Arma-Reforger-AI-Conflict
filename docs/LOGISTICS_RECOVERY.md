# Восстановление физического движения AI-логистики

## Установленная причина

В пользовательском `logs_2026-09-06_21-24-56/script.log`, строки 2857,
4052 и 4826, `USSR / slot=1000000` теряет `Ready()` при `getting_out=1`,
сохраняя vehicle/group/driver identity, живого водителя и отсутствие чужого
occupant. Controller немедленно отменял job и передавал lease в cleanup.
`AICF_LogisticsDriverInteraction.TryBegin()` допускал только доказанную
выбранную цепочку OpenGate с exact return. Для этих эпизодов STARTED нет;
исходный log не позволяет определить конкретный несовпавший native action.
Называть их открытием ворот нельзя. Забор/камень — наблюдение пользователя;
реальная физическая причина в прежних script logs не инструментирована.

Отдельный дефект в `TickLogistics`: повторная выдача того же endpoint вызывала
`MarkProgress()` без движения. После retirement не сохранялась история
неудачного выезда: следующий generation снова выбирал первый catalog slot.

Dedicated-прогон выявил ещё один существовавший дефект: `IsTerminal()`
включает успешный `COMPLETE_TRIP`. Controller проверял его раньше ветки
возврата и отправлял даже успешно вернувшегося водителя в retirement.
Успешный outcome теперь отделён от terminal failure; mutation contract
проверяет, что этот порядок нельзя вернуть незаметно.

## Поведение

`AICF_LogisticsRouteRecovery` хранит контекст одного leg. При первом выезде
сначала используется проверенный при spawn локальный участок 12–18 м
([контракт](LOGISTICS_SPAWN_EGRESS.md)); достижение на 5 м подтверждается
положением машины. В остальных случаях он пробует промежуточную точку через закреплённый
`RoadNetworkManager.GetReachableWaypointInRoad()`: 45 м в направлении цели,
радиус поиска 30 м, результат в пределах 12–90 м от машины. Неуспешный query
не изменяет конечный endpoint и не запрещает spawn. Это подсказка road graph;
фактическую проходимость вокруг world geometry доказывает только движение.
Достижение промежуточной точки на 8 м продолжает прежний leg/job к конечной цели.

После 20 с без физического перемещения выдаётся новый waypoint к текущей
точке. Через следующие 20 с без результата выбирается отличающаяся reachable
точка справа, затем слева, в фиксированном порядке. Всего две попытки на leg;
общий бюджет восстановления — 90 с. Waypoint attach/detach выполняет только
handoff, controller интерпретирует outcome. Ни waypoint, ни retry не вызывают
`MarkProgress()`. Успех восстановления требует одновременно перемещения
минимум 6 м от начала попытки и уменьшения расстояния к её endpoint минимум
на 3 м. Физический leg deadline остаётся 600 с. После исчерпания native
попыток разрешён перенос той же машины; политика изменена 2026-09-08 по
запросу владельца и описана в [LOGISTICS_FALLBACK.md](LOGISTICS_FALLBACK.md).
Изменение препятствий и собственное управление рулём не используются.

Подтверждённая OpenGate цепочка сохраняет прежний read-only observer и
exact-seat proof. Уже завершённый возврат принимается и после завершения
related group activity. Нераспознанный выход получает отдельный
`AICF_LogisticsDriverRecovery`: только тот же authoritative driver/group/lease,
job token, generation, vehicle/Rpl identity и exact seat, без чужого occupant
или reservation. Машина должна двигаться не быстрее 1 м/с, водитель —
оставаться в пределах 20 м и не переходить в другую машину/seat.

Неизвестному выходу даётся до 60 с; общий бюджет всех ожиданий на leg — 120 с.
Живые native interaction/get-in/get-out actions имеют приоритет. Когда их
нет, handoff сразу снимает свой route waypoint, чтобы вышедший водитель не
продолжал автомобильный маршрут пешком. После завершения transition,
минимум 5 с наблюдения и 2 с спокойного состояния разрешён единственный
tracked animated get-in. Teardown и выдача разделены scheduler ticks; перед выдачей снова
проверяются отсутствие конкурирующих actions и свободный exact seat.
`AICF_LogisticsReturnAction` отключает stock failure callback, способный
телепортировать персонажа. Завершение action до окончания посадочной анимации
не вызывает повторного get-in: observer ждёт физический exact-seat proof
в пределах того же deadline. В переходе допустим временно пустой
`GetCompartment()`, но другой seat или vehicle отклоняется.
При возврате восстанавливается waypoint прежнего
leg, без нового job/lease/generation и без обнуления возраста progress.

Ledger продлевает только ещё живые reservations того же job/generation.
Ограниченный recovery разрешает renewal после обычного progress timeout;
истёкший TTL не возрождается. Наблюдаемое ожидание водителя исключается из
progress, leg и recovery clocks. Native ожидание ограничено общими 120 с;
новый fallback имеет отдельные конечные бюджеты из LOGISTICS_FALLBACK.md.
Transfer требует заново доказанных identity, радиуса, скорости, stationary
hold, безопасности и ресурсных прав. Во время driver wait transfer закрыт.
Receipts, cargo custody и компенсация supplies не изменены.

`AICF_LogisticsExitHistory` разделяется только workers одного exact depot и
фракции. Две безуспешные route attempts возле spawn дают cooldown 300 с
конкретному stock slot. Два отдельных исчерпанных неизвестных выхода без
движения машины также дают cooldown; смерть и чужой occupant его не создают.
История проверяет component/entity identity, исходные position/forward,
generation повторного отказа и срок. После удаления/перемещения slot или
истечения срока candidate снова проходит обычные catalog/body/surface gates.
При всех заблокированных slots service пишет
`ALL_EXACT_DEPOT_EXITS_COOLING` и ждёт без создания lease/машины.
Другие depot и фракция целиком не блокируются. History очищается при Stop.

## Диагностика и проверки

Существующие events и поля сохранены. Добавлены `LOGISTICS_MOTION_SNAPSHOT`,
`LOGISTICS_DRIVER_ACTION`, `LOGISTICS_ROUTE_SAMPLE`, `LOGISTICS_ROUTE_ASSIGNED`,
`LOGISTICS_RECOVERY_ATTEMPT/SUCCEEDED`, `LOGISTICS_DRIVER_RETURN_ISSUED`,
`LOGISTICS_EXIT_FAILURE/COOLDOWN/SKIPPED`. Snapshot содержит current behavior,
очередь actions, target identity, vehicle/endpoint positions и clocks; sample
содержит перемещение между замерами, speed и текущий промежуточный endpoint.
Неизвестная причина явно обозначена `EXIT_CAUSE_UNRECOGNIZED`.

`Test-LogisticsStatic.ps1` проверяет новые ownership/clock/identity/TTL guards.
`Test-LogisticsContracts.ps1` вызывает тот же аудитор на повреждённых исходниках
и log fixtures. `Test-LogisticsLog.ps1 -RequireRecovery` требует подтверждённый
recovery и последующую delivery того же job/vehicle; одна выдача waypoint не
закрывает gate. `-RequireDriverInteraction` отдельно требует exact возврат и
delivery. Синтетические проверки не являются физическим runtime evidence.

Test-only `tools/fixtures/AICF_LogisticsRecoveryProbe.c` используется вместе с
`AICF_LogisticsRuntimeProbe.c` только в dedicated process. Параметр
`aicfLogisticsRecoveryProbe=obstacle|exit|both` создаёт конечные физические
стенки впереди/сзади stock slot с открытыми боками и/или вызывает animated exit
при первом leg. Препятствия существуют только в fixture; production их не
удаляет. Fixture также проверяет реальные Matches/CanRenew/deadlines с
устаревшими snapshot identity/token, Stop и исчерпанием budget; эти contracts
отдельны от live смерти/чужого occupant. В Core обе копии удаляются перед
финальными static и Workbench gates.
В recovery-режиме test preparation раз в секунду поддерживает supplies
источника, чтобы стройка базы не уничтожала подготовленный ресурсный контекст
до начала рейса. Это изменение только fixture, без подмены production
reservation, planner, transfer или физического движения.

## Evidence текущей задачи

Корень: `C:/Users/retar/IdeaProjects/Arma-Reforger-AI-Conflict/.codex-runtime/logistics-recovery-20260906/`.
Коммит не создавался. Посторонний `docs/AI_LOGISTICS_IMPLEMENTATION_PROMPT.md`
сохранён. Пользовательские процессы не изменялись; использованы только
терминальный Workbench и canonical dedicated launcher. Client/JIP/visual —
**NOT RUN**, клиент не запускался.

Baseline: восемь аудиторов `LogisticsStatic`, `LogisticsContracts`,
`Stage3Static`, `Stage35Static`, `Stage35RecoveryPolicy`, `Stage4Static`,
`ConstructionStatic`, `ConstructionContracts` — PASS/0; сохранённых static
baseline failures нет. `baseline-results.json` и `baseline-*.txt` содержат
результаты до правки. Финальные те же восемь — PASS/0; LogisticsContracts —
102 случая против baseline 70. Команды: `powershell.exe -NoProfile -ExecutionPolicy Bypass -File
tools/Test-<Name>.ps1`; результаты `release-static-results.json`, `release-*.txt`.
Промежуточные Stage3/3.5 FAIL с установленной fixture вызваны дублированием
modded controller/spawner; после удаления fixture те же guards проходят.

Финальные Arland/Everon/RHS Workbench — PASS/0, `Game successfully created`,
`Script validation successful`, без SCRIPT E/F и VM errors. Команда:
`& .codex-runtime/logistics-recovery-20260906/Run-Workbench.ps1 -Phase release`; раскрытые
native argv, exit codes, времена и пути полных logs сохранены в
`release-workbench-results.json`. Tools/runtime используют 1.8.0.13.
Первый sandbox-запуск `compile-1` завершился -1 из-за SteamAPI_Init failed;
терминальный запуск в пользовательском окружении выполнил validation.

Первый dedicated Everon завершился штатно, launcher exit 0. В полном
остановленном `runtime-everon-1-logs/console.log` подтверждена delivery 446
supplies для USSR `slot=1000000`, generation 1, job `L1_S1000000_G1`, та же
пара vehicle/driver; supplies другой машины (US, 260) сохранены через
`SURVIVING_WORLD_POOL`. Это промежуточная версия и обычная доставка, не
доказательство recovery. Общий audit — FAIL/1: cleanup retained
`DELETE_AUTHORITY_OR_IDENTITY_REJECTED` с error bridge и две stock teardown
ошибки `SCR_BaseResupplySupportStationComponent`. Последние присутствуют в
документированном историческом baseline; конкретный cleanup error не объявляется
сохранённым baseline без отдельного воспроизведения. Все native resource,
Hierarchy и pathfinding ошибки также сохранены в полном log и
`runtime-everon-1-errors.json`.

Прогоны 2 и 3 завершились штатно с exit 0, SCRIPT/VM errors — 0.
Строгий recovery audit — FAIL: положительная цепочка recovery → delivery не
состоялась. Прогон 2 показал промежуток между завершением native get-in и
физической посадкой; прогон 3 — уход вышедшего водителя по оставленному
route waypoint. Оба перехода учтены описанными выше исправлениями.

Прогон 4 подтвердил физический возврат обеих сторон примерно за 11 с, но
обнаружил ошибочную обработку `COMPLETE_TRIP`. Он досрочно остановлен после
проверки exact command line собственного server PID; `runtime-everon-4-stop.json`
содержит идентичность процесса и причину. Native exit -1, строгий audit FAIL,
штатный Stop/полное завершение игры в этом прогоне не доказаны. Все доступные
закрытые логи сохранены в `runtime-everon-4-logs/`, включая нефильтрованный
console и companion logs. Его результат не выдаётся за положительный gate.

Финальный dedicated-прогон 5 использует ту же production-версию, что
`release-*` проверки. Fixture была загружена server process до удаления её
временных Core-копий. Команда запуска:

```powershell
& ./tools/Start-AICFRuntime.ps1 -Role Server -Variant Everon -AICommanderMode USSR `
  -ProfileRoot 'C:/Users/retar/AppData/Local/AICF/LogisticsRecovery-20260906-Everon-5' `
  -AdditionalArguments @(
    '-aicfRequirePlayerForResult','0',
    '-aicfLogisticsProbe','1','-aicfLogisticsProbeDurationMs','600000',
    '-aicfLogisticsProbePrepare','1','-aicfLogisticsProbePeace','1',
    '-aicfLogisticsProbeRepeatSource','1','-aicfLogisticsProbeDepotAtSource','1',
    '-aicfLogisticsProbeMaintainDemand','1','-aicfLogisticsRecoveryProbe','both'
  )
```

`runtime-everon-5-launch.txt` сохраняет команду canonical launcher,
`AICF_RUNTIME_MANIFEST_JSON`, native `CLI Params` и `[ROSTER_READY]`.
Хеши исходников при старте — `runtime-everon-5-source-hashes.json`;
полные остановленные логи, их хеши, все error lines и audit verdict —
`runtime-everon-5-logs/`, `runtime-everon-5-log-hashes.json`,
`runtime-everon-5-errors.json`, `runtime-everon-5-audit.txt` и
`runtime-everon-5-result.json`.

Финальный native exit — **0**, `Game destroyed` присутствует.
Строгий audit — **PASS/0**: 2 доставки, 42 balance samples, длительность
664101 мс, client samples 0. Команда:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Test-LogisticsLog.ps1 `
  -LogPath .codex-runtime/logistics-recovery-20260906/runtime-everon-5-logs/console.log `
  -RequireDelivery -RequireRecovery -RequireDriverInteraction `
  -RequirePolicy -RequireLedger -RequireGraph -RequireSearch -MinimumDurationMs 600000
```

Проверены полный console и companion logs, а не только `[AICF]` индекс.
SCRIPT/VM errors — **0**, AICF error bridge — **0**. Native logs остаются
не полностью чистыми: сохранены 96 записей E/F в совокупности console и
error.log (те же сообщения повторяются в обоих файлах). Это resource GUID/
material loading, stock world keywords, дублирование Hierarchy, pathfinding
tile position и resource leak `robotomono_msdf_28.edds`. Эти категории видны
и в предыдущих прогонах задачи; audit PASS не означает отсутствие native
asset errors. Ошибка cleanup из прогона 1 в финальном прогоне не повторилась.

Подтверждённая физическая цепочка прогона 5:

| Событие | Evidence (`t_ms`) |
|---|---|
| Первый animated exit и exact возврат | 125640 → 134671, 9031 мс |
| Первая доставка после возврата | `L1_S1000001_G1`, 260 supplies, 271167 |
| Второй, уже не инъецированный fixture выход и возврат | 310289 → 320288, 10000 мс, `EXIT_CAUSE_UNRECOGNIZED` |
| Восстановление остановившегося второго рейса | `L2_S1000001_G1`, attempt 1 на 328294 после 21038 мс без progress |
| Физическое подтверждение восстановления | 342313, displacement 6,77203 м, route progress 6,57336 м |
| Загрузка и последующая доставка того же job | 445538 → 563890, 260 supplies, `discrepancy=0` |
| Повторный Stop | 604954; дальнейшие transfers закрыты |

Вся цепочка выполнена `US / slot=1000001 / generation=1`, vehicle
`0x400000000000430C`, driver `0x4000000000004342`, vehicle Rpl
`-2147460468`. Между recovery и delivery job/token не менялись.
Две доставки дали 520 supplies без повторных operation receipts.
Второй выход нельзя назвать gate interaction: известен только фактический
переход и диагностированная неизвестная причина.

Runtime contracts: policy 15/15, ledger 18/18, graph 12/12, search 6/6,
новые recovery clocks 8/8 и live identity/deadline snapshots 10/10.
`readiness_mock`, `ownership_mock` и `physical_recovery=0` явно отделяют
контракты от описанного выше настоящего движения и transfer.

Оставшиеся ограничения:

| Сценарий | Verdict |
|---|---|
| Временный выход, exact возврат и delivery того же job/vehicle | Подтверждён в dedicated Everon |
| Остановка, route retry, измеренное движение и delivery того же job | Подтверждён в dedicated Everon |
| Конечные test-only препятствия на выезде | Установлены; машина покинула площадку, контакт с конкретным камнем/забором пользователя не воспроизведён |
| Повторный доказанный провал exact slot, live cooldown, альтернативный slot и восстановление после 300 с | **NOT RUN** в физическом runtime; static/mutation guards проверены |
| Вторая попытка с альтернативным reachable endpoint после физически непроходимого препятствия | **NOT RUN** в физическом runtime |
| Реальные OpenGate/navlink и застревание в пути именно из-за world geometry | **NOT RUN**; второй выход/остановка не доказывают gate или collision |
| Живая смерть водителя и чужой occupant во время recovery | **NOT RUN**; production fail-closed guards проверены статически |
| Stale generation/token/vehicle identity, Stop и budget exhaustion | Runtime contracts и mutations; не выдаются за живую замену/смерть actor |
| Сохранение ненулевого surviving cargo | Зафиксировано только в промежуточном прогоне 1 (260 supplies), не повторено в финальном прогоне |
| Arland/RHS dedicated runtime | **NOT RUN**; их Workbench graphs прошли |
| Client/JIP/visual | **NOT RUN**, клиент не запускался |

Production hash comparison — `release-source-integrity.json`: совпадение с
запущенной версией и отсутствие обеих fixture-копий. `git diff --check` —
PASS/0 (`release-final-checks.json`). Коммит не создавался.

## Изменённые файлы

Все production-пути ниже относительны `AIConflictCore/Scripts/Game/AIConflict/`.

| Файл | Изменение |
|---|---|
| `Config/AICF_LogisticsConfig.c` | Конечные recovery/cooldown budgets |
| `Economy/AICF_LogisticsJob.c` | Состояние route recovery и exit history |
| `Economy/AICF_LogisticsDepotRegistry.c` | Общая history exact depot/faction и Stop |
| `Economy/AICF_LogisticsExitHistory.c` | Новый exact-slot cooldown |
| `Economy/AICF_LogisticsLedger.c` | Ограниченное renewal живых reservations |
| `Economy/AICF_LogisticsService.c` | Ожидание при всех cooling slots |
| `Vehicles/AICF_LogisticsDriverInteraction.c` | Диагностика и exact возврат завершённой native chain |
| `Vehicles/AICF_LogisticsDriverRecovery.c` | Новый bounded unknown-exit observer и guarded animated return |
| `Vehicles/AICF_LogisticsRouteRecovery.c` | Новый reachable initial leg и finite motion recovery |
| `Vehicles/AICF_TransportTripController.c` | Orchestration, success/failure dispatch, teardown |
| `Vehicles/AICF_VehicleSpawner.c` | Пропуск только exact cooling candidate |
| `Vehicles/AICF_VehicleTaskHandoff.c` | Пауза owned route, единственный exact-seat get-in |
| `Vehicles/AICF_VehicleTransitFlow.c` | Физический progress вместо waypoint refresh |

Проверки и документация: `tools/Test-LogisticsStatic.ps1`,
`tools/Test-LogisticsContracts.ps1`, `tools/Test-LogisticsLog.ps1`,
`tools/fixtures/AICF_LogisticsRuntimeProbe.c`, новый
`tools/fixtures/AICF_LogisticsRecoveryProbe.c`, `docs/ARCHITECTURE.md`,
`docs/TESTING.md`, `docs/LOGISTICS_DRIVER_INTERACTION.md`,
`docs/LOGISTICS_SPAWN_CLEARANCE.md` и этот отчёт.

# Отказ покупки машины при недостатке снабжения

`AICF_EconomySystem.InspectVehiclePresetPreflight` возвращает immutable
`AICF_VehiclePresetEconomyDecision`: `INVALID_CONTEXT`, `AFFORDABLE` или
`INSUFFICIENT_AVAILABLE_SUPPLIES`. Стоимость берётся из native faction catalog;
доступный stock учитывает `LogisticsObligations`. Неверная authority, graph,
фракция, operational base, stock spawn rejection либо неизвестная стоимость
не становятся ожидаемой нехваткой средств.

В `ProcessSpawnCommit` решение читается после текущих commit/staging gates и
до `EnsureReservedLease`/`BeginAttempt`/main-world spawn/payment. Уже оплаченный
preset или текущий physical asset сохраняют прежний payment/rollback путь.
Собственная зарегистрированная `RESERVED` lease без когда-либо созданного asset
также проходит preflight: между native reserve и следующим tick запас может
уменьшиться. Успешный typed decline допустим только при total native attempts0: ранее
создававшийся candidate не может получить ложный receipt отсутствия spawn.
Site reservation/plan уже существует со стадии approach; он освобождается
штатным controller terminal handoff. Гарантия касается новой машины в campaign
world и vehicle debit. До decline собственный fleet cap равен0 либо1; штатный
controller освобождает пустую lease, после чего собственный cap обязан стать0.
Общий cap фракции может оставаться занят другими отрядами.

Receipt связывает точные trip/request/plan, operation, group/trip/request/preset
generation, selected base owner EntityID/componentRpl, native quote/stock и
obligations. Перед записью повторяются live world lookup, authority, revision,
actual stock/obligations и native catalog price. Несогласованный public constructor
status отклоняется. `MatchesIdentity` проверяет live request identity. При штатном
выходе из acquisition `RequestState.Reset` обнуляет live request generation;
после этого `MatchesIdentity` и `IsCurrentPreflight` возвращают false. Прежнее
утверждение, что `MatchesIdentity` работает после любого FALLBACK, отозвано:
plan=null без настоящего Reset было более слабой моделью. Live fence повторяет точный
fleet/lease pointer, регистрацию slot, все поколения и отсутствие vehicle,
EntityID/Rpl/prefab/lifecycle/cleanup snapshot. Наличие старого asset либо чужой,
released или незарегистрированной пустой lease даёт `INVALID_CONTEXT`.
Исторический receipt хранит lease generation и cap отдельными значениями и
остаётся читаемым после обычных ReleaseEmptyReservation/DetachLease. Fixture
проверяет live receipt до controller handoff, а после него связывает тот же
receipt/trip/request object, неизменный `Details`, operation/generations,
actual reset0/noPlan и текущие registry/group/intent. Она не вызывает live match
для исторического подтверждения.

Недостаток средств завершает request существующим `FALLBACK_TO_FOOT`. Flow не
меняет phase или waypoint; это остаётся работой TripController/TaskHandoff.
Существующий same-intent finished barrier предотвращает автоматическую новую
покупку и постоянное перестроение пешего waypoint. AcceptanceMonitor не менялся:
нет MarkComplete, очистки ранее latched failures либо whitelist причины.
Ошибка покупки после actual spawn сохраняет прежний fail-closed и rollback.

Исходный положительный сценарий без купленной машины/посадки/маршрута остаётся
неподтверждённым. После подтверждённой естественной гибели original5 отдельное
наблюдение может подтверждать корректное восстановление пешего исполнения;
оно не доказывает покупку машины или original full route.

## Проверки

`Test-VehiclePresetEconomyDeclineContracts.ps1` исполняет извлечённые реальные
quote/receipt/Record/early flow методы с API shims; текущий E2 набор содержит
114cases,4production mutants и2fixture mutations. Включён извлечённый настоящий
`RequestState.Reset`, а не только упрощённое удаление plan.
Включены exact affordability boundary, invalid context, existing assets bypass,
stale request/preset/base/world/Rpl/stock/price, prior attempts и historical
FALLBACK receipt. Canonical PS5.1 PASS:
`.codex-runtime/issue78-finalization-20261029/spawn/economy-decline-E2-foot-after-v4`.
Root full E2 snapshot203/203PASS сохраняет предыдущую fixture DDB32; текущая
fixture normal foot reconciliation проверяется отдельно. Terminal WB
текущей fixture PASS в `spawn/economy-decline-E2-prepare-v4`; independent review
114/4/2/35 и runner16/11 PASS, source drift0 в
`economic-decline-E2-review/fixture-reset-final`.

`AICF_VehiclePresetEconomyDeclineProbe.c` использует Behavior/Settings scaffold
в отдельном controlled world. Только test stock изменяется при первом commit;
здоровье и готовность не исправляются. Native navmesh tile load ждётся в старом
180s setup budget. Независимые hooks считают actual main-world spawn, fleet lease
и vehicle payment. Insufficient case требует zero side effects, restored current
foot order, того же native leader walk≥10m и20s без нового same-intent request.
Affordable case требует одну настоящую машину и debit ровно quote. Число
прошлых empty leases сохраняется: каждый обязан иметь native receipt отсутствия
asset, снятого cap и удаления из registry; одновременно разрешён ровно один cap
и один текущий bound asset. Обычный безопасный replan не приравнивается к
повторной покупке.
`Test-VehiclePresetEconomyDeclineLog.ps1` проверяет полные stopped logs,
CLI/source, typed quote и independent handoff; fatal/resource gates раздельны.
`DRAIN_AFTER_RESERVE` уменьшает test stock строго после успешного native reserve
и требует typed decline именно той же пустой lease/epoch при cap1, затем её
штатный release/cap0. Geometry replan с последующим decline без lease не является
доказательством этого окна.

Private VBState не входит в обычный faction reconciliation. Поэтому fixture
после exact typed terminal decline вызывает настоящий `ReconcileStrategicOrder`
с обычным commander interval/dwell, сохраняя intent/kind/target/group/generation
до и после вызова. Она не создаёт waypoint самостоятельно и не повторяет player
intent. Barrier остаётся не менее20s; прежний60s бюджет ждёт реального current
owned waypoint и движения того же живого лидера≥10m.

Воспроизводимый runner: `tools/Invoke-AICFVehicleEconomyDecline.ps1`.
`SourceRoot` может указывать сохранённый source checkout до исправления;
`ExpectedProductionManifest` проверяет каждый raw SHA. Для трёх текущих случаев:

```powershell
./tools/Invoke-AICFVehicleEconomyDecline.ps1 -EvidenceRoot .codex-runtime/economy-insufficient -Mode INSUFFICIENT -ServerPort 2660
./tools/Invoke-AICFVehicleEconomyDecline.ps1 -EvidenceRoot .codex-runtime/economy-affordable -Mode AFFORDABLE -ServerPort 2660
./tools/Invoke-AICFVehicleEconomyDecline.ps1 -EvidenceRoot .codex-runtime/economy-drain -Mode DRAIN_AFTER_RESERVE -ServerPort 2660
```

Runner сохраняет terminal WB, exact runtime manifest, native exit, полный stopped
profile, source hashes и frozen auditor. `PrepareOnly` не запускает native test;
`StageRoot`/`SkipCompile` принимают только уже проверенный неизменный stage,
повторно сравнивая весь source set/SHA с parent manifest, который компилировался.
Expected production manifest требует точного набора файлов; разрешены только
три указанные fixture paths и generated resourceDatabase.rdb.
Полный audit может иметь resourceFAIL при функциональном PASS; результат не
фильтруется. Это controlled economy proof, не ordinary30min/full route.

E2 BEFORE `economy-decline-E2-before-drain-native-v1` воспроизвёл старый
IDENTITY latch после reserve→stock drain при native0/source0. E2 AFTER
`economy-decline-E2-insufficient-native-v1` подтвердил atomic decline, но сохранил
functionalFAIL из-за отсутствующего normal infantry reconciliation приватного
стенда. `insufficient-native-v2` остановлен с отдельным fixtureFAIL: новый guard
ошибочно требовал live request generation после реального Reset0. Это не ошибка
производственного preflight. Новая fixture исправляет состав и момент проверки
стенда; оба старых результата не пересчитаны.

Финальные AFTER на immutable340-E2 canonical manifest2E82 и fixture1D64:

| Case/evidence в `spawn/` | Native/source | Functional/fatal | Resources | Фактический результат |
|---|---|---|---|---|
| `economy-decline-E2-insufficient-native-v3` | 0/drift0 | PASS/PASS | FAIL28 | quote475/stock0, reserve0/spawn0/payment0; тот же leader57.7185m, barrier20.088s |
| `economy-decline-E2-affordable-native-v1` | 0/drift0 | PASS/PASS | FAIL28 | одна actual car/current bound lease, cappeak1, payment475; exact current trip |
| `economy-decline-E2-drain-native-v2` | 0/drift0 | PASS/PASS | FAIL28 | reserve1→stock675→0; следующий tick stock20/quote475; та же lease1/cap1→typed decline→native empty release/cap0; leader50.8477m, barrier20.032s |

Все decline cases требуют RegistryCreate1 и неизменный current trip. Нативный
income20 между drain и следующим tick сохранён как фактический stock; он не
подменялся повторным drain. Affordable сохраняет raw exact own
COORDINATOR_STOP latch после DONE в ограниченном purchase-only teardown scope.
Общие SCRIPT/engine fatal и resources проверяются по всем sibling logs.
Все native cases остановлены, own CIM0 в `spawn/economy-E2-native-closed-final.json`.

`economy-decline-E2-drain-native-v1` остаётся coverageFAIL: после drain физическая
проверка площадки штатно отклонила FOOT_PAD_PROJECTION_GAP, lease освобождена до
экономики, поздний NO_LEASE decline не принят за E2 window. Fresh v2 прошёл без
изменения fixture/geometry rules. Общий runtime exit runner остаётся1 из-за
resourcesFAIL; эти результаты не объявляют полностью чистый gate или original
full route.

Исходный US ordinary diagnostics-v6 сохраняет frozen E550 functionalFAIL:
две ошибки были mixed EntityID orientation в старом анализаторе, третья —
настоящий generic IDENTITY latch нового экономического отказа до recoveryDONE.
Его старый результат не переобъявляется PASS. Native1800s/source337drift0,
fullfatal0/resource32FAIL сохранены в `spawn/soak-US-proof4-diagnostics-v6`.

# Compartment topology и inventory посаженного экипажа

`AICF_AIQuotaVehicleUse.CollectSeats` собирает структурные compartment managers
автомобиля. Seated `ChimeraCharacter` и вложенный inventory относятся к составу,
а не к этой структуре. Их subtree исключается до добавления в очередь обхода.
Это не исключает occupants из учёта: все seats структурных managers по-прежнему
читают `GetOccupant()`, затем выполняются исходные `CaptureRow` и occupant
`Capture`/`IsCurrent`, включая human custody и native agent flags.

В прежней реализации `GetChildren()` обходил также экипаж с оружием, одеждой и
предметами. После нескольких посадок такой общий граф превышал256nodes, поэтому
полный quota census оставался UNKNOWN и блокировал последующие допустимые
операции. Bounds и отказ при неопределённости правильны; состав графа был шире
ответственности vehicle compartment topology.

Native US diagnostic600 с исходной реализацией дал fresh hierarchy witness:
car20EC/Rpl-2147454305,316nodes,4characters,204nodes в character subtrees,
2managers и265inventory-marked nodes. Root stable, walk complete, parent/world/
null/duplicate/truncation flags0. Оставшаяся структура —112nodes. Witness помечен
`AFTER_DECISION_NOT_ORIGINAL_INPUT`: он не подменяет original local значения
решения. На этом наблюдённом стабильном графе прежний256bound неизбежно отвергает
обход; отдельный native повтор исправления ещё требуется.

## Сохранённые границы

- Child parent и world проверяются до type boundary. Неверный foreign character
  тоже отвергается, хотя его inventory не обходится.
- `MAX_TOPOLOGY=256` сохраняется для structural nodes и examined seats. Каждый
  список siblings имеет тот же256bound и собственный seen set, включая пропущенные
  characters: self/sibling cycles и бесконечный пропуск actors невозможны.
- Character с собственным `BaseCompartmentManagerComponent` даёт fail-closed,
  не доказательство пустой машины. Возвращаемый seat обязан иметь owner внутри
  собранных structural nodes, manager и exact `GetVehicle()`.
- Пин1.8.0.13 `BaseCompartmentSlot.GetVehicle` идёт по parent chain к внешнему
  manager owner. Одного `GetVehicle()==root` недостаточно, чтобы разрешить owner
  внутри исключённого character subtree; проверка `nodes.Contains(owner)` сохранена.
- Один exact seat, опубликованный двумя managers, учитывается один раз.
  Повторное чтение topology и исходных occupants остаётся обязательным в `IsCurrent`.
- `InventoryItemComponent` не является основанием пропуска. В native witness
  inventory-marked nodes больше, чем всех character subtrees; часть таких nodes
  относится к структуре самого автомобиля.

Новые permissions, world queries, pose/action/health writes, timers, quota units
или deadlines не добавлены. Распределение native stock/AICF, physical custody,
неизвестные occupants и native hard/faction gates этим изменением не ослабляются.

## Проверки изменения

Baseline: production384 из V25 Full261 PASS; этот прежний полный набор не выдаётся
за проверку новой правки. Перед изменением дополнительно выполнены canonical
`Test-AIQuotaVehicleBackendContracts.ps1` и `Test-AIQuotaVehicleDispatchContracts.ps1`
с сохранением полного вывода; после изменения оба повторены — PASS. Backend
исполняет actual ledger/census/vehicle paths116cases и8unsafe variants, также
source entry56cases. Исходные native US/USSR boarding FAIL сохраняются.

Новый `Test-VehicleCensusTopologyContracts.ps1` исполняет полные actual production
`CollectSeats`, `CaptureRow`, `IsCurrent`, identity и occupant predicates:
прежний body с закреплённым SHA отвергает3/4/5characters с100вложенными предметами;
новый проходит те же три случая. Всего36cases и8executable unsafe variants — PASS.
Проверены structural257, bounded skipped-character siblings, world/parent/cycles,
character manager, seat owner под исключённым character, чужая машина/manager,
повторный seat, stale occupants/controller/agent/health/human flags и native row.
Человек в сиденье по-прежнему наблюдается; его смена не считается прежней custody.

Первые два запуска нового harness сохранены как FAIL: отсутствовало Enforce bool
преобразование массива в C# shim; затем authority ошибочно проверялась у чистого
read-only `CollectSeats`. Корректный authority отказ проверяется на существующей
границе `CaptureRow`. Исправления касались harness; production guards не снимались.
Последний targeted — PASS; `git diff --check` — PASS.

Новый production384 Stock/RHS/ArlandWCSRHS Workbench —3/3PASS, exit0/validation/
full3sibling fatal0/sourceSHA0, session34961 closed0. Combined Soak390v8 и
Multi388v4 на тех же bytes отдельно compiled0/fullfatal0/source0; sessions40727
и65151 closed0. Root independent replay также old3/new36/8unsafe PASS.
Frozen V27 завершён:262/262PASS, collector0/exact union/Logistics1/1021SHA0,
все6actual sessions closed0. Native повтор **NOT RUN**. Ни устранение всех
Census UNKNOWN, ни прохождение исходного
ordinary1800/MOVING6 пока не объявляются. Evidence и before source:
`.codex-runtime/issue78-finalization-20261030/vehicle-census-topology-fix/`.
Compile receipts: `.codex-runtime/issue78-finalization-20261029/topology-v27-*`,
`ordinary-topology-diagnostic-source-v8/`, `multi-topology-diagnostic-source-v4/`.

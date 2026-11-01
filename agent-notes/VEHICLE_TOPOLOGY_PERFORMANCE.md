# Производительность чтения compartment topology

`AICF_VehiclePreset.Collect` раньше выполнял insertion sort мест и вызывал
`RelativeOwnerIndex` от двух до четырёх раз на сравнение. Каждый вызов повторял
полный DFS vehicle hierarchy. Стоимость составляла `O(S² × H)`, где `S` — число
мест, `H` — количество entity в иерархии машины. `H` включает пассажиров и их
inventory; это важно для полной машины с экипированным составом.

Теперь `Collect` один раз получает тот же полный DFS owner list и сортирует
параллельные ordinal values. `CollectCaptured` один раз получает owner list и
применяет прежние structural/accessibility guards. `MatchesSeats` использует
локальные таблицы для каждого distinct vehicle: обычный однородный массив мест
требует один DFS; даже mixed-root input сохраняет прежнюю семантику ключей.

Снимок существует только внутри одного синхронного вызова. Между ticks нет
cache, strong ownership или новой invalidation policy. Иерархия не обрезается
на `ChimeraCharacter`/inventory. Порядок owner ordinals, stable seat keys,
catalog accessibility filter, captured inaccessible seats, запрет foreign/missing/
duplicate/accessible extras и atomic output не изменены. Standalone `SeatKey` и
`RelativeOwnerIndex` сохраняют прежний полный обход.

## Offline доказательства

`Test-VehicleTopologyPerformanceContracts.ps1` исполняет actual production тела
и сравнивает их с зафиксированными pre-change методами. Native object/array API
заменены узкими адаптерами. Проверяются 1/5/15/32/64 места, разный размер inventory
поддерева, nested compartment owner внутри character subtree, native enumeration
order, mutable accessibility, смена hierarchy между вызовами, mixed vehicle roots
и outside owner. Счётчик native-like `GetChildren` подтверждает не более одного
полного DFS на `Collect` и один на `CollectCaptured`/однородный `MatchesSeats`.

2319 actual cases, 3 executable unsafe mutations и 6 fixture scope controls
прошли. Та же новая проверка отвергает frozen старое тело `Collect` в отдельной
source copy: нарушается traversal budget. Это доказательство устранения повторных
обходов, **не измерение реального времени Enfusion и не доказательство причины
конкретного кадра 2.5с**.

Релевантные 8 исходных проверок дали baseline8/8 и after8/8 PASS. Финальная
выборка с новым контрактом —9/9 PASS; последующий fixture guard rerun —2/2 PASS.
`Test-VehicleCapturedTopologyContracts` сохраняет15 resolver cases; старый
синтетический Ordinal=-1 заменён реальным owner вне model hierarchy с согласованным
manager, поскольку production теперь читает actual owner list.
Evidence: `.codex-runtime/topology-performance-20261101/`.

## Отдельный native benchmark

Stage-only `AICF_VehicleTopologyPerformanceProbe.c` подключается явно с
`-aicfTopologyPerformanceProbe 1`; `-aicfTopologyPerformanceIterations` допускает
1–3 повтора, default1. Он ничего не создаёт, не перемещает и не покупает.

После единственного `super.Update` fixture ждёт существующий actual trip с ровно
10 живыми conscious original members, всеми10 в exact settled compartments того
же authoritative vehicle, без foreign/duplicate occupants. Для каждого бойца
проверяется непустой native inventory. До/после benchmark повторяются group/vehicle
entity/Rpl, generations, preset/assignment identity, member IDs и physical occupancy.

Derived legacy class содержит старые literal `OldCollect`, `OldCollectCaptured`,
`OldMatchesSeats`. По одному и тому же actual native vehicle печатаются отдельные
старые/новые timings, результаты и равенство references/keys:
`TOPOLOGY_PERFORMANCE_CASE`, затем `TOPOLOGY_PERFORMANCE_DONE`.
Этот benchmark сознательно добавляет дорогие диагностические вызовы и имеет
`ordinary_fps_measurement=0`. Он не включается по умолчанию и не закрывает игру.
Отсутствие DONE означает отсутствие benchmark proof, а не PASS.

Root завершил terminal Workbench Stock/RHS/ArlandWCSRHS3/3 и frozen WCS North
current-v1: native0/stopped/fullfatal0, source395/drift0, exactCLI. На actual
оплаченном Ural с10equipped crew/672raw inventory entries old/new Collect299/0мс,
MatchesSeats10/1мс, Captured26/0мс;15мест, references/keys/results совпали,
identity/10settled occupants до/после сохранены.0мс — ниже разрешения таймера.
Полные sibling resource errors466 остаются FAIL. Общий FPS/load/startup scope
и оставшиеся ограничения — [SERVER_PERFORMANCE.md](SERVER_PERFORMANCE.md).

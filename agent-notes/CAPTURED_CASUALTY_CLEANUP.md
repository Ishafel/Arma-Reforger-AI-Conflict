# Исходный погибший actor вне native roster

Финальный commit proof входит в frozen `r30`: **265/265 PASS**, collector0,
exact unique union/Logistics1/1029SHA0 и6actual sessions closed0. Production386
Stock/RHS/ArlandWCSRHS terminal WB3/3PASS/native0/validation/full3siblingfatal0;
прочие Error rows50/52/78 сохранены как strict baseline. Native нового cleanup
пока NOT RUN; WCS bootstrap и контролируемая проверка учитываются отдельно.

В ordinary USSR 1800 на source V27 зафиксирован конкретный blocker: водитель
`0x4000000000000EED` (`actor_rpl=-2147475748`) исходной группы
`0x400000000000013C`, slot `USSR/0`, group/trip/lease/vehicle generation `1`
остался `DEAD` в точном seat `0:2:0` машины
`0x400000000000295A` (`rpl=-2147447129`). Native parent group уже отсутствовал
у этого actor, сама группа и её живые участники сохранились.

`02:34:36.696 VEHICLE_CLEANUP_RETAINED` с
`PROTECTED_CLEARANCE_FOREIGN_GRACE_EXPIRED` подтверждён следующим
`VEHICLE_CLEANUP_BLOCKING_ACTOR` в `02:34:36.697`: exact occupant `1`,
`life_state=DEAD`, controlled/main player id `0`, authoritative master,
`historical_assignment=TRIP_PRESET`. `player_scan=1` означает выполненный
опрос игроков, а не присутствие игрока в машине. Birth и исходная Rpl identity
водителя подтверждены в `02:24:27`; последнее сохранённое наблюдение ALIVE —
`02:33:33`. Точное время и причина смерти не установлены.

Прежний orphan recovery требует отсутствия всей группы; другой путь перебирает
только текущий `group.GetAgents()`. Поэтому исходный DEAD actor, исключённый
движком из сохранившейся группы, не попадал ни в один список кандидатов.
Диагностический `ActorHistory` сам по себе не разрешает физическое действие.

## Граница исправления

Новый `AICF_CapturedDeadCleanupIdentity` выдаёт отдельный capability
`CAPTURED_DEAD_CLEANUP` на terminal trip. Он связывает admission roster,
исходные world/entity/Rpl/component/controller, точные group/slot/spawn/trip,
неизменный assignment и его revisions, terminal transition/cause,
cleanup job/action token, fleet, lease и frozen fence со всеми generations,
физическую машину и её исходный Rpl component.

Разрешён только исходный ALIVE при admission actor, который сейчас DEAD,
authoritative и не управляется игроком. Parent `NONE` либо исходная группа
допустимы; другая группа запрещена. Проверяются PlayerManager, possessing main
entity, controller flag и все `GetAllPlayers()`/PlayerController. Неизвестный
player/controller, несовпадение списка, более 256 записей, смена world,
компонента, поколения или владельца приводят к отказу.

Только CleanupManager вызывает штатный `CompartmentAccessComponent.KickFromVehicle`
для того же exact actor/access/seat/vehicle после повторных guards, включая
проверки после door/player getters. После native callback смена контекста
останавливает текущий cleanup проход. Эффект использует прежний общий лимит
`NON_ALIVE_DETACH_MAX_ATTEMPTS=3` и интервал `1000 ms`; отдельный лимит не добавлен.
Прежние relocation budgets, protected-clearance deadlines и полный scan перед
release/delete не изменены. Жизнь, здоровье, позиция, native group membership и
foreign occupants не изменяются этим кодом. Orphan resolver не расширен.

## Проверки и evidence

Fresh baseline до правки: пять профильных контрактов PASS, exit `0`:
`Test-OrphanTripRetirementContracts`, `Test-CleanupAliveHullContracts`,
`Test-CleanupIncapacitatedHullContracts`,
`Test-CapturedDeathRetirementProjectionContracts`,
`Test-VehicleCleanupTraceContracts`.

Первый контракт **65 actual-source cases + 13 source mutants PASS** не покрывал
позднюю смену human ownership в последнем seat getter. Отдельный actual-source
counterexample воспроизвёл ошибочный `Kick` для каждой из четырёх ветвей:
controller flag, PlayerManager controlled id, possessing main id и другой
PlayerController. Этот FAIL сохранён; прежний PASS не доказывает новый порядок.

Финальный `ResolveTerminalDeadActorForCommit` читает исходные world/entity/Rpl/
component/parent до exact access/seat/vehicle, затем подтверждает тот же
controller, DEAD и все nonplayer пути. Последний main-world/PlayerManager
lookup — явная native read-only часть proof, перед заключительным direct player
veto. После этого до `Kick` разрешены только project-owned identity/budget
getters и проверки счётчиков. Native parent/access/seat/Rpl после финального
actor proof не читаются. При потере Game/actor/controller/PlayerManager либо исходной живой группы weak refs приводят к отказу, включая момент после последнего PC getter. Финальный nonnull commitActor обязателен: сравнение двух null не даёт разрешения. Повтор all-PC после parent чтений закрывает также
поздний parent callback, меняющий PlayerController.

Расширенный контракт: **90 actual-source cases + 15 source mutants PASS**,
включая прежние 65; два исполняемых unsafe mutants дают ожидаемый FAIL в обеих
версиях PowerShell. Дополнительно выполнен общий бюджет трёх попыток с
отказом четвёртой. Он исполняет извлечённые production
method bodies в C# с явно моделируемыми native boundaries: positive исходного
DEAD вне roster, unsafe identity/life/player/world/seat/lease/generation/budget
и callback cases. Это не native физический detach и не доказательство release.
Первые ошибки транслятора/тестового harness сохранены отдельно; они не скрыты
под итоговым PASS. Workbench и native runtime для нового production изменения
пока **NOT RUN**, до отдельного разрешённого окна запусков. Три профильных
статических аудита `Test-Stage3Static`, `Test-Stage35Static`,
`Test-Stage35RecoveryPolicy` после правки также PASS, exit `0`.

Полные baseline, source copies, wall intervals и результаты:
`.codex-runtime/issue78-finalization-20261030/captured-casualty-cleanup-fix/`.
Исходный полный live prefix, hash и индексы:
`.codex-runtime/issue78-finalization-20261030/cleanup-protected-occupant-readonly/`.
Live prefix не выдаётся за аудит полного остановленного server log.

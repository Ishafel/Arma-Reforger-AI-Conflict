# Исходный погибший actor вне native roster

## Отозванный production seat emitter и отдельный observer

Попытка добавить canonical fields прямо в `VEHICLE_PRESET_OCCUPANT` отозвана
после peer review. Native temporary `GetID()` vararg и cached EntityID могут
печататься по-разному; caching мог нарушить существующие regex/raw-string joins
в Preset/Transit/Scenario/DamageFallback/QueuedApproach и остальных log audits.
BoardingFlow восстановлен побайтово до baseline; старый event и его потребители
не изменены. Новый untracked emitter contract перенесён в ignored evidence,
в runner/релиз не входит. Никакие aliases, low32 conversions или ослабления
обычных/Multi критериев не добавлены.

Baseline ParallelEntry37/EntryTrace43 PASS, deferred emitter model9/3 PASS в обеих
версиях PowerShell сохраняются как история неотправленной diagnostic идеи,
не как проверка shipped production. Полные source copies, первый harness FAIL,
выводы и exact-before SHA: `.codex-runtime/issue78-finalization-20261030/seat-canonical-bridge/withdrawn-receipt.json`.

Одобрен следующий изолированный read-only D1 observer: `CANONICAL_SEAT_CAPTURE`
из уже существующего foreach реальных seats, cached actor row/Rpl/EntityID и
единожды прочитанного прежнего GetType. Пять unique canonical actors/seats,
pilot и исходные admission identities проверяются непосредственно по этим
наблюдениям; старые production counts/slotkeys остаются отдельным источником.
Это смена источника строгого физического proof, а не соединение decimal/hex.
В immutable r32/396 такого observer ещё нет: canonical seat criterion для него
`NOT RUN`/FAIL, старые native результаты не пересчитываются в PASS.

## Accepted retarget и отдельный cleanup assignment fence

В controlled r31 actual900 исходный admission snapshot5 сохранялся, но обычное
восстановление пешего приказа после `CREW_UNAVAILABLE` создало runtime revision6.
В05:52:56.152 штатный `SAFE_REUSE_RETARGET` принял snapshot6; RESET принят позже,
в05:52:56.183. Следующий тик завершил trip по `PRESET_CHANGED`. Новый read-only
observer показал первый реальный отказ `ADMISSION_03 actual=0
current_assignment_same=0`; исходные intent2/base1 и group/trip generations1
сохранились. Controlled DONE05:55:55.505 — `FAIL`, `detached=0`. Это конкретный
доказанный отказ pointer guard, без заявления об атомарном native snapshot.

Исправление вводит отдельный cleanup assignment fence. Исходный admission
snapshot, его revisions и пять actor records неизменны. Exact pending token
создаётся до штатного retarget, actual `Trip.CommitRetarget` отмечает принятие
только своего candidate, а Controller завершает перенос только после прежних
lease postchecks. Перед записью проверяются captured group/world/Rpl и ещё раз
чистые trip/slot/token/current-assignment/lease fields. Stale callback отменяет
только свой token; terminal и Stop инвалидируют pending token, Stop запрещает
повторный запуск этого bookkeeping. Отказ переноса не меняет gameplay результат
или допустимые новые intent/цель штатного retarget.
`RecordCommitted` явно отвергает null caller/candidate, в том числе после
истечения слабой ссылки token→Trip, до разыменования.

Observer сохраняет прежний `current_assignment_same` для original admission и
add-only выводит `cleanup_assignment_same`, cached cleanup assignment/intent/base
revisions. Поэтому original=false при cleanup=true и ADMISSION_FINAL=true имеет
однозначное значение принятого fence. Это только cached script fields после
решения; новый native getter или повторная проверка admission не выполняются.

Terminal capability продолжает захватывать exact текущий terminal snapshot и
все transition/cause/job/fleet/lease/vehicle fences. Поздняя замена snapshot,
включая чужой объект с теми же полями, не допускается. Actor/player/DEAD/seat,
budget/deadline/cap/release policy не ослаблены. Add-only
`member_id_canonical` хранится из исходного `EntityID.ToString()`; pure accessor
`GetCapturedMemberIdString(index)` не делает новых native queries и не создаёт
numeric/hex alias. Admission и production Kick сохраняют старые поля и добавляют
canonical string; vehicle `entity_id_canonical` также берётся из cached fence
без повторного actor/world чтения. Сравнение полных canonical strings не заменяется
усечением handle64, low32 или преобразованием старых несовместимых форматов.

Новый `Test-CleanupAssignmentRebindContracts.ps1` проверяет связанные actual
source сегменты planner/accepted commit и RESET/End/terminal capability/physical
handler с явными native-boundary shims. Это не полный мир, не native physical
release и не ACK. **48 actual cases + 4 executable unsafe PASS** в pwsh и WinPS5.1:
old r31 admission отказывает для обоих accepted retarget, новый fence допускает
same-intent rebuild и допустимый new intent/target; исходные5 records и4 ALIVE
сохраняются. Проверены forged/no-commit/successor/lease/terminal/Stop и поздние
native group component callbacks с заменой world/authority перед чистым tail.
Отдельно **старые90/15 и observer57/3 PASS** в обеих версиях PowerShell; два
прежних unsafe дают ожидаемый exit1. Старый90 oracle не ослаблен: механически
добавлены реальные accessor/fence methods и поля boundary shim. Observer inverse
сохраняет прежний r30 hash после удаления только диагностических hooks и точной
обратной подстановки заявленной admission fence delta.

Полные outputs и closures: `captured-casualty-cleanup-fix/rebind-final-targeted-v1/`
(06:14:38.627–06:15:06.349) и `rebind-final-targeted-v2/`
(06:19:39.081–06:19:57.607). Старые55 observer cases сохранены; добавлены два
cached split/revision-dedupe случая. Три соседних static audits также PASS в
`rebind-neighbor-statics-v1/`. Первые translation/wiring failures сохраняются в
`rebind-contract-preparation/`, `rebind-targeted-v1/`; они не выдаются за production
runtime результат. Старый Full267 относится к immutable r31 до исправления.
Новый WB и native900 пока `NOT RUN`; старые controlled FAIL сохраняются.

## Поздний controlled r30 FAIL и диагностический seam

Остановленный `controlled-r30-USSR-900-v1-receipt-replay-v2` воспроизвёл
предусловие на неизменных r30 production386+D1: обычные оплаченные исходные5,
exact5seats, единственный native Kill исходного Pilot `0x4000000000000EF0`
(`actor_rpl=-2147475742`), машина `0x400000000000267F`
(`rpl=-2147450253`), все group/trip/lease/vehicle generations1.
05:13:31.092 наблюдались тот же DEAD exact seat, `KNOWN_AGENT_ABSENT` при
сохранившемся AIControl и четыре исходных ALIVE, затем принят обычный RESET1→2.
Однако production captured-dead Kick не зарегистрирован. В05:14:20.439
`VEHICLE_CLEANUP_RETAINED` оставил protected corpse/capHELD, а D1 в05:16:30.260
завершился `POST_KILL_CONTROLLED_DEADLINE`, detached0. Это настоящий FAIL,
не SETUP_NOT_REPRODUCED и не подтверждение исправления выше.

Полный900s завершён native0/actual runner17603 exit1/audit1. Внешний exact
auditor `4EB3CC33…F766CAB` через WinPS5.1 также получил FAIL: full sibling
fatal6/otherError28, original Soak failures2. Не выполнены production Kick,
physical detach и original release/ACK. Контракт не ослаблялся; причины отказа
конкретного guard старый log не содержит. Отличие представления EntityID в
direct getters и cached handle fields не разрешает low32 alias или ручной PASS.
Evidence — ignored finalization20261030/`controlled-r30-USSR-900-v1-receipt-replay-v2/`,
`controlled-external-audit/`, read-only live indexes.

Следующее изменение — только observability. Пустые instance hooks
`AICF_ObserveTerminalAdmissionDecision` и
`AICF_ObserveCapturedCleanupDecision` получают уже вычисленный actual bool
и literal guard stage непосредственно перед исходным return. Для final return
исходное выражение вычисляется один раз и возвращается та же local value.
Условия, их порядок, short circuit, native calls и cleanup policy не меняются;
inverse трёх методов совпадает с r30 после удаления scaffolding и нормализации
whitespace. Новые блоки используют Allman/tabs.

Изолированная `AICF_CapturedDeadCleanupDecisionProbe.c` включается только через
`-aicfCapturedDeadCleanupDecisionProbe 1`. Она пишет фактический stage/result и
cached script context после решения: assignment refs/revisions, transition/cause,
job/lease/fence refs, cached generations/state/EntityIdString. Native actor,
seat/world/component getters и повторные admission/IsCurrent не вызываются.
Cache ограничен64frames/512records/2048символами details; Stop выключает probe
и очищает только диагностические данные. Factory wrapper вызывает один
`super.CaptureCapturedDeadCleanupIdentity(job)` и возвращает тот же результат,
включая ранний null. Production static factory не переопределяется.

Отдельный opt-in `-aicfCapturedDeadCleanupRearmExact 1` при имеющемся
`-aicfQuotaCensusExactCaptureProbe 1` может один раз перед первым cleanup factory
контролируемого Soak faction/slot0 вызвать существующие Exact `Stop→Start`,
только когда `s_Frame==null`. Это происходит до original factory, не после
native actor proof. Отдельный `CAPTURED_DEAD_CLEANUP_EXACT_EPOCH` marker
фиксирует новую диагностическую эпоху; прежний boot384 overflow остаётся в
полных логах. Новых CallLater, изменения D1/Exact fixture, квоты, бюджетов,
health/life или gameplay deadline нет. Это не непрерывная Census coverage.

Новый canonical contract: **3 policy inverse +55 actual fixture/callback/cache/
lifecycle/factory/exception cases +3 executed unsafe variants PASS** в WinPS5.1
и pwsh. Прежний90/15 остаётся PASS в обоих shell; изменён лишь extractor пустой
actual hook declaration, его oracle/cases/mutants не менялись. Оба прежних
unsafe final-human/final-actor варианта по-прежнему дают ожидаемый FAIL.
Workbench и следующий diagnostic native для этого seam учитываются отдельно;
эти offline результаты не исправляют описанный r30 physical FAIL.

Пустые hooks вошли в новый production387: terminal WB Stock/RHS/ArlandWCSRHS
actual14391 CLOSED0/native0/validation/full3fatal0/source387SHA0; прочие Error
50/52/78FAIL сохранены. Отдельный diagnostic controlled396 (387+9fixtures)
compiled actual24094 CLOSED0/native0/full3fatal0/source0. Новый900s native
`controlled-r31-USSR-900-v2` сейчас выполняется, stopped verdict ещё нет.
Его wrapper меняет только CLI decision observer/rearm, original D1 и игровые
gates сохранены. Предыдущий v1 остановился до native из-за отсутствующего
auditor в урезанном tools source; v2 использует полный неизменный r30 tools/
auditor repository и тот же новый compiled396 stage. Это не повтор WB и не
замена игровых исходников. Общий All267 ещё отдельный ожидающий gate.

Финальный commit proof входит в frozen `r30`: **265/265 PASS**, collector0,
exact unique union/Logistics1/1029SHA0 и6actual sessions closed0. Production386
Stock/RHS/ArlandWCSRHS terminal WB3/3PASS/native0/validation/full3siblingfatal0;
прочие Error rows50/52/78 сохранены как strict baseline. На момент заморозки
native cleanup был NOT RUN; последующий controlled r30 FAIL описан в начале
этой заметки. WCS bootstrap не подменяет физическую проверку.

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

## Совместный Stop pending token и native gate observer

В Full r32 сохранён исходный FAIL `Test-NativeGateCompletionContracts`:
`INVALID_TRIP_STOP_DETACHES_LOCAL_OBSERVER`. Причина — устаревший exact-prefix
контракт `AbortForStop`, который не разрешал новую строку
`trip.StopCleanupAssignmentRebind()` до снятия native gate listener и до
invalid-retirement return. Production уже сохраняет null guard, локальный
listener Stop и прежние retirement/asset fences; gameplay для этой правки
контракта не менялся.

Контракт теперь требует ровно этот дополнительный lifecycle вызов; произвольные
actions до invalid-trip return по-прежнему запрещены. Replay исполняет actual
Trip `StopCleanupAssignmentRebind`/`InvalidateCleanupAssignmentRebind` вместе
с actual Controller prelude. Для invalid trip, отсутствующего movement state
и retirement asset проверяются снятие token и запрет его перезапуска без
изменения native action. Прежние callback/Insert/Remove/physical/deadline
контроли сохранены. PowerShell7 и Windows PowerShell5.1: **PASS29 controls +
2 executed unsafe**, оба exit0. Unsafe варианты реально оставляют pending token
при пропуске Stop либо его clear; тест их отвергает. Baseline FAIL и начальный
adapter-only placeholder FAIL сохранены отдельно, production source SHA до/после
совпадает. Evidence:
`.codex-runtime/issue78-finalization-20261030/native-gate-completion-r32-contract/`.
Эта проверка не подменяет новый полный offline набор, Workbench или native run.

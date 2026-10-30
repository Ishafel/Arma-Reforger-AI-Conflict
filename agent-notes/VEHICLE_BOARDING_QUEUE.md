# Подход к машине до очереди входа

В current383 ordinary US/USSR обнаружен дополнительный initial-boarding случай:
собственный GetIn остаётся `EVALUATED` в utility, а current и executed действия
бойца — один `SCR_AIIdleBehavior:RUNNING`, группы — один
`SCR_AIIdleActivity:RUNNING`. У USSR последний пассажир стоял1.92м от машины,
точное пустое место было доступно;20с не было progress, но прежний short-recovery
trigger принимал лишь FAILED/disappeared/current-stalled. Оба исходных native
FAIL и полные1800s наблюдения сохраняются отдельно.

`AICF_PassiveBoardingRecovery` добавляет только этот узкий read-only proof:
тот же queued/owned GetIn,≥5с без progress, exact passive current/executed pointers
и отсутствие других живых competing actions. Обе очереди проверяются полностью
до64записей; null/overflow означают UNKNOWN. Proof связывает phase, transition,
trip/lease/assignment/token set/control agent/utility и исходный deadline.
После quota/physical getters он повторяется до owner-safe Cancel; после Cancel
допускается лишь тот же terminal GetIn при неизменных idle pointers. Последнее
повторение выполняется перед pure counter Commit; между Commit и GetIn нет
native getters/callbacks. Прежние quota veto,25м/5м/1м·с⁻¹, one-shot budget и
абсолютные сроки сохраняются. Длительный quota CENSUS_UNKNOWN может по-прежнему
запретить dispatch: этот отдельный blocker не обходится idle proof.

`Test-PassiveBoardingRecoveryContracts.ps1` исполняет actual scope и flow,
исходный flow отклоняет положительный observed case. Actual scope/flow41cases,
5unsafe и counter/native adjacency27cases/6unsafe прошли независимый повтор.
Fixture-free payload384 прошёл terminal Workbench Stock/RHS/ArlandWCSRHS3/3:
native0/validation/fullfatal0/sourceSHA0. Новый native positive ещё NOT RUN;
старые current383 native stages неизменны. Полные compile receipts:
`.codex-runtime/issue78-finalization-20261029/idle384-three-graph-verdict.json`.

В Multi-v9 последний назначенный пассажир около108с стоял без actor task,
пока два других бойца занимали доступные входы. Когда ему выдали GetIn,
он действительно бежал к машине16.9с и оставался в34м при истечении общего
boarding deadline. Это не RUNNING stall последнего бойца: последний progress
age составлял1065мс. Короткий перенос или сброс deadline здесь не обоснованы.

`AICF_PresetQueuedApproach` даёт далёкому, ещё не поставленному в очередь
GetIn бойцу один собственный `SCR_AIMoveIndividuallyBehavior`. Native entity
target — та же машина, radius8м; это радиус завершения ходьбы, не дистанция
телепортации. Несколько таких foot actions могут работать одновременно с
прежними максимум двумя GetIn. Резервации мест, лимит дверей, исходный общий
deadline, здоровье и координаты helper не меняет.

Состояние принадлежит `AICF_VehicleBoardingState`. Каждый helper привязан к
текущим trip/lease/group/actor/token/seat; проверяются actor EntityID/component
RplId, настоящий control AIAgent и его parent group, authoritative nonplayer
ownership, exact token set, preset и живой BOARDING phase. Seated, unconscious,
foreign и transitioning actors не получают foot action. Helper выдаёт не более
одной собственной native команды и не продлевает фазу при её завершении/отказе.

Перед `QueuePresetSeat` собственная foot-команда снимается. Native `Fail()`
может синхронно вызвать обработчик, поэтому после отмены снова проверяются
phase/terminal/preset/state/exit flag/deadlines/current token set/control agent/
lease fences. Utility разрешается заново и должна совпадать с той, для которой
создан GetIn. Access/occupancy/reservation также читаются повторно. Устаревший
callback не может поставить старый GetIn в очередь.

`RetirePresetQueueApproach` и `ClearPresetQueueApproaches` сначала отделяют
snapshot от массива, затем вызывают native Stop/Fail. Флаг изменения очереди
запрещает reentrant создание/вход; вложенная cleanup сохраняет прежнее значение
флага. `Reset` и `ApplyExitEffectsOwnerSafe` снимают все свои foot actions.
В Enforce `array<ref T>.Copy` не принимает такой ref-array: snapshot собирается
через `foreach/Insert` до очистки и callbacks. Первоначальный Workbench FAIL
сохранён в evidence, не замаскирован stock cascade errors.

Проверки: `Test-PresetQueuedApproachContracts.ps1` исполняет извлечённые scope/
distance/deadline guards и проверяет lifecycle. Независимый review дополнительно
исполняет настоящее тело Queue/eligibility с15 callback mutations, reentrant
Retire/Clear/Maintain и owner-only Stop. Контролируемый native probe
`AICF_PresetQueuedApproachProbe.c` удерживает только допуск одного пассажира
в GetIn до фактической native ходьбы; никаких pose/health/AI-cap writes нет.
Наблюдать нужно в каждом `TickPreset`, а не только при попытке Queue: foot action
может штатно завершиться раньше освобождения двери. Настоящий observed action,
разнесённые по времени polls, движение, EntityID/Rpl и health проверяются до
отдельного допуска GetIn. `Test-PresetQueuedApproachLog.ps1` требует causal
START→WALK→ENTRY, actual exact occupant, всех четырёх occupants базового probe,
неизменного deadline, native0 и полных stopped logs. Fatal/resource gates
независимы, resource errors не исключаются.

Evidence текущего продолжения: `.codex-runtime/issue78-finalization-20261029/`
`boarding-queue-approach/`, `queued-approach-readonly-review/`,
`backend-prequeue-native-v*/`. Старые setup/observer/compile FAIL сохраняются.
Положительный отдельный probe не заменяет итоговый MOVING2→4→6 или visual UX.

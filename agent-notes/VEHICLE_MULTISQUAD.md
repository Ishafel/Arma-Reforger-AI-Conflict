# Ступенчатая нагрузка транспорта и native combat hold

## Фактический outcome ожидания acquisition

В isolated `AICF_VehicleMultiSquadProbe.c` добавлен выключенный по умолчанию
read-only observer: CLI `aicfVehicleAcquisitionOutcomeProbe=1`. Он оборачивает
исходный public `AICF_VehicleAcquisitionFlow.Update` ровно одним `super.Update`
с теми же аргументами и возвращает тот же `AICF_TripOutcome`, включая `null`.
Исходное исключение не перехватывается. Это источник фактических
`actual_kind`, `actual_reason`, `actual_causation`, `actual_retry_at_ms`, а не
повторная оценка acquisition. `ApplyOutcome` при `WAIT` не сохраняет причину
в transition reason trip: прежний `SPAWN_COMMIT_READY` не доказывает причину
продолжающегося ожидания.

`[AICF][VEHICLE_ACQUISITION_OUTCOME] event=RESULT` содержит отдельно
`BEFORE` и `AFTER` script fields: faction/numeric slot, group/trip generation,
assignment/intent/base revision, operation/phase/transition count, request
generation/counters/deadline/backoff, plan/staging/deadline, reservation
token/generation/expiry и lease identity/state. Это последовательные чтения,
не атомарный снимок входов исходного решения. `trip_deadline_stored_ms` —
только исходное поле; `deadline_scope=RAW_SCRIPT_FIELDS_NOT_EFFECTIVE_CLOCK`
исключает заявление о текущем effective deadline с паузами. Group identity
основана на faction/slot/generation и weak reference, без native EntityId/Rpl.
`stable_identity_unchanged` проверяет непрерывность этих script identities,
не заменяет gameplay `IsCurrent`. Законная смена request/plan/reservation/lease
показывается как `acquisition_context_changed`, а не как чужой trip.
Этот флаг сравнивает ownership references/presence, request generation и
reservation token, а не все значения `BEFORE`/`AFTER`: изменение одного
lease state без смены lease reference само по себе не включает флаг.

Каждый вызов получает отдельный token без domain references. После всех
snapshot/outcome getters проверяется, что token всё ещё принадлежит вызову.
Вложенный Update, включая вызов из getter, и `Stop` с новым запуском делают
внешний token устаревшим: внешний вызов возвращает исходный outcome, но уже
не пишет cache, timestamps или counters поверх преемника. Новый владелец
может сохранить bounded `SCOPE_LOSS cause=PREVIOUS_FRAME_UNFINISHED`; это
также может означать ранее выброшенное исходное исключение, а не доказанный
параллельный gameplay callback. При смене stable identity сохраняется
`SCOPE_LOSS cause=STABLE_IDENTITY_CHANGED` без добавления result в cache.

Общий бюджет —384 records, включая scope-loss receipts; cache содержит
не более32 value-only context keys/signatures/timestamps. Изменение полей
или actual outcome печатается сразу, повтор прежнего состояния — не чаще
10с по исходному `nowMs`. `OVERFLOW observations_incomplete=1` явно означает
неполное наблюдение. `OnGameEnd` очищает cache, flags и текущий token.
Weak domain references существуют только в локальных context objects.
Observer не вызывает `Capture`, `IsCurrent`, native getters, physics/nav
queries, economic decisions или setters; не меняет timers, deadlines,
watchdog/idle exemptions, посадку и движение. Выключенный observer не читает
script snapshots или outcome getters. Это диагностический инструмент,
не исправление taskless failures и не доказательство прохождения MOVING.

До изменения fixture полный frozen V27 набор:262/262 PASS,1021 source hashes
без расхождений. Он не покрывает новый observer. Первое targeted выполнение
после правки:3/3 PASS (Multi fixture31 actual replay cases/10 executable
unsafe variants/68 прежних controls; соседние Background и Protection PASS),
evidence `.codex-runtime/issue78-finalization-20261030/multi-acquisition-outcome-observer/targeted-v1/`.
Финальный targeted `targeted-v3` (2026-10-31,01:52:44–01:53:20 MSK):
Multi fixture **33 actual replay cases/10 executable unsafe variants/43
исходных pure-field getter bindings/68 прежних controls PASS/0**.
Проверены disabled/null/exception, исходные аргументы/result reference,
legitimate mutable acquisition context, stable identity loss, reentry из
обоих snapshot reads и каждого из4 outcome getters, Stop+новый token,
неизменность cache/signatures/timestamps преемника, обе границы объёма,
bounded unfinished-frame metadata и OnGameEnd cleanup. `SpawnCandidate`
только перенесён вместе с modded class: исходный raw method body совпадает
побайтно, обе версии содержат ровно один метод. `git diff --check` PASS.

Промежуточный `targeted-v2` FAIL сохранён: unsafe-вариант убирал отдельное
сравнение trip generation, но добавленный stable context key независимо
содержит ту же generation. Вместо дублирующего изменения проверяется
удаление group weak-reference comparison; этот unsafe-вариант исполняется
и отвергается actual replay. Это исправление теста избыточного mutation,
не исключение gameplay guard или принятие FAIL.

Первый терминальный Workbench mixed stage
`.codex-runtime/issue78-finalization-20261029/multi-outcome-diagnostic-source-v5/`:
**FAIL/native exit-1**, validation false,219 full fatal matches во всех3 полных
логах; raw388/compiled376, source drift0. Первый собственный error —
`AICF_VehicleMultiSquadProbe.c(36): Formula too complex` в длинном boolean
выражении `SameSource`. Процесс и session22560 завершились, CIM01:56:31 MSK
не нашёл native/Workbench процессов. Исходный stage и все логи сохранены.
Оба identity predicate разбиты на эквивалентные короткие ordered guard clauses;
ни одно условие не удалено. Compile FAIL не превращён в PASS результатом
C# replay: потребовался отдельный свежий Workbench этой версии.
После guard-clause исправления `targeted-v4-guard-clauses`: **3/3 PASS/0**,
с прежними33 actual/10 unsafe/43 getter bindings/68 controls Multi и
неизменёнными Background/Protection. Запуск01:56:52–01:57:55 MSK сохранён
отдельно от прежних результатов.

Повторный fresh mixed stage
`.codex-runtime/issue78-finalization-20261029/multi-outcome-diagnostic-source-v6/`
скомпилирован **PASS/native0/validation successful/full fatal0/source drift0**
2026-10-31,02:03:07 MSK. Session93425 closed0; CIM02:03:13.816 MSK:
native/Workbench processes отсутствуют. Состав:384 production files строго
из frozen V27 с SHA-проверкой текущего checkout,3 неизменённые V27 fixtures
Recruit/Census/Mob и новый Multi fixture
`BC72EC5826C5DAA8DA88A7B759FC34D45A97207788C9406FB475819C9D95D361`.
Raw388/compiled376; mixed source manifest
`876AF3D6650165541777EBFFB0ED63A90C26245CEDCE43E69DA8BFDD10CFB49C`.
Точный DryRun/compile command, per-fixture origins, SHA всех3 полных sibling
logs и process closure сохранены рядом со stage. Исходный stage v5 не менялся.
Компиляция подтверждает устранение `Formula too complex`, но не доказывает
фактический WAIT reason в игре: native observer остаётся `NOT RUN`.
Оригинальные native taskless FAIL и все прежние runtime blockers сохранены.

## Native отказ до преждевременного выхода из машины

`AICF_TransitMoveFailure` передаёт точный STUCK/UNREACHABLE/UNKNOWN текущего
vehicle handler ручного preset из `SCR_AIProcessFailedMovementResult` в обычный
Transit Tick. Stock для STUCK/UNREACHABLE немедленно создаёт high-priority GetOut,
а UNKNOWN оставляет debug RUNNING и пишет NodeError: до прежнего project timeout
существующий recovery уже не успевал получить машину с settled экипажем.

Новый scoped handoff требует authoritative current trip/lease/vehicle/route,
native handler именно этой машины, current/executed MoveActivity, исходный
живой AI roster целиком в compartments, отсутствие чужих occupants и живую
подвижность. Смена generation/revision/identity, истёкший deadline, recovery owner
или исчерпанный прежний бюджет сохраняют stock путь. Native OnMoveFailed вызывается
один раз; повторный callback того же receipt не создаёт новую попытку. Callback
не меняет phase, waypoint, progress clocks или physics. Только Transit расходует
прежний максимум2 попытки: STUCK использует прежний bounded поиск короткой позиции
4..15м только при captured и повторной скорости<=1м/с; при большей скорости,
UNKNOWN/UNREACHABLE перестраивается маршрут. Ни budgets, ни absolute deadline
не увеличиваются. Контекст повторно проверяется до recovery, receipt снимается
при Reset и ExitTransit/AbortOwnedActions, включая переход в пеший бой.

Node возвращает штатный FAIL для повторной оценки дерева, не удерживает старую
failed-movement ветвь через RUNNING. Короткий node-local dedup допускает только
тот же FAILED/removing action, native handler/result/точку при доказанной смене
route generation и superseded/снятом старом waypoint. Срок5с отсчитывается от
исходного capture и не обновляется. OnEnter снимает node-local record; новый
RUNNING action с теми же координатами не считается старым отказом. Lease identity
сверяется с `RplComponent.Id()`, как при её создании: `Replication.FindItemId`
может указывать другой native item той же машины и не является её component ID.

`tests/fixtures/AICF_TransitFailureRecoveryProbe.c` — отдельный controlled scaffold
на настоящих native seated actors/vehicle handler после начальных25м движения.
Он проверяет отказ чужого,
default и unrelated handler, STOPPED, один native callback при повторной доставке,
два counted repairs, настоящее движение>=25м/3velocity polls после каждого и отказ
третьей recovery при прежнем deadline. Это `CONTROLLED_FAILURE_RECOVERY_ONLY`,
не естественное воспроизведение STUCK и не замена полного MOVING2→4→6. Анализатор
`Test-TransitFailureRecoveryLog.ps1` читает полный stopped log; strict resource
вердикт хранится отдельно и не становится PASS из targeted функционального PASS.
Актуальные результаты требуют свежего runtime evidence; прежние Multi FAIL
не переоцениваются новым contract.

Опциональный `-RecruitmentNavDomainProbe` добавляет только isolated read-only
наблюдение за подходом к казарме: exact order/group/actor/action/waypoint,
повторный полный native path до512points, actor/group navmesh/tile/projection.
Для failed/stalled подхода ограниченные surrogate-body/ray queries сравнивают
CharacterAI и Projectile; читаются velocity/stance/LOD, публикуется hit identity.
Ни query, ни наличие пути не гарантируют фактический проход. Все данные повторно
сверяются до публикации; super.CheckApproach вызывается ровно один раз.

Короткий5min diagnostic case не заменяет30min MOVING2→4→6 gate. Initial spawn,
current actor position и первый path node могут различаться: физическое препятствие
на node нельзя автоматически приписывать прежнему SpawnPlacement/Recovery.

Это продолжение issue78. Пользователь выбрал сначала проверить2→4→6 отрядов
на каждую сторону. Обычные defaults INFANTRY и cap10 машин/faction не меняются.
Оба новых runner работают только с отдельным immutable snapshot, canonical
`Start-AICFRuntime.ps1` и терминальным Workbench Validate. Fixtures в production
не устанавливаются. Offline contracts не запускают игру.

## Два режима нагрузки

`-ProtectionScope ROOT_ONLY` сохраняет историческую CAPACITY защиту только root
damage manager. `-ProtectionScope HIERARCHY` — отдельный opt-in CAPACITY control:
все native hitzones перечисляются дважды, проверяются exact managers/owners и
readback disabled у всех managers. `VEHICLE_MULTI_PROTECTION` привязывает receipt
к original actor/vehicle; strict log oracle требует полное и свежее покрытие.
NORMAL+HIERARCHY отклоняется до запуска. Ни physics, ни actions, ни production
health gates не меняются; protected capacity не доказывает обычную войну.

```powershell
& tools/Invoke-AICFVehicleMultiSquad.ps1 `
  -EvidenceRoot .codex-runtime/multi-normal-<unique> -Mode NORMAL `
  -DurationSeconds 1800 -ServerPort 2520 -SnapshotWorkingTree

& tools/Invoke-AICFVehicleMultiSquad.ps1 `
  -EvidenceRoot .codex-runtime/multi-capacity-<unique> -Mode CAPACITY `
  -DurationSeconds 1800 -ServerPort 2521 -SnapshotWorkingTree

& tools/Watch-AICFVehicleSoak.ps1 -EvidenceRoot .codex-runtime/multi-capacity-<unique>
```

`NORMAL` сохраняет обычные отношения, урон и экономику. `CAPACITY` — отдельная
контролируемая проверка: truce до spawn, HOLD_FIRE, защита выбранных бойцов/машин
и конечный дополнительный бюджет4000/faction. Test-only метод EconomySystem
пополняет только дефицит относительно настоящих quotes, проверяет operational
owned HQ и записывает before/requested/applied/after/token/remaining budget.
Штатный платный найм и покупка техники сохраняются. Такой PASS не доказывает
обычный бой или достаточность обычного снабжения; classification
`CONTROLLED_CAPACITY_ONLY` явно отделена от `NORMAL_CAMPAIGN`.

По умолчанию `-ConcurrencyMode MOVING` требует движение всех машин. Дополнительный
`-Mode CAPACITY -ConcurrencyMode MOUNTED_STAGING -DurationSeconds 900` отдельно
проверяет реальную посадку2→4→6 на сторону и30s штатного ожидания первой цели:
route generation0, waypoint отсутствует, `IsAwaitingFirstPresetDestination`.
До подтверждения6 дальняя цель не выдаётся; затем всем задаётся обычный конечный
маршрут/высадка/foot oracle. Результат явно `CAPACITY_MOUNTED_ONLY`,
`notmovingproof=true`; он не подтверждает6 машин в движении и не заменяет
получасовые MOVING наблюдения. Чужие seats/sites, создание машин и clearance
не обходятся. Нехватка6 реальных площадок остаётся FAIL, не виртуальным счётчиком.

CAPACITY JIT funding в fixture выполняется также перед PRESPAWN preflight и
PURCHASE: фоновая служба может потратить запас между десятисекундными polls.
Economy-owned метод проверяет исходные selected0..5/group/gen/paid5/preset,
актуальную цену и собственный HQ, current site plan/reservation/request generation.
Lease до spawn может быть reserved без physical entity; имеющаяся lease проверяется.
Пополняется только действительный дефицит с учётом logistics obligations через
тот же ledger/общий budget4000; оба hooks вызывают ровно один native super,
результат native debit не подменяется. NORMAL не получает JIT grants.

Используются настоящие managed numeric slots0..5 обеих сторон, исходный spawn
по одному бойцу. Будущие ступени остаются INFANTRY/desired1. На текущей ступени
поднимается desired5, запрашивается `RequestPlayerRecruitment`, фиксируются
четыре native debit/join и исходные пять actor identities. Каждый отряд после
собственного `RECRUITED` сразу может запросить настоящую UAZ469 или M1025/M2HB;
его машина не ждёт найма других отрядов или противоположной стороны.
`MUSTERED` остаётся aggregate marker, подтверждающим платный найм всех групп
текущей ступени, и не разрешает выдачу техники. `ACTIVATED/MUSTERED` содержат
cohort2/4/6. Будущая ступень активируется только после физических машин предыдущей
ступени со всеми exact occupants и непрерывного stable30s.

Политика `muster_policy=PER_GROUP_BEFORE_VEHICLE` обязательна в fixture events,
source manifest, timing и summary. Auditor отдельно связывает собственные
`RECRUITED`, пять исходных MEMBER identities и четыре native paid joins с выдачей
машины тому же отряду; общий marker не заменяет эти доказательства. Funding для
техники учитывает собственный paid5 в phase1/2 без ожидания aggregate marker,
а JIT повторяет прежние identity/paid5/site/transaction gates. Reused stage со
старой политикой отклоняется. Прежние M1/M2 проверяются сохранёнными matching
auditors: новый порядок не пересматривает их FAIL и не подавляет `MOB_EGRESS`.

Каждая машина сразу получает дальнюю runtime road/navmesh цель. Пока набирается
следующая ступень, rolling retarget до сближения400m удерживает поездку в TRANSIT.
Каждый sample перечисляет все12 групп с group/gen, operation, lease/entity/Rpl,
пятью исходными бойцами, displacement>=25m и velocity poll. Aggregate heartbeat
сам по себе не доказывает concurrency. Требуются2, затем4, затем6 разных физических
машин на сторону непрерывно30s. Все строки sample используют один настоящий
captured decision timestamp; auditor связывает его с t_ms и elapsed_ms.
После последней ступени задаются конечные цели: доезд, подтверждённая высадка
и все пять пешком≤35m при неизменном PLAYER/POSITION intent/revision.
Для manual preset текущий endpoint — retained `TACTICAL_DEPLOYMENT`: тот же
оплаченный lease остаётся у отряда, `Handoff.IsOrderRestored()` подтверждает
пеший приказ. Это не terminal `COMPLETE`. Fixture отдельно сверяет native
watchdog clearance, прежние пять actor IDs и свежие compartment/transition/hull
readbacks каждого. Только после полного clear публикуются пять
`VEHICLE_MULTI_RETAINED_MEMBER`, `VEHICLE_MULTI_RETAINED_CLEARANCE` и
`DISMOUNTED normal_complete=0 retained_manual=1`; физическое прибытие всех пяти
к конечной точке остаётся отдельным `FOOT_ARRIVAL`.

Matching auditor связывает retained endpoint с реальным production переходом
`TRANSIT → TACTICAL_DEPLOYMENT` с `DESTINATION_REACHED` либо `LOCAL_THREAT`, началом deployment
и `INFANTRY_FALLBACK order_restored=1 alive=5`. Он отклоняет отсутствующий clear,
заменённого actor, занятую compartment/transition/hull, смену lease generation,
несовпадающие причины перехода/deployment и ложный terminal `COMPLETE`.
`LOCAL_THREAT` допускается как endpoint только после настоящего прибытия всех
пяти пешком≤35m к финальной точке. Перед `FOOT_ARRIVAL` retained ветвь повторяет
свежий physical clear: ранее вышедший, но уже вернувшийся в машину отряд не
считается пешим. Старые logs с настоящим
`HANDOFF → COMPLETE` читаются прежней отдельной ветвью с обязательным
`DISEMBARK_COMPLETE`; их результаты задним числом не меняются. Эти изменения
endpoint не расширяют MOVING oracle: exact paid5, исходные машины и непрерывные
30s для2/4/6 на сторону по-прежнему обязательны. Временный пеший бой при том же
trip/lease, исходных пяти бойцах и прежнем intent не объявляется завершением
поездки: fixture ждёт возврата в `TRANSIT`. За время такой паузы active count
равен нулю, непрерывное stable30s окно сбрасывается; пауза не даёт concurrency credit.
Displacement/moving polls накапливаются за всю поездку, а дистанция к конечной
цели проверяется отдельно.

Замена roster/машины/trip, fallback и потери остаются FAIL. Наблюдение продолжается
до1800s даже после функционального отказа. Setup имеет отдельный предел180s;
нативный STARTING допускается до первого RUNNING, остановка матча раньше срока
даёт ABORTED. Проверки duration/liveness/provenance/environment/funding/recruitment/
cohorts/functional/native fatal/native resources независимы. Resource FAIL не
превращается в PASS из-за известного baseline.

`-PrepareOnly` у Multi runner готовит и компилирует snapshot; `-StageRoot`
с `-SkipCompile` требует исходный manifest, совпадающий набор hashes и успешный
compile. Активный stage менять нельзя. Evidence включает exact CLI manifest,
native exit, UTC wall timing, полные stopped sibling logs и source hashes.

## Контролируемая остановка и возобновление

```powershell
& tools/Invoke-AICFTransportCombatResume.ps1 `
  -EvidenceRoot .codex-runtime/combat-resume-<unique> -ServerPort 2528
```

Это отдельный fault injection с изолированным пятичленным roster в UAZ,
truce/HOLD_FIRE/защитой. Не обычная война и не проверка player RPC. После
настоящего начала движения test-only `CustomEvaluate` удерживает приоритет
точного уже существующего native VehicleCombatActivity этой группы. Native
utility сама выбирает его; release лишь снимает test gate. Запрещены подмена
current/executed actions, Add/Fail/Complete action, teleport и запись в production
movement/deadline clocks. Gate очищается на любом terminal/Stop пути.

Проверяется непрерывный hold≥90s, реальная неподвижность≥60s, неизменность clocks
и absolute deadline, natural deselection≤10s, возобновление≤150s, новый displacement
после release≥100m/5velocity polls с exact seats/native route, normal completion
и все пять у той же цели≤35m. Identity fence держит assignment/group/lease/entity/
Rpl/generations/actors/target/revision; route generation может измениться лишь
через обычный lifecycle. Аудитор связывает поля с настоящими `t_ms` и порядком
production START/END/DISEMBARK/COMPLETE, а не доверяет заявленному hold_ms.
`-PrepareOnly` этого runner только сохраняет snapshot, без compile/native.

Актуальные результаты и ограничения — [TESTING.md](TESTING.md). Полные
технические отчёты сохраняются локально по решению пользователя; GitHub не меняется.

## Свой экипаж и диагностика посадки

Opt-in `-BoardingNavDomainProbe` добавляет read-only paired group/actor snapshots
до/после spawn и на cadence5s boarding. Сравниваются exact pathfinding/navmesh,
tile loaded/valid/requested и projections возле actor/site/реальных entry doors.
В BOARDING повторяются lease pointer/generations/entity/Rpl; PRE/POST остаются
в candidate scope до bind. Сам observer не запрашивает tiles и не меняет actions,
deadlines или допуски посадки. Разные pathfinding components не доказывают разные
nav worlds; переход Pilot на AICarMovementComponent ожидаемо меняет domain.

`AICF_VehicleCrewRoleOwnership` различает чужого occupant и своего сознательного
authoritative AI в точном назначенном Pilot/Turret месте актуального manual preset.
Совпадения фракции, group или просто живого occupant недостаточно. Проверяются
assignment/intent/preset/spawn/lease/entity/Rpl и actual compartment. Для такого
своего бойца StartCrewRecovery сначала использует прежний bounded settled-loss
observer3polls/3000ms, затем обычный recheck. Passive wait не расходует recovery
attempt и не меняет reservations/actions/progress clocks. Текущий crew token
сохраняется при переходе Pilot; owner/FAILED/deadline проверяются раньше WAIT.
Unknown/human/foreign остаются fail-closed. Это устраняет ложное FOREIGN в
переходе; настоящий native STUCK или невозможность посадки этим не исправляются.

Перед stalled yield и boarding deadline read-only события
`VEHICLE_PRESET_ENTRY_SNAPSHOT/ENTRY_DOOR/ENTRY_TRACE_REJECTED` сохраняют exact
token/actor/lease/vehicle/seat, native action, readiness veto и до16 дверей:
available/access/can-get-in, entrypoint world position и расстояние. Snapshot
снимается до отмены action. `native_bt_failure_cause=UNDETERMINED` отделяет эти
наблюдения от недоказанной причины engine path/door отказа. Deadline125s и два
повтора сохранены. Контракты: VehicleCrewOwnTransition и VehiclePresetEntryTrace.

`AICF_VehiclePresetNativeSeatPolicy` принимает штатный GetIn message для exact
actor/current manual preset и передаёт native actor reaction локальную копию
с назначенным compartment. Native super создаёт и исполняет behavior; уже
существующий exact owned GetIn и settled actor не заменяются. Связи с trip/group/
lease/vehicle/actors слабые, регистрируются после ValidateContext в Transit Tick
и удаляются при AbortOwnedActions, включая normal exit/fallback/Stop. Нет timers.
Lookup/dispatch повторно проверяют immutable identities, current assignment,
preset, sender/receiver и current selected owned GetInActivity либо Move с current
route waypoint. Исходный sender проверен до clone: native actor overload его
не читает, clone не rebroadcast-ится. Original message не меняется.
Резервируется только exact безопасное пустое место для своего AI; созданная
бронь откатывается owner-safe при отсутствии native action, дальше ей владеет
обычный native action cleanup. Human/foreign/ambiguous/stale requests отклоняются
до generic fallback. Другой настоящий native activity сохраняет обычное поведение
только при explicit related-current proof. BaseContainerProps reaction обязателен;
UnknownClass GetIn/GetOut — fatal, не resource whitelist.

`NATIVE_VEHICLE_PATH_TRACE/POINT` сохраняют raw path exact водителя через
`AIBaseMovementComponent.GetCurrentPath`, raw component type/associated-agent/car-cast,
current trip/lease/route/driver identities, raw native index и до128 точек с
count/truncated/first/last. Source явно `DRIVER_MOVEMENT_COMPONENT`: эти данные
не доказывают автомобильный путь или remaining distance. Owner==agent и обязательный
AICar cast не предполагаются; тот же getter/component и контекст проверяются
повторно после чтения. Unknown/unavailable/empty/discarded разделены. Watchdog
и progress clocks от этой диагностики не меняются.

`VEHICLE_PRESET_ENTRY_PATH/PATH_DOOR/PATH_POINT` перед yield/deadline дополняют
данные foot path: два чтения current path/HasCompletedRequest(false/true),
associated agent/pathfinding, projection entrypoint на foot navmesh (half extents4m),
расстояние последней native точки до входа. Максимум64 точки/16 дверей. Изменившийся
снимок отбрасывается по повторным token/actor/seat/lease/slot/intent/action fences.
Порядок path points и его last point не объявляются доказательством достижимости.

## Состояние машины и первичный отказ

`AICF_VehicleHealthTrace.Capture` вызывается в обычном GroupTrace и перед
`VEHICLE_FAILURE_SNAPSHOT`. Это read-only snapshot с повторным entity/component
Rpl/damage manager/default hitzone/physics fence. Он сохраняет native CanMove,
movement damage и fire отдельно, engine/gearbox functional/efficiency, wheels
efficiency, default hitzone health, root damage handling, до64 hierarchy zones,
damaged/destroyed/burning counts, enabled child zones и худшую observed hitzone.
Pose/velocity/angular velocity дополнены terrain Y и origin-surface delta:
абсолютная высота сама по себе не доказывает полёт. Неполный hierarchy snapshot
явно сообщает truncation; поля не участвуют в gameplay решениях.

CombatResume fixture пишет `TCR_HEALTH` каждые5s и добавляет leaf snapshot прямо
в `TCR_REJECT` до остановки. `category=HEALTH|IDENTITY|LIFECYCLE` и check
`WITNESS_HEALTH` различают общий health veto и смену identity. `TCR_CLEANUP_BEGIN`
фиксирует границу teardown. Auditor отдельно сохраняет первичный отказ;
последующий COORDINATOR_STOP не выдаётся за независимый mobility defect и не
превращает полный FAIL в PASS. Успешный сценарий по-прежнему требует прежних
hold/resume/arrival/dismount/foot proofs без fallback и mobility recovery.

`damage_protection_scope=ROOT_ONLY` уточняет прежний fixture guard: он выключает
damage handling только root manager. Дочерние hitzone managers и отдельные
movement modifiers могут оставаться активны. Даже совпадение их повреждения с
`VehicleCanMove=false` ещё не устанавливает collision/combat instigator;
`health_cause_proven=0` сохраняется. Штатные health gates не ослабляются.

Подход к казарме после STUCK теперь выбирает детерминированные projected точки
внутри зоны найма; прежний ненаправленный GetReachablePoint не решал поиск возле
service. Два repair attempts и physical arrival сохраняются. Геометрический
endpoint не объявляется доказанно достижимым: подробности и ограничения —
[RECRUITMENT_SUPPLY_PLANNING.md](RECRUITMENT_SUPPLY_PLANNING.md).

## Учёт repair при обновлении point hold

В stopped `mounted-fixed-v2` attempt18 (`US/S3`, generation1) завершил исходный
point waypoint callback, после чего `POINT_HOLD_REFRESH` заменил его до конца
durability window. `ReplacePointOrder` очищал pending candidate раньше terminal
receipt; последующий `ORDER_RECOVERED` использовал старую локальную переменную,
а строгий accounting обнаруживал один неучтённый attempt. Это не было потерей
receipt при vehicle suspension: в этот момент trip оставался `WAITING_FOR_SITE`.

`ProcessPendingOrderRecovery` теперь перед replacement отдельно проверяет
точный pending attempt/group/waypoint/generation/assignment/intent, stock callback
и живого leader в радиусе point destination. Он записывает `SUPERSEDED` с reason
`POINT_HOLD_REFRESH` и basis `COMPLETED_AT_POINT_REPLACEMENT`, очищает candidate,
обновляет hold и возвращается без `ORDER_RECOVERED` для нового waypoint.
Отклонённая проверка не обходит fences через общую completion ветку. Обычное
подтверждение после durability window и accounting invariant сохранены.

Контракт `Test-PointHoldRepairAccountingContracts.ps1` проверяет порядок receipt
до replacement, отсутствие двойного terminal/ложного success, identity/physical
fences и неизменность строгого accounting. Evidence до/после и полный stopped
causal witness: `.codex-runtime/vehicle-repair-20261027/review/m2-accounting/`.
Новый native повтор этого случая до его фактического запуска остаётся **NOT RUN**.

Первый compile нового accounting preflight обнаружил `Formula too complex`;
длинные выражения разбиты на короткие guards с теми же fences. Этот compile FAIL
сохранён, последующий Stock compile прошёл. Контракт теперь дополнительно
ограничивает сложность выражений и требует использования каждой proof-группы:
31 controls, соседние 7 checks PASS. Это не подменяет новый runtime результата.

## Native waypoint и route progress

`AICF_VehicleNativeRouteProgress` допускает transient server-only waypoint без
`RplComponent`. Его ownership по-прежнему требует точного trip-owned pointer и
`EntityID`, route generation, текущего waypoint в очереди группы и соответствия
`SCR_AIMoveActivity.m_RelatedWaypoint`. Если `RplComponent` существует, он обязан
быть master, не proxy; component pointer/Id и waypoint identity проверяются
повторно. Group/vehicle/driver replication fences сохранены.

`native_waypoint_has_rpl=-1|0|1` означает соответственно ещё не проверен,
отсутствует или присутствует. Это диагностическое значение не входит в
persistent scope истории пути; обычный replan не сбрасывает replay frontier.
Optional component identity входит только в route identity, поэтому изменение
маршрута требует нового baseline и само не считается progress.

N6 telemetry зафиксировала `CURRENT_WAYPOINT_AUTHORITY` между двумя exact
current-WP/native-Move readbacks. Ранние логи не различали отсутствие component
и отказ его authority; новый прогон должен показать `native_waypoint_has_rpl`
и реальный credit. До stopped-log проверки это **NOT RUN**; прежний N4 FAIL
и выявленный N6 veto не становятся PASS. Source baseline/after — 4/4 checks PASS, route contract
59 controls, 7 step cases, 8 geometry cases и 3 replay cases; evidence:
`.codex-runtime/vehicle-repair-20261027/review/native-wp-rpl/`.

## Native OpenGate у пассажира

В N7 cargo actor получал `PerformAction` с priority54 и связанный `GetOut`
с priority154 под текущим `SCR_AIMoveActivity`; это согласуется со stock
`SCR_AIGoalReaction_OpenNavlinkDoor`. Старый observer проверял только PILOT/TURRET.
Фактические smart component/tag в N7 не записывались, поэтому attribution
`OpenGate` остаётся strongly consistent, а не подтверждённым runtime payload.
Полный N7 завершился FAIL после `NO_PHYSICAL_MOVEMENT`, сохранив 67 настоящих
native route credits и неизменные clocks в течение 90.505s combat hold.

Observer теперь рассматривает все назначенные preset seats и принимает только
один доказанный live OpenGate perform текущего Move/waypoint, один exact return
и не более одного exact exit. Неоднозначные цепочки отклоняются; bounded reject
telemetry пишет реальные tags и число candidates не чаще одного раза в10s.
Неизвестный GetOut не получает protected wait. Первичный physical exit требует
реального отсутствия character/access/linked vehicle; начатый и отменённый GetOut
не считается выходом. Возврат требует completed native return, exact settled seat,
затем настоящего движения машины после возврата.

Вся цепочка, включая resume, ограничена тем же первым60s deadline и absolute trip
deadline. Value-only receipts по smart owner и route/assignment/lease identity
делят срок между бойцами; recapture или смена actor не обновляют начало. История
ограничена32 записями без eviction и очищается только при movement Reset.
Observer не пишет native actions, reservations или progress clocks.

Baseline соседних checks — 7/7 PASS; итоговый набор — 8/8 PASS, dedicated
`Test-TransportNativeGateContracts.ps1` — 46 controls, включая исполняемые
two-chain, отменённый GetOut, fixed deadline и recapture cases. Evidence:
`.codex-runtime/vehicle-repair-20261027/review/native-cargo-gate/`.
N8 не создал ни одного OpenGate candidate: эта ветка **NOT EXERCISED**. Его
полный FAIL вызван `VEHICLE_OVERTURNED` при roll−110.155°, а не новым observer.
Подтверждение изменённой ветки новым native прогоном остаётся **NOT RUN** до
фактического observed payload и stopped-log проверки.

M6 затем реально исполнил cargo OpenGate: боец вышел, вернулся в exact seat,
а та же машина проехала156m. Старый poll всё равно дождался60s и дал ложный
fallback: stock `OnActionDeselected` уже сменил transient `COMPLETED` return action
на `EVALUATED`. Этот M6 FAIL сохранён отдельно и не превращается в PASS.

Положительный return receipt теперь приходит только от `m_OnActionCompleted`
точного captured native return action. Callback проверяет stopped/attached,
trip/group/member/seat/vehicle/route/assignment identity и текущего gate owner.
Receipt переживает `COMPLETED → EVALUATED`; отмена, другой action или stale callback
его не создают. Actual outside, exact settled return, реальное движение после
возврата и прежний первый60s deadline сохраняются.

`Stop` снимает listener и разрывает ссылки observer→action→callback; замена gate,
movement Reset и Abort проходят через `SetNativeGate` со снятием старого observer.
Delta baseline — 8/8 PASS; after — 9/9 PASS, новый completion contract —
23 controls с исполнением actual callback/lifecycle/invalid Stop bodies. Evidence:
`.codex-runtime/vehicle-repair-20261027/review/native-gate-completion/`.
Полный127 набор на N9 относится к предыдущему source, новый callback требует
отдельного native подтверждения и compile результата своей версии.

Даже invalid trip должен снять local listener: `AbortForStop` после проверки
nonnull trip вызывает local movement `SetNativeGate(null)` до раннего
`!trip.IsValid()` return. Эта ветка не получает native action effects; null trip
не разыменовывается. Regression проверяет уничтоженный context, удаление listener
и отклонение позднего completion, а также null trip и отсутствующий movement state.

## Сохранение native vehicle subgroup после пешего боя

`SCR_AIGroupVehicle` создаётся с unassigned handle `-1`. Штатный
`FindVehicleBySubgroupId(-1)` возвращает первый такой record, поэтому совпадение
entity при отрицательном handler не доказывает принадлежность failed movement.
Transit receipt отклоняет отрицательные и default handlers и дополнительно
сопоставляет positive record handle с actual Pilot handler и его
`AICarMovementComponent`, управляющим exact leased vehicle.

При `TACTICAL_DEPLOYMENT → TRANSIT` Controller сохраняет native vehicle utility:
бойцы уже физически сидят, а удаление и повторное добавление record теряло handle
после последнего `CompartmentEntered`. Выход в terminal phase по-прежнему снимает
utility. Для COMBAT entry, где экипаж остаётся внутри, Handoff после регистрации
record восстанавливает только native bookkeeping через штатный
`OnJoinGroupFromVehicle`. Каждый вызов ограждён повторной проверкой exact current
trip/slot/assignment/intent/generations/component Rpl, authoritative original AI,
settled seat, actual Pilot/car и отсутствия Gate/Crew/entry owner или активного
native GetIn/GetOut/PerformAction. Отсутствующий боеспособный Pilot ничего не
переназначает: решение остаётся у BehaviorFlow после боя. Физические позиции,
health, actions, waypoint, budgets и clocks этот boundary не изменяет.
Создание нового native handler допустимо только при отрицательном unassigned
record. Положительный record должен уже совпадать с actual Pilot handler;
default или положительный mismatch отклоняются до stock callback. Девять
исполняемых случаев проверяют actual extracted pure predicate, включая такие
stale/default значения; отдельно проверяются18 source mutation controls.

Fresh controlled proof 29.10.2026: `subgroup-native-handlers-US-v1` — compile0,
native0,29 native checks, frozen recovery audit PASS; actual movement после двух
repairs и отрицательные `-1/-2` подтверждены. `subgroup-combat-USSR-v1` — compile0,
native0, frozen targeted audit0/source drift0. В TACTICAL реально наблюдались
record `-1`/Pilot handler1 → record2/Pilot2, joined2; после return record2 сохранён.
Строгие resource errors в обоих случаях остаются FAIL; controlled checks не
заменяют полный MOVING gate. Evidence:
`.codex-runtime/issue78-finalization-20261029/movement/`.

Предшествующий `multi-moving-v5` остановлен адресно после первого необратимого
fallback USSR0; native exit `-1`, duration NOT_PASS. Его полные ошибки сохранены.
Это не30-minute PASS: cohort4/6 не доказаны. Новый полный прогон должен использовать
исправленный subgroup boundary и прежний oracle.

## Controlled funding перед фактическим preflight

В frozen r30 MOVING1800 сохранён FAIL:05:11:50.779 US имеет stock485 при
quote475; последующий recruit расходует20, а исходный economic preflight
05:11:53.794 честно отказывает при stock465/obligations0. `SpawnCandidate`
не вызывается после этого отказа, поэтому прежний JIT в его override был
слишком поздним. Это дефект подготовки controlled CAPACITY, а не основание
менять production отказ при недостатке средств. Полный r30 run завершился
штатно native0/audit1, elapsed1800109ms, cohort2/completed0; старые FAIL
(также destroyed/abandoned vehicle), source391 и полные логи сохранены в
`.codex-runtime/issue78-finalization-20261030/multi-r30-moving-1800-v1/`.

Новая версия fixture переносит этот JIT перед единственным исходным
`InspectVehiclePresetPreflight(trip, faction, base, fleet)`; возвращает тот же
фактический decision object/exception. `SpawnCandidate` больше не override,
конструктор и compartment fences не менялись; JIT перед исходным purchase
сохранён. Только CAPACITY, текущая cohort, те же5 original actors, текущие
trip/request/plan/reservation/preset generations и exact fleet; допустимы
только отсутствие lease либо собственная неиспользованная RESERVED lease.
Script context и preset повторно проверяются после metadata reads и перед
native grant. Вложенная preflight делает старый quote-frame устаревшим.

Helper quote помечен `BEFORE_METADATA_SNAPSHOT`: он readonly, но не pure —
конструктор catalog читает native metadata/Resource.Load. Это дополнительный
metadata read, не повтор original decision; preview `Open`, actor factory,
Census и health/pose setters здесь не вызываются. Original quote остаётся
авторитетным: смена цены между helper и original по-прежнему может отказать.
Прежние per-faction4000, native capacity/readback и единственный AddSupplies
сохранены; invocation token отличает новый реальный preflight от replay того
же grant. Отрицательный decision не подменяется, request/retry/deadlines,
MOVING/ROOT_ONLY/PRODUCTION_BACKGROUND и игровые составы не меняются.

Targeted baseline до правки: MultiFixture+PresetEconomyDecline2/2 PASS,
`multi-preflight-funding-race/baseline-v2`. Первый CLI с объединённой строкой
имён был отклонён до запуска тестов. Новый composed replay исполняет actual
core preflight/flow и fixture grant/ownership bodies; adapter-only initial
FAIL сохранены в `multi-preflight-funding-race/`, включая defaults/out/types.
Финальный targeted replay в PowerShell7 и Windows PowerShell5.1: PASS23 actual
composed cases /7 executed unsafe; прежние68 fixture controls и outcome observer
33 actual /10 unsafe /43 bindings также PASS. Проверены старый485→465 отказ,
JIT до единственного original preflight, неизменный result/exception, scope loss,
physical/foreign lease, предел4000, неполный native grant и изменение actual price
после helper. Полные результаты и exit receipts: `final3-pwsh.txt`,
`final3-winps51.txt`, `final3-dual-exit.json` в том же evidence-каталоге.
Новые Workbench/native gates этой версии пока **NOT RUN**; старый r30 run
не является проверкой новой правки.

Повторный `multi-moving-v6` использовал narrow positive-handler guard и final
Admission. Cohort2 реально держался31.618s; при наборе cohort4 группа USSR3
получила `NATIVE_GATE_IDENTITY_OR_DEADLINE` на повторном входе в тот же native
OpenGate после возврата. Все исходные бойцы сохранились, однако bounded60s owner
истёк. Cohort4/6 недостижимы в этом прогоне. После полного snapshot exact PID13980
остановлен: native-1/audit1, wall934.997s, duration NOT_PASS, native_fatal gate PASS.
Причина сохранена для отдельного bounded gate recovery; этот результат не
подтверждает30минут и не разрешает ослабить исходный oracle.

## Actual staging actor task

В отдельном ordinary-USSR soak обнаружен настоящий diagnostic error
`MEANINGFUL_TASK_DEADLINE_MISSED`: после единственного reissue stock снял All-members
staging waypoint, но часть exact owned actor actions продолжала `RUNNING` подход.
`AICF_VehicleStagingTaskProof` учитывает эту реальную выполняемую работу только для
current manual rally. Token захватывает immutable assignment snapshot при issue;
проверяются original AI/entity/Rpl, slot/group/trip/request generations, assignment/
intent/base revisions, reservation и исходные deadlines, captured/native goal,
utility ownership и actual selected/executed `RUNNING` action. Phase exemption,
продление сроков, изменение task clocks или физические эффекты отсутствуют.

Fresh compile-v4/native-v3 29.10.2026: native0/full stopped, заранее frozen auditor
PASS15 native checks; source23 и log oracle13 mutation controls PASS. Реально снят
групповой waypoint; stale/foreign/unowned/suspended/cancelled/failed/noactivity
отклонены. После controlled `Fail()` обычный acquisition owner восстановил задачу
и собрал original5 за9038ms, боец естественно прошёл5.03946m, deadlines/revisions/
health не изменились. Source/fixture bytes staged/current6/6. Строгие14 resource E
остаются FAIL/overall FAIL; ordinary30min этим controlled proof не заменён.
Evidence `movement/staging-task-native-v3/` под тем же finalization root.

### Истёкшая native OpenGate цепочка

`multi-moving-v6` обнаружил повторный OpenGate после настоящего native return:
бойцу снова назначалась высадка у тех же ворот, машина не продолжала движение,
а gate observer до60s блокировал обычное mobility recovery. По истечении окна
код сразу переходил в foot fallback, хотя два прежних recovery attempts не
использовались. Это отдельный FAIL, он не переоценивается в PASS.

`CanRecoverExpiredReturn` разрешает только истёкшую exact current цепочку с
положительным native completion/physical return history, исходным полным живым
AI roster, текущей lease/preset/route identity, исправной медленной машиной и
бойцами не дальше25м от своих мест. Transit flow снимает listener через прежний
Stop boundary и расходует один прежний mobility attempt на `ROUTE_REBUILD_ONLY`.
Физического переноса из gate observer нет; окна, trip deadline и clocks не
продлеваются, progress не выдумывается. Остальные случаи сохраняют прежний
fallback, исчерпанный budget даёт terminal recovery failure.

`Test-NativeGateRecoveryContracts.ps1`:34 source/extracted controls PASS;
исполняются реальные recovery predicate и dispatch, включая null/stale roster,
неполную native history,3D boundary25м, old budget и исходный successful path.
Native `GateHQ-v5`: Workbench0/native0/frozen targeted audit0,10 checks PASS,
полный fatal0. Реальный OpenGate/return предшествовал единственному controlled
expiry; route1→2, budget0→1/2, trip deadline1188092 неизменён. Затем actual35.7453м/
7moving polls/8.27m/s, samecar/ComponentRpl/original5/одна покупка. Закрытие/readback
связаны с actual captured gate; следующая native цепочка завершилась штатно.
Это fault injection, не доказательство естественного истечения60s; новые
MOVING2→4→6 ещё PENDING. Строгие28 raw sibling resource errors остаются FAIL.
Ранние US/USSR coverage FAIL без ворот и v4 diagnostic binding/oracle FAIL
сохранены отдельно. Evidence `gate-expiry/native-gatehq-v5/` под finalization root;
точные пути/commands/source hashes находятся в отчёте владельца fixture.

### Покрытие короткого terrain recovery

`multi-moving-v8` прошёл MOVING2+2 в течение31.931s, затем US1 исчерпал два
исходных mobility attempts: старые16 search centers давали только две свободные
terrain точки, обе возле постороннего живого бойца. Native GetOut завершился
`NATIVE_GROUP_ABANDONED_VEHICLE`. Это сохранённый FAIL;4/6 не доказаны.

Терминальный geometry repro на той же позе и модели подтвердил пропущенные
безопасные точки. Поиск теперь ограничен32 направлениями и четырьмя rings
8/13/6/10.5м; исходные восемь направлений идут первыми внутри каждого ring.
Новые направления первого ring идут раньше старого второго ring. Прежние
пределы4..15м, hazard clearance6м, identity/player/occupancy fences и два attempts
сохранены. Живые чужие бойцы и pressure triggers по-прежнему запрещают candidate.

`Test-UnstuckSearchCoverageContracts.ps1` исполняет извлечённый production builder:
55 checks, включая порядок, уникальность, единичные направления, поворот и
ограниченное покрытие. Native controlled sampler-v3: WB0/native0/fatal0,8 checks
PASS; старые точки отклонены из-за чужого бойца, actual production sampler выбрал
новую точку с3D correction8.00069м за2ms. Затем native Pilot проехал17.374м,
здоровье original5 и машины не изменилось. Это локальная проверка sampler;
Полный Multi-v9 затем завершён как addressed ABORT/FAIL: после MOVING2+2 в течение
30.876s появились initial boarding deadline USSR3, driven healthy overturn US3
и staging exhaustion USSR2. MOVING4/6 не доказаны; каждый дефект требует своего
исправления и новой проверки. Dense sampler при US3 overturn не вызывался,
mobility attempts были0. Raw sibling resource errors сохранены отдельно как FAIL.
Диагностические координаты живут
только в ignored evidence fixtures; подробные команды, frozen SHA и прежние
fixture readiness FAIL находятся в `movement/REPORT.md` finalization evidence.

### Неподвижный manual staging actor после исчерпания двух native actions

Multi-v9 выявил узкий дефект: exact owned actor оставался в36.0178м от
rally25м после настоящих retry0/retry1 no-progress, групповой All-members waypoint
уже был снят движком. Прежний short helper требовал этот live group waypoint и
немедленный полный rally после переноса≤8м, поэтому полезное короткое продвижение
не допускалось; fallback сработал при ещё46с исходного approach budget.

`AICF_VehicleManualStagingRecovery` допускает второй вариант guidance: пустая
native group queue, отсутствие current group waypoint и exact current individual
staging task через `AICF_VehicleStagingTaskProof`. Общие guards прежние: current
assignment/intent/base и request/preset/group generations, original component Rpl,
reservation, authoritative AI, fit foot roster с точным cardinality (включая1),
owned selected/executed RUNNING action, retry1/exhausted,12с без progress и5с без
физического движения. Foreign waypoint/action либо неизвестный roster запрещают
коррекцию. Другой уже exhausted outlier также запрещает её.

Разрешён один перенос original actor на2..8м по3D, vertical≤1м с native placement,
projection≤0.5м, pad10..90м и продвижением к rally более1м. One-shot marker
потребляется до asynchronous Teleport с повторными fences. Acquisition сохраняет
тот же native action; нового action/retry/deadline/payment не создаётся. Arrival
не утверждается: обычный readiness должен затем физически подтвердить весь roster
и исходный hold перед настоящими spawn/debit.

Новый controlled `-aicfManualStagingUnblockProbe 1` явно отделяет bounded setup
(безопасная36м точка, setupshift≤25м и disable movement controls) от production
коррекции.50мс observer сначала читает реальную landing≤8м с тем же native action,
после этого возвращает controls, отдельно читает естественный walk≥1м, затем
полный native rally и настоящие spawn/debit. Перенос не считается walking proof.
`-aicfManualStagingExhaustionProbe 1` теперь явно вводит fixture-only отказ всех
bounded placement candidates: одна только исходная дистанция40м больше не
доказывает невозможность короткого продвижения. Этот отдельный fault case
проверяет прежний bounded fallback, физический order restore и отсутствие покупки.
Старые runtime FAIL и исходные oracle receipts сохраняются без переоценки.

30.10 fresh source checks: affected6/6PASS; short helper25source mutants,
12actual geometry,16guidance,44cardinality; fixture42negatives, logoracle76controls.
Causal native before-v5: WB0/native0/source drift0/full stopped; реальный
retry0 timeout12.131с, retry1 timeout12.033с, actor35.9989м,59.786с остатка
approach deadline → ранний `SPAWN_STAGING_ACTIONS_EXHAUSTED`. Functional FAIL,
full fatal0, strict resources28FAIL. Native after-v3 выполняется; до результата
это не runtime PASS. Evidence `spawn/manual-unblock-before-v5/` и
`spawn/manual-unblock-after-v3/` под `.codex-runtime/issue78-finalization-20261029/`.

Условный after-v5 (тот же fixture SHA, что before-v6) подтвердил actual3D
correction3.00012м и прежний selected/executed native action; лишь после actual
landing observer вернул movement controls. Затем native actor прошёл1.01263м,
все original5 физически вошли вrally до оплаты, произошла одна настоящая покупка
и создание машины. Health и исходные deadlines неизменны. WB0/native0/source
SHA drift0/full stopped/auditor15PASS/fullfatal0; strict resources28FAIL.
After-v1/v2 compileFAIL отражают соседний array.Copy API mismatch; native в них
не запускался. After-v3/v4 functionalFAIL сохранены: все bounded destination
были отвергнуты; успешный сценарий требует явного native safeEscape preflight.

Это условное доказательство механизма, без заявления о повторении динамического
окружения Multi-v9. Отдельный read-only historical geometry-v2 проверил exact
старые origin/staging/pad на свежем native мире: old9 имели6 safe points,
добавочная сетка82 — ещё61. Полная91-grid проверена по actual nativeUsable,
3D/vertical/projection/pad/progress receipts; WB0/native0/source0/fullfatal0,
resources28FAIL. Исторические dynamic occupants не воссозданы (явныйreceipt0).
Пропуск всех допустимых точек вold9 не доказан, поэтому production search не
расширен. Original Multi-v9 FAIL сохраняется; практический gate — fresh Multi.
Geometry fixture имеет10source negatives +4 gameplay-mutation vetoes,91unique
candidates/old9prefix. Evidence `manual-historical-geometry-v2/geometry-audit.json`.

Final negative `manual-exhaustion-after-v2`: strict13/13PASS, WB0/native0/source0/
fullfatal0, resources28FAIL. Fixture ставит native-safe ordinary point220m до
trip, чтобы близкая home цель не завершилась законным POINT_HOLD_REFRESH раньше
проверки. Exact restored/current/queued waypoint, original identities, production
order durability pending0/meaningful1 и старый deadline подтверждены. Oracle
не ослаблялся. Final source audits7/7PASS; frozen9files manifest в evidence.
Новые ordinary final-v5 US/USSR используют335 rawSHA из root manifest, старые
FAIL не переоцениваются; outcome30min ожидается отдельно.

Свежий Multi-v10 на335 byte-exact production files (`production-final-v6-manifest.json`)
с Workbench0 завершён как **ABORTED / native-1 / audit1**, 757.606s из1800.
Первый permanentFAIL: US/1 `SPAWN_STAGING_ACTIONS_EXHAUSTED` в00:29:18.857,
trip-US-1-1-3, все5 живы, asset ещё отсутствует; actual selected/executed actor
move FAILED в25.716m от staging goal. Stable cohort receipts0, MOVING4/6 не
доказаны. Полные остановленные3 logs: script/enginefatal0; resource gateFAIL
сохранён. Evidence `movement/multi-moving-v10/verified-final-result.json` и
`first-failure-window.log` под `.codex-runtime/issue78-finalization-20261029/`.

### Живой server при временно отсутствующей группе

Latest ordinary final-v5 обеих сторон честно FAIL: USполные1800s безSCRIPTfatal,
но routeосталсяUNPROVEN придолгомTACTICAL_DEPLOYMENT; USSR имел настоящийpartial
replacement4/5 timeout после midspawn worldbudgetrace и отдельный heartbeatgap.
Все исходные335SHA после остановки совпали; native0/0, полные остановленные логи
и frozen E550 auditor verdict сохранены. Эти результаты не переоценены.

Heartbeatgap имеет отдельную fixture причину: BaseSoak.Update возвращался при
nullgroup до10s sample, хотя Proof4 уже принимал доказанное all-DEAD retirement
и штатный capacityWAIT. Новый fixture helper вызывается перед прежним
GROUP_UNAVAILABLE classification, сообщает actualalive0/groupNONE/currentgen,
slotstate/nativecapacitywait/typedretirementstatus. Readiness, progress и route
не выдумываются, production clocks не меняются;10s cadence и frozen60s gap oracle
прежние. Новый actual extracted Update-nullbranch/helper contract проверяет6
случаев и исполняет неизменный auditor liveness block в2случаях: настоящий
100s hung gap остаётсяFAIL. Это исправляет сбор evidence, а не midspawn defect.
Production staging native diagnostics добавлены отдельно без изменения поведения:
expected/current/executed pointer+goal/entity/radius, exact actual controlagent,
bounded raw native path/prefixfinite flags и отдельные physical/radial samples;
никаких path-progress credits. Новые native diagnostics начинаются со свежих
337source, старые full198/335nativePASS к ним не относятся.

Дополнительная подготовка `MINIMUM_BACKGROUND_CAPACITY` отделена от default
`PRODUCTION_BACKGROUND`. Исходный stress Multi-v11 завершён ABORT/native-1/
audit1,1412.662s из1800: реальные stable2=30.832s и stable4=30.985s; затем
native capacity WAIT (actual230..233 / hard228) и US/0 TRANSIT_DEADLINE_EXPIRED.
Full3logs fatal0,337raw-sourceSHA0, старые fixture54261016/oracleAEF12F34
сохранены. Это FAIL исходного фона, не PASS дополнительной подготовки.

Opt-in fixture `AICF_VehicleMultiMinimumBackgroundProbe.c` изменяет только
желаемый состав background slots6..9 у обеих сторон на1 до native пополнения.
Весь preflight сначала требует8 original живых одиночных actor, exact group/
generation/EntityID/Rpl/current AI agent и отсутствие active/queued recruitment.
Actual roster>1 приводит к setup rejection: уменьшения физического состава,
удаления, лечения или изменения native лимита нет. Pure initial muster posture
не равна уже созданному order; queued/active order проверяются отдельно.
Постоянные20 slots и selected12×5 остаются. SETUP/periodic/FINAL readback
повторно подтверждает все8 original identity/alive1/desired1/physical1/queue0.

В том же отдельном fixture bounded read-only world census на SETUP/COHORT и
capacity WAIT раз в30s читает до4096 agents и4096 unique groups. Counted+
activated классифицируются как Character/Vehicle/AIGroup controller/null/other,
с selected/background membership US/USSR и foreign characters. Native active/
intended считываются до/после; truncated/missing/changed snapshots не заявляются
complete. Дельта active-vs-intended сама по себе не доказывает число машин.
Census агрегатный, тысячи actor rows не печатает и native state не меняет.

Runner `-BackgroundMode MINIMUM_BACKGROUND_CAPACITY` добавляет только
проверенный CLI enum, отдельный fixture и явные preparation metadata. Default
остаётся `PRODUCTION_BACKGROUND`. Старый MOVING oracle/1800s/30s stable и все
health/deadline gates не изменяются; отдельный background log audit добавляет
обязательные8 исходных actors до/после. Дополнительный результат не подтверждает
обычную войну или исходный stress с default background10.

Причинный stage для этого опыта копируется из immutable Multi-v11 all337
rawSHA canonical2C694E36, не из рабочего дерева с последующими production fixes.
Stage `movement/minimum-background-v1-source/stage`: terminalWB0,337raw
before/after equal. Native пока NOT RUN, требует отдельного review closure.
Контракт `Test-VehicleMultiBackgroundContracts.ps1`:73PASS (37actual identity/
census cases,16source negatives,19 log cases); старый Fixture68PASS.

### Proof4 mixed EntityID comparator, 30.10.2026

`NativeEntityMatches` теперь принимает native fullhex и legacy signed low32
fixture receipt в обеих ориентациях. Два fullhex по-прежнему сравниваются целиком:
совпавшие low32 при другом namespace не дают equality. Replacement physical
`GROUP_TRACE` дополнительно привязан к единственному full `SPAWN_BOUND.group`
той же faction/slot/generation; current/owned waypoint должны иметь один full ID.
Пять новых actor Rpl остаются уникальными, old native DEAD component identities
и paid/readiness gates не ослаблены. Canonical PS5.1 observation contract:
8 actual comparator cases + mixed positive/5 negative aliases/generation/Rpl,
старые source16/log20/typed14/full7 PASS. Evidence:
`.codex-runtime/issue78-finalization-20261029/spawn/observation-comparator-after-v1`.
Исходный US diagnostics-v61800s/native0 имеет frozen auditorE550 functionalFAIL3
и resourceFAIL32; новый comparator не переобъявляет его PASS. Два отказа были
форматом ID, третий — настоящий generation2 `INSUFFICIENT_SUPPLIES` acceptance
latch до physical recoveryDONE; его typed economic design пока не реализован.

Final owner/epoch closure: root review потребовал exact active controller и
latched US/USSR state pair. Перед/после Capture, actual IsCurrent и каждого
ApplyCommanderConfiguration эти identity повторяются. Stopped/superseded или
state-swap возвращают fail-closed без Abort/RequestClose successor world.
Readback повторно проверяет все8 epoch перед summary; stale state не считается
погибшим actor. Новый final контракт97PASS,51actual (в том числе14 owner/
callback cases),26source negatives и19log cases; независимое review0/hashdrift0.
Тестовый C#shim WIP compileFAIL сохраняется в ignored variant-after-owner log;
source primitive guard был compilable, shim затем получил native-ref base и
latched fields. Старые73/WB0 до owner guard — historical, не final proof.
Последний stage fixture rawSHA ADE9983FE1475250C8490185346A3E7663AF1778EF5DFF61BB083C8204068
(полный SHA в review-source-manifest-final.json); terminal WB0 01:42:47.782,
all337 oldcausal rawSHA0; оригинальные basefixture/oracle bytes неизменны.
Native на этом final варианте пока NOT RUN.

Дополнительный MINIMUM v1 остаётся setupFAIL (15.559s): desired1 не завершает
сам по себе штатную INFANTRY_MUSTER posture. V2 завершает только этот известный
1/1 muster через текущий `OrderPlanner.RefreshInfantryMuster`, с повторными
owner/epoch/actor fences; raw recruiting0 остаётся обязательным. Контракт этой
версии110PASS/60actual, independent review0 и WB0. V2 native достиг stable2
31.503s и stable4 31.738s, затем настоящий USSR/1 STUCK→два разрешённых repair→
BUDGET_EXHAUSTED→NATIVE_GROUP_ABANDONED_VEHICLE. Адресный ABORT/native-1/audit1,
864.504s из1800, полные3logs fatal0; old337raw и fixture/oracle unchanged.
Census228=221 counted activated characters+7 vehicles при8 физических машинах
не доказывает деактивацию конкретного UAZ2630: per-car activation/LOD не писались.
Foreign169 здесь означает отсутствие managed membership, происхождение UNKNOWN.

MINIMUM v3 добавляет только read-only observer раз в5s для не более12 машин:
exact current lease/group/handler/Pilot, movement-associated car agent и unique
world-agent join повторяются после native getters. Missing/null/truncated world
snapshots, duplicates или identity drift дают UNPROVEN/UNKNOWN. PROVEN сообщает
raw IsAIActivated/CountTowardsAIWorldLimit/LOD/permanent/max, goal/path/navlink,
velocity/handbrake/health; никакой активации, LOD, geometry или clock mutation.
Fixture5F916683/contract1057EFDC: canonical PS5.1 137PASS/75actual, WB0;
337raw stage точно равен causal manifest2C694E36. Историческая подготовка v3 на этом этапе имела native NOT RUN до
peer closure; окончательный диагностический FAIL приведён ниже. Evidence `movement/minimum-background-v3-source`; старый stress
и V2 этим не переобъявляются PASS.
MINIMUM v3 — диагностический FAIL, не movement PASS. 03:01:46.707 USSR/0
NATIVE_GROUP_ABANDONED_VEHICLE на1030.291s; exact car2915/car-agent2924,
pilot0E99/agent0E9E, handler1 и текущий lease/native control подтверждены.
Перед STUCK и после обеих коррекций наблюдались active1/count1/LOD0, pilot1,
health1/movementDamage0/fire0/gate0. 604 observer rows:545PROVEN,506active,
39inactive только при gatePending1,59UNPROVEN; inactive вне gate не наблюдалось.
Первая коррекция8м,03:00:48.609:1581.85,8.598,2001.13→1580.1,10.95,2008.93;
вторая8м,03:01:34.461:1580.21,11.062,2009.42→1578.45,12.65,2017.22.
Native fragment3nodes после первой коррекции не совпадает с global endpoint:
он остаётся только local hint, не route-progress proof. После второй native
UNREACHABLE/budget exhausted→штатный SCR_AIGetOutActivity1162/1000.
Stable2=30.733s; stable4/6 не выполнены. Census229=225characters+4vehicles,
из них189 foreign characters имеют factionFIA; prefab/origin UNKNOWN.
Адресный ABORT/native-1/audit1/background1,1069.620s из1800; полные3logs fatal0,
337raw after0drift, original fixture/oracle unchanged. Evidence
`movement/minimum-background-v3/verified-final-result.json`, exact observer
windows, full-before-addressed-stop.log SHA05F78EF85FDE0CF3952AA9C037D507684CCF4DF0BB3B8333E1AD87C295E6E4EA.
Новый geometry/activation fix здесь не применялся; прежние FAIL не закрыты.
После MINIMUM v3 введён отдельный read-only `AICF_UnstuckPathHint`:
он не меняет `ValidatePath`, route-progress receipt, clocks или budget. Полный
native путь обязан соответствовать endpoint по прежним правилам. Отдельный
local fragment (2..128 finite points, первая точка≤2м от origin) может только
предпочесть площадку; label `hint_only=1 path_progress=0` обязателен. Кандидат
должен иметь unique projection, forward arc≥4м, gap≤2м и height≤2м. Существующие
128 centres/4..15м/hazard6/порядок rings и bearings/две попытки сохраняются.
UNKNOWN hint оставляет первый безопасный legacy candidate; valid hint без
safe forward возвращает route rebuild only, captured stale hint не превращается
в UNKNOWN fallback. Orientation коррекции не меняется.

`AICF_UnstuckMutationScope` фиксирует actual managed slot/lease/trip/assignment/
intent, EntityID/Rpl/поколения/цель/origin и исходный owner до native проверок;
после последней physical query ownership повторяется даже при UNKNOWN hint.
Stopped/superseded controller, чужая lease/entity, expired/deleted/weak-null или
изменённая цель запрещают mutation. Hint отдельно повторяет actual Pilot/car/
positive handler/current+executed Move/queued owned waypoint/full raw path,
health/active/count/LOD0/speed≤1; окно3с неизменно. Старые private synthetic
VehicleScenario groups не дают такого ownership proof; новый runtime scaffold
использует только штатную initial bind/readiness/acquisition/boarding цепочку.

Canonical PS5.1: geometry40 (23 actual extracted), mutation owner59 +3 executed
unsafe mutants, hint identity52 +3 executed unsafe mutants; neighbour gates
Unstuck55/Overturn56/NativeRoute59/Stage35Recovery PASS. Stock WB0/all342 raw0;
independent source/test review0. Эти результаты ещё не подтверждают native
geometry fix. G2 BEFORE-v1: real fresh five/20slots READY, но native8м spawn
cylinder трижды отверг историческую точку из-за static environment5FC9/20CC0;
car не создавалась. Native0,312.626s,setupFAIL/physicsNOTRUN,3fullLogs fatal0,
resources28FAIL. AFTER-v1 WB0/nativeNOTRUN. Следующая отдельная preparation
ищет initial8м safe pad в3D≤15м, сохраняет normal commit и near-point→real5seated→
far-point handoff; если площадки нет — setupNOTRUN, без расширения clearance.
Evidence `movement/path-hint-recovery/`; старые PASS/FAIL не переобъявляются.

Preparation BEFORE-G2-v3 завершилась за17.284с: все24 `FindEmptyTerrainPosition`
вернули false, native0/fatal0/resources28FAIL/source343raw0. Цилиндр в этой
версии условно не вызывался при false, поэтому её `cylinder8=0` не является
фактическим измерением. Отдельная read-only collision-v4 (fixtureBAF8ECAF,
contract420A7836, canonical PS5.1 37+7 PASS, WB0, peer0) вызвала native8м
цилиндр для каждого центра:24false,60 bounded diagnostic rays,24 blocking
cylinder hits,0own-roster hits. Из них22 имеют static Granite/Erosion/GuardBox
prefab,2 — Generic/StaticModel с неизвестным prefab. `FindEmpty` внутри
проецирует высоту; его false-cause join остаётся `UNPROVEN`, хотя блокировка
цилиндра в исходной высоте центра наблюдалась непосредственно. Это24 центра,
а не исчерпывающее доказательство всего диска. Native0/17.773с,3fullLogs
fatal0/resources28FAIL/source343raw0; автомобиль не создан, physicsNOTRUN.
Evidence `movement/path-hint-recovery/native-before-G2-v4-collision/`.
AFTER-v3 остаётся WB0/nativeNOTRUN: известный неготовый setup не повторяется.

Отдельный opt-in `aicfUnstuckPathProbeSetup=FRESH56` выбирает максимум24 центра
24/40/56м до рождения managed actors; это исходный spawn, а не перемещение живых
ботов/машины. Native8м cylinder и production commit остаются обязательными.
Один setup pose fault разрешён только после normal far binding, при actual3D≤15м,
same original5/lease/car/health/deadline/old2, без human/foreign use и с native OBB
clearance; orientation/velocity/path/phase/clocks не записываются. Он явно
`native_progress=0`, наблюдение результата recovery начинается после fault.
Fixture30F6715E/contractEFE2AFA9: PS5.1 46 source controls+7actual wait+6actual
distance controls PASS, independent review0, BEFORE/AFTER WB0; оба343raw stage
различаются только TransitFlow и содержат одинаковый G2.

FRESH56 BEFORE-v5: safe pad40м к югу выбран до рождения; normal5READY,
native commit/one paid150 purchase/5settled/near→far same trip+lease подтверждены.
Это тот же GUID259EE7B78C51B624/UAZ469.et и цена150, что MIN3 car2915; native
engine192142 одинаков, но точный asset SHA и сравнительные bounds не записаны.
До исторического участка машина не доехала: два UNREACHABLE исчерпали old2 через
route rebuild only (сначала captured speed2.14672, затем empty native path),
физических коррекций0. Native fragment3 в начале находился у нового spawn,
а не у historical fault pose; исходный3node-case и pose fault — NOT RUN.
Затем native GetOut/abandon: native0/95.996с/3fullLogs fatal0/resources28FAIL/
343raw drift0, итог SETUP/COVERAGE FAIL. AFTER-v5 compilePASS/nativeNOTRUN;
этот результат не подтверждает geometry fix. Evidence
`movement/path-hint-recovery/native-before-G2-v5-fresh/verified-final-result.json`.

Дополнительный readonly DIAG-v6 завершился native0/84.096с/fatal0/resources28FAIL/
343raw0 после первого native callback. Car24F1/agent9472/Pilot3741:
worldunique1, active1/count1/LOD0, permanentLOD=-1/maxLOD10, current/executed
Move RUNNING, handler1, health1/movementDamage0/fire0. За84мс до callback
actual fragment3 шёл от нового spawn1969.41 на5.4м к востоку; перед controller
super путь уже empty, speed2.4292. Отключение car AI перед этим отказом не
наблюдалось. Road graph возвращал far endpoint, но actual closest-road point
находился25.7287м от свежего spawn и его native8м cylinder был blocked;
`drivable_proven=0` обязателен. Это диагностическая подготовка, не historical
3node recovery proof. Full source и соседние native события сохранены в
`native-before-G2-v6-road-flags/`. Prefix двух diagnostic events потерялся
из-за nested `string.Format(...) + NativeFlags(...)`; callback row определяется
по отдельному timestamp, source hook до super и следующему native CAPTURED.
Последующая test-only правка заранее материализует flags в локальную строку,
затем форматирует prefix; native этого форматирования ещё NOT RUN, compile0
на изолированном прежнем G2 snapshot. Никаких activation/LOD/policy setters
диагностика не содержит; новый native setup требует отдельного обоснования.

ROAD240-v7 выбирал fresh spawn из24 actual native-polyline centres (arc±20..240),
только с native8м clearance и обычным commit; семь центров оказались clear.
Actual5ready/5boarded/onepurchase150/samelease и native full76-point driving
подтверждены, в том числе исправленные диагностические prefixes/actual bounds.
Но full native route прошёл минимум43.11485м от historicalPad, автомобиль —
45.27144м. Через неизменные300с fixture завершился coverageFAIL: posefault0,
recovery0, historical3node-caseNOTRUN. Wall315.8867с/native0/3fullLogs fatal0/
resources28FAIL/343raw0; AFTER-v7 WB0/nativeNOTRUN. Evidence
`movement/path-hint-recovery/native-before-G2-v7-road240/verified-final-result.json`.

Следующий opt-in `aicfUnstuckRoadGoalDiagnostics=1` только читает neighboring
roads у endpoint по другую сторону ближайшей historical projection. Ограничения:
16roads,1024finitepoints/road,4FARnativeendpoints, endpointjoin3D≤3м, goal>200м
отspawn/historical, raw graph radius10. Даже подходящий geometric corridor≤15м
не является drivable proof и не выдаёт приказ. Metadata-only ветка пропускает
fixture preset/preflight/point command, дожидается normal aggregateREADY и
закрывается в собственном Update. Это не запрещает штатную Core coordination
внутри предшествующего super.Update; отсутствие физической машины проверяется
по полному native контексту отдельно. Unexamined roads после goal cap и
lateNULL запрещают complete; goal_search_complete учитывается отдельно.
Canonical PS5.1:76source negatives +7wait +6localfault +10RoadPoint +7corridor
+8actual inspection cases PASS; terminal WB0 на oldG2 snapshot. Native этого
metadata дополнения на этом подготовительном checkpoint было NOT RUN.

Metadata AFTER-oldG2-v1 затем завершился native0/16.313с/3fullLogs fatal0/
resources28FAIL/343raw0. Normal Core успел создать site plan, но exact observed
operation оставалась SITE_PLANNED с vehicle/lease NONE и entity_created0 до
COORDINATOR_STOP; это не census всех мировых машин. Actual neighboring road
дал rawreachable1 для FAR goal `<684.711,1.5,2276.18>` через точный общий endpoint,
enum2/examined2/queries1/complete1. Source road projection к historical pad
находилась в10.862м. Это geometric corridor, не physical drivable proof:
passed0/READONLY_NATIVE_NEIGHBOR_GOALS_CAPTURED, movement/recovery NOT RUN.
Evidence `movement/path-hint-recovery/metadata-after-oldG2-native-v1/`.

Новый ordinary US-v11 обнаружил отдельный deterministic cycle при UNKNOWN hint
и native path0: после45с без physical evidence первая коррекция переместила
Humvee22CF изA `<1690.38,29.3301,3276.95>` вB
`<1692.77,28.1706,3284.58>`, следующая после45с вернула его почти точно вA.
Обе попытки8м были израсходованы; затем VEHICLE_STUCK_PERSISTENT и подтверждённый
foot fallback. При resume Pilot2100/native handler1 совпадали, own parking был
RELEASED с persistent_brake0; последующие activation/LOD/brake не записаны и
остаются UNKNOWN. Cycle не доказывает первичную причину native неподвижности.

Candidate `AICF_UnstuckFailedSites` хранит максимум два source origins только
после actual NO_PHYSICAL_MOVEMENT timeout и успешного transform. В пределах
того же trip/lease/car/assignment/intent/target/phase-transition epoch и точного
owned route он запрещает кандидат в3D≤4м от доказанно неудачной точки. Перенос
history на новую route generation разрешён лишь явной последовательностью
mobility flow Begin→Stage→actual bound waypoint/queue с повторной identity
проверкой; unrelated+1 и TACTICAL→TRANSIT ABA отвергаются. Reset и настоящее
physical motion очищают память. Старые128 samples/4..15м/2attempts/deadlines и
health/player/foreign fences сохранены. Failed transform не создаёт record.
Новые extracted contracts9source/37actual/4executed unsafe mutants PASS;
старые sampler61+3/hintIdentity52+3 также PASS. Isolated frozenG3+history
Workbench343/source drift0 PASS (`movement/failed-sites-wb-v3/`). Native
anti-cycle proof ещё NOT RUN; этот candidate не превращает ordinary FAIL в PASS.

Опциональный test-only `aicfUnstuckBrakeDispatch=1` использует ту же настоящую
managed5/одну покупку/normal near→far поездку, но вместо historical pose fault
однократно владеет native persistent handbrake false→true. При первоначальном
native true он только ждёт фактического false в пределах прежних300с;
production parking, human/foreign use или identity mismatch запрещают запись.
Успех требует обычные45с watchdog, две реальные коррекции/history2, безопасное
снятие только своего brake и native10м/3distinct polls при тех же ID/Rpl,
health/lease/deadline/pay1. Native callback не подавляется: свой brake снимается
до unchanged super, preemption остаётся coverageFAIL. Stop/Finish пытаются
снять только собственную запись; неподтверждённое снятие не объявляется PASS.
Это отдельный dispatch gate; `cycle_case_proven=0` обязателен. Auditor проверяет
actual correction4..15 finite, порядок Applied→NoPhysical1→Correction1→
NoPhysical2→Correction2→Released и exact car/operation/deadline.

Brake native-v3 завершился native0/3fullLogs fatal0/resources28FAIL/344raw0,
но initial Begin отказал до записи: corrections0/old watchdog NOT RUN.
При far binding наблюдались active1/LOD0/nativeIdle/path0/speed0; конкретный
отказавший parking predicate не был записан, причина UNKNOWN. Исходный runner
также выявил несовместимость array binding с пустыми строками полного native
лога; failure сохранён, adapter исправлен с отдельным executed blank-line case.
Следующий test-only fixture пишет actual guard receipt и ждёт только штатного
начального brake release;11source/17actual brake/33log cases PASS, independent
peer/WB/native следующего checkpoint учитываются отдельно. Production3 не менялся.

Bounded-wait Brake-v4 подтвердил actual initial guard: exact1/usable1/
noForeignHuman1/nativeBrake1/noProductionParking/stopped0/currentPreset1/TRANSIT.
Четыре ticks он ждал без записи; после actual native false записал own true.
Следующий tick завершился BRAKE_NATIVE_OVERRIDE_OR_GATE_PREEMPTED_COVERAGE;
свой brake безопасно отпущен/readback0, repairs0. В тот момент car/Pilot были
active1/count1/LOD0, current+executed Move RUNNING/full76path, health1/fire0/
movementDamage0; gate/GetOut events отсутствуют. Raw brake/точный отказавший
predicate непосредственно перед release не записан: native override вероятен,
но это inference. Native0/77.730с/3fullLogs fatal0/resources28FAIL/344raw0,
PID25028 stopped. Watchdog45с/anti-cycle corrections остаются NOT RUN.
Fixture BA0E6137/Brake contract DDD93D3C/lib336C2515: independent review0,
PS5.1 11source/17actual/33log cases PASS, isolated WB344/0drift PASS. Полный
verdict `movement/brake-dispatch-native-v4/summary.json`; идентичный replay
не требуется без новой controlled setup причины.

Последний post-motion Brake-v5 убрал startup неоднозначность: перед own записью
actual native distance16.14→23.25→32.68м/3distinct polls, exact current/executed
Move RUNNING и native brake false. Следующий tick дал вычисленный однократно
receipt own1/exact1/noForeign1/controllerKnown1/nativeBrake0/
failed=NATIVE_BRAKE_FALSE/gateNULL0. Native override теперь наблюдался прямо.
Свой brake отпущен/Stop подтверждён; native0/85.296с/fatal0/resources28FAIL/
344raw0, repairs0. Brake family закрыта без дальнейшего одинакового replay;
она не исполнила45с watchdog или anti-cycle recovery. Frozen113592CB/
11E04688/B40B5B88: independent11source/27actual/41logs PASS и WB344/0drift.

Opt-in `aicfUnstuckHistoricalCorridor=1` использует только metadata-подтверждённую
FAR goal684.711,1.5,2276.18 и historical **первый** STUCK origin
1581.85,8.59795,2001.13 из MIN3 03:00:48.609; прежний Pad был destination
неудачной коррекции. Исторический yaw неизвестен, fault сохраняет фактический
текущий orientation. Default goal/Pad прежней fixture не изменены. Для этого
варианта допустим actual FULL_NATIVE_PATH или LOCAL3 hint, захваченный после
единственного actual≤15м fault; actual production sampler receipt должен
подтвердить hint/forward correction, затем native10м/3distinct polls при прежних
car/Rpl/lease/deadline/health/pay1. Новый auditor проверяет каждую коррекцию
finite4..15, unique attempts1[,2] и её собственный production receipt; только
последняя обязана быть forward с доказанным hint.

Corridor-v1 достиг exact first origin: actual pose fault9.701м с OBB clear,
затем car278D сам продолжил движение по172-node native path к другой FAR goal.
Post-fault native distance22.12м/32.84м и далее не требует recovery и не выдаётся
за proof исправления. Repairs0, исходные300с дали coverageFAIL:
native0/312.854с/3fullLogs fatal0/resources28FAIL/344raw0, PID29248 stopped.
Evidence `movement/corridor-native-v1/{summary,verified-final-result}.json`.
FixtureA0EB5EA0 входит в frozen recovery-v14:219/219 offline PASS/919SHA0 и
3/3 compile PASS; Corridor-specific12source/37logs independent PASS — отдельный
последующий supplement, а не часть того full219.

Последняя разрешённая вариация `aicfUnstuckCorridorReturnOriginal=1` использует
этот FAR endpoint только для подхода. При actual≤30м/budget0 штатный Planner
однократно получает исходную goal1475.78,36.6684,2473.24. Fixture ждёт настоящий
новый assignment/intent/waypoint/current+executed Move и double-read native path
с исходным endpoint, сохраняя тот же trip/car/lease/pay1/первоначальный deadline.
Естественный route generation reset допустим лишь с фактическим новым waypoint.
Никаких phase/path/clock/AI setters или увеличения300/15/2 нет. Source80AA7DBA/
contractE3B7A952/lib86461349: independent20source/49logs PASS, terminal WB344/
0drift PASS. Native `movement/corridor-original-native-v1/` теперь полностью
остановлен:0/314.823с/3fullLogs fatal0/resources28FAIL/344raw0, PID28764 closed.
Actual command при23.2053м изменил assignment/intent на4/4; новый waypoint/
route2 и82-point native path к исходному endpoint подтвердились при том же
deadline1158532. Fault11.0617м выполнен только после этого. Затем native сам
перестроил86-point path и проехал12.13м/3distinct polls от fault без recovery.
Исходные300с закончились coverageFAIL, repairs0/forward-correction NOT RUN.
Все lab вариации закрыты: следующая проверка — current+quota ordinary; position/
goal воспроизведены, historical yaw и прочий динамический контекст неизвестны.
Native escape не подтверждает placement hint или anti-cycle runtime fix.

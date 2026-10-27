# Ступенчатая нагрузка транспорта и native combat hold

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
После последней ступени задаются конечные цели: доезд, normal COMPLETE высадки
и все пять пешком≤35m при неизменном PLAYER/POSITION intent/revision.
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

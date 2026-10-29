# Ступенчатая нагрузка транспорта и native combat hold

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

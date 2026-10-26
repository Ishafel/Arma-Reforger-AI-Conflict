# Ступенчатая нагрузка транспорта и native combat hold

Это продолжение issue78. Пользователь выбрал сначала проверить2→4→6 отрядов
на каждую сторону. Обычные defaults INFANTRY и cap10 машин/faction не меняются.
Оба новых runner работают только с отдельным immutable snapshot, canonical
`Start-AICFRuntime.ps1` и терминальным Workbench Validate. Fixtures в production
не устанавливаются. Offline contracts не запускают игру.

## Два режима нагрузки

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
четыре native debit/join и исходные пять actor identities. После muster текущей
ступени выдаются настоящие UAZ469 и M1025/M2HB с пятью exact occupants.
Первые машины освобождают штаб до найма следующих: предварительного скопления
всех60 бойцов у казарм больше нет. `ACTIVATED/MUSTERED` содержат cohort2/4/6.

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

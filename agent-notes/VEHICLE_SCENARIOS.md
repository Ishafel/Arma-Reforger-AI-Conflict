# Полные сценарии техники и диагностика групп

Продолжение issue #78. Прежний TransitProbe проверяет начало движения и высадку
после retarget к машине. Он не подтверждает доезд до исходной дальней цели.
Для этого служит `AICF_VehicleScenarioProbe.c` и отдельный строгий анализатор.

## Повторяемый запуск

Только Windows terminal, без клиента, GUI, screenshots и установленных изменений:

```powershell
& tools/Invoke-AICFVehicleScenarios.ps1 `
  -EvidenceRoot .codex-runtime/vehicle-scenarios-<уникальное-имя>
```

Runner готовит isolated stage из текущего HEAD + production diff/untracked,
добавляет только этот fixture, компилирует Workbench и последовательно запускает
canonical `Start-AICFRuntime.ps1 -Role Server`. Каждый case получает свежий profile.
CLI/manifests, native exits, полные logs, auditor verdict и hashes stage сохраняются
в EvidenceRoot. `-Cases uaz5,ural5` выбирает поднабор; `-ServerPort` задаёт также
реальный native `-addr`. `-StageRoot` подключает заранее подготовленный stage,
`-SkipCompile` допускается лишь после его успешной компиляции. Два independent
runner можно запустить с разными портами и EvidenceRoot; общий stage неизменяем.

Scope — Stock Arland, recipient USSR. Изолированный транспортный прогон включает
`-AICommanderMode USSR`, HOLD_FIRE, запрет урона только тестовому составу/машине
и взаимную дружбу всех faction в тестовом мире. Это только fixture, отсутствующий
в production; проверки подтверждают поездку без боевого противодействия.
`-Combat -AICommanderMode BOTH` оставляет обычные отношения, урон и огонь для
разбора боевого прерывания поездки. Возле гарнизона native
VehicleCombatActivity может сам потребовать высадку; это отдельный исход,
не PASS доезда. Для поездок fixture выбирает реальные road vertices в350..900m,
исключает близость других баз и загружает native navmesh tile самой цели.
Нет координат/имён баз в Core или подготовленного teleport маршрута.

| Case | Состав и проверяемое действие |
|---|---|
| `uaz5` | Пятиместный закрытый UAZ,5 бойцов |
| `ural5` | Transport Ural,5 назначенных бойцов, остальные места свободны |
| `armed3` | UAZ PKM, обязательные Pilot/Turret + passenger,3 бойца |
| `solo1` | Невооружённый UAZ, только один водитель |
| `retarget5` | Та же поездка, после80m новый удалённый target, без подмены машины |
| `repeat5` | Две полные поездки теми же actor IDs/group; повторный admission и посадка |

Каждый case физически садится на exact seats, ждёт30s у исходной близкой цели,
получает дальний приказ и едет до его dismount zone. В обычном case target/revision
не меняются до прибытия; `retarget5` разрешает ровно одно явное дальнее изменение.
После штатного DISEMBARK_COMPLETE/HANDOFF→COMPLETE бойцы должны пешком дойти к
той же точке: все живы, исходные actor IDs, farthest<=35m. Vehicle arrival требует
distance<=dismount radius+10m, displacement>=100m, >=5 polls с реальной скоростью,
без mobility relocation. Native timeout20min завершает case FAIL, а не зависает.
Нехватка достижимого test target после30s — отдельный TARGET_PRECONDITION FAIL.

`Test-VehicleScenarioLog.ps1 -LogPath ... -LauncherPath ... -NativeExitPath ...
-Scenario uaz5` проверяет полный остановленный log и соседние logs. Он требует
реальные occupant/arrival/foot/completion receipts, порядок, неизменность цели,
group/member diagnostics и native0. Старый «проехал25m» или пеший выход через
fallback не пройдут. Resource errors не превращаются в script PASS общего gate:
они остаются отдельным native baseline/blocker. Его contracts содержат45 controls.

Штатный OpenGate может временно высадить пассажира: fixture требует живой exact
цепочки PerformAction/OpenGate + GetOut + GetIn на исходное место, тот же group
activity, vehicle/member/seat identity. Возврат ограничен60s и должен завершиться
до начала высадки у цели. Самовольный выход остаётся FAIL; зависание у ворот,
смена actor/seat или незакрытая пара STARTED/RETURNED не проходят анализатор.
Production actions/таймеры ради теста не изменяются. Завершённый trip сохраняется
fixture до пешего прибытия: registry вправе убрать его раньше.
Прежняя машина также может законно удалиться в cleanup во время пешего подхода:
fixture больше не разыменовывает её после arrival. `repeat5` повторно запрашивает
транспорт к другой дальней точке после первой полной поездки; повтор прежней цели
идемпотентен. Это не приказ припарковаться
у spawn HQ. Исходные actor IDs сохраняются на обоих этапах.

Для ручного preset `AICF_TransportNativeGate` наблюдает доказанную OpenGate цепочку
водителя/стрелка до обычной mandatory crew recovery. Наблюдение хранится в movement
state, сверяет operation/group/member/vehicle EntityID/RplId, route и assignment
revisions, exact AI owner и seat. Оно не создаёт actions, не ставит reservations,
не меняет waypoint, motion/progress timestamps или абсолютный срок trip. До60s
возвращается WAIT/NATIVE_OPEN_GATE; exact settled return возобновляет обычный transit.
Deadline/identity loss возвращает typed fallback; состояние очищается при Reset
и AbortOwnedActions. Events: TRANSPORT_NATIVE_GATE_STARTED/RETURNED/REJECTED.
Случайный выход без native paired chain этого ожидания не получает.

После анализа архивируй и убери собственные stages/profiles. Не меняй stage,
пока жив хоть один использующий его сервер/клиент. Пользовательскую библиотеку
и profiles Frontend не удаляй. Перенос workflow на RHS/WCS и проверка всех
машин не подтверждаются одним Stock case; нужны отдельные future fixtures.

Для регрессии раздельных dismount budgets сначала выполняется обычный
`-Cases ural5`, затем отдельный новый stage:

```powershell
& tools/Invoke-AICFVehicleScenarios.ps1 -Cases ural5 -DismountBudgetProbe `
  -EvidenceRoot .codex-runtime/dismount-budget-<уникальное-имя> -ServerPort 2540
```

Opt-in добавляет только в stage `AICF_DismountOperationBudgetProbe.c` и CLI
`-aicfVehicleScenarioDismountBudget 1`; production не содержит тестовых веток.
Это controlled fault injection, не обычная игровая последовательность. Fixture
ускоряет расписание трёх существующих `super.TryForceExactManagedExit` и делит
exact native actors по batches1/1/3. Каждый положительный FORCE_BATCH обязан
иметь реальный charge, вызов original member recovery и принятый native exit;
прямая запись счётчиков не используется. После действительного полного logical
egress fixture переносит одного исходного AI внутрь hull, сохраняя native flags
снаружи. Production затем должен выполнить свою normal physical relocation.
Auditor требует все три native receipts, logical0/transitions0/hull blocker,
физический выход ghost через production relocation, прежние deadlines и всю
обычную цепочку COMPLETE→FOOT_ARRIVAL. Отсутствие реального charge/egress/recovery
остаётся coverage FAIL; одного успешного обычного `ural5` недостаточно.
В clearance telemetry допускаются только exit<=3, physical<=3 и aggregate=sum<=6
на фазу с явно объявленными максимумами3/3/6. Прежний общий предел3 не заявляется
сохранённым. Обычный `ural5` запускается отдельно без этой инъекции.

## Снимки групп

`AICF_GroupTrace` — read-only authority observer у VehicleCoordinator. По
умолчанию пишет группы с ручным vehicle preset раз в5s, включая пеший остаток
после trip. Он не создаёт actions, reservations, waypoint, callbacks или RPC.
Cursor хранит только stable key и счётчики; Stop его очищает.

- `-aicfGroupTrace 1` включает также обычные READY группы.
- `-aicfGroupTraceFaction USSR` и `-aicfGroupTraceSlot 0` ограничивают выбор.
- `-aicfGroupTraceIntervalMs 5000` задаёт cadence, clamp1000..60000ms.

`GROUP_TRACE`: run/time, faction/numeric slot/stable key, callsign, group/entity
и generation, assignment/intent/vehicle revisions, role/posture/decision authority,
target, живой leader/position, alive/inside, native current и owned waypoint,
queue count/position, native group action, trip phase/reason/age/deadline,
route generation/endpoint, recovery counts, vehicle position/speed/distances.
Controller origin группы не используется как позиция отряда.

`MEMBER_TRACE`: numeric position/actor ID, world coordinates/life, actual linked
vehicle/seat и transition flags; выбранное и реально исполняемое native action;
expected seat/occupant/reservation/lock, tracked boarding state/age. Данные tokens
читаются только после их создания. Перед boarding rejection пишется внеочередной
полный snapshot с trigger=reason. Sequence связывает членов с group snapshot;
group generation и entity различают старый состав и замену.

`PHYSICAL_MEMBER_TRACE` отдельно перечисляет всех фактических `group.GetAgents`,
включая платно нанятых сверх исходного deployment size. `physical_index` — лишь
порядок native массива; `roster_position` — найденная текущая позиция0..MAX_GROUP_SIZE-1
или-1, не постоянный ID. Пишутся parent/control/replication identity, расстояние
к target/waypoint и его completion radius, selected/executed action, group activity
с native waypoint/current proof и фактический DefendBehavior waypoint. Этот observer
не меняет policy и помогает отличить отставших бойцов от native cover formation.

Для разбора сначала найди faction+numeric slot+group generation, затем operation.
Сопоставь transition reason и target revision с native group action, selected и
executed member actions. Например token может остаться EVALUATED после удаления
из utility, пока другой SCR_AIGetInVehicle уже FAILED; одна проверка token state
этого не показывает. VehicleCombatActivity + GetOutVehicle далеко от target
объясняют боевой выход; это отличается от arrival и от deadline fallback.

`NATIVE_VEHICLE_ACTION_SELECTED` пишет actual action/type/state, related activity,
priority/level, group/member/vehicle identity и координаты при native выборе
GetIn/GetOut и WrapBehaviorOutsideOfVehicle. `NATIVE_GET_OUT_MESSAGE` сохраняет
источник сообщения, activity/waypoint и priority. Все оригинальные callbacks
вызываются ровно один раз; observer не завершает и не создаёт actions.
Priority54 с PerformAction/OpenGate отличается от VehicleCombatActivity125.
`NATIVE_GROUP_MOVE_FAILED` сохраняет штатный EMoveError, связанную машину,
waypoint flag, координаты отказа и живого лидера. Original OnMoveFailed вызывается
ровно один раз. Native ProcessFailedMovementResult при некоторых ошибках выдаёт
SCR_AIGetOutActivity с PLAYER priority: это объясняет выход далеко от цели даже
при HOLD_FIRE/truce. ROAD_REACHABLE означает найденный road endpoint, а не гарантию
успеха последующего engine pathfinding. Такие случаи остаются FAIL поездки.

Preset boarding теперь повторяет застрявший/осиротевший action до2 раз на том же
member/seat, после12s без прогресса (20s для ещё owned nonterminal action), без
продления общего deadline. Повтор запрещён для linked/transitioning бойцов,
чужой reservation/occupant, stale identity или locked seat. Старый token снимается
owner-safe, replacement получает свежий fence. `VEHICLE_PRESET_SEAT_RETRY`
пишет причину, попытку, прошлое ownership/state и неизменный deadline; teleport0.
Общий budget рассчитывается один раз:25s × max(3, assigned seats), ограничен
absolute trip deadline; retries не продлевают срок. Все места сначала
резервируются, затем actions ставятся последовательно:
соседние бойцы не конкурируют за узкий вход. После двух settled polls завершается
только собственный GetIn на проверенном exact occupant/seat; Fail для сидящих
не вызывается. Намеренно выбранный состав из одного водителя после высадки
продолжает приказ к точке; правило отступления единственного выжившего относится
только к потерявшему остальных отряду с desired size>1.

Свежие verdict и ограничения — [TESTING.md](TESTING.md), полные evidence вне Git.

При планировании acquisition срок подхода теперь учитывает самого далёкого
живого участника до staging: запас60s + расстояние/2m/s, минимум configured
timeout, динамическая часть не более10min и всегда в пределах absolute trip
deadline. Срок фиксируется один раз, reservation не создаётся до проверки
живого roster. `VEHICLE_SPAWN_APPROACH_BUDGET` пишет основание расчёта.

Обычная высадка допускает до3 повторных animated exact-exit с интервалом5s
до исходного normal/terminal/trip deadline. Повтор затрагивает только точного
authoritative AI occupant без входа/выхода/смены места; identity перепроверяется
после прерывания его очереди. Общая очередь группы повторно не сбрасывается.
Ни сроки, ни hidden recovery/combat/player fences этими повторами не ослабляются.

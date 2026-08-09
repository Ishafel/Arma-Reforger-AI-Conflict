# Stage 3 — наземная техника

Stage 3 принимается отдельно от пехотных Stage 1/2. Он добавляет транспорт существующим ATTACK-слотам, но не меняет количество пехотных групп, билеты, `OnEmpty`, правила победы или `CaptureRelay`.

Автоматическая компиляция и строка `[AICF][STAGE3][RESULT_CANDIDATE] ... status=READY final=0` являются только кандидатными доказательствами. Кандидат может быть позднее инвалидирован cumulative acceptance-failure latch; финальный PASS требует полного ручного dedicated runtime-прогона этой матрицы и проверки журнала после остановки сервера.

## Срезы приёмки

1. **Transport** — одна невооружённая транспортная машина на сторону, вооружённые машины выключены.
2. **Armed light** — запускается только после PASS transport; одна лёгкая вооружённая машина на сторону с водителем и стрелком.
3. **Recovery** — потеря экипажа, отсутствие прогресса, отказ от машины и продолжение пешком.
4. **Limits/cleanup** — лимит машин, отсутствие double spawn, безопасная очистка abandoned/destroyed entity.
5. **Stage 2 regression** — техника полностью выключена и поведение совпадает с пехотным baseline.

## Перед каждым прогоном

1. Записать branch, commit, версии Game/Tools/Server, дату и тестировщика.
2. Использовать один и тот же commit для всех срезов.
3. Открыть `AIConflictArland` в Workbench и дождаться Resource Database.
4. Выполнить `Build → Validate Scripts` и `Build → Compile and Reload Scripts`.
5. Запустить статический аудит:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\Test-Stage3Static.ps1
```

6. Любая script/resource/dependency error означает `BLOCKED`.
7. Каждый runtime-прогон использует новый `-profile` и `-backendFreshSession`.
8. Сервер и клиент должны быть Diag-версии одной сборки. Retail и Diag не смешивать.

## CLI-конфигурация

| Параметр | Default | Назначение |
|---|---:|---|
| `aicfVehiclesEnabled` | `0` | Явно включает Stage 3; `0` сохраняет поведение Stage 2 |
| `aicfTransportVehiclesPerFaction` | `1` | Число транспортных ATTACK-слотов на сторону |
| `aicfArmedLightVehiclesPerFaction` | `0` | Число лёгких вооружённых ATTACK-слотов на сторону |
| `aicfMaxVehiclesPerFaction` | `2` | Общий лимит active/reserved/abandoned машин стороны до cleanup |
| `aicfVehicleBoardingTimeoutMs` | `60000` | Soft deadline каждой реально начатой boarding-фазы, а также deadline высадки и смены экипажа; immutable total cap равен `planned_phases × timeout`, где transport планирует максимум 3 фазы, armed-light — максимум 4; на всю boarding-попытку возможна ровно одна target-scoped grace 10 секунд с абсолютным hard cap |
| `aicfVehicleStuckTimeoutMs` | `120000` | Deadline отсутствия фактического физического перемещения |
| `aicfVehicleProgressMeters` | `25` | Минимальное чистое сокращение дистанции до route endpoint для `VEHICLE_PROGRESS` |
| `aicfVehicleMotionMeters` | `3` | Минимальное физическое перемещение машины для `VEHICLE_MOTION` и сброса stationary deadline |
| `aicfVehicleObjectiveProgressTimeoutMs` | `300000` | Независимый deadline отсутствия чистого progress к route endpoint |
| `aicfVehicleMaxRecoveries` | `2` | Общий budget смен экипажа и перестроений маршрута на поездку |
| `aicfVehicleDismountDistanceMeters` | `150` | Плановая дистанция высадки до тактического capture point, а не до road endpoint |
| `aicfVehicleRetryIntervalMs` | `10000` | Повтор после временно недоступного safe spawn |
| `aicfVehicleCleanupDelayMs` | `60000` | Grace-период abandoned/destroyed entity до удаления; пустая terminal-машина старого generation может быть очищена раньше ради готовой replacement-группы |
| `aicfVehicleMinimumRouteMeters` | `400` | Короткий маршрут выполняется пешком без машины |
| `aicfVehicleMaximumReuseDistanceMeters` | `250` | Максимальная дистанция группы до оставленной машины для reuse |
| `aicfVehicleMaximumSpawnDistanceMeters` | `2000` | Максимальная дистанция safe friendly-базы от назначенной группы |
| `aicfVehicleCohesionDistanceMeters` | `100` | Допустимый отрыв живого бойца группы от движущейся машины |

Недопустимая комбинация, где requested transport+armed превышает число ATTACK-слотов или общий cap, не создаёт лишние машины: лишний slot остаётся пешим и пишет `VEHICLE_CAP_BLOCKED`.

Пустой faction key штатной main-base spawn point во время запуска — временное `SPAWN_FACTION_INITIALIZING`: ожидается информационный `VEHICLE_SPAWN_DEFERRED` и короткий повтор через 1 секунду. Это не `VEHICLE_FACTION_MISMATCH`, не `NO_SAFE_SPAWN_AVAILABLE` и не warning/error.

Групповые map-маркеры включены всегда. В текущем срезе каждый клиент видит обе фракции; фильтрация до союзной фракции остаётся отдельной следующей задачей.

Штатные мины Arland остаются частью обычного gameplay. Stage 3 не удаляет, не отключает и не переносит их; отдельного test-mode CLI или scenario override для мин нет.

## Прогон T — невооружённый транспорт

Окно PowerShell №1:

```powershell
$serverRoot = "C:\Program Files (x86)\Steam\steamapps\common\Arma Reforger Server"
$repoRoot = "C:\Users\<имя>\IdeaProjects\Arma-Reforger-AI-Conflict"
$stamp = Get-Date -Format "yyyyMMdd-HHmmss"
$profileRoot = "$env:LOCALAPPDATA\AICF\Stage3-Transport-$stamp"

Set-Location $serverRoot
& "$serverRoot\ArmaReforgerServerDiag.exe" `
  -gproj "$repoRoot\AIConflictArland\addon.gproj" `
  -server "worlds/MP/CTI_Campaign_Arland.ent" `
  -MissionHeader "Missions/23_Campaign_Arland.conf" `
  -worldSystemsConfig "Configs/Systems/ConflictSystems.conf" `
  -addonsDir "$repoRoot,$serverRoot\addons" `
  -addons "9178E5822AFE48EA,B52C5F6AEDBF423E" `
  -profile "$profileRoot" `
  -aicfVehiclesEnabled 1 `
  -aicfTransportVehiclesPerFaction 1 `
  -aicfArmedLightVehiclesPerFaction 0 `
  -aicfMaxVehiclesPerFaction 1 `
  -aicfInitialTickets 20 `
  -aicfRequirePlayerForResult 0 `
  -backendFreshSession `
  -maxFPS 60 `
  -logStats 10000
```

Окно №2:

```powershell
$gameRoot = "C:\Program Files (x86)\Steam\steamapps\common\Arma Reforger"
$repoRoot = "C:\Users\<имя>\IdeaProjects\Arma-Reforger-AI-Conflict"

Set-Location $gameRoot
& "$gameRoot\ArmaReforgerSteamDiag.exe" `
  -gproj "$repoRoot\AIConflictArland\addon.gproj" `
  -client 127.0.0.1 `
  -addonsDir "$repoRoot,$gameRoot\addons" `
  -addons "9178E5822AFE48EA,B52C5F6AEDBF423E"
```

Окно №3 для наблюдения:

```powershell
$log = Get-ChildItem "$profileRoot\logs" -Filter console.log -File -Recurse |
  Sort-Object LastWriteTime -Descending |
  Select-Object -First 1

Get-Content -LiteralPath $log.FullName -Wait |
  Select-String -SimpleMatch "[AICF][STAGE3]"
```

### Обязательное наблюдение T

Для US и USSR отдельно:

1. `VEHICLE_REQUESTED` появляется только у назначенного ATTACK-slot.
2. Перед spawn есть `VEHICLE_SPAWN_SITE_SELECTED` с friendly owner и `contested=0`.
3. Создан ровно один transport на сторону; prefab и entity указаны в `VEHICLE_SPAWNED`.
4. До выдачи GetIn измеряются все живые члены группы. `leader_m`, `nearest_m`, `farthest_m` и `member_samples` относятся к authoritative server positions; если `farthest_m > aicfVehicleMaximumReuseDistanceMeters`, ожидается `BOARDING_REJECTED reason=VEHICLE_TOO_FAR` и ни один GetIn/action не выдаётся.
5. Если `farthest_m > 75`, сначала выдаётся `BOARDING_PHASE_STARTED phase=APPROACH allowance=MOVE_ONLY_NO_VEHICLE_UTILITY`: машина не подключена к group utility, GetIn отсутствует, вся группа идёт одним обычным Move-waypoint к машине. Waypoint должен присутствовать в queue сразу и стать current не позднее 5 секунд; иначе ожидается `BOARDING_APPROACH_LOST` и bounded fallback. Exact-role фаза разрешена только после `BOARDING_APPROACH_COMPLETE ... farthest_m<=75`.
6. Если `farthest_m <= 75`, `APPROACH` не создаётся. В отдельном near-threshold сценарии проверить точки немного ниже и выше 75 м: первая сразу начинает role chain, вторая обязательно проходит MOVE-only staging без snap/teleport.
7. До exact `DRIVER` и повторно после завершения `APPROACH` синхронно нормализуется любой `mounted>0 && driver=0`: ограниченная цепочка `BOARDING_ROLE_RESET → BOARDING_ROLE_RETRY` либо `BOARDING_ROLE_VIOLATION → INFANTRY_FALLBACK`, но не продолжение посадки с неверным occupant.
8. Посадка проходит строго `DRIVER → optional GUNNER → PASSENGERS`: exact Pilot/Turret actions резервируют конкретные места; passenger `CARGO_ONLY` waypoint и group utility появляются лишь после settled обязательного crew.
9. План фиксируется один раз в `BOARDING_STARTED` и содержит только реально нужные фазы: обязательную `PASSENGERS`, при необходимости `APPROACH`, `DRIVER` и для armed-light `GUNNER`; максимум 3 фазы для transport и 4 для armed-light. Каждая фаза получает soft timeout, immutable total равен `planned_phases × timeout`.
10. После soft deadline допускается ровно одна grace 10 секунд на всю попытку и только если есть target-scoped `getting_in` либо свежий физический progress именно к этой машине. Grace не перекатывается между фазами; phase/total hard deadline равны соответствующему soft cap `+10 секунд`.
11. `BOARDING_COMPLETE mounted=<alive>` появляется только после двух последовательных settled poll всех живых бойцов именно в этой машине и до `VEHICLE_ROUTE_ASSIGNED`.
12. Маркер лидера показывает `VEH BOARDING`, затем `VEH MOVING` и `VEH DISEMBARKING`.
13. Машина действительно движется к objective; `VEHICLE_MOTION` подтверждает физическое перемещение, даже когда дорожный объезд временно не сокращает остаток route.
14. `VEHICLE_PROGRESS` отражает только чистое сокращение дистанции до road/direct route endpoint. Эта дистанция не является длиной дорожного пути и на объезде может временно расти.
15. Высадка происходит примерно на настроенной дистанции до того же тактического capture point, который получит пехотный приказ, но не внутри опасной/заблокированной геометрии.
16. Если protected occupants остаются внутри на половине dismount deadline, GetOut выдаётся повторно ровно один раз (`DISEMBARK_REISSUED`). На полном deadline отдельный `DISEMBARK_TIMEOUT` защёлкивает acceptance failure и запускает bounded fallback.
17. До `DISEMBARK_COMPLETE` все protected members текущей группы (`ALIVE` и `INCAPACITATED`) находятся вне машины. Только затем они получают пехотный order и продолжают objective пешком.
18. Группа способна захватить обычную базу или выполнить штатный relay-order; правила relay не обходятся техникой.
19. После завершения каждого configured vehicle-slot обеих сторон допустим только `RESULT_CANDIDATE status=READY ... final=0 requires_log_review=1`. Это не PASS: поздний acceptance failure должен дать `status=INVALIDATED`, а итог определяется после проверки полного остановленного прогона.

## Прогон A — лёгкая вооружённая техника

Запускается новым процессом и профилем только после PASS transport. В серверной команде заменить:

```powershell
-aicfTransportVehiclesPerFaction 0 `
-aicfArmedLightVehiclesPerFaction 1 `
-aicfMaxVehiclesPerFaction 1
```

Обязательные дополнительные проверки:

1. Выбран именно `ARMED_LIGHT` prefab штатного faction catalog.
2. До начала движения есть живые `DRIVER_ASSIGNED` и `GUNNER_ASSIGNED`.
3. `BOARDING_COMPLETE ... driver=1 gunner=1`.
4. Машина не создаётся без доступного pilot/turret compartment; такой prefab приводит к безопасному пешему fallback.
5. После высадки стрелок также выходит, группа получает пехотный приказ, машина не становится второй независимой AI-группой.
6. Потеря стрелка вызывает ограниченную попытку `GUNNER_REASSIGNED`; исчерпание recovery budget заканчивается `INFANTRY_FALLBACK`.

## Прогон R — recovery и отсутствие прогресса

Каждый дефект лучше воспроизводить отдельным новым профилем. Допускается ускоренная конфигурация:

```powershell
-aicfVehicleBoardingTimeoutMs 10000 `
-aicfVehicleStuckTimeoutMs 30000 `
-aicfVehicleProgressMeters 100 `
-aicfVehicleMotionMeters 3 `
-aicfVehicleObjectiveProgressTimeoutMs 60000 `
-aicfVehicleMaxRecoveries 1 `
-aicfVehicleCleanupDelayMs 10000
```

Проверяются сценарии:

| Сценарий | Как доказать | Ожидаемый результат |
|---|---|---|
| Boarding не завершён | Заблокировать подход либо занятие места до soft/hard deadline текущей фазы или общего cap | При свежем target-scoped progress допустим ровно один `BOARDING_TRANSITION_GRACE`; затем один `BOARDING_TIMEOUT` с `phase`, точной `cause` (`APPROACH_NOT_COMPLETE`, `DRIVER_NOT_ASSIGNED`, `GUNNER_NOT_ASSIGNED` или `PASSENGERS_NOT_MOUNTED`), состоянием группы, `planned_phases`, phase/total age и `deadline_scope`; после hard cap — высадка оставшихся и `INFANTRY_FALLBACK` |
| Driver погиб/вышел | Обычным игровым уроном вывести водителя из строя, не уничтожая всю группу | `DRIVER_LOST → VEHICLE_RECOVERY_STARTED role=PILOT mode=DIRECT_ROLE_ACTION → DRIVER_REASSIGNED` либо bounded `VEHICLE_RECOVERY_FAILED → INFANTRY_FALLBACK`; выбранный живой агент получает зарезервированный `Pilot` slot и один точный synchronous `SCR_AIGetInVehicle` token |
| Gunner погиб | Только armed-run; вывести стрелка из строя | `GUNNER_LOST → VEHICLE_RECOVERY_STARTED role=TURRET mode=DIRECT_ROLE_ACTION → GUNNER_REASSIGNED` либо пеший fallback; водитель исключён из кандидатов, а recovery использует сохранённый synchronous `SCR_AIGetInVehicle` token, не `SendGetInMessage` |
| Нет физического движения | Наблюдать неподвижную машину дольше `aicfVehicleStuckTimeoutMs`; не телепортировать её | `VEHICLE_STUCK_DETECTED reason=NO_PHYSICAL_MOVEMENT`, не более configured recoveries, затем `VEHICLE_MOTION`/progress или fallback |
| Движение без objective progress | Машина физически движется, но дольше `aicfVehicleObjectiveProgressTimeoutMs` не сокращает route endpoint | `VEHICLE_STUCK_DETECTED reason=NO_OBJECTIVE_PROGRESS`; recovery считается успешным только после route progress, одного физического движения недостаточно |
| Машина перевёрнута/неподвижна | Зафиксировать фактическое состояние видео/скриншотом | Немедленный отказ от машины и продолжение пешком |
| Машина уничтожена | Уничтожить транспорт, сохранив хотя бы одного живого члена managed-группы | `VEHICLE_DESTROYED`, прежний infantry slot/reinforcement contract остаётся корректным |
| Protected occupant не вышел | Оставить в машине `INCAPACITATED` managed member; отдельно проверить foreign `ALIVE`/`INCAPACITATED` occupant | `INCAPACITATED` и foreign не force-eject-ятся. На hard deadline: один `FALLBACK_DISEMBARK_FAILED`, primary cause получает suffix, runtime становится `ABANDONED` с restore pending; infantry order ждёт выхода managed protected members, cleanup ждёт отсутствия любого protected occupant |
| Цель сменилась в пути | Дождаться реального base owner change другой группой | Существующая машина получает новый `VEHICLE_ROUTE_ASSIGNED reason=STRATEGIC_TARGET_CHANGED` либо высаживает группу, если новый target уже близко; новая машина для retarget не создаётся |
| Группа уничтожена | Уничтожить всех членов | Штатный `GetOnEmpty → reinforcement → ticket debit`; vehicle runtime становится terminal и не привязывается к новому generation |

Game Master/teleport не является доказательством нормального движения или захвата. Если он используется только для создания fault condition, это явно записывается в отчёте; причинная последовательность recovery всё равно подтверждается серверным логом.

## Прогон L — лимиты, reuse и cleanup

1. Запустить с двумя requested vehicle-slots, но `aicfMaxVehiclesPerFaction=1`.
2. До cleanup на стороне существует не более одной active/reserved/abandoned машины.
3. Заблокированный slot пишет `VEHICLE_CAP_BLOCKED` один раз, а не каждый tick.
4. После dismount близкая группа может повторно использовать ту же машину; `vehicle_generation` увеличивается, новая entity не создаётся.
5. Далёкая, уничтоженная или небезопасная машина получает `VEHICLE_ABANDONED/DESTROYED`, затем `VEHICLE_CLEANUP`.
6. Cleanup не вызывает `DeleteRplEntity` при любом protected occupant (`ALIVE` или `INCAPACITATED`) в любом compartment, включая foreign group/faction. Forced exit разрешён только для `ALIVE` членов managed-группы; `INCAPACITATED` и foreign не выталкиваются. После выхода/смерти всех protected occupants cleanup завершается и cap снова доступен.
7. Если terminal runtime уже не принадлежит текущей группе, `slot.GetSpawnGeneration()` вырос и replacement-группа действительно готова начать vehicle trip, пустая старая машина может быть удалена до обычного grace-периода. Ожидается `VEHICLE_CLEANUP reason=REPLACEMENT_CAPACITY_REQUIRED`. Cooldown хранит owner generation: новое поколение отбрасывает timestamp старого до eligibility-check. На expedited path пара timestamp/generation сбрасывается только после protected-occupant gate, фактического удаления и identity-safe очистки runtime; наличие protected occupant запрещает раннее удаление.
8. В логе нет двух `VEHICLE_SPAWNED` с одной парой `faction/slot/group_generation/vehicle_generation`.
9. За 30 минут число vehicle entities и waypoint не растёт монотонно после завершённых cleanup-циклов.

## Регрессия Stage 2

Запустить отдельный baseline с:

```powershell
-aicfVehiclesEnabled 0
```

Критерии:

- нет `VEHICLE_REQUESTED` и vehicle entity;
- все восемь групп получают обычные пехотные orders;
- order recovery, cohesion, stuck lifecycle, replacement, tickets и victory работают по `STAGE_2_TESTING.md`;
- маркер показывает `VEH ON_FOOT`;
- нет `[AICF][STAGE3][ERROR]`.

## Лог-контракт

Каждая строка `[AICF][STAGE3]` содержит общий `run`, `t_ms`, `faction`, `slot`, `group_generation`, `vehicle_generation`, `vehicle`, `kind`, `state` и `reason`, если событие относится к конкретной машине.

Обязательные события:

```text
CONFIG
VEHICLE_STATE_CHANGED
VEHICLE_REQUESTED
VEHICLE_SPAWN_SITE_SELECTED / VEHICLE_SPAWN_SITE_REJECTED / VEHICLE_SPAWN_DEFERRED
VEHICLE_SPAWNED
VEHICLE_ASSIGNED
DRIVER_ASSIGNED / GUNNER_ASSIGNED
PASSENGERS_ASSIGNED
BOARDING_STARTED / BOARDING_REJECTED / BOARDING_PHASE_STARTED
BOARDING_APPROACH_COMPLETE / BOARDING_APPROACH_LOST / BOARDING_PROGRESS
BOARDING_TRANSITION_GRACE / BOARDING_COMPLETE / BOARDING_TIMEOUT
BOARDING_ROLE_RESET / BOARDING_ROLE_RETRY / BOARDING_ROLE_VIOLATION
VEHICLE_ROUTE_ASSIGNED / VEHICLE_PROGRESS / VEHICLE_MOTION
DISEMBARK_STARTED / DISEMBARK_REISSUED / DISEMBARK_TIMEOUT / DISEMBARK_COMPLETE
VEHICLE_STUCK_DETECTED
VEHICLE_RECOVERY_STARTED / VEHICLE_RECOVERY_SUCCEEDED / VEHICLE_RECOVERY_FAILED
DRIVER_LOST / DRIVER_REASSIGNED / GUNNER_LOST / GUNNER_REASSIGNED
DISEMBARK_CLEARANCE_RECOVERY
FALLBACK_FORCE_DISEMBARK / FALLBACK_DISEMBARK_FAILED
VEHICLE_ABANDONED / VEHICLE_DESTROYED
VEHICLE_DELETE_REQUESTED / VEHICLE_DELETE_RETRIED / VEHICLE_DELETE_NOT_CONFIRMED
VEHICLE_CLEANUP_CONFIRMED / VEHICLE_CLEANUP
INFANTRY_FALLBACK
VEHICLE_CAP_BLOCKED
HEARTBEAT
RESULT_CANDIDATE / ACCEPTANCE_FAILURE_LATCHED / RESULT
```

Повторяющийся warning с одинаковыми faction/slot/generations/reason каждый tick является дефектом, даже если поездка позднее завершилась.

`BOARDING_STARTED` фиксирует `phase_timeout_ms`, immutable `planned_phases`/`total_timeout_ms`, leader-distance, `nearest_m`, `farthest_m`, прерванные старые actions и per-member samples. План содержит обязательную `PASSENGERS` и только реально нужные `APPROACH`, `DRIVER`, optional `GUNNER`: максимум 3 фазы transport и 4 armed-light. Если `farthest_m > 75`, сначала создаётся обычный Move-waypoint `APPROACH` без vehicle utility/GetIn; он должен оставаться в queue и стать current в пределах 5 секунд. Driver и gunner затем получают точные `SCR_AIGetInVehicle` action на заранее зарезервированный Pilot/Turret slot; переход роли подтверждается только после физического settled-state. Машина подключается к group utility и получает `CARGO_ONLY` лишь после settled полного обязательного crew.

Каждая начатая фаза имеет soft deadline, общий clock не сбрасывается. На всю boarding-попытку допускается ровно одна `BOARDING_TRANSITION_GRACE` длительностью 10 секунд только при target-scoped `getting_in` или свежем физическом progress; phase/total hard cap остаётся абсолютным. `BOARDING_TIMEOUT` обязан содержать текущую фазу, точную причину, число живых/посаженных, состояние driver/gunner, `planned_phases`, phase/total age, `deadline_scope`, physical maxima, waypoint state и per-member samples. `BOARDING_COMPLETE` требует двух подряд settled poll всех живых членов именно в этой машине.

До `SpawnEntityPrefabEx` spawner детерминированно обходит все safe friendly base по расстоянию и ключу. Для каждого кандидата до создания проверяются максимальная spawn-дистанция, точный empty-terrain site и расстояние всех живых бойцов до этого site. Entity создаётся только для полностью допустимого кандидата; отсутствие живых бойцов даёт retryable `GROUP_NOT_READY` без spawn.

Обычная посадка AICF не вызывает teleport-in: `APPROACH` использует только Move-waypoint, обязательные роли — штатный exact-role action, пассажиры — stock `SCR_BoardingWaypoint` с `CARGO_ONLY`. Teleport API используется только наружу при bounded physical-clearance recovery/forced exit. В T8/A2 отсутствие snap должно подтверждаться последовательными authoritative `member_samples`/`BOARDING_PROGRESS` и видео клиента, а не только конечным occupant-state.

`SAFE_REUSE` перед новой role-ordered посадкой освобождает старые compartment reservations и прерывает vehicle action queue всех живых managed members. Состояние `mounted > 0 && driver=0` нормализуется синхронно до первой exact-role action и повторно сразу после `APPROACH`: старый waypoint удаляется, живые managed occupants высаживаются, затем exact `DRIVER` выдаётся повторно. Повторное нарушение или истечение reset deadline даёт `BOARDING_ROLE_VIOLATION`, acceptance failure и пеший fallback.

`DISEMBARK_REISSUED` означает одноразовую очистку stale vehicle actions/waypoint и повторную выдачу штатного GetOut на половине `aicfVehicleBoardingTimeoutMs`. Завершение требует двух последовательных чистых poll: отсутствуют logical vehicle/compartment link и get-in/get-out transition, а персонаж находится вне ориентированных bounds машины. Логически вышедший, но физически оставшийся внутри bounds живой managed member получает ограниченное relocation наружу (`DISEMBARK_CLEARANCE_RECOVERY`). Если к полному deadline clearance не подтверждён, `DISEMBARK_TIMEOUT` фиксирует `logical/transitions/inside_bounds`, защёлкивает acceptance failure и только затем запускает fallback.

Crew recovery резервирует конкретный role slot и синхронно создаёт один точный `SCR_AIGetInVehicle` action-token. Runtime хранит именно этот token: abort/fallback завершает только его, а успешное settled-занятие места снимает tracking без `Fail()`. При одновременной потере ролей driver восстанавливается первым, затем gunner; `VEHICLE_RECOVERY_SUCCEEDED reason=ALL_REQUIRED_CREW_RESTORED` допустим только после повторной проверки полного обязательного crew. `SendGetInMessage`/`SendCancelMessage` с `relatedActivity=null` для recovery не допускаются.

При fallback сначала используется штатный animated get-out waypoint. После bounded deadline forced teleport/no-door exit применяется только к `ALIVE` членам текущей managed-группы; `INCAPACITATED` и foreign occupants не force-eject-ятся. Попытки ограничены hard deadline `2 × aicfVehicleBoardingTimeoutMs`; успешная принудительная высадка отмечается единичным `FALLBACK_FORCE_DISEMBARK`.

Если к hard deadline protected member текущей группы всё ещё внутри, один `FALLBACK_DISEMBARK_FAILED` дополняет исходную terminal cause, runtime конечным образом переходит в `ABANDONED`, записывает slot terminal failure/suppression и выставляет infantry restore pending. Пехотный приказ восстанавливается только после `AreAllProtectedMembersOutOfVehicle()` для текущего group generation. Cleanup отдельно сканирует все compartments: любой `ALIVE`/`INCAPACITATED`, включая foreign occupant, блокирует `DeleteRplEntity`; hard failure не зацикливается и не удаляет защищённого персонажа вместе с машиной.

Cleanup не освобождает runtime/cap в tick delete-запроса. `VEHICLE_DELETE_REQUESTED` сохраняет `entity_id`, `rpl_id`, origin и attempt; authority ограниченно повторяет удаление, а `VEHICLE_CLEANUP_CONFIRMED` появляется только после того, как тот же `EntityID` больше не разрешается. Через 10 секунд живой authority entity даёт `VEHICLE_DELETE_NOT_CONFIRMED` и acceptance failure. Это не client acknowledgement: возможный визуальный despawn desync должен быть проверен в Armed A2 по тем же `EntityID`/`RplId`.

Пехотный order recovery пишет `ORDER_RECOVERY_ISSUED`, а `ORDER_RECOVERED` — только после трёх наблюдений, где exact тот же waypoint одновременно остаётся current и присутствует в queue, и не раньше `max(10 с, 2 × reliability interval)`. `ORDER_RECOVERY_STABILITY` фиксирует число poll, длительность, queue/current context и решение durability; нестабильный кандидат расходует bounded stuck budget и в пределе вызывает recycle вместо бесконечного повтора каждые 5 секунд.

`VEHICLE_PROGRESS` означает чистое сокращение `route_distance_m` до road/direct endpoint минимум на `aicfVehicleProgressMeters`. `VEHICLE_MOTION` означает физическое перемещение минимум на `aicfVehicleMotionMeters` без такого чистого сокращения. Эти события обслуживают независимые objective/stationary deadline. Dismount проверяется по `target_distance_m` до тактического capture point.

Recovery после `NO_PHYSICAL_MOVEMENT` может завершиться физическим movement (или route progress). Recovery после `NO_OBJECTIVE_PROGRESS` требует route progress; езда без приближения к endpoint не создаёт ложный `VEHICLE_RECOVERY_SUCCEEDED`.

## Текущая матрица приёмки

Матрица отражает последний runtime (Transport T7, Armed A1) и post‑T7-кандидат. Исторические T1–T6 не являются актуальным доказательством и ниже не разворачиваются.

| Проверка | Transport | Armed | Что требуется дальше |
|---|:---:|:---:|---|
| Workbench 1.7 validation / static audit | PASS | PASS | Повторять на точном commit перед runtime |
| Safe spawn, faction и configured cap | PASS | PARTIAL | A2: подтвердить вооружённые машины обеих сторон и cap fault |
| Строгий DRIVER → cargo | FAIL | FAIL | T8/A2: ни одного compartment до exact driver |
| Строгий DRIVER → GUNNER → cargo | N/A | FAIL | A2: обе роли settled до passenger phase |
| Полная посадка всех живых | FAIL | FAIL | T8/A2: new vehicle и SAFE_REUSE у US/USSR |
| Движение и route progress | PASS | PARTIAL | Подтвердить после новой посадки без remote snap |
| Высадка и восстановление приказа | PARTIAL | NOT RUN | T8: два stable-clear poll и продолжение пехотой |
| Пехотный захват после высадки | NOT RUN | NOT RUN | Наблюдать owner change после штатной высадки |
| Bounded boarding/recovery/fallback | PASS | FAIL | Fault-run без churn и преждевременного recovery |
| Reuse и vehicle generation | FAIL | FAIL | Повторное использование без нарушения ролей |
| Cleanup / authority delete | PARTIAL | PARTIAL | A2: сопоставить server/client EntityID и RplId |
| Stuck/recovery и order durability | PARTIAL | PARTIAL | Отдельный fault-run, три stable poll и ≥10 с |
| 30 минут без роста сущностей/ошибок | NOT RUN | NOT RUN | После T8/A2 |
| Полные server/client доказательства | PASS | PARTIAL | Для A2 обязателен клиентский despawn probe |

### Архив T1–T6

Подробные журналы, команды, профили, временные срезы и пути к артефактам T1–T6 удалены как неактуальные. Все шесть прогонов исторически завершились FAIL. Они последовательно выявили уже закрытые или заменённые классы дефектов: faction-init mismatch и warning churn, ложный immobility, abandoned-spam, захват пассажирского места до водителя, общий deadline фаз, преждевременный RESULT PASS и ложный dismount completion. Для текущей приёмки источниками истины являются T7, Armed A1 и следующие T8/A2.

### Transport T7 — актуальный runtime FAIL

- US SAFE_REUSE начался на расстоянии 114–128 м. Водитель занял pilot seat через 43 с; затем пассажирская фаза получила собственные 60 с и завершилась при mounted=2/3. Последний солдат визуально входил, но сервер ещё не считал его settled. Итог: BOARDING_TIMEOUT, acceptance failure, forced dismount и пехотный fallback.
- USSR SAFE_REUSE начался на расстоянии 143–151 м. Водитель сел через 6 с, но к hard deadline пассажирской фазы осталось mounted=1/3. Итог тот же: bounded timeout, infantry order и подтверждённый cleanup.
- Общая причина: пассажиры не подходили к машине до подтверждения водителя, а штатный поиск посадки ограничен примерно 100 м. Простое увеличение тайм-аута не устраняет дальний последовательный подход.
- T7 также подтвердил, что прежний ORDER_RECOVERY стал bounded, но двух ранних stable poll было недостаточно: новый waypoint позднее снова исчезал, после чего группа перерабатывалась.

### Armed A1 — актуальный runtime FAIL

- PILOT_ONLY не исключил раннее занятие gunner/cargo; strict role ordering нарушался у обеих фракций.
- Crew recovery был недостаточно прозрачен и мог завершаться без доказательства восстановления всех обязательных ролей.
- Машина могла создаваться далеко от группы и только после создания отклоняться как VEHICLE_TOO_FAR.
- Возможный client despawn desync остаётся PARTIAL: authority delete подтверждался, но клиентского EntityID/RplId доказательства нет.
- Infantry waypoint recovery давал повторный WAYPOINT_NOT_CURRENT churn; это исправлено только в post-A1-кандидате.

### Post‑T7-кандидат

Кандидат не меняет штатные мины Arland и пока не имеет runtime PASS.

- При farthest member >75 м запускается MOVE-only APPROACH всей группы. Vehicle utility и GetIn actions до завершения подхода отсутствуют.
- APPROACH waypoint получает 5 с на активацию в queue; потеря/неактивация ограниченно завершает попытку и фиксирует acceptance failure.
- После staging действует exact DRIVER → optional exact GUNNER → PASSENGERS. Уже занятые неправильные места синхронно нормализуются до выдачи DRIVER action, в том числе после APPROACH_COMPLETE.
- Общий budget вычисляется один раз из реально запланированных фаз: transport максимум 3, armed максимум 4. Каждая фаза имеет soft timeout; один sticky grace +10 с разрешён только при target-scoped getting-in или свежем физическом progress. Общий hard cap также не расширяется более чем на 10 с.
- BOARDING_COMPLETE требует два последовательных poll, где все живые участники settled в целевой машине.
- ORDER_RECOVERED требует exact current+queued waypoint в трёх последовательных наблюдениях и не раньше max(10 с, 2 × reliability interval). Нестабильность расходует bounded stuck budget.
- `tools/Test-Stage3Static.ps1`: PASS.
- Workbench 1.7.0.54: `.cache/stage3-t7-boarding-final2-20260809-164902/console.log`, Game CRC32 `1d647a62`, пять конфигураций, `Script validation successful`, `SCRIPT (E/F)=0`.

### Transport T8 — текущий runtime FAIL

Текущий runtime: `C:\Users\retar\AppData\Local\AICF\Stage3-Transport-T8-Morning-20260809-170234`. USSR generation=1 штатно завершил посадку `3/3`, доехал до dismount-порога цели 48 и безопасно высадился. В 17:06:21 восстановлен пехотный ATTACK-приказ на ту же цель 48, без `TARGET_REASSIGNED`. Сразу после высадки группа визуально пошла в обратную сторону, но дошла до промежуточной точки, развернулась и продолжила правильно к цели. Классификация: штатный navmesh-обход, не дефект post-dismount маршрута.

- Initial boarding обеих сторон прошёл штатно `3/3`. СССР завершил первую поездку и затем успешный `SAFE_REUSE`: общий `APPROACH` с 148–154 м завершился при farthest=71.8 м, после чего строго прошли DRIVER → PASSENGERS, вторая поездка и безопасная высадка. Одному бойцу потребовалась bounded `DISEMBARK_CLEARANCE_RECOVERY relocated=1`.
- US generation=1 подорвался/загорелся в первой поездке; `VEHICLE_ON_FIRE` штатно завершился fallback, подтверждённым cleanup и созданием replacement-группы с новой машиной. Replacement generation=2 полностью сел, доехал, безопасно высадился; clearance recovery переместил трёх логически вышедших бойцов из bounds машины.
- После этих первых завершённых поездок появился `RESULT_CANDIDATE status=READY final=0`, но это не финальный PASS.
- US `SAFE_REUSE` в 17:13:27 начал запланированный `APPROACH` с 130–132 м. Waypoint был current+queued, однако за 60 секунд farthest сократился только примерно до 120 м и не достиг порога 75 м. В 17:14:27 получены `BOARDING_TIMEOUT phase=APPROACH cause=APPROACH_NOT_COMPLETE`, `ACCEPTANCE_FAILURE_LATCHED` и `RESULT_CANDIDATE status=INVALIDATED`; группа перешла в пеший fallback, исправная машина стала abandoned. Итог T8: `FAIL`.
- Ручное наблюдение US уточняет характер отказа: во время активного `APPROACH` боты остановились и продолжительное время стояли; сразу после timeout, отказа от машины и восстановления обычного пехотного приказа они снова начали нормально двигаться. Это локализует дефект в MOVE-only подходе к машине/его целевой navmesh-позиции или waypoint, а не в общей способности этой группы выполнять пеший маршрут. Для доработки нужны diagnostics доступности и проекции точки подхода на navmesh, фактического прогресса каждого участника и причины остановки при формально current+queued waypoint.
- Продолженное наблюдение: USSR vehicle generation=3 перевернулся (`VEHICLE_OVERTURNED`), после bounded fallback двое выживших были освобождены clearance recovery, а abandoned-машина штатно удалена. После захвата базы 31 для новой дальней цели 33 создан новый «Урал» рядом с двумя живыми бойцами; оба settled, машина начала маршрут и даёт progress.
- US после fallback оказался сильно разорван: при попытке создать новый транспорт около базы 55 лидер был в 27 м от boarding site, а два остальных участника — в 622 и 677 м. Проверка всей группы корректно отклоняет такую машину как `NO_BOARDING_SITE_WITHIN_RANGE maximum_m=250`, но запрос затем повторяется каждые 10 секунд с переходами `REQUESTED → SPAWNING → REQUESTED`. Зафиксировать для доработки: восстановление cohesion/сбор группы перед новым vehicle request и ограничение повторяющегося spawn state/log churn, когда безопасной точки для всей группы нет.
- Позднее US-группа визуально самостоятельно воссоединилась после обычного пехотного движения и `GROUP_STUCK_RECOVERY`. Новых per-member samples после последнего spawn rejection лог не дал, поэтому точная конечная дистанция не подтверждена сервером; классификация остаётся дефектом временного cohesion loss, который несколько минут блокировал транспорт, а не постоянной потерей членов группы.
- В 17:35:29 US захватил базу 48 и получил новую дальнюю цель 31, после чего визуально перешёл в статус запроса транспорта. Однако сервер продолжил прежний 10-секундный churn `SPAWNING → REQUESTED` без `VEHICLE_SPAWNED/ASSIGNED`; после throttled `NO_BOARDING_SITE_WITHIN_RANGE` новые переходы уже не содержат конкретной причины отказа. Даже после воссоединения и захвата новой friendly-базы запрос не восстанавливается автоматически. Нужны bounded retry/backoff, повторная полная диагностика при изменении strategic/base context и явное ожидание допустимой boarding site вместо постоянного state churn.

#### Дефект T8-US-1: запрос новой машины не восстанавливается после временной потери cohesion

- **Предусловие:** US generation=2 завершил поездку и запросил `SAFE_REUSE` исправной M923A1.
- **Последовательность:** MOVE-only `APPROACH` остановился примерно в 120 м от машины и завершился `BOARDING_TIMEOUT_APPROACH_NOT_COMPLETE`; машина была брошена. На последующем пехотном маршруте группа временно разделилась: один участник оказался в 27 м от candidate boarding site, двое других — в 622 и 677 м.
- **Первичный отказ:** новая машина корректно не создана из-за `NO_BOARDING_SITE_WITHIN_RANGE`, поскольку установленный предел 250 м проверяется по каждому живому участнику.
- **Невосстановившееся состояние:** позднее все трое визуально воссоединились, US захватил базу 48 и получил новую дальнюю цель 31, однако машина не появилась. Runtime продолжил бесконечные переходы `REQUESTED → SPAWNING → REQUESTED` каждые 10 секунд без `VEHICLE_SPAWNED`, `VEHICLE_ASSIGNED`, bounded fallback или новой подробной причины.
- **Повторное воспроизведение:** в 17:46:45 US захватил следующую базу 31 и получил новую дальнюю цель 60. Ни изменение ownership, ни второй `TARGET_REASSIGNED` не сбросили зависший request: в 17:46:53, 17:47:03, 17:47:14, 17:47:24 и далее снова записаны только `SPAWNING → REQUESTED`, без созданной машины. Последняя повторно выведенная подробная причина до этого — `NO_BOARDING_SITE_WITHIN_RANGE` для базы 19 с distances 1205–1351 м; после захвата 31 актуальная причина снова подавлена.
- **Фактический результат:** группа остаётся без транспорта и не может начать посадку, хотя исходная причина — разрыв участников — визуально устранена и strategic/base context изменился.
- **Ожидаемый результат:** после восстановления cohesion либо появления новой допустимой friendly-базы координатор заново измеряет всех участников и перечень spawn sites, создаёт машину при валидной точке; если валидной точки всё ещё нет — пишет актуальную диагностическую причину и использует bounded backoff/пехотный исход без постоянного state/log churn.

#### Дефект T8-CLEANUP-1: удаление abandoned-машины во время посадки игрока зависает персонажа

- **Severity:** critical для gameplay/client state; cleanup safety FAIL.
- **Шаги воспроизведения:** игрок уничтожил сопровождавших USSR-ботов у оставленного «Урала» `EntityID=0x4000000000001307`, затем начал ручную посадку в машину. Во время enter-анимации машина исчезла, персонаж остался в зависшем interaction/animation state.
- **Серверная последовательность:** после `DISEMBARK_COMPLETE` группа потеряла combat-ready state, в 17:40:51 машина стала `ABANDONED reason=GROUP_NOT_COMBAT_READY`. В 17:41:21.989 выдан `VEHICLE_DELETE_REQUESTED reason=REPLACEMENT_CAPACITY_REQUIRED`; в 17:41:23.006 — `VEHICLE_CLEANUP_CONFIRMED` для того же `EntityID=0x4000000000001307`, `RplId=-2147470878`.
- **Клиентская корреляция:** клиент получил RPL-событие удаления в 17:41:22, то есть внутри наблюдаемой анимации входа. В серверном cleanup-логе отсутствует protected-occupant/interaction defer для игрока.
- **Причина-кандидат:** compartment scan защищает уже связанного occupant, но персонаж в начатой enter-анимации ещё не зарегистрирован в compartment/vehicle и не удерживает delete gate. Между последней safety-проверкой и authority delete также отсутствует устойчивое подтверждение, что рядом нет нового `getting_in` interaction.
- **Ожидаемый результат:** abandoned vehicle нельзя удалять, пока любой игрок или AI выполняет target-scoped get-in/get-out interaction либо находится в зоне активного vehicle action. Нужен server-authoritative interaction reservation/delete lock, повторная проверка непосредственно перед `DeleteRplEntity`, короткий stable-clear window и безопасная отмена/восстановление персонажа при race. Cleanup должен писать причину defer и данные foreign/player protection.
- **Рекомендуемая lifecycle-политика:** исправный abandoned-транспорт не удалять автоматически ради replacement capacity. Снять его с управления Stage 3 и исключить из лимита активных/reserved AI-машин либо учитывать в отдельном ограниченном world pool, оставив доступным игрокам и для возможного safe reuse. Автоматическое удаление применять к уничтоженной/непригодной технике или после длительного TTL, но только после устойчивого stable-clear подтверждения отсутствия occupants, активных get-in/get-out interactions и игроков в защитном радиусе. Нужны отдельный предел мирового пула и oldest-safe cleanup, чтобы сохранение техники не приводило к неограниченному росту сущностей.

1. Проверить US и USSR, initial vehicle и SAFE_REUSE.
2. При farthest >75 м сначала получить APPROACH без vehicle utility/GetIn; waypoint должен стать queued/current, farthest — уменьшаться, COMPLETE — произойти не дальше 75 м.
3. До DRIVER_ASSIGNED должно быть mounted=0. После него transport переходит только в PASSENGERS.
4. Все живые должны стать settled в двух последовательных poll без role reset/violation и без remote snap.
5. На границе soft deadline допускается ровно один grace только с target-scoped доказательством; hard outcome — не позже phase/total +10 с.
6. Отдельный no-progress fault обязан дать ровно один BOARDING_TIMEOUT и bounded fallback.
7. После штатной высадки подтвердить пехотное движение и реальный захват.
8. ORDER_RECOVERED не должен сопровождаться тем же WAYPOINT_NOT_CURRENT внутри durability window.

### Armed A2 и отдельные fault-срезы

- Доказать exact DRIVER → GUNNER → PASSENGERS у US и USSR, включая SAFE_REUSE и потерю каждой crew role.
- Сопоставить server/client EntityID и RplId при delete; визуально оставшаяся машина без совпадающей identity не считается desync.
- Проверить cap, occupied cleanup, overturned/destroyed fallback и 30-минутную стабильность.
- Stage 3 остаётся непринятым до успешных T8, A2, fault-срезов и последующего 30-минутного прогона.

## PASS / FAIL / BLOCKED

`PASS`:

- transport и armed-light срезы выполнены на одном commit;
- все применимые строки матрицы подтверждены;
- recovery ограничен и всегда заканчивается progress либо пехотным fallback;
- лимит и cleanup соблюдаются;
- Stage 2 disabled regression чист;
- получен `RESULT_CANDIDATE status=READY final=0`, затем до остановки полного окна нет `status=INVALIDATED` и `ACCEPTANCE_FAILURE_LATCHED`;
- нет `[AICF][STAGE3][ERROR]`, `SCRIPT (E/F)`, duplicate spawn или бесконечного warning churn.

`FAIL`: Stage 3 загрузился, но нарушен любой инвариант: unsafe/double spawn, неполная посадка без fallback, движение без водителя, преждевременный infantry order до выхода protected members, бесконечный recovery/fallback, spawn сверх cap, stale generation, удаление машины с `ALIVE`/`INCAPACITATED` occupant (включая foreign) или утечка entity/waypoint.

`BLOCKED`: несовместимая версия, addon/resource не загружен, compile error, занят порт или окружение не позволяет начать нужный сценарий. `BLOCKED` не является PASS.

После Stage 3 PASS всё ещё отдельно выполняются полная MVP-матрица и двухчасовой soak с техникой; текущая реализационная работа их не запускала.

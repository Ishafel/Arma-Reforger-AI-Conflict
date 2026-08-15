# Stage 3 — наземная техника

Текущий статус Stage 3: **FAILED — REWRITE IMPLEMENTATION/CUTOVER COMPLETE, RUNTIME ACCEPTANCE NOT PASSED**. Новое решение владельца отменяет прежнюю заочную фиксацию post-T9 snapshot: legacy vehicle runtime/coordinator fragments удалены и новый vehicle-domain включён единственным active side-effect path, но требования этапа ещё не подтверждены controlled runtime. Stage 3 по-прежнему добавляет транспорт существующим managed-slot и не меняет билеты, `OnEmpty`, правила победы или `CaptureRelay`.

Все исторические результаты ниже сохраняются без переклассификации: Transport T1–T9 и Armed A1 остаются `FAIL`; Transport T10, Armed A2 и незапущенные fault/soak-срезы остаются `NOT RUN`. Автоматическая компиляция, static audit и строка `[AICF][STAGE3][RESULT_CANDIDATE] ... status=READY final=0` являются только development evidence и не дают PASS ни прежней, ни новой архитектуре. Положительные наблюдения и safety-контракты этой матрицы обязательны для rewrite.

## Финальная development-проверка rewrite

Обе обязательные команды завершились `PASS`:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\Test-Stage3Static.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\Test-Stage35Static.ps1
```

Negative-fixture self-check подтвердил как минимум rule IDs `COORDINATOR_SIDE_EFFECT`, `FLOW_CROSS_CALL`, `WAYPOINT_SIDE_EFFECT_OWNER`, `TRANSITION_OUTSIDE_CONTROLLER`, `TRANSITION_EFFECT_ORDER`, `WAITING_WITH_LEASE`, `HANDOFF_CLEARANCE_GATE`, `CLEANUP_CLEARANCE_OWNER`, `CLEANUP_IDENTITY_SAFETY` и `VEHICLE_LIVENESS_OWNERSHIP`.

Полный Workbench validate: `.cache/vehicle-rewrite-final-validate-20260812-r2/console.log`; Game `5692` files / `11109` classes, CRC32 `7f2cbec0`, `Game successfully created`, `SCRIPT(E/F)=0`, `ENGINE(F)=0`. В полном журнале также сохранены два harness `PLATFORM(E)` (`SteamAPI_Init failed` и platform services) и 25 `RESOURCES(E)` строк shutdown resource-leak list, всего 27 generic `(E)/(F)` matches. Они классифицированы как Workbench platform/shutdown-resource caveat, не как AICF script compile error, и не являются runtime gameplay evidence.

Короткий final-tree headless development smoke использовал профиль `.cache/Stage35-Rewrite-FinalSmoke-20260812-002210` и журнал `.cache/Stage35-Rewrite-FinalSmoke-20260812-002210/logs/logs_2026-08-12_00-22-10/console.log`. Старт: `2026-08-12T00:22:10+03`; точный cutoff: `2026-08-12T00:22:51.7568935+03`. CLI содержал canonical Arland world/header/systems/addons, `-backendFreshSession`, roles `1`, max agents `64`, require player `0`, vehicles `1`, transport `4`, armed `0`, cap `4`, minimum request agents `3`. Engine создал Game, `SCRIPT(E/F)=0`, `ENGINE(F)=0`, VM exceptions `0`, но до AICF bootstrap возникло 12 `BACKEND(E)` (`SSL peer certificate` и `BAD_REQUEST`): `AICF=0`, roster/vehicle evidence отсутствует. Итог smoke: **BLOCKED внешним backend**. Это не Transport T10, не Repeat T/Repeat-T2 и не M30; gameplay/runtime acceptance не проверена. Commit/SHA не записан, потому что проверялось dirty working tree.

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

6. Любая AICF script/resource/dependency error означает `BLOCKED`. Stock/versioned error не считается AICF-дефектом автоматически, но сохраняется в отчёте, классифицируется по symbol/stack/resource context и не может быть молча исключён из acceptance evidence.
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
| `aicfVehicleMaxRecoveries` | `2` | Независимый верхний предел для crew-role recovery и mobility/unstuck recovery на поездку; смена экипажа не расходует и не сбрасывает physical-motion budget |
| `aicfVehicleDismountDistanceMeters` | `150` | Плановая дистанция высадки до тактического capture point, а не до road endpoint |
| `aicfVehicleRetryIntervalMs` | `10000` | Базовый интервал retryable safe-spawn запроса |
| `aicfVehicleSpawnMaxAttempts` | `4` | Число retryable spawn-attempt до перехода в cap-free `WAITING_FOR_SITE` |
| `aicfVehicleRetryBackoffMaxMs` | `60000` | Верхняя граница экспоненциального backoff между попытками |
| `aicfVehicleWaitProbeIntervalMs` | `60000` | Период полного site preflight в `WAITING_FOR_SITE`; target/base-context change будит запрос раньше |
| `aicfVehicleNoRangeProgressTimeoutMs` | `90000` | Bounded deadline отсутствия сокращения дистанции до ближайшей safe-base reference при `NO_BOARDING_SITE_WITHIN_RANGE` и нуле spawn-attempt; затем запрос возвращается к пехотному выполнению без spawn/teleport |
| `aicfVehicleCleanupDelayMs` | `60000` | Grace-период destroyed/unusable entity до protected cleanup |
| `aicfVehicleAbandonedWorldPoolPerFaction` | `4` | Safety-first soft target исправных abandoned-машин стороны вне active AI cap; pool может временно превышать его, пока player/interaction/proximity gate запрещает retirement |
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
5. Если `farthest_m > 75`, сначала выдаётся `BOARDING_PHASE_STARTED phase=APPROACH allowance=MOVE_ONLY_NO_VEHICLE_UTILITY`: машина не подключена к group utility, GetIn и group Move-waypoint отсутствуют. Каждый удалённый живой участник получает отдельный MOVE-only action к фактической машине с радиусом не больше 70 м; authoritative distance/progress отслеживается отдельно по member. При terminal action или 15 с без сокращения минимум на 2 м action переиздаётся ровно один раз; следующий stall даёт `BOARDING_APPROACH_MEMBER_STALLED`, acceptance failure и bounded fallback.
6. Если `farthest_m <= 75`, `APPROACH` не создаётся. В отдельном near-threshold сценарии проверить точки немного ниже и выше 75 м: первая сразу начинает role chain, вторая обязательно проходит per-member staging без snap/teleport. `BOARDING_APPROACH_COMPLETE ... farthest_m<=75` требует два последовательных settled poll всей группы.
7. До exact `DRIVER` и повторно после завершения `APPROACH` синхронно нормализуется любой `mounted>0 && driver=0`: ограниченная цепочка `BOARDING_ROLE_RESET → BOARDING_ROLE_RETRY` либо `BOARDING_ROLE_VIOLATION → INFANTRY_FALLBACK`, но не продолжение посадки с неверным occupant.
8. Посадка проходит строго `DRIVER → optional GUNNER → PASSENGERS`: exact Pilot/Turret actions резервируют конкретные места. После settled обязательного crew допускается group utility, но не passenger group waypoint; до первой passenger action создаётся атомарное сопоставление всех живых пассажиров с exact `CargoCompartmentSlot`, reservation и owner-token, затем каждому выдаётся отдельный `SCR_AIGetInVehicle`.
9. План фиксируется один раз в `BOARDING_STARTED` и содержит только реально нужные фазы: обязательную `PASSENGERS`, при необходимости `APPROACH`, `DRIVER` и для armed-light `GUNNER`; максимум 3 фазы для transport и 4 для armed-light. Каждая фаза получает soft timeout, immutable total равен `planned_phases × timeout`.
10. После soft deadline допускается ровно одна grace 10 секунд на всю попытку и только если есть target-scoped `getting_in` либо свежий физический progress именно к этой машине. Grace не перекатывается между фазами; phase/total hard deadline равны соответствующему soft cap `+10 секунд`.
11. `BOARDING_COMPLETE mounted=<alive>` появляется только после двух последовательных settled poll всех живых бойцов именно в этой машине и до `VEHICLE_ROUTE_ASSIGNED`.
12. Маркер лидера показывает `VEH BOARDING`, затем `VEH MOVING` и `VEH DISEMBARKING`.
13. Машина действительно движется к objective; `VEHICLE_MOTION` подтверждает физическое перемещение, даже когда дорожный объезд временно не сокращает остаток route.
14. `VEHICLE_PROGRESS` отражает только чистое сокращение дистанции до road/direct route endpoint. Эта дистанция не является длиной дорожного пути и на объезде может временно расти.
15. Высадка происходит примерно на настроенной дистанции до того же тактического capture point, который получит пехотный приказ, но не внутри опасной/заблокированной геометрии.
16. Если protected occupants остаются внутри на половине dismount deadline, GetOut выдаётся повторно ровно один раз (`DISEMBARK_REISSUED`). На полном deadline отдельный `DISEMBARK_TIMEOUT` защёлкивает acceptance failure и запускает bounded fallback.
17. `DISEMBARK_COMPLETE` требует, чтобы все protected members текущей группы (`ALIVE` и `INCAPACITATED`) вышли из машины. Если vehicle control прекращается по timeout/fallback/terminal-причине раньше полной clearance, meaningful infantry order для живой current-группы восстанавливается немедленно; pending logical link, get-in/get-out transition, oriented bounds или player blocker задерживает только lease release/delete, но не пешее продолжение задачи.
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
| Нет физического движения | Безопасно заблокировать исправную машину дольше `aicfVehicleStuckTimeoutMs`; бойцы должны быть settled, игроки/foreign occupants — вне защитной области | `VEHICLE_STUCK_DETECTED → VEHICLE_UNSTUCK_STARTED → VEHICLE_UNSTUCK_ATTEMPT`; authority-only reposition не дальше 15 м либо route-only attempt, затем успех только после нового `VEHICLE_MOTION`/route progress; каждая неподтверждённая попытка даёт `VEHICLE_UNSTUCK_FAILED`, и лишь после configured mobility attempts допустим fallback |
| Движение без objective progress | Машина физически движется, но дольше `aicfVehicleObjectiveProgressTimeoutMs` не сокращает route endpoint | `VEHICLE_STUCK_DETECTED reason=NO_OBJECTIVE_PROGRESS`; recovery считается успешным только после route progress, одного физического движения недостаточно |
| Машина перевёрнута/неподвижна | Зафиксировать фактическое состояние видео/скриншотом | Немедленный отказ от машины и продолжение пешком |
| Машина уничтожена | Уничтожить транспорт, сохранив хотя бы одного живого члена managed-группы | `VEHICLE_DESTROYED`, прежний infantry slot/reinforcement contract остаётся корректным |
| Protected occupant не вышел | Оставить в машине `INCAPACITATED` managed member; отдельно проверить foreign `ALIVE`/`INCAPACITATED` occupant | `INCAPACITATED` и foreign не force-eject-ятся. На hard deadline: один `FALLBACK_DISEMBARK_FAILED`, primary cause получает suffix, vehicle control прекращается и немедленно запускается доказуемое восстановление meaningful infantry order. Pending protected occupant удерживает lease release/delete до `clearance_safe`, но не удерживает `order_restored` и не оставляет живую current-группу без пешей задачи |
| Цель сменилась в пути | Дождаться реального base owner change другой группой | Существующая машина получает новый `VEHICLE_ROUTE_ASSIGNED reason=STRATEGIC_TARGET_CHANGED` либо высаживает группу, если новый target уже близко; новая машина для retarget не создаётся |
| Группа уничтожена | Уничтожить всех членов | Штатный `GetOnEmpty → reinforcement → ticket debit`; прежний Trip/Lease terminal identity не привязывается к replacement group generation и stale callback безопасно self-cancel |

Game Master/teleport не является доказательством нормального движения или захвата. Если он используется только для создания fault condition, это явно записывается в отчёте; причинная последовательность recovery всё равно подтверждается серверным логом.

## Прогон L — лимиты, reuse и cleanup

1. Запустить с двумя requested vehicle-slots, но `aicfMaxVehiclesPerFaction=1`.
2. До cleanup на стороне существует не более одной active/reserved AI-машины; исправные released abandoned-машины считаются отдельно в faction world pool с default soft target 4.
3. Заблокированный slot пишет `VEHICLE_CAP_BLOCKED` один раз, а не каждый tick.
4. После dismount близкая группа может повторно использовать ту же машину; `vehicle_generation` увеличивается, новая entity не создаётся.
5. Исправная abandoned-машина получает `VEHICLE_WORLD_POOL_RELEASED ... ai_cap_reserved=0 player_available=1`, через `FactionFleet` теряет Trip/Lease ownership, остаётся в мире и не блокирует новую AI trip.
6. Destroyed/on-fire/overturned/неподвижная либо выбранная для pool retirement машина удаляется только после отсутствия любого protected occupant, target-scoped player transition и живого игрока в радиусе 15 м в течение непрерывных 5 с. Непосредственно перед `DeleteRplEntity` выполняется повторный scan; blocker сбрасывает всё stable-clear окно и диагностируется через `VEHICLE_CLEANUP_DEFERRED`.
7. Заполнить world pool до 4 исправных машин стороны и освободить пятую. Пятая всегда немедленно выходит из active AI cap и остаётся в мире; `FactionFleet` хранит pool, а `VehicleCleanupManager` выбирает и проверяет oldest-safe retirement. При занятой игроком, transition или proximity-защищённой машине pool временно превышает target, а не выполняет небезопасный delete. После освобождения gate oldest entry удаляется безопасно и размер возвращается к target.
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
VEHICLE_REQUEST_WAITING / VEHICLE_SPAWN_WAIT_HEARTBEAT
VEHICLE_REQUEST_RESUMED / VEHICLE_REQUEST_CONTEXT_CHANGED
VEHICLE_SPAWNED
VEHICLE_ASSIGNED
DRIVER_ASSIGNED / GUNNER_ASSIGNED
PASSENGERS_ASSIGNED
BOARDING_STARTED / BOARDING_REJECTED / BOARDING_PHASE_STARTED
BOARDING_APPROACH_COMPLETE / BOARDING_APPROACH_REISSUED / BOARDING_APPROACH_MEMBER_STALLED / BOARDING_PROGRESS
BOARDING_TRANSITION_GRACE / BOARDING_COMPLETE / BOARDING_TIMEOUT
BOARDING_ROLE_RESET / BOARDING_ROLE_RETRY / BOARDING_ROLE_VIOLATION
VEHICLE_ROUTE_ASSIGNED / VEHICLE_PROGRESS / VEHICLE_MOTION
DISEMBARK_STARTED / DISEMBARK_REISSUED / DISEMBARK_TIMEOUT / DISEMBARK_COMPLETE
VEHICLE_STUCK_DETECTED
VEHICLE_RECOVERY_STARTED / VEHICLE_RECOVERY_SUCCEEDED / VEHICLE_RECOVERY_FAILED
VEHICLE_CREW_RECOVERY_SUCCEEDED
VEHICLE_UNSTUCK_STARTED / VEHICLE_UNSTUCK_ATTEMPT / VEHICLE_UNSTUCK_SUCCEEDED / VEHICLE_UNSTUCK_FAILED
DRIVER_LOST / DRIVER_REASSIGNED / GUNNER_LOST / GUNNER_REASSIGNED
DISEMBARK_CLEARANCE_RECOVERY
FALLBACK_FORCE_DISEMBARK / FALLBACK_DISEMBARK_FAILED
VEHICLE_ABANDONED / VEHICLE_DESTROYED
VEHICLE_WORLD_POOL_RELEASED / VEHICLE_WORLD_POOL_SOFT_OVERFLOW
VEHICLE_CLEANUP_DEFERRED
VEHICLE_DELETE_REQUESTED / VEHICLE_DELETE_RETRIED / VEHICLE_DELETE_NOT_CONFIRMED
VEHICLE_CLEANUP_CONFIRMED / VEHICLE_CLEANUP
VEHICLE_STOP_CLEANUP_STARTED / VEHICLE_STOP_CLEANUP_CONFIRMED / VEHICLE_STOP_CLEANUP_RETAINED
INFANTRY_FALLBACK
VEHICLE_CAP_BLOCKED
HEARTBEAT
RESULT_CANDIDATE / ACCEPTANCE_FAILURE_LATCHED / RESULT
```

Повторяющийся warning с одинаковыми faction/slot/generations/reason каждый tick является дефектом, даже если поездка позднее завершилась.

`BOARDING_STARTED` фиксирует `phase_timeout_ms`, immutable `planned_phases`/`total_timeout_ms`, leader-distance, `nearest_m`, `farthest_m`, прерванные старые actions и per-member samples. План содержит обязательную `PASSENGERS` и только реально нужные `APPROACH`, `DRIVER`, optional `GUNNER`: максимум 3 фазы transport и 4 armed-light. Если `farthest_m > 75`, для каждого удалённого живого участника создаётся собственный MOVE-only action к фактической машине с радиусом не больше 70 м, без group Move-waypoint, vehicle utility и GetIn. Per-member progress составляет минимум 2 м; после 15 с stall action переиздаётся не более одного раза. Driver и gunner затем получают точные `SCR_AIGetInVehicle` action на заранее зарезервированный Pilot/Turret slot; переход роли подтверждается только после физического settled-state. После settled полного обязательного crew машина может подключиться к group utility, но BoardingFlow не создаёт passenger group waypoint: он до первой action атомарно резервирует отдельный exact Cargo slot каждому живому пассажиру и выдаёт token-owned `SCR_AIGetInVehicle` per member.

Каждая начатая фаза имеет soft deadline, общий clock не сбрасывается. На всю boarding-попытку допускается ровно одна `BOARDING_TRANSITION_GRACE` длительностью 10 секунд только при target-scoped `getting_in` или свежем физическом progress; phase/total hard cap остаётся абсолютным. `BOARDING_TIMEOUT` обязан содержать текущую фазу, точную причину, число живых/посаженных, состояние driver/gunner, `planned_phases`, phase/total age, `deadline_scope`, physical maxima, waypoint state и per-member samples. `BOARDING_ACTION_OWNERSHIP` раз в 10 секунд и `BOARDING_CREW_ROLE_LOST` на failure-edge дополнительно фиксируют для каждого живого бойца authoritative `ai_action`/state и показывают, остаётся ли tracked exact crew action текущим. `BOARDING_COMPLETE` требует двух подряд settled poll всех живых членов именно в этой машине.

До `SpawnEntityPrefabEx` spawner детерминированно обходит все safe friendly base по расстоянию и ключу. Для каждого кандидата до создания проверяются стратегический rejection, максимальная spawn-дистанция, точный empty-terrain site и расстояние всех живых бойцов до этого site. `VEHICLE_SPAWN_CANDIDATES_EVALUATED` агрегирует исход каждой невражеской базы (`CONTESTED`, inactive spawn, `TOO_FAR`, `NO_EMPTY_TERRAIN`, `NO_BOARDING_SITE_WITHIN_RANGE` или `SELECTED`) на каждую bounded attempt/редкий wait-probe; вражеские базы сводятся в `hostile_skipped`. Entity создаётся только для полностью допустимого кандидата; отсутствие живых бойцов даёт retryable `GROUP_NOT_READY` без spawn.

Обычная посадка AICF не вызывает teleport-in: `APPROACH` использует только per-member move actions, обязательные роли — exact-role action, а пассажиры — отдельные token-owned `SCR_AIGetInVehicle` на заранее атомарно зарезервированные exact `CargoCompartmentSlot`; group vehicle boarding waypoint отсутствует. Teleport/relocation наружу запрещён в normal dismount и используется только в bounded terminal/fallback fail-closed recovery. В T10/A2 отсутствие snap должно подтверждаться последовательными authoritative `member_samples`/`BOARDING_PROGRESS` и видео клиента, а не только конечным occupant-state.

`SAFE_REUSE` перед новой role-ordered посадкой освобождает старые compartment reservations и прерывает vehicle action queue всех живых managed members. Состояние `mounted > 0 && driver=0` нормализуется синхронно до первой exact-role action и повторно сразу после `APPROACH`: per-member actions очищаются, живые managed occupants высаживаются, затем exact `DRIVER` выдаётся повторно. Повторное нарушение или истечение reset deadline даёт `BOARDING_ROLE_VIOLATION`, acceptance failure и пеший fallback.

`DISEMBARK_REISSUED` означает одноразовую очистку stale vehicle actions/waypoint и повторную выдачу штатного GetOut на половине `aicfVehicleBoardingTimeoutMs`. Normal completion требует двух последовательных чистых poll: отсутствуют logical vehicle/compartment link и get-in/get-out transition, а персонаж находится вне ориентированных bounds машины. До normal deadline физически застрявший managed member получает только bounded per-member movement guidance (`DISEMBARK_CLEARANCE_GUIDANCE`) без relocation. Если к полному deadline clearance не подтверждён, `DISEMBARK_TIMEOUT` фиксирует `logical/transitions/inside_bounds`, защёлкивает acceptance failure и запускает bounded terminal/fallback recovery, где exact eject/relocation допустим только как fail-closed escalation.

`VehicleTransitFlow` резервирует конкретный role slot и синхронно создаёт один exact CrewToken/`SCR_AIGetInVehicle`; `VehicleMovementState` хранит owner identity, поэтому abort/fallback завершает только этот token, а settled seat снимает tracking без `Fail()`. При одновременной потере ролей driver восстанавливается первым, затем gunner. `DRIVER_LOST`/`GUNNER_LOST` обязаны содержать immutable last-role snapshot: last entity/Rpl, assigned/actual seat, link/get-in/get-out, current action/state и `loss_kind=EXIT_ACTION|SEAT_CHANGED|ROLE_SLOT_EMPTY`. Settled mandatory crew лишь armed pending evidence; physical movement даёт промежуточный `VEHICLE_MOBILITY_RESTORED`, а общий success требует последующего route progress. `SendGetInMessage`/`SendCancelMessage` с `relatedActivity=null` не допускаются.

При fallback сначала используется штатный animated get-out waypoint. После bounded deadline forced teleport/no-door exit применяется только к `ALIVE` членам текущей managed-группы; `INCAPACITATED` и foreign occupants не force-eject-ятся. Попытки ограничены hard deadline `2 × aicfVehicleBoardingTimeoutMs`; успешная принудительная высадка отмечается единичным `FALLBACK_FORCE_DISEMBARK`.

Если к hard deadline protected member текущей группы всё ещё внутри, один `FALLBACK_DISEMBARK_FAILED` дополняет исходную terminal cause, Transport Trip конечным образом прекращает vehicle control и записывает terminal failure/suppression. Это прекращение немедленно запускает `VehicleTaskHandoff`: meaningful infantry order восстанавливается для живой current-группы и доказывается независимо от physical clearance. Cleanup параллельно сканирует все compartments; любой `ALIVE`/`INCAPACITATED`, включая foreign occupant, а также logical/transition/bounds/player blocker удерживает lease release/delete до `clearance_safe`. Hard failure не зацикливается, не оставляет группу taskless и не удаляет защищённого персонажа вместе с машиной.

Исправная abandoned-машина через `FactionFleet` немедленно освобождает Trip lease и active AI cap, пишет `VEHICLE_WORLD_POOL_RELEASED` и остаётся доступной игрокам в отдельном faction world pool. Default 4 — soft target: при превышении `VEHICLE_WORLD_POOL_SOFT_OVERFLOW` фиксирует `pool_size`, `pool_limit`, `retirement_candidates` и `policy=PLAYER_SAFE_DEFERRED_RETIREMENT`; `VehicleCleanupManager` выбирает oldest-safe retirement, но pool временно остаётся больше target при player/interaction/proximity blocker. Destructive cleanup предназначен только для unusable или явно retired entity и никогда не имеет приоритет над safety gate.

Перед любым delete watchdog сканирует все compartments, target-scoped get-in/get-out transitions и живых игроков, связанных с машиной или находящихся в 15 м. Любой blocker обнуляет stable-clear clock и может дать `VEHICLE_CLEANUP_DEFERRED`; destructive call разрешён только после 5 с непрерывного clear и повторного scan непосредственно перед удалением. Затем `VEHICLE_DELETE_REQUESTED` сохраняет `entity_id`, `rpl_id`, origin и attempt; authority ограниченно повторяет удаление, а `VEHICLE_CLEANUP_CONFIRMED` появляется только после того, как тот же `EntityID` больше не разрешается. Это не client acknowledgement: возможный визуальный despawn desync должен быть проверен в Armed A2 по тем же `EntityID`/`RplId`.

При `Stop(cleanupEntities=true)` `VehicleCleanupManager` не удаляет assets синхронно: он создаёт identity-fenced cleanup jobs и запускает one-shot poll раз в секунду, перепланируя его только пока очередь не пуста. Каждый callback проверяет group/trip/lease/vehicle generations, `EntityID`/`RplId` и action token. На получение тех же 15 м/5 с safety-условий отведено 60 с; затем действуют bounded retry и authority confirmation. `VEHICLE_STOP_CLEANUP_CONFIRMED` означает подтверждённое исчезновение. Stale callback self-cancel, identity mismatch, delete без confirmation или отсутствие stable-clear к deadline дают `VEHICLE_STOP_CLEANUP_RETAINED ... action=FAIL_CLOSED`: машина остаётся в мире.

При любом прекращении vehicle control `VehicleTaskHandoff` немедленно очищает owned vehicle utility/waypoint и временные group move handlers, восстанавливает stock infantry movement и meaningful order для живой current-группы. Успех `order_restored` подтверждается current+queued waypoint и meaningful-task postcondition и не зависит от `clearance_safe`; occupant/transition/bounds/player cleanup продолжается параллельно и не телепортирует бойцов normal-path.

Пехотный order recovery пишет `ORDER_RECOVERY_ISSUED`, а `ORDER_RECOVERED` — только после трёх наблюдений, где exact тот же waypoint одновременно остаётся current и присутствует в queue, и не раньше `max(10 с, 2 × reliability interval)`. `ORDER_RECOVERY_STABILITY` фиксирует число poll, длительность, queue/current context и решение durability; нестабильный кандидат расходует bounded stuck budget и в пределе вызывает recycle вместо бесконечного повтора каждые 5 секунд.

`VEHICLE_PROGRESS` означает чистое сокращение `route_distance_m` до road/direct endpoint минимум на `aicfVehicleProgressMeters`. `VEHICLE_MOTION` означает физическое перемещение минимум на `aicfVehicleMotionMeters` без такого чистого сокращения. Эти события обслуживают независимые objective/stationary deadline. Dismount проверяется по `target_distance_m` до тактического capture point.

Recovery после `NO_PHYSICAL_MOVEMENT` проходит bounded safe unstuck. Reposition всей машины выполняется только authority, с settled managed occupants, без foreign/player/transition, после obstacle/ocean и active-mine/living-character clearance; максимальное смещение — 15 м. `MANAGED_MEMBERS_NOT_SETTLED` откладывает recovery с backoff и `attempt_consumed=0`. Само смещение создаёт только `VEHICLE_UNSTUCK_ATTEMPT evidence=PENDING`, самостоятельное движение — `VEHICLE_MOBILITY_RESTORED`; `VEHICLE_RECOVERY_SUCCEEDED` разрешён только после дополнительного чистого route progress. Движение без приближения к endpoint и простое возвращение водителя больше не очищают recovery episode.

## Текущая матрица приёмки

Матрица отражает исторические Transport T8/T9, Armed A1 и статус post-T9-патча. Исторические T1–T6 не являются актуальным доказательством и ниже не разворачиваются. Static/Workbench evidence относится к текущему post-T9 snapshot; runtime-строки остаются историческими или `NOT RUN` до T10/A2.

| Проверка | Transport | Armed | Что требуется дальше |
|---|:---:|:---:|---|
| Workbench 1.7 validation / static audit post-T9 | PASS | PASS | 5 конфигураций, CRC32 `946e5a78`, `SCRIPT (E/F)=0`; это не runtime PASS |
| Safe spawn, faction и configured cap | PASS | PARTIAL | A2: подтвердить вооружённые машины обеих сторон и cap fault |
| Строгий DRIVER → cargo | PARTIAL | FAIL | T10/A2: ни одного compartment до exact driver, включая SAFE_REUSE; отдельно no-combat repeat action ownership |
| Строгий DRIVER → GUNNER → cargo | N/A | FAIL | A2: обе роли settled до passenger phase |
| Полная посадка всех живых | PARTIAL | FAIL | T10/A2: new vehicle и SAFE_REUSE у US/USSR без stalled approach/crew-role loss |
| Движение и route progress | PASS | PARTIAL | Подтвердить после новой посадки без remote snap |
| Высадка и восстановление приказа | PASS | NOT RUN | T10: `NormalizeAfterVehicle`, продолжение пехотой и owner change |
| Пехотный захват после высадки | NOT RUN | NOT RUN | Наблюдать owner change после штатной высадки |
| Bounded boarding/recovery/fallback | FAIL | FAIL | T10: отдельно доказать crew-role recovery и safe vehicle-unstuck; никакого success до post-recovery motion/progress |
| Reuse и vehicle generation | FAIL | FAIL | Повторное использование без нарушения ролей |
| Retry → WAITING_FOR_SITE → wake | PARTIAL | N/A | T9 подтвердил context wake, но не выдачу у Farm; T10 должен показать per-base trace и успешный wake после clear |
| Functional abandoned world pool | NOT RUN | NOT RUN | Soft target 4, player available, безопасный overflow и oldest-safe retirement |
| Cleanup / authority delete | FAIL | PARTIAL | T10/A2: player/transition gate 15 м + 5 с stable clear; сопоставить ID |
| Stuck/recovery и order durability | FAIL | PARTIAL | T9 доказал разрушительный recycle; T10: FIELD_HOLD сохраняет entity/позицию/target и bounded resume |
| 30 минут без роста сущностей/ошибок | NOT RUN | NOT RUN | После T10/A2 |
| Полные server/client доказательства | PASS | PARTIAL | Для A2 обязателен клиентский despawn probe |

### Архив T1–T6

Подробные журналы, команды, профили, временные срезы и пути к артефактам T1–T6 удалены как неактуальные. Все шесть прогонов исторически завершились FAIL. Они последовательно выявили уже закрытые или заменённые классы дефектов: faction-init mismatch и warning churn, ложный immobility, abandoned-spam, захват пассажирского места до водителя, общий deadline фаз, преждевременный RESULT PASS и ложный dismount completion. Для текущей приёмки источниками истины являются исторические T7/T8/T9, Armed A1 и следующие post-T9 T10/A2.

### Transport T7 — исторический runtime FAIL

- US SAFE_REUSE начался на расстоянии 114–128 м. Водитель занял pilot seat через 43 с; затем пассажирская фаза получила собственные 60 с и завершилась при mounted=2/3. Последний солдат визуально входил, но сервер ещё не считал его settled. Итог: BOARDING_TIMEOUT, acceptance failure, forced dismount и пехотный fallback.
- USSR SAFE_REUSE начался на расстоянии 143–151 м. Водитель сел через 6 с, но к hard deadline пассажирской фазы осталось mounted=1/3. Итог тот же: bounded timeout, infantry order и подтверждённый cleanup.
- Общая причина: пассажиры не подходили к машине до подтверждения водителя, а штатный поиск посадки ограничен примерно 100 м. Простое увеличение тайм-аута не устраняет дальний последовательный подход.
- T7 также подтвердил, что прежний ORDER_RECOVERY стал bounded, но двух ранних stable poll было недостаточно: новый waypoint позднее снова исчезал, после чего группа перерабатывалась.

### Armed A1 — исторический runtime FAIL

- PILOT_ONLY не исключил раннее занятие gunner/cargo; strict role ordering нарушался у обеих фракций.
- Crew recovery был недостаточно прозрачен и мог завершаться без доказательства восстановления всех обязательных ролей.
- Машина могла создаваться далеко от группы и только после создания отклоняться как VEHICLE_TOO_FAR.
- Возможный client despawn desync остаётся PARTIAL: authority delete подтверждался, но клиентского EntityID/RplId доказательства нет.
- Infantry waypoint recovery давал повторный WAYPOINT_NOT_CURRENT churn; это исправлено только в post-A1-кандидате.

### Исторический post‑T7-кандидат, проверенный T8

Этот snapshot не менял штатные мины Arland и не получил runtime PASS: следующий прогон T8 завершился FAIL. Ниже сохранён исторический контракт, чтобы связать T8-наблюдения с проверявшейся реализацией; он заменён текущим post-T8-патчем.

- При farthest member >75 м запускается MOVE-only APPROACH всей группы. Vehicle utility и GetIn actions до завершения подхода отсутствуют.
- APPROACH waypoint получает 5 с на активацию в queue; потеря/неактивация ограниченно завершает попытку и фиксирует acceptance failure.
- После staging действует exact DRIVER → optional exact GUNNER → PASSENGERS. Уже занятые неправильные места синхронно нормализуются до выдачи DRIVER action, в том числе после APPROACH_COMPLETE.
- Общий budget вычисляется один раз из реально запланированных фаз: transport максимум 3, armed максимум 4. Каждая фаза имеет soft timeout; один sticky grace +10 с разрешён только при target-scoped getting-in или свежем физическом progress. Общий hard cap также не расширяется более чем на 10 с.
- BOARDING_COMPLETE требует два последовательных poll, где все живые участники settled в целевой машине.
- ORDER_RECOVERED требует exact current+queued waypoint в трёх последовательных наблюдениях и не раньше max(10 с, 2 × reliability interval). Нестабильность расходует bounded stuck budget.
- Перед T8 этот старый snapshot проходил static/Workbench validation; результат не переносится на post-T8-патч.

### Transport T8 — исторический runtime FAIL

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
- **Воспроизведение на USSR:** `SAFE_REUSE` «Урала» `EntityID=0x400000000000145E` начался в 17:48:00 с distances 123–126 м. В 17:48:43 approach-waypoint исчез (`BOARDING_APPROACH_LOST reason=MOVE_WAYPOINT_NOT_IN_QUEUE`): один боец оказался в 8 м от машины, двое других — в 157.8 м. Машина стала `ABANDONED` и была authority-удалена в 17:49:46 по обычному `CLEANUP_DUE`. Следующие запросы USSR также зациклились: сначала candidate base 48 отклонена при distances 27/34/278 м, затем base 60 — при 319/631/633 м, после чего продолжились `SPAWNING → REQUESTED`. Таким образом, cohesion/request-recovery defect общий для обеих фракций, не US-specific.
- **Фактический результат:** группа остаётся без транспорта и не может начать посадку, хотя исходная причина — разрыв участников — визуально устранена и strategic/base context изменился.
- **Ожидаемый результат:** после восстановления cohesion либо появления новой допустимой friendly-базы `VehicleAcquisitionFlow` заново измеряет всех участников и перечень spawn sites, создаёт машину при валидной точке; если валидной точки всё ещё нет — возвращает typed bounded wait/fallback outcome с актуальной диагностической причиной без постоянного state/log churn.

#### Дефект T8-CLEANUP-1: удаление abandoned-машины во время посадки игрока зависает персонажа

- **Severity:** critical для gameplay/client state; cleanup safety FAIL.
- **Шаги воспроизведения:** игрок уничтожил сопровождавших USSR-ботов у оставленного «Урала» `EntityID=0x4000000000001307`, затем начал ручную посадку в машину. Во время enter-анимации машина исчезла, персонаж остался в зависшем interaction/animation state.
- **Серверная последовательность:** после `DISEMBARK_COMPLETE` группа потеряла combat-ready state, в 17:40:51 машина стала `ABANDONED reason=GROUP_NOT_COMBAT_READY`. В 17:41:21.989 выдан `VEHICLE_DELETE_REQUESTED reason=REPLACEMENT_CAPACITY_REQUIRED`; в 17:41:23.006 — `VEHICLE_CLEANUP_CONFIRMED` для того же `EntityID=0x4000000000001307`, `RplId=-2147470878`.
- **Клиентская корреляция:** клиент получил RPL-событие удаления в 17:41:22, то есть внутри наблюдаемой анимации входа. В серверном cleanup-логе отсутствует protected-occupant/interaction defer для игрока.
- **Причина-кандидат:** compartment scan защищает уже связанного occupant, но персонаж в начатой enter-анимации ещё не зарегистрирован в compartment/vehicle и не удерживает delete gate. Между последней safety-проверкой и authority delete также отсутствует устойчивое подтверждение, что рядом нет нового `getting_in` interaction.
- **Ожидаемый результат:** abandoned vehicle нельзя удалять, пока любой игрок или AI выполняет target-scoped get-in/get-out interaction либо находится в зоне активного vehicle action. Нужен server-authoritative interaction reservation/delete lock, повторная проверка непосредственно перед `DeleteRplEntity`, короткий stable-clear window и безопасная отмена/восстановление персонажа при race. Cleanup должен писать причину defer и данные foreign/player protection.
- **Рекомендуемая lifecycle-политика:** исправный abandoned-транспорт не удалять автоматически ради replacement capacity. Снять его с управления Stage 3 и исключить из лимита активных/reserved AI-машин либо учитывать в отдельном ограниченном world pool, оставив доступным игрокам и для возможного safe reuse. Автоматическое удаление применять к уничтоженной/непригодной технике или после длительного TTL, но только после устойчивого stable-clear подтверждения отсутствия occupants, активных get-in/get-out interactions и игроков в защитном радиусе. Нужны отдельный предел мирового пула и oldest-safe cleanup, чтобы сохранение техники не приводило к неограниченному росту сущностей.

### Post-T8-патч и Transport T9

Post-T8 source прошёл static/Workbench и затем был проверен Transport T9. T9 сохранил итог `FAIL`: новый per-member approach работал, но были подтверждены destructive persistent-stuck recycle и недостаточная диагностика выбора базы; passenger drift/crew-role loss в бою остался кандидатом. Пункты ниже описывают реализацию, с которой запускался T9, а не runtime `PASS`.

- Общий `APPROACH` waypoint заменён точным per-member staging: отдельный `SCR_AIMoveIndividuallyBehavior` к фактической машине для каждого живого бойца дальше 75 м, action radius ≤70 м, без group waypoint, vehicle utility и GetIn. Progress наблюдается по каждому участнику; после 15 с stall action переиздаётся ровно один раз, следующий stall даёт bounded `BOARDING_APPROACH_MEMBER_STALLED`. Завершение требует двух settled poll всей группы в радиусе 75 м.
- До `DRIVER` и повторно после approach выполняется synchronous wrong-seat normalization. Дальше неизменно действует `DRIVER → optional GUNNER → PASSENGERS`; immutable plan содержит максимум 3 фазы transport/4 armed, у каждой свой soft timeout, на попытку доступна ровно одна target-scoped grace 10 с и абсолютный total hard cap.
- Retryable spawn делает максимум 4 попытки с экспоненциальным backoff до 60 с. Затем `WAITING_FOR_SITE` сохраняет пехотный приказ, не резервирует AI cap и выполняет полный periodic preflight; допустимый site пробуждает запрос через `VEHICLE_REQUEST_RESUMED`, а смена target/base revision сбрасывает request context немедленно. Recoverable `APPROACH_LOST/STALL/TIMEOUT_APPROACH`, `VEHICLE_TOO_FAR` и `GROUP_COHESION` также переходят в ожидание без assignment suppression.
- После verified dismount/fallback `NormalizeAfterVehicle` очищает временные move handlers и восстанавливает stock `Column`/default handler перед пехотным приказом.
- Исправный abandoned-транспорт всегда освобождает managed runtime/active AI cap и остаётся доступным игрокам в faction world pool с default soft target 4. При overflow координатор ищет oldest-safe retirement; pool может временно превысить target и не применяет destructive replacement cleanup вопреки safety gate.
- Destructive cleanup unusable/retired entity блокируется protected occupant, target-scoped player transition или живым игроком в радиусе 15 м. Нужны 5 с непрерывного clear и повторный scan непосредственно перед delete; blocker диагностируется через `VEHICLE_CLEANUP_DEFERRED`.
- Coordinator-stop cleanup использует one-shot polling раз в секунду и до 60 с пытается получить те же 15 м/5 с safety-условия. После этого действует тот же `EntityID`/`RplId` guard; невозможность безопасно удалить заканчивается fail-closed `VEHICLE_STOP_CLEANUP_RETAINED`, а не force delete.
- Durable infantry recovery сохраняет прежний контракт: exact waypoint одновременно current+queued в трёх poll и не меньше `max(10 с, 2 × reliability interval)`.

Штатные мины Arland остаются включёнными и не изменялись.

#### Кандидат-дефект T9-USSR-1: пассажиры расходятся после успешного APPROACH до завершения DRIVER в боевой обстановке

- В runtime 19:49:36–19:51:11 `USSR A0`, generation 4, начал `SAFE_REUSE` транспорта `0x4000000000001FDD` с тремя живыми бойцами примерно в 112–117 м.
- Новый per-member APPROACH работал правильно: все три individual action сокращали расстояние, и `BOARDING_APPROACH_COMPLETE` был зарегистрирован при 72.8/73.3/75.0 м.
- После перехода в фазу `DRIVER` approach-actions были сняты со всей группы. Пока водитель шёл к машине и садился около 30 с, два будущих пассажира возобновили пехотное движение от машины: к `DRIVER_ASSIGNED` расстояния выросли до 83.8 и 86.6 м.
- `PASSENGERS_ASSIGNED requested=2` был выдан, но пассажиры визуально продолжили движение в другую сторону. В 19:51:07 цикл завершился `INFANTRY_FALLBACK reason=CREW_ROLE_LOST_DURING_BOARDING`; машина стала `ABANDONED` и была выпущена в world pool.
- Во время посадки рядом шёл бой. Логи не показывают потерь (`alive=3` сохранялся), возгорания или повреждения машины, но не позволяют отличить возобновившийся stock infantry order от combat reaction/cover behavior. Поэтому механизм причины считается недоказанным до повторения без контакта с противником либо до добавления диагностики action/behavior ownership.
- Ожидаемое поведение: после `BOARDING_APPROACH_COMPLETE` все неводительские участники должны удерживаться в staging-зоне возле машины до начала своей role-phase. Нельзя снимать/терять их точные удерживающие действия так, чтобы stock infantry order снова уводил их от машины. Переход `APPROACH → DRIVER → PASSENGERS` должен сохранять invariant `farthest_m <= staging_threshold_m` либо немедленно возвращать удалившегося member в bounded per-member approach без потери уже назначенного водителя.

#### Срез T9 — 2026-08-09 19:56–19:57, US A0 после захвата фермы

- В 19:56:20 база 65 (Farm) сменила владельца `USSR → US`; ожидавший транспорт запрос US A0 проснулся по `BASE_REVISION_CHANGED`.
- US A0 визуально находилась рядом с фермой. Однако транспорт там выдан не был. Лог показал только ближайший точный boarding-rejection для другой безопасной базы 29: расстояние около 1126 м при максимуме 250 м.
- Это сообщение не доказывает, что Farm не рассматривалась: диагностика выводит только ближайший отказ среди safe-кандидатов. Недавно захваченная Farm могла быть исключена раньше как `CONTESTED`, `IsBeingCaptured()` или `AreEnemiesPresent()`; незадолго до смены владельца база 65 явно отмечалась contested. Точная причина отклонения Farm в текущем логе отсутствует.
- В 19:56:28 прежняя US A0 после повторяющихся `WAYPOINT_NOT_CURRENT` и persistent-stuck была завершена через `GROUP_RECYCLED`. Это не перемещение существующих бойцов: старая группа удалена вместе со своим положением возле фермы.
- В 19:56:58 замещающая US A0 создана на MOB-базе 30. В 19:57:00 ей выдан новый M923A1, посадка началась с расстояния 24–29 м. Наблюдаемая «телепортация на базу» является удалением старой группы и созданием новой на MOB.
- Кандидат-дефект: recovery/recycle группы возле только что захваченной точки теряет её достигнутое положение и переносит продолжение операции на удалённую MOB. Из-за recycle через 8 секунд невозможно проверить, получила бы исходная группа транспорт после окончательной очистки Farm.
- Пробел диагностики: при отсутствии подходящего spawn-site необходимо логировать причину отклонения каждой релевантной owned-базы либо как минимум выбранной ближайшей к группе базы. Иначе нельзя отличить ожидаемое ограничение `CONTESTED/enemies present` от ошибочного отказа выдачи транспорта на безопасной захваченной точке.
- Статус: `CANDIDATE / NEEDS REPRO`. Подтверждать функциональный дефект выдачи транспорта следует в повторе, где Farm визуально очищена, capture-state завершён и группа остаётся возле базы дольше одного spawn-probe. Сам факт recycle на MOB и потеря позиции подтверждён.

### Post-T9-патч — static/Workbench candidate

Подтверждённый destructive recycle устранён: после исчерпания order/stuck recovery-budget прежняя группа больше не получает `MarkDestroyed`, не удаляется и не создаёт replacement на MOB. `GROUP_STUCK_PERSISTENT action=FIELD_HOLD` очищает stale movement handlers, создаёт локальный defend-waypoint на authoritative позиции живого лидера и сохраняет тот же group entity, generation и strategic target. `GROUP_STUCK_FIELD_HOLD entity_preserved=1 ticket_policy=NONE` подтверждает containment. Через один `aicfStuckTimeoutMs` выполняется bounded `GROUP_STUCK_FIELD_RESUMED trigger=HOLD_TIMEOUT`; изменение graph/target возобновляет или переназначает операцию немедленно. Неудача создания нового objective-waypoint возвращает ту же группу в hold, а не запускает respawn.

Пробел выбора базы закрыт событием `VEHICLE_SPAWN_CANDIDATES_EVALUATED`. На каждую из максимум четырёх попыток и каждый редкий `WAIT_PREFLIGHT` оно содержит агрегированный trace всех невражеских кандидатов: `base`, расстояние от группы и точный результат стратегического/site фильтра вплоть до `SELECTED`. Поэтому следующий прогон должен прямо показать, была ли Farm `CONTESTED`, inactive, недоступна по расстоянию/terrain или действительно выбрана; вражеские базы не создают длинный log spam и учитываются счётчиком `hostile_skipped`.

Кандидат passenger/combat пока не маскируется принудительным hold и не объявлен закрытым. Для причинного повтора добавлены `BOARDING_ACTION_OWNERSHIP` каждые 10 секунд и `BOARDING_CREW_ROLE_LOST` на точном failure-edge. Per-member samples теперь содержат `ai_action` и `ai_action_state`, а crew snapshot — tracked agent/action, crew phase и `is_current`. Это позволяет отличить AICF exact GetIn от stock combat/cover/infantry activity. Нужны два повтора: без контакта и с контролируемым контактом.

Текущий post-T9 snapshot прошёл `tools/Test-Stage3Static.ps1` и Workbench 1.7 `Validate Scripts` по конфигурациям `WORKBENCH/PC/XBOX/PS4/PS5`: `.cache/stage3-post-t9-vehicle-unstuck-final2-20260809/console.log`, Game CRC32 `946e5a78`, `Script validation successful`, `SCRIPT (E/F)=0`. Dedicated runtime на этом snapshot ещё не запускался; Transport T9 остаётся историческим `FAIL`.

#### Срез T9 — US: отсутствует восстановление физически застрявшей машины

- После успешной повторной посадки US A0 машина `0x40000000000014BB` физически застряла при движении к цели. Визуально выход и повторная посадка водителя выглядели как попытки освободить транспорт.
- Runtime дважды зарегистрировал `DRIVER_LOST`, затем `DRIVER_REASSIGNED` и `VEHICLE_RECOVERY_SUCCEEDED reason=ALL_REQUIRED_CREW_RESTORED`. Эти события подтверждают только восстановление роли водителя, но не восстановление физической подвижности машины.
- После двух формально успешных role-recovery транспорт остался застрявшим. В 21:27:58 система завершила цикл через `VEHICLE_RECOVERY_FAILED reason=DRIVER_RECOVERY_EXHAUSTED` и `INFANTRY_FALLBACK`; группа окончательно бросила машину.
- Дефект: физическое застревание ошибочно классифицируется как потеря водителя, а возвращение водителя считается успешным recovery без доказательства перемещения транспорта. Это расходует recovery budget, не устраняя первопричину.
- Требуемая доработка: добавить bounded vehicle-unstuck recovery по аналогии с восстановлением застрявшего персонажа. До отказа от исправной машины система должна попытаться безопасно восстановить её положение/подвижность, не телепортируя бойцов в салон.
- Возможная последовательность: определить `NO_PHYSICAL_MOVEMENT` при живом водителе и исправной машине → остановить конфликтующие vehicle actions → выполнить ограниченную попытку корректировки машины на ближайшую безопасную проезжую позицию или иной штатный unstuck → заново назначить маршрут → подтвердить recovery только после реального `VEHICLE_MOTION` либо достаточного route progress.
- Каждая попытка должна быть ограничена числом и временем, сохранять occupants и проверять безопасность новой позиции: отсутствие воды, препятствий, мин/опасной зоны, пересечения с объектами и недопустимого удаления от текущего маршрута. Если безопасное восстановление невозможно, только тогда выполнить штатную высадку и infantry fallback.
- Новая диагностика: `VEHICLE_UNSTUCK_STARTED`, `VEHICLE_UNSTUCK_ATTEMPT`, `VEHICLE_UNSTUCK_SUCCEEDED` с подтверждённым displacement/progress и `VEHICLE_UNSTUCK_FAILED` с причиной. `VEHICLE_RECOVERY_SUCCEEDED` не должен появляться только из-за возвращения водителя, если исходной причиной была неподвижность.
- Статус: `CONFIRMED / NEEDS FIX`. Сам bounded infantry fallback после исчерпания попыток является ожидаемым; дефект находится в отсутствии восстановления машины и ложном успешном результате промежуточных recovery.

### External/versioned evidence caveat T8

- В server log до AICF init при загрузке stock `GameMode_Campaign`/RNGD присутствовали `WORLD (E): Unknown keyword/data 'm_bEnabled'` и `DEFAULT (E): Unknown keyword/data 'm_bCanAIMarkTargets'`. Ссылок на эти поля в репозитории нет; сообщения классифицируются как external/versioned resource noise, а не AICF `SCRIPT (E/F)`, но не считаются исправленными post-T8-патчем.
- Client log содержал stock UI `ScriptInvoker::Invoke: Incompatible parameter ... ShowFactionPlayerList`; bind/signature относится к `SCR_RoleSelectionMenu`, символ отсутствует в репозитории. Это внешний UI/version mismatch, который сохраняется как acceptance caveat и не является доказательством чистого окружения.

### Transport T10 — критерии повторной проверки post-T9

1. Проверить обе фракции, initial vehicle и `SAFE_REUSE`, включая точки немного ниже и выше порога 75 м.
2. Fault-срез `WAYPOINT_NOT_CURRENT → persistent stuck` обязан оставить тот же `group`, `group_generation`, target и полевую позицию; допустимы только `GROUP_STUCK_FIELD_HOLD` и bounded `GROUP_STUCK_FIELD_RESUMED`, без `GROUP_RECYCLED`, MOB replacement и ticket debit.
3. После захвата Farm дождаться минимум одной полной spawn-attempt и одного wait-probe. `VEHICLE_SPAWN_CANDIDATES_EVALUATED` обязан содержать Farm с точной причиной либо `SELECTED`; после исчезновения временного blocker транспорт должен появиться не позже следующего coordinator/probe interval.
4. Boarding повторить отдельно без противника и с контролируемым боевым контактом. Для каждого drift/role-loss сопоставить `ai_action`, `ai_action_state`, tracked crew `is_current`, waypoint state и расстояния бойцов; до этой причинной пары не подавлять stock combat behavior.
5. При `farthest_m >75` подтвердить отдельный approach-token для каждого удалённого живого member, отсутствие group waypoint/vehicle utility/GetIn, authoritative progress и `BOARDING_APPROACH_COMPLETE` после двух poll не дальше 75 м.
6. В stall-fault подтвердить не более одного `BOARDING_APPROACH_REISSUED` для конкретного member и затем один bounded `BOARDING_APPROACH_MEMBER_STALLED`/fallback без snap или teleport-in.
7. До `DRIVER_ASSIGNED` должно быть `mounted=0`; затем transport проходит только `DRIVER → PASSENGERS`, а armed-light — `DRIVER → GUNNER → PASSENGERS`. Все живые settled в двух poll без wrong-seat recurrence и remote snap.
8. На границе soft deadline допускается ровно одна grace только с target-scoped доказательством; hard outcome наступает не позже соответствующего phase/total cap +10 с.
9. В spawn-site fault подтвердить ровно 4 attempts, экспоненциальный backoff, `WAITING_FOR_SITE ... cap_reserved=0`, отсутствие state/log churn, periodic probe и отдельное пробуждение при смене target/base revision.
10. После штатной высадки проверить `NormalizeAfterVehicle`, устойчивое пехотное движение и реальный owner change. `ORDER_RECOVERED` не должен сопровождаться тем же `WAYPOINT_NOT_CURRENT` внутри durability window.
11. Выпустить исправную abandoned-машину в world pool и убедиться, что она остаётся доступной игроку и не блокирует новую AI trip. Отдельно превысить soft target 4: получить `VEHICLE_WORLD_POOL_SOFT_OVERFLOW ... policy=PLAYER_SAFE_DEFERRED_RETIREMENT`; защищённая пятая машина должна остаться в мире, а после освобождения gate должен выполниться oldest-safe retirement и размер вернуться к target.
12. Повторить T8 cleanup race: игрок в машине, в enter/exit transition и в радиусе 15 м блокирует delete; только после 5 с непрерывного clear допустим authority delete с тем же `EntityID`/`RplId`.
13. Отдельно вызвать coordinator stop с cleanup: подтвердить one-shot poll, максимум 60 с на acquire, `VEHICLE_STOP_CLEANUP_CONFIRMED` для безопасной машины и `VEHICLE_STOP_CLEANUP_RETAINED ... action=FAIL_CLOSED` при blocker/identity mismatch/unconfirmed delete.

### Armed A2 и отдельные fault-срезы

- Доказать exact DRIVER → GUNNER → PASSENGERS у US и USSR, включая SAFE_REUSE и потерю каждой crew role.
- Сопоставить server/client EntityID и RplId при delete; визуально оставшаяся машина без совпадающей identity не считается desync.
- Проверить active AI cap отдельно от functional world-pool cap, occupied/player-transition cleanup, overturned/destroyed fallback и 30-минутную стабильность.
- Прежняя заочная приёмка Stage 3 отменена. Rewrite implementation/cutover завершены, но T10, A2, fault-срезы и последующий 30-минутный прогон остаются обязательными acceptance-проверками, способными открыть новые дефекты.

## PASS / FAIL / BLOCKED

`PASS`:

- transport и armed-light срезы выполнены на одном commit;
- все применимые строки матрицы подтверждены;
- recovery ограничен и всегда заканчивается progress либо пехотным fallback;
- лимит и cleanup соблюдаются;
- Stage 2 disabled regression чист;
- получен `RESULT_CANDIDATE status=READY final=0`, затем до остановки полного окна нет `status=INVALIDATED` и `ACCEPTANCE_FAILURE_LATCHED`;
- нет `[AICF][STAGE3][ERROR]`, AICF `SCRIPT (E/F)`, duplicate spawn или бесконечного warning churn; любые stock/resource/UI errors отдельно объяснены и не коррелируют с проверяемым поведением.

`FAIL`: Stage 3 загрузился, но нарушен любой инвариант: unsafe/double spawn, неполная посадка без fallback, движение без водителя, отсутствие немедленного meaningful infantry order после прекращения vehicle control, зависимость `order_restored` от clearance, бесконечный recovery/fallback, spawn сверх cap, stale generation, release/delete машины до safe-clear с `ALIVE`/`INCAPACITATED` occupant (включая foreign) или утечка entity/waypoint.

`BLOCKED`: несовместимая версия, addon/resource не загружен, compile error, занят порт или окружение не позволяет начать нужный сценарий. `BLOCKED` не является PASS.

Прежняя заочная фиксация Stage 3 отменена. Implementation/cutover новой архитектуры завершены; полная MVP-матрица и двухчасовой soak с техникой всё ещё выполняются отдельно и остаются `NOT RUN`, пока не получено новое runtime evidence.

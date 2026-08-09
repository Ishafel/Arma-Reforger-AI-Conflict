# Stage 3 — наземная техника

Stage 3 принимается отдельно от пехотных Stage 1/2. Он добавляет транспорт существующим ATTACK-слотам, но не меняет количество пехотных групп, билеты, `OnEmpty`, правила победы или `CaptureRelay`.

Автоматическая компиляция и строка `[AICF][STAGE3][RESULT] ... status=PASS` являются только кандидатными доказательствами. Финальный PASS требует ручного dedicated runtime-прогона этой матрицы.

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
| `aicfVehicleBoardingTimeoutMs` | `60000` | Deadline посадки, высадки и смены экипажа |
| `aicfVehicleStuckTimeoutMs` | `120000` | Время без достаточного прогресса машины |
| `aicfVehicleProgressMeters` | `25` | Минимальное сокращение остатка маршрута, считающееся прогрессом |
| `aicfVehicleMaxRecoveries` | `2` | Общий budget смен экипажа и перестроений маршрута на поездку |
| `aicfVehicleDismountDistanceMeters` | `150` | Плановая дистанция высадки до objective |
| `aicfVehicleRetryIntervalMs` | `10000` | Повтор после временно недоступного safe spawn |
| `aicfVehicleCleanupDelayMs` | `60000` | Grace-период abandoned/destroyed entity до удаления |
| `aicfVehicleMinimumRouteMeters` | `400` | Короткий маршрут выполняется пешком без машины |
| `aicfVehicleMaximumReuseDistanceMeters` | `250` | Максимальная дистанция группы до оставленной машины для reuse |
| `aicfVehicleMaximumSpawnDistanceMeters` | `2000` | Максимальная дистанция safe friendly-базы от назначенной группы |
| `aicfVehicleCohesionDistanceMeters` | `100` | Допустимый отрыв живого бойца группы от движущейся машины |

Недопустимая комбинация, где requested transport+armed превышает число ATTACK-слотов или общий cap, не создаёт лишние машины: лишний slot остаётся пешим и пишет `VEHICLE_CAP_BLOCKED`.

Групповые map-маркеры включены всегда. В текущем срезе каждый клиент видит обе фракции; фильтрация до союзной фракции остаётся отдельной следующей задачей.

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
4. Вся живая группа использует одну машину: один `DRIVER_ASSIGNED`, остальные — пассажиры.
5. `BOARDING_COMPLETE mounted=<alive>` появляется до `VEHICLE_ROUTE_ASSIGNED`.
6. Маркер лидера показывает `VEH BOARDING`, затем `VEH MOVING` и `VEH DISEMBARKING`.
7. Машина действительно движется к objective; `VEHICLE_PROGRESS` отражает сокращение дистанции.
8. Высадка происходит примерно на настроенной дистанции, но не внутри опасной/заблокированной геометрии.
9. После `DISEMBARK_COMPLETE` все живые бойцы находятся вне машины, получают пехотный order и продолжают objective пешком.
10. Группа способна захватить обычную базу или выполнить штатный relay-order; правила relay не обходятся техникой.
11. Для каждой стороны есть завершённая поездка. После двух сторон допустим автоматический `RESULT status=PASS scope=AUTOMATED_TRIP_INVARIANTS`.

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
-aicfVehicleMaxRecoveries 1 `
-aicfVehicleCleanupDelayMs 10000
```

Проверяются сценарии:

| Сценарий | Как доказать | Ожидаемый результат |
|---|---|---|
| Boarding не завершён | Один живой член не смог занять доступное место до deadline | `BOARDING_TIMEOUT`, высадка оставшихся, `INFANTRY_FALLBACK`; группа не зависает в boarding |
| Driver погиб/вышел | Обычным игровым уроном вывести водителя из строя, не уничтожая всю группу | `DRIVER_LOST → VEHICLE_RECOVERY_STARTED → DRIVER_REASSIGNED` либо bounded `VEHICLE_RECOVERY_FAILED → INFANTRY_FALLBACK` |
| Gunner погиб | Только armed-run; вывести стрелка из строя | `GUNNER_LOST`, ограниченная смена стрелка или пеший fallback |
| Нет прогресса | Наблюдать неподвижную машину дольше timeout; не телепортировать её | `VEHICLE_STUCK_DETECTED`, не более configured recoveries, затем progress или fallback |
| Машина перевёрнута/неподвижна | Зафиксировать фактическое состояние видео/скриншотом | Немедленный отказ от машины и продолжение пешком |
| Машина уничтожена | Уничтожить транспорт, сохранив хотя бы одного живого члена managed-группы | `VEHICLE_DESTROYED`, прежний infantry slot/reinforcement contract остаётся корректным |
| Цель сменилась в пути | Дождаться реального base owner change другой группой | Старый vehicle order удалён, группа безопасно высаживается и получает актуальный пехотный target |
| Группа уничтожена | Уничтожить всех членов | Штатный `GetOnEmpty → reinforcement → ticket debit`; vehicle runtime становится terminal и не привязывается к новому generation |

Game Master/teleport не является доказательством нормального движения или захвата. Если он используется только для создания fault condition, это явно записывается в отчёте; причинная последовательность recovery всё равно подтверждается серверным логом.

## Прогон L — лимиты, reuse и cleanup

1. Запустить с двумя requested vehicle-slots, но `aicfMaxVehiclesPerFaction=1`.
2. До cleanup на стороне существует не более одной active/reserved/abandoned машины.
3. Заблокированный slot пишет `VEHICLE_CAP_BLOCKED` один раз, а не каждый tick.
4. После dismount близкая группа может повторно использовать ту же машину; `vehicle_generation` увеличивается, новая entity не создаётся.
5. Далёкая, уничтоженная или небезопасная машина получает `VEHICLE_ABANDONED/DESTROYED`, затем `VEHICLE_CLEANUP`.
6. Cleanup не удаляет машину с живым occupant. После освобождения entity удаляется и cap снова доступен.
7. В логе нет двух `VEHICLE_SPAWNED` с одной парой `faction/slot/group_generation/vehicle_generation`.
8. За 30 минут число vehicle entities и waypoint не растёт монотонно после завершённых cleanup-циклов.

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
VEHICLE_SPAWN_SITE_SELECTED / VEHICLE_SPAWN_SITE_REJECTED
VEHICLE_SPAWNED
VEHICLE_ASSIGNED
DRIVER_ASSIGNED / GUNNER_ASSIGNED
PASSENGERS_ASSIGNED
BOARDING_STARTED / BOARDING_COMPLETE / BOARDING_TIMEOUT
VEHICLE_ROUTE_ASSIGNED / VEHICLE_PROGRESS
DISEMBARK_STARTED / DISEMBARK_COMPLETE
VEHICLE_STUCK_DETECTED
VEHICLE_RECOVERY_STARTED / VEHICLE_RECOVERY_SUCCEEDED / VEHICLE_RECOVERY_FAILED
DRIVER_LOST / DRIVER_REASSIGNED
VEHICLE_ABANDONED / VEHICLE_DESTROYED / VEHICLE_CLEANUP
INFANTRY_FALLBACK
VEHICLE_CAP_BLOCKED
HEARTBEAT
RESULT
```

Повторяющийся warning с одинаковыми faction/slot/generations/reason каждый tick является дефектом, даже если поездка позднее завершилась.

## Итоговая матрица

| № | Проверка | Transport | Armed | Recovery/Limits | Доказательство |
|---:|---|:---:|:---:|:---:|---|
| 1 | Workbench Validate/Compile | | | | log/screenshot |
| 2 | Static lifecycle audit PASS | | | | console output |
| 3 | Stage 2 disabled regression | | | | full log |
| 4 | Safe friendly spawn US | | | | event + video |
| 5 | Safe friendly spawn USSR | | | | event + video |
| 6 | Ровно configured vehicle count | | | | entity/event count |
| 7 | Driver assigned | | | | event + observation |
| 8 | Gunner assigned | N/A | | | event + observation |
| 9 | Все живые бойцы посажены | | | | mounted=alive |
| 10 | Реальное движение и progress | | | | video + events |
| 11 | Dismount и восстановление order | | | | events + map |
| 12 | Пехотный захват после высадки | | | | owner change |
| 13 | Boarding timeout bounded | | | | causal log |
| 14 | Driver recovery bounded | | | | causal log |
| 15 | Gunner recovery bounded | N/A | | | causal log |
| 16 | Stuck recovery/fallback bounded | | | | causal log |
| 17 | Destroyed/overturned fallback | | | | causal log |
| 18 | Group generation не принимает stale vehicle | | | | generation audit |
| 19 | Cap block без warning spam | | | | event count |
| 20 | Reuse увеличивает vehicle_generation | | | | event pair |
| 21 | Cleanup без удаления живого occupant | | | | observation + event |
| 22 | Нет duplicate spawn/waypoint | | | | lifecycle audit |
| 23 | 30 минут без роста сущностей/ошибок | | | | before/after counts |
| 24 | Полные server/client logs сохранены | | | | artifact paths |

## PASS / FAIL / BLOCKED

`PASS`:

- transport и armed-light срезы выполнены на одном commit;
- все применимые строки матрицы подтверждены;
- recovery ограничен и всегда заканчивается progress либо пехотным fallback;
- лимит и cleanup соблюдаются;
- Stage 2 disabled regression чист;
- нет `[AICF][STAGE3][ERROR]`, `SCRIPT (E/F)`, duplicate spawn или бесконечного warning churn.

`FAIL`: Stage 3 загрузился, но нарушен любой инвариант: unsafe/double spawn, неполная посадка без fallback, движение без водителя, потеря группы/приказа после высадки, бесконечный recovery, spawn сверх cap, stale generation, удаление занятой живым персонажем машины или утечка entity/waypoint.

`BLOCKED`: несовместимая версия, addon/resource не загружен, compile error, занят порт или окружение не позволяет начать нужный сценарий. `BLOCKED` не является PASS.

После Stage 3 PASS всё ещё отдельно выполняются полная MVP-матрица и двухчасовой soak с техникой; текущая реализационная работа их не запускала.

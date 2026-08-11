# Stage 3.5 — Active Motorized Forces: приёмочное тестирование

Статус полной приёмки Stage 3.5: **NOT RUN**. Зафиксированный Transport-срез T: **FAIL**. Preliminary repeat smoke `Stage35-T-RepeatSmoke-20260810-224931`: **BLOCKED**, не засчитан как repeat T.

Документ совмещает бланк для ещё не выполненных обязательных срезов, фактические журналы неуспешных Transport-прогонов `Stage35-T-20260810-210932` и `Stage35-T-20260811-190311`, а также журнал заблокированного preliminary smoke. Незаполненные поля сохраняют `NOT RUN`; они не отменяют уже подтверждённый FAIL среза T и не являются runtime-доказательством. Ни один текущий T, ни заблокированный smoke, ни автоматическая компиляция, статический аудит, отдельная успешная поездка или строка Stage 3 RESULT_CANDIDATE не заявляют PASS полной Stage 3.5.

Stage 3.5 принимается только после выполнения всех обязательных срезов на одном commit: пехотного baseline B, активного планирования P, невооружённого транспорта T, отдельного armed-light среза A, recovery/replacement R, limits/cleanup L, 30-минутной матрицы M30 и двухчасового soak S120.

## Нормативная граница

Stage 3.5 проверяет:

- четыре устойчивых managed-slot на каждую сторону;
- ровно пять faction-correct бойцов в каждой initial- и replacement-группе;
- штатные 20 managed AI на фракцию и 40 одновременно без игроков;
- один групповой ticket debit за replacement, а не пять отдельных списаний;
- распределение 3 ATTACK / 1 forward DEFEND-QRF / 0 RESERVE;
- отсутствие необъяснимого idle на MOB дольше двух commander-интервалов;
- детерминированное распределение трёх ATTACK-задач и bounded retarget;
- передовую оборону и QRF-переходы D0 с hysteresis/minimum dwell;
- vehicle eligibility всех четырёх slot;
- минимальную боеспособность группы для **нового** vehicle request; уже назначенная пригодная машина остаётся доступной выжившим и не отбирается только из-за последующих потерь;
- не более одной active/reserved машины на живой slot и active cap 4 на фракцию;
- capacity preflight и политику ALL_OR_FALLBACK для всех живых бойцов;
- предпочтение truck для A0/A1 и вместительного unarmed light transport для A2/D0;
- SAFE_REUSE при смене цели;
- bounded request/recovery/fallback после потери машины;
- отдельный player-safe functional world pool;
- устойчивость 30 минут и два часа до начала Stage 4.

Stage 3.5 не добавляет supply, economy, строительство или сложную логистику. Эти системы не используются для маскировки дефектов состава, планирования или транспорта.

## Унаследованный safety-контракт Stage 3

Все применимые инварианты docs/STAGE_3_TESTING.md обязательны и для Stage 3.5. Новый размер групп и расширение eligibility не ослабляют существующий контракт:

1. Authority оценивает несколько safe-site позиций и отклоняет water/undrivable surface до `SpawnEntityPrefabEx`. Отклонённая позиция не создаёт live entity, не привязывает vehicle и не продвигает vehicle generation; request runtime и cap reservation к этому моменту уже могут существовать. Spawned prefab принимается и привязывается только после faction/resource и role-compatible capacity preflight, а отклонённая пустая entity безопасно удаляется до следующего кандидата.
2. Обычная посадка не использует teleport-in, remote GetIn или snap к машине.
3. Для transport соблюдается DRIVER → PASSENGERS; для armed-light — DRIVER → GUNNER → PASSENGERS.
4. До обязательного crew не выдаются passenger actions. После crew каждый будущий пассажир получает отдельную atomic reservation точного `CargoCompartmentSlot`; ожидается `PASSENGERS_ASSIGNED policy=EXACT_PER_MEMBER_CARGO_AFTER_CREW`.
5. Passenger token сохраняет action state/current/retry, назначенные compartment manager/slot и reservation; settled засчитывается только при совпадении фактических manager/slot с назначенными. Reissue допускается только bounded и с `transition_fenced=1`; terminal/wrong-compartment outcome очищает reservations/actions и завершает runtime fail-closed.
6. BOARDING_COMPLETE разрешён только после двух settled poll всех живых членов именно в назначенной машине.
7. Для полного controlled roster ожидается settled 5/5. После боевых потерь уже назначенная пригодная машина продолжает использовать mounted=alive. Новый vehicle request разрешён только при достижении явно заданного minimum combat-ready состава (штатно 3); группа ниже порога получает `VEHICLE_REQUEST_INELIGIBLE policy=NEW_REQUEST_ONLY assigned_vehicle_present=<0|1> assigned_vehicle_policy=PRESERVE_EXISTING`, а не новую машину. `GROUP_NOT_COMBAT_READY` ожидается только при завершении уже существующего pending runtime в состоянии REQUESTED/WAITING без assigned vehicle; pre-runtime rejection не обязан создавать такой runtime/fallback event.
8. Boarding, dismount, crew recovery, mobility recovery и fallback имеют абсолютные bounded deadline и attempt budget.
9. До normal dismount timeout физически застрявшему бойцу выдаётся bounded per-member movement guidance (`DISEMBARK_CLEARANCE_GUIDANCE`), без relocation/teleport. `DISEMBARK_CLEARANCE_RECOVERY` с relocation допустим только в terminal/fallback fail-closed recovery и не доказывает штатный dismount PASS.
10. Успех mobility recovery подтверждается только последующим VEHICLE_MOTION либо достаточным route progress.
11. Target change сохраняет пригодную достижимую машину; одна смена цели не создаёт вторую entity.
12. Protected ALIVE/INCAPACITATED occupants, включая foreign occupants, блокируют destructive cleanup.
13. Player occupancy, target-scoped enter/exit transition и игрок в радиусе 15 м блокируют delete; нужен непрерывный 5-секундный clear и повторный scan.
14. Исправная abandoned-машина немедленно освобождает active AI cap, переходит в player-safe world pool и не удаляется ради новой AI trip.
15. World-pool target является soft cap: protected overflow остаётся в мире до безопасного oldest-entry retirement.
16. Group generation, vehicle generation, EntityID и RplId не могут быть перепутаны между replacement/reuse/cleanup.
17. Нет duplicate spawn, бесконечного warning/state churn, stale waypoint или бесконечного recovery/fallback.
18. RESULT_CANDIDATE status=READY final=0 остаётся только кандидатом. Любой последующий acceptance failure обязан инвалидировать его.

Полный журнал важнее отфильтрованных строк. Stock/resource/UI ошибки сохраняются, классифицируются по symbol/stack/resource context и не исключаются из evidence молча.

## Срезы приёмки

| Срез | Назначение | Обязательный режим | Статус |
|---|---|---|:---:|
| B | Пехотный baseline 4 × 5 со старым распределением ролей | active roles off, vehicles off | NOT RUN |
| P | 3 ATTACK / 1 forward DEFEND-QRF, распределение целей и hysteresis | active roles on, vehicles off | NOT RUN |
| T | Четыре невооружённых транспорта на сторону | active roles on, transport=4, cap=4 | FAIL |
| A | Отдельная проверка armed-light и capacity policy пятёрки | новый профиль после T | NOT RUN |
| R | Replacement, retarget, SAFE_REUSE, crew/mobility recovery и fallback | отдельный профиль на fault | NOT RUN |
| L | Active cap, cap fault, world pool, guarded cleanup и stop cleanup | отдельные чистые профили | NOT RUN |
| M30 | 30-минутная headless-матрица штатной конфигурации | transport=4, cap=4 | NOT RUN |
| S120 | Двухчасовой headless soak штатной конфигурации | transport=4, cap=4 | NOT RUN |

Порядок выполнения: B → P → T → A/R/L → M30 → S120. Срезы A, R и L можно выполнять в любом порядке после успешного T, но каждый fault использует отдельный чистый профиль.

## Общие метаданные комплекта

| Поле | Значение |
|---|---|
| Версия Arma Reforger | ______________________________ |
| Версия Arma Reforger Server/Diag | engine `190965` по T server log |
| Версия Reforger Tools | `1.7.0.54` по `ArmaReforgerWorkbenchSteamDiag.exe` |
| Ветка/тег | NOT RECORDED — evidence gap |
| Commit, полный SHA | NOT RECORDED — evidence gap; повтор T обязан зафиксировать SHA |
| Дата начала | 2026-08-10 21:09:33, Transport T |
| Дата окончания | ______________________________ |
| Тестировщик | ______________________________ |
| Карта/mission header | Arland / `Missions/23_Campaign_Arland.conf` |
| Путь к repository | `C:\Users\retar\IdeaProjects\Arma-Reforger-AI-Conflict` |
| Результат Validate Scripts | PASS — локальный Workbench compile-smoke текущего рабочего снимка; `.cache/stage35-newrun-telemetry-final7-validate-20260811/console.log` |
| Результат Compile and Reload Scripts | PASS — `Module: Game; loaded 5675x files; 11049x classes`, `Game successfully created`, без AICF `SCRIPT(E)`/`ENGINE(F)`/`Broken expression`; только development evidence |
| Результат Stage 3 static audit | PASS — `tools/Test-Stage3Static.ps1`; только development/static evidence, не runtime-приёмка |
| Результат Stage 3.5 static audit | PASS — `tools/Test-Stage35Static.ps1`; только development/static evidence, не runtime-приёмка |
| Общий итог | NOT RUN |

## Метаданные отдельного прогона

Этот блок копируется и заполняется для каждого профиля.

| Поле | Значение |
|---|---|
| Срез / fault | ______________________________ |
| Run ID | ______________________________ |
| Commit | ______________________________ |
| Дата/время старта | ______________________________ |
| Дата/время остановки | ______________________________ |
| Длительность | ______________________________ |
| Server profile | ______________________________ |
| Server console.log | ______________________________ |
| Client profile/log | ______________________________ |
| Полная server command line | ______________________________ |
| Полная client command line | ______________________________ |
| Число игроков | ______________________________ |
| Сторона игрока | US / USSR / NONE |
| CONFIG/RELIABILITY_CONFIG/Stage 3 CONFIG | ______________________________ |
| Первая AICF error/failure с контекстом | NONE / ______________________________ |
| Stock/resource/UI caveats | NONE / ______________________________ |
| Итог прогона | NOT RUN |
| Evidence bundle | ______________________________ |

### Фактические метаданные Transport T

| Поле | Значение |
|---|---|
| Срез / fault | T — штатный transport=4, armed-light=0, cap=4 |
| Profile / run ID | `Stage35-T-20260810-210932` / `stage1-server-11212` |
| Commit | NOT RECORDED — evidence gap; результат нельзя переносить на другой SHA |
| Дата/время старта | log start `2026-08-10 21:09:33`; `MATCH_START` 21:09:44.217 |
| Дата/время остановки | Не остановлен при первичном заполнении; журнал продолжал изменяться как минимум до 22:09:11. Это FAIL-наблюдение, не завершённый M30/S120 evidence bundle |
| Server profile | `C:\Users\retar\AppData\Local\AICF\Stage35-T-20260810-210932` |
| Server console.log | `C:\Users\retar\AppData\Local\AICF\Stage35-T-20260810-210932\logs\logs_2026-08-10_21-09-32\console.log` |
| Сопутствующие server logs | в том же каталоге: `crash.log`, `error.log`, `script.log`; на проверенной границе файлы пусты |
| Client profile/log | NOT RECORDED — visual evidence нельзя считать полным client bundle |
| Полная server command line | `ArmaReforgerServerDiag.exe -gproj C:\Users\retar\IdeaProjects\Arma-Reforger-AI-Conflict\AIConflictArland\addon.gproj -server worlds/MP/CTI_Campaign_Arland.ent -MissionHeader Missions/23_Campaign_Arland.conf -worldSystemsConfig Configs/Systems/ConflictSystems.conf -addonsDir "C:\Users\retar\IdeaProjects\Arma-Reforger-AI-Conflict,C:\Program Files (x86)\Steam\steamapps\common\Arma Reforger Server\addons" -addons 9178E5822AFE48EA,B52C5F6AEDBF423E -profile C:\Users\retar\AppData\Local\AICF\Stage35-T-20260810-210932 -aicfActiveForcesRolesEnabled 1 -aicfMaxManagedAgents 64 -aicfRequirePlayerForResult 0 -aicfVehiclesEnabled 1 -aicfTransportVehiclesPerFaction 4 -aicfArmedLightVehiclesPerFaction 0 -aicfMaxVehiclesPerFaction 4 -backendFreshSession -maxFPS 60 -logStats 10000 -noThrow` |
| Полная client command line | NOT RECORDED — evidence gap |
| CONFIG | Stage 1: groups=4, max_managed_agents=64; Stage 3: enabled=1, transports=4, armed_light=0, cap=4; Stage 3.5: group_size=5, active_roles=1, attack/defend/reserve=3/1/0 |
| Первая AICF error/failure | 21:19:00.461, `[STAGE3.5][ERROR][MOB_IDLE_DEADLINE_MISSED]`, USSR A1 |
| Итог прогона | **FAIL** |
| Evidence boundary | Все приведённые ниже факты относятся к указанному server console.log. Поскольку профиль оставался живым, повторный анализ обязан фиксировать точный stop/cutoff и включать все более поздние ошибки |

## Перед каждым прогоном

1. Зафиксировать commit и версии Game/Tools/Server.
2. Не менять исходный код между срезами одной приёмочной серии.
3. Открыть AIConflictArland в Workbench и дождаться Resource Database.
4. Выполнить Build → Validate Scripts и Build → Compile and Reload Scripts.
5. Выполнить действующий Stage 3 static audit:

~~~powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File ./tools/Test-Stage3Static.ps1
~~~

6. Выполнить Stage 3.5 static audit и приложить полный вывод:

~~~powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File ./tools/Test-Stage35Static.ps1
~~~

7. Любая AICF script/resource/dependency error означает BLOCKED до исправления среды или сборки.
8. Каждый runtime-прогон использует новый -profile и -backendFreshSession.
9. Server и client используют Diag executables одной сборки; Retail/Diag не смешиваются.
10. Перед fault-injection записать нормальное состояние и точный момент вмешательства.
11. Game Master/teleport допустим только для создания явно записанного fault condition и не является доказательством нормального движения, посадки или захвата.
12. После остановки сохранить весь каталог logs, а не только строки AICF.

## CLI-профили

Флаг aicfActiveForcesRolesEnabled переключает только распределение и планирование ролей. В обоих режимах B и P размер initial/replacement группы остаётся равным пяти.

| Параметр | B | P | T | A | R/L | M30/S120 |
|---|---:|---:|---:|---:|---:|---:|
| aicfActiveForcesRolesEnabled | 0 | 1 | 1 | 1 | 1 | 1 |
| aicfMaxManagedAgents | 64 | 64 | 64 | 64 | 64 | 64 |
| aicfRequirePlayerForResult | 0 | 0 | 0 | 0 | 0 | 0 |
| aicfVehiclesEnabled | 0 | 0 | 1 | 1 | 1 | 1 |
| aicfTransportVehiclesPerFaction | 0 | 0 | 4 | 3 | по сценарию | 4 |
| aicfArmedLightVehiclesPerFaction | 0 | 0 | 0 | 1 | по сценарию | 0 |
| aicfMaxVehiclesPerFaction | 0 | 0 | 4 | 4 | 4 или fault cap | 4 |
| aicfVehicleMinimumRequestAgents | 3 | 3 | 3 | 3 | 3 | 3 |

Обязательный baseline override B:

~~~text
-aicfActiveForcesRolesEnabled 0
-aicfVehiclesEnabled 0
-aicfTransportVehiclesPerFaction 0
-aicfArmedLightVehiclesPerFaction 0
-aicfMaxVehiclesPerFaction 0
~~~

Обязательный active override P:

~~~text
-aicfActiveForcesRolesEnabled 1
-aicfVehiclesEnabled 0
~~~

Обязательный штатный motorized override T/M30/S120:

~~~text
-aicfActiveForcesRolesEnabled 1
-aicfVehiclesEnabled 1
-aicfTransportVehiclesPerFaction 4
-aicfArmedLightVehiclesPerFaction 0
-aicfMaxVehiclesPerFaction 4
~~~

Для M30/S120 применяются нормальные Stage 2/3 значения:

| Параметр | Значение |
|---|---:|
| aicfWarTempoPercent | 100 |
| aicfCommanderIntervalMs | default, фактическое значение из CONFIG |
| aicfReliabilityIntervalMs | 5000 |
| aicfStuckTimeoutMs | 120000 |
| aicfStuckProgressMeters | 25 |
| aicfMaxStuckRecoveries | 3 |
| aicfObjectiveHoldTimeoutMs | 300000 |
| aicfMaxConcurrentSpawns | 1 |
| aicfVehicleBoardingTimeoutMs | 60000 |
| aicfVehicleStuckTimeoutMs | 120000 |
| aicfVehicleProgressMeters | 25 |
| aicfVehicleMotionMeters | 3 |
| aicfVehicleObjectiveProgressTimeoutMs | 300000 |
| aicfVehicleMaxRecoveries | 2 |
| aicfVehicleSpawnMaxAttempts | 4 |
| aicfVehicleRetryIntervalMs | 10000 |
| aicfVehicleRetryBackoffMaxMs | 60000 |
| aicfVehicleWaitProbeIntervalMs | 60000 |
| aicfVehicleAbandonedWorldPoolPerFaction | 4 |

Ускоренные значения разрешены только в явно названном fault-срезе R/L. Они не заменяют M30/S120 на нормальной конфигурации.

## Канонический запуск dedicated server

Окно PowerShell №1:

~~~powershell
$serverRoot = "C:/Program Files (x86)/Steam/steamapps/common/Arma Reforger Server"
$repoRoot = "C:/Users/<имя>/IdeaProjects/Arma-Reforger-AI-Conflict"
$slice = "T"
$stamp = Get-Date -Format "yyyyMMdd-HHmmss"
$profileRoot = "$env:LOCALAPPDATA/AICF/Stage35-$slice-$stamp"

Set-Location $serverRoot
& "$serverRoot/ArmaReforgerServerDiag.exe" `
  -gproj "$repoRoot/AIConflictArland/addon.gproj" `
  -server "worlds/MP/CTI_Campaign_Arland.ent" `
  -MissionHeader "Missions/23_Campaign_Arland.conf" `
  -worldSystemsConfig "Configs/Systems/ConflictSystems.conf" `
  -addonsDir "$repoRoot,$serverRoot/addons" `
  -addons "9178E5822AFE48EA,B52C5F6AEDBF423E" `
  -profile "$profileRoot" `
  -aicfActiveForcesRolesEnabled 1 `
  -aicfMaxManagedAgents 64 `
  -aicfRequirePlayerForResult 0 `
  -aicfVehiclesEnabled 1 `
  -aicfTransportVehiclesPerFaction 4 `
  -aicfArmedLightVehiclesPerFaction 0 `
  -aicfMaxVehiclesPerFaction 4 `
  -aicfVehicleMinimumRequestAgents 3 `
  -backendFreshSession `
  -maxFPS 60 `
  -logStats 10000
~~~

Для B/P/T/A/R/L/M30/S120 заменить только параметры согласно таблице и обязательно создать новый profile.

Окно PowerShell №2:

~~~powershell
$gameRoot = "C:/Program Files (x86)/Steam/steamapps/common/Arma Reforger"
$repoRoot = "C:/Users/<имя>/IdeaProjects/Arma-Reforger-AI-Conflict"
$slice = "T"
$clientStamp = Get-Date -Format "yyyyMMdd-HHmmss"
$clientProfile = "$env:LOCALAPPDATA/AICF/Stage35-Client-$slice-$clientStamp"

Set-Location $gameRoot
& "$gameRoot/ArmaReforgerSteamDiag.exe" `
  -gproj "$repoRoot/AIConflictArland/addon.gproj" `
  -client 127.0.0.1 `
  -addonsDir "$repoRoot,$gameRoot/addons" `
  -addons "9178E5822AFE48EA,B52C5F6AEDBF423E" `
  -profile "$clientProfile" `
  -backendFreshSession
~~~

Окно наблюдения:

~~~powershell
$log = Get-ChildItem "$profileRoot/logs" -Filter console.log -File -Recurse |
  Sort-Object LastWriteTime -Descending |
  Select-Object -First 1

Get-Content -LiteralPath $log.FullName -Wait |
  Select-String -Pattern "\[AICF\]\[(STAGE1|STAGE2|STAGE3|STAGE3\.5)\]"
~~~

Префикс `[AICF][STAGE3.5]` обязателен для roster/planning/capacity/activity evidence; существующие Stage 1/2/3 события сохраняются и используются совместно с ним. Для приёмки важны поля и причинная последовательность.

## Общий лог-контракт Stage 3.5

Для конкретного managed slot доказательства должны позволять сопоставить:

- run и монотонный t_ms;
- faction;
- стабильный numeric slot и отображаемую identity A0/A1/A2/D0;
- role и task mode;
- group и group_generation;
- фактическое число живых/всего бойцов;
- target, assignment reason и commander interval;
- vehicle kind, vehicle_generation, EntityID/RplId и capacity;
- transition trigger, dwell/age и решение hysteresis;
- active cap и world-pool counters.

Обязательные существующие события используются без ослабления:

~~~text
CONFIG
ROLE_ASSIGNED
GROUP_SPAWNED
SLOT_READY
ROSTER_READY
ORDER_ASSIGNED
TARGET_REASSIGNED
BASE_OWNER_CHANGED
GROUP_EMPTY
REINFORCEMENT_SCHEDULED
REINFORCEMENT_SPAWNED
TICKET_DEBIT
HEARTBEAT
RELIABILITY_HEARTBEAT

VEHICLE_REQUESTED
VEHICLE_SPAWN_CANDIDATES_EVALUATED
VEHICLE_SPAWN_PREFLIGHT_READY
VEHICLE_SPAWN_SITE_SELECTED / VEHICLE_SPAWN_SITE_REJECTED
VEHICLE_SPAWNED
VEHICLE_ASSIGNED
BOARDING_STARTED / BOARDING_PHASE_STARTED / BOARDING_COMPLETE / BOARDING_TIMEOUT
DRIVER_ASSIGNED / GUNNER_ASSIGNED / PASSENGERS_ASSIGNED
BOARDING_PROGRESS / BOARDING_ACTION_OWNERSHIP
PASSENGER_BOARDING_REISSUED / PASSENGER_BOARDING_ACTION_FAILED
VEHICLE_ROUTE_ASSIGNED / VEHICLE_MOTION / VEHICLE_PROGRESS
DISEMBARK_STARTED / DISEMBARK_COMPLETE / DISEMBARK_TIMEOUT
DISEMBARK_CLEARANCE_GUIDANCE
DISEMBARK_CLEARANCE_RECOVERY (только terminal/fallback recovery)
DRIVER_LOST / DRIVER_REASSIGNED / GUNNER_LOST / GUNNER_REASSIGNED
VEHICLE_STUCK_DETECTED
VEHICLE_RECOVERY_STARTED / VEHICLE_RECOVERY_SUCCEEDED / VEHICLE_RECOVERY_FAILED
VEHICLE_UNSTUCK_STARTED / VEHICLE_UNSTUCK_ATTEMPT / VEHICLE_UNSTUCK_SUCCEEDED / VEHICLE_UNSTUCK_FAILED
INFANTRY_FALLBACK
VEHICLE_WORLD_POOL_RELEASED / VEHICLE_WORLD_POOL_SOFT_OVERFLOW
VEHICLE_CLEANUP_DEFERRED / VEHICLE_DELETE_REQUESTED / VEHICLE_CLEANUP_CONFIRMED
VEHICLE_STOP_CLEANUP_STARTED / VEHICLE_STOP_CLEANUP_CONFIRMED / VEHICLE_STOP_CLEANUP_RETAINED
VEHICLE_CAP_BLOCKED
RESULT_CANDIDATE / ACCEPTANCE_FAILURE_LATCHED / RESULT
~~~

Новый префикс реализации Stage 3.5:

~~~text
[AICF][STAGE3.5]

CONFIG
GROUP_ENTITY_SPAWNED
GROUP_ROSTER_READY
STRATEGIC_ASSIGNMENT
STRATEGIC_CANDIDATE_HELD
DEFEND_POSTURE_CHANGED
VEHICLE_CAPACITY_PREFLIGHT
VEHICLE_TRANSPORT_FALLBACK
VEHICLE_REQUEST_INELIGIBLE
VEHICLE_SPAWN_CANDIDATE_REJECTED
FORCE_HEARTBEAT
SLOT_ACTIVITY
MEANINGFUL_TASK_LOST / MEANINGFUL_TASK_RECOVERED
ORDER_RESTORE_REQUESTED / ORDER_RESTORE_RESULT
WAYPOINT_REMOVED / WAYPOINT_BIND_MISMATCH
ABANDONED_EXIT_AUDIT / FORCE_DISEMBARK_MEMBER
IDLE_DEADLINE_SUPPRESSED
COHESION_OUTCOME / WAITING_FOR_SITE_EXIT

ERROR: GROUP_ROSTER_CONFIG_INVALID
ERROR: GROUP_ROSTER_REJECTED
ERROR: MOB_IDLE_DEADLINE_MISSED
ERROR: MEANINGFUL_TASK_DEADLINE_MISSED
~~~

`FORCE_HEARTBEAT` содержит `managed_groups`, `managed_agents`, `managed_waypoints`, приближённый `tracked_entities`, раздельные active/reserved/world-pool counters и vehicle cap. `SLOT_ACTIVITY` содержит numeric и role-local slot, state/role/posture, alive, target, подтверждённое присутствие infantry/vehicle waypoint в group queue, vehicle state, положение относительно MOB, meaningful-task, разрешённое исключение idle, `unexplained_idle_ms`, taskless/restore/failure/suppression context.

Любое из четырёх перечисленных Stage 3.5 `ERROR`-событий делает прогон `FAIL`: roster не был безопасно сформирован/подтверждён либо active slot превысил допустимые два commander-интервала необъяснимого idle на MOB или глобально остался без meaningful task.

Для forward-defense/QRF реализации записать фактические event names:

| Семантическое событие | Фактическое event name | Обязательные поля | Статус |
|---|---|---|:---:|
| Назначена передовая оборона | `STRATEGIC_ASSIGNMENT posture=FORWARD_DEFEND` | faction, slot, posture, target, trigger, reason | NOT RUN |
| QRF активирован | `STRATEGIC_ASSIGNMENT` или `DEFEND_POSTURE_CHANGED`, `posture=QRF` | trigger, target, dwell_ms/waypoint | NOT RUN |
| QRF удержан hysteresis | `STRATEGIC_CANDIDATE_HELD` | current/desired posture/target, assignment/candidate age, remaining dwell/stable | NOT RUN |
| QRF стабилизирован | `DEFEND_POSTURE_CHANGED` или `STRATEGIC_ASSIGNMENT` | target, trigger, dwell/waypoint evidence | NOT RUN |
| Возврат на передовую оборону | `STRATEGIC_ASSIGNMENT` или `DEFEND_POSTURE_CHANGED`, `posture=FORWARD_DEFEND` | target, trigger, dwell/waypoint evidence | NOT RUN |
| ATTACK-направления распределены | `STRATEGIC_ASSIGNMENT` | `ATTACK_PRIMARY/SECONDARY/SUPPORT`, target, ranked trigger | NOT RUN |
| Idle на MOB объяснён | `SLOT_ACTIVITY` | at_mob, meaningful_task, allowed_idle_reason, unexplained_idle_ms | NOT RUN |

Отсутствие диагностического доказательства перехода, dwell или fallback делает соответствующую строку матрицы FAIL, даже если визуально waypoint выглядел правдоподобно.

## Срез B — infantry baseline 4 × 5

Конфигурация:

~~~text
aicfActiveForcesRolesEnabled=0
aicfVehiclesEnabled=0
aicfTransportVehiclesPerFaction=0
aicfArmedLightVehiclesPerFaction=0
aicfMaxVehiclesPerFaction=0
aicfMaxManagedAgents=64
aicfRequirePlayerForResult=0
~~~

Ожидается:

1. Четыре US и четыре USSR slot создаются на одном запуске.
2. Каждая initial-группа содержит ровно пять faction-correct бойцов.
3. Старое распределение ролей остаётся 2 ATTACK / 1 DEFEND / 1 RESERVE.
4. Все восемь групп получают обычный пехотный order.
5. Heartbeat без игроков показывает managed_agents=40.
6. Нет VEHICLE_REQUESTED, vehicle entity и Stage 3 vehicle runtime.
7. Formation, cohesion, stuck/recovery и map marker используют фактический состав.
8. Нет size-dependent ошибки, duplicate binding или AI-limit блокировки.

### Roster B

| Faction | Numeric slot | Expected role | Group | Generation | GROUP_SPAWNED initial_agents | SLOT_READY agents | 5/5 faction-correct | Order | Статус | Evidence |
|---|---:|---|---|---:|---:|---:|---|---|:---:|---|
| US | 0 | ATTACK | | | | | | | NOT RUN | |
| US | 1 | ATTACK | | | | | | | NOT RUN | |
| US | 2 | DEFEND | | | | | | | NOT RUN | |
| US | 3 | RESERVE | | | | | | | NOT RUN | |
| USSR | 0 | ATTACK | | | | | | | NOT RUN | |
| USSR | 1 | ATTACK | | | | | | | NOT RUN | |
| USSR | 2 | DEFEND | | | | | | | NOT RUN | |
| USSR | 3 | RESERVE | | | | | | | NOT RUN | |

Heartbeat managed_agents=40: **NOT RUN**

Итог B: **NOT RUN**

## Срез P — активные роли и QRF

Конфигурация:

~~~text
aicfActiveForcesRolesEnabled=1
aicfVehiclesEnabled=0
aicfMaxManagedAgents=64
aicfRequirePlayerForResult=0
~~~

Ожидается:

1. Stable identities на сторону: A0, A1, A2 и D0.
2. Роли: три ATTACK, один DEFEND с task mode FORWARD_DEFEND или QRF, ноль RESERVE.
3. ATTACK-группы получают primary, соседнее достижимое направление и support assignment.
4. Если граф временно даёт меньше трёх содержательных направлений, детерминированный fallback явно логируется; молчаливое случайное stacking запрещено.
5. D0 выбирает ближайшую к противнику безопасную friendly-базу. HQ допускается только как объяснённый fallback при отсутствии иной безопасной позиции или непосредственной угрозе.
6. Отдельно воспроизводятся contested, потеря соседней базы и HQ-threat.
7. После стабилизации D0 возвращается на передовую оборону.
8. Hysteresis/minimum dwell запрещает role/waypoint churn каждый commander tick.
9. После owner/contested change новый содержательный task появляется не позднее двух commander-интервалов.
10. Нахождение на MOB дольше двух интервалов имеет только разрешённую диагностируемую причину.

### Planning P

| Faction | Identity | Numeric slot | Role | Assignment mode | Target | Trigger/reason | Dwell ms | Commander latency | Churn absent | Статус | Evidence |
|---|---|---:|---|---|---|---|---:|---:|---|:---:|---|
| US | A0 | 0 | ATTACK | PRIMARY | | | | | | NOT RUN | |
| US | A1 | 1 | ATTACK | ADJACENT | | | | | | NOT RUN | |
| US | A2 | 2 | ATTACK | SUPPORT | | | | | | NOT RUN | |
| US | D0 | 3 | DEFEND | FORWARD_DEFEND/QRF | | | | | | NOT RUN | |
| USSR | A0 | 0 | ATTACK | PRIMARY | | | | | | NOT RUN | |
| USSR | A1 | 1 | ATTACK | ADJACENT | | | | | | NOT RUN | |
| USSR | A2 | 2 | ATTACK | SUPPORT | | | | | | NOT RUN | |
| USSR | D0 | 3 | DEFEND | FORWARD_DEFEND/QRF | | | | | | NOT RUN | |

### QRF fault-table P

| Faction | Scenario | T0 event | QRF task/event | Latency <= 2 intervals | Minimum dwell respected | Stable evidence | Return event | Статус |
|---|---|---|---|---|---|---|---|:---:|
| US | CONTESTED | | | | | | | NOT RUN |
| US | ADJACENT_BASE_LOST | | | | | | | NOT RUN |
| US | HQ_THREAT | | | | | | | NOT RUN |
| USSR | CONTESTED | | | | | | | NOT RUN |
| USSR | ADJACENT_BASE_LOST | | | | | | | NOT RUN |
| USSR | HQ_THREAT | | | | | | | NOT RUN |

Итог P: **NOT RUN**

## Срез T — четыре невооружённых транспорта

Конфигурация:

~~~text
aicfActiveForcesRolesEnabled=1
aicfVehiclesEnabled=1
aicfTransportVehiclesPerFaction=4
aicfArmedLightVehiclesPerFaction=0
aicfMaxVehiclesPerFaction=4
aicfVehicleMinimumRequestAgents=3
aicfMaxManagedAgents=64
aicfRequirePlayerForResult=0
~~~

Для US и USSR отдельно проверить:

1. A0/A1/A2/D0 проходят одну и ту же route/cap eligibility-проверку, но **новый** vehicle request дополнительно требует minimum combat-ready живого состава.
2. Ниже порога новая машина не создаётся: требуется `VEHICLE_REQUEST_INELIGIBLE alive=<n> required_minimum=3 policy=NEW_REQUEST_ONLY assigned_vehicle_present=<0|1> assigned_vehicle_policy=PRESERVE_EXISTING`. Pre-runtime rejection продолжает suppression/current infantry assignment без `VEHICLE_REQUESTED`; `GROUP_NOT_COMBAT_READY` требуется только для уже pending REQUESTED/WAITING runtime без assigned vehicle.
3. Уже назначенная исправная машина остаётся доступной выжившим и не отбирается только из-за падения alive ниже new-request threshold.
4. Active и reserved counters не превышают четыре на фракцию; если их множества пересекаются, heartbeat и отчёт явно фиксируют семантику и не складывают их как независимые entity.
5. Один slot не имеет более одной active/reserved машины.
6. A0/A1 предпочитают faction-correct truck.
7. A2/D0 предпочитают faction-correct unarmed light transport, только если capacity достаточна для всех живых.
8. US-кандидаты проверяются среди M923A1 и подходящего M998; USSR — «Урал» и UAZ-452 transport.
9. Candidate trace содержит catalog/resource result и фактическую accessible capacity.
10. До `SpawnEntityPrefabEx` surface trace содержит `candidate_index`, `candidates`, authoritative `origin`, `surface`, `water`, `footprint_delta_m` и число `probes`. Water/uneven position даёт `VEHICLE_SPAWN_CANDIDATE_REJECTED` без live entity, vehicle binding и продвижения vehicle generation; request runtime/cap reservation уже могут существовать. Достижимость дальнейшего route подтверждается отдельным route evidence и не выводится из surface preflight.
11. Недостаточный light candidate приводит к truck либо bounded infantry fallback без частичной посадки.
12. До DRIVER_ASSIGNED mounted=0.
13. После crew ожидается `PASSENGERS_ASSIGNED policy=EXACT_PER_MEMBER_CARGO_AFTER_CREW requested=<n> issued=<n>`. `BOARDING_ACTION_OWNERSHIP` перечисляет каждый живой EntityID и token state/current/retry, assigned compartment manager/slot, reserved, actual compartment manager/slot, а также group/vehicle generation; missing member указывается явно, а не только агрегатом mounted.
14. Любой `PASSENGER_BOARDING_REISSUED` bounded и содержит `transition_fenced=1`; `PASSENGER_BOARDING_ACTION_FAILED`, terminal/wrong/unsupported compartment или утечка reservation/action являются FAIL чистого повтора.
15. BOARDING_COMPLETE появляется после двух settled poll и для полного состава содержит mounted=5/alive=5.
16. Пять бойцов действительно движутся в одной машине, штатно высаживаются и продолжают infantry order. Normal clearance использует только bounded `DISEMBARK_CLEARANCE_GUIDANCE`; relocation/teleport разрешён только как terminal/fallback `DISEMBARK_CLEARANCE_RECOVERY`, не как штатный путь.
17. Нет snap/teleport-in; это подтверждается authoritative member samples и client video.
18. Смена target использует ту же исправную EntityID/RplId с SAFE_REUSE.
19. Released usable vehicle освобождает AI cap и переходит в world pool.

### Transport T

| Faction | Identity | Group generation | Alive | Preferred class | Candidate trace | Selected prefab | Accessible capacity | Vehicle ID/generation | BOARDING settled | Route/dismount | Статус | Evidence |
|---|---|---:|---:|---|---|---|---:|---|---|---|:---:|---|
| US | A0 | 1 initial | 5 | TRUCK | M923A1 accepted, required=5/available=15 | M923A1 transport | 15 | `0x4000000000001336` / 1 | 5/5, 21:10:11 | initial ROAD_REACHABLE; later gen2 passenger timeout | FAIL | `BOARDING_COMPLETE`; `BOARDING_TIMEOUT` 21:33:17 и 21:58:23 |
| US | A1 | 1 initial | 5 → 1 | TRUCK | M923A1 accepted; later singleton request accepted 1/15 | M923A1 transport | 15 | `0x400000000000136E` / 1; singleton `0x4000000000001BB2` | 5/5 initial; 1/1 singleton | initial vehicle lost; new-request combat-ready policy violated | FAIL | S35-T-2 |
| US | A2 | 1 initial | 5 | LIGHT→TRUCK fallback | M998 4 seats rejected; M923A1 15 accepted | M923A1 transport | 15 | `0x40000000000013DB` / 1 | 5/5, 21:10:09 | later replacement exhausted driver recovery | FAIL | capacity fallback; S35-T-6 |
| US | D0 | 1 initial | 5 | LIGHT→TRUCK fallback | M998 4 seats rejected; M923A1 15 accepted | M923A1 transport | 15 | `0x4000000000001448` / 1 | 5/5, 21:10:09 | repeated clearance recovery; later driver recovery exhausted | FAIL | `DISEMBARK_CLEARANCE_RECOVERY`; S35-T-6 |
| USSR | A0 | 1 initial | 5 | TRUCK | Ural accepted, required=5/available=15 | Ural4320 transport | 15 | `0x4000000000001480` / 1 | 5/5, 21:10:06 | replacement gen2 boarded only 3/5 and timed out | FAIL | S35-T-1/S35-T-4 |
| USSR | A1 | 1 initial | 5 | TRUCK | Ural accepted, required=5/available=15 | Ural4320 transport | 15 | `0x40000000000014C2` / 1 | 5/5, 21:10:06 | persistent stuck, prolonged site/churn and Failed move VM exceptions | FAIL | S35-T-1/S35-T-3/S35-T-6 |
| USSR | A2 | 1 initial | 5 | LIGHT | UAZ-452 accepted, required=5/available=8 | UAZ-452 transport | 8 | `0x4000000000001504` / 1 | 5/5, 21:10:06 | persistent stuck; fallback disembark error | FAIL | S35-T-1/S35-T-6; `FALLBACK_DISEMBARK_FAILED` |
| USSR | D0 | 1 initial | 5 | LIGHT | UAZ-452 accepted, required=5/available=8 | UAZ-452 transport | 8 | `0x400000000000154B` / 1 | 5/5, 21:10:10 | driver recovery exhausted; later replacement 5/5 | FAIL | S35-T-1/S35-T-6 |

Наблюдаемый максимум US: `active=4`, `reserved=4`; counters совпадают и не доказаны как независимые множества. Unique active/reserved entity bound: **NOT RUN** до отдельной корреляции EntityID.

Наблюдаемый максимум USSR: `active=4`, `reserved=4`; counters совпадают и не доказаны как независимые множества. Unique active/reserved entity bound: **NOT RUN** до отдельной корреляции EntityID.

Итог T: **FAIL**

## Срез A — отдельный armed-light/capacity

Запускается только новым процессом и профилем после успешного T. Основной диагностический профиль:

~~~text
aicfActiveForcesRolesEnabled=1
aicfVehiclesEnabled=1
aicfTransportVehiclesPerFaction=3
aicfArmedLightVehiclesPerFaction=1
aicfMaxVehiclesPerFaction=4
aicfMaxManagedAgents=64
aicfRequirePlayerForResult=0
~~~

Фактический slot, которому назначен ARMED_LIGHT, фиксируется из CONFIG/vehicle diagnostics.

Для обеих фракций допустим только один из двух доказанных исходов:

- prefab имеет accessible capacity не меньше alive=5: соблюдены DRIVER → GUNNER → PASSENGERS, driver=1, gunner=1, settled mounted=5;
- prefab не вмещает всю пятёрку: capacity preflight отклоняет его до частичной посадки, затем выполняется штатный transport selection либо bounded infantry fallback.

Armed-light не может молча уменьшить группу, оставить бойца снаружи или считаться обычным подходящим transport без capacity evidence.

| Faction | Slot/identity | Prefab | Pilot | Turret | Capacity | Outcome FULL/FALLBACK | Strict role order | Partial boarding absent | Статус | Evidence |
|---|---|---|---|---|---:|---|---|---|:---:|---|
| US | | | | | | | | | NOT RUN | |
| USSR | | | | | | | | | NOT RUN | |

Итог A: **NOT RUN**

## Срез R — replacement, retarget и recovery

Каждый fault лучше воспроизводить отдельным профилем. Ускорение timeout допустимо только если точные значения записаны в CONFIG и причинная последовательность не меняется.

### Replacement R

Критерии для каждого slot:

1. GROUP_EMPTY относится к текущему group_generation.
2. Replacement не появляется раньше configured delay.
3. Safe spawn подтверждён непосредственно перед созданием.
4. Новая группа имеет новый generation и ровно пять faction-correct бойцов.
5. Есть ровно один TICKET_DEBIT с delta, равным configured group replacement cost.
6. Не возникает пять отдельных debit.
7. Managed projection и фактическое число агентов не превышают aicfMaxManagedAgents.
8. Старый vehicle runtime не привязывается к новой группе.

| Faction | Identity | Old group/gen | GROUP_EMPTY | Replacement group/gen | Agents | Faction-correct | Debit count/delta | Peak projection/actual | Stale vehicle absent | Статус | Evidence |
|---|---|---|---|---|---:|---|---|---|---|:---:|---|
| US | A0 | | | | | | | | | NOT RUN | |
| US | A1 | | | | | | | | | NOT RUN | |
| US | A2 | | | | | | | | | NOT RUN | |
| US | D0 | | | | | | | | | NOT RUN | |
| USSR | A0 | | | | | | | | | NOT RUN | |
| USSR | A1 | | | | | | | | | NOT RUN | |
| USSR | A2 | | | | | | | | | NOT RUN | |
| USSR | D0 | | | | | | | | | NOT RUN | |

### Fault/recovery R

`Protected occupant remains` выполняется отдельным expected-negative профилем. Единственный разрешённый отрицательный исход этого профиля — ровно один `[AICF][STAGE3][ERROR][FALLBACK_DISEMBARK_FAILED]`, коррелирующий `ACCEPTANCE_FAILURE_LATCHED` и, при естественном входе через обычный dismount timeout, предшествующая пара `DISEMBARK_TIMEOUT → ACCEPTANCE_FAILURE_LATCHED`. Затем runtime обязан перейти в конечное состояние, сохранив protected occupant/entity. Такой профиль получает PASS только как доказательство fail-closed safety и не используется как чистый `RESULT_CANDIDATE`. Число и порядок latch-событий фиксируются заранее выбранным setup; любой другой AICF ERROR, повтор terminal ERROR, unsafe eject/delete или незавершённый runtime означает FAIL. Для общего результата R дополнительно обязателен отдельный чистый recovery-профиль без AICF ERROR и invalidation.

| Сценарий | Как создать fault | Ожидаемый результат | Фактически | Статус | Evidence |
|---|---|---|---|:---:|---|
| Target changed in route | Реальный owner/contested change | Та же пригодная машина, SAFE_REUSE/re-route, без duplicate spawn | | NOT RUN | |
| Insufficient capacity | Кандидат с capacity < alive | Другой transport либо bounded foot fallback, без partial boarding | | NOT RUN | |
| Driver lost | Игровым уроном потерять driver | Bounded exact-role recovery либо foot fallback | | NOT RUN | |
| Gunner lost | Только A | Bounded exact-role recovery либо foot fallback | | NOT RUN | |
| No physical motion | Безопасно заблокировать машину | Bounded unstuck; success только после motion/progress | | NOT RUN | |
| No objective progress | Машина движется без route progress | Recovery требует route progress | | NOT RUN | |
| Vehicle destroyed | Сохранить живого managed member | Terminal old runtime, bounded request/fallback, no stale binding | | NOT RUN | |
| Vehicle unavailable | Сделать reuse недостижимым | Bounded request/recovery, затем foot fallback | | NOT RUN | |
| Spawn site unavailable | Временно заблокировать safe sites | 4 attempts, cap-free WAITING_FOR_SITE, bounded wake | | NOT RUN | |
| Protected occupant remains (expected-negative profile) | Удерживать INCAPACITATED/foreign occupant до hard deadline | Ровно один allowlisted `FALLBACK_DISEMBARK_FAILED` + latch; no unsafe eject/delete; bounded terminal/fail-closed retained entity | | NOT RUN | |

Итог R: **NOT RUN**

## Срез L — limits, world pool и cleanup

### L1 — штатный cap 4

~~~text
aicfTransportVehiclesPerFaction=4
aicfArmedLightVehiclesPerFaction=0
aicfMaxVehiclesPerFaction=4
~~~

Проверить максимум четыре active/reserved машины на сторону и максимум одну на slot.

### L2 — cap fault

~~~text
aicfTransportVehiclesPerFaction=4
aicfArmedLightVehiclesPerFaction=0
aicfMaxVehiclesPerFaction=1
~~~

Лишние slot остаются пешими и диагностируют VEHICLE_CAP_BLOCKED без tick-churn. После освобождения cap следующий допустимый request обслуживается без duplicate spawn.

### L3 — world pool и guarded cleanup

1. Исправная abandoned-машина пишет VEHICLE_WORLD_POOL_RELEASED, ai_cap_reserved=0 и player_available=1.
2. Active AI cap и world-pool size фиксируются раздельно.
3. Заполнить pool до soft target 4 и освободить пятую исправную машину.
4. Protected fifth entry остаётся в мире с PLAYER_SAFE_DEFERRED_RETIREMENT.
5. После снятия blocker выполняется oldest-safe retirement и pool возвращается к target.
6. Player occupant, enter/exit transition и игрок в 15 м каждый отдельно блокируют delete.
7. Delete допустим только после 5 с непрерывного clear и повторного scan.
8. VEHICLE_CLEANUP_CONFIRMED относится к тем же EntityID/RplId.
9. Coordinator stop использует deferred fail-closed cleanup; unsafe/unconfirmed entity остаётся в мире.

| Проверка | US фактически | USSR фактически | Ожидается | Статус | Evidence |
|---|---|---|---|:---:|---|
| Max active/reserved при cap=4 | | | <=4 | NOT RUN | |
| Max per slot | | | <=1 | NOT RUN | |
| Cap fault 4 requested / cap 1 | | | <=1, остальные bounded blocked/on foot | NOT RUN | |
| Duplicate spawn | | | 0 | NOT RUN | |
| World-pool release | | | cap освобождён немедленно | NOT RUN | |
| Soft overflow | | | protected overflow сохранён | NOT RUN | |
| Oldest-safe retirement | | | после снятия blocker | NOT RUN | |
| Occupant gate | | | delete blocked | NOT RUN | |
| Transition gate | | | delete blocked | NOT RUN | |
| Proximity 15 m gate | | | delete blocked | NOT RUN | |
| Stable clear 5 s | | | обязателен | NOT RUN | |
| EntityID/RplId confirmation | | | identity совпадает | NOT RUN | |
| Stop cleanup | | | confirmed либо fail-closed retained | NOT RUN | |

Итог L: **NOT RUN**

## Срез M30 — 30-минутная headless-матрица

Использовать новый профиль, normal values, active roles on, transport=4, armed=0, cap=4, max managed agents=64, достаточный запас билетов и без test-hook.

За 30 минут подтвердить:

- обе стороны действуют без игрока;
- initial roster достигает 40 managed AI;
- происходят естественные owner changes и retarget;
- три ATTACK и D0 имеют содержательные task;
- нет необъяснимого idle на MOB;
- QRF/hysteresis не создаёт churn;
- replacement сохраняет размер пять и один групповой ticket debit;
- active/reserved cap не превышает 4/фракцию;
- SAFE_REUSE не создаёт duplicate entity;
- все boarding/fallback конечны;
- group, vehicle, waypoint и world-pool counters не растут монотонно;
- heartbeat продолжается;
- server FPS и managed AI остаются в записанном бюджете;
- нет AICF error, acceptance-failure latch или необъяснённого script/resource failure.

### Performance M30

| Метрика | Начало | Минимум | Максимум | Конец | Бюджет/ожидание | Статус | Evidence |
|---|---:|---:|---:|---:|---|:---:|---|
| managed_agents | | | | | штатно 40, никогда >64 | NOT RUN | |
| US ready groups | | | | | <=4, без утечки | NOT RUN | |
| USSR ready groups | | | | | <=4, без утечки | NOT RUN | |
| US active/reserved vehicles | | | | | <=4 | NOT RUN | |
| USSR active/reserved vehicles | | | | | <=4 | NOT RUN | |
| US world pool | | | | | soft target 4, overflow объяснён | NOT RUN | |
| USSR world pool | | | | | soft target 4, overflow объяснён | NOT RUN | |
| Vehicle entities | | | | | без бесконечного роста | NOT RUN | |
| AICF waypoints | | | | | без бесконечного роста | NOT RUN | |
| WorkingSet64 | | | | | без необъяснимого устойчивого роста | NOT RUN | |
| PrivateMemorySize64 | | | | | без необъяснимого устойчивого роста | NOT RUN | |
| Server FPS | | | | | записанный локальный budget: ______ | NOT RUN | |
| Warning/error count | | | | | без бесконечного churn/AICF error | NOT RUN | |

Итог M30: **NOT RUN**

## Срез S120 — двухчасовой soak

Использовать отдельный чистый profile, ту же normal-конфигурацию, aicfRequirePlayerForResult=0 и достаточный запас билетов. Test-hook выключен.

Сохранить:

- полный каталог server logs;
- начальный и конечный Get-Process с WorkingSet64, PrivateMemorySize64 и CPU;
- первую и последнюю Stage 1/2/3 heartbeat;
- выборку logStats по server FPS;
- число initial/replacement spawn;
- число capture, retarget, forward-defense/QRF transitions;
- число vehicle request/spawn/reuse/recovery/fallback;
- active/reserved maximum и world-pool maximum;
- число cleanup/delete/retained;
- count сущностей и waypoint в начале, на пике и в конце;
- все acceptance failure/error с контекстом.

PASS S120 требует не просто жизни процесса два часа, а отсутствия:

- зависших slot;
- необъяснимых idle-групп;
- бесконечного role/waypoint/recovery churn;
- duplicate group/vehicle spawn;
- stale group/vehicle generation;
- unsafe cleanup;
- необслуженных групп без task;
- бесконечного роста памяти, entities, groups, vehicles, waypoint или pool entries;
- ступенчатой деградации server FPS.

### Performance S120

| Метрика | Start | 30 min | 60 min | 90 min | 120 min | Peak | Ожидание | Статус |
|---|---:|---:|---:|---:|---:|---:|---|:---:|
| managed_agents | | | | | | | <=64, штатно около 40 | NOT RUN |
| Ready groups US/USSR | | | | | | | <=4/<=4, без утечки | NOT RUN |
| Active vehicles US/USSR | | | | | | | <=4/<=4 | NOT RUN |
| World pool US/USSR | | | | | | | overflow только player-safe | NOT RUN |
| Group entities | | | | | | | без бесконечного роста | NOT RUN |
| Vehicle entities | | | | | | | без бесконечного роста | NOT RUN |
| Waypoints | | | | | | | без бесконечного роста | NOT RUN |
| WorkingSet64 | | | | | | | без устойчивой необъяснимой утечки | NOT RUN |
| PrivateMemorySize64 | | | | | | | без устойчивой необъяснимой утечки | NOT RUN |
| CPU | | | | | | | без runaway | NOT RUN |
| Server FPS | | | | | | | без ступенчатой деградации | NOT RUN |
| Idle/QRF churn events | | | | | | | 0 необъяснимых | NOT RUN |
| AICF errors/failures | | | | | | | 0 | NOT RUN |

Итог S120: **NOT RUN**

## Runtime-дефекты текущей приёмки

### S35-T-1 — техника USSR появилась в воде

- Прогон: `Stage35-T-20260810-210932`, server run `stage1-server-11212`, срез T.
- Server evidence: `C:\Users\retar\AppData\Local\AICF\Stage35-T-20260810-210932\logs\logs_2026-08-10_21-09-32\console.log`. Client log/video locator не записан и остаётся evidence gap.
- Визуальное наблюдение клиента: техника USSR при начальном развёртывании появилась в воде.
- В 21:09:47 Stage 3 зарегистрировал четыре успешных спавна возле MOB-базы 52 и для каждого предварительно сообщил `VEHICLE_SPAWN_SITE_SELECTED reason=SAFE_FRIENDLY_BASE contested=0`:
  - A0, Ural4320 transport, entity `0x4000000000001480`;
  - A1, Ural4320 transport, entity `0x40000000000014C2`;
  - A2, UAZ452 transport, entity `0x4000000000001504`;
  - D0, UAZ452 transport, entity `0x400000000000154B`.
- Текущий лог не содержит координат выбранного spawn-site, water-depth либо результата отдельной проверки поверхности, поэтому по server evidence нельзя однозначно определить, какая из четырёх сущностей находилась в воде. Это отдельный пробел диагностики.
- Фактическое поведение нарушает safe-spawn invariant: водная позиция не должна приниматься как безопасная для колёсной техники, даже если база дружественная и не contested.
- Реализационный контракт повтора: несколько позиций проверяются по типу поверхности, глубине воды, пригодности для колёсной техники и устойчивой опоре до `SpawnEntityPrefabEx`. Неподходящая позиция отклоняется без live entity, vehicle binding и продвижения vehicle generation; уже созданные request runtime/cap reservation этим не опровергаются. Возможность дальнейшего движения проверяется отдельным route evidence.
- Требуемая диагностика: `VEHICLE_SPAWN_CANDIDATE_REJECTED reason=WATER_OR_UNDRIVABLE_SURFACE` с `candidate_index`, `candidates`, authoritative `origin`, `surface`, `water`, `footprint_delta_m`, `probes`, `preflight` и `request_generation`; принятые `VEHICLE_SPAWN_PREFLIGHT_READY`/`VEHICLE_SPAWN_SITE_SELECTED` повторяют validated surface telemetry.
- Исторический статус: `CONFIRMED FAIL` по визуальному наблюдению. Реализационный surface-контракт и диагностика добавлены; repeat: **NOT RUN**. Для точного определения проблемного slot/entity требуется новый runtime evidence.

### S35-T-2 — единственный выживший US захватил базу и получил полноразмерный транспорт

- Прогон: `Stage35-T-20260810-210932`, server run `stage1-server-11212`, срез T.
- US A1 generation 1 потеряла четырёх из пяти бойцов после возгорания первого транспорта `0x400000000000136E`. С 21:13:44 до захвата Stage 3.5 устойчиво сообщал `alive=1`, при этом группа не была заменена и продолжала ATTACK-задачу.
- Единственный выживший самостоятельно дошёл до цели 65 и в 21:19:34 обеспечил `BASE_OWNER_CHANGED base=65 old_owner=FIA new_owner=US`.
- После смены цели на 38 тот же slot A1 немедленно запросил машину. Spawn preflight выбрал только что захваченную базу 65: `alive=1`, расстояние до spawn-site 24.3 м, база считалась safe и uncontested.
- В 21:19:36 для одного бойца создан полноразмерный M923A1 transport entity `0x4000000000001BB2` с вместимостью 15. Capacity preflight принял его как `required=1 available=15 accepted=1`.
- Боец занял водительское место; пассажирская фаза штатно завершилась как `requested=0 policy=NO_CARGO_REQUIRED`. В 21:19:53 получен `BOARDING_COMPLETE mounted=1 driver=1`, после чего одиночный водитель начал движение к цели 38.
- Принятая policy: minimum combat-ready применяется только к **новому** vehicle request. Группа ниже порога не создаёт новую машину и получает bounded recovery/replacement/infantry outcome. Уже назначенная исправная машина остаётся пригодной для оставшихся в живых и не отбирается/не удаляется только из-за последующего уменьшения alive.
- Наблюдаемое нарушение принятой policy: A1 с `alive=1` получила новую 15-местную машину. Для явно разрешённой singleton-задачи понадобились бы отдельная policy и подходящий light transport; общий `required=alive` сам по себе такого разрешения не даёт.
- Обязательная диагностика повтора: `VEHICLE_REQUEST_INELIGIBLE alive=1 required_minimum=3 policy=NEW_REQUEST_ONLY assigned_vehicle_present=<0|1> assigned_vehicle_policy=PRESERVE_EXISTING`. Pre-runtime rejection не создаёт `VEHICLE_REQUESTED` и продолжает suppression/current infantry assignment; `GROUP_NOT_COMBAT_READY` ожидается только для уже pending REQUESTED/WAITING runtime без assigned vehicle. Stage 3.5 `SLOT_ACTIVITY` показывает выбранный recovery/replacement/infantry outcome.
- Исторический статус: `CONFIRMED FAIL / ACCEPTED POLICY`. Реализационный контракт добавлен; repeat: **NOT RUN**.

### S35-T-3 — водная/непроходимая позиция вызывает повторяющиеся VM exceptions и распад USSR A1

- В 21:23:17, 21:25:18, 21:27:19 и 21:29:40 stock group movement завершался `SCRIPT (E): Virtual Machine Exception`, `Reason: Failed move`.
- Исходные координаты имели отрицательную высоту от `-1.51` до `-2.28` м: примерно `<1111–1122, -1.5…-2.3, 3273–3281>`. Целевые waypoint находились на наземной высоте около 38 м. Это подтверждает failed-move из низкой/непроходимой области и коррелирует с визуальным S35-T-1, но без surface/water telemetry само по себе не доказывает тип поверхности.
- USSR A1 распалась пространственно: `VEHICLE_SPAWN_SITE_REJECTED reason=NO_BOARDING_SITE_WITHIN_RANGE` показывал farthest примерно 281 м в 21:21:40, 658 м в 21:24:57, 985 м в 21:26:37 и 1506 м в 21:29:33. Группа продолжительно переходила между spawn attempts и `WAITING_FOR_SITE`; это не следует называть бесконечным, поскольку после изменения обстановки она получила новую машину в 21:56:25 и завершила boarding 4/4 в 21:57:28.
- Контракт повтора: water/undrivable validation до spawn и bounded cohesion recovery для stranded member. Один недостижимый member не должен создавать бесконечный request churn для всего slot; terminal outcome обязан быть диагностирован и ограничен.
- Исторический статус: `CONFIRMED FAIL`. Реализационный контракт добавлен; repeat: **NOT RUN**.

### S35-T-4 — массовые ошибки посадки/высадки групп из пяти

- USSR A0 generation 2 завершила passenger-phase только с тремя из пяти: два бойца остановились примерно в 71–72 м. В 21:22:41 получены `BOARDING_TIMEOUT cause=PASSENGERS_NOT_MOUNTED`, `ACCEPTANCE_FAILURE_LATCHED` и infantry fallback; машина позднее брошена.
- US A0 получил ещё два terminal `BOARDING_TIMEOUT_PASSENGERS_NOT_MOUNTED`: 21:33:17 (mounted=3/5) и 21:58:23 (mounted=4/5).
- Несколько US/USSR slot после логического выхода оставались внутри vehicle bounds. `DISEMBARK_CLEARANCE_RECOVERY` регулярно перемещал наружу четыре или все пять бойцов; отдельные USSR fallback потребовали `FALLBACK_FORCE_DISEMBARK` и повторных clearance attempts.
- На server-log boundary 22:09:11 зафиксированы 3 `BOARDING_TIMEOUT`, 29 `DISEMBARK_CLEARANCE_RECOVERY` и 5 `FALLBACK_FORCE_DISEMBARK`. Это не единичная визуальная погрешность: штатные пятичленные boarding/dismount циклы систематически зависят от corrective relocation, а часть циклов завершается acceptance failure.
- Контракт повтора: после обязательного crew каждому пассажиру атомарно резервируется точный Cargo compartment; per-member token/retry fenced переходом и очищается при completion/fallback. Normal dismount использует bounded movement guidance, а relocation допускается только terminal/fallback. Repeat trace обязан перечислять состояние, assigned/actual compartment manager/slot и reservation каждого EntityID, чтобы missing member определялся без агрегатной догадки.
- Исторический статус: `CONFIRMED FAIL`. Exact-cargo и normal-dismount contracts добавлены; repeat: **NOT RUN**.

### S35-T-5 — active slot нарушает MOB idle deadline

- `[STAGE3.5][ERROR][MOB_IDLE_DEADLINE_MISSED]` зарегистрирован для USSR A1 в 21:19:00, USSR A2 в 21:20:49 и USSR D0 в 21:23:32. Все три события переданы через `[STAGE1][ERROR][CORE_ERROR_BRIDGE]`.
- По контракту Stage 3.5 любое такое событие делает чистый прогон FAIL: active slot превысил два commander intervals без разрешённой причины.
- Контракт повтора: корректно классифицировать bounded vehicle/fallback/recovery ожидание либо своевременно выдавать meaningful task. Реальное необъяснимое бездействие должно устраняться, а не маскироваться продлением deadline.
- Исторический статус: `CONFIRMED FAIL`; repeat: **NOT RUN**.

### S35-T-6 — bounded vehicle fallback, запрещённый churn и runtime exceptions

- USSR A1/A2 неоднократно получили `NO_PHYSICAL_MOVEMENT`; две попытки vehicle-unstuck не подтвердили displacement/progress и завершились `VEHICLE_STUCK_PERSISTENT`.
- USSR D0 завершила цикл через `DRIVER_RECOVERY_EXHAUSTED` в 21:23:02. Позднее replacement D0 успешно получила новую машину и посадила 5/5 в 21:55:36, что подтверждает локальность сбоя, а не общий отказ spawner.
- US A2 исчерпала driver recovery в 21:33:44; US D0 — в 21:54:28. Одиночный US A1 перевернул выданный M923A1 и завершил поездку через `VEHICLE_OVERTURNED` в 21:27:11.
- Сам по себе terminal `VEHICLE_RECOVERY_FAILED` → bounded `INFANTRY_FALLBACK`/retained world-pool outcome допустим и в этом прогоне наблюдался. FAIL образуют повторный request/recovery churn без устранения причины, ложный success без последующего movement/progress, unsafe cleanup и любой SCRIPT/VM exception.
- Помимо четырёх Failed move VM exceptions из S35-T-3, в 22:03:07 возник `Virtual Machine Exception: NULL pointer to instance`, `SCR_AIGetInVehicle.OnActionFailed`, variable `m_OwnerEntity`; в 21:33:52 зарегистрирован отдельный `SCRIPT (E)` от `RandomGenerator::GenerateRandomPointInRadius` с min=max=0. Оба события входят в runtime-cleanliness FAIL и требуют точной causal correlation на повторе.
- Исторический статус: `BOUNDED FALLBACK CONFIRMED` как допустимый fail-closed исход; `CHURN / SCRIPT(E) / VM EXCEPTION — FAIL`. Реализационные guards добавлены; чистый repeat: **NOT RUN**.

## Итог Transport-среза T — `Stage35-T-20260810-210932`

- Initial roster: `PASS` — восемь групп по пять faction-correct бойцов, роли `3 ATTACK / 1 DEFEND`, `managed_agents=40` до потерь.
- Capacity preflight: техническая seat-проверка `PASS`, но new-request combat-ready policy `FAIL` — четырёхместный US light transport корректно заменялся M923A1 для пятёрки, однако singleton A1 получила новую 15-местную машину.
- Initial vehicle allocation: `FAIL` — водная/непроходимая позиция USSR была принята как safe spawn-site.
- Boarding/dismount: `FAIL` — один terminal passenger timeout у USSR A0, два у US A0 и массовый clearance recovery для групп из пяти.
- Vehicle recovery: bounded terminal fallback наблюдался, но категория `FAIL` из-за persistent stuck, повторного churn, driver recovery exhaustion и нескольких abandoned исправных машин.
- Active-force activity: `FAIL` — `MOB_IDLE_DEADLINE_MISSED` у USSR A1, A2 и D0.
- Runtime cleanliness: `FAIL` — пять `Virtual Machine Exception`, отдельный `RandomGenerator` SCRIPT(E), AICF Stage 3/3.5 errors и `ACCEPTANCE_FAILURE_LATCHED`.
- Итог T: **FAIL**. На момент первичного анализа сервер и клиент были намеренно оставлены запущенными для дальнейшего наблюдения; отсутствие финального stop-result в этом историческом срезе не меняет уже подтверждённый FAIL.

## Preliminary repeat smoke — BLOCKED, не repeat T

Этот запуск проверял только возможность начать новый server/client runtime после исправлений. Он не привязан к итоговому commit/SHA и предшествует двум последним точечным guards passenger transition/eligibility. До AICF-поведения запуск не дошёл и не заполняет ни одну строку обязательного repeat-gate.

| Поле | Фактически |
|---|---|
| Статус | **BLOCKED** — backend handshake/authentication не позволил подключить клиента |
| Server profile | `C:\Users\retar\AppData\Local\AICF\Stage35-T-RepeatSmoke-20260810-224931` |
| Server console.log | `C:\Users\retar\AppData\Local\AICF\Stage35-T-RepeatSmoke-20260810-224931\logs\logs_2026-08-10_22-49-31\console.log` |
| Server CLI | `-gproj ...\AIConflictArland\addon.gproj -config ...\.codex-runtime\parallel-transport-server.json -addonsDir <repo>,<server addons> -profile ...\Stage35-T-RepeatSmoke-20260810-224931 -aicfActiveForcesRolesEnabled 1 -aicfMaxManagedAgents 64 -aicfRequirePlayerForResult 0 -aicfVehiclesEnabled 1 -aicfTransportVehiclesPerFaction 4 -aicfArmedLightVehiclesPerFaction 0 -aicfMaxVehiclesPerFaction 4 -aicfVehicleMinimumRequestAgents 3 -backendFreshSession -maxFPS 60 -logStats 10000 -noThrow` |
| Наблюдаемое окно | `2026-08-10 22:49:31` — `22:58:16` |
| Runtime counters | `Player: 0`, `AIChar: 0`; AICF behavior не стартовал, `[AICF]`/`CONFIG`/roster/vehicle evidence отсутствует |
| Client attempt 1 | `C:\Users\retar\AppData\Local\AICF\Stage35-Client-T-RepeatSmoke-20260810-225140\logs\logs_2026-08-10_22-51-40\console.log`; server authentication identity `0`, timeout/refused `22:52:15` |
| Client attempt 2 | `C:\Users\retar\AppData\Local\AICF\Stage35-Client-T-AuthProbe-20260810-225509\logs\logs_2026-08-10_22-55-09\console.log`; server authentication identity `0`, timeout/refused `22:55:44` |
| Client attempt 3 | `C:\Users\retar\AppData\Local\AICF\Stage35-Client-T-RetailSmoke-20260810-225612\logs\logs_2026-08-10_22-56-13\console.log`; server authentication identity `0`, timeout/refused `22:56:49` |
| Завершение | Diag clients завершились после handshake timeout; оставшиеся server/retail-client процессы остановлены; этот запуск не считается repeat T |

Классификация **BLOCKED** относится к внешнему handshake/auth препятствию, а не заявляет ни implementation/runtime FAIL, ни PASS. Исторический `Stage35-T-20260810-210932` остаётся **FAIL**, полный Stage 3.5 остаётся **NOT RUN**, обязательный repeat T ниже остаётся целиком **NOT RUN**.

## Обязательный repeat-gate Transport T

Повтор выполняется после исправлений новым server/client процессом и двумя новыми profile на одном записанном commit. Старый `Stage35-T-20260810-210932` остаётся FAIL evidence и не перезаписывается. До анализа повтор останавливается, фиксируется точный cutoff и архивируются полные server/client log directories.

| Gate | Обязательные поля/события | Фактически | Статус |
|---|---|---|:---:|
| Reproducibility | commit SHA, Game/Server/Tools versions, обе полные CLI, server/client profile и console.log, start/stop/cutoff | | NOT RUN |
| Initial roster | 8 × `GROUP_ROSTER_READY initial_agents=5 expected_agents=5 faction_correct=1`; общий `ROSTER_READY managed_agents=40` | | NOT RUN |
| Surface preflight | Для каждого candidate: `candidate_index/candidates`, `origin`, `surface`, `water`, `footprint_delta_m`, `probes`, `preflight`, request generation и accepted/rejected reason | | NOT RUN |
| Surface rejection | Water/uneven position даёт `VEHICLE_SPAWN_CANDIDATE_REJECTED reason=WATER_OR_UNDRIVABLE_SURFACE` до `SpawnEntityPrefabEx`, live entity, vehicle binding и продвижения vehicle generation; accepted preflight/site повторяет validated telemetry. Request runtime/cap reservation уже могут существовать | | NOT RUN |
| New-request combat readiness | Ниже threshold=3: `VEHICLE_REQUEST_INELIGIBLE alive=<n> required_minimum=3 policy=NEW_REQUEST_ONLY assigned_vehicle_present=<0|1> assigned_vehicle_policy=PRESERVE_EXISTING`; pre-runtime rejection не порождает `VEHICLE_REQUESTED`, а `GROUP_NOT_COMBAT_READY` допустим только для уже pending REQUESTED/WAITING runtime без assigned vehicle; новой entity нет | | NOT RUN |
| Assigned-vehicle retention | После потерь уже назначенная пригодная entity остаётся usable/reusable; нет abandon/delete только по причине alive<threshold | | NOT RUN |
| Capacity | Для A0/A1 truck и A2/D0 light→truck trace: catalog/resource, required=alive, accessible seats, candidate deletion и выбранный prefab | | NOT RUN |
| Exact boarding | После crew: `PASSENGERS_ASSIGNED policy=EXACT_PER_MEMBER_CARGO_AFTER_CREW requested=<n> issued=<n>`; `BOARDING_ACTION_OWNERSHIP` содержит полный список живых EntityID, token state/current/retry, assigned compartment manager/slot, reserved и actual compartment manager/slot; missing member назван явно | | NOT RUN |
| Boarding action lifecycle | `PASSENGER_BOARDING_REISSUED` только bounded с `transition_fenced=1`; нет `PASSENGER_BOARDING_ACTION_FAILED`, wrong/unsupported compartment и stale reservation/action после completion/fallback | | NOT RUN |
| Boarding completion | Ровно два settled poll, для полного состава mounted=alive=5; нет timeout, remote GetIn или snap | | NOT RUN |
| Route/dismount | Подтверждённые route progress/physical motion; normal physical blocker получает bounded per-member `DISEMBARK_CLEARANCE_GUIDANCE` без relocation/teleport; terminal/fallback `DISEMBARK_CLEARANCE_RECOVERY` bounded, не churn-ит и не считается доказательством normal-path PASS; затем полная безопасная высадка и infantry order | | NOT RUN |
| Recovery/fallback | Attempt/deadline bounded; success только после последующего motion/progress; terminal fallback не создаёт повторный request/state churn и не удаляет protected/usable entity небезопасно | | NOT RUN |
| Cap/pool identity | По EntityID/RplId доказаны <=4 unique active/reserved на фракцию, <=1 на slot; описано пересечение heartbeat counters; world-pool отделён | | NOT RUN |
| Runtime cleanliness | 0 AICF ERROR, 0 `SCRIPT (E/F)`, 0 VM exception, 0 late `ACCEPTANCE_FAILURE_LATCHED`; предупреждения не churn-ят | | NOT RUN |
| Client evidence | Полный client log и видео initial five-member boarding, движения и dismount без teleport/snap | | NOT RUN |

Repeat T получает PASS только при PASS всех строк gate. Технически успешный spawn/boarding отдельного slot, bounded fallback после уже возникшего дефекта или отсутствие финального result не заменяют чистый повтор.

## Итоговая матрица Stage 3.5

| № | Проверка | Ожидается | Фактически | PASS / FAIL / BLOCKED / NOT RUN | Доказательство |
|---:|---|---|---|:---:|---|
| 1 | Один commit | Все B/P/T/A/R/L/M30/S120 на одном SHA | | NOT RUN | |
| 2 | Validate/Compile | Без AICF ошибок | Локальный current-snapshot compile-smoke PASS; это development evidence без зафиксированного commit | PASS | `.cache/stage35-newrun-telemetry-final7-validate-20260811/console.log`: Game 5675/11049, `Game successfully created` |
| 3 | Static audit | Stage 3 и Stage 3.5 contracts соблюдены | Оба audit scripts завершились PASS; это development evidence, не runtime acceptance | PASS | `tools/Test-Stage3Static.ps1`, `tools/Test-Stage35Static.ps1` |
| 4 | Initial roster | 8 групп × 5 | 8 групп × 5 в историческом T | PASS | Исторический T only, не repeat/full acceptance: `GROUP_ROSTER_READY`, общий `ROSTER_READY` |
| 5 | Faction correctness | 40/40 бойцов принадлежат своей фракции | Все initial roster исторического T `faction_correct=1` | PASS | Исторический T only, не repeat/full acceptance: run `stage1-server-11212`, 21:09:45 |
| 6 | Heartbeat | managed_agents=40 без игроков | Начальный managed roster 40 подтверждён, но T имел подключённого клиента (`Player: 1`) и не доказывает no-player условие | NOT RUN | Требуется отдельный headless/no-client evidence |
| 7 | Managed budget | lower bound >=48, test value 64, runtime <=64 | | NOT RUN | |
| 8 | Replacement size | Каждый из 8 slot снова получает 5 | | NOT RUN | |
| 9 | Replacement tickets | Один group debit на replacement | | NOT RUN | |
| 10 | Member-count policies | Formation/cohesion/stuck/markers используют actual count | | NOT RUN | |
| 11 | Baseline roles | B: 2 ATTACK / 1 DEFEND / 1 RESERVE | | NOT RUN | |
| 12 | Active roles US | P+: 3 ATTACK / 1 DEFEND-QRF / 0 RESERVE | 3 ATTACK / 1 DEFEND / 0 RESERVE в историческом T | PASS | Исторический T only, не repeat/full acceptance: `CONFIG`, `ROSTER_READY` |
| 13 | Active roles USSR | P+: 3 ATTACK / 1 DEFEND-QRF / 0 RESERVE | 3 ATTACK / 1 DEFEND / 0 RESERVE в историческом T | PASS | Исторический T only, не repeat/full acceptance: `CONFIG`, `ROSTER_READY` |
| 14 | Attack distribution | Primary/adjacent/support deterministic или объяснённый fallback | | NOT RUN | |
| 15 | D0 forward defense | Не MOB по умолчанию | | NOT RUN | |
| 16 | QRF triggers | Contested, adjacent loss и HQ threat проверены | | NOT RUN | |
| 17 | QRF return/hysteresis | Возврат после стабилизации без tick-churn | | NOT RUN | |
| 18 | MOB idle | Нет >2 commander intervals без разрешённой причины | Ошибка у USSR A1, A2 и D0 | FAIL | `MOB_IDLE_DEADLINE_MISSED` 21:19:00 / 21:20:49 / 21:23:32 |
| 19 | Retarget deadline | <=2 commander intervals | | NOT RUN | |
| 20 | Vehicle eligibility | A0/A1/A2/D0 допускаются при route/cap и new-request combat-ready policy | Все восемь initial slot получили vehicle; singleton policy позднее нарушена | FAIL | Initial `VEHICLE_SPAWNED`; S35-T-2 |
| 21 | Per-slot vehicle bound | <=1 active/reserved на slot | | NOT RUN | |
| 22 | Faction active cap | <=4 US и <=4 USSR | | NOT RUN | |
| 23 | A0/A1 preference | Faction-correct truck | | NOT RUN | |
| 24 | A2/D0 preference | Adequate light либо truck fallback | | NOT RUN | |
| 25 | Capacity/new-request policy | Catalog/resource/accessible seats >= alive; новая выдача только combat-ready группе; assigned vehicle сохраняется после потерь | Seat capacity соблюдена, но singleton получил новую 15-местную машину | FAIL | S35-T-2, `required=1 available=15`; отсутствует `VEHICLE_REQUEST_INELIGIBLE` |
| 26 | ALL_OR_FALLBACK | Нет частичной отправки группы | | NOT RUN | |
| 27 | Boarding complete | Full roster settled 5/5; runtime mounted=alive; exact per-member Cargo token/reservation/actual-compartment trace | Есть успешные 5/5, но USSR A0 завершилась timeout один раз, US A0 — дважды | FAIL | `BOARDING_TIMEOUT_PASSENGERS_NOT_MOUNTED` 21:22:41 / 21:33:17 / 21:58:23 |
| 28 | No remote boarding | Нет teleport-in/remote GetIn/snap | | NOT RUN | |
| 29 | Strict crew roles | DRIVER→PASSENGERS или DRIVER→GUNNER→PASSENGERS | | NOT RUN | |
| 30 | Route/dismount | Реальное движение; normal blocker использует bounded movement guidance без teleport; terminal/fallback relocation только bounded fail-closed; полная безопасная высадка и infantry continuation | Массовый clearance recovery как системный штатный путь; failed movement из низкой/непроходимой области, surface type не телеметрирован | FAIL | S35-T-1/S35-T-3/S35-T-4 |
| 31 | SAFE_REUSE | Target change использует ту же пригодную entity | | NOT RUN | |
| 32 | Vehicle loss | Bounded recovery/request либо foot fallback без churn/false success/VM exception | Terminal fallback был bounded, но повторный churn и runtime exceptions нарушили полный контракт | FAIL | S35-T-6 |
| 33 | No duplicate spawn | Нет повторной entity для той же generation/assignment | | NOT RUN | |
| 34 | Armed-light slice | Полный crew при capacity либо preflight fallback | | NOT RUN | |
| 35 | Active cap vs pool | Счётчики разделены | | NOT RUN | |
| 36 | World-pool release | Usable abandoned освобождает AI cap и доступен игроку | | NOT RUN | |
| 37 | Soft overflow | Protected fifth vehicle не удаляется небезопасно | | NOT RUN | |
| 38 | Cleanup gates | Occupant/transition/15 m/5 s/identity соблюдены | | NOT RUN | |
| 39 | Stop cleanup | Confirmed delete либо fail-closed retained | | NOT RUN | |
| 40 | Stage 2/3 regression | Lifecycle/order/boarding/recovery contracts чисты | Acceptance latch, boarding timeout, stuck/driver churn и SCRIPT(E) | FAIL | Stage 3 warnings/errors текущего T; S35-T-3/T-4/T-6 |
| 41 | M30 stability | Нет роста groups/vehicles/waypoints/pool и FPS деградации | | NOT RUN | |
| 42 | S120 stability | Нет stuck slot, idle, unsafe cleanup или memory/entity growth | | NOT RUN | |
| 43 | Result candidate | READY не инвалидирован до остановки полного окна | | NOT RUN | |
| 44 | Errors | В чистых профилях нет AICF ERROR, SCRIPT E/F или warning churn; в expected-negative — только exact allowlist R | 3 Stage 3.5 MOB errors, Stage 3 fallback error, 5 VM exceptions, RandomGenerator SCRIPT(E), acceptance failures | FAIL | S35-T-3/S35-T-4/S35-T-5/S35-T-6; server log through 22:09:11 |
| 45 | Evidence | Полные server/client logs, metrics и visual proofs приложены | Server log найден; commit, client profile/log и video locator не записаны; stop/cutoff первоначально отсутствовал | FAIL | Фактические метаданные T и repeat-gate |

## PASS / FAIL / BLOCKED / NOT RUN

PASS:

- все обязательные срезы B/P/T/A/R/L/M30/S120 выполнены на одном commit;
- все применимые строки итоговой матрицы имеют PASS и доказательство;
- initial и replacement roster везде равен пяти faction-correct бойцам;
- роли, forward-defense/QRF и retarget соблюдают deadline/hysteresis;
- active vehicle cap, capacity policy, SAFE_REUSE, recovery, world pool и cleanup соблюдены;
- exact per-member Cargo boarding ownership/reservations очищаются корректно, normal dismount использует movement guidance без relocation/teleport, а terminal/fallback recovery остаётся bounded и fail-closed;
- Stage 2/3 safety regression чист;
- M30 и S120 завершены без бесконечного роста или деградации;
- полный остановленный журнал чистых профилей не содержит позднего ACCEPTANCE_FAILURE_LATCHED, invalidated candidate, AICF ERROR или необъяснённого SCRIPT E/F; отдельный protected-occupant expected-negative профиль содержит только явно allowlisted exact failure из среза R.

FAIL:

- реализация загрузилась, но нарушен любой игровой или safety-инвариант;
- группа имеет не пять бойцов, неверную фракцию или replacement списывает билет по бойцам;
- роль/target отсутствует, D0 необъяснимо простаивает на MOB, QRF churn-ит или retarget опаздывает;
- частичная посадка, движение без обязательного crew, remote GetIn/teleport-in, wrong/unsupported passenger compartment, stale reservation/action, normal-path relocation/teleport или неполный dismount;
- spawn/cap/reuse/generation нарушены;
- recovery/fallback бесконечен, churn-ит, создаёт VM exception/unsafe relocation либо ложный success не подтверждён движением/progress; сам по себе bounded terminal/fallback recovery не является FAIL;
- protected occupant/player transition/proximity gate обойдены;
- entity, waypoint, group, vehicle, pool entry или память бесконечно растут;
- автоматический READY-кандидат чистого профиля позднее инвалидирован либо в expected-negative профиле возникло что-либо кроме единственного allowlisted protected-occupant failure.

BLOCKED:

- несовместимая версия Game/Tools/Server;
- addon/resource не загружен;
- Validate/Compile или обязательный static audit не запускается;
- AICF compile/script/dependency error;
- занят порт или среда не позволяет начать нужный сценарий;
- отсутствует воспроизводимый переключатель baseline/active roles;
- runtime не дошёл до проверяемого поведения по внешней причине.

BLOCKED не является PASS.

NOT RUN:

- срез ещё не запускался либо evidence отсутствует;
- автоматическая компиляция/static audit без runtime не меняет NOT RUN на PASS;
- отдельный успешный визуальный эпизод без полного причинного журнала не меняет NOT RUN на PASS.

Текущий итог полной Stage 3.5: **NOT RUN** — остальные обязательные B/P/A/R/L/M30/S120 не выполнены. Текущий Transport-срез T: **FAIL**.

## Что приложить к итоговому отчёту

- branch/tag и полный commit SHA;
- версии Game/Server/Tools;
- результаты Validate/Compile и всех static audit;
- полные command line каждого среза;
- все фактические CONFIG;
- полные каталоги server logs;
- полные client logs controlled-срезов;
- нефильтрованные Stage 1/2/3/3.5 строки;
- заполненные roster/replacement/planning/transport/performance таблицы;
- client video посадки пяти бойцов без snap;
- видео/скриншоты D0 forward-defense и QRF transition;
- EntityID/RplId correlation для cleanup;
- начальные/конечные Get-Process и logStats для M30/S120;
- первую error/failure с минимум 30 строками контекста;
- итоговую матрицу с одним evidence locator на каждую строку.

## Дополнительный Transport-прогон 2026-08-11 — автономная деградация

Прогон: `Stage35-T-20260811-190311`, server run `stage1-server-8620`.

Evidence:

- server profile: `C:\Users\retar\AppData\Local\AICF\Stage35-T-20260811-190311`;
- server log: `C:\Users\retar\AppData\Local\AICF\Stage35-T-20260811-190311\logs\logs_2026-08-11_19-03-11\console.log`;
- initial client profile: `C:\Users\retar\AppData\Local\AICF\Stage35-Client-T-20260811-190311`;
- initial client log: `C:\Users\retar\AppData\Local\AICF\Stage35-Client-T-20260811-190311\logs\logs_2026-08-11_19-03-24\console.log`;
- reconnect client profile: `C:\Users\retar\AppData\Local\AICF\Stage35-Client-T-Reconnect-20260811-202916`;
- reconnect client log: `C:\Users\retar\AppData\Local\AICF\Stage35-Client-T-Reconnect-20260811-202916\logs\logs_2026-08-11_20-29-16\console.log`;
- сервер работал без игрока примерно с 19:20:43 до повторного подключения около 20:29; evidence cutoff — 20:33:06.736;
- процесс после cutoff остановлен, но в журнале нет `RESULT_CANDIDATE`, `RESULT`, `MATCH_STOP` или `CLEAN_SHUTDOWN`, поэтому прогон не считается корректно финализированным.

Начальный roster был корректным: 8 групп по 5 бойцов, всего `managed_agents=40`. В 20:01 осталось 24 управляемых бойца. FPS оставался стабильным около 60, поэтому перечисленные ниже сбои нельзя объяснить перегрузкой сервера.

### S35-T-7 — после ABANDONED живая группа теряет meaningful task

- US A1 имела 64 последовательных `SLOT_ACTIVITY` с `vehicle_state=ABANDONED`, обоими waypoint=0 и `meaningful_task=0` с 19:26:20 до 20:29:20. Приказ появился только в 20:29:57 после очистки logical occupant.
- USSR A2 имела 52 таких последовательных среза с 19:26:20 до 20:17:20; в 20:17:56 восстановление было пропущено как `GROUP_NO_LONGER_EXISTS`.
- Обе группы сохраняли стратегическую цель, но фактически оставались без исполняемого приказа 52–64 минуты.
- Они находились примерно в 1.4–1.7 км от MOB, поэтому отсутствие `MOB_IDLE_DEADLINE_MISSED` соответствует узкому MOB-контракту. Дефект — отсутствие независимого global meaningful-task deadline и объясняющего suppression event.
- Требуется гарантированный выход `ABANDONED -> infantry order` и диагностические события `MEANINGFUL_TASK_LOST`, `ORDER_RESTORE_REQUESTED/RESULT`, `WAYPOINT_REMOVED`, `ABANDONED_EXIT_AUDIT`, `IDLE_DEADLINE_SUPPRESSED`.
- Статус: **CONFIRMED FAIL**.

### S35-T-8 — повторный DISEMBARK_TIMEOUT и невозможность принудительной высадки

- Всего зафиксировано 7 `DISEMBARK_TIMEOUT` по 5 слотам: USSR A2 в 19:09:11, USSR A1 в 19:10:44, USSR A0 в 19:11:39, US A1 в 19:13:51 и 19:24:13, US A2 в 19:14:07 и 19:25:28. В большинстве эпизодов бойцы уже покинули compartments, но оставались внутри vehicle bounds; у US A1 в 19:24:13 ещё сохранялся `logical=1` при `inside_bounds=4`.
- В 19:25:49 USSR A2 и в 19:26:13 US A1 дошли до terminal `FALLBACK_DISEMBARK_FAILED reason=FORCED_EXIT_FAILED protected_occupants_remain=1` с `CORE_ERROR_BRIDGE`.
- Таким образом, bounded fallback не гарантирует освобождение группы и восстановление пехотного приказа.
- Статус: **CONFIRMED FAIL**.

### S35-T-9 — аварии посадки и восстановления водителя завершаются потерей транспорта

- US A0: `BOARDING_APPROACH_MEMBER_STALLED` в 19:22:18, затем `VEHICLE_ABANDONED`.
- US D0: `CREW_ROLE_LOST_DURING_BOARDING` в 19:35:20, затем `VEHICLE_ABANDONED`.
- US A2: `VEHICLE_RECOVERY_FAILED reason=DRIVER_RECOVERY_EXHAUSTED` в 19:37:05.716, затем `VEHICLE_ABANDONED` в 19:38:06.849.
- USSR A1 ранее бросила транспорт из-за `VEHICLE_OVERTURNED_DURING_BOARDING`; USSR A0 — из-за `VEHICLE_OVERTURNED` в 19:21:45.
- Отдельные восстановления работали: US A0 получил `VEHICLE_RECOVERY_SUCCEEDED`, а US A2 ранее успешно завершил `VEHICLE_CREW_RECOVERY_SUCCEEDED`. Это подтверждает частичную, но не устойчивую работу recovery.
- Статус: **CONFIRMED FAIL** для полного транспортного контракта.

### S35-T-10 — длительный распад групп и циклы WAITING_FOR_SITE

- USSR A1 уже в 19:19 имела одного бойца примерно в 594–629 м от spawn-site и остальных около 234–254 м. Позднее разрыв вырос: лидер находился примерно в 1964 м, остальные в 1094–1171 м.
- US A0 при `alive=3` имела участников примерно в 148, 844 и 1315 м от одной точки появления.
- Несколько US/USSR групп исчерпывали четыре spawn attempts и переходили в `WAITING_FOR_SITE reason=NO_BOARDING_SITE_WITHIN_RANGE`, продолжая минутные preflight-пробы.
- Само отсутствие безопасной дружественной точки в пределах 250 м допустимо. Дефектом является длительное отсутствие bounded cohesion outcome для пространственно распавшейся группы, из-за которого транспортная задача не может быть выполнена.
- Статус: **CONFIRMED FAIL**.

### S35-T-11 — массовые пехотные застревания при автономной работе

- На последнем heartbeat 20:32:20: `stuck_detected=35`, `stuck_recovered=30`, `stuck_field_holds=5`. В полном журнале 36 строк `GROUP_STUCK_DETECTED`: последняя появилась после heartbeat в 20:32:38.
- US A0 и USSR A1 неоднократно доходили до четырёх `GROUP_STUCK_DETECTED`; USSR A0 также получил повторные stuck-события.
- Order recovery иногда был устойчивым (`order_attempts=3`, `order_recovered=3`), но не устранил общий повторяющийся churn.
- Статус: **CONFIRMED FAIL** для S120/stability; механизм обнаружения и часть recovery работают.

### S35-T-12 — runtime/script errors

- 19:10:23: `[STAGE3.5][ERROR][MOB_IDLE_DEADLINE_MISSED]` для US A0 с `CORE_ERROR_BRIDGE`.
- 19:15:38: `SCRIPT (E): Virtual Machine Exception` с сообщением `No agent provided!` в stock AI callback; причинная связь с конкретной AICF-операцией существующим логом не доказана.
- 19:25:49 и 19:26:13: `[STAGE3][ERROR][FALLBACK_DISEMBARK_FAILED]` с `CORE_ERROR_BRIDGE`.
- 19:34:17 и 19:36:38: `RandomGenerator::GenerateRandomPointInRadius: minRadius (0.000000) must be lower than maxRadius (0.000000)`.
- 20:30:00: второй `[STAGE3.5][ERROR][MOB_IDLE_DEADLINE_MISSED]` для USSR A0: `idle_ms=30133`, `waypoint=1`, `vehicle_state=NONE`, затем `CORE_ERROR_BRIDGE`.
- Acceptance latch вырос как минимум до `count=11`; среди причин: `DISEMBARK_TIMEOUT`, `BOARDING_APPROACH_MEMBER_STALLED`, `FALLBACK_DISEMBARK_FAILED`, `CREW_ROLE_LOST_DURING_BOARDING`.
- Статус: **CONFIRMED FAIL**. Требование runtime cleanliness нарушено.

### Срез состояния перед повторным подключением клиента

| Слот | Alive | Состояние | Наблюдение |
|---|---:|---|---|
| US A0 | 1 | Infantry | Есть infantry waypoint |
| US A1 | 5 | ABANDONED | Нет waypoint и meaningful task |
| US A2 | 5 | WAITING_FOR_SITE | Пехотный приказ активен, безопасной точки рядом нет |
| US D0 | 4 | MOVING | Единственная группа, двигавшаяся на машине в срезе |
| USSR A0 | 4 | WAITING_FOR_SITE | Пехотный приказ активен |
| USSR A1 | 1 | Infantry | Есть infantry waypoint |
| USSR A2 | 1 | ABANDONED | Нет waypoint и meaningful task |
| USSR D0 | 3 | Infantry | Есть infantry waypoint |

Повторное подключение клиента в 20:29 успешно: сервер создал игрока `Shitcher`, `Players connected: 1`, packet loss 0. После контрольного среза сервер был остановлен без штатной result/finalization последовательности.

Положительная регрессия этого прогона: exact-Cargo посадка завершилась 32 событиями `BOARDING_COMPLETE`; `BOARDING_TIMEOUT`, `PASSENGER_BOARDING_ACTION_FAILED` и `PASSENGER_BOARDING_REISSUED` отсутствуют. Surface gate также сработал: все выбранные позиции были `LAND/water=0`, а водные кандидаты отклонялись до создания техники. Эти результаты не отменяют общий FAIL.

Итог дополнительного Transport-прогона: **FAIL**. Подтверждены потеря приказа после `ABANDONED`, отсутствие независимого global taskless deadline, повторные сбои высадки вплоть до `FALLBACK_DISEMBARK_FAILED`, неустойчивое vehicle/driver recovery, длительный распад групп, stuck churn и runtime/script errors.

### Реализовано после прогона — требуется runtime-перепроверка

- `ABANDONED` и `DESTROYED` больше не владеют движением группы: terminal cleanup не должен подавлять восстановленный infantry order.
- Terminal handoff пытается освободить точного occupant сначала штатным GetOut, затем, если link сохранился, не позднее следующей bounded попытки через owner-directed `BaseCompartmentSlot.EjectOccupant`; принятие GetOut API больше не считается доказательством выхода, а удаление техники по-прежнему запрещено до safe-clear.
- Пехотный приказ восстанавливается и проверяется по фактической waypoint queue даже при продолжающейся terminal clearance; простой факт создания waypoint больше не считается успехом bind.
- Для любой combat-ready группы введён независимый от MOB global meaningful-task timer: переход логируется, выполняется order recovery, а отсутствие задачи дольше двух commander intervals даёт Stage 3.5 ERROR.
- `WAITING_FOR_SITE` с `NO_BOARDING_SITE_WITHIN_RANGE`/`POST_APPROACH_COHESION_WAIT` получает отдельный deadline только при фактическом распаде живых участников (`maximum_pair_m > cohesion threshold`). На половине deadline выполняется одна cohesion/order recovery, на полном — vehicle request завершается bounded infantry fallback и подавляется для текущего assignment. Компактная группа, просто удалённая от безопасной базы, этот timeout не получает.
- Normal dismount не расходует recovery attempt, когда per-member move action уже активен; новая попытка учитывается только после реального поиска safe position.
- Новый CLI для повторного теста: `-aicfVehicleCohesionWaitTimeoutMs <60000..1800000>`, default `300000`.

Runtime-статус этих изменений: **NOT RUN**. До успешного Repeat-T2 текущий срез остаётся **FAIL**.

### Расширенный лог-контракт Repeat-T2

Обязательные новые события и проверяемые поля:

| Event | Когда | Минимальные поля |
|---|---|---|
| `MEANINGFUL_TASK_LOST` / `MEANINGFUL_TASK_RECOVERED` | переход task ↔ taskless | faction, slot, group generation, alive, target, role/posture, vehicle state, оба waypoint, taskless age, at-MOB |
| `MEANINGFUL_TASK_DEADLINE_MISSED` | taskless не устранён за 2 commander intervals | taskless age, deadline, target, vehicle state, at-MOB |
| `ORDER_RESTORE_REQUESTED` / `ORDER_RESTORE_RESULT` | reliability/vehicle handoff | trigger, reason, old/new waypoint, bound-to-group, is-current, queue count, postcondition, failure reason, latency |
| `WAYPOINT_REMOVED` | удаление infantry/vehicle waypoint | waypoint, kind, owner, trigger, reason, target |
| `WAYPOINT_BIND_MISMATCH` | planner сообщил success, но waypoint отсутствует в queue | faction, slot, waypoint, queue count |
| `ABANDONED_EXIT_AUDIT` | terminal state change и затем rate-limited | state/pending age, alive, logical occupants, transitions, inside bounds, restore pending, meaningful task, force/clearance attempts, next action |
| `FORCE_DISEMBARK_MEMBER` | bounded terminal force-exit | member, exact compartment/owner, direct/eject accepted, immediate result, linked/transition state, attempt/max, exact escalation |
| `IDLE_DEADLINE_SUPPRESSED` | вход, смена причины или выход из подавления узкого MOB deadline | suppression rule, suppression active, distance to MOB, meaningful task, allowed reason, vehicle state, taskless age |
| `COHESION_OUTCOME` | half/full fragmented-wait deadline | wait reason/age/deadline, alive, leader/pair spread, threshold, normalization/order outcome, member samples |
| `WAITING_FOR_SITE_EXIT` | найден site либо request завершён bounded fallback | всегда: outcome, wait age, cumulative attempts, target; при eligible-site: old request generation/base revision; при bounded fallback: request generation и vehicle-retry suppression |

`VEHICLE_SPAWN_WAIT_HEARTBEAT` дополнительно должен содержать `wait_age_ms`, `cumulative_attempts` и `cohesion_wait_age_ms`; `SLOT_ACTIVITY` — `taskless_age_ms`, `restore_pending`, spawn/terminal failure reason и `idle_suppression`.

### Repeat-T2 — обязательный gate

1. Запустить тот же T-профиль с новым отдельным server/client profile и зафиксированным commit SHA; завершить сервер штатно с полным cutoff/result evidence.
2. Принудительно воспроизвести terminal dismount failure. Для каждого живого slot должны присутствовать `FORCE_DISEMBARK_MEMBER`, `ABANDONED_EXIT_AUDIT` и коррелирующая пара `ORDER_RESTORE_REQUESTED/RESULT`.
3. Ни одна combat-ready группа не может оставаться без meaningful task дольше двух commander intervals независимо от расстояния до MOB. `MEANINGFUL_TASK_DEADLINE_MISSED` в чистом профиле запрещён.
4. Pending occupant может задержать release/delete техники, но не infantry order оставшихся бойцов. Cleanup допускается только после protected safe-clear.
5. Для искусственно распавшейся группы должна быть ровно одна half-deadline recovery и один bounded full-deadline outcome; минутный WAITING churn после полного deadline запрещён. Компактный safe-site wait не должен ложно завершаться cohesion timeout.
6. Для всех `ORDER_RESTORE_RESULT success=1` обязательны `bound_to_group=1` и `postcondition_meaningful_task=1`; `WAYPOINT_BIND_MISMATCH` запрещён.
7. Повтор должен сохранить положительные свойства текущего прогона: водные spawn-кандидаты не создают entity, exact-Cargo boarding не даёт timeout/action leak, active/reserved cap не превышен.
8. Чистый Repeat-T2 требует ноль AICF ERROR/bridge, `Virtual Machine Exception`, `SCRIPT (E/F)`, zero-radius `RandomGenerator` и необъяснённого lifecycle churn.

Итог Repeat-T2: **NOT RUN**.

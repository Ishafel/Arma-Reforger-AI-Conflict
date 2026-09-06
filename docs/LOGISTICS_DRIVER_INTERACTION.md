# Временный выход logistics driver к воротам

## Причина

В hosting evidence `logs_2026-09-06_19-05-27/script.log`, строки 7197–7313,
`US / slot=1000003 / generation=1 / job=L9_S1000003_G1` получает рейс
`TO_SOURCE`. В 19:37:57 водитель ещё жив, остаётся exact occupant и linked с
машиной, но `getting_out=1`. Пользователь лично наблюдал открытие шлагбаума.
`AICF_LogisticsWorker.Ready()` отвергал такой переход, после чего
`TickLogisticsVehicle()` немедленно вызывал `RetireLogistics()`:
отмена job/reservations, снятие waypoint/utility и передача lease в cleanup.

Закреплённый Script Diff **1.8.0.13**, commit
`3d77cc212d5cda9922daf5f45635c7300d2d4cce`, подтверждает механизм:

- `AI/ScriptedNodes/Vehicles/SCR_AISelectDoorOperatorAgent.c`: водитель может
  стать оператором ворот; в одиночном logistics roster других кандидатов нет.
- `AI/Reaction/SCR_AIGoalReaction.c`, `SCR_AIGoalReaction_OpenNavlinkDoor`:
  `SCR_AIPerformActionBehavior` для smart action с tag `OpenGate`, priority
  `PRIORITY_BEHAVIOR_OPEN_NAVLINK_DOOR`.
- `AI/Components/SCR_AIUtilityComponent.c`, `WrapBehaviorOutsideOfVehicle()`:
  добавляет `SCR_AIGetOutVehicle` и `SCR_AIGetInVehicle` с сохранённым exact
  compartment и общей related group activity.
- `AI/Behavior/SCR_AIVehicleBehavior.c`: параметры vehicle/compartment доступны
  для чтения. `SCR_AIGetInVehicle.OnActionFailed()` может запускать выход или
  teleport, поэтому этот callback не вызывается для terminal teardown нашего
  отслеживаемого return action.

Исходный лог не содержал identity native action/target. Новые события добавляют
такое evidence для последующих прогонов.

## Реализация и ограничения

`AICF_LogisticsDriverInteraction` наблюдает очередь utility единственного
живого authoritative AI. Начало допустимо только в `TO_SOURCE`,
`TO_DESTINATION` или `RETURN_HOME`, при сохранённых vehicle/lease/group/driver
identity и связанной цепочке `OpenGate + exact native return`. Выбранным
behavior должен быть соответствующий выход либо `OpenGate`; одного pending
action, `getting_out` или близких ворот недостаточно. Target находится не далее
50 м от машины, водитель остаётся в этом же ограниченном окружении.

Наблюдатель не создаёт actions, callbacks, subscriptions, get-in retries или
принудительные посадки. Job, его token, lease, cargo batches и receipts
сохраняются. Exact seat reservation штатного возврата допускается только если
она принадлежит тому же водителю; чужой occupant/reservation не разрешается.
Успех требует завершённого native `OpenGate` и полного прежнего `Ready()`:
тот же driver/group/vehicle, exact seat, отсутствие getting-in/getting-out.
Waypoint и этап рейса сохраняются.

Ожидание ограничено 120 с на взаимодействие и **120 с суммарно на весь leg**.
Через 30 с без прогресса оно завершается. Прогресс — однократные milestones,
новое лучшее расстояние к target/машине и, если у target доступен
`BaseDoorComponent`, увеличение открытого состояния двери. Повторные
`getting_out`/getting-in и колебания расстояния не переармляют clocks.
Deadline проверяется до учёта нового progress/успешного возврата.

Время подтверждённого ожидания исключается из leg/progress age; сохранённый
до остановки возраст прогресса не обнуляется. Новый физический progress
сохраняет текущую величину паузы, чтобы предыдущие ворота не дали лишнее время
следующему участку. `MAX_ROUTE_RETRIES` не меняется; route retry не сбрасывает
общий бюджет ожиданий. Предельное время leg составляет 600 с движения плюс
не более 120 с подтверждённых пауз (с дискретностью scheduler poll).

Ledger продолжает обычный `Renew()` только после повторной проверки job,
generation, endpoints, identity и ограниченного active wait. TTL остаётся
прежним; уже истёкший reserve не возрождается. Потеря graph/context или TTL
во время ожидания ведёт через controller к retirement и `Cancel()`.
Ожидание не разрешает `Reserve()` нового job или `Commit()`: после возврата
service заново выдерживает stationary hold, проверяет радиус, скорость,
exact identity, combat safety, ресурсные pools и права операции.

Controller остаётся владельцем phase/terminal orchestration; ledger —
reservations/receipts; handoff — terminal teardown сохранённых native actions
при доказанной identity; cleanup/fleet — physical clearance и release.
Смерть, смена token/generation/entity identity, native failure, чужой occupant,
Stop и deadlines прекращают ожидание. Cleanup по-прежнему не удаляет
защищённую машину или груз и не выдаёт отсутствие identity за успешный release.

Публичного уникального идентификатора причины `OpenNavlinkDoor` у behavior нет.
Распознавание консервативно сопоставляет tag, priority, выбранный behavior,
общую group activity и точный return action. Изменённые модами цепочки, утраченные
actions, target вне 50 м и выходы без этих доказательств получают прежний
fail-closed control-loss path. Проверка `!IsGettingOut()` в `Ready()` сохранена.

## Диагностика и проверка

Новые Stage4 events: `LOGISTICS_DRIVER_INTERACTION_STARTED`,
`LOGISTICS_DRIVER_INTERACTION_RETURNED`, `LOGISTICS_DRIVER_INTERACTION_FAILED`.
Сохраняется прежний schema 2 identity envelope. Дополнительные поля:
`wait_job`, `wait_generation`, `wait_group`, `wait_vehicle`, `wait_driver`,
`wait_vehicle_rpl`, `wait_driver_rpl`, `target`, `elapsed_ms`, `progress_age_ms`,
`milestone`, `leg_wait_ms`, `exact_return`, `reason`. Snapshot identity остаётся
в terminal event даже после изменения текущего контекста. Прежние events и
их поля не переименованы.

`Test-LogisticsStatic.ps1` и негативные source mutations проверяют native chain,
exact return, identity, clocks, отсутствие AI mutation в наблюдателе, TTL и
transfer gates. `Test-LogisticsLog.ps1` отвергает transfer во время ожидания,
смену identity при возврате, повторный START и transfer после terminal failure.
`-RequireDriverInteraction` требует возврат и delivery того же job/vehicle.
Синтетические логи не являются runtime evidence.

В `tools/fixtures/AICF_LogisticsRuntimeProbe.c` добавлен Enforce clock contract
`LOGISTICS_PROBE_DRIVER_CLOCK_CONTRACT passed=13 total=13`: вызываются реальные
`DeadlineReason`, `MarkProgress`, `ProgressAgeMs`, `LegAgeMs` с управляемым
временем. Это проверка clocks, не live death/foreign-driver или открытие ворот.

Evidence текущей правки: `.codex-runtime/logistics-door-20260906/`.
Исходный commit `9ef6f967997b63dcc6229e14eefd2864f93b11a1`; исходное дерево
содержало только посторонний untracked `docs/AI_LOGISTICS_IMPLEMENTATION_PROMPT.md`.
Baseline: `LogisticsStatic`, `LogisticsContracts` (37 cases), `Stage3Static`,
`Stage35Static`, `Stage35RecoveryPolicy`, `Stage4Static` — PASS, exit 0.
Сохранённых baseline failures этих шести проверок нет.

## Итоговые gates — 2026-09-06

Branch `main`, commit не создавался. Финальная временная Core fixture удалена.
Все production `.c` совпадают по SHA256 с последним Stock-2 runtime:
`final-runtime-production-hashes.json`, `FINAL_RUNTIME_CHANGED_PRODUCTION=0`.
Установленные Tools и runtime — `1.8.0.13`, engine `192142`.

| Команда / gate | Verdict | Evidence |
|---|---|---|
| `powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Test-LogisticsStatic.ps1` | PASS / 0 | `final-static/LogisticsStatic.txt` |
| Та же команда с `Test-LogisticsContracts.ps1` | PASS / 0, 56 cases | `final-static/LogisticsContracts.txt` |
| Та же команда с `Test-Stage3Static.ps1` | PASS / 0 | `final-static/Stage3Static.txt` |
| Та же команда с `Test-Stage35Static.ps1` | PASS / 0 | `final-static/Stage35Static.txt` |
| Та же команда с `Test-Stage35RecoveryPolicy.ps1` | PASS / 0 | `final-static/Stage35RecoveryPolicy.txt` |
| Та же команда с `Test-Stage4Static.ps1` | PASS / 0 | `final-static/Stage4Static.txt` |
| `git -c safe.directory=C:/Users/retar/IdeaProjects/Arma-Reforger-AI-Conflict -c core.safecrlf=false diff --check` | PASS / 0 | Нет whitespace errors |
| Terminal Workbench Validate/Compile, Arland / Everon / RHS, без fixture | PASS / 0 для каждого | `final-workbench-arland/`, `final-workbench-everon/`, `final-workbench-rhs/` |
| Everon-1 runtime, launcher | Завершён / 0 | `runtime-everon-1-launch.txt`, полный `runtime-everon-1-logs/` |
| Everon-1 log audit с `-RequireDelivery -RequireDriverInteraction` | FAIL / 1 | Нет delivery/interaction, две SCRIPT ошибки stock teardown |
| Stock-2 runtime, окончательный production, launcher | Завершён / 0 | `runtime-stock-2-launch.txt`, полный `runtime-stock-2-logs/` |
| Stock-2 log audit без требования delivery | FAIL / 1 | Две SCRIPT ошибки stock teardown |
| Production Enforce clocks | PASS, 13/13 в обоих runtime | `LOGISTICS_PROBE_DRIVER_CLOCK_CONTRACT` |

Workbench выполнялся через
`C:\Program Files (x86)\Steam\steamapps\common\Arma Reforger Tools\Workbench\ArmaReforgerWorkbenchSteamDiag.exe`
с `-noThrow -wbsilent -gproj <root/addon.gproj> -addonsDir <game/addons,repo[,RHS]>
-addons <полный graph> -logsDir <fresh evidence> -wbModule=ScriptEditor -run -validate`.
Точные root/graph arguments соответствуют трём командам из
[DEVELOPMENT.md](DEVELOPMENT.md#workbench-validate-из-терминала).
Native вызов направлялся через `| Out-File <launch.txt>` для ожидания завершения
GUI-subsystem executable из PowerShell. Все полные Workbench logs содержат
`Game successfully created`, `Script validation successful`; SCRIPT E/F,
ENGINE F, VM/null errors — 0. Resource leaks при shutdown сохранены отдельно,
как и в документированном предыдущем Workbench baseline.

Runtime запускался только canonical launcher (из корня репозитория):

```powershell
& ./tools/Start-AICFRuntime.ps1 -Role Server -Variant Everon -AICommanderMode BOTH `
  -ServerPort 2117 `
  -ProfileRoot 'C:/Users/retar/AppData/Local/AICF/LogisticsDoor-20260906-Everon-1' `
  -AdditionalArguments @('-aicfRequirePlayerForResult','0','-aicfLogisticsProbe','1',
    '-aicfLogisticsProbeDurationMs','300000','-aicfLogisticsProbePrepare','1',
    '-aicfLogisticsProbePeace','1','-aicfLogisticsProbeRepeatSource','1',
    '-aicfLogisticsProbeDepotAtSource','1')

& ./tools/Start-AICFRuntime.ps1 -Role Server -Variant Stock -AICommanderMode BOTH `
  -ProfileRoot 'C:/Users/retar/AppData/Local/AICF/LogisticsDoor-20260906-Stock-2' `
  -AdditionalArguments @('-aicfRequirePlayerForResult','0','-aicfLogisticsProbe','1',
    '-aicfLogisticsProbeDurationMs','30000')
```

`ServerPort` текущего launcher используется для client readiness и не вошёл в
server native argv; порт 2117 не заявляется как фактически применённый.
Перед финальным runtime process inventory был пуст; пользовательские процессы
не останавливались. Первый выбор profile внутри repo launcher отклонил до spawn;
успешные profiles находятся в LocalAppData. Оба launch transcripts содержат
`AICF_RUNTIME_MANIFEST_JSON` и фактический `CLI Params`.

Everon-1: 19:56:20–20:02:47, `ROSTER_READY` в 19:56:43, Stop в 20:01:43.
Окончательная защита чужого seat в terminal handoff добавлена после старта
этого прогона, затем проверена компиляцией и следующим Stock-2.
Stock-2: 20:04:00–20:05:45, `ROSTER_READY` в 20:04:14, Stop в 20:04:44.
Fixture сама вызывает `RequestClose` после 60 с cleanup observation;
`Game destroyed` подтверждён в обоих полных логах.
Их исходные абсолютные директории:

- `C:\Users\retar\AppData\Local\AICF\LogisticsDoor-20260906-Everon-1\logs\logs_2026-09-06_19-56-20`;
- `C:\Users\retar\AppData\Local\AICF\LogisticsDoor-20260906-Stock-2\logs\logs_2026-09-06_20-04-00`.

Анализировались полные `console.log` и соседние `script.log`/`error.log`.
В обоих runs при teardown две ошибки
`'SCR_BaseResupplySupportStationComponent' needs a entity catalog manager!`;
engine resource/Hierarchy/pathfinding сообщения также сохранены. Общий runtime
FAIL не повышен до PASS на основании launcher exit 0 или положительного clock
contract. В Everon-1 за выделенное время fixture не подготовила logistics рейс;
Stock-2 предназначался для окончательных clocks/startup/Stop.

**NOT RUN:** реальное открытие ворот → exact возврат → delivery; обычная
доставка без препятствий; live невозврат/смерть/чужой driver/stale identity;
Stop во время активного native interaction и физическое освобождение его lease;
client/JIP, длительный soak и ручное визуальное подтверждение. Source mutations
и synthetic log contracts проверяют соответствующие guards/analyzers, но не
заменяют эти сценарии. GUI automation не применялась.

## Изменённые файлы

В `AIConflictCore/Scripts/Game/AIConflict/`:

- `Config/AICF_LogisticsConfig.c` — конечные budgets и config diagnostics;
- `Economy/AICF_LogisticsJob.c` — state ожидания, clock accounting, exact reservation;
- `Economy/AICF_LogisticsLedger.c` — bounded renew и закрытый transfer gate;
- `Economy/AICF_LogisticsService.c` — сохранение job, stationary revalidation, Stop;
- `Vehicles/AICF_LogisticsDriverInteraction.c` — новый native-chain observer;
- `Vehicles/AICF_TransportTripController.c` — orchestration ожидания/возврата/terminal;
- `Vehicles/AICF_VehicleTransitFlow.c` — progress/route clocks с учётом паузы;
- `Vehicles/AICF_VehicleTaskHandoff.c` — identity-safe terminal cancellation.

Проверки: `tools/Test-LogisticsStatic.ps1`, `tools/Test-LogisticsContracts.ps1`,
`tools/Test-LogisticsLog.ps1`, `tools/fixtures/AICF_LogisticsRuntimeProbe.c`.
Документация: этот файл, `docs/ARCHITECTURE.md`, `docs/TESTING.md`.

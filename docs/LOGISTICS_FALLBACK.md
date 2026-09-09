# Возврат водителя и перенос машины логистики

Политика изменена по запросу владельца от 2026-09-08: для логистики допустим
перенос через проблемные места, включая возврат вышедшего водителя в машину.
Нахождение в поле зрения камеры не запрещает восстановление. Игрок внутри
машины, чужой occupant/reservation и физическое использование игроком в радиусе
8 м остаются запретами. Политика скрытого восстановления армейского транспорта
не меняется.

## Поведение и владельцы

Обычная езда, две попытки перестроения маршрута и штатная цепочка открытия ворот
сохраняются. После исчерпания маршрута либо неудачного возврата/взаимодействия
`AICF_TransportTripController` создаёт `AICF_LogisticsFallback`. Этот контекст
наследует immutable snapshot водителя, группы, exact seat, vehicle/Rpl, lease,
generation, phase и job token. Он не меняет фазу, ресурсы или native actions.

Handoff завершает только отслеживаемую native цепочку и снимает свой waypoint.
После отдельного scheduler tick `AICF_VehicleBoardingFlow` прерывает очередь
посадки exact AI и вызывает закреплённый `GetInVehicle(... forceTeleport=true)`
для его свободного pilot seat. До трёх попыток с интервалом 3 с. Истина от
native вызова не означает успешную посадку: следующий tick повторяет `Ready()`
с проверкой occupant, compartment link и отсутствия get-in/get-out transition.
Живой водитель в другой машине или на другом месте не перемещается.

При сбое ворот или застревании `AICF_VehicleTransitFlow` переносит ту же машину
на свободный участок дороги на расстоянии 12–90 м. Проверяется не более 15
кандидатов, по одному на tick. Направление берётся к обнаруженным воротам либо
текущей цели; конкретных имён баз и координат карт в Core нет. Проверяются
земля, вода, наклон и prefab OBB в конечной позиции. Прямой свободный коридор
через препятствие не требуется. Линейная и угловая скорости обнуляются перед
`SetWorldTransform`. Ожидание combat или близкого использования игроком также
ограничено общим deadline.

В одной фазе поездки допускается до четырёх восстановлений, каждое до 45 с.
Возраст leg и счётчик fallback не сбрасываются при переносе. Истёкший TTL,
потеря identity, смерть, горение, чужое место, cargo fault и недоступный graph
завершают восстановление безопасно. Возврат водителя и перемещение машины не
создают новый job/lease, не пересоздают entities, не начисляют припасы и не
подменяют расход ресурса.

`ResumeAfterFallback` начинает наблюдение с **новой** позиции. Успех движения
по-прежнему требует 6 м фактического перемещения и сокращения расстояния к цели
на 3 м. Пока это не доказано после переноса, прибытие закрыто. Выгрузка затем
требует обычных проверок радиуса, скорости, stationary hold и stock containers.

Первичное появление водителя также происходит на свободной земле рядом с
машиной, за её габаритами. Ранее asynchronous roster создавался прямо в origin
кузова до посадки; в тесте СССР это сопровождалось `SPAWN_IDENTITY_LOST` на
шаге физики. Создание возле машины сохраняет прежние roster readiness gates
и штатную принудительную первичную посадку.

## Проверки

Основные события: `LOGISTICS_FALLBACK_STARTED`, `LOGISTICS_DRIVER_TELEPORT_ISSUED`,
`LOGISTICS_VEHICLE_RELOCATED`, `LOGISTICS_FALLBACK_RESUMED`, затем
`LOGISTICS_RECOVERY_SUCCEEDED` и реальные `LOGISTICS_*_COMMITTED`.
Общие identity-поля сохранены; перенос пишет `cargo_before/cargo_after`.
Остановка fallback использует существующий identity-safe terminal handoff и
`LOGISTICS_DRIVER_INTERACTION_FAILED`; анализатор сопоставляет его с активным
`LOGISTICS_FALLBACK_STARTED`, не считает новой native OpenGate цепочкой.

`Test-LogisticsStatic.ps1` проверяет authority, seat/cargo identity, ограничение
попыток, физическую конечную позицию и исключение скачка из progress/arrival.
`Test-LogisticsContracts.ps1` проверяет отрицательные изменения этих контрактов
и синтетические logs: чужой driver, изменение cargo, отсутствие motion или
resume, превышение дальности/deadline и ложное подтверждение переноса.

`Test-LogisticsLog.ps1 -RequireFallback` требует в полном остановленном логе
resume и delivery того же job/vehicle; после переноса требуется отдельный
физический motion proof. Отсутствие события либо только `issued=1` gate не
закрывают. `-AllowActiveAtEnd` годится только для промежуточной диагностики.

Для воспроизводимого runtime временно копируются `AICF_LogisticsRuntimeProbe.c`
и `AICF_LogisticsFallbackProbe.c` в отдельный source root. Параметры:
`aicfLogisticsProbe=1`, `aicfLogisticsProbeRouteMatrix=1`, подготовка баз из
[LOGISTICS_ROUTE_MATRIX.md](LOGISTICS_ROUTE_MATRIX.md) и
`aicfLogisticsProbeFallback=1`. Последний один раз на сторону вызывает выход
водителя после настоящей погрузки. Весь возврат, перенос, дальнейшее движение
и transfer выполняет production-код. Синтетический отказ явно помечен
`test_only=1`; естественные сбои ворот проверяются отдельно.

## Evidence 2026-09-08

Каталог: `.codex-runtime/logistics-relocation-20260908-212403/`.
Baseline сохранён до правок: восемь применимых аудиторов — PASS/0, включая
110 Logistics contracts. Ошибочный вызов несуществующего
`Test-LogisticsStaticContracts.ps1` исправлен на `Test-LogisticsContracts.ps1`;
это ошибка команды, не baseline failure кода.

Статические gates: `powershell.exe -NoProfile -ExecutionPolicy Bypass -File
tools/Test-<Name>.ps1`, где Name — `LogisticsStatic`, `LogisticsContracts`,
`Stage3Static`, `Stage35Static`, `Stage3StaticContracts`, `Stage35RecoveryPolicy`,
`Stage4Static`, `LogisticsMapMarkersStatic`: **PASS/0**, все восемь.
LogisticsContracts: 134 случая против 110 до правок. Результаты и полные команды:
`release-static-results.json`, `release-Test-*.txt`.

Workbench: терминальная команда `ArmaReforgerWorkbenchSteamDiag.exe -noThrow
-wbsilent -gproj <root>/addon.gproj -addonsDir <installed>,<source>[,<RHS>]
-addons <dependency graph> -logsDir <evidence> -wbModule=ScriptEditor -run
-validate` для Everon, Arland и RHS — **PASS/0**. В каждом есть
`Script validation successful`, SCRIPT E/F отсутствуют. Exact CLI сохранён в
полных `workbench-*-release/console.log`, итог — `release-workbench-results.json`.
Первое слишком длинное условие новой fixture дало `Formula too complex`;
условие разбито на guard clauses, повторный compile прошёл.

Runtime запускается только `tools/Start-AICFRuntime.ps1`, `BOTH`, свежие profiles,
source snapshots `source`/`source-2`. Итоговые 111 production `.c` побайтно
совпадают с запущенным `source-2`: `release-source-hashes.json`. Манифесты CLI
сохранены; client launcher подтвердил живой process и `ROSTER_READY`.

Первый run подтвердил:

| Сторона / job | Возврат и перенос | Доставка |
|---|---|---|
| US / `L1_S1000002_G1` | Driver D=`…456C`, vehicle V=`…44FE`: teleport driver, перенос 12,736 м; позднее ещё 20,742 м после естественного stuck | Kermovan → Military Depot, 260 в 21:45:31, cargo 260→0, discrepancy=0 |
| USSR / `L3_S1000001_G3` | D=`…4BD9`, V=`…4B8B`: teleport driver, перенос 22,259 м, затем отдельный physical recovery | Chotain → Provins, 260 в 21:48:46, cargo 260→0, discrepancy=0 |

Другая американская машина дважды восстановилась после **естественного**
`DRIVER_INTERACTION_NATIVE_CHAIN_LOST` у ворот. Индекс:
`run-1-recovery-event-index.txt`; полные остановленные logs: `run-1-stopped/`.
Этот run остановлен через проверенные PID после доставки и не объявляется
полным lifecycle PASS. Даже промежуточный `Test-LogisticsLog.ps1
-AllowActiveAtEnd -RequireDelivery -RequireFallback` даёт **FAIL/1** из-за шести
`SCR_InstigatorContextData / INSTIGATOR_OTHER` до исправления начального spawn
водителя. Ошибок identity/cargo/motion в анализаторе не обнаружено.

В заключительном run с исправленным spawn оба водителя USSR достигли
`DRIVER_READY` с первой generation; прежний `SPAWN_IDENTITY_LOST` не повторился.
Тот же `L1_S1000000_G1`, D=`…4320`, V=`…42D3` прошёл возврат водителя,
перенос 22,1 м, физическое движение и доставку 260 из Régina в Pennants Pass
в 21:55:45. На клиенте до закрытия подтверждены те же Rpl IDs,
`pilot=1`, `cargo=260`, `capacity=800` и движение.

В этом random HQ layout подготовка US осталась `found=0 required=4`;
двустороннее движение подтверждено первым run, повтор за US на итоговой
версии начального spawn — **NOT RUN**. Автоматический запрос spawn игрока
из клиентской fixture вызвал две VM exceptions в штатных
`SCR_BaseGameMode.CanPlayerSpawn_S` и `ConsumeSuppliesOnPlayerSpawn_S`.
Поэтому весь runtime не объявляется PASS: эти ошибки сохраняются в evidence,
не исключаются из log audit и не исправляются под желаемый verdict.

Оба итоговых процесса завершились штатно, exit=0: client в 21:56:16,
server в 21:59:35; оба полных console logs содержат `Game destroyed.`.
Архив всех engine logs — `release-stopped/server` и `release-stopped/client`.
Клиентский полный log не содержит SCRIPT E/F/VM errors.

Итоговая команда `powershell.exe -NoProfile -ExecutionPolicy Bypass -File
tools/Test-LogisticsLog.ps1 -LogPath <server-console> -ClientLogPath <client-console>
-RequireDelivery -RequireFallback -RequireLoadedClient -RequirePolicy
-RequireLedger -RequireGraph -RequireSearch -MinimumDurationMs 480000` дала
**FAIL/1** (`release-log-audit.txt`). Кроме двух указанных respawn exceptions,
cleanup удержал две машины с `VEHICLE_CLEANUP_RETAINED`: после отключения
клиента штатный player list ещё содержал player=1 без позиции, поэтому
существующий fence выбрал `PLAYER_POSITION_UNKNOWN_GRACE_EXPIRED`. Этот
cleanup gate не ослаблялся ради PASS. Отдельные fixture ledger/graph contracts
в итоговой топологии не наблюдались: соответствующие дополнительные gates —
**NOT RUN**. Проверки same-job fallback→motion→delivery, баланса грузов и
client cargo/identity не дали отдельных failures, но это не заменяет общий
runtime verdict. После проверки server и client закрыты, компьютер не выключался.

Изменённые production-файлы текущей правки:

- `Vehicles/AICF_LogisticsFallback.c` — контекст, guards и конечная площадка;
- `Vehicles/AICF_VehicleBoardingFlow.c` — exact driver teleport;
- `Vehicles/AICF_VehicleTransitFlow.c` — перенос машины и закрытие arrival до motion;
- `Vehicles/AICF_TransportTripController.c` — orchestration и возобновление leg;
- `Vehicles/AICF_LogisticsRouteRecovery.c` — новый physical baseline;
- `Vehicles/AICF_LogisticsAcquisitionFlow.c` — первичный spawn водителя вне кузова;
- `Economy/AICF_LogisticsJob.c` — ограничение числа fallback на leg;
- `UI/AICF_LogisticsMapMarkers.c` — статус «Восстанавливает рейс».

Все пути выше относительно `AIConflictCore/Scripts/Game/AIConflict/`.
Также изменены три Logistics аудитора, добавлена `AICF_LogisticsFallbackProbe.c`,
обновлены README, ARCHITECTURE, TESTING, LOGISTICS_RECOVERY и этот отчёт.
Ранее сделанные маркеры и route-matrix fixture сохранены, новый commit не создавался.
Проверка визуального отображения маркеров, ручного управления, чужого игрока
в машине и JIP — **NOT RUN**; identity guards дополнительно проверены статически.

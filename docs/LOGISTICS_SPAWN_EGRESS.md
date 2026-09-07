# Позиция спавна и свободный выезд AI-логистики

## Причина

В пользовательском Everon-прогоне `stage1-server-18633` Урал USSR,
`slot=1000002`, vehicle `0x40000000000067F4`, появился в 22:17:21 и после
начала рейса оставался около `<2505.2,85.0,6029.2>`. Две попытки восстановления
не дали движения; пользователь наблюдал пересечение с постройкой.
Лог доказывает отсутствие движения, но не содержит точной пары контакта.

В исходниках `ac91f46` `KeepExternalObstacle` исключал физические children
всей editable-композиции depot. Такое исключение не применяется к физике
созданной машины, поэтому допуск мог разрешить пересечение с постройкой.
Прежние runtime-пробы проверяли внешние препятствия, но не такой контакт.

## Новый контракт

- Body OBB конкретного prefab проверяется против **всех** физических объектов
  с `EPhysicsLayerPresets.Vehicle`, включая свои props, персонажей и машины.
  Stock `IsOccupied` с удалением wrecks не вызывается.
- Совместимые child/near slots остаются привязаны к точному production/catalog.
  У каждого проверяются исходное направление, разворот на 180° и восемь
  направлений на кольцах 6, 12 и 18 м. Перебор детерминированный: максимум
  16 stock slots × 26 вариантов, без бесконечного retry внутри вызова.
- Новые позиции остаются внутри базы. Ориентация вариантов на кольцах
  учитывает наклон земли. Четыре нижних угла OBB проверяются на воду/опору:
  перепад поправок не больше 0,6 м, вертикальная поправка до 1,5 м,
  зазор над землёй 0,08 м. Общий vehicle surface gate также сохраняется.
- В направлении машины проверяется короткий выезд: длина кузова + 4 м,
  минимум 12 и максимум 18 м. Шесть последовательных `TraceMove` проводят
  **полный OBB** по рельефу; `TracePosition` проверяет конечные положения.
  Это не требование прямого коридора до дороги или места доставки.
- Свободная позиция и выезд повторно проверяются при commit. Все четыре
  вектора snapshot stock slot сверяются с живым slot; отдельный выбранный
  transform используется для создания машины. Construction reservations,
  общие vehicle reservations, authority, provider, faction, lease и generation
  остаются обязательными. История неудачных выездов хранит stock transform.
- Первый waypoint направлен в проверенный выезд. Его прохождение требует
  фактического приближения машины до 5 м. Дальше продолжается обычный leg;
  отдельные recovery budgets не сбрасываются выдачей waypoint.

`LOGISTICS_SPAWN_SITE_SELECTED` содержит slot, candidate, spawn position,
forward, exit, offset и prefab. `LOGISTICS_SPAWN_EXIT_REACHED` подтверждает
физический выезд. Старые rejection events сохранены; причины дополнены
`SLOT_SEARCH_EXHAUSTED`, `SPAWN_EXIT_BLOCKED`, `OUTSIDE_DEPOT_SEARCH_AREA`.
Неудачный поиск не создаёт vehicle и не удаляет окружение.

## Проверки и evidence

Корень локального evidence: `.codex-runtime/spawn-egress-20260907/`.
`live-server-snapshot/` — копия ещё работающего исходного сервера, **не**
остановленный runtime gate новой версии.

Baseline до изменения: все девять команд дали PASS / 0, failures отсутствовали:
`Test-Stage3Static.ps1`, `Test-Stage35Static.ps1`,
`Test-Stage3StaticContracts.ps1`, `Test-Stage35RecoveryPolicy.ps1`,
`Test-Stage4Static.ps1`, `Test-LogisticsStatic.ps1`,
`Test-LogisticsContracts.ps1`, `Test-ConstructionStatic.ps1`,
`Test-ConstructionContracts.ps1`. Каждая запускается через
`powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/<имя>`.
Результаты: `baseline-results.json` и `baseline-*.txt`.

После правки те же девять команд дали PASS / 0 (`after-results.json`);
`Test-LogisticsContracts.ps1` выполнил 110 positive/negative cases. После
окончательного выравнивания машины по рельефу повторены Stage3Static,
Stage35Static, LogisticsStatic и ConstructionStatic — PASS / 0
(`final-static-results.json`). Сохранённых static baseline failures нет.

Терминальный Workbench Validate/Compile по командам `docs/DEVELOPMENT.md`
для трёх production graphs — Arland, Everon, RHS — PASS / 0: есть
`Game successfully created`, `Script validation successful`, нет
`SCRIPT (E/F)` и `ENGINE (F)`. Exact argument arrays и полные console/script/error
logs сохранены в `workbench-final-*`, результаты — `workbench-final-results.json`.
Это отдельный compile gate; native resource cleanup messages не являются
доказательством успешного игрового прогона.

Подготовлена изолированная Everon-сборка `source/` с runtime fixture.
Все 104 production Core `.c` совпадают с рабочей копией
(`test-source-integrity.json`). Компиляция окончательной fixture — PASS / 0,
`workbench-fixture-ready/` и `workbench-fixture-ready-exit.txt`.
Game/Server/Tools — `1.8.0.13` (`versions.json`). `git diff --check` — PASS / 0.
Команда запуска подготовлена в
`run-test-server.ps1` и вызывает только canonical `tools/Start-AICFRuntime.ps1`.
Сборка автоматически заканчивает прогон после доставки Урала и cleanup либо
по deadline. Этот изолированный fixture-прогон не запускался; пользователь
попросил обычный сервер и клиент с новым кодом.

Mutation contracts обновлены под явное изменение admission: запрет исключать
props, обязательный sweep выезда, проверка воды/опоры, snapshot transform,
ограниченность поиска и первая физическая точка. Прежнее требование
«при заблокированных обоих выездах разрешить spawn» противоречит новому
пользовательскому требованию и больше не является контрактом.

Обе fixtures спавна обновлены: заблокированный выезд запрещён, препятствие
только сзади не запрещает свободный передний выезд, physical child своего depot
блокирует кузов, позднее препятствие отклоняет commit. Проверяется reservation
точной выбранной позиции, а не предположение, что она равна stock origin.

Ограничения: OBB консервативно приближает форму машины. Свободные 12–18 м
не доказывают проходимость всего маршрута, не предотвращают появление новых
препятствий и не исправляют уже застрявшие машины работающего старого сервера.
Физический spawn/выезд/доставка новой версии, fixture-прогон и JIP —
**NOT RUN / не покрыты**. Обычный server/client startup выполнен ниже.

## Пользовательский Everon-прогон 22:45–23:04

По разрешению пользователя сервер и клиент перезапущены через
`tools/Start-AICFRuntime.ps1 -Role Server|Client -Variant Everon
-AICommanderMode BOTH` с отдельными свежими profiles и `-ServerProfileRoot`
для клиента. Полные manifest/CLI и logs находятся в
`.codex-runtime/everon-play-20260907-224509/`. Server profile:
`C:\Users\retar\AppData\Local\AICF\Server-Everon-20260907-224509`,
client profile: `C:\Users\retar\AppData\Local\AICF\Client-Everon-20260907-224509`.
Все 104 production Core `.c` совпали с сохранёнными при запуске hashes.

`ROSTER_READY` подтверждён в 22:45:50, подключение игрока — в 22:46:32.
У СССР зарегистрированы depot УАЗ/Урал, у США Humvee/M923. До остановки
все четыре worker оставались `IDLE_AT_DEPOT`, generation 0, без vehicle.
`LOGISTICS_SPAWN_REQUESTED`, `LOGISTICS_SPAWN_REJECTED`,
`LOGISTICS_SPAWN_SITE_SELECTED`, `LOGISTICS_JOB_RESERVED`, загрузки и доставки
не наблюдались. Повторялся общий planning reason
`NO_SOURCE_SURPLUS_OR_MINIMUM_BATCH`; он не доказывает конкретную причину
отказа для конкретной пары баз. Новый physical admission не вызывался.

Пользователь закрыл клиент; в 23:04:58 по его просьбе сервер остановлен
`Stop-Process -Id 25448 -Force` после повторной проверки PID/name/profile.
Повторный process inventory подтвердил отсутствие сервера и клиента.
Native launcher exit: server 1 после принудительной остановки, client 0.
Сохранены полные `stopped-server-logs/` и `stopped-client-logs/`.

`powershell.exe -NoProfile -ExecutionPolicy Bypass -File
tools/Test-LogisticsLog.ps1 -LogPath <stopped-server-console.log>
-RequireDelivery` — **FAIL / 1**: `FULL_STOPPED_LOG_REQUIRED` (нет штатного
Stop marker после принудительного завершения) и
`PHYSICAL_DELIVERY_NOT_OBSERVED`. Этот результат не является PASS runtime.
В трёх server logs нет `SCRIPT (E/F)`, `ENGINE (F)` или найденных VM/null
exceptions. В клиенте в 23:04:04 два сообщения `Cannot find editor component
'SCR_MenuLayoutEditorComponent', local instance of editor manager not found!`,
повторённые в console/script/error logs (шесть записей). Эти ошибки сохранены,
client lifecycle не объявляется полностью успешным. Native GUI/resource/pathfinding
сообщения остаются в полных logs; фильтр ошибок не заменяет эти logs.

## Изменённые файлы

- `Vehicles/AICF_LogisticsVehicleFootprint.c`,
  `Vehicles/AICF_LogisticsSpawnGeometry.c`, `Vehicles/AICF_VehicleSpawner.c`:
  физический допуск, позиции, проверка выезда, reserve/commit.
- `Economy/AICF_LogisticsDepotRegistry.c`, `Economy/AICF_LogisticsJob.c`,
  `Economy/AICF_LogisticsExitHistory.c`, `Vehicles/AICF_LogisticsRouteRecovery.c`:
  удаление composition exclusions, раздельные transforms, cooldown и первый leg.
  Пути здесь относительно `AIConflictCore/Scripts/Game/AIConflict/`.
- `tools/Test-LogisticsStatic.ps1`, `tools/Test-LogisticsContracts.ps1`,
  `tools/fixtures/AICF_LogisticsSpawnClearanceProbe.c`,
  `tools/fixtures/AICF_LogisticsSpawnParityProbe.c`: проверки нового контракта.
- `docs/ARCHITECTURE.md`, `docs/LOGISTICS_RECOVERY.md`,
  `docs/LOGISTICS_SPAWN_CLEARANCE.md`, `docs/LOGISTICS_SPAWN_PARITY.md`,
  `docs/LOGISTICS_SPAWN_EGRESS.md`: актуальный контракт и пометки исторических отчётов.

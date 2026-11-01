# Длительный WCS бой под высокой нагрузкой — 01.11.2026

Проверка **не подтвердила устранение просадок FPS**. Обе исправленные vehicle
ошибки не вернулись, реальные поездки и повторное использование тех же машин
подтверждены. Полный runtime gate остаётся **FAIL** из-за других SCRIPT errors.

Ветка `codex/wcs-highload-soak`, production snapshot `52ab958`.
Evidence: `.codex-runtime/wcs-highload-soak-20261101/REPORT.md`;
основной запуск — `full-v5/`, полные остановленные журналы —
`full-v5/server-logs/` и `full-v5/client-logs/`.

## Условия и границы вывода

WCS Everon, `EveronWCSRHS`, `Standard`, `Hard`, `AICommanderMode BOTH`:
32 минуты после server roster readiness, 30 минут client game session.
Оба процесса завершились сами, native exit0 / `Game destroyed`.
CLI manifests и server readiness proof сохранены в launcher logs.
395 production files совпали с root; frozen stage398files/source drift0.

Изолированный `AICF_LongBattleProbe.c` назначает обеим армиям существующие
Co-op mixed-fleet recipes до materialization slots. Это controlled initial
intent, а не обычный Standard default. Дальше работают штатные purchase,
экономика, quotas, recruitment, boarding, native driving и combat.
Лимиты, здоровье, game clock, позиции бойцов и машин не меняются.
Дополнительно установлены `AICF_ServerPerformanceProbe.c` и
`AICF_BoardingRegistrationProbe.c`; native stack trace observer не установлен.

Клиент сам проходит native faction/loadout/spawn и использует локальную
`CameraBase` через штатный `CameraManager`. Камера следует за живым участником
наступающей группы; она не перемещает игрока или managed actors.
Это terminal observer benchmark. Визуальная проверка человеком и совпадение
с первоначальным игровым маршрутом/камерой **NOT RUN**; GUI workflow запрещён.
Прямое causal before/after сравнение разных миров/карт/составов не выполнялось.
Тестовый компьютер: Ryzen9 5900X, RTX3080Ti,31,93GB RAM; server/client на одном
компьютере. Разрешение рабочего стола не выдаётся за проверенное render resolution.

## FPS

High load определён заранее как `AIChar >= 140`. Максимум: server302,
client303. Значения FPS ниже — средние штатных десятисекундных окон,
длинный кадр — их максимальный `frame time max`.

| Срез | Окон | Средний FPS | Минимум | Длинный кадр | Окон ниже40FPS |
|---|---:|---:|---:|---:|---:|
| Server, high load и подключённый client | 156 | 16,30 | 0,6 | 6786,8мс | 149 |
| Server, весь high load | 164 | 18,01 | 0,6 | 6786,8мс | 150 |
| Client, весь high load | 161 | 51,66 | 29,0 | 194,5мс | 31 |
| Client, подтверждённые окна у фронта и high load | 114 | 52,74 | 29,0 | 146,8мс | 23 |

Qualified frontline window требует проверки всей границы десятисекундного окна:
`frontline=1`, target base не своей фракции, >=3 живых членов группы,
leader-target distance<=500м, actor действительно replicated,
actor distance от server readback<=30м, camera error<=1м,
camera-actor distance<=150м. Ближайшие view samples на обеих границах имеют
разрыв не больше6с. 114окон —19мин суммарного подтверждённого времени
между18:32:05 и18:53:46, а не непрерывная21-минутная visual verification.
У наблюдаемой группы `0x60000000000008AD` также подтверждён реальный
`SCR_AISuppressGroupClusterBehavior:RUNNING` в18:36:27; это не только близость базы.

Все188server/180client windows сохранены. Весь server avg21,58/min0,6;
client avg52,71/min29,0. Загрузка не скрыта: client initial max3207,6мс;
server worst6786,8мс случился уже в бою18:44:27 при295AIChar.

## Подтверждённый источник серверной нагрузки

109полных profiler windows после входа в high load:
`RefreshCensus`8027calls,776568inclusive_ms из1664573window_ms —46,65%;
max424мс за один вызов. Самое тяжёлое окно18:37:10:
13366из15124мс —88,37%. `BuildCounts`3472calls,456649мс —27,43%, max255мс.
`VehicleUseCurrent`552548calls,142157мс —8,54%; max3мс.
`GroupTrace`0,40%, max42мс.

Это inclusive elapsed time, а не аппаратный CPU counter. Вложенные timings
не суммируются. Наблюдение подтверждает большую стоимость повторных полных
quota census; оно не разделяет все внутренние затраты `BuildCounts` и
не доказывает, что census объясняет каждый длинный кадр. Найм/оборудование
новых WCS actors и ошибки pathfinding также присутствуют в полных журналах.
Производственная оптимизация выявленного bottleneck в этот проверочный
snapshot не внесена; общего FPS PASS нет.

## Техника и исправленные ошибки

В полных server/client sibling logs: original
`vehicle provided is not registered as used inside group!`0,
`VEHICLE_NATIVE_SUBGROUP_REJECTED reason=ASSIGNMENT_SCOPE`0, VM0.
8actual initial registration checks, все до actions;5exact occupant confirmations;
10tactical registrations, отказов0;5same-vehicle resumes.
В4из10scope probes BASE имеет разные snapshot/slot positions при совпадении
base/revisions/group/generation. Исправленный guard реально exercised.

После tactical return сохранены entity/Rpl/lease_generation/
vehicle_generation/lifecycle identity и подтверждён native route progress:
`trip-RHS_AFRF-1-1-4` —547,54м/31progress events;
`trip-RHS_USAF-0-1-1` —238,11м/12events.
Это максимальное XZ удаление от первой trace позиции после resume.

8покупок,1666supplies;5машин реально двигались. Таблица показывает максимальное
XZ удаление от первой trace позиции, а не полную длину пройденного маршрута.

| Operation | Машина | Supplies | Удаление,м | Max speed,m/s | Итог |
|---|---|---:|---:|---:|---|
| RHS_AFRF-0-1-2 | Ural4320 transport | 200 | 845,58 | 17,68 | TACTICAL_DISEMBARK_TIMEOUT |
| RHS_USAF-0-1-1 | M923A1 transport | 200 | 456,56 | 14,36 | TACTICAL_DISEMBARK_TIMEOUT |
| RHS_USAF-1-1-3 | M923A1 transport | 200 | 269,43 | 14,55 | TACTICAL_DISEMBARK_TIMEOUT |
| RHS_AFRF-1-1-4 | Ural4320 transport | 200 | 907,78 | 17,56 | NO_SURVIVING_MOBILE_CREW |
| RHS_AFRF-3-1-5 | Ural4320 transport | 200 | 567,45 | 17,45 | TACTICAL_DISEMBARK_TIMEOUT |
| RHS_USAF-3-1-10 | M923A1 transport | 200 | 0,02 | 0,10 | PRESET_BOARDING_DEADLINE |
| RHS_USAF-4-1-8 | M1025 armed M2HB | 133 | 0,01 | 0,10 | PRESET_BOARDING_DEADLINE |
| RHS_AFRF-5-1-9 | BTR70 | 333 | 0,01 | 0,10 | PRESET_BOARDING_DEADLINE |

В operation table опущен общий префикс `trip-`. Общая сумма покупок —1666;
танки не покупались, их поездки этим запуском не доказаны.
Отдельный exact boarding observer подтвердил103,158м для AFRF slot3.
Успех транспортных поездок не означает PASS всех классов/полного lifecycle.

## Строгие gates и оставшиеся ошибки

Server:4`MEANINGFUL_TASK_DEADLINE_MISSED` (US slots2/5 в APPROACHING_SITE,
US slot0 в TACTICAL_DEPLOYMENT),4`VEHICLE_CLEANUP_RETAINED` после client exit
с `PROTECTED_CLEARANCE_PLAYER_POSITION_UNKNOWN_GRACE_EXPIRED`.
Вместе с8bridge rows это16console SCRIPT E /48raw sibling SCRIPT E.
Неизвестная player position не превращена в разрешение удаления машины.
Client SCRIPT E/F0. ENGINE F/VM/OOM0 на обеих ролях.

`Test-BoardingRegistrationLog.ps1 -MinimumMovementMeters 100 -RequireAssignmentScope`
**FAIL1** на complete sibling SCRIPT errors. Отсутствие двух конкретных ошибок
и motion proof не подменяют этот FAIL. Raw E: server552, client548;
в том числе WORLD/RESOURCES/PATHFINDING/ENTITY и другие категории,
поэтому это не только resource-free smoke. Terminal WB compilePASS0,
validation successful/fatal0; WB raw resourceE78 остаётся FAIL.

Профильный baseline3/3PASS. Полный offline292 в четырёх canonical runner shards:
290PASS/2FAIL. `Test-LoadoutLibraryContracts` упал на junction permission;
точный повтор с разрешением временной junction далPASS0 без изменения кода.
`Test-Stage35Static` ложно считал `%25` в percent encoding неподдерживаемым
format placeholder: доказанный audit bug исправлен отдельным test-only
commit0fa8fbe. [LANGUAGE_AUDIT.md](LANGUAGE_AUDIT.md): новый17-case contract
воспроизводимо отвергает старый helper; исправленный contract и Stage35PASS0.
Это единственные потребители изменённого language audit.
Итоговая точная объединённая матрица —293unique checksPASS, missing/duplicates0,
с исходными FAIL и тремя dependency/environment rechecks в evidence.
Это не новый монолитный запуск All293; runtime FAIL им не снимается.

## Воспроизведение и артефакты

Подготовить изолированный production stage по `DEVELOPMENT.md`, добавить
в Core `Scripts/Game/AIConflict/` три перечисленных fixture с именами `ZAICF_*`.
Получить graph через canonical `Start-AICFRuntime.ps1 -DryRun`, выполнить
terminal Workbench Validate/Compile, сохранить source hashes.
Обе роли запускаются из отдельных терминалов только canonical launcher.

Server: `-Role Server -Variant EveronWCSRHS -MatchMode Standard -Difficulty Hard
-AICommanderMode BOTH -IsolatedLoadoutLibrary`, additional arguments:
`-addr 127.0.0.1:2690 -aicfLongBattle 1 -aicfLongBattleDurationMs 1920000
-aicfBoardingRegistrationObserveOnly 1 -aicfBoardingRegistrationDistanceM 100`.
Client: тот же variant/mode/difficulty/stage, `-Role Client -ClientLogStats
-ServerPort 2690 -ServerProfileRoot <fresh-server-profile> -IsolatedLoadoutLibrary`,
additional `-aicfLongBattle 1 -aicfLongBattleClientDurationMs 1800000`.
Сохранять `AICF_RUNTIME_MANIFEST_JSON`, полные логи и native exit каждой роли.

Точные команды текущей задачи и анализаторы сохранены в evidence `Run.ps1`,
`Analyze.ps1`, `QualifyView.ps1`, `Profile.ps1`, `Vehicles.ps1`, `Facts.ps1`.
`integrity.json`, `tactical-cycles.json`, `profiler-*.json`, `*-stats.json`,
`qualified-*.json`, `vehicles.json` содержат полные численные результаты.

Предварительные попытки сохранены: два WB FAIL тестовой камеры исправлены;
pilot-v3 штатно остановлен, но камера перезаписывалась native player camera;
pilot-v4 принудительно завершён из-за выбора muster target;
soak-v5 North принудительно завершён при недостаточной нагрузке и
`NO_FEASIBLE_SAFE_BARRACKS` у одной армии. Они не объявляются финальными PASS.
Первый последовательный All прерван для четырёх полных shards; это не PASS All.
Все6stages byte-verified в ZIP и удалены; собственных native процессов0.

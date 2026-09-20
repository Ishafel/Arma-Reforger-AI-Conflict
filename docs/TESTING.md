# Проверки и evidence

## Строительство на всех точках stock Everon — 2026-09-20

Изолированный 15-минутный прогон: все 39 активных точек переданы US и полностью
снабжены, production поиск и физические строители сохранены. Завершены 32
постройки на 24 точках вне HQ; на 15 точках результата нет. 145 отмен по
`SEARCH_BUDGET_EXHAUSTED`; отсутствие свободного места этим не доказано.
Static до/после и Workbench — PASS; native exit 0. Строгие runtime audits —
FAIL из-за двух stock resupply script errors при shutdown; других нарушений
целевых construction/builder audits не найдено. Полная таблица всех точек,
точные команды, source parity, evidence и NOT RUN:
[CONSTRUCTION_ALL_BASES_20260920.md](CONSTRUCTION_ALL_BASES_20260920.md).

## Строительство при боевой угрозе — 2026-09-19

Удалён threat gate в `Build()`, отдельная группа строителя получает `HOLD_FIRE`.
`AICF_BaseBuilderWorkPolicy` по текущей authoritative identity исключает выбор
боевых и danger behaviors, сохраняя движение и физические проверки работы.
Команды, baseline, результаты и ограничения:
[BUILDER_COMBAT_POLICY.md](BUILDER_COMBAT_POLICY.md).

## Спавн пехоты вне препятствий — 2026-09-19

В `AICF_GroupSpawner` добавлен opt-in `AICF_InfantrySpawnPlacement`:
свободное место и выход с navmesh проверяются непосредственно перед каждым
асинхронным member spawn. Координата застрявшего бойца у авиабазы отклонена;
отдельный stock EveronNorth runtime дал `passed=1 alive=20 moving_attackers=12`:
12 атакующих групп сместились на 200–302 м за 90 секунд, `Failed move`/VM нет.
Native resource/world diagnostics и две resupply ошибки при shutdown сохранены.
Native exit — 0. Текущий пользовательский матч не перезапускался.

Stage35Static, Stage35RecoveryPolicy, InfantryRecruitmentStatic, Stage3Static —
PASS до/после; AILoadoutStatic — PASS после. AICommanderModeStatic сохраняет
две прежние AI_COMMANDER_UI_STATE. Workbench Everon, EveronRHS и test stage —
PASS / 0. Полные команды, изменённые файлы, evidence и NOT RUN:
[INFANTRY_SPAWN_PLACEMENT.md](INFANTRY_SPAWN_PLACEMENT.md).

## Восстановление Failed move — 2026-09-19

Изменены infantry route recovery в `AICF_OrderPlanner`, waypoint identity и
одноразовый budget в `AICF_GroupSlot`, orchestration stuck watchdog и новый
`AICF_IsolatedNavmeshRecovery`. Причина, границы и fixture описаны в
[NAVIGATION_RECOVERY.md](NAVIGATION_RECOVERY.md).

Stage3Static, Stage35Static, Stage35RecoveryPolicy, Stage4Static,
MapPointOrdersStatic — **PASS / 0** до/после. AICommanderModeStatic сохраняет
**FAIL / 1**, две прежние `AI_COMMANDER_UI_STATE`. Workbench Validate/Compile
финальных Arland, Everon, ArlandRHS, EveronRHS — **PASS / 0**.

Изолированный RHS navigation probe — **PASS по целевым assertions**:
US прошёл 92.1914 м, USSR — 406.209 м с сохранением group/generation/intent;
player fence и single-use budget — PASS. Полный остановленный server log
содержит одну воспроизведённую исходную `Failed move` до восстановления,
повторов после восстановления нет. Общий runtime не объявляется чистым:
native/RHS resource/world/RPL diagnostics сохранены. Client/JIP, visual,
долгий soak и обновление живого пользовательского матча — **NOT RUN**.
Evidence: `.codex-runtime/navigation-fix-20260919/`, включая полные logs,
native manifest, версии, source hashes, baseline/after и exact compile args.

## Север Эверона без RHS — 2026-09-19

Добавлены `AIConflictEveron/Missions/AICF_Conflict_Everon_North.conf` и `.meta`
с GUID `A1CF190919300000`, наследованием stock AICF Everon, двумя HQ и шестью
точками захвата. Андре исключён. Стандартные фракции `US`/`USSR`, content profile
`STOCK`; gameplay scripts и GUID проектов не менялись.

Изменены `tools/Start-AICFRuntime.ps1` (`-Variant EveronNorth`),
`Test-RuntimeLauncherStatic.ps1`, `Test-EveronNorthStatic.ps1`,
`Test-LocalizationStatic.ps1`, каталог `AIConflictCore/Language/AICF_Localization.st`
и обе runtime таблицы (389 записей), README, ARCHITECTURE, DEVELOPMENT,
EVERON_NORTH и этот отчёт. В README восстановлены буквы, ошибочно заменённые
при предыдущем редактировании; содержательные изменения сохранены.
В EVERON_NORTH исправлено ошибочное описание победы: используется ticket
exhaustion, а stock territorial countdown отключён.

Команды `powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Test-<Name>Static.ps1`:
`EveronNorth`, `Localization`, `ScenarioHeaders`, `RuntimeLauncher` — **PASS / 0**
до и после. Launcher dry-runs проверяют server/client с отсутствующим каталогом
RHS и точными тремя stock addon GUID. `AICommanderMode` сохраняет **FAIL / 1**,
два прежних сообщения `AI_COMMANDER_UI_STATE`. `Build-AICFLocalization.ps1`
и `git diff --check` — **PASS / 0**.

Терминальный `ArmaReforgerWorkbenchSteamDiag.exe -noThrow -wbsilent
-gproj <AIConflictEveron/addon.gproj> -addons <Core,Arland,Everon>
... -wbModule=ScriptEditor -run -validate` для production и отдельного stage
с существующей `AICF_EveronNorthProbe.c` — **PASS / 0**,
`Script validation successful.`.

`Start-AICFRuntime.ps1 -Role Server -Variant EveronNorth -RepositoryRoot <stage>
-AdditionalArguments @('-aicfNorthProbe','1','-addr','127.0.0.1:22199')`:
**10/10 функциональных checks**, `profile=STOCK`, 8 initialized bases,
2 HQ, 6 control points, `missing=0` для всех пар радиографа,
`ROSTER_READY` с 10 группами на сторону. RHS отсутствует в CLI addon graph.
Обнаружены две stock `SCR_AIProcessFailedMovementResult.NodeErrorOnce`
VM Exception `Failed move` на маршруте от авиабазы к Maiden's Bay;
проверка топологии не подтверждает исправность всех путей AI.
Тестовый сервер штатно завершился с native exit **0**. Полный остановленный
лог проверен: кроме этих двух VM Exception, сохранены stock resource/world
сообщения, две resupply ошибки при shutdown и resource leak; ENGINE fatal
не обнаружен. Runtime не объявляется error-free.

Evidence: `.codex-runtime/everon-north-stock-20260919/`, `before-*`, `after-*`,
`wb-stage*`, `wb-production*`, `server-launch.txt` с manifest, полные
`server-logs/` и индекс `server-errors.txt`.
**NOT RUN:** подключённый клиент/JIP, ручная проверка плитки и карты,
длительный бой/победа и Workshop packaging. Текущий пользовательский RHS
матч не перезапускался и не переключался на стандартный сценарий.

## Север Эверона без обзорного пункта Андре — 2026-09-19

Из whitelist удалён `SmallBaseAndresBeacon`: теперь 8 активных баз,
2 HQ и 6 control points. Изменены
`AIConflictEveronRHS/Missions/AICF_RHS_Conflict_Everon_North.conf`,
`AIConflictCore/Language/AICF_Localization.st` и обе runtime таблицы,
`tools/Test-EveronNorthStatic.ps1`, `tools/fixtures/AICF_EveronNorthProbe.c`,
README, ARCHITECTURE, EVERON_NORTH и этот отчёт. Предыдущие изменения сохранены.
Исторический прогон ниже с девятью базами относится к первоначальной версии.

Команды `powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Test-<Name>Static.ps1`:
`EveronNorth`, `Localization`, `ScenarioHeaders` — **PASS / 0** до и после.
`AICommanderMode` — сохранённый **FAIL / 1**: два сообщения
`AI_COMMANDER_UI_STATE` (waiting label и SYSTEM_HOLD marker).
`tools/Build-AICFLocalization.ps1` — **PASS / 0**, 386 записей.
`git diff --check` — **PASS / 0**.

Терминальный `ArmaReforgerWorkbenchSteamDiag.exe -noThrow -wbsilent
-gproj <EveronRHS/addon.gproj> ... -wbModule=ScriptEditor -run -validate`:
production — **PASS / 0**, stage с обновлённой fixture —
`Script validation successful.` (exit code stage отдельно не получен).
Production `.c` в этой правке не менялись.

`Start-AICFRuntime.ps1 -Role Server -Variant EveronNorthRHS
-RepositoryRoot <stage> -AdditionalArguments @('-aicfNorthProbe','1',
'-addr','127.0.0.1:22199')`: **10/10 функциональных checks**.
8 initialized bases, 2 нужных HQ, 6 control points, Андре и южные базы
неактивны; граф содержит 8 nodes / 32 directed edges, `missing=0` для всех пар.
Тестовый сервер штатно завершился с native exit **0**. Полный остановленный
лог сохранён и проверен: четыре прежние RHS faction-init SCRIPT ошибки,
две stock resupply ошибки при shutdown, resource/world/RPL/pathfinding
сообщения. VM/null и ENGINE fatal не обнаружены; runtime не объявляется
error-free. Эти ошибки не отменяют результат проверки конфигурации баз.

Evidence: `.codex-runtime/north-without-andre-20260919/`: `before-*`,
`after-*`, `wb-stage*`, `wb-production*`, `server-launch.txt` с manifest,
полные `server-logs/` и индекс `server-errors.txt`.
**NOT RUN:** новый client/JIP, ручной осмотр карты, длительный бой и победа.
Изменение применяется при новом запуске кампании; текущий матч не перезапускался.

## Метка атаки во время пополнения — 2026-09-19

В пользовательском северном Everon RHS отряд US A2 получил
`INFANTRY_RECRUITMENT_STARTED base=10` в 16:55:13 и 16:59:13: союзный
Saint-Philippe был временной целью казарм. Сводная метка `ATK A2` ошибочно
следовала роли ATTACK без проверки recruitment и владельца базы.

`UI/AICF_GroupMapMarkers.c` теперь исключает текущий `IsRecruitingInfantry()`
и союзные базы из attack objectives; задача отображается как «Пополнение
состава в казарме». Изменены также `Language/AICF_Localization.st`, обе runtime
таблицы (386 записей), `tools/Test-GroupMapMarkersStatic.ps1`, добавлена
`tools/fixtures/AICF_RecruitmentMapProbe.c`, обновлена ARCHITECTURE.
Gameplay приказы, waypoint lifecycle и recruitment не изменены.

Команды `powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Test-<Name>Static.ps1`:
`GroupMapMarkers`, `Localization`, `InfantryRecruitment` — **PASS / 0**
до/после; `AICommanderMode` — тот же **FAIL / 1, AI_COMMANDER_UI_STATE**.
Терминальный `ArmaReforgerWorkbenchSteamDiag.exe -noThrow -wbsilent
-gproj <EveronRHS/addon.gproj> ... -wbModule=ScriptEditor -run -validate`
для production и отдельного stage с fixture — **PASS / 0**,
`Script validation successful.`. `git diff --check` — **PASS / 0**.

Runtime: `Start-AICFRuntime.ps1 -Role Server -Variant EveronNorthRHS|Stock
-RepositoryRoot <stage> -AdditionalArguments @('-aicfRecruitmentMapProbe','1',
'-addr','127.0.0.1:22199|22200')`, в отдельных terminal sessions —
**9/9 cases** для каждой карты, оба native exit **0**. Synthetic slot использует
реальные stock/RHS HQ и faction identities. Проверены enemy/friendly filters,
скрытие recruitment, восстановление атаки, defender/null guards и RU/EN.
Производственный roster и ownership fixture не меняет.

Полные остановленные runtime logs проверены. RHS run содержит stock
`SCR_AIProcessFailedMovementResult.NodeErrorOnce: Failed move` (VM Exception),
четыре faction-init SCRIPT ошибки и две resupply shutdown ошибки; поэтому
полный runtime не объявляется error-free. Ошибка движения произошла в native
behavior tree до вызова fixture и не затрагивает проверяемые методы отображения.
Stock run сохраняет shutdown resupply ошибки. Успешные 9/9 — только verdict
функциональных cases меток.

Evidence: `.codex-runtime/recruitment-map-20260919/`, `before-*`, `after-*`,
`wb-stage*`, `wb-production*`, `runtime-rhs.txt`, `runtime-stock.txt` с manifest,
полные `rhs-logs/`, `stock-logs/`, индексы `*-errors.txt`,
`reported-live-server.log` (snapshot пользовательского матча).
Тестовые процессы закрылись; текущий пользовательский матч не перезапущен.
**NOT RUN:** новый connected client/JIP и ручная визуальная проверка меток.

## Север Эверона RHS — 2026-09-19

Добавлен inherited `AIConflictEveronRHS/Missions/AICF_RHS_Conflict_Everon_North.conf`
с `.meta`: две HQ (авиабаза/госпиталь), семь control points, whitelist девяти
баз, radio range 2000 м и запрет establishing bases. Дополнены RU/EN каталог
и runtime tables, `Start-AICFRuntime.ps1`, static audits launcher/localization,
новые `Test-EveronNorthStatic.ps1` и `fixtures/AICF_EveronNorthProbe.c`.
Обновлены README, ARCHITECTURE, DEVELOPMENT и [EVERON_NORTH.md](EVERON_NORTH.md).
Предшествующая правка скорости захвата сохранена.

Команды `powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Test-<Name>Static.ps1`:

| Name | До | После |
|---|---|---|
| `ScenarioHeaders`, `RuntimeLauncher`, `RHSIntegration`, `Localization` | PASS / 0 | PASS / 0 |
| `AICommanderMode` | FAIL / 1: `AI_COMMANDER_UI_STATE` | тот же единственный FAIL / 1 |
| `EveronNorth` | новая проверка | PASS / 0 |

Отдельные отрицательные inputs `wrong-hq`, `whitelist-disabled`,
`disconnected-radio` — **PASS**, каждый отклонён с exit 1 и нужным rule.
`git diff --check` — **PASS / 0**. Localization содержит 385 записей.

Терминальный `ArmaReforgerWorkbenchSteamDiag.exe -noThrow -wbsilent
-gproj <EveronRHS/addon.gproj> ... -wbModule=ScriptEditor -run -validate`
для production и отдельного stage с fixture — **PASS / 0**, оба вывели
`Script validation successful.`; SCRIPT E/F, ENGINE F и VM/null — 0.

Runtime через `Start-AICFRuntime.ps1 -Role Server -Variant EveronNorthRHS
-RepositoryRoot <stage> -AdditionalArguments @('-aicfNorthProbe','1','-addr','127.0.0.1:22199')`
и отдельный `-Role Client -Variant EveronNorthRHS -RepositoryRoot <stage>
-ServerProfileRoot <profile> -ServerPort 22199 -ClientAddress '127.0.0.1:22199'
-AdditionalArguments @('-aicfNorthProbe','1','-language','ru_ru')`:

- сервер **10/10**, клиент **8/8**, native exit обоих **0**;
- 9 initialized bases, 2 HQ в нужных местах, 7 control points,
  остальные базы неактивны на обеих сторонах соединения;
- оба HQ назначены разным фракциям, `ROSTER_READY`, командиры готовы,
  выданы начальные attack orders;
- радиограф 9 nodes, `missing=0` для всех пар; client ready gate подтвердил
  точный CLI, живой PID и готовность roster;
- начальный диагностический запуск через raw world показал
  `GetMissionHeader() == null`. Передача GUID header прямо в `-server`
  подтвердила native whitelist; новый launcher использует этот путь.

Проверены полные остановленные logs. Runtime **не является чистым error-free
baseline**: сервер содержит четыре знакомых `SCR_Faction` init ошибки US/USSR
(есть и в предварительном полном Everon), две resupply/catalog ошибки при
закрытии, RHS/stock resource/world/arsenal RPC сообщения и pathfinding tile
ошибки; клиент — resource/world/GUI сообщения и shutdown resource leaks.
VM/null и fatal не обнаружены. Эти сообщения сохранены в evidence, а успешная
fixture подтверждает только перечисленные свойства сценария.

Evidence: `.codex-runtime/everon-north-20260919/`: `before-*`, `after-*`,
`negative-*/result.txt`, `wb-probe*`, `wb-production*`, `north-server.txt`,
`north-client.txt` (включая `AICF_RUNTIME_MANIFEST_JSON`), полные
`server-logs/`, `client-logs/` и индексы `*-errors.txt`.
Profiles: `Server-EveronNorthRHS-20260919-163050-080`,
`Client-EveronNorthRHS-20260919-163133-230`. Тестовые процессы закрылись;
пользовательские server/client полного Everon не перезапускались.

**NOT RUN:** ручная проверка плитки/карты и читаемости меток, packaged Workshop
build, длительный бой, фактический захват всех семи точек/победа,
баланс направлений и полноценный прогон транспортной навигации.

## Скорость захвата баз ×3 — 2026-09-19

`AIConflictCore/Scripts/Game/AIConflict/Integration/AICF_CaptureSpeedPolicy.c`
делит четыре штатных временных параметра захвата на 3 при server-side
`OnPostInit`. Ускорение одинаково для AI и игроков, stock и RHS.
Формула сверена с `SCR_CampaignSeizingComponent.RefreshSeizingTimer`
закреплённого Script Diff `1.8.0.13`: масштабируются max/min и обе надбавки,
поэтому множитель служб/радиосвязей сохраняется, а длительность уменьшается втрое.
Обновлены также defaults в `README.md` и контракт в `ARCHITECTURE.md`.

Команды `powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Test-RHSIntegrationStatic.ps1`
и `tools/Test-ScenarioHeadersStatic.ps1` — **PASS / 0** до и после.
`tools/Test-AICommanderModeStatic.ps1` — прежний **FAIL / 1**, только
`AI_COMMANDER_UI_STATE`. Терминальный Workbench по команде
`docs/DEVELOPMENT.md` с `-wbModule=ScriptEditor -run -validate` — **PASS / 0**
для Arland, Everon, ArlandRHS и EveronRHS; SCRIPT E/F, ENGINE F и VM/null — 0.
`git diff --check` — **PASS / 0**.
Evidence: `.codex-runtime/capture-speed-20260919/`, baseline/after audits,
`wb-*-args.json`, `wb-*-output.txt`, полные остановленные `wb-*/console.log`.

**NOT RUN:** runtime-замер времени захвата, contested/reinforcement/JIP проверка
и визуальная проверка таймера. Текущая пользовательская сессия RHS Everon
не перезапускалась; изменение вступит в силу со следующим запуском миссии.

## Язык клиента — 2026-09-19

Локализованы сценарии и весь AICF UI, включая server-produced метки и ответы.
Профильные static gates проходят; прежний `AI_COMMANDER_UI_STATE` сохранён.
Stock/Everon/ArlandRHS/EveronRHS Workbench — PASS / 0.
Целевая runtime fixture: по 15 checks, 0 failures на dedicated server и
поздно подключившемся клиенте; оба завершились через `RequestClose`.
Общий clean-runtime PASS не заявлен из-за stock resource/shutdown diagnostics.
Команды, полные логи, изменённые файлы и `NOT RUN`:
[LOCALIZATION.md](LOCALIZATION.md).

## Плотный поиск и обход препятствий — 2026-09-19

Поиск проверяет 256 различных центров, до 16 рабочих точек, ограниченный
navmesh A* с шагами 2/4 м и общий лимит пути одного кандидата. AI builder
получает проверенный старт с прежними identity/readiness guards.
Вторая ручная площадка Large Barracks СССР проходит все geometry/path guards.
Окончательный replay: native exit 0, SCRIPT/VM errors 0.

Итоговая статика: ConstructionStatic, ConstructionContracts (16 log +
positive/9 negative static), BaseBuildersStatic, Stage4Static — **PASS / 0**;
прежний AICommanderModeStatic `AI_COMMANDER_UI_STATE` — **FAIL / 1**.
Все четыре production Workbench graphs — **PASS / 0**. Native helpers — 30/30.

Окончательная 15-минутная Stock matrix: native exit 0, четыре оплаченных
placement/completion — SMALL_BARRACKS и LIGHT_DEPOT обеих сторон; tool use и
online services подтверждены. ARMORY уже покрыт stock службами. LARGE/HEAVY
не достроены, Heavy search прерван завершением теста. Runtime audits —
**FAIL / 1** из-за двух прежних shutdown SCRIPT ошибок; полный matrix audit
также отмечает недостающие типы. Max window 96 queries, max полный tick 290 мс.
Это не сравнение частоты строительства в одинаковых матчах.

Визуальные критерии, construction client/JIP test и Everon/RHS runtime —
**NOT RUN**. Команды, файлы, полные verdict и ограничения:
[CONSTRUCTION_IMPROVED_SEARCH_20260919.md](CONSTRUCTION_IMPROVED_SEARCH_20260919.md).
Evidence: `.codex-runtime/construction-improved-20260919/`.

## Повтор двух ручных площадок — 2026-09-19

На двух местах больших казарм СССР повторены production geometry/path guards
с ручными и горизонтальными transforms: **4/4 cases, native exit 0**.
Основное расхождение — ограниченный direct/two-segment navmesh тест;
на первой площадке дополнительно rock/slope, на второй горизонтальной —
small rock после расширения physics bounds. Ближайшие из 256 кандидатов
находятся в 34.77/8.39 м. Gameplay исправление на этом шаге не применялось.
Static до/после: четыре PASS, прежний `AI_COMMANDER_UI_STATE` FAIL;
Workbench fixture PASS. Полный остановленный runtime содержит E/F и не
объявляется чистым PASS. Подробности, команды и ограничения:
[CONSTRUCTION_MANUAL_REPLAY_20260919.md](CONSTRUCTION_MANUAL_REPLAY_20260919.md).

## Остальные здания на Arland — 2026-09-19

Полный 25-минутный прогон с припасами и последовательным запросом всех типов
завершился штатно, native exit 0. СССР построил малые/большие казармы и Light
Depot; Heavy Depot обеих сторон не размещён, Armory не получил нового order.
Полная matrix audit — **FAIL / 1**: неполное покрытие типов и SCRIPT errors.
Contracts — **PASS / 0**, 16 log inputs; baseline `AI_COMMANDER_UI_STATE`
сохранён. Пользователь поставил две Large Barracks на северном HQ СССР;
обе достроены. Результат повторения поисковых проверок приведён выше.
Команды, изменённые файлы и полные verdict:
[CONSTRUCTION_MATRIX_20260919.md](CONSTRUCTION_MATRIX_20260919.md).

## Light Factory: местный уклон — 2026-09-19

В пользовательской сессии Arland СССР не находил Light Depot за 201/256
кандидатов, но поставленный пользователем проект был достроен. Исправлен
излишне строгий общий перепад 0.8 м: terrain grid проверяет соседние высоты
по обеим осям (0.8 м на 3 м фактического расстояния). Physics bounds
commit/completion расширяются по сохранённому диапазону высот.

ConstructionStatic, ConstructionContracts, BaseBuildersStatic и Stage4Static:
**PASS / 0** до/после; AICommanderModeStatic сохраняет
**FAIL / 1, AI_COMMANDER_UI_STATE**. Contracts: 13 log inputs и шесть negative
static inputs; native helper checks — **30/30**. Все четыре production
Workbench graphs без fixture — **PASS / 0**.

Focused Stock run: четыре оплаченных placement и completion, в том числе
Light Depot обеих сторон и казарма СССР с `terrain_delta=1.39552`.
Native exit 0; полный log audit **FAIL / 1**: известные stock shutdown SCRIPT
errors и `CONSTRUCTION_DECISION_TOO_EARLY` (59998 мс по timestamps).
Аудитор не ослаблялся. Визуальная проверка, новый client/JIP, runtime Everon/RHS
и полная типовая matrix — **NOT RUN**. Последующий обычный порядок строительства,
команды, файлы, полные logs и границы выводов:
[LIGHT_FACTORY_TERRAIN_20260919.md](LIGHT_FACTORY_TERRAIN_20260919.md).
В обычном порядке достроены две казармы и Light Depot US; Light Depot USSR
не завершил поиск до остановки. Native exit 0, log audit **FAIL / 1**, только
две известные shutdown SCRIPT ошибки. Распределение HQ между фракциями изменилось.
Evidence: `.codex-runtime/light-factory-20260919/`.

## Поиск площадок строительства — 2026-09-19

Исправлены распределение квот между базами, повтор поиска после занятого
перед commit участка, центр/повороты кандидатов и oriented bounds. Крупные
compositions больше не отсекаются лимитом 256 collision volumes: входы
объединяются порциями до 24 OBB без потери геометрии. Cold metadata и поиск
получают отдельные deadline по 120 секунд; attempts — 256 при прежних квотах
4 candidates/tick, 96 queries/window и одной попытке placement/tick.

`Test-ConstructionStatic.ps1`, `Test-ConstructionContracts.ps1`,
`Test-BaseBuildersStatic.ps1`, `Test-Stage4Static.ps1` — **PASS / 0**.
Contracts: 13 log inputs, positive и четыре negative static inputs;
native geometry helpers: **24/24**. Сохранён baseline
`Test-AICommanderModeStatic.ps1`: **FAIL / 1, AI_COMMANDER_UI_STATE**.
`git diff --check` — **PASS / 0**. После удаления временной fixture все четыре
production Workbench graphs — **PASS / 0**.

Stock runtime подтвердил оплату и достройку больших и малых казарм USSR,
инструмент и online services: `Test-ConstructionLog -RequireCompletion`
**PASS / 0**, native exit 0. Этот completion-run предшествует последнему
разделению deadline. Финальный focused run на окончательной версии planner
подтвердил полный срок поиска после 93–108 секунд metadata: 190/230 кандидатов,
без размещений на занятых HQ, корректные `NO_SAFE_SITE`, max window 60 queries;
`Test-ConstructionLog` без `-RequireCompletion` — **PASS / 0**, native exit 0.
Это не сравнение частоты строительства в одинаковых игровых условиях.

Полные logs сохраняют resource/world diagnostics: 15 error lines каждого
последующего runtime, SCRIPT/VM/fatal — 0. Baseline содержит ещё две
shutdown SCRIPT ошибки resupply. **NOT RUN:** runtime Everon/RHS, client/JIP,
ручная проверка входов/проезда/анимации, полная типовая matrix и длительный soak.
Команды, profiles, изменённые файлы и границы evidence:
[CONSTRUCTION_SEARCH_20260919.md](CONSTRUCTION_SEARCH_20260919.md).
Локальные артефакты: `.codex-runtime/construction-search-20260918/`.

## Совместимый запасной магазин — 2026-09-18

Для выбранного оружия и внутри его обвесов доступна кнопка «+ Магазин».
Она добавляет ровно один запасной магазин в конечный карман, включая вложенные
подсумки RHS; заряженный магазин сохраняется. Приоритет: текущий, штатный,
остальные разрешённые faction catalog. Используются native magazine well,
capacity и канонический адрес storage. Подбор read-only, операция проходит
обычный recipe/Rebuild/rollback и серверную валидацию при сохранении. Каталог
магазинов не сканируется из кадрового Refresh.

Evidence: `.codex-runtime/loadout-magazine-20260918-193530/`.
Команды `tools/Test-AILoadoutStatic.ps1` и `tools/Test-Stage4Static.ps1`
до/после — **PASS / 0**, loadout negatives **122 → 132**.
`tools/Test-AICommanderModeStatic.ps1` сохраняет **FAIL / 1,
AI_COMMANDER_UI_STATE**. `git diff --check` — **PASS / 0**.

Терминальный `ArmaReforgerWorkbenchSteamDiag.exe -noThrow -wbsilent ...
-wbModule=ScriptEditor -run -validate` — **PASS / 0** на всех четырёх production
graphs (Arland, Everon и RHS), а также на отдельном RHS test stage. Полные
аргументы и логи находятся в `wb-final-*` и `wb-probe-final-*`.
Все **140** production scripts совпали по SHA-256 с проверенным runtime stage.

`tools/fixtures/AICF_LoadoutMagazineProbe.c` вместе с clothing fixture копируется
только в отдельный stage. Запуск обоих peers через `tools/Start-AICFRuntime.ps1
-Role Server|Client -Variant Everon|EveronRHS -RepositoryRoot <stage>
-AdditionalArguments @('-aicfMagazineProbe','1')` на свежих profiles.
Manifests содержат точные CLI; клиент подтверждает server process и ROSTER_READY.

Финальные результаты: **122/122** server и **122/122** client в vanilla;
**130/130** server и **130/130** client в RHS. Проверены обе фракции, автоматы
и пистолеты, приоритет заряженного магазина, штатный магазин для разряженного
оружия, +1 в карман, неизменность заряженного, Undo, snapshot restore/signature,
серверный Validate, несовместимый magazine well, изоляция миров, отсутствие
карманов и native заполнение кармана. RHS дополнительно проверяет добавление
и сохранение магазина во вложенном подсумке 6Sh117 (SL Kit).
В измеренных вызовах подбор занимал **0–14 мс**; это не FPS/soak gate.

Оба клиента завершились сами с **exit 0**; серверы остановлены по точному
profile после результатов, **exit -1**. Полные stopped logs: `Everon-final/`
и `EveronRHS-final/`. VM/fatal = **0**; vanilla SCRIPT errors = **0**;
RHS server сохраняет **4** штатных `SCR_Faction` ошибки ключей US/USSR,
RHS client SCRIPT errors = **0**. Полный runtime не объявляется чистым PASS:
native error lines server/client — vanilla **87/195**, RHS **251/348**;
есть resource/world, preview RPL, overlapping PP effects и shutdown diagnostics.
Количество выше предыдущего probe из-за повторных Build/restore, классы
диагностик сохранены в `*-error-categories.json`.

Неудачные начальные прогоны сохранены: неверная проверка мира manager entity
исправлена сравнением мира candidate с оружием; тест пистолетов исключает
одноразовые сигнальные средства. Первый запуск клиента остановился на проверке
launcher, попытка server через PowerShell 5 — на native stderr; итоговые peers
успешно запущены через PowerShell 7 без изменения launcher.

**NOT RUN:** ручной клик и видимость новой кнопки на разных разрешениях,
полная матрица сторонних магазинов/оружия, JIP, respawn с этим конкретным
действием, FPS и длительный soak. Автоматические проверки не заменяют визуальную
проверку пользователем; результат не объявляется ACCEPTED.

## Основной список, ПКМ и подсумки 6Sh117 — 2026-09-17

Убраны выпадающий список storages и кнопки «Содержимое»/«Снять». Одежда и оружие
доступны в одном списке; двойной клик открывает предмет, ПКМ удаляет выбранную
вещь из черновика, включая контейнер с содержимым; Undo возвращает её.
`AICF_LoadoutNavigation` объединяет storages одного предмета на общей странице,
сохраняя отдельные адреса операций. Переход использует identity установленного
предмета; native `GetItem` заменяет поиск произвольного InventoryItemComponent.
Пустые служебные страницы не открываются. Подсумки получают native имя либо
понятный fallback без prefab filename и отсутствующего translation key.

Evidence: `.codex-runtime/loadout-navigation-20260917-223544/`.
`Test-AILoadoutStatic.ps1`: до **PASS / 0, 115 negatives**, после **PASS / 0, 122**.
Два прежних контракта удаления двойным кликом заменены по новому запросу
пользователя на отдельные open/remove handlers; добавлены native addressing,
ПКМ, lifecycle и отсутствие лишних controls. `Test-Stage4Static.ps1` до/после
**PASS / 0**; `Test-AICommanderModeStatic.ps1` до/после сохраняет прежний
**FAIL / 1, AI_COMMANDER_UI_STATE**.

Терминальный Workbench Validate/Compile четырёх production graphs и RHS stage
с fixtures — **PASS / 0**, SCRIPT E/F/VM = 0. Финальные 139 production scripts
совпали по SHA-256 с runtime stage (`source-hashes.json`). Команды, args.json,
exit codes и полные логи сохранены в `wb-final-*` и `wb-probe-final-*`.

`AICF_LoadoutNavigationProbe.c` запускается только в test stage вместе с clothing
fixture по `-aicfNavigationProbe 1`. RHS финал: **69/69 server и 69/69 client**;
stock: **8/8 server**, прежняя полная клиентская fixture **109/109** с SAVE_ACK,
проверкой старой ревизии, чужой фракции, позиции, оружия, preview и сохранения.
RHS проверяет точный 6Sh117 (SL Kit), 17 креплений, 10 целей наполнения карманов,
переходы и возврат, все native operation addresses, отсутствие технических имён,
вместимость, удаление подсумка и Undo. Первый прогон выявил две пустые служебные
страницы; они скрыты из навигации, финальные проверки проходят. Его логи сохранены.

Оба финальных клиента завершились сами, **exit 0**; тестовые серверы остановлены
после результатов, **exit -1**. Manifests содержат точные CLI и ROSTER_READY;
полные stopped logs сохранены в `EveronRHS-final` и `Everon-final`.
Полный runtime не чистый PASS: native error lines stock **51/117**, RHS **211/239**
(server/client), включая resource/world/RPL diagnostics. В RHS server остаются
4 прежних `SCR_Faction` init errors. В stock peers и RHS client SCRIPT E/F/VM = 0.
Ручная доставка двойного клика/ПКМ, внешний вид, steady FPS, JIP inventory и
packaged Workshop validation — **NOT RUN**. GUI automation не применялась.

## Крепления шлемов RHS и цельная одежда — 2026-09-17

Фиксированные `LoadoutSlotInfo` внутри одежды показываются как отдельные места,
включая пустые. Каталог проверяет native совместимость конкретного места;
балаклаву больше не пытаются вставить как обычное содержимое кармана.
КЛМК выбирается из «Куртки» и «Брюк»: снимается конфликтующая одежда,
сохраняется единственный предмет, поддержаны снятие/содержимое через оба места.
Перед восстановлением полного snapshot исходная одежда снимается до вставки.

Evidence: `.codex-runtime/loadout-clothing-20260917-210803/RESULT.md`.
`Test-AILoadoutStatic.ps1` **PASS / 0**, negatives 107 → 115;
`Test-Stage4Static.ps1` **PASS / 0**. Прежний
`Test-AICommanderModeStatic.ps1` **FAIL / 1, AI_COMMANDER_UI_STATE** сохранён.
Production Workbench Arland/Everon/ArlandRHS/EveronRHS (`wb-release-*`) и
RHS stage с fixtures (`wb-probe-release-*`) — **PASS / 0**, SCRIPT E/F/VM = 0.

Новая `AICF_LoadoutClothingProbe.c` включается только в test stage флагом
`-aicfClothingProbe 1`. Stock КЛМК: **24/24 server, 22/22 client**.
Проверены выбор через обе области, замена костюма брюками, снятие, Undo,
snapshot roundtrip, Validate и реальная выдача ИИ. Финальная прежняя
клиентская fixture — **109/109**, включая SAVE_ACK и отказы stale/faction/member.
Первый прогон выявил смешивание бронежилетов и разгрузок; фильтр исправлен,
финальный прогон подтвердил прежнее разделение категорий.

RHS: **34/34 server, 32/32 client**, обе фракции, MICH2000/Spartan3 и балаклавы.
Проверены native slot filter из полного каталога, отсутствие изменений от
фильтрации, установка, сохранение, восстановление, выдача ИИ, удаление и Undo.
Первый холодный поиск в клиенте: 677/772 ms из 603/805 предметов; после
предварительной категории HEADWEAR — 278/175 ms из 132/93 предметов,
совместимые 9/8 балаклав сохранены. Это замер операции, не FPS gate.

Финальные test clients завершились с **exit 0**, серверы остановлены после
результатов (**exit -1**, не graceful shutdown test). Полные остановленные
логи и manifests сохранены. Stock final SCRIPT E/F/VM = 0; RHS server сохраняет
4 известных `SCR_Faction` init errors, client SCRIPT E/F/VM = 0. Native error
lines: stock 64/117 и RHS 225/267 (server/client); полный runtime не объявляется
чистым PASS. Внешний вид куклы, подписи и доставка мыши в форме, steady FPS,
JIP inventory — **NOT RUN**, необходима ручная проверка. Обычная RHS-сессия
после тестов запущена отдельно, без fixtures и таймера, с прежними шаблонами.

## Бесплатные комплекты ботов — 2026-09-17

Доплата supplies за шаблон удалена из recruitment (включая предварительный
выбор казармы), replacement reservation и supply pacing. Сохраняются базовые
цены роли/восстановления, ticket/supply транзакции и rollback. Валидация
создаёт binding с нулевой стоимостью; лимит по каталожной цене убран,
вместимость и совместимость остаются обязательными. В редакторе убраны цены
карточек и расчёт доплаты; показано «Экипировка — бесплатно».

Evidence: `.codex-runtime/loadout-free-20260917-202202/RESULT.md`.
`Test-AILoadoutStatic.ps1` **PASS / 0**, negatives 103 → 107;
`Test-Stage4Static.ps1` и `Test-InfantryRecruitmentStatic.ps1` **PASS / 0**.
Сохранён baseline `Test-AICommanderModeStatic.ps1` **FAIL / 1,
AI_COMMANDER_UI_STATE**. Прежний контракт `LOADOUT_SURCHARGE` заменён на
бесплатный комплект по прямому запросу пользователя. Дополнительные guards
защищают от возврата доплаты в выбор казармы, найм и replacement.

Финальный production Workbench Arland/Everon/ArlandRHS/EveronRHS и Everon stage
с обеими fixtures — **PASS / 0**, SCRIPT E/F/VM/null/Pointer warnings = 0.

Runtime fixtures обновлены: сервер проверяет нулевой binding обеих фракций
и deployment, клиент — нулевой owner snapshot до и после сохранения.
Новые runtime и визуальный прогон — **NOT RUN**: текущая игровая сессия
оставлена на прежнем stage до решения пользователя о перезапуске. Результат
Workbench и точные команды сохранены в evidence; runtime не заменяется
статическим аудитом или компиляцией.

## Упрощение редактора экипировки — 2026-09-17

Убраны три дублирующие кнопки: одежда, оружие и добавление в контейнер.
Вместо них используются существующий список мест и строка добавления.
Содержимое, удаление, возврат, количество и очистка фильтров видны по контексту;
кнопки прокрутки — только при переполнении списка. Области куклы и экипировки
увеличены. Сохранение выделено цветом и требует изменений, корректного имени и
актуальной server revision; проверка повторяется перед RPC. Сериализация для
сравнения обновляется после Rebuild, не на каждом UI refresh.

Evidence: `.codex-runtime/loadout-usability-20260917-195601/RESULT.md`.
`Test-AILoadoutStatic.ps1` **PASS / 0**, negatives 100 → 103;
`Test-Stage4Static.ps1` **PASS / 0**. Сохранён baseline
`Test-AICommanderModeStatic.ps1` **FAIL / 1, AI_COMMANDER_UI_STATE**.
Production Workbench Arland/Everon/ArlandRHS/EveronRHS и отдельная Everon fixture —
**PASS / 0**, SCRIPT E/F/VM/null/Pointer warnings при компиляции = 0.

Полная клиентская fixture (без preview-only режима) — **107/107,
finished=1 failures=0**, client exit 0. Проверены owner snapshot, память
черновиков, native inventory/attachment/preview операции, `SAVE_ACK`,
`STALE_RPC_REJECTED`, `WRONG_FACTION_REJECTED`, `OUT_OF_RANGE_REJECTED`.
Server остановлен после завершения fixture, exit -1 (`Stop-Process`),
это не graceful shutdown test. Полные остановленные логи и manifests — `final/`.
SCRIPT E/F/VM/null/Pointer warnings = 0; native error lines — 49 server / 128
client (stock resources/Hierarchy/pathfinding/network/GUI, preview RPL и
shutdown resources). Полный runtime не объявляется чистым PASS.

Доставка мыши/клавиатуры в изменённой форме, видимость и читаемость кнопок,
визуальные цвета, FPS и RHS runtime — **NOT RUN**. Fixture не открывает и
не управляет GUI; проверяет связанные операции, а не внешний вид нового UI.

## Выбор отряда в редакторе экипировки — 2026-09-16

Отряд выбирается непосредственно в верхнем списке редактора: стабильный номер
и текущее обозначение, например «Отряд 1 — A0». Список берётся из summary своей
фракции; при переключении заново запрашивается комплект нужной позиции и
обновляется число доступных позиций. Несохранённые черновики сохраняются в форме
раздельно по numeric slot/member и восстанавливаются только при той же server
revision и совместимой identity рецепта. Закрытие формы очищает эту память.
Сохранение по-прежнему назначает комплект одной позиции выбранного отряда.

Evidence: `.codex-runtime/loadout-respawn-20260916-223612/RESULT.md`.
`Test-AILoadoutStatic.ps1` **PASS / 0**, negatives 91 → 100;
`Test-Stage4Static.ps1` **PASS / 0**. Сохранён baseline
`Test-AICommanderModeStatic.ps1` **FAIL / 1, AI_COMMANDER_UI_STATE**.
Production Workbench Arland/Everon/ArlandRHS/EveronRHS и финальная Everon fixture —
**PASS / 0**, SCRIPT E/F/VM/null/Pointer warnings = 0 при компиляции.

Терминальная server-only fixture проверила read model всех 20 отрядов US/USSR,
границы выбора и отсутствие campaign, восемь сценариев памяти черновиков:
**30/30, finished=1 failures=0**, native exit 0 после `RequestClose()`.
Production-файлы в проверенном stage совпадают с исходниками по SHA-256.
Полные остановленные логи, manifest и exit — `final/`. В полном console log
50 error lines, включая два SCRIPT E от stock
`SCR_BaseResupplySupportStationComponent` во время завершения мира; VM/null/Pointer
ошибок нет. Полный runtime не объявляется чистым PASS.

Первый прогон не стартовал из-за занятого порта 2001: `-ServerPort` launcher
используется для проверки подключения клиента и не меняет порт server native CLI.
Этот запуск сохранён в `probe1-port-collision/` как **FAIL setup**, несмотря на
native exit 0. После остановки прежнего сервера финальный запуск выполнен с
обычным портом. Предыдущая игровая сессия и её библиотека сохранены в
`prior-session-stopped/`; server exit -1, client exit 0.

Ручная доставка событий нового dropdown, внешний вид, FPS, RHS runtime и
повторная выдача комплекта через recruitment RPC — **NOT RUN**. Новый клиент
не запускался. Исходная жалоба относилась к выбору другого отряда; ошибка
выдачи комплекта командиру по этим логам не подтверждена.

## Обвесы по слоту и категории содержимого — 2026-09-16

Каталог обвесов использует native совместимость конкретного attachment slot:
для пустого места проверяется вставка, для занятого — замена. Магазины берутся
из AMMUNITION и проверяются по muzzle slot. Проверка только читает изолированный
draft; campaign world и общий thumbnail cache не участвуют в ней. Имена мест
соответствуют native типам: магазин, прицел, штык, дульное устройство и т. д.
Двойной клик сначала открывает содержимое предмета, если оно есть; удаление
двойным кликом сохраняется для вложенных предметов без содержимого.

В контейнере доступны восемь категорий, текстовый поиск и сброс фильтров.
Слоты надевания/оружия/обвесов сохраняют автоматический контекст. Поиск не
сбрасывается при выборе другого предмета в том же контейнере; неизменившийся
каталог сохраняет прокрутку. Исправлена доступность последнего нечётного ряда;
пустой список объясняет отсутствие совместимых предметов или результатов поиска.

Evidence: `.codex-runtime/loadout-attachments-20260916/RESULT.md`.
`Test-AILoadoutStatic.ps1` **PASS / 0**, negatives 77 → 91;
`Test-Stage4Static.ps1` **PASS / 0**. Сохранён baseline
`Test-AICommanderModeStatic.ps1` **FAIL / 1, AI_COMMANDER_UI_STATE**.
Production Workbench Arland/Everon/ArlandRHS/EveronRHS и финальная Everon fixture —
**PASS / 0**, SCRIPT E/F/VM/null/Pointer warnings = 0.

Финальный client probe **113/113, finished=1 failures=0**, exit 0. На M16A2
подходят 3 магазина, 6 прицелов, 1 штык и 4 дульных устройства; на AK74_GP25 —
6 магазинов, 1 подствольный обвес, 0 штыков и 3 дульных устройства. Пустой
слот штыка с установленным GP25 отклоняет вставку по native inventory check.
Для каждого непустого списка выполнены реальная установка первого кандидата
в draft и повторная проверка замены занятого места. Фильтрация не меняет
inventory signature. Категории содержимого проверены по metadata US/USSR.

Полные остановленные логи, manifests и exits — `final/`. Сервер остановлен
`Stop-Process`, exit -1; это не graceful shutdown test. SCRIPT E/F/VM/null/Pointer
warnings = 0; native error lines — 47 server / 117 client: stock resource,
Hierarchy/pathfinding/network diagnostics, preview RPL/PP и shutdown resources.
Полный runtime не объявляется чистым PASS. Диагностические `probe1/` и `probe2/`
сохранены: они выявили, что entity preview manager находится снаружи его
native prefab-preview world; итоговая проверка сравнивает мир candidate с
миром storage и отдельно проверяет изоляцию менеджера.

Обычная Everon-пара оставлена без probe flags и таймера; profiles/manifests —
`play/`. Доставка double-click и dropdown/input, внешний вид/читаемость,
FPS, RHS runtime и повторный save/recruitment RPC — **NOT RUN**.
Fixture проверяет данные и native операции без открытия/управления GUI.

## Контекст слота, лицо и удаление из контейнера — 2026-09-16

Редактор сохраняет head/body собственного draft при rebuild, смене головного
убора и rollback. Ручной выбор категории заменён подписью контекста: список
предметов определяется выбранным местом. Native weapon type разделяет primary,
secondary и grenade; UI показывает четыре места, как stock inventory, и скрывает
пятый технический throwable. Двойной клик по содержимому контейнера вызывает
тот же проверяемый путь удаления, что кнопка. Local delete отсоединяет предмет
от слота до удаления entity; campaign entities этим путём не допускаются.

Evidence: `.codex-runtime/loadout-context-20260916/RESULT.md`.
`Test-AILoadoutStatic.ps1` **PASS / 0**, negatives 70 → 77;
`Test-Stage4Static.ps1` **PASS / 0**. Сохранён baseline
`Test-AICommanderModeStatic.ps1` **FAIL / 1, AI_COMMANDER_UI_STATE**.
Production Workbench Arland/Everon/ArlandRHS/EveronRHS и финальная Everon fixture —
**PASS / 0**, SCRIPT E/F/VM/null/Pointer warnings = 0.

Финальный client probe **89/89, finished=1 failures=0**, exit 0. Проверены
типы всех пяти native weapon slots, четыре пользовательских места, каталоги
US/USSR: primary 27/19, secondary 4/4, grenade 1/1. Три смены головного убора
сохраняют identity и фактический mesh головы; mannequin copy сохраняет тот же
mesh. В кармане ALICE удалён магазин объёмом 200 см³: занято 600 → 400 см³,
вместимость остаётся 800 см³; Undo возвращает предмет и объём.

Полные остановленные логи и manifests — `final/`. Сервер остановлен
`Stop-Process`, exit -1; это не graceful shutdown test. SCRIPT E/F/VM/null/Pointer
warnings = 0; native error lines — 47 server / 108 client (stock resources,
Hierarchy/pathfinding, preview RPL/PP и shutdown resources). Полный runtime
не объявляется чистым PASS. Диагностические прогоны `probe1/` … `probe4/`
сохранены; проверка головы уточнена по фактическому mesh, поскольку native
preview head не имеет prefab, а проверка объёма — по предмету внутри подсумка,
поскольку ремни ClothNode не занимают объём карманов.

Обычная Everon-пара оставлена без probe flags и таймера; profiles/manifests —
`play/`. Ручная доставка double-click/input, читаемость автоматического контекста,
визуальный verdict лица, FPS, RHS runtime и повторный save/recruitment RPC —
**NOT RUN**. Fixture не открывает и не управляет GUI.

## Камера, вместимость и применение экипировки — 2026-09-16

Добавлены вертикальное перемещение камеры с зажатым колесом, native объём
контейнера в литрах/процентах и применение двойным левым кликом через общий
с кнопкой путь проверки и rollback. Высота камеры сохраняется при rebuild;
панорамирование не меняет направление света на модель и не создаёт entities.
Отдельные категории «Бронежилеты» / «Разгрузки» используют фактический
`BaseLoadoutClothComponent.GetAreaType()` с кэшем metadata на время формы:
stock catalog объединяет обе группы в `VEST_AND_WAIST`. Вместимость не выводится
для служебных weapon/attachment storages, имеющих формальные native dimensions.

Evidence и итоговые runtime verdict: `.codex-runtime/loadout-controls-20260916/RESULT.md`.
`Test-AILoadoutStatic.ps1` **PASS / 0**, negatives 62 → 70;
`Test-Stage4Static.ps1` **PASS / 0**. Сохранён baseline
`Test-AICommanderModeStatic.ps1` **FAIL / 1, AI_COMMANDER_UI_STATE**.
Production Workbench Arland/Everon/ArlandRHS/EveronRHS и Everon fixture —
**PASS / 0**, без SCRIPT E/F и Pointer warnings.
Финальный client probe: **71/71, finished=1 failures=0**, exit 0. US:
3 бронежилета / 8 разгрузок; USSR: 3 / 16, без пересечений и потерь из
native vest category. Проверены камера, сохранение высоты после rebuild,
границы перемещения, неизменность света и реальные значения объёма.
Сервер остановлен `Stop-Process`, exit -1; полные остановленные логи сохранены.
SCRIPT E/F, VM/null и Pointer warnings — 0; native error lines — 47 server /
107 client (stock resources/pathfinding, preview RPL/PP и shutdown resources).
Полный runtime не объявляется чистым PASS. Для ручной проверки оставлена
обычная Everon-пара без probe/таймера, profiles и manifests в `play/`.
Ручные критерии нового ввода, читаемости объёма и FPS — **NOT RUN**;
runtime fixture проверяет данные и камеру без открытия/управления GUI.

## Удобство редактора и путь миниатюр — 2026-09-16

Снимок пользователя дал **FAIL** предыдущему визуальному варианту: пустые
карточки/пейзаж вместо вещи, светлая заливка, нечитаемые стрелки и неясная
пара «место — предмет». Новая версия использует компактный список 8 мест,
6 крупных карточек, native LoadoutArea для названия/категории (включая пустые
места), отдельные категории головных уборов и жилетов, явные действия
«Надеть»/«Заменить»/«Добавить», отключённые неприменимые кнопки, «К родителю»
и текстовые «Назад»/«Далее». Выбор сохраняется по фактическому slot ID.

Миниатюры получают только prefab names через stock client-local UI manager,
не manager изолированного draft. Bind отложен до widget initialization,
заданы alpha blend/LDR_SRGB и единая SRGBA-палитра. Mutable draft и mannequin
сохраняют собственные миры; shared thumbnail cache не редактируется и не удаляется.

Evidence: `.codex-runtime/loadout-usability-20260916/RESULT.md`.
`Test-AILoadoutStatic.ps1` **PASS / 0**, negatives 57 → 62;
`Test-Stage4Static.ps1` **PASS / 0**. Сохранён baseline
`Test-AICommanderModeStatic.ps1` **FAIL / 1, AI_COMMANDER_UI_STATE**.
Production Workbench всех четырёх графов и изолированная Everon fixture —
**PASS / 0**. Client probe **57/57, finished=1 failures=0**, включая singleton
UI manager и native head/jacket slot context; клиент завершился **exit 0**.
Сервер остановлен после probe через Stop-Process, **exit -1**; это не graceful
shutdown test. Сохранены полные остановленные server/client logs и manifests.
SCRIPT E/F, VM/null и Pointer warnings — **0**. Полный runtime не объявляется
PASS: native diagnostics **47 server / 124 client**, включая preview RPL/PP,
stock resources и resource-leak diagnostics при штатном завершении клиента.
В логе есть открытие новой формы (`EDIT_LOADOUT`), без последующих script errors;
это не подтверждение изображения или usability.

После smoke поднята обычная Everon-пара без probe/таймера закрытия, profiles и
manifests в `play/`. Пользователь подтвердил: **«Да, видны сами предметы»** —
миниатюры **PASS по ручному verdict**. Выбор/замена/возврат в контейнере,
отдельная оценка цветов и общего удобства, FPS, полная выдача через новый UI
и runtime RHS — **NOT RUN**.

## Каталог и карточки экипировки — 2026-09-16

Исправлен UI-фильтр, скрывавший оружие и одежду при выбранном свободном месте
в уже заполненном clothing storage. Каталог теперь зависит от категории,
arsenal mode и поиска; совместимость проверяется при операции и на сервере.
Добавлены ограниченные по числу widgets сетки (6 мест / 9 предметов), native
превью, выбор занятого места для замены, прокрутка колесом/кнопками и переход
в содержимое предмета. Перед rebuild/close превью отсоединяются от мира draft.

Evidence: `.codex-runtime/loadout-inventory-ui-20260916/RESULT.md`.
`Test-AILoadoutStatic.ps1` — **PASS / 0**, negative cases 50 → 57;
`Test-Stage4Static.ps1` — **PASS / 0**. Сохранён baseline
`Test-AICommanderModeStatic.ps1` — **FAIL / 1, AI_COMMANDER_UI_STATE**.
Production Workbench Arland / Everon / ArlandRHS / EveronRHS — **PASS / 0**.
Client probe на Everon — **54/54 checks, finished=1 failures=0**:
US — 150 предметов (31 оружие, 38 боеприпасов, 8 torso), USSR — 139
(23 оружия, 29 боеприпасов, 10 torso). Проверены загрузка sample prefab previews
в отдельном мире и прежние camera/light/lifecycle checks. Probe не открывает UI.

Сервер и клиент оставлены для ручной проверки; сохранены полные live snapshots,
CLI manifests и остановленные логи предыдущей сессии. В snapshot SCRIPT E/F,
VM/null и Pointer warnings — 0. Native diagnostics не исчезли: 47 server / 40
client, включая hidden-draft RPL и stock resource/PP diagnostics; новых уникальных
сообщений относительно остановленной предыдущей сессии не обнаружено.
Финальный runtime gate по остановленной новой паре, изображение карточек,
обработка ввода, полный сценарий сохранения через новую форму, освещение и FPS
— **NOT RUN**. Runtime RHS для новой формы — **NOT RUN**.

## Направление dynamic подсветки preview — 2026-09-16

В сессии `Loadout-Play-20260916-191224` пользователь сообщил: свет направлен
в спину и не следует за камерой. Визуальный verdict предыдущего варианта —
**FAIL**. Проверка только `GetOrigin` была недостаточна для dynamic spot.

`UpdateFillLight` теперь собирает полную матрицу через `DirectionAndUpMatrix`
и вызывает `SetWorldTransform` / `Update`, вместо отдельных `SetOrigin` и
`SetLightDirection`. LV повышен с 3.5 до 6.5 для фронтальной подсветки на 3 м.
Сохранены отдельный мир, stock fallback, отсутствие shadow pass и 30 FPS.
Client fixture читает реальный forward источника на 0/90/180/270/360°,
после rebuild и между callbacks, проверяя направление на модель и сторону камеры.

Evidence: `.codex-runtime/loadout-light-follow-20260916/RESULT.md`.
`AILoadoutStatic` **PASS / 0**, negative cases 49 → 50; `Stage4Static`
**PASS / 0**. `AICommanderModeStatic` сохраняет **FAIL / 1, AI_COMMANDER_UI_STATE**.
Production Workbench Arland / Everon / ArlandRHS / EveronRHS — **PASS / 0**.
Изолированная Everon fixture также **compile PASS / 0**. В клиенте завершены
**46/46 checks, finished=1 failures=0**, включая фактическую ось света и
camera-side на всех углах, callbacks и cleanup. Это functional probe, не
визуальный PASS. Сервер/клиент оставлены открытыми для пользователя; сохранены
полные live snapshots, финальный gate по остановленным logs ещё не выполнен.
На момент snapshot SCRIPT E/F, VM/null и Pointer warnings — 0; client содержит
2 прежних hidden-draft RPL errors и native resource/PP diagnostics.
Изображение, итоговая яркость и FPS требуют нового ручного verdict.

## Дополнительная подсветка preview — 2026-09-16

Штатная `InventoryPreviewWorld.et` сохранена вместе с HDR/postprocess.
В её отдельном мире создаётся один обычный `LightEntity` из штатного dynamic
`CameraLight.et`. Он даёт нейтральный заполняющий свет со стороны камеры,
без дополнительного shadow pass и specular; радиус 8 м, LV 3.5. При вращении
меняется существующая сущность, при zoom её дистанция до центра постоянна.
Отказ подсветки оставляет штатную сцену и не блокирует отображение бойца.
Поля `LightHandle` не возвращены. Installed resources читались без изменения
и не копировались в addon.

Evidence: `.codex-runtime/loadout-lighting-20260916/RESULT.md`.
`Test-AILoadoutStatic.ps1`: **PASS / 0**, negative cases 46 → 49.
`Test-Stage4Static.ps1`: **PASS / 0** до/после.
`Test-AICommanderModeStatic.ps1`: сохранён **FAIL / 1, AI_COMMANDER_UI_STATE**.
Терминальный Workbench Arland / Everon / ArlandRHS / EveronRHS и отдельная
изолированная client fixture — **PASS / 0**, SCRIPT E/F, VM/null и `Pointer type`
warnings — 0. Полные logs находятся в evidence. Сохранились прежний warning
`AICF_OrderPlanner` об up-cast и Workbench resource-leak diagnostics при выходе;
список native E/F совпадает с предыдущим compile (25 stock / 26 RHS).
Client fixture расширен проверками ownership, движения, zoom, повторного
использования, delayed lifetime и cleanup дополнительного света.

**NOT RUN:** client fixture, изображение и яркость, FPS, runtime/JIP/RHS visual.
Клиент не запускается по инструкции пользователя. Успешная компиляция не
подтверждает визуальное качество. Прежние runtime diagnostics hidden draft
и shutdown из прогона 2026-09-15 этой правкой не исправлялись и не перепроверялись.

## Возврат отображения бойца после регрессии света — 2026-09-15

Пользователь в ручной сессии `loadout-play-20260915-231424` сообщил, что боец
перестал отображаться. В client compile log обнаружены три предупреждения
`Pointer type 'LightHandle' can only be used with local variables`.
Предыдущая проверка учитывала SCRIPT E/F, но пропустила эти предупреждения;
мгновенный 10/10 fixture не доказывал жизнь сцены между кадрами или её изображение.
Доработка света из предыдущего раздела отозвана.

`AICF_LoadoutPreview` снова использует штатный `InventoryPreviewWorld.et` в
своём мире — вариант, для которого пользователь уже подтвердил видимость бойца.
Убраны поля `LightHandle` и принудительная HDR brightness. Сцена освещения
удерживается как `IEntity` и удаляется явно. Палитра, слои, `SetWorld` и лимит
preview 30 FPS сохранены. Качество прежнего света остаётся открытым замечанием.

Evidence: `.codex-runtime/loadout-preview-regression-20260915/RESULT.md`.
До/после: `AILoadoutStatic` и `Stage4Static` **PASS / 0**;
`AICommanderModeStatic` сохраняет **FAIL / 1, AI_COMMANDER_UI_STATE**.
Loadout audit теперь содержит 46 negative cases, включая запрет `LightHandle`
в полях. Терминальный Workbench **Arland, Everon, ArlandRHS, EveronRHS —
PASS / 0**, SCRIPT E/F, VM/null и `Pointer type` warnings — 0; fixture отдельно
скомпилирован. Preview-only fixture расширен проверками на 2/4/6 секунде.

**NOT RUN:** обновлённый client fixture, ручное подтверждение возврата изображения
после отката и FPS. Пользователь запретил запуск клиента; ожидавший launcher
отменён до `PROCESS_STARTED`. Server fixture завершился отдельно. Запуск клиента
для gate не возобновлялся. Отдельная ошибка подготовки: у `Start-AICFRuntime`
`ServerPort` задаёт readiness check, не actual native bind port; проба с 2003
не запускала сервер на 2003 и клиент ожидал неверный порт. Launcher в этой задаче
не менялся. Полные логи и это ограничение сохранены.

## Исправление формы экипировки и preview — 2026-09-15

Evidence: `.codex-runtime/loadout-ui-fix-20260915/RESULT.md`, полные логи
в `profile-{Server,Client}/logs` и `lighting-profile-{Server,Client}/logs`.
Первый прогон и поздняя правка света сохранены раздельно.

Исправлены применение цвета и flags программных widgets, порядок
background/input/text, слой редактора над scrim и штатное поле имени.
`AICF_LoadoutPreview` создаёт визуальную копию в собственном мире, явно
привязанном к `RenderTargetWidget`. До привязки renderer скрыт. Лимит preview —
30 FPS; вращение не пересоздаёт модель и не обновляет камеру при неподвижной
мыши. Свет нейтральный, из трёх источников своего мира, следует за камерой.

До/после: `AILoadoutStatic`, `Stage4Static`, `Stage35Static`,
`MapPointOrdersStatic`, `SupplyMapUIStatic` — **PASS / 0**.
`AILoadoutStatic` теперь включает 45 отрицательных случаев, включая палитру,
слои, привязку renderer, запрет пересборки при вращении и cleanup света.
`AICommanderModeStatic` сохраняет прежний **FAIL / 1**: `AI_COMMANDER_UI_STATE`.
Финальный Workbench Validate/Compile **Arland, Everon, ArlandRHS, EveronRHS —
PASS / 0**, SCRIPT E/F и VM/null — 0. Fixtures компилировались отдельно.

В реальном клиенте `-aicfLoadoutClientProbe 1 -aicfLoadoutPreviewOnly 1`:
**10/10, finished=1 failures=0** — owner snapshot, isolated draft,
визуальная копия, отдельный мир, свет, движение камеры, повторная сборка
с сохранением мира и cleanup. GUI fixture не открывает. Это не визуальная
проверка и не замер FPS. Server loadout fixture также завершился без нарушений
своих contracts. Native diagnostics старого hidden draft manager сохраняются;
успех этих checks не повышает полный runtime gate до PASS.

Первый смешанный прогон подтвердил `SAVED`, после чего в том же клиенте
выполнялась ручная работа с формой. Полный RPC probe не завершился до
планового server shutdown; stale/faction/index часть этого прогона — **NOT RUN**.
Shutdown dialog errors сохранены. Пользователь подтвердил, что цвета исправились
и боец появился; сообщил о плохом освещении. После этого освещение переделано.
**NOT RUN:** ручная оценка финального света, полный набор controls, сравнительный
client FPS до/после и после закрытия формы, длительный soak и RHS visual/runtime.
Предыдущие source/RPC/recruitment evidence остаются отдельными gates.

## Редактор экипировки ИИ — 2026-09-15

Реализация и terminal fixtures: [AI_LOADOUT_EDITOR.md](AI_LOADOUT_EDITOR.md).
Evidence: `.codex-runtime/loadout-editor-20260915/`; полные остановленные
runtime logs — `full/`, manifest и SHA-256 — `runtime-sha256.csv`, сводка
диагностик — `runtime-summary.csv`, итог — `RESULT.md`.

До/после: `Stage35Static`, `Stage4Static`, `RHSIntegrationStatic`,
`InfantryRecruitmentStatic`, `MapPointOrdersStatic`, `RuntimeLauncherStatic` —
**PASS / 0**. `AICommanderModeStatic` сохраняет единственный прежний
`AI_COMMANDER_UI_STATE` **FAIL / 1**. Новые `AILoadoutStatic` (32 отрицательные
копии), расширенный launcher audit с проверками импорта и `SupplyMapUIStatic`
(8 отрицательных входов) — **PASS / 0**.

Финальные production graphs без fixtures: Workbench Validate/Compile
**Arland, Everon, ArlandRHS, EveronRHS — PASS / 0**; во всех созданы Game module,
получен `Script validation successful`, SCRIPT E/F и VM/null — 0.
Исходники зафиксированы в `final-production-sha256.csv`.

Подтверждённые functional contracts:

- Stock/RHS: другая одежда и оружие, совместимый обвес, два предмета в кармане,
  roundtrip, сохранение/повторное чтение библиотеки и выдача реальному ИИ.
- Три exact member indices, отказ stale revision/deployment. В stock MP server
  создана новая replacement group и получен правильный inventory readback.
- Stock/RHS recruitment: обе группы достигли 10 бойцов при сохранении group ID;
  изменённые комплекты медиков выданы перед debit/transfer. Stock стоимость
  медиков: US `15 + 38 = 53`, USSR `15 + 105 = 120` supplies. RHS: `15 + 0`,
  `15 + 5`. Повторного debit в проверенном пути нет.
- Реальный stock client: OwnerOnly snapshot, локальный isolated draft, `SAVED`,
  отказ stale revision, неправильной faction и member index; native client exit 0.
- Импорт двух RHS шаблонов в свежий profile сохранил SHA-256 обоих файлов.

Полный runtime gate — **FAIL**. В stock recruitment console — десять
`SCR_BaseResupplySupportStationComponent` SCRIPT E, в RHS — шесть прежних
faction-init SCRIPT E. `Test-InfantryRecruitmentLog.ps1 -RequireFullRosters`
даёт соответственно **FAIL / 1** (10 и 6 diagnostics), без нарушений проверенных
recruitment/payment contracts. Native RPL сообщает о незарегистрированном
`SCR_CharacterDamageManagerComponent` preview entity: 13 записей в stock MP
server и одна в client. Это открытое ограничение новой функции, а не скрытый
baseline. Остальные native resource/world diagnostics сохранены полностью.
В финальных functional runs VM/null errors — 0; ранние неудачные прогоны,
включая исправленный RHS catalogue null и утечки первого прототипа, сохранены.

**NOT RUN:** ручной visual/controls, два одновременно редактирующих союзника,
gameplay JIP/reconnect и визуальная репликация изменённого ИИ, стрельба/перезарядка
с разными комплектами, loadout-specific fault injection частичного inventory
failure/нехватки supplies и длительный soak. Выполненный client snapshot test
не подменяет эти критерии. Результат не объявляется ACCEPTED.

## Срез перед тегом 0.1.17 — 2026-09-15

Тег объединяет параллельные ручные заявки, снятие обоих лимитов логистических
машин, постоянную видимость валидации следующей заявки и ожидание всех принятых
результатов в concurrent-probe. Бюджет AI и ограничения обычной техники сохранены.

На момент подготовки тега 128/128 production-файлов совпадают по SHA-256 с
прошедшими терминальную компиляцию четырьмя graphs от 2026-09-14. Результаты
static/compile и их baseline приведены ниже; повторная компиляция неизменных
исходников не запускалась. `AI_COMMANDER_UI_STATE` остаётся прежним FAIL / 1.

Ручной запуск Everon через `Start-AICFRuntime.ps1`, BOTH, свежие server/client
profiles: `.codex-runtime/everon-play-20260915-195020/`. В полном срезе обоих
логов до 20:14:04 MSK подтверждены три независимые заявки СССР одного игрока:
100/100, 198/198 и 739/739 припасов. Вторая принята до завершения первой,
третья — до завершения второй. Итого `dispatched=delivered=1037`,
`in_transit=lost=balance_delta=discrepancy=0`; server Stop для завершения не нужен.

`Test-LogisticsLog.ps1 -AllowActiveAtEnd -RequireDelivery` — **FAIL / 1**:
после первой доставки `VEHICLE_CLEANUP_RETAINED` и его `CORE_ERROR_BRIDGE`
дают две SCRIPT E записи в console и две копии в companion `error.log`.
Через 55 секунд cleanup восстановился, освободил lease и slot; это не отменяет
зафиксированный failure. Client SCRIPT E/F и VM/null errors обоих процессов — 0;
native resource/world/pathfinding и client icon-mask errors сохранены.

Полные снимки, индекс событий, хеши и отчёт:
`inspection-20260915-201330/DIAGNOSIS.md` внутри этого evidence. Сервер и клиент
на момент среза работают; полный stopped runtime gate, visual UI, JIP/reconnect,
исчерпание AI budget в живом мире и soak — **NOT RUN**. Срез не объявляется ACCEPTED.

## Перевозки без лимита машин — 2026-09-14

Для ручных перевозок убраны `WorkersPerDepot` и admission cap фракционного
fleet. Бюджет AI, identity, ledger и physical clearance сохраняются; обычная
техника отрядов продолжает учитывать собственный cap. Логистические lease
остаются в полном custody-счётчике и отдельно показываются как
`logistics_cap_exempt`; `capped_held` используется для ограниченной техники.

Evidence: `.codex-runtime/unlimited-transfer-vehicles-20260914-222953/`.
Baseline и после правки: `Test-ManualSupplyStatic.ps1`,
`Test-LogisticsStatic.ps1`, `Test-LogisticsContracts.ps1`,
`Test-Stage3Static.ps1`, `Test-Stage35Static.ps1`, `Test-Stage4Static.ps1` —
**PASS / 0**. Manual supply: 30 → 41 отрицательных входов; logistics contracts:
142 → 145 случаев, включая восстановленный логистический cap, удалённый cap
отрядов и удалённый AI gate. `Test-AICommanderModeStatic.ps1` сохраняет
**FAIL / 1** только по прежнему `AI_COMMANDER_UI_STATE`.

Терминальный Workbench Validate/Compile production graphs **Arland, Everon,
ArlandRHS, EveronRHS — PASS / 0**; isolated Everon с runtime fixtures —
**PASS / 0**. Полные логи, native argument arrays и сверка 128 production-файлов
с isolated source сохранены в evidence. Runtime verdict и ограничения
фиксируются отдельно в `RESULT.md` этого каталога; compile не доказывает рейс.

Isolated Everon с `AICF_LogisticsVehicleLimitProbe.c`: **38/38 runtime contracts
PASS**, 12 service lease при заполненном обычном cap=1; final server exit 0.
Полный `Test-LogisticsLog.ps1 -RequirePolicy` — **FAIL / 1** из-за двух ошибок
штатного `SCR_BaseResupplySupportStationComponent` при shutdown (также в
companion `error.log`). Полные stopped logs сохранены в `probe-long-full-logs/`.
Физические параллельные delivery, runtime-исчерпание AI, client/JIP, визуальный
UI и soak после изменения — **NOT RUN**. Первые две неуспешные попытки сохранены:
конфликт порта с живым матчем и истечение короткого preparation budget без depot.

## RHS Everon — 2026-09-13

Новый root `AIConflictEveronRHS` объединяет Everon radio policy и существующий
RHS profile; локальный callsign adapter устраняет исчерпание stock pool на
полном острове (40 позывных, 41 initialized base). Полный отчёт, точные команды
и logs: [RHS_EVERON.md](RHS_EVERON.md).

`ScenarioHeadersStatic`, `RuntimeLauncherStatic`, `RHSIntegrationStatic`,
`RankRestrictionsStatic`, `Stage35Static` — PASS / 0 до и после применимых
изменений. `AICommanderModeStatic` сохраняет FAIL / 1 только по
`AI_COMMANDER_UI_STATE`. Три отрицательные копии scenario/callsign контракта
отклонены ожидаемыми rules. Терминальный Workbench EveronRHS — PASS / exit 0.

Server подтвердил RHS profile, Everon radio policy и 20 READY groups; client
подключился через canonical readiness gate и получил replicated snapshot.
Финальные VM/null errors — 0. Общий `Test-AICommanderModeLog` с полными
остановленными server/client logs — **FAIL / 1**, шесть прежних RHS faction-init
`SCRIPT (E)`; остальные RHS/stock resource/world errors также сохранены.
Ручная визуальная проверка, deployment, gameplay JIP/reconnect, soak и
packaged Workshop build — **NOT RUN**. Ограниченный bootstrap PASS не повышает
общий runtime verdict.

## Срез перед тегом 0.1.15 — 2026-09-13

Evidence: `.codex-runtime/release-0.1.15-20260913-144657/`.
Свежая команда `powershell.exe -NoProfile -ExecutionPolicy Bypass -File
tools/Test-<Name>.ps1` выполнена для следующих аудиторов:

- **PASS / 0:** `ManualSupplyStatic`, `SupplyMapUIStatic`, `LogisticsStatic`,
  `LogisticsContracts` (142 cases), `LogisticsMapMarkersStatic`,
  `GroupMapMarkersStatic`, `Stage3Static`, `Stage35Static`, `Stage4Static`,
  `MapPointOrdersStatic`.
- **Сохранённый FAIL / 1:** `AICommanderModeStatic`, только
  `AI_COMMANDER_UI_STATE` — старое требование точной английской подписи
  ожидания player command. Такой же failure зафиксирован на HEAD до ручных
  перевозок в `manual-shipping-20260913/head-Test-AICommanderModeStatic.txt`.

Терминальный `ArmaReforgerWorkbenchSteamDiag.exe -noThrow -wbsilent
-gproj <root>/addon.gproj -addonsDir <paths> -addons <graph>
-logsDir <evidence> -wbModule=ScriptEditor -run -validate`:
**Arland, Everon, ArlandRHS — PASS / exit 0**. Во всех трёх полных логах есть
`Game successfully created`, `Script validation successful`; SCRIPT E/F,
ENGINE F и VM/null errors — 0. Аргументы сохранены в `workbench-*-args.json`.

Runtime при подготовке тега повторно не запускался. Предыдущий полный
server audit остаётся FAIL (stock AI `Failed move` и protected cleanup),
хотя доставка 100/100 и клиентские смены ETA подтверждены. Ручная визуальная
проверка, JIP/reconnect и полная multiplayer-матрица остаются NOT RUN.
Подробности и полные ссылки на evidence: [SUPPLY_MAP_UI.md](SUPPLY_MAP_UI.md).
Тег не меняет эти verdicts и не означает принятия всех runtime gates.

## Семантика gates

Каждый уровень отвечает на отдельный вопрос:

| Gate | Что доказывает | Чего не доказывает |
|---|---|---|
| Static audit | Структурные и текстовые project contracts | Компиляцию Enforce и игровое поведение |
| Workbench Validate/Compile | Совместимость исходников с текущим Enfusion API | Server/client lifecycle и долгий runtime |
| Server runtime | Authoritative gameplay и server diagnostics | Клиентский UI, replication/JIP и визуальное поведение |
| Client runtime/logs | Запуск клиента, log-side ошибки и наблюдаемую в логах репликацию | Визуальный UI и controls без ручной проверки пользователя |
| Soak | Устойчивость на заданном времени и профиле | Поведение в неиспытанных конфигурациях |

`NOT RUN` не равен `PASS`. Запущенный процесс или отсутствие `[AICF][ERROR]` в
коротком фрагменте не образуют runtime PASS. Агент не присваивает результату
статус `ACCEPTED` без решения владельца.

## Terminal-only evidence

Codex не использует Computer Use, screen capture, GUI automation,
screenshots/video или управление мышью и клавиатурой для разработки и
тестирования этого проекта. Workbench, server и client запускаются только из
терминала командами из `docs/DEVELOPMENT.md`.

Evidence агента состоит из команд, exit codes и полных логов. Если критерий
можно проверить только визуально или через интерактивный UI, Codex фиксирует
его как `NOT RUN`. Такой критерий становится проверенным только после отдельной
ручной проверки пользователя; наличие запущенного окна этого не доказывает.

## Статические аудиторы

Запускай из корня репозитория:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\Test-Stage3Static.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\Test-Stage35Static.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\Test-Stage3StaticContracts.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\Test-Stage35RecoveryPolicy.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\Test-Stage4Static.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\Test-AICommanderModeStatic.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\Test-MapPointOrdersStatic.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\Test-RHSIntegrationStatic.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\Test-ScenarioHeadersStatic.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\Test-RankRestrictionsStatic.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\Test-RuntimeLauncherStatic.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\Test-BaseBuildersStatic.ps1
```

Назначение:

| Script | Основной охват |
|---|---|
| `Test-Stage3Static.ps1` | vehicle architecture, acquisition, trip, cleanup и diagnostics contracts |
| `Test-Stage35Static.ps1` | force structure, vehicle/infantry integration и Enforce language audit |
| `Test-Stage3StaticContracts.ps1` | регрессии извлечения методов и отрицательные копии production sources: timeout, grace/blocker, поле техники в маркере и waypoint queue |
| `Test-Stage35RecoveryPolicy.ps1` | точные recovery, timing, ownership и fail-closed policies |
| `Test-Stage4Static.ps1` | economy transaction, supply balance, replication, strategic UI и RPC authority |
| `Test-AICommanderModeStatic.ps1` | stock Arland/Everon CLI preflight, supported-world gate, immutable authority policy, faction commander boundary, intent, availability replication и UI contract |
| `Test-MapPointOrdersStatic.ps1` | `BASE|POSITION` model, RPC trust boundary, bounded streamable-navmesh retry и identity guards, terrain/navmesh validation, planner ownership, отдельные current-destination/base-candidate validity boundaries, durable recovery, vehicle stale guards, deferred stock map cursor, dynamic UI palette, static allied/JIP marker и diagnostics |
| `Test-RHSIntegrationStatic.ps1` | optional dependency graph, Core isolation, stock/RHS profiles, fail-closed roles/vehicles, personnel building-browser adapter, loadout UI guard, stable-side stock radio normalization, RHS_AFRF identity voice, single lifecycle и cleanup symmetry |
| `Test-ScenarioHeadersStatic.ps1` | Arland/Everon/RHS inherited MissionHeader, menu visibility, отключённый persistence, stable project/resource GUID, platform metadata и отсутствие собственных world/layer resources |
| `Test-RankRestrictionsStatic.ps1` | `GENERAL` join/XP floor, maximum non-renegade fallback для container без `GENERAL`, central mutation/restore hook, authoritative faction/spawn recheck, replicated character state, authority/replication, запрет раннего polling и сохранение остальных admission checks |
| `Test-RuntimeLauncherStatic.ps1` | прямой native invocation без повторной сериализации, целостность `addonsDir` с пробелами/кириллицей, Arland/Everon/RHS graph, fresh profile и fail-closed client readiness gate |
| `Test-BaseBuildersStatic.ps1` | один base slot/один member, асинхронная readiness, provider/radius, authority, stock progress, физический idle gate, queue purge, безопасное удаление и отсутствие runtime fixture в production |

Аудиторы проверяют часть архитектуры регулярными выражениями. Красный rule ID
может означать реальный regression или drift тестового контракта. Сначала
сопоставь правило с поведением и историей; не меняй production-код либо regex
только ради зелёного вывода.

## Текущий baseline Stage 3/3.5 — 2026-09-05

Проверен source commit `490b6a8` после исправления устаревших контрактов
аудиторов. Production `.c` в этой правке не меняются. Перечисленные ниже
пять прежних failures больше не являются текущими failures.

| Проверка | До исправления аудиторов | После |
|---|---|---|
| `Test-Stage3Static.ps1` | FAIL / 1, 3 issues | PASS / 0 |
| `Test-Stage35Static.ps1` | FAIL / 1, 2 issues | PASS / 0 |
| `Test-AICommanderModeStatic.ps1` | PASS / 0 | PASS / 0 |
| `Test-Stage3StaticContracts.ps1` | Новая проверка | PASS / 0: parser fixtures и 10 отрицательных запусков для 8 мутаций |

`Test-Stage35RecoveryPolicy.ps1` также выполнен с PASS / 0. До правки
`Test-Stage4Static.ps1` и `Test-MapPointOrdersStatic.ps1` дали PASS / 0;
общий parser эти два аудитора не используют, повтор не требовался.

Причины обновления контрактов:

- `STAGE3_PROGRESS_EVIDENCE`: default objective-progress timeout —
  `120000` ms. Он изменён с пяти до двух минут в `59c402e` от 2026-08-14
  вместе с bounded transport recovery. Аудитор теперь требует две минуты;
  независимый трёхметровый motion threshold сохраняется.
- `STAGE3_BOUNDED_PROTECTED_CLEARANCE` и
  `STAGE35_BOUNDED_PROTECTED_CLEARANCE`: deadline вызывает
  `HandleProtectedClearanceDeadline`; после ограниченного recovery/grace
  требуется `RetainFailClosed` с причиной `PROTECTED_CLEARANCE_*GRACE_EXPIRED`
  и `m_sBlockerSignature`. Оба аудитора используют общий контракт: 30 секунд
  grace от исходного deadline, конкретные причины и сохранение blocker
  непосредственно в ветке истечения grace. Старый литерал
  `PROTECTED_CLEARANCE_DEADLINE_EXCEEDED:` больше не требуется.
- `STAGE3_MARKER_STATE`: `BuildMarkerText` выводит `ТЕХНИКА %5` и передаёт
  туда `vehicleState`, полученный из `GetSlotDisplayStatusText(slot)`.
  Проверяется и placeholder, и получение/вывод состояния.
- `STAGE35_MEANINGFUL_TASK_PROOF`: парсер теперь распознаёт полную сигнатуру
  объявления перед `{`, исключает вызовы, prototypes, comments и строки.
  `IsWaypointBoundToGroup` по-прежнему обязан использовать `GetWaypoints`
  и `Contains`; проверка не отключена.

`Test-Stage3StaticContracts.ps1` проверяет parser fixtures и восемь
отрицательных мутаций в отдельной временной копии sources. Удаление queue
evidence, blocker signature, grace check или отображаемого состояния,
а также возврат старого timeout должны давать соответствующий FAIL.
Изменение производственных файлов для этих тестов не требуется.

Полный pre/post output: `.codex-runtime/play-static-contracts-20260905/`.
Workbench и runtime для этой правки — `NOT RUN`: изменены PowerShell-аудиторы
и документация, а production Enforce/API не менялись. Исторические отчёты
ниже и в validation documents описывают результаты на даты своих прогонов;
их прежние failures не следует переносить в новый baseline.

## Карточки маркеров отрядов — 2026-09-08

Состояние техники перенесено из старой строки `BuildMarkerText` в
`BuildMarkerDetails` (`Техника: %1`). `STAGE3_MARKER_STATE` проверяет получение,
форматирование и публикацию подробностей; отрицательные fixtures продолжают
удалять источник состояния, placeholder и переданный аргумент.
`STAGE4_MAP_DIRECTION` проверяет текущий вывод расстояния и compass вместо
прежнего литерала `DIR`. Шесть применимых аудиторов дали PASS до изменения.
Дополнительная команда для этой области:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\Test-GroupMapMarkersStatic.ps1
```

Она проверяет authority, JIP, разделение callbacks отрядов/логистики,
обновление существующих маркеров, живую позицию и lifecycle hover.
Результаты изменения: [GROUP_MAP_MARKERS.md](GROUP_MAP_MARKERS.md).

## Форма снабжения на карте — 2026-09-09

Матрица и свежие результаты: [SUPPLY_MAP_UI.md](SUPPLY_MAP_UI.md).
Focused audit:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Test-SupplyMapUIStatic.ps1
```

Он проверяет stock data/ownership/identity, диапазон количества, обновление
открытой формы, input isolation, cleanup и config registration модифицированного
cursor, lifetime штатных label widgets и GUID используемых layouts; содержит восемь отрицательных
input. Нельзя вызывать `UseLabel(false)` на созданных ComboBox/Slider этой формы:
stock AutomaticScroll уже хранит animation component удаляемого label.
`BaseContainerProps` обязателен и на
`modded class SCR_MapCursorModule`: compile без него проходит, но stock Map*.conf
не создаёт module, вызывая `Unknown class` и последующие VM exceptions.
После изменения UI также обязательны Stage 4 и терминальный Workbench.
Client/JIP/visual проверяются отдельно и не выводятся из static PASS.

## Исторический baseline — 2026-08-30

Baseline повторно проверен `2026-08-30` на чистом `main`, commit
`2575ff07a4c9d2afe1e1cc7aa8f6c0d657e72a5b`, до изменений map-point orders.
После изменения HEAD его нужно запускать заново.

| Проверка | Exit | Результат |
|---|---:|---|
| `Test-Stage3Static.ps1` | 1 | `FAIL`, 5 issues |
| `Test-Stage35Static.ps1` | 1 | `FAIL`, 3 issues |
| `Test-Stage35RecoveryPolicy.ps1` | 0 | `PASS` |
| `Test-Stage4Static.ps1` | 0 | `PASS` |

`Test-AICommanderModeStatic.ps1` добавлен после этого исторического baseline и
должен передаваться отдельным verdict, без выдуманного сравнения с `main`.
То же относится к `Test-RHSIntegrationStatic.ps1`: его первый успешный прогон
является отдельным focused verdict, а не частью исторического baseline commit.
`Test-ScenarioHeadersStatic.ps1` добавлен вместе со scenario headers и также
передаётся отдельным focused verdict без сравнения с историческим commit.
`Test-MapPointOrdersStatic.ps1` добавлен вместе с map-point orders и передаётся
как отдельный focused verdict относительно этого baseline.

Stage 3 baseline failures:

- `STAGE3_PROGRESS_EVIDENCE`: auditor ожидает five-minute objective timeout,
  код задаёт `120000` ms;
- два `STAGE3_RUNNING_CARGO_STALL` contracts;
- `STAGE3_BOUNDED_PROTECTED_CLEARANCE` reason signature;
- `STAGE3_MARKER_STATE`: auditor ожидает marker fragment `VEH `.

Stage 3.5 baseline failures:

- `STAGE35_MEANINGFUL_TASK_PROOF` queue evidence;
- `STAGE35_EXACT_CARGO_STALL`;
- `STAGE35_BOUNDED_PROTECTED_CLEARANCE` reason signature.

При работе в этих областях отчёт должен показывать pre-change и post-change
наборы failures. Новых failures быть не должно; исчезнувший failure объясняется
изменением реализации или осознанным обновлением контракта.

## Переход на Reforger 1.8.0.13

Совместимость целевой версии `1.8.0.13` проверена `2026-09-03` на commit
`c8bcd798298bf79fdc9fbfbc4b6847eee77180f8`:

| Gate | Результат |
|---|---|
| Script Diff `1.8.0.10 -> 1.8.0.13` | Изменённые stock API сопоставлены с AICF; используемых удалённых или несовместимо изменённых сигнатур нет |
| Stock Workbench Validate | `PASS`: `Game successfully created`, `Script validation successful`, `SCRIPT (E/F)=0`, `ENGINE (F)=0`, VM/null errors `0` |
| RHS Workbench Validate | `PASS` с теми же compile-критериями |
| Stock server startup | `PASS`: `ROSTER_READY` за `27033` ms, `US=10`, `USSR=10`, AICF errors/FAIL `0` |
| Static audits | Stage 3: сохранены 3 известных issues; Stage 3.5: 2 известных issues; остальные focused audits `PASS` |
| Client/JIP, RHS runtime, soak, visual | `NOT RUN` |

Пять повторяющихся AICF compile warnings об избыточном up-cast и набор
Workbench shutdown resource leaks совпали с последним baseline `1.8.0.10`.
Двенадцать stock `RESOURCES/WORLD/ENTITY (E)` при server startup также совпали
с прежним baseline и не являются новой регрессией `1.8.0.13`.

## Выбор проверок по области

Для набора живой пехоты дополнительно нужны
`tools/Test-InfantryRecruitmentStatic.ps1` и
`tools/Test-InfantryRecruitmentLog.ps1`. Сценарии и fixture описаны в
[INFANTRY_RECRUITMENT.md](INFANTRY_RECRUITMENT.md). Контракты
`STAGE35_EXACT_ROSTER` и `STAGE4_VARIABLE_SUPPLY_COST` используют
`GetDeploymentSize()`: пехота появляется с одним бойцом, независимо от
цели набора `GetDesiredSize()`. Точная readiness и оплата выбранного
deployment roster остаются обязательными.

Проверка навыка `VETERAN` у управляемых групп, команды и ограничения smoke:
[AI_COMBAT_VALIDATION.md](AI_COMBAT_VALIDATION.md). Для нового прогона требуется
полное покрытие `GROUP_COMBAT_POLICY_APPLIED` по stable slots до `ROSTER_READY`;
для подкрепления — отдельное событие с `deployment=REPLACEMENT_DEPLOYMENT` и
новой generation. Измерение точности в бою является отдельным gate.

| Изменение | Минимум |
|---|---|
| Только Markdown | проверить ссылки/команды, `git diff --check` |
| Command config/bootstrap, authority, intent или availability state | `Test-AICommanderModeStatic.ps1` + затронутые Stage audits + Workbench Validate; server/client runtime и JIP при изменении snapshot |
| Прочие faction/group state, forces, objectives, orders | релевантные static audits + Workbench Validate; runtime при изменении поведения |
| `Vehicles/` или `State/Vehicles/` | Stage 3, Stage 3.5 и RecoveryPolicy + Workbench + targeted runtime |
| `Economy/` | Stage 4 static + Workbench + server runtime/log audit |
| `UI/`, RPC или campaign replicated state | Stage 4 static + Workbench + server/client runtime; JIP при изменении snapshot |
| Map-point orders (`POSITION`) | `Test-MapPointOrdersStatic.ps1` + AI commander/Stage 3/3.5/RecoveryPolicy/Stage 4 + Workbench; server/client/JIP runtime и отдельный ручной visual/input verdict |
| Stock `modded` integration или supported-world gate | отдельные Arland/Everon Workbench и canonical server runtime; клиент, если меняется UI/replication |
| `AIConflictEveron` или его header | `Test-ScenarioHeadersStatic.ps1` + `Test-RuntimeLauncherStatic.ps1`; Everon Workbench; fresh Everon server; ручная проверка плитки и packaged build |
| Content profile или `AIConflictArlandRHS` | все Stage/authority audits + `Test-RHSIntegrationStatic.ps1`; отдельный stock и RHS Workbench; fresh RHS server; client/JIP при изменении faction/UI mapping |
| `Missions/*.conf` или `.conf.meta` | `Test-ScenarioHeadersStatic.ps1` на позитивном и негативном input; отдельный затронутый Arland/Everon/RHS Workbench; source/direct MissionHeader load; ручная проверка плитки и packaged build |
| PowerShell analyzer | позитивный и негативный representative input; не ослаблять rule молча |
| Enfusion version/API | pinned reference diff + полный Workbench Validate по целевой версии |

Static audit после Enforce-изменения обязателен, но не заменяет Workbench.
Runtime, выполненный до последнего product-code изменения, не доказывает новый
commit.

## Workbench gate

Используй только терминальную Diag-команду из `docs/DEVELOPMENT.md`. Сохраняй:

- exact game/tools version;
- commit и dirty status;
- команду или последовательность действий;
- полный `console.log`, `script.log`, `error.log`;
- exit code;
- факт `Game successfully created`;
- количество `SCRIPT (E/F)`, `ENGINE (F)`, VM/null exceptions.

Platform/backend или shutdown resource messages нельзя автоматически скрывать.
Их нужно классифицировать отдельно от AICF compile error.

## Анализаторы runtime-логов

### AI commander mode

Для valid mode проверяй server authority contract и, при необходимости, JIP
client log:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File .\tools\Test-AICommanderModeLog.ps1 `
  -ServerLogPath 'C:\absolute\path\server-console.log' `
  -ExpectedMode US `
  -RequireInitialCoverage `
  -ClientLogPath 'C:\absolute\path\client-console.log'
```

Для rejected value используй отдельный parameter set:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File .\tools\Test-AICommanderModeLog.ps1 `
  -ServerLogPath 'C:\absolute\path\server-console.log' `
  -ExpectedInvalidValue NONE
```

Анализатор сверяет `CONFIG`, два `COMMAND_AUTHORITY_SET`, допустимый
`decision_authority`, initial `S0..S9` coverage и отсутствие скрытых AI
assignments на player-commanded стороне. Для invalid run он требует ровно один
`CONFIG_INVALID`/Stage 1 `RESULT FAIL` и отсутствие `MATCH_START`, roster,
strategic assignment и heartbeat activity. `-ClientLogPath` дополнительно
проверяет `COMMAND_AUTHORITY_REPLICATED`, но не заменяет ручную визуальную
проверку UI.

Server `CONFIG ai_commander_us/ussr` описывает immutable policy, но сам по себе
не доказывает момент публикации RplProp. Availability остаётся `false/false` до
перехода всех двадцати асинхронных slot в `READY`; source ordering внутри
`TryLogRosterReady()` проверяет rule `AI_COMMANDER_REPLICATION` статического
аудитора. Для runtime JIP запускай новый
client только после server `MATCH_START` и initial coverage PASS, а затем требуй
в client log последнюю согласованную пару `COMMAND_AUTHORITY_REPLICATED`.

Обычный valid analyzer ожидает mode flags последним client snapshot и поэтому
не является проверкой shutdown reset. Для отдельного lifecycle-run оставь
client подключённым при `AICF_MatchController.Stop()` и потребуй последующее
`COMMAND_AUTHORITY_REPLICATED ai_commander_us=0 ai_commander_ussr=0`; этот raw
client-log criterion и визуальный `COMMAND SYNC` фиксируются отдельно от
standard valid-mode verdict.

### Stage 2

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File .\tools\Test-Stage2Log.ps1 `
  -LogPath 'C:\absolute\path\console.log'
```

Опционально: `-MaxRepeatedOrderRecoveries 3`.

Известное ограничение: binding regex принимает только slots `[0-3]` и требует
минимум восемь bindings, тогда как текущая модель имеет slots `0-9` на каждую
фракцию. Поэтому `PASS` этого анализатора может быть false green для полного
20-slot roster и не заменяет ручную проверку всех `SPAWN_BOUND` identities.

### Stage 4

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File .\tools\Test-Stage4Log.ps1 `
  -LogPath 'C:\absolute\path\console.log'
```

`Test-Stage4Log.ps1` требует постоянный always-on invariant
`[AICF][STAGE4][INFO][CONFIG] ... enabled=1` и `SUPPLY_PROBE`.
`-AllowActiveAtEnd` разрешает незавершённые deployment reservations и legacy
shipments в исторических logs; errors он не игнорирует. Для schema 2 обновлён
float balance. Физические jobs/receipts дополнительно проверяются отдельным
`Test-LogisticsLog.ps1`; Stage 4 PASS его не заменяет.

### Физическая логистика

Возврат exact водителя телепортацией и перенос застрявшей машины проверяются
по [LOGISTICS_FALLBACK.md](LOGISTICS_FALLBACK.md). Новый
`Test-LogisticsLog.ps1 -RequireFallback` требует resume, реальное движение
после переноса и delivery того же job/vehicle. Отдельная test-only fixture
проверяет обе стороны на загруженной машине; выдача teleport-команды сама
по себе не закрывает gate.

Ограниченное восстановление движения, unknown driver exit и cooldown exact
spawn slot описаны в [LOGISTICS_RECOVERY.md](LOGISTICS_RECOVERY.md).
`Test-LogisticsLog.ps1 -RequireRecovery` требует физический recovery и delivery
того же job/vehicle. Это отдельный runtime gate; static и clock contracts
его не заменяют.

Для geometry admission logistics spawn применяется
[LOGISTICS_SPAWN_CLEARANCE.md](LOGISTICS_SPAWN_CLEARANCE.md): непаддированный
prefab OBB, сохранённые отрицательные collision/reservation contracts и отдельный
terminal-only probe. Свободные коридоры выезда больше не являются spawn gate.

Для выхода водителя к воротам дополнительно применяется
[LOGISTICS_DRIVER_INTERACTION.md](LOGISTICS_DRIVER_INTERACTION.md).
`Test-LogisticsLog.ps1 -RequireDriverInteraction` требует в полном остановленном
логе пару STARTED/RETURNED и delivery того же job/vehicle; отсутствие такого
эпизода не закрывает gate. Fixture clock contract `passed=13 total=13` проверяет
production timers отдельно от фактической анимации, возврата и failure lifecycle.

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Test-LogisticsStatic.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Test-LogisticsContracts.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Test-LogisticsLog.ps1 -LogPath 'C:\absolute\console.log' -RequireDelivery -RequirePolicy -RequireLedger -RequireGraph -RequireSearch
```

Static проверяет owners, identity, lifecycle и отсутствие timer transfers.
После замены автономной отправки формой снабжения он также запрещает вызовы
автоматического выбора доставки и spawn в `AICF_LogisticsService`, сохраняя
проверки обслуживания jobs, vehicle tick и возврата cargo. Contracts повреждает
каждую из этих границ отдельно. Прежняя проверка ожидания cooldown перед
автоматическим spawn удалена вместе с этой веткой; cooldown/identity в spawner
и их negative fixtures остаются. Старые delivery/reuse probes ниже не должны
ожидать автоматического старта рейсов. Для ручной отправки дополнительно выполняется
`powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Test-ManualSupplyStatic.ps1`:
41 отрицательный вход проверяет RPC authority, reserves, exact endpoints,
generation, результат и lifecycle, включая ожидание завершения acquisition перед
Reserve. Проверяются также разделение pending reply и активного рейса, дедупликация
по player + token, отсутствие отдельного лимита заявок и защита последнего status
от завершения более старой заявки. Валидация формы остаётся видимой независимо
от активного рейса и не перекрывается областью результата.
`tools/fixtures/AICF_ManualSupplyClientProbe.c`
в изолированной source-копии вместе с `AICF_LogisticsRuntimeProbe.c` отправляет
через клиентский facade заявки на себя, отрицательное количество, чужую базу,
затем настоящую перевозку и повторное нажатие. После итогового ответа fixture
вызывает `RequestClose()`. Fixture не открывает GUI.
Для проверки нескольких заявок передай клиенту `-aicfManualSupplyProbeConcurrent 1`:
через 40 секунд после наблюдения принятого активного рейса fixture отправляет
новую заявку и повторный клик. Событие `MANUAL_PROBE_CONCURRENT_SENT` само по себе
не доказывает принятие: в полном server log нужны два `LOGISTICS_MANUAL_ACCEPTED`
одного player с разными request/slot до завершения первого. Для каждой заявки
отдельно сверяются readiness, job, transfer и завершение; итог второй не закрывает
проверку первой. Test-only owner snapshot отдельно считает уникальные принятые
и завершённые request tokens на сервере, включая результаты старых tokens,
которые production snapshot последней заявки уже не показывает. Concurrent-probe
вызывает `RequestClose()` только после отправки второй заявки и получения всех
результатов принятых заявок, без pending reply. Отказ второй заявки не завершает
ожидание первой. Deadline 330 секунд фиксирует `MANUAL_PROBE_TIMEOUT` / FAIL,
прекращает отправку, но ждёт результаты оставшихся заявок; bounded server fixture
затем выполняет свой Stop. Завершение через server Stop не доказывает доставку.
Машины выделяются без лимитов количества на автопарк и фракцию; нужны бюджет AI
и свободные spawn sites. `aicfLogisticsWorkersPerDepot` больше не требуется.
Fixture требует
подготовленный склад/автопарк выбранной стороны; отсутствие условий не является PASS.
Текущая матрица: [SUPPLY_MAP_UI.md](SUPPLY_MAP_UI.md).

Учёт результатов concurrent-probe дополнительно проверяется в Enforce: скопируй
`AICF_ManualSupplyClientProbe.c` и `AICF_ManualSupplyProbeContracts.c` только в
`AIConflictArland/Scripts/Game/AIConflict/Tests` изолированной source-копии
(после Core, чтобы override видел `AICF_SetSupplyStatus`), выполни её terminal
Workbench Validate, затем запусти `Start-AICFRuntime.ps1 -Role Server -Variant Everon`
с её `-RepositoryRoot`, свежим `-ProfileRoot` и
`-AdditionalArguments @('-aicfManualSupplyProbeContracts', '1')`.
Через пять секунд fixture проверяет 14 сценариев и вызывает `RequestClose()`.
В полном остановленном логе нужны 14 `MANUAL_PROBE_CONTRACT passed=1` и
`MANUAL_PROBE_CONTRACTS_FINISHED cases=14 failures=0`.
Проверяются отказ второй заявки, оба порядка завершения, повторные статусы,
невалидный token и pending reply. Это gate учёта результатов, не доказательство
доставки или client replication.

`AICF_LogisticsVehicleLimitProbe.c` в isolated Arland `Scripts/Game/AIConflict/Tests`
вместе с `AICF_LogisticsRuntimeProbe.c` в isolated Core проверяет создание 12
workers одного настоящего depot и выдачу logistics leases при заполненном
обычном fleet cap. CLI: `-aicfVehicleLimitProbe 1 -aicfLogisticsProbe 1
-aicfLogisticsProbePrepare 1 -aicfLogisticsProbePeace 1`.
Проверяются уникальность slot/ordinal, неизменность исходной generation,
общая exit history, повторная reconciliation без лишних slots, отказ чужой
фракции/повторного slot, release и запрет расширения после Stop. Fixture
завершает server после результата. Это Enforce admission gate; физические
перевозки и бюджет AI дополнительно проверяются отдельным runtime.

Contracts запускает тот же аудитор на повреждённых source fixtures и позитивных/
негативных float log receipts. Проверка production policy выполняется в Enforce
через временную `tools/fixtures/AICF_LogisticsRuntimeProbe.c`, а не копией формул
на PowerShell. Временная копия находится в Core/Economy только на время probe;
после её удаления необходимы финальные Workbench gates трёх root projects.

`Test-LogisticsLog` требует полный остановленный log, schema 2, paired receipts,
readiness до jobs, отсутствие transfers после Stop и расширенный cargo balance.
`-RequireDelivery` запрещает считать отсутствие рейсов доказанной доставкой;
`-MinimumDurationMs 1800000` проверяет продолжительность soak.
`RequirePolicy/Ledger/Graph/Search` требуют отдельных Enforce evidence: 15
policy/parser cases, 18 reservation cases, 12 directed BFS cases и 6 случаев
bounded candidate preparation/priority. Search probe расширяет только собственный
test snapshot на маленькой карте; production inventory не изменяется.
Fixture flags `aicfLogisticsProbeRepeatSource`, `aicfLogisticsProbeDepotAtSource`
и `aicfLogisticsProbeReturn` создают контролируемые supply/depot условия:
повторное заполнение источника, depot на базе с избытком и заполнение получателей
после load для проверки физического RETURN. Вмешательства помечены `test_only=1`.
Для нескольких направлений обеих сторон используется
`aicfLogisticsProbeRouteMatrix=1` вместе с `Prepare`, `Peace`, `DepotAtSource`,
`RepeatSource` и `MaintainDemand`. Та же fixture выбирает четыре ближайшие к
своему HQ дорожные базы с provider/resource pool: два источника и два получателя.
Чужой HQ и половина карты ближе к нему исключаются. Точки и известные radio
connectors захватываются подготовкой; фактическую связанность после обновления
production graph показывает `LOGISTICS_MATRIX_GRAPH depth=… owner_matches=…`.
`LOGISTICS_MATRIX_READY` подтверждает подготовку, но не факт доставки.
Источники пополняются, HQ/connectors держатся на 80%; получатели сбрасываются
до 20% только после заполнения обоих, чтобы первый не вытеснял второй.
Light/heavy depots размещаются на разных источниках. Стоимость и время их
строительства пропущены только fixture; spawn geometry, driver, route recovery
и transfer остаются production. `Peace` в этом режиме делает дружественными
также `US` и `USSR`, чтобы отделить дорожные проблемы от боя.
Запускать из замороженной копии addon sources с новым profile для каждой попытки;
клиент использует ту же копию. Production файлы из-за тестовой подготовки не меняются.
Матрица Everon, команды текущего запуска и первые результаты:
[LOGISTICS_ROUTE_MATRIX.md](LOGISTICS_ROUTE_MATRIX.md).
Client fixture `aicfLogisticsClientProbe=1` ограничивает сессию пятью минутами;
опциональный `aicfLogisticsClientSpawnFaction=US|USSR` запрашивает обычный player
spawn через native faction/respawn RPC, сохраняя серверные проверки. Это отдельный
player, не logistics driver; streaming не форсируется. Клиент подключается только
через `Start-AICFRuntime.ps1` с exact свежим `ServerProfileRoot`.
Наблюдатель удерживает штатный resource subscription handle через inventory
component local player: чтение generator без подписки может вернуть старое
aggregate value. После смены машины/окончания наблюдения handle освобождается.
Server и client должны загружать одинаковую версию fixture; изменение исходника
между запусками отклоняется native script checksum validation.
Для сопоставления actual replicated cargo добавь к анализатору
`-ClientLogPath 'C:\absolute\client\console.log' -RequireLoadedClient`.
Проверяются exact slot/generation/driver и vehicle RplId, capacity и хотя бы одно
наблюдение положительного cargo. Только пустая replica этот gate не закрывает.
Обычный engine teardown и все SCRIPT errors сохраняются и анализируются отдельно.
Текущие evidence, preparations и `NOT RUN` перечислены в
[LOGISTICS_VALIDATION.md](LOGISTICS_VALIDATION.md).

### Маркеры машин логистики

`powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Test-LogisticsMapMarkersStatic.ps1`
проверяет authority, identity, faction streaming, JIP snapshot, live cargo и
cleanup; он дополняет Stage 3/3.5/4 и существующие logistics contracts.
`tools/fixtures/AICF_LogisticsMapMarkerProbe.c` копируется только в изолированную
копию Core/UI. Сервер запускается через `Start-AICFRuntime.ps1` с
`-AdditionalArguments @('-aicfLogisticsMarkerProbe','1')`, новым profile вне
репозитория и `RepositoryRoot` этой копии. Fixture создаёт catalog vehicles и
настоящие fleet leases обеих сторон, проверяет production markers, затем штатно
закрывает сервер. Она не проверяет driver, доставку или карту клиента.
Результат — `LOGISTICS_MARKER_PROBE_DONE checks=… failures=0` в полном
остановленном console log; SCRIPT errors оцениваются отдельно.
Ручная проверка карты, hover, faction switching и JIP перечислена в
[LOGISTICS_MAP_MARKERS.md](LOGISTICS_MAP_MARKERS.md).

### Rank floor

Rank runtime выполняется с отдельным fresh server/client profile. После
автоматического или ручного входа за игровую фракцию зафиксируй XP и rank,
затем последовательно проверь:

```text
GENERAL -> teamkill/collision penalty -> GENERAL -> respawn -> GENERAL
        -> disconnect/reconnect или новый JIP client -> GENERAL
```

Server log должен содержать
`[AICF][RANK][INFO][XP_FLOOR_VERIFIED] ... current_xp=... floor_xp=...
catalog_floor_rank=... rank=GENERAL` для initial join, XP mutation, respawn и
reconnect/JIP. `catalog_floor_rank=MAJOR` ожидаем для stock/default Reforger 1.8
container без отдельной записи `GENERAL`; это источник effective XP threshold,
а итоговый replicated character rank всё равно обязан быть `GENERAL`.
`XP_FLOOR_APPLIED` дополнительно требуется для каждой коррекции, которая иначе
опустила бы XP ниже порога. Сами эти строки не доказывают всю цепочку: отдельно
сохрани server evidence исходного штрафа, server respawn и
disconnect/reconnect, а в client log и HUD проверь итоговый owner-only XP/rank
после каждой фазы. Визуальный HUD остаётся ручным критерием пользователя.
Default AICF scenario headers отключают persistence, поэтому загрузка save не
входит в их release gate. Защитный deserialize path rank policy проверяется
отдельно только при появлении явно поддержанного persistence-enabled integration
header; такой тест должен закончиться `XP_FLOOR_APPLIED`, а затем
`XP_FLOOR_VERIFIED` до наблюдаемого rank state.

Сейчас отсутствуют:

- общий Stage 0/1 log verdict за пределами command-authority contract;
- специальный Stage 3/3.5 runtime log analyzer;
- общий client-log validator за пределами command-authority contract и
  автоматизированный visual validator;
- единая команда `test all` и CI.

Это пробелы покрытия, а не неявный PASS.

## Runtime contract

Запускай canonical Arland/Everon server/client из терминала по `docs/DEVELOPMENT.md`:

- новый server profile на каждый run;
- новый client profile, если нужен клиент;
- `-backendFreshSession`;
- точный launch target canonical launcher: для `EveronNorthRHS` GUID header
  в `-server`, для прежних полных вариантов — raw world;
- exact addon GUID graph выбранного варианта;
- записанные `aicf*` flags, включая факт отсутствия или exact value
  `aicfAICommanderMode`;
- одинаковые версии Game, Server и Tools.

Для stock runtime используй `Missions/AICF_Conflict_Arland.conf` или
`Missions/AICF_Conflict_Everon.conf`, для RHS —
`Missions/AICF_RHS_Conflict_Arland.conf`. Все headers наследуют официальный
scenario contract; raw world и `worldSystemsConfig` в terminal-команде должны
соответствовать выбранному родителю. Наличие `-MissionHeader` в CLI само по
себе не подтверждает применение его настроек: для северного сценария
это проверяется по фактическим initialized bases и HQ (см. evidence выше).

### Scenario menu

Scenario gate разделяется на четыре независимых уровня:

1. `Test-ScenarioHeadersStatic.ps1` подтверждает exact official parent,
   `m_bShowInScenarioMenu`, `m_eSaveTypes 0`, metadata GUID и отсутствие
   скопированных `.ent`/`.layer`.
2. Отдельный Arland/Everon/RHS Workbench Validate подтверждает регистрацию ресурсов и
   отсутствие `SCRIPT (E/F)`, `ENGINE (F)`, VM/null ошибок проекта.
3. Direct source run с GUID header в `-server` должен прочитать header и
   загрузить world/systems его официального родителя; это ещё не доказывает
   появление плитки в UI или полный gameplay runtime.
4. Пользователь вручную проверяет плитку, её название, выбор и начало новой
   сессии в меню `Сценарии`, затем повторный hosting с тем же game profile:
   прежняя progression не должна загружаться, а save/continue UI для AICF не
   должен появляться. Для релиза проверка повторяется на packaged Workshop
   build, который не видит source checkout.

Визуальные пункты остаются `NOT RUN`, пока пользователь не передаст ручной
verdict. Запуск из меню использует default `aicfAICommanderMode=BOTH`; другие
режимы command authority проверяются через отдельный dedicated server CLI.

После завершения изучи полный остановленный log. Минимальный поиск:

```powershell
Select-String -LiteralPath $log.FullName -Pattern `
  '\[AICF\]|SCRIPT\s+\((E|F)\)|ENGINE\s+\(F\)|Virtual Machine Exception|NULL pointer'
```

Префиксы приложения:

```text
[AICF][STAGE0]
[AICF][STAGE1]
[AICF][STAGE2]
[AICF][STAGE3]
[AICF][STAGE3.5]
[AICF][STAGE4]
[AICF][CONTENT]
```

Stage 2–4 используют Stage 1 `run` и `t_ms`, поэтому события одной сессии можно
сопоставлять без приблизительных wall-clock timestamps.

Для RHS runtime дополнительно обязательно подтвердить по полному остановленному
логу:

- `PROFILE_SELECTED` содержит `RHS_USMC_MSV_0_16_5150`, runtime keys
  `RHS_USAF`/`RHS_AFRF` и stable `US`/`USSR`;
- ровно двадцать initial slots дошли до `GROUP_ROSTER_READY`, затем один
  `ROSTER_READY`; каждый ready snapshot подтверждает `expected=10`, `actual=10`;
- `AI_WORLD_CAPACITY` имеет `effective_limit >= required_limit`, а каждый
  `GROUP_ROSTER_CONFIGURED` содержит `size=10`, `fallback_slots=0`, RHS USMC/MSV
  `prefabs` и ни одного `Character_US_`/`Character_USSR_`;
- при первом открытии building mode событие `PERSONNEL_BROWSER_BOUND` содержит
  `bound_count=2`, ненулевой `filtered_count` и только USMC/MSV `SentryTeam`;
- после нажатия карточки `PERSONNEL_SERVER_VALIDATED` содержит
  `essential_projected=1 allowed=1`, а server log подтверждает spawn выбранной
  RHS group prefab;
  ручной клиентский запрос одного бойца из каждой постройки проходит через
  штатные supplies, capacity и authority checks без VM/null ошибки;
- после захвата односторонней frontier-base событие `RADIO_BRIDGE_NORMALIZED`
  предшествует graph rebuild, а новый graph даёт владельцу путь от базы к relay;
- на Everon, когда friendly-компонент от HQ исчерпал все допустимые цели и не имеет даже
  одностороннего контакта с relay, `RADIO_COMPONENT_BRIDGE_NORMALIZED`
  предшествует graph rebuild; выбранная обычная база и relay образуют ближайшую
  детерминированную пару, после чего AI получает хотя бы одну новую цель;
- vehicle metadata `CATALOG` повторена событием `LIVE` после spawn; выбранные
  prefab принадлежат заявленному RHS faction catalog;
- нет `SCRIPT (E/F)`, `ENGINE (F)`, VM exception или null-pointer;
- RHS deployment map не пишет `Can't find image ''` для
  `UI/Imagesets/MilitarySymbol/ID_D.imageset`; spawn-point factions отображаются
  как полные `BLUFOR`/`OPFOR` symbols, а не пустые цветные квадраты;
- за `RHS_AFRF` сообщения HQ-комментатора воспроизводятся русским голосом;
  это клиентский аудиокритерий и без ручного прослушивания остаётся `NOT RUN`;
- отдельный stock Arland run по обычному `AIConflictArland` не выбрал RHS
  profile и сохранил stock roster/vehicle behavior.

Комплектные loadouts принадлежат RHS character prefab. Лог доказывает выбранный
prefab, но визуальное соответствие оружия/формы и UI остаётся `NOT RUN`, пока
пользователь не проверит клиент вручную.

### Runtime matrix: command authority

Каждая строка является отдельным run на новом profile. Default и explicit
`BOTH` не объединяются: первый доказывает default при отсутствии параметра,
второй — разбор exact CLI value.

| Сценарий | CLI / действие | Обязательное evidence |
|---|---|---|
| Default | параметр отсутствует | `CONFIG ai_commander_mode=BOTH ai_commander_us=1 ai_commander_ussr=1`; `COMMAND_AUTHORITY_SET` = `AI` для обеих сторон; initial `AI_COMMANDER` coverage |
| Explicit both | `-aicfAICommanderMode BOTH` | то же authority state, exact CLI записан в evidence |
| AI only US | `-aicfAICommanderMode US` | `US=AI`, `USSR=PLAYER`; у initial USSR slots `AWAITING_PLAYER_COMMAND`, `SYSTEM_HOLD` на HQ и ни одного `AI_COMMANDER` assignment |
| AI only USSR | `-aicfAICommanderMode USSR` | зеркально: `US=PLAYER`, `USSR=AI` |
| Invalid values | отдельные runs для `NONE`, unknown, lowercase и empty | один `CONFIG_INVALID` с raw value и Stage 1 `RESULT FAIL`; нет `CONFLICT_READY`, любого `RADIO_BRIDGE_*`, deferred controller `CONFIG`, `MATCH_START`, roster/spawn, `COMMAND_AUTHORITY_SET`, assignments и loops |
| Initial spawn | valid single-AI mode | static rule доказывает передачу exact prevalidated config и publish ordering; до единственного `ROSTER_READY` runtime даёт `reason=INITIAL_DEPLOYMENT group_generation=1 assignment_revision=1` своей authority для всех `S0..S9`; `COMMAND_WAITING` совпадает с `SYSTEM_HOLD` по generation/revision и идёт после assignment; hold использует HQ Defend waypoint независимо от role |
| Player override | отправить valid player order на player-commanded и AI-controlled стороне | `PLAYER_COMMAND` имеет приоритет, снимает `COMMAND_WAITING`; server повторно проверяет faction/slot/target |
| Replacement | уничтожить группу с active player и AI intent | новый `group_generation` того же `faction + stable_slot`; intent target/authority переживает runtime cleanup и восстанавливается только после revalidation |
| Base capture | сделать player target недопустимым | player-commanded slot переходит в `SYSTEM_HOLD` без нового autonomous target; AI side получает решение только через свой commander |
| Reliability/stuck/lone survivor | вызвать потерю/rebuild waypoint и завершение retreat | valid intent восстанавливает тот же target; safety recovery не меняет `decision_authority`; player side не получает скрытый AI retarget; owned MOB rebuild принимает только exact runtime transition `assignment revision N → N+1` при неизменных target/authority/intent и завершается отдельным physical confirmation |
| Vehicle replan/fallback | invalidation или fallback во время active trip | vehicle domain продолжает/восстанавливает выбранный intent, не создаёт `AI_COMMANDER` assignment для player-commanded faction; hold не поступает в vehicle admission; новый intent revision в начатом `HANDOFF` получает свежий bounded restore budget без сброса cleanup, waypoint-only revision budget не переармит |
| Client JIP | после `ROSTER_READY` и initial coverage PASS подключить новый client | server connection marker расположен после `ROSTER_READY`, client connection marker — до `COMMAND_AUTHORITY_REPLICATED`, последняя реплика содержит согласованные flags; UI показывает `AI COMMANDER`/`PLAYER COMMAND`, waiting state доступен; визуальный verdict пользователя или `NOT RUN` |
| Controller stop | отдельный lifecycle-run: оставить client подключённым при fatal/completed Stop | после ранее опубликованного valid pair client получает `ai_commander_us=0 ai_commander_ussr=0`; UI возвращается в `COMMAND SYNC`; source ordering также закрывает `AI_COMMANDER_REPLICATION` |

### Runtime matrix: vehicle boarding и dismount

| Сценарий | Действие | Обязательное evidence |
|---|---|---|
| Staging и deterministic crew | дождаться transport request со свежим spawn, включая неровный рельеф/растянутую формацию и одного настоящего outlier | `SPAWN_SITE_PLANNED` сохраняет guidance `43 м / 25 м`; `VEHICLE_SPAWN_STAGING_PROGRESS` считает всех живых в фактическом safe pad envelope `10..90 м`, поэтому собранная формация не остаётся на `9/10`; outlier получает bounded `VEHICLE_STAGING_MEMBER_ACTION_ISSUED`/`REISSUED` с live progress и exact identity; после исчерпания двух actions при доказанном `N-1/N` появляются `VEHICLE_STAGING_MEMBER_RELOCATION_ATTEMPTED` и успешный `..._RESULT postcondition=1`, после чего staging завершается; plan cancellation завершает owned action; waypoint-only `assignment revision` и чужой глобальный `base revision` дают `VEHICLE_REQUEST_RUNTIME_REVISION_ADOPTED action=KEEP_SPAWN_PLAN` без нового request generation, exact site при этом revalidated live; boarding не начинает повторный долгий `APPROACH` для уже staged roster; `CREW_AGENT_SELECTED` содержит nearest exact-door distance и stable member identity |
| Partial exact-seat plan | временно заблокировать одну Cargo reservation/seat при наличии других готовых мест | `PASSENGERS_ASSIGNED policy=DETERMINISTIC_PARTIAL_EXACT_CARGO_AFTER_MANDATORY_CREW`; действия готовых `member -> seat` пар продолжаются; `BOARDING_BLOCKER` для заблокированной пары содержит `phase`, `blocker_member`, `distance_m`, `action_state`, `progress_age_ms`, `retry`, `seat`, `linked`, `getting_in`, `recovery_fence`, `deadline_remaining_ms`; корректные reservations не снимаются |
| Boarding recovery fence | получить stalled Pilot/Turret/Cargo сначала без игроков и combat рядом, затем повторить с активным fence | normal action предшествует новому tracked `SCR_AIGetInVehicle` к тому же exact seat; retry не вызывает параллельный `CompartmentAccessComponent.GetInVehicle(... false ...)`, сохраняет action/reservation ownership, а их потеря обнаруживается следующим poll; forced exact-seat mutation происходит не раньше следующего scheduler tick и не более чем для одного member; identity/seat остаются exact; при player radius/LOS/combat Cargo пишет `*_DEFERRED` и ждёт, а mandatory Pilot/Turret сначала пишет `CREW_AGENT_ROTATION_SCHEDULED`, не раньше следующего scheduler tick — `CREW_AGENT_ROTATED`, и выдаёт видимый exact-seat action следующему детерминированному кандидату; новая reservation остаётся owned после следующего poll; исчерпание кандидатов ждёт до общего phase deadline; при terminal fallback не должно быть позднего `linked/getting_in` после восстановления infantry order |
| Full boarding at deadline | завершить физическую посадку всех живых на первом tick после истечения grace | `BOARDING_FULL_OCCUPANCY_DEADLINE_SUPPRESSED` содержит `alive=linked=settled`, `settled_polls=1`, `action=WAIT_NEXT_TICK_NO_FALLBACK`; следующий tick даёт `BOARDING_COMPLETE` и `TRANSIT`; для того же operation отсутствуют `BOARDING_TIMEOUT`, `INFANTRY_FALLBACK` и `FALLBACK_FORCE_DISEMBARK` |
| Explicit vehicle reacquisition | после terminal fallback, когда до текущей цели осталось менее `minimum_route_m`, переключить unit type `INFANTRY -> MOTORIZED_*` | `GROUP_CONFIG_ACCEPTED explicit_vehicle_admission=1`, затем `VEHICLE_EXPLICIT_ADMISSION_REQUESTED route_threshold_bypass=1`; после прохождения остальных gates один `VEHICLE_EXPLICIT_ADMISSION_CONSUMED trip_created=1` и новый `VEHICLE_REQUESTED`; обычный motorized admission без explicit intent по-прежнему получает `ROUTE_BELOW_VEHICLE_THRESHOLD` |
| Early dismount handoff | выполнить поездку и оставить вышедших members рядом с vehicle не на target-side | через `5 с` допустим exact animated retry, через `10 с` — fenced forced exact exit; `DISEMBARK_COMPLETE reason=TRIP_EXIT_PHYSICALLY_PROVEN` появляется после двух consecutive polls без occupants/transitions/inside-bounds и infantry order восстанавливается; cleanup отдельно сохраняет target-side/clearance и непрерывный `5 с` stable-clear до release/delete |

### Runtime matrix: map-point orders

| Сценарий | Действие | Обязательное evidence |
|---|---|---|
| Valid point | выбрать READY slot, нажать `MOVE TO MAP POINT`, затем отдельным кликом выбрать доступную navmesh; дождаться минимум одного commander tick и одной несвязанной смены владельца базы | первый клик только скрывает panel и после input-frame активирует cursor; для unloaded tile есть `PLAYER_POINT_ORDER_PENDING reason=NAVMESH_TILE_LOADING`, затем server `PLAYER_ORDER_ACCEPTED target_kind=POSITION` и `STRATEGIC_ASSIGNMENT target_kind=POSITION`; summary содержит `POSITION`, authoritative X/Z и новый intent revision; waypoint `MOVE_AND_HOLD` current/queued; последующие `COMMANDER_REPLAN`/`BASE_OWNER_CHANGED` не создают для того же stable slot `target_kind=BASE decision_authority=AI_COMMANDER` и не очищают player intent |
| Invalid point | отправить non-finite, за terrain bounds и точку без nearby navmesh отдельными RPC/runtime fixtures | `PLAYER_ORDER_REJECTED target_kind=POSITION` с exact `COORDINATE_NOT_FINITE`, `OUTSIDE_WORLD_BOUNDS`, `NO_NAVMESH_ENDPOINT_NEARBY`/`NAVMESH_ENDPOINT_OUT_OF_RANGE`, `NAVMESH_TILE_UNAVAILABLE` или bounded `NAVMESH_TILE_TIMEOUT`; reliable owner response приходит до UI timeout, UI показывает exact reason и actionable подсказку; runtime target/intent/waypoint не изменились |
| Cancel, input и map lifecycle | начать выбор, убедиться, что клик кнопки не выбрал точку; отменить; повторить и закрыть карту | panel/scrim скрыты только во время выбора; prompt имеет тёмно-синий фон, читаемый светлый текст и красную cancel-action; map-point action синяя, base targets янтарные; stock pan/zoom остаются доступны; после cancel/close нет deferred activation, selection callback и повторного cursor update; визуальный verdict пользователя или `NOT RUN` |
| Role/replacement/recovery | сменить role, уничтожить группу, вызвать waypoint loss и stuck rebuild | сохраняются `faction + numeric slot`, `POSITION`, X/Z и intent revision; новая group generation получает тот же endpoint; bounded recovery не выбирает BASE и не запускает capture/S&D |
| Base target list after point | выдать `POSITION` первому slot роли, затем открыть base targets другого или того же slot этой роли | ATTACK/DEFEND/RESERVE списки по-прежнему содержат только role/ownership-valid `BASE`; текущий `POSITION` representative-slot не делает все objective graph nodes видимыми; выбранная base повторно проходит ту же server validation |
| Active vehicle retarget | выдать point order в acquisition, boarding, transit и handoff | snapshot содержит kind/position; старый request/trip не коммитит stale destination; acquisition site выбирается независимо от destination; после fallback/dismount восстановлен тот же point order |
| Allied marker/JIP | выдать point order, подключить союзного и вражеского JIP client, затем сменить point и BASE | static server marker виден только союзникам, присутствует у allied JIP, меняет позицию/label и удаляется при BASE/clear/Stop; визуальный verdict пользователя или `NOT RUN` |

Для всех valid runs сохрани `CONFIG`, `COMMAND_AUTHORITY_SET`,
`COMMAND_WAITING`, `STRATEGIC_ASSIGNMENT decision_authority=...`, полные logs и
отдельные server/client/JIP verdict. Одних этих filtered events недостаточно для
общего runtime PASS.

### Автономное строительство

После изменений `Construction`, construction economy/content mappings и
пространственного контракта vehicles дополнительно запускаются:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Test-ConstructionStatic.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Test-ConstructionContracts.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Test-ConstructionLog.ps1 -LogPath '<полный остановленный console.log>' -ExpectedMode BOTH -RequireCompletion -RequireAllTypes -RequireAllFactions
```

`Test-ConstructionContracts` проверяет положительные/отрицательные log inputs
и отдельную копию исходников с нарушенной provider identity. Это проверка
анализаторов, не runtime доказательство. `-RequireCompletion` требует связь
layout с `BUILDER_PROGRESS/BUILDER_COMPLETED`, активным инструментом и online
service. Без него анализатор допускает корректные отказы поиска без построек;
такой PASS не доказывает end-to-end строительство.

`-RequireAllFactions` требует completion каждого из пяти типов отдельно для
`US` и `USSR` (либо только выбранной `-ExpectedMode` стороны). Тип и faction
completion должны совпадать с исходным order. Наличие готового stock здания
и пропуск дубликата не засчитываются как новая постройка. Synthetic contracts
проверяют полную матрицу, пропущенную пару и подменённую faction completion;
всего 16 log inputs и 25 negative static inputs.

Проверки поиска от 2026-09-20 и их ограничения описаны в
[CONSTRUCTION_SEARCH_20260920.md](CONSTRUCTION_SEARCH_20260920.md).
`Test-ConstructionSearchLog.ps1 -LogPath <stopped-console.log> -RequireBothSmall
-MaxSmallPlacementMs 60000` проверяет обе малые казармы, физическую работу и
completion, отдельно от общей проверки оплаты `Test-ConstructionLog.ps1`.
`-Scenario dynamic|owner|provider|cancel|context|no-site` проверяет evidence fault cases.
Ошибки движка не исключаются из verdict даже при успешных functional checks.
`Measure-ConstructionSearch.ps1` сохраняет времена этапов, query categories,
причины отказов, накопленное время script-вызовов и frame samples. Поля
`*_cpu_ms` основаны на `GetTickCount`, а не на OS CPU profiler.
Frame time сравнивается только у запусков
без одновременно работающего Workbench/другого тестового сервера.

Fixture `tools/fixtures/AICF_ConstructionRuntimeProbe.c` временно копируется в
Core `Construction` и запускается только через `Start-AICFRuntime.ps1` с
`-aicfConstructionProbe 1`. Она готовит supplies; решения, поиск, spawn, debit,
rollback и progress выполняет production path. Параметры test-only:
`aicfConstructionProbeMs`, `aicfConstructionProbeRefill`,
`aicfConstructionProbeType` (0..4), `aicfConstructionProbeFault partial_debit`.
`aicfConstructionProbeMatrix 1` последовательно запрашивает все пять типов,
не более одного order на faction одновременно. На тип отводится пять минут;
после completion обеих сторон проверка переходит к следующему типу раньше.
Для этого изолированного теста обычные базы, кроме HQ и control points,
распределяются между сторонами по близости к HQ. Припасы пополняются только
с `aicfConstructionProbeRefill 1`. Coverage, authority, поиск, оплата и работа
строителя остаются production. Combat может отменить order; отсутствие
completion нельзя считать PASS. `CONSTRUCTION_MATRIX_PHASE/CASE` — индекс
проверки, verdict определяется полным остановленным log.
Для повторной проверки одного типа используется
`aicfConstructionProbeMatrixType 0..4`; `aicfConstructionProbeMatrixPhaseMs`
задаёт длительность этого этапа (60000..1200000 мс).
`CONSTRUCTION_MATRIX_COVERAGE` фиксирует, какие потребности уже закрыты
stock compositions/services, без дополнительных geometry queries.
Для ручного сравнения `tools/fixtures/AICF_ConstructionManualProbe.c` в
изолированной копии пишет `CONSTRUCTION_MANUAL_HQ` и
`CONSTRUCTION_MANUAL_PLACED`: faction/HQ, prefab, provider, точные position
и angles принятого player проекта. Probe только наблюдает существующий
callback `OnPlaced`, новых subscriptions и изменений оплаты не добавляет.
Для игровой сессии `aicfConstructionProbe` оставляется выключенным, чтобы
не запускались supply preparation и автоматическая остановка.
`tools/fixtures/AICF_ConstructionReplayProbe.c` в отдельной копии Core
повторяет guards на записанных position/angles/provider и фактических work
endpoints. `aicfConstructionReplay=1` подавляет новые orders и завершает
сервер после четырёх cases; это диагностика без placement/debit. Точные CLI
параметры и ограничения восстановления мира приведены в replay report.
`aicfConstructionProbeSupplies` задаёт supply target для граничных/отказных
проверок. `CONSTRUCTION_DEFERRED_PROBE` повторно читает props и supplies через
пять секунд; анализатор обнаруживает повторное изменение props и оставшийся
непринятый root. Client probe проверяет stock entity state, но не заменяет
ручной visual verdict.
В конце fixture печатает elapsed/tick/query measurements и штатно закрывает
процесс. После удаления копии обязательны финальные Workbench gates для трёх
addon graphs. Матрица, точные команды и evidence:
[CONSTRUCTION_VALIDATION.md](CONSTRUCTION_VALIDATION.md).

### Runtime matrix: строители баз

| Сценарий | Обязательное evidence |
|---|---|
| Два и более проекта одной базы | Один `BUILDER_SPAWN_REQUESTED`, один `BUILDER_READY agents=1`; несколько `BUILDER_TARGET_ASSIGNED`/`BUILDER_COMPLETED` с той же generation/group |
| Несколько баз | Независимые очереди; одновременно не более одного live или spawning worker каждой базы |
| Работа | Перед `BUILDER_PROGRESS` есть readiness, equip и `BUILDER_WORK_STARTED`; физическая позиция соответствует рабочему endpoint, `tool_active=1 item_using=1`, character identity и `character_rpl` совпадают |
| Пустая очередь | `BUILDER_RETURNING`, фактическое прибытие к master provider, `BUILDER_HOME`, не ранее 30 секунд непрерывного простоя — `BUILDER_RETIRED reason=IDLE_AT_MAIN_TENT` |
| Новая работа при возврате/ожидании | Сохраняются group, numeric slot и generation; idle timer отменяется |
| Смерть | Cleanup старой generation, отсутствие позднего spawn callback, следующий одиночный roster не ранее 60 секунд после retirement |
| Потеря базы/удаление provider | Прогресс старой стороны прекращён, очередь очищена от stale target, `BUILDER_RETIRED` |
| Отмена/перемещение/недостижимый проект | Старый endpoint не даёт прогресс новой позиции; удалённая цель пропускается, недостижимая откладывается без второго строителя |
| Client/JIP | Видны обычный faction bot, перемещение, инструмент/анимация и готовый service; без ручного verdict визуальные пункты `NOT RUN` |

Терминальная fixture `tools/fixtures/AICF_BaseBuilderRuntimeProbe.c` создаёт
настоящие stock layouts, используя building registry и master providers.
Она обходит player placement и оплату **только в тестовом прогоне**, поэтому
не доказывает UI, проверку supplies или player RPC. Обычный addon fixture
не загружает. Для воспроизведения временно скопируй её в
`AIConflictCore/Scripts/Game/AIConflict/Construction/`, затем запусти:

```powershell
& .\tools\Start-AICFRuntime.ps1 -Role Server -Variant Stock `
  -AdditionalArguments @('-aicfBuilderProbe', '1', '-aicfRequirePlayerForResult', '0')
```

Значение probe `4` предназначено для server-only проверки:
перед каждым builder tick задаётся `ThreatBulletImpact(20)`, проверяется
отрицательный priority danger behavior у worker и положительный при stale
character identity. `BUILDER_COMBAT_PROBE` пишет `threat`, `worker_priority`,
`stale_priority`, `moving`; завершение должно сохранять `tool_active=1 item_using=1`.
Этот тест моделирует угрозу, а не полноценную перестрелку.
Значение `3` задерживает размещение на 45 секунд после readiness для
подключения клиента через canonical launcher и добавляет временное JIP-поле
в character controller. `BUILDER_CLIENT_PROBE` читает на клиенте позицию,
`IsUsingItem`, hand attachment и proxy state конкретного `character_rpl`.
Эти записи доказывают сетевое движение и состояние анимации, но не заменяют
ручную визуальную проверку. Значение probe `2` дополнительно размещает новый проект во время idle, убивает
строителя и после замены меняет владельца тестовой базы. `-Variant RHS`
проверяет ту же службу с RHS rosters. Fixture запрашивает остановку через
5 минут после readiness; если смена владельца HQ завершила match loops раньше,
нужно отдельно остановить только созданный process и зафиксировать способ
остановки. После остановки удали временную копию fixture из Core и повтори
Workbench Validate. `Test-BaseBuildersStatic.ps1` отклоняет оставленную копию.

Полный остановленный log анализируется командой:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\Test-BaseBuildersLog.ps1 `
  -LogPath 'C:\absolute\path\console.log' -RequireLifecycle -RequireToolUse
```

Для обычного probe `1` опусти `-RequireLifecycle`. Анализатор проверяет порядок
событий, уникальность active worker и completion, readiness, интервалы work,
idle и replacement. Все остальные engine/resource сообщения полного лога
классифицируются отдельно; автоматический PASS не заменяет client/visual gate.
Для probe `3` добавь `-ClientLogPath 'C:\absolute\client\console.log'`:
проверяются совпадающий RplId, client displacement не менее 2 метров и
`using=1 tool_attached=1 proxy=1`. Оба лога должны быть полностью остановлены.

## Evidence checklist

Для каждого проверочного прогона запиши:

- цель и конфигурацию сценария;
- branch, full commit SHA и dirty status;
- Game/Server/Tools versions;
- server/client роли, world, mission header и systems config;
- полный CLI с `aicf*` flags;
- время начала/завершения и способ остановки;
- команды, exit codes и rule IDs;
- абсолютные пути к полным Workbench/server/client logs;
- отдельные verdict: static, compile, server, client, JIP, soak;
- ручной verdict пользователя для визуальных критериев либо `NOT RUN`; Codex
  сам screenshots/video не создаёт;
- все `FAIL`, `BLOCKED` и `NOT RUN` без повышения статуса.

Финальная передача должна отделять новый regression от сохранённого baseline и
явно перечислять непроверенные свойства.

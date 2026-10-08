# Проверки и baseline

Перед правкой выполни относящиеся к домену проверки и сохрани полный вывод,
commit/branch, dirty status и exit codes. После правки повтори тот же набор и
сравни конкретные rule IDs. Известный FAIL не скрывай и не исправляй regex ради PASS.

## Выбор проверок

Статика находится в `tests/static/`, контракты — в `tests/contracts/`, анализаторы
логов — в `tests/log-audits/`. Полный каталог, параметры и статусы:
[TOOLS.md](TOOLS.md), runtime probes: [TEST_FIXTURES.md](TEST_FIXTURES.md).
Шаблон запуска из корня:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tests/static/Test-Stage3Static.ps1
```

Полный offline набор с сохранением argv, exit codes и полного вывода:
`powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Invoke-AICFChecks.ps1`.
`-List` показывает точные пути без запуска, `-Name Test-Stage3Static.ps1`
выбирает отдельный тест. Известные baseline failures не исключаются из exit code.

| Область | Минимальная статика / contracts |
|---|---|
| Markdown и ссылки | `git diff --check`; локальные ссылки должны работать из чистого checkout, команды и пути сверяются с исходниками |
| Offline runner и размещение скриптов | `Test-CheckRunnerContracts.ps1`; при переносе — весь offline набор до/после и проверка ссылок/зависимостей |
| Vehicle ownership, trips, cleanup | `Test-Stage3Static.ps1`, `Test-Stage35Static.ps1`, `Test-Stage3StaticContracts.ps1`, `Test-Stage35RecoveryPolicy.ps1` |
| Экономика, strategic snapshot, RPC | `Test-Stage4Static.ps1` и профильный audit |
| Bootstrap, command authority | `Test-AICommanderModeStatic.ps1`, `Test-AICommanderUIContracts.ps1` |
| Приказы в точку, навигация, recovery | `Test-MapPointOrdersStatic.ps1`, `Test-DefendWaypointInputContracts.ps1`, `Test-MovementRecoveryContracts.ps1`, `Test-RecoveryEpisodeContracts.ps1`, `Test-Issue12RecoveryContracts.ps1`, `Test-Issue14RecoveryContracts.ps1`, `Test-EndgameContracts.ps1` — выбери затронутые контракты |
| Пехотный spawn, движение и набор | `Test-InfantrySpawnPlacementStatic.ps1`, `Test-InfantryAdvanceContracts.ps1`, `Test-InfantryApproachContracts.ps1`, `Test-InfantryRecruitmentStatic.ps1`, `Test-BarracksCombatContracts.ps1` |
| Прогноз снабжения казарм | `Test-RecruitmentSupplyContracts.ps1`, InfantryRecruitment, BarracksCombat, Stage35, Stage4 |
| Команды отряда и respawn | `Test-SquadCommandsContracts.ps1`, `Test-SquadRespawnContracts.ps1` |
| Победа и полная замена группы | `Test-VictoryRespawnStatic.ps1`, `Test-EndgameContracts.ps1`, Stage35, Stage4 |
| Логистика и карта снабжения | `Test-LogisticsStatic.ps1`, `Test-LogisticsContracts.ps1`, `Test-ManualSupplyStatic.ps1`, `Test-SupplyMapUIStatic.ps1`, `Test-LogisticsMapMarkersStatic.ps1` |
| Construction и worker lifecycle | `Test-ConstructionStatic.ps1`, `Test-ConstructionContracts.ps1`, `Test-BaseBuildersStatic.ps1`, `Test-BaseBuilderDangerContracts.ps1`, `Test-ForcedSmallBarracksContracts.ps1` |
| Loadouts и личные пресеты | `Test-AILoadoutStatic.ps1`, `Test-PersonalLoadoutContracts.ps1` |
| Маркеры, локализация, rank | `Test-GroupMapMarkersStatic.ps1`, `Test-LocalizationStatic.ps1`, `Test-RankRestrictionsStatic.ps1` |
| Headers, карты, launcher | `Test-ScenarioHeadersStatic.ps1`, `Test-EveronNorthStatic.ps1`, `Test-RuntimeLauncherStatic.ps1` |
| RHS/WCS content и integration | `Test-RHSIntegrationStatic.ps1`, `Test-WCSIntegrationStatic.ps1`, затронутые scenario/authority/Stage audits |
| Сложность гарнизонов FIA | `Test-FIAGarrisonContracts.ps1`, `Test-RuntimeLauncherStatic.ps1`, ScenarioHeaders, FIAPatrol, AICombatInput и Stage3/35; runtime `AICF_FIAGarrisonProbe.c` в stage |
| Combat inputs, FIA | `Test-AICombatInputContracts.ps1`, `Test-FIAPatrolStatic.ps1` |

Изменение аудитора требует позитивного и негативного representative input.
Новые tests нужны для проверяемого контракта, а не для дублирования реализации.
Изменение production `.c`/Enfusion API требует Workbench для затронутых graph;
изменение поведения — соответствующий runtime. Для новых headers нужны source
load, отдельно ручная проверка плитки и packaged build.

## Раздельные gates

| Gate | Что подтверждает |
|---|---|
| Static audit | Проверенные структурные/текстовые контракты |
| Terminal Workbench Validate/Compile | Совместимость Game module с API и выбранным source graph |
| Server runtime | Поведение authority в заданном сценарии |
| Client/JIP | Проверенные по полным client/server logs сетевые свойства |
| Soak | Устойчивость на указанной длительности и конфигурации |
| Manual | Внешний вид и интерактивные действия, проверенные пользователем |

Один gate не заменяет другой. Запущенный процесс, native exit 0 или отсутствие
ошибок в коротком фрагменте лога не образуют runtime PASS. `NOT RUN` не означает
`PASS`, агент самостоятельно не присваивает `ACCEPTED`. Без ручного verdict
визуальные критерии остаются `NOT RUN`; screenshots/GUI automation не используются.

Команды Workbench и launcher: [DEVELOPMENT.md](DEVELOPMENT.md). Runtime fixture
из `tests/fixtures/` копируется только в изолированный stage; обычный addon не
должен содержать probe. Укажи, что fixture моделирует и чего не доказывает.

Анализаторы `Test-Stage2Log.ps1`, `Test-Stage4Log.ps1`,
`Test-InfantryRecruitmentLog.ps1`, `Test-LogisticsLog.ps1`,
`Test-ConstructionLog.ps1`, `Test-BaseBuildersLog.ps1` и другие `*Log.ps1`
читают **полные остановленные** логи. Параметры и требуемую матрицу смотри в
`param` конкретного script; для recruitment supply используй профильные
[команды и ограничения](RECRUITMENT_SUPPLY_PLANNING.md).

## Baseline реорганизации tools, 2026-10-07

Исходный commit `2cd6bf3abc8b019729f5f3a65e7d013a9d6f6872`, ветка
`codex/docs-player-readme`, до правки рабочее дерево чистое. Свежий baseline
46 offline проверок через `powershell.exe`: **41 PASS, 5 FAIL** (exit 1):

| Проверка | Сохранённая причина FAIL |
|---|---|
| `Test-InfantryApproachContracts.ps1` | `APPROACH_CLEANUP` |
| `Test-LocalizationStatic.ps1` | ParserError в Windows PowerShell: UTF-8 без BOM с кириллицей в regex. Через PowerShell 7 файл разбирается; это не PASS выполнения |
| `Test-NorthFailureContracts.ps1` | `MOVE_CALLBACK_FENCE`, `MOVE_ACTIVITY` |
| `Test-SquadCommandsContracts.ps1` | `STABLE_NEAREST` |
| `Test-Stage4Static.ps1` | `STAGE4_ATTACKED_BASES` |

Evidence: `.codex-runtime/tools-reorganization-20261007/`, `before/summary.json`
и полные `before/*.txt`. FAIL не отключены и не объявлены устаревшими контрактами;
их причины требуют отдельных исправлений. Общая матрица возвращает exit 1.

Полный повтор после переноса: **42 PASS, те же 5 FAIL** (47 проверок, включая
новый `Test-CheckRunnerContracts.ps1`), общий exit 1. Вывод — `after/summary.json`
и `after/*.txt`. После окончательного выбора `tests/log-audits/` ещё раз выполнены
семь зависимых gates: CheckRunner, Construction, ForcedSmallBarracks, Logistics,
RecoveryEpisode, SquadCommands contracts и Stage4Static — 5 PASS, прежние 2 FAIL,
exit 1 (`final-paths-verified/`). Runner проверен через `-File` и вложенный `&`
в Windows PowerShell 5.1 и PowerShell 7. Сводный `final-summary.json` и
`comparison.txt` подтверждают прежние exit codes и причины всех пяти FAIL,
новых регрессий нет. Промежуточный `final-paths/` прерван при отладке runner и
не является финальным verdict.

`Build-AICFLocalization.ps1 -Check` — PASS/0 до и после,
вывод в `localization-build-*.txt`. Проверены ссылки на отслеживаемые файлы,
каталог всех tools/tests, относительные зависимости, PowerShell 7 parsing и
`git diff --cached --check`; перенесённые fixture и четыре локальных файла
сохранены побайтово (`layout-verification.txt`).

Production `.c` и содержимое runtime probes не менялись. Workbench, server/client,
JIP, soak, визуальные проверки, showcase и watchdog — **NOT RUN** в этой задаче.
Переезд файлов не подтверждает их runtime совместимость.

## Исключение Кермована, 08.10.2026

Продолжение той же задачи на `codex/everon-north-powerplant`, baseline
`d3c9baa`, чистое дерево до правки. По уточнённому запросу Кермован удалён
из обоих North whitelist без замены; WCS+RHS наследует RHS. Остаются 7 баз,
2 HQ и 5 целей. Полные сценарии и production `.c` не изменены. Обновлены
RU/EN описания, runtime tables, North audit и fixture, README и эти заметки.

Evidence: `.codex-runtime/everon-north-exclude/`.

- `tools/Invoke-AICFChecks.ps1 -Name Test-EveronNorthStatic.ps1,Test-ScenarioHeadersStatic.ps1,Test-WCSIntegrationStatic.ps1,Test-RHSIntegrationStatic.ps1,Test-LocalizationStatic.ps1 -EvidenceRoot <before|after>`:
  4 PASS и прежний 1 FAIL, exit 1 до/после. FAIL — ParserError
  `Test-LocalizationStatic.ps1` в Windows PowerShell 5.1; новых failures нет.
- `pwsh -NoProfile -File tests/static/Test-LocalizationStatic.ps1` и
  `pwsh -NoProfile -File tools/Build-AICFLocalization.ps1 -Check`:
  PASS/0 до/после, 533 записи. Генерация таблиц тем же builder без `-Check` — PASS.
- North audit с корректным representative input — PASS/0; возврат Кермована
  вместо Тайрона при том же числе баз — ожидаемый FAIL/1 `NORTH_BASE_IDENTITIES`.
- Terminal Workbench stage для `EveronNorth` и `EveronNorthWCSRHS`:
  PASS/0, `Script validation successful`, SCRIPT E/F и ENGINE F отсутствуют.
  Команды/полные logs/exit codes: `compile.ps1`, `wb-<Variant>-argv.json`,
  `wb-<Variant>/`, `compile-summary.json`. Остались 25/39 строк resource errors,
  включая shutdown leaks; это не error-free verdict.

Runtime запускается последовательно через `runtime.ps1`, вызывающий
`tools/Start-AICFRuntime.ps1 -Role Server -Variant <Variant> -RepositoryRoot <stage>`
в PowerShell 7. Дополнительные аргументы: `-aicfNorthProbe 1`
и `-aicfRequirePlayerForResult 0`. Установленная версия — 1.8.0.13.
Fixture проверяет активный whitelist, механизмы захвата и radio routes;
завершение — `RequestClose()`, задержка сокращена с 150000 до 1000 ms только
в stage. 232 production source-файла stage совпадают с checkout по SHA-256:
`source-hashes.json`, `source-match.txt`.

Все три остановленных server logs проверены целиком через
`pwsh -NoProfile -File .codex-runtime/everon-north-exclude/analyze-runtime.ps1`
(PASS/0, `runtime-summary.json`):

| Variant | Targeted runtime | Native exit | Остальные строки ошибок |
|---|---|---|---|
| `EveronNorth` | PASS, 15/15 | 0 | 47 |
| `EveronNorthRHS` | PASS, 15/15 | 0 | 198 |
| `EveronNorthWCSRHS` | PASS, 15/15 | 0 | 233 |

Везде `nodes=7 missing=0`, есть `ROSTER_READY`, Кермована нет среди активных
баз и `GRAPH_NODE`, SCRIPT E/F и ENGINE F отсутствуют. Остальные ошибки
относятся к ресурсам/свойствам мира, pathfinding и shutdown leaks;
сохранены `errors-<Variant>.txt`. PASS относится к составу/механизмам/связности,
а не к отсутствию всех engine errors. Полные logs и временные метки:
`server-<Variant>-run/logs/*/console.log`; точные CLI, manifest и profiles:
`server-<Variant>-launcher.txt`. Все тестовые процессы завершились сами;
stage и negative-input copy удалены после сохранения hashes. `git diff --check`
и staged diff check — PASS/0.

Client/JIP, ручная карта, боевой захват/победа, строительство, soak и packaged
build — NOT RUN. Полный offline набор и его остальные известные failures
не перепроверялись. Старый неудачный опыт с электростанцией приведён ниже
как история; он не описывает финальную конфигурацию этого изменения.

## Проверка лагеря у электростанции, 08.10.2026

Исходник — `0bc09b5` (`origin/main`), чистое дерево; ветка задачи
`codex/everon-north-powerplant`. В эксперименте North headers stock/RHS
заменяли `TownBaseKermovan` на `StartingPos06`; WCS+RHS наследовал RHS.
Изменения headers и fixture **отменены после runtime FAIL**, игровой перенос
не выполнен. Сохраняется исходный состав баз.

Evidence: `.codex-runtime/everon-north-powerplant/`. Запуск
`tools/Invoke-AICFChecks.ps1 -Name Test-EveronNorthStatic.ps1,Test-ScenarioHeadersStatic.ps1,Test-WCSIntegrationStatic.ps1,Test-RHSIntegrationStatic.ps1`
с отдельным `-EvidenceRoot` дал 4 PASS/0 до и после эксперимента
(`before/`, `after/`); baseline failures в этом наборе отсутствуют.
После отмены эксперимента тот же набор — 4 PASS/0 (`restored/`),
`git diff --check` — PASS/0. Финальные изменения только в агентских заметках.
Representative positive/negative для изменённого North audit — 0/1;
это доказательство проверки списка, а не возможности захвата.

Терминальный Workbench для изолированного stage, варианты `EveronNorth` и
`EveronNorthWCSRHS`: exit 0, `Script validation successful`, без SCRIPT E/F
и ENGINE F. Полные логи: `wb-<Variant>/`, команды: `wb-<Variant>-argv.json`.
Первый stock запуск wrapper прерван обработкой native stderr в PowerShell 5.1;
он не считается PASS. Успешные повторные прогоны сохранили native exit codes.

Сервер `EveronNorth` 1.8.0.13 через `tools/Start-AICFRuntime.ps1`, PowerShell 7:
**FAIL, 18/19 checks**, `CAPTURE_MECHANISM_StartingPos06 passed=0`.
Положение, spawn point, 8 активных баз, 2 HQ, 6 control points, roster и
radio graph (`nodes=8 missing=0`) прошли. Native exit 0 не отменяет FAIL.
Fixture в stage завершила сервер через `RequestClose()` спустя 1000 ms после
проверок; штатная задержка fixture 150000 ms сокращена только в stage.
Полный stopped log: `server-EveronNorth-run/logs/*/console.log`; точные CLI,
profile и `AICF_RUNTIME_MANIFEST_JSON` — `server-EveronNorth-launcher.txt`.
Предварительный запуск в PowerShell 5.1 прервался на native stderr и не даёт
runtime verdict. SCRIPT E/F и ENGINE F в завершённом прогоне нет;
ошибки ресурсов, мира и pathfinding сохранены, логи не объявляются чистыми.

RHS/WCS server runtime после выявленного функционального FAIL, client/JIP,
ручная карта/захват, стройка, soak и packaged build — **NOT RUN**.
Старые 5 baseline failures полного offline набора не перепроверялись.
Диагностические копии удалены после сохранения evidence; production `.c`
не менялись. Вывод по архитектурному ограничению — в [GAMEPLAY.md](GAMEPLAY.md).

## Гарнизоны FIA и три сложности — 2026-10-08

Ветка `codex/fia-difficulty` от `origin/main` `27fc20a`. Baseline снят
на `9fbf85f`, чей source tree совпадает с этой целевой веткой. Evidence:
`.codex-runtime/fia-difficulty/`; полные argv/exit codes, manifests и логи
сохранены там, а не в Git. Изменены Core difficulty/garrison/crew, composition
root, fleet/spawner/cleanup, WCS content profile, два inherited North header,
локализация, launcher, профильные проверки и технические заметки.

Команда `tools/Invoke-AICFChecks.ps1 -Name <список> -EvidenceRoot <каталог>`:
`before/` — 10 PASS/0; `after-final/` — те же 10 плюс новый
`Test-FIAGarrisonContracts.ps1`, 11 PASS/0. Список: AICombatInputContracts,
DefendWaypointInputContracts, EveronNorthStatic, FIAPatrolStatic,
RuntimeLauncherStatic, ScenarioHeadersStatic, Stage35RecoveryPolicy,
Stage35Static, Stage3Static, Stage3StaticContracts. В `expanded/` также
CheckRunnerContracts, RHSIntegrationStatic и WCSIntegrationStatic — PASS/0.
Launcher audit проверяет Medium/Hard server/client manifests и отказ для
несовместимого Variant. `pwsh -File tests/static/Test-LocalizationStatic.ps1`
и `pwsh -File tools/Build-AICFLocalization.ps1 -Check` — PASS/0, 539 записей.

В выбранном baseline failures нет. Полный offline набор **NOT RUN**:
исторические пять failures (InfantryApproach APPROACH_CLEANUP, Localization
ParserError в PowerShell 5.1, NorthFailure MOVE_CALLBACK_FENCE/MOVE_ACTIVITY,
SquadCommands STABLE_NEAREST, Stage4 STAGE4_ATTACKED_BASES) не перепроверялись
и не объявляются исправленными.
После защиты null-цели повторены AICombatInputContracts, FIAGarrisonContracts
(20 guards) и FIAPatrolStatic: `after-null-guard/` — 3 PASS/0.

Терминальный Workbench 1.8.0.13: `compile.ps1 -Label production-final` и
`-Variant EveronNorth -Label stock-production-final` — PASS/0,
`Script validation successful`, без SCRIPT E/F и ENGINE F. Логи:
`wb-production-final/`, `wb-stock-production-final/`, аргументы в
соответствующих `wb-*-argv.json`. Fixture graph `wb-fixture3/` также
PASS/0. Прочих строк (E) соответственно 39/25/39; resource logs не чистые.
`source-hashes.json`/`source-match.txt` фиксируют совпадение 240 production
файлов с runtime stage; диагностические fixture добавлялись отдельно.
После защиты null-цели `wb-production-guard/`, `wb-stock-production-guard/`
и `wb-combat-guard/` вновь прошли Validate/Compile, exit 0, validation successful,
без SCRIPT E/F и ENGINE F. Финальные SHA-256 — `source-hashes-final.json` /
`source-match-final.txt`, 240/240 совпадений.

Dedicated server 1.8.0.13, Variant `EveronNorthWCSRHS`, источник — stage.
Запуск через `tools/Start-AICFRuntime.ps1 -Role Server -Difficulty <уровень>`
с `-aicfGarrisonProbe 0|1|2 -aicfRequirePlayerForResult 0`; точные команды
в `runtime.ps1`, CLI и `AICF_RUNTIME_MANIFEST_JSON` в
`server-<label>-launcher.txt`, свежие profiles — `server-<label>/`.
Все три процесса завершены fixture через `RequestClose()`, native exit 0,
в полном логе есть `ROSTER_READY` и `Game destroyed.`.

| Уровень / label | Состав на пяти FIA-точках | Проверки | Время MSK | Verdict |
|---|---|---|---|---|
| Easy | Без новых гарнизонов | 2/2 | 17:53:08–17:54:41 | PASS |
| Medium | 5 БТР-70, по 10 бойцов со всеми десантными местами | 30/30 | 17:48:56–17:51:45 | PASS |
| Hard / Hard3 | 5 БТР-70 + 5 Т-72А, по 3 бойца в танке | 55/55 | 17:45:19–17:48:18 | PASS |

Проверены полный roster/посадка, начальная дистанция до базы, отсутствие
waypoint и сдвига машины более 5 м за 60 с. Затем fixture захватывает одну
базу и убивает одного члена экипажа; спустя ещё 60 с проверяет сохранение
остальных защитников и отсутствие пополнения. Это не тест гибели всего
гарнизона. Полные остановленные логи: `server-<label>/logs/*/console.log`;
`runtime-summary.json` содержит их точные пути, CLI и время.
`pwsh -File tests/log-audits/Test-FIAGarrisonLog.ps1 -LogPath <полный лог> -Difficulty 0|1|2`
— PASS/0 для трёх логов, SCRIPT E/F и ENGINE F отсутствуют, по 233 прочих
строки (E) сохранены. Negative input без READY — ожидаемый FAIL/1
(`log-audit-negative.txt`).

Предварительные попытки не считаются PASS: `wb-initial` — неверный root
wrapper; `compile1` — ошибка float/modulo, исправлена; `server-Hard` и
`server-Hard2` — неверные prefab GUID/нулевая lease capacity, исправлены.
Hard2 остановлен адресно по process/profile, exit -1, запись
`Hard2-forced-stop.json`. Остальные тестовые серверы останавливает fixture.

Состав/удержание выше проверены до последней защиты null-цели; после неё
повторён боевой Hard, а полный трёхуровневый probe повторно не запускался.

Дополнительная изолированная `AICF_FIAGarrisonCombatProbe.c` сохранена в evidence.
Она создаёт активные BLUFOR AI-группы с неуязвимыми пехотными целями в 45 м
перед машинами, наблюдает расход боеприпасов у стрелков и удержание всех машин
в пределах 5 м в течение 90 с. Это проверка стрельбы по пехоте, а не всей
огневой эффективности. Запуск — `runtime-combat.ps1 -Difficulty Hard -Label <label>`,
штатный launcher с `-aicfGarrisonCombatProbe 1`, остановка через `RequestClose()`.

Ранние `Combat`/`Combat2` — FAIL: цели не были активированы через AI-группы.
`Combat3` подтвердил огонь БТР/танков, но весь gate — FAIL из-за трёх VM
exceptions `currentTarget` в stock `SCR_AIUpdateTargetAttackData`. Повтор
`CombatDiag4` без исправления прошёл: сбой непостоянный. По source API 1.8.0.13
два threat callback не проверяли пустую цель; добавлен ранний return, при
существующей цели остаётся super. Финальная fixture отдельно вызывает оба
callback без цели и отмечает `GARRISON_NULL_GUARD escalation=1 damage=1`.
`CombatGuard` прошёл функциональные критерии и не имел VM exceptions, но весь
log gate — FAIL из-за одной `PMC_EQUIPMENT_FAILED`: выдача нового комплекта
не удалась, `rollback=1` вернул исходный. Этот отдельный сбой RHS equipment
не исправлялся в данной задаче и не выдаётся за старый подтверждённый baseline.
`CombatGuard2` — **PASS/0**: 18:18:01–18:20:30 MSK, `btr=1 tank=1`,
оба null-callback проверены, машины удержались на месте, SCRIPT E/F и ENGINE F
нет. Полный лог — `server-CombatGuard2/logs/*/console.log`, 233 прочие строки (E).
Пулемётный огонь подтверждён расходом боеприпасов; стрельба танковой пушки
по бронецели не проверялась. Ошибка PMC из предыдущего прогона не повторилась,
но её лог и ограничение сохранены.
`git diff --check` — PASS/0. Тестовые engine processes остановлены; stage
удалён после проверки абсолютного пути, полные логи и baseline сохранены.
Точный список 33 изменённых файлов — `changed-files.txt` в evidence.
Client/JIP, ручная проверка плиток меню, packaged build, долгий soak,
танковая пушка против бронетехники и полный бой до гибели гарнизона — **NOT RUN**.

## Результаты прежних изменений

Источники — [0.1.25](../releases/0.1.25.md) и
[проверки recruitment](RECRUITMENT_SUPPLY_PLANNING.md). Это сохранённые результаты
предыдущих изменений, не новый прогон текущей задачи.

- `Test-Stage4Static.ps1`: известный **FAIL**, exit 1,
  `STAGE4_ATTACKED_BASES`. Его нужно заново фиксировать в baseline релевантной правки.
- Recruitment supply contracts: PASS с 14 отрицательными мутациями; production/
  fixture Workbench и stock fixture прошли свои targeted gates. Lifecycle и
  stability матрицы после финальных исправлений полностью не повторялись.
- Everon WCS headers/launcher: профильная статика, manifests обеих ролей и
  Workbench — PASS; server/client runtime, JIP, меню и visual — NOT RUN.
- Resource errors и shutdown leaks в старых логах сохранены. Targeted PASS
  не означает полностью чистые engine logs или единую проверку всего релиза.
- Новые WCS/RHS recruitment runtime, полная матрица всех сценариев, client/JIP,
  физическое переполнение stock склада, долгий soak, packaged build и upload
  остаются NOT RUN, пока не появится отдельное evidence.

При новом прогоне зафиксируй фактические failures; старые PASS не наследуются.
Датированное evidence не переписывай задним числом, новый вывод явно привязывай
к новому commit и конфигурации.

## Передача результата

Отчёт содержит outcome и изменённые файлы, branch/commit и dirty status,
команды с exit codes, отдельные verdict, baseline failures и новые regressions,
все `BLOCKED`/`NOT RUN`. Для runtime также нужны версии, scenario/Variant,
точный CLI и `AICF_RUNTIME_MANIFEST_JSON`, profiles, начало/конец и способ остановки,
пути к полным Workbench/server/client logs. Нужное evidence сохраняй в
игнорируемом каталоге задачи, временные copies/stages убирай.

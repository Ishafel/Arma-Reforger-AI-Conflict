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

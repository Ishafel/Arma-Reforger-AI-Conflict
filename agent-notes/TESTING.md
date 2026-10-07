# Проверки и baseline

Перед правкой выполни относящиеся к домену проверки и сохрани полный вывод,
commit/branch, dirty status и exit codes. После правки повтори тот же набор и
сравни конкретные rule IDs. Известный FAIL не скрывай и не исправляй regex ради PASS.

## Выбор проверок

Все перечисленные scripts находятся в `tools/`. Шаблон запуска из корня:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Test-Stage3Static.ps1
```

| Область | Минимальная статика / contracts |
|---|---|
| Markdown и ссылки | `git diff --check`; локальные ссылки должны работать из чистого checkout, команды и пути сверяются с исходниками |
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
из `tools/fixtures/` копируется только в изолированный stage; обычный addon не
должен содержать probe. Укажи, что fixture моделирует и чего не доказывает.

Анализаторы `Test-Stage2Log.ps1`, `Test-Stage4Log.ps1`,
`Test-InfantryRecruitmentLog.ps1`, `Test-LogisticsLog.ps1`,
`Test-ConstructionLog.ps1`, `Test-BaseBuildersLog.ps1` и другие `*Log.ps1`
читают **полные остановленные** логи. Параметры и требуемую матрицу смотри в
`param` конкретного script; для recruitment supply используй профильные
[команды и ограничения](RECRUITMENT_SUPPLY_PLANNING.md).

## Известные результаты до этой реорганизации

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

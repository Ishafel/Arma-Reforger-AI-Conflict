# Утилиты и тестовые скрипты

Этот каталог описывает поддерживаемые файлы, их назначение и границы проверки.
Выбор тестов по игровому домену — в [TESTING.md](TESTING.md), запуск игры и
Workbench — в [DEVELOPMENT.md](DEVELOPMENT.md). Наличие файла в Git или статус
«поддерживаемый» не означает PASS. Новый verdict требует собственного evidence.

## Структура и статус

Prepare-AICFWorkshopCatalog.ps1 — общий offline workflow для семи native
Publishing bundles. -PublishingRoot/-OutputRoot/-ExpectedVersion готовят
отдельные очищенные копии; -OutputRoot/-ExpectedVersion/-Check проверяют весь
завершённый набор. До копирования проверяются все inputs; после — реальные
payload, metadata, version, полный список owners и один entry. catalog.json
содержит hashes и publication=NOT_RUN. Workbench/uploader/сеть не вызываются.
Test-WorkshopCatalogContracts.ps1 проверяет 20 положительных и отрицательных
cases: смешанные версии, повреждение позднего input без частичного output,
неполный/чужой набор, изменённые manifests/payload/report и повторное появление
legacy после пересборки. Команды и границы — [DEVELOPMENT.md](DEVELOPMENT.md#подготовка-окончательного-списка-workshop).

| Каталог | Назначение | Статус |
|---|---|---|
| `tools/` | Подготовка ресурсов, launcher, измерения и watchdog | Поддерживаемые утилиты; запускать явно |
| `tools/lib/` | Общий код локализации | Библиотека, не самостоятельная команда |
| `tests/static/` | Структурные проверки production sources и launcher | Поддерживаемые offline gates |
| `tests/contracts/` | Узкие контракты, положительные/отрицательные входы и мутации | Поддерживаемые offline gates; не каждый файл содержит мутации |
| `tests/log-audits/` | Анализ полных server/client logs | Требуют evidence конкретного runtime |
| `tests/lib/` | Общий parser и vehicle audit | Библиотека для static/contracts |
| `tests/fixtures/` | Enforce runtime probes и synthetic log inputs | Диагностика; runtime совместимость проверяется перед применением |
| `.codex-runtime/` | Вывод проверок, временные копии и manifests | Игнорируется Git; не источник тестов |

`Stage2`, `Stage3`, `Stage35`, `Stage4` в именах сохранены: это исторические
имена проверок действующих контрактов, а не признак устаревшего файла.
`Issue12`, `Issue14`, `NorthFailure`, `RemainingErrors` обозначают regression
сценарии; удалять их только из-за старого названия нельзя.

Старые пути `tools/Test-*.ps1`, `tools/Stage3StaticAudit.Common.ps1` и
`tools/fixtures/` для отслеживаемых файлов заменены путями выше. Совместимых
обёрток нет: они снова загромоздили бы `tools`. Исторические release notes
сохраняют команды того релиза; для текущего checkout используй этот каталог.

## Единый offline запуск

`Test-FIAProximityContracts.ps1` проверяет scoped native lifecycle, logical
losses, authority/identity, snapshot/equipment, occupancy, transfer rollback
и Stop (40 guards и отдельная no-revival mutation).
`Test-FIAProximityLog.ps1` требует полный остановленный log, manifest, exit 0
и native readback обязательных случаев. Режимы baseline/player/ownership/Easy
и порядок stage fixtures — в [FIA_PROXIMITY.md](FIA_PROXIMITY.md).
`Test-FIADesantTransferLog.ps1` отдельно проверяет native отказ переноса,
rollback/retry и existing полный proximity gate; `-Baseline` подтверждает
исходный capacity/Ready defect, не PASS исходного поведения.

`Test-FIAArmamentContracts.ps1` проверяет RHS/WCS weapon pool, стабильный
выбор на character, authority/identity/snapshot fences, одноразовый commit,
rollback и оба boarding gates; включает отрицательные мутации.
`Test-FIAArmamentLog.ps1` проверяет полную живую матрицу с manifest и native
exit, включая отсутствие WCS в RHS graph. Применение трёх stage probes,
ожидаемые размеры и ограничения — в [FIA_ARMAMENT.md](FIA_ARMAMENT.md)
и [TEST_FIXTURES.md](TEST_FIXTURES.md).

`Test-AICommanderModeStatic.ps1` выбирает единственного владельца commander
state/API среди тел `SCR_GameModeCampaign`; нейтральные modded расширения
допустимы. Общий `Find-AICFClassRecord` по-прежнему требует единственный класс.
`Test-SquadCommandsContracts.ps1` проверяет внутри `SelectBarracks` строгий
nearest/pending выбор по расстоянию и feasible выбор по минимальному ETA,
сохраняя первый кандидат при равенстве в порядке graph/services. Оба graph
цикла обязаны читать `GetNode` по собственному возрастающему counter;
обратное индексирование проверяется отдельными мутациями.
`Test-AuditSelectionContracts.ps1 [-EvidenceRoot <новый каталог>]` запускает
оба полных аудитора на изолированной копии текущих source: 6 положительных
контролей и 39 отрицательных мутаций. Проверяет конкретный rule ID и exit 1
испорченного входа, а не просто отсутствие PASS. Сохраняет полные outputs/argv/
exit codes в `summary.json`, удаляет только свою `source-copy` в `finally`.
Без `-EvidenceRoot` использует уникальный `.codex-runtime/audit-selection-*`.

`Test-AdaptiveCommanderContracts.ps1` проверяет read-only strategic policy,
authority/slot/group/revision, данные живого состава, manual allocation, TTL,
route deferral, score/tie-break, gates, suspension и ограниченную trace.
`Test-AdaptiveCommanderLog.ps1 -LogPath <полный console.log> [-Baseline]`
проверяет controlled original selector либо полную production-policy/live-waypoint
матрицу и остановку. Подробнее: [ADAPTIVE_COMMANDER.md](ADAPTIVE_COMMANDER.md).

`Test-SocialContracts.ps1` проверяет authority, readiness/generation, initial-only
idle, общий reservation/cooldown, identity, английский event, cleanup и dedupe;
включает 15 отрицательных мутаций. `tests/log-audits/Test-SocialLog.ps1` принимает
полный остановленный server log через `-LogPath`; `-ClientLogPath` вместе с
`-JIPLogPath` добавляют проверку sound tokens, cooldown, dedupe и отсутствия replay.
Опция `-Policy` проверяет selection/lifecycle, `-GreetingPriority` — отмену
SALUTE приказом, `-ExitGuard` — запрет SALUTE в out-переходе.
Native script/resource diagnostics сохраняются отдельно от targeted verdict.

`tests/static/Test-MenuPriceStatic.ps1` проверяет границы RHS/WCS price policy:
фактический roster, единый CAMPAIGN quote, native fallback, lifecycle sources,
отсутствие side effects транзакций и зависимости Core от RHS.
`tests/log-audits/Test-MenuPriceLog.ps1 -LogPath <console.log>` анализирует полный
остановленный лог: 67 групп и 122 машины (RHS-only: `-ExpectedVehicles 48`),
повторы/getter accumulation/CopyFrom, цену spawner и debit budget. `-Client`
требует client marker, `-CompareLogPath <server-console.log>` сравнивает каждый
prefab, index, roster и цену. `-Measurements` требует actual compartments всех
машин, `-Transactions` — четыре native building preflight/debit случая.
`-CsvPath <file.csv>` экспортирует проверяемый snapshot; при FAIL экспорт
сохраняет диагностические строки и не подтверждает корректность таблицы.
Автоматический runner запускает только static audit, а не игру и log auditor.

`tests/contracts/Test-FIAStrongpointGraphContracts.ps1` проверяет общий graph gate
в трёх успешных путях radio setup и в Hard planner. Шесть отрицательных мутаций
отклоняют bypass; поведение графа отдельно проверяет `AICF_FIARandomGarrisonProbe`.

Из корня checkout:

```powershell
# Только список; никаких проверок или каталогов evidence.
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Invoke-AICFChecks.ps1 -List

# Весь offline набор, даже если один из аудиторов завершится с FAIL.
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Invoke-AICFChecks.ps1

# Только статика или один именованный тест.
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Invoke-AICFChecks.ps1 -Suite Static
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Invoke-AICFChecks.ps1 `
  -Name Test-RecruitmentSupplyContracts.ps1
```

`-Suite All|Static|Contracts`, `-Name` — точные имена файлов (при вызове через
`&` можно передать массив). Неизвестное имя — ошибка, пустой выбор не даёт PASS.
`-EvidenceRoot` задаёт **новый** каталог; существующий не перезаписывается.
По умолчанию создаётся `.codex-runtime/checks-<время>-<id>/`.
`AICF_CHECKS_EVIDENCE` печатает абсолютный путь. `context.json` содержит commit,
dirty status, выбор набора и время; `summary.json` — argv, время, exit code и
путь к полному выводу каждого теста. Runner возвращает 1 при любом FAIL, включая
известный baseline, и 0 только при PASS всего выбранного набора.

Runner запускает дочерние `powershell.exe` последовательно. Некоторые contracts
используют фиксированные вспомогательные пути в `.codex-runtime/`, поэтому не
запускай две матрицы одновременно в одном checkout. Их дополнительное evidence
может находиться вне каталога runner — точный путь задаётся исходником теста.
Параметры `-RuntimeLogPath`, `-BuilderLogPath`, `-DefendLogPath` и другие
специализированные режимы передавай непосредственно соответствующему тесту.

Он не запускает игру, Workbench, network probe или watchdog. `tests/log-audits/` и
Enforce fixture не входят в автоматическую матрицу. Native runtime и ручные
критерии остаются отдельными gates.

## Рабочие утилиты

`Test-WorkshopBootstrapStatic.ps1` защищает первый Workshop вход: стандартный
header с `m_bAICFSetupEntry`, literal metadata, точный локальный preparation
context, ограниченный offline scan, native download queue и cleanup/cancel.
`Test-WorkshopBootstrapLog.ps1 -LogPath ... -ManifestPath ... -NativeExitPath ...`
проверяет полный native run fixture; сам он игру не запускает. Настройка fixture
и границы source/packaged проверки — [TEST_FIXTURES.md](TEST_FIXTURES.md#универсальное-меню-и-native-переход).

`tests/static/Test-WorkshopScenarioNamesStatic.ps1` проверяет предел Workshop
50 символов у сырого `m_sName` и всех RU/EN названий собственных mission headers;
`-CsvPath` сохраняет полный список и длины, также при FAIL. Отсутствующая
локализация и пустое имя запрещены. `Test-WorkshopScenarioNamesContracts.ps1`
проверяет восемь граничных/отрицательных inputs через тот же аудитор. Оба входят
в полный offline runner. Это проверка метаданных, не реальная публикация.

Начальная погода: `tests/log-audits/Test-MatchWeatherLog.ps1` читает один или
несколько полных остановленных server logs с `AICF_MatchWeatherProbe`.
`-Baseline` проверяет исходный код, `-RequireVariation` — разные результаты
при нескольких новых матчах. Подробности probe: [TEST_FIXTURES.md](TEST_FIXTURES.md).

Противотанковая тактика: `tests/contracts/Test-AntiTankContracts.ps1` входит
в offline runner и проверяет границы новой политики с отрицательными мутациями.
`tests/log-audits/Test-AntiTankLog.ps1 -LogPath <полный остановленный console.log>`
проверяет выстрел на дальней дистанции и SCRIPT/fatal gate; fixture и ограничения
описаны в [TEST_FIXTURES.md](TEST_FIXTURES.md).

| Файл | Вход, результат и побочные эффекты |
|---|---|
| [Start-AICFRuntime.ps1](../tools/Start-AICFRuntime.ps1) | `-Role`, `-Variant`, `-MatchMode Standard/CoopUS/CoopUSSR`, `-Difficulty`; foreground server/client, свежий profile, manifest и readiness. Полный контракт в DEVELOPMENT. `-DryRun` только строит план |
| [AICF_Scenarios.ps1](../tools/AICF_Scenarios.ps1), [каталог](../tools/config/AICF_Scenarios.json) | Единый resolver карты/контента, режима, сложности, ресурсов и addon graph; без запуска игры. Recipe расширения — COOP_SCENARIO |
| [Update-AICFCoopScenarios.ps1](../tools/Update-AICFCoopScenarios.ps1) | Генерация тонких Co-op headers/meta, RU/EN строк и native header probe из каталога; `-Check` проверяет актуальность без записи |
| [Invoke-AICFChecks.ps1](../tools/Invoke-AICFChecks.ps1) | Offline набор и evidence, контракт выше |
| [fetch_reforger_api_reference.sh](../tools/fetch_reforger_api_reference.sh) | Git Bash, сеть при отсутствии кэша; закреплённые version/commit/SHA-256, результат только в `.cache/reforger-api/` |
| [Build-AICFLocalization.ps1](../tools/Build-AICFLocalization.ps1) | `-RepositoryRoot`, `-Check`; без `-Check` обновляет производные localization файлы. С `-Check` проверяет соответствие |
| [AICFLocalization.Common.ps1](../tools/lib/AICFLocalization.Common.ps1) | Dot-source библиотека чтения и представления localization table |
| [Stage3StaticAudit.Common.ps1](../tests/lib/Stage3StaticAudit.Common.ps1) | Dot-source parser Enforce и общие vehicle rules/negative self-checks; используется несколькими аудиторами |
| [Measure-ConstructionSearch.ps1](../tools/Measure-ConstructionSearch.ps1) | `-LogPath`, `-OutputDirectory`; таблицы поиска/этапов, JSON/CSV/Markdown. Измерение не заменяет log gate |
| [Measure-ConstructionAllBases.ps1](../tools/Measure-ConstructionAllBases.ps1) | Те же параметры; snapshot полного лога, вызывает Search, строит статистику баз. Live snapshot допускается для наблюдения, не для итогового runtime verdict |
| [Start-AICFRHSWardrobeShowcase.ps1](../tools/Start-AICFRHSWardrobeShowcase.ps1) | `-Role Server|Client`, `-SessionRoot`; создаёт отдельный source stage EveronNorthRHS и копирует showcase fixture. Нужны RHS и generated databases. Оба peers — отдельными терминальными сессиями с одним SessionRoot; запуск через canonical launcher |
| [Watch-AICFRuntimeDiagnostics.ps1](../tools/Watch-AICFRuntimeDiagnostics.ps1) | `-Profile`, `-DeadlineSeconds` (650 по умолчанию); ищет сервер по profile и **принудительно останавливает** его при диагностической ошибке/дедлайне. Только для своего тестового server profile. Exit 0 — наблюдал выход; 2 — ошибка; 3 — дедлайн. Это не graceful shutdown и не runtime PASS |

`Test-InstigatorContextStatic.ps1` и `Test-InstigatorContextContracts.ps1`
входят в полный offline набор. Отдельный `tests/log-audits/Test-InstigatorContextLog.ps1`
проверяет полные stopped runtime logs и не входит в offline runner.
Native probe/CLI и требования: [INSTIGATOR_CONTEXT.md](INSTIGATOR_CONTEXT.md).

`Test-ScenarioSetupStatic.ps1` входит в полный offline набор. Отдельный
`tests/log-audits/Test-ScenarioSetupLog.ps1 [-Client]` проверяет stopped runtime
logs legacy default Arland; UI readbacks не подменяют ручной input/Host.
Описание: [SCENARIO_SETUP.md](SCENARIO_SETUP.md).
`Test-ScenarioCatalogLogContracts.ps1` входит в offline набор: положительный
control и 15 отрицательных случаев отдельного `Test-ScenarioCatalogLog.ps1`.
Последний проверяет native регистрацию/hidden presets в 16 cases, точные CLI и stopped log,
отклоняет SCRIPT/VM/fatal; описание fixture — [TEST_FIXTURES.md](TEST_FIXTURES.md).

`Prepare-AICFWorkshopBundle.ps1 -BundlePath ... -OutputPath ...` копирует
native bundle с проверенными hashes и очищает только `asset.scenarios`:
один setup в Arland, 0 в остальных addons. `-Check` требует окончательный
список без legacy. Input, payload и версия сохраняются, uploader не вызывается.
`Test-WorkshopBundleContracts.ps1`: 26 cases, семь проектов, native Core schema
без scenarios field, неизменность payload/прочей metadata и отказы на повреждения.
Фактический upload подготовленной копии пока не подтверждён.

`tools/Update-AICFScenarioRegistry.ps1 [-Check]` генерирует frontend descriptors
из `tools/config/AICF_Scenarios.json`; проверка входит в новый
`Test-UniversalScenarioStatic.ps1`. `Test-UniversalScenarioLogContracts.ps1`
проверяет UI/transition анализатор на положительных и отрицательных логах,
включая UTF-8 пути, лишние/недостающие addons и неправильный launch resource.
`Test-UniversalScenarioLog.ps1 [-Transition]` анализирует полный stopped native
log универсального меню; в offline runner не входит.
`Test-ScenarioEnvironmentStatic.ps1` проверяет authority и одноразовую передачу
времени/погоды через GameSessionStorage, exact target/TTL, cleanup отказа и native
weather boundary. Transition analyzer с `-aicfProbeTime/-aicfProbeWeather` в
manifest требует реальные начальные часы/погоду, repeat и save/load readbacks;
его отрицательные контракты входят в общий offline набор.

## Локальные файлы вне набора

В `tools/` остаются два ignored/untracked скрипта с отдельными точными ignore rules:

- `tools/Test-FIAPatrolContracts.ps1`
- `tools/Test-FIAPatrolLog.ps1`

Статус: **LOCAL ONLY / NOT RUN**, не доступны из чистого checkout и не являются
зависимостями поддерживаемого набора. Их происхождение и готовность не установлены;
содержимое не менялось и в Git не добавлялось. Для принятия локального скрипта:
прочитать его и соседний домен, проверить необходимые dependencies, снять baseline,
перенести в соответствующий `tests/` каталог, добавить документацию и выполнить
нужные gates. Только после этого убрать его точное ignore rule.

## Правила сопровождения

1. Новый reusable script сразу добавляется в Git: общего ignore для `tools/*`
   больше нет. Одноразовые команды и outputs остаются в `.codex-runtime/<задача>/`.
2. Проверку исходников клади в `static`, узкий regression/проверку аудитора —
   в `contracts`, анализатор пользовательского лога — в `log-audits`. Имена `Test-*.ps1`
   в первых двух каталогах означают безопасный offline запуск **без обязательных
   параметров**; runner автоматически включает новый файл.
3. Для нового файла укажи здесь назначение; для runtime probe обнови
   [TEST_FIXTURES.md](TEST_FIXTURES.md). При новом домене обнови TESTING.
4. `RepositoryRoot` тестов по умолчанию — два уровня выше `$PSScriptRoot`.
   Общие библиотеки и дочерние аудиторы привязывай к реальному checkout скрипта;
   `RepositoryRoot` может указывать на намеренно неполную/сломанную fixture.
5. При изменении проверяющего правила нужны positive и negative inputs.
   Сохраняй baseline rule IDs, не ослабляй аудит ради зелёного набора.
6. Fixture копируется только в отдельный stage нужного addon, затем выполняются
   terminal compile и runtime. Некоторые probes меняют ownership, roster,
   supplies, перемещают/убивают entities и завершают игру. Нельзя включать все
   probes одновременно или переносить их в production.
7. Старый PASS не наследуется. Статус скрипта, его offline verdict и runtime
   результат описываются отдельно. Удаление допустимо после проверки всех
   callers, замещающего покрытия и ссылок в текущей документации.

`tests/contracts/Test-MatchShutdownContracts.ps1` проверяет остановку controller
до native teardown и пять отрицательных lifecycle mutations.
`tests/contracts/Test-ShutdownLogContracts.ps1` проверяет strict full-log gate,
native/отложенный entity readback и уникальность 24 операций на positive/negative
inputs, включая обязательные точные batch/equipment пары либо server idle mode
в launcher manifest и соответствующий completion mode. Client допускается
для обоих modes. `tests/log-audits/Test-ShutdownLog.ps1` анализирует полные stopped logs
`AICF_ShutdownProbe`; параметры и границы — [SHUTDOWN_LIFECYCLE.md](SHUTDOWN_LIFECYCLE.md).

`tests/log-audits/Test-LoadoutContentsInventoryLog.ps1` проверяет полные
остановленные logs исходного personal kit: independent native oracle,
ContentRows, точные адреса, remove/Undo, Capture/Restore, manifest и native exit.
`-WCS` требует шесть исходных запасных магазинов АК-12, `-Client` выбирает client
peer. `-Baseline -WCS` подтверждает только старое расхождение rows.
Script/VM/fatal/shutdown leak ошибки не исключаются из gate. Fixture и
ограничения описаны в [LOADOUT_CONTENTS.md](LOADOUT_CONTENTS.md).

## Каталог offline проверок

Все файлы в таблице поддерживаются как отдельные gates. `Static` и `Contracts`
обозначают размещение, а не verdict. Имена ведут прямо к исходнику и его `param`.

| Проверка | Назначение |
|---|---|
| [Test-MatchShutdownContracts.ps1](../tests/contracts/Test-MatchShutdownContracts.ps1) | Early controller Stop, native entity owner и отрицательные teardown mutations |
| [Test-ShutdownLogContracts.ps1](../tests/contracts/Test-ShutdownLogContracts.ps1) | Full shutdown log, unique batches, немедленный/отложенный native cleanup readback |
| [Test-ManagedObserversContracts.ps1](../tests/contracts/Test-ManagedObserversContracts.ps1) | Authority/live roster и cleanup native observers для штатной охраны; отрицательные мутации |
| [Test-AICombatInputContracts.ps1](../tests/contracts/Test-AICombatInputContracts.ps1) | Пустые combat/weapon inputs native BT |
| [Test-AICommanderModeStatic.ps1](../tests/static/Test-AICommanderModeStatic.ps1) | CLI режима, authority и UI состояния командующего |
| [Test-AICommanderUIContracts.ps1](../tests/contracts/Test-AICommanderUIContracts.ps1) | Положительные и отрицательные мутации UI authority/markers |
| [Test-AILoadoutStatic.ps1](../tests/static/Test-AILoadoutStatic.ps1) | Catalog, выдача, persistence, identity и rollback экипировки |
| [Test-BarracksCombatContracts.ps1](../tests/contracts/Test-BarracksCombatContracts.ps1) | Безопасность казарм в бою |
| [Test-BaseBuilderDangerContracts.ps1](../tests/contracts/Test-BaseBuilderDangerContracts.ps1) | Реакция worker на опасность и возврат к работе |
| [Test-BaseBuildersStatic.ps1](../tests/static/Test-BaseBuildersStatic.ps1) | Lifecycle и ownership строительных workers |
| [Test-CheckRunnerContracts.ps1](../tests/contracts/Test-CheckRunnerContracts.ps1) | Выбор offline набора, продолжение после FAIL и сохранность evidence |
| [Test-ConstructionContracts.ps1](../tests/contracts/Test-ConstructionContracts.ps1) | Synthetic logs и source mutations строительства |
| [Test-ConstructionStatic.ps1](../tests/static/Test-ConstructionStatic.ps1) | Planner, оплата, search, path и worker completion |
| [Test-DefendAllocationLogContracts.ps1](../tests/contracts/Test-DefendAllocationLogContracts.ps1) | 2 positive / 12 negative cases полного allocation log audit |
| [Test-DefendWaypointInputContracts.ps1](../tests/contracts/Test-DefendWaypointInputContracts.ps1) | Входы native defend waypoint и scoped allocation guard; 11 mutations |
| [Test-EndgameContracts.ps1](../tests/contracts/Test-EndgameContracts.ps1) | Terminal hold и завершение матча |
| [Test-EveronNorthStatic.ps1](../tests/static/Test-EveronNorthStatic.ps1) | Whitelist северного Everon |
| [Test-FIAPatrolStatic.ps1](../tests/static/Test-FIAPatrolStatic.ps1) | Stock FIA patrol lifecycle |
| [Test-ForcedSmallBarracksContracts.ps1](../tests/contracts/Test-ForcedSmallBarracksContracts.ps1) | Synthetic logs forced-small fallback |
| [Test-GroupMapMarkersStatic.ps1](../tests/static/Test-GroupMapMarkersStatic.ps1) | Репликация и содержимое маркеров групп |
| [Test-InfantryAdvanceContracts.ps1](../tests/contracts/Test-InfantryAdvanceContracts.ps1) | Продвижение пехоты и handoff приказа |
| [Test-InfantryApproachContracts.ps1](../tests/contracts/Test-InfantryApproachContracts.ps1) | Approach geometry, identity и cleanup |
| [Test-InfantryRecruitmentStatic.ps1](../tests/static/Test-InfantryRecruitmentStatic.ps1) | Recruitment transaction, readiness и muster |
| [Test-InfantrySpawnPlacementStatic.ps1](../tests/static/Test-InfantrySpawnPlacementStatic.ps1) | Размещение пехоты и проверка spawn inputs |
| [Test-Issue12RecoveryContracts.ps1](../tests/contracts/Test-Issue12RecoveryContracts.ps1) | Recovery regression; опционально -RuntimeLogPath |
| [Test-Issue14RecoveryContracts.ps1](../tests/contracts/Test-Issue14RecoveryContracts.ps1) | Builder/defend recovery; опционально -BuilderLogPath/-DefendLogPath |
| [Test-LocalizationStatic.ps1](../tests/static/Test-LocalizationStatic.ps1) | IDs, placeholders, таблица и производные localization файлы |
| [Test-LogisticsContracts.ps1](../tests/contracts/Test-LogisticsContracts.ps1) | Synthetic server/client logs и мутации logistics source |
| [Test-LogisticsMapMarkersStatic.ps1](../tests/static/Test-LogisticsMapMarkersStatic.ps1) | Logistics map state и маркеры |
| [Test-LogisticsStatic.ps1](../tests/static/Test-LogisticsStatic.ps1) | Logistics lifecycle, graph, доставка и транзакции |
| [Test-ManualSupplyStatic.ps1](../tests/static/Test-ManualSupplyStatic.ps1) | Ручное снабжение и клиентские intents |
| [Test-MapPointOrdersStatic.ps1](../tests/static/Test-MapPointOrdersStatic.ps1) | Приказ в точку и waypoint |
| [Test-MovementRecoveryContracts.ps1](../tests/contracts/Test-MovementRecoveryContracts.ps1) | Ownership и границы movement recovery |
| [Test-InfantryMoveHandlerContracts.ps1](../tests/contracts/Test-InfantryMoveHandlerContracts.ps1) | Method-scoped guards, fail-closed registered sentinel и семь отрицательных мутаций; `-RuntimeLogPath` вместе с `-ManifestPath` проверяет все 25 cases полного завершённого probe и точный CLI |
| [Test-NorthFailureContracts.ps1](../tests/contracts/Test-NorthFailureContracts.ps1) | Recovery regression для failed movement inputs |
| [Test-PersonalLoadoutContracts.ps1](../tests/contracts/Test-PersonalLoadoutContracts.ps1) | Личные recipes, identity, валидация и RPC |
| [Test-RankRestrictionsStatic.ps1](../tests/static/Test-RankRestrictionsStatic.ps1) | Rank gates; опционально -PolicyPath |
| [Test-RecoveryEpisodeContracts.ps1](../tests/contracts/Test-RecoveryEpisodeContracts.ps1) | Episode fencing и synthetic logs; дополнительные runtime switches в param |
| [Test-RecruitmentSupplyContracts.ps1](../tests/contracts/Test-RecruitmentSupplyContracts.ps1) | Прогноз казарм и отрицательные мутации supply model |
| [Test-RecruitmentPriorityContracts.ps1](../tests/contracts/Test-RecruitmentPriorityContracts.ps1) | Приоритет, FIFO, pending demand, preemption и повторный gate перед оплатой; отрицательные мутации |
| [Test-RecruitmentSequenceContracts.ps1](../tests/contracts/Test-RecruitmentSequenceContracts.ps1) | Владелец очереди фракции, первый шаг 5000 ms, spawn/debit fences и последовательный прогноз; отрицательные мутации |
| [Test-RHSIntegrationStatic.ps1](../tests/static/Test-RHSIntegrationStatic.ps1) | RHS dependencies и compatibility adapters |
| [Test-RuntimeLauncherStatic.ps1](../tests/static/Test-RuntimeLauncherStatic.ps1) | Manifests, readiness и отказы preflight без native запуска |
| [Test-ScenarioHeadersStatic.ps1](../tests/static/Test-ScenarioHeadersStatic.ps1) | Inherited headers и сценарные resources |
| [Test-SquadCommandsContracts.ps1](../tests/contracts/Test-SquadCommandsContracts.ps1) | Squad intents и synthetic recruitment logs |
| [Test-SquadRespawnContracts.ps1](../tests/contracts/Test-SquadRespawnContracts.ps1) | Respawn/possession и authority |
| [Test-Stage35RecoveryPolicy.ps1](../tests/static/Test-Stage35RecoveryPolicy.ps1) | Recovery policy, bounded retries и accounting |
| [Test-Stage35Static.ps1](../tests/static/Test-Stage35Static.ps1) | Stable slots, identity и meaningful task proof |
| [Test-Stage3Static.ps1](../tests/static/Test-Stage3Static.ps1) | Vehicle ownership, flows, cleanup и handoff |
| [Test-Stage3StaticContracts.ps1](../tests/contracts/Test-Stage3StaticContracts.ps1) | Parser regression и отрицательные vehicle/source mutations |
| [Test-Stage4Static.ps1](../tests/static/Test-Stage4Static.ps1) | Economy, strategic snapshot и authority/RPC |
| [Test-SupplyMapUIStatic.ps1](../tests/static/Test-SupplyMapUIStatic.ps1) | UI карты снабжения и authority state |
| [Test-VictoryRespawnStatic.ps1](../tests/static/Test-VictoryRespawnStatic.ps1) | Victory и полная замена группы |
| [Test-WCSIntegrationStatic.ps1](../tests/static/Test-WCSIntegrationStatic.ps1) | WCS profile, content и addon graph |

## Анализаторы логов

Статус всех анализаторов: поддерживаемые, но полный runtime при реорганизации
**NOT RUN**. Synthetic inputs в contracts проверяют сам анализатор, не игру.
Передавай полные остановленные logs; `AllowActiveAtEnd` годится для диагностики
незаконченного запуска и не делает его итоговым runtime PASS. Дополнительные
пороговые значения и switches выбирай по `param` и сценарию, не ради PASS.

| Анализатор | Обязательный вход | Назначение |
|---|---|---|
| [Test-DefendAllocationLog.ps1](../tests/log-audits/Test-DefendAllocationLog.ps1) | `-LogPath`, `-LauncherPath`, `-NativeExitPath` | Exact CLI, native allocation, 128 late calls, 22 readbacks, full script/fatal gate; baseline отдельно через `-ExpectReproduction` |
| [Test-ShutdownLog.ps1](../tests/log-audits/Test-ShutdownLog.ps1) | `-LogPath`, `-LauncherPath`, `-NativeExitPath` | Stopped full log, script/fatal/leaks, memory snapshots и native cleanup; optional `-Batches`, `-RequireStop`, `-Client` |
| [Test-AICommanderModeLog.ps1](../tests/log-audits/Test-AICommanderModeLog.ps1) | `-ServerLogPath` + `-ExpectedMode` либо `-ExpectedInvalidValue` | CLI BOTH/US/USSR либо ожидаемое невалидное значение; optional client и initial coverage |
| [Test-BaseBuildersLog.ps1](../tests/log-audits/Test-BaseBuildersLog.ps1) | `-LogPath` | Worker completion, lifecycle и tool use; optional client |
| [Test-ConstructionLog.ps1](../tests/log-audits/Test-ConstructionLog.ps1) | `-LogPath` | Транзакции, identity, завершение; optional all types/factions |
| [Test-ConstructionSearchLog.ps1](../tests/log-audits/Test-ConstructionSearchLog.ps1) | `-LogPath` | Сценарии functional/dynamic/owner/provider/cancel/context/no-site |
| [Test-ForcedSmallBarracksLog.ps1](../tests/log-audits/Test-ForcedSmallBarracksLog.ps1) | `-LogPath` | Forced fallback и recruitment; optional probe и стороны |
| [Test-InfantryRecruitmentLog.ps1](../tests/log-audits/Test-InfantryRecruitmentLog.ps1) | `-LogPath` | Roster, muster, supply planning и supply bounds |
| [Test-LogisticsLog.ps1](../tests/log-audits/Test-LogisticsLog.ps1) | `-LogPath` | Physical delivery, ledger, recovery, fallback и exact client identity |
| [Test-PresetRespawnLog.ps1](../tests/log-audits/Test-PresetRespawnLog.ps1) | `-ServerLogPath`, `-ClientLogPath` | Сохранение preset и respawn через server/client pipeline |
| [Test-RecruitmentAcceptanceLog.ps1](../tests/log-audits/Test-RecruitmentAcceptanceLog.ps1) | `-LogPath`, `-Mode` | Матрица -Mode Lifecycle или Stability |
| [Test-RecruitmentPriorityLog.ps1](../tests/log-audits/Test-RecruitmentPriorityLog.ps1) | `-LogPath` | Полный остановленный лог priority fixture, порядок реальных покупок и точные списания |
| [Test-RecruitmentSequenceLog.ps1](../tests/log-audits/Test-RecruitmentSequenceLog.ps1) | `-LogPath` | Последовательность двух баз, первый/следующий шаг по timestamps, состав/оплата, освобождение и независимость сторон |
| [Test-RHSWardrobeInventoryLog.ps1](../tests/log-audits/Test-RHSWardrobeInventoryLog.ps1) | `-LogPath`, `-OutputDirectory` | Inventory showcase; сохраняет отдельный отчёт |
| [Test-Stage2Log.ps1](../tests/log-audits/Test-Stage2Log.ps1) | `-LogPath` | Приказы, repeated recovery, route replans и failed barracks visits |
| [Test-Stage4Log.ps1](../tests/log-audits/Test-Stage4Log.ps1) | `-LogPath` | Экономика, tickets/supplies и незавершённые операции |
| [Test-EquipmentLog.ps1](../tests/log-audits/Test-EquipmentLog.ps1) | `-LogPath`, `-Mode`, `-OutputDirectory`, `-ExpectedFamilies` | Full/inventory/safety/lifecycle/player/search; обязательные native cases, полный остановленный log, matrix и отдельная E/F-диагностика |

`Test-EquipmentContracts.ps1` проверяет authority, identities, реальные transfers,
receipts/rollback, ограниченное recovery и освобождение собственной movement lock;
16 правил и 11 отрицательных source mutations. Он не заменяет runtime.
Порядок staging и scope probes — [TEST_FIXTURES.md](TEST_FIXTURES.md),
текущие результаты — [TESTING.md](TESTING.md).

## Сложность гарнизонов FIA

`tests/contracts/Test-FIAGarrisonContracts.ps1` проверяет authority/identity,
полный roster, отсутствие замены и наследование Medium/Hard headers.
`tests/contracts/Test-FIARandomGarrisonContracts.ps1` добавляет границы PRNG plan,
JIP state, catalog, topology и rollback radio setup для issue #25.
`tests/log-audits/Test-FIARandomGarrisonLog.ps1 -LogPath <console.log>` анализирует
текущий `AICF_FIARandomGarrisonProbe.c`; для режима plan нужен `-PlanOnly`.
Targeted probe и полный SCRIPT/fatal gate различаются в выводе, ошибки сохраняют exit 1.
Следующий анализатор исторический, для фиксированного состава до issue #25:
`tests/log-audits/Test-FIAGarrisonLog.ps1 -LogPath <полный-остановленный-console.log>
-Difficulty 0|1|2` анализирует прогон `AICF_FIAGarrisonProbe.c` на North WCS+RHS:
состав 0/5/10, экипажи 10/3, физический локальный патруль, захват и casualty. Отдельно сообщает
число остальных engine/resource errors; PASS не означает отсутствие этих ошибок.

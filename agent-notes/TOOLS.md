# Утилиты и тестовые скрипты

Этот каталог описывает поддерживаемые файлы, их назначение и границы проверки.
Выбор тестов по игровому домену — в [TESTING.md](TESTING.md), запуск игры и
Workbench — в [DEVELOPMENT.md](DEVELOPMENT.md). Наличие файла в Git или статус
«поддерживаемый» не означает PASS. Новый verdict требует собственного evidence.

## Структура и статус

`Invoke-AICFQuotaCatalogProbe.ps1` — public PowerShell7 runner с отдельными
`Prepare`, `Server`, `Client`, `Audit` фазами. Фиксирует весь raw payload,
Workbench/source/CLI и full stopped logs; canonical direct-connect client
требует точный живой Server и ROSTER_READY. `-Case group|character|vehicle`
не подменяет необходимость реального совместимого native producer/slot.
Текущий v17: native0/0/fatal0/source0, но funding consumer readback0 и
functional/resource/other FAIL; покупка/full-basket native positive не доказаны.
Команды, контролируемый native funding и точные ограничения —
[AI_QUOTA_CATALOG.md](AI_QUOTA_CATALOG.md).

`Invoke-AICFQuotaStockPressure.ps1` — PowerShell7 Prepare/Run/Full для настоящего
заполнения STOCK и обычного оплаченного найма AICF. Сохраняет все7raw addons,
две fixture, frozen auditor, compile/source/CLI/CIM receipts и полные stopped
logs; errors/FPS остаются отдельными строгими gates. Команды и узкий scope —
[AI_QUOTA_STOCK_PRESSURE.md](AI_QUOTA_STOCK_PRESSURE.md).

`Invoke-AICFQuotaCompartmentProbe.ps1` — отдельный PowerShell7 runner реального
`BaseCompartmentSlot.SpawnCharacterInCompartment`. `-Phase Prepare` сохраняет
точные bytes всех семи addons (кроме generated.rdb), late fixture, frozen auditor,
canonical CLI и терминальный Workbench. `-Phase Run` использует тот же проверенный
stage/manifest и запрещает повторный native launch. Пример с новым EvidenceRoot:

```powershell
pwsh -File tools/Invoke-AICFQuotaCompartmentProbe.ps1 -Phase Prepare -EvidenceRoot .codex-runtime/compartment-new -Variant Stock -ServerPort 2680
pwsh -File tools/Invoke-AICFQuotaCompartmentProbe.ps1 -Phase Run -EvidenceRoot .codex-runtime/compartment-new -Variant Stock -ServerPort 2680
```

`-SourceRoot` предназначен для явно указанного fixture-free frozen payload;
его точные файлы/SHA записываются в manifest. После runtime проверяются все
source bytes и полные stopped sibling logs. Functional proof, fatal и все
остальные native `(E/F)` считаются раздельно; ошибки ресурсов не становятся PASS.
Сохранённый `native-initial-wait-v9`: exact382 Stock source,12functional checks
PASS/native0/fatal0/source0, отдельно strictFAIL/resources28. Это не whole383,
RHS, Catalog/payment или ordinary1800 proof. Fixture и oracle описаны в
[TEST_FIXTURES.md](TEST_FIXTURES.md#персонаж-в-compartment-и-квоты).

`Invoke-AICFVehicleEconomyDecline.ps1` запускает отдельные controlled
`INSUFFICIENT`, `AFFORDABLE`, `DRAIN_AFTER_RESERVE` cases. Сохраняет exact production
set/SHA, terminal Workbench, canonical CLI, frozen auditor и полные stopped logs.
`SkipCompile` повторно связывает stage с реально компилировавшимся source manifest;
native gameplay success не отменяет resourceFAIL. Команды и границы доказательства
описаны в [VEHICLE_ECONOMY_DECLINE.md](VEHICLE_ECONOMY_DECLINE.md).
E2 targeted evidence:114actual/4business mutants/2fixture mutations/35log cases,
runner16actual/11mutants; три остановленных AFTER native cases имеют
functional/fatalPASS и отдельно resources28FAIL. После настоящего request Reset
live `MatchesIdentity` возвращает false; fixture связывает immutable receipt,
проверенный до handoff. Эти узкие результаты не заменяют общий offline gate.

`Invoke-AICFSpawnApproachFailure.ps1` создаёт isolated controlled UNKNOWN case;
`Test-SpawnApproachFailureLog.ps1` отделяет functional outcome от strict native
resources. Callback/fallback не снимает resource FAIL. Scenario runner
`-Cases repeat5 -Faction US` проверяет оба рейса native Humvee; default — USSR.
Multi opt-in `-RecruitmentNavDomainProbe` снимает read-only paths/physics/LOD
при подходе к казарме. Runners сохраняют immutable hashes и полный native CLI.

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

`Invoke-AICFVehicleBehavior.ps1` с новым `-EvidenceRoot`, списком
`-Cases transport,combat`, стороной `-Faction US|USSR` и своим `-ServerPort` —
отдельный terminal controlled runner для
TRANSPORT/COMBAT. Он сохраняет точные checkout bytes и SHA256 production/двух
fixtures, компилирует isolated stage, запускает canonical Server и проверяет
полные stopped logs через `Test-VehicleBehaviorLog.ps1`. Targeted functional,
strict native resources и overall verdict хранятся раздельно в `summary.json`.
Это не30минутная обычная война и не часть offline All. Для controlled cleanup
недееспособного hull blocker использовать отдельный `-Cases combat` с
`-CleanupIncapProbe`; два cases с этой опцией runner отклоняет. Физическое
освобождение, неизменные life/health,3 attempts/deadline и release подтверждаются
более поздними native readbacks. Контракт и подготовка fixture —
[VEHICLE_BEHAVIOR.md](VEHICLE_BEHAVIOR.md), [TEST_FIXTURES.md](TEST_FIXTURES.md).

В offline All автоматически входят новые `Test-VehicleBehaviorSettingsContracts.ps1`,
`Test-VehicleBehaviorFlowContracts.ps1`, `Test-VehicleBehaviorNativeContracts.ps1`,
`Test-VehicleBehaviorLifecycleContracts.ps1`, `Test-VehicleBehaviorLogContracts.ps1`,
`Test-VehicleCapturedTopologyContracts.ps1`, `Test-DegradedVehicleCrewContracts.ps1`,
`Test-VehicleFitRosterContracts.ps1`, `Test-VehicleShortRecoveryContracts.ps1` и
`Test-CleanupIncapacitatedHullContracts.ps1`, `Test-CleanupAliveHullContracts.ps1`. Они проверяют server schema и
редактор, captured roster/seats, effective crew, scoped native policy, ownership
и конечные бюджеты. FitRoster исполняет извлечённые production identity,
unique-seat и cargo-fill тела; cleanup contract исполняет native-state predicate
на комбинациях состояния. Это не замена Workbench/native gates. Свежий focused
fit roster receipt —
`.codex-runtime/vehicle-behavior-20261028/fit-roster-after-v3/REPORT.md`; итоговый
compile/runtime конечного snapshot подводится отдельно и здесь не отмечен PASS.

Для server/client/JIP settings readback применяется прежний client probe с
дополнительным `-aicfVehicleBehaviorReadback 1` на всех peers и
`Test-VehiclePresetClientLog.ps1 -BehaviorSettings`. Проверяются malformed N без
RPC, COMBAT/437 в Save/Apply/reload и подтверждённое JIP состояние. Native
widgets не подтверждают видимость/читаемость или ввод мышью; ручные пункты
[VEHICLE_VISUAL_CHECK.md](VEHICLE_VISUAL_CHECK.md) остаются NOT RUN.

`Invoke-AICFTransportDamageFallback.ps1` — отдельный native controlled runner
повреждения колёс, пешего fallback и запрета повторной покупки на том же intent.
Использует immutable snapshot, terminal Workbench и canonical runtime launcher.
Его targeted PASS сохраняет отдельный strict-native-resource FAIL в summary;
это не обычная поездка и не общий runtime PASS. Контракты входят в offline All.

`Invoke-AICFVehicleMultiSquad.ps1` запускает ступени2/4/6 на сторону в NORMAL
или отдельно маркированном CAPACITY; `Watch-AICFVehicleSoak.ps1` наблюдает оба
формата heartbeat/failure. `Invoke-AICFTransportCombatResume.ps1` проверяет
fault-injected native hold/release/resume. Это terminal runtime runners вне
offline All. Команды, бюджет и ограничения — [VEHICLE_MULTISQUAD.md](VEHICLE_MULTISQUAD.md).
Их fixture/log contracts входят в All. `Test-TransportCombatHoldContracts.ps1`
и `Test-TransportCombatTaskContracts.ps1` проверяют read-only proof и projection
в watchdog; `Test-RecruitmentApproachEndpointContracts.ps1` — bounded alternative
endpoint без подмены физического прибытия.
`Test-RecruitmentShortRecoveryContracts.ps1` проверяет immutable failure receipt,
существующий budget2 и повторённые fences перед переносом до8m;
`Test-RecruitmentShortRecoveryProbeContracts.ps1` проверяет отдельный controlled
native fixture. `Test-RecruitmentShortRecoveryLog.ps1` требует20 настоящих cases,
цепочку callback/physical hop/native approach/arrival/debit и полный остановленный
лог с manifest/native exit. Эти gates не заменяют новый ordinary1800s Soak после
изменения production; strict native resources сохраняют самостоятельный FAIL.
`Test-VehicleCrewOwnTransitionContracts.ps1` проверяет exact own occupant,
human/foreign отказ,3s предел и expiry-before-wait; `Test-VehiclePresetEntryTraceContracts.ps1`
проверяет немутирующий pre-cancel/deadline readback дверей и token identity.
`Test-VehiclePresetNativeSeatContracts.ps1` проверяет exact slot remap, native
clone/super boundary, weak lifecycle, stale/current traffic и чужие/неоднозначные
запросы. GroupTrace contracts также сохраняют raw path семантику без progress writes.

`Test-VehicleHealthTraceContracts.ps1` проверяет native leaf health readback,
повторный component identity fence, bounded hierarchy и интеграцию с прежней
GroupTrace cadence/failure snapshot. CombatResume contracts сохраняют отдельные
health/identity/cleanup причины и строгий итоговый FAIL при первичном отказе.

`tools/Invoke-AICFVehicleScenarios.ps1 -EvidenceRoot <новый каталог>` — отдельный
native runner шести полных Stock поездок; в offline All он не входит. Готовит
stage, выполняет Workbench, canonical Server и строгий `Test-VehicleScenarioLog.ps1`.
Cases/flags и формат group/member trace — [VEHICLE_SCENARIOS.md](VEHICLE_SCENARIOS.md).
Offline All включает `Test-VehicleScenarioLogContracts.ps1` (130 controls) и
`Test-GroupTraceContracts.ps1` (read-only/authority/rate/lifecycle и полный
physical roster/raw native path и supported associated-agent API,48 controls).

Для issue #78 runner автоматически включает `Test-VehiclePresetContracts.ps1`
(78 guards), `Test-VehiclePresetLogContracts.ps1` (24 controls),
`Test-VehiclePresetClientLogContracts.ps1` (12 controls) и
`Test-VehicleCatalogLogContracts.ps1` (13 controls).
`tests/log-audits/Test-VehiclePresetLog.ps1` принимает `-LogPath`,
`-LauncherPath`, `-NativeExitPath`; `-LibraryReload` выбирает restart-only oracle.
`-Lifecycle` и `-HumanProtection` включают расширенные обязательные cases.
`-Policy` проверяет manual/automatic admission, `-Civilian` — civilian physical
model и CLI. `Test-VehicleCatalogLog.ps1` использует те же три path parameters,
но требует отдельный `-aicfVehicleCatalogProbe 1`,21 checks и own/CIV totals.
`Test-VehiclePresetClientLog.ps1` принимает пары `-ServerLog/-ServerLauncher/
-ServerExit`, `-ClientLog/-ClientLauncher/-ClientExit` и
`-JIPLog/-JIPLauncher/-JIPExit`. Проверяются все соседние полные logs и native
завершение. Script errors, в том числе serializers, не фильтруются как baseline.
`-CatalogFilters` включает28 client/5 JIP oracle и actual CLI проверки фильтров.
Flags fixtures и порядок stage — в [VEHICLE_PRESETS.md](VEHICLE_PRESETS.md).
`Test-VehiclePresetTransitLogContracts.ps1` добавляет32 controls для
`tests/log-audits/Test-VehiclePresetTransitLog.ps1` (`-LogPath`, `-LauncherPath`,
`-NativeExitPath`):16 native cases, exact roster5 в UAZ,30s ожидания, пауза бюджета,
реальное движение >=25m без recovery, высадка после движения, один purchase125.
`Test-VehicleCrewReturnLogContracts.ps1` добавляет32 positive/negative controls.
`tests/log-audits/Test-VehicleCrewReturnLog.ps1` принимает Server/Client пары
Log/Launcher/Exit, требует15 UI +8 server checks, неизменный inventory,9 разных
physical arrivals и один purchase50. Full stopped-log fatal gate не ослаблен.

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

Runner запускает дочерние Windows PowerShell5.1 `powershell.exe`
последовательно, даже если сам вызван из PowerShell7. Изменённый contract
проверяй через runner либо этим же `powershell.exe`; успешный прямой запуск
из PowerShell7 не подтверждает совместимость с canonical offline набором
(например, `ConvertFrom-Json -AsHashtable` отсутствует в5.1).
Некоторые contracts
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
| [Update-AICFScenarioRegistry.ps1](../tools/Update-AICFScenarioRegistry.ps1) | Генерация runtime registry карты/контента, native targets и addon graph из каталога; `-Check` проверяет актуальность без записи. Legacy Co-op headers удалены |
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

Для issue #66 `Test-LoadoutQolContracts.ps1` проверяет разделение Save/Apply,
scope/revisions, admin boundary, batch atomicity и независимые bindings;
`Test-LoadoutQolLogContracts.ps1` — полноту и негативные мутации runtime oracle.
Оба автоматически входят в All/Contracts. `Test-LoadoutLibraryContracts.ps1`
проверяет shared library/junction, lease, hashes и сохранность файлов между
свежими profiles. `tools/AICF_LoadoutLibrary.ps1` владеет filesystem mount/import
для canonical launcher; JSON schema остаётся у LoadoutStore. Отдельный
`tests/log-audits/Test-LoadoutQolLog.ps1` читает полный stopped server/client log,
manifest и native exit; игра не входит в offline runner. Контракт и запуск:
[LOADOUT_PRESETS.md](LOADOUT_PRESETS.md).

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

Дополнительные контракты транспортного продолжения #78:

| Проверка | Контракт |
|---|---|
| `Test-PlayerPersistenceIdentityContracts.ps1` |32 исполняемых комбинации production callback: пустая backend identity снимает tracking без удаления данных; непустая делегируется stock |
| `Test-GroupSpawnDispatchContracts.ps1` | Read-only native expansion/dispatch counters и строгий oracle controlled replacement stress |
| `Test-NativeSpawnAdmissionContracts.ps1` | Исполняемые границы global native budget и admission до reservation/generation; conservative pending roster |
| `Test-VehicleNativeSubgroupContracts.ps1` | Exact owned Pilot/record, отказ positive mismatch и ограниченный native reconcile уже сидящего экипажа |
| `Test-NativeGateRecoveryContracts.ps1` | Исполняемые отказ/допуск после доказанного native return, прежний mobility budget и исключительно route rebuild |
| `Test-StagingActorTaskContracts.ps1` | Точный selected/executed RUNNING actor task учитывается как работа при исчезнувшем group waypoint; deadlines не продлеваются |
| `Test-StagingActorTaskLogContracts.ps1` | Native withdraw-group-waypoint, stale/foreign/cancelled negatives и фактическое прибытие исходных бойцов |
| `Test-VehicleSoakObservationContracts.ps1` | Proof4 ordinary-war observation: causal casualty receipt, bounded cleanup/replacement и неизменные полные native gates |
| `Test-OrphanTripRetirementContracts.ps1` | Отдельная immutable absent-group identity, exact DEAD/native seat и повторные human/foreign/player/lease guards |
| `Test-CapturedDeathRetirementProjectionContracts.ps1` | Actual extracted classification/cause/lease epochs/monitor; prior failures сохранены, pending не PASS, исторический physical release |
| `Test-OrphanTripRetirementLogContracts.ps1` | Controlled native ledger: independent admission/DEAD/release/readiness/economy, early/original-operation Stop negatives |
| `Test-OrphanTripRetirementLog.ps1` | Разбор полного stopped controlled before/after, source freeze и всех sibling native/resource ошибок; baseline leak coverage отдельно |
| `Test-RecruitmentNativeAdmissionContracts.ps1` | Actual requested1 native budget, missing groups/donors, simultaneous requests, current authority/context и WAIT перед donor creation |
| `Test-RecruitmentCapacityLogContracts.ps1` | Негативы native pressure→35s WAIT без request/payment→restore→точный paid physical join; token/total visit deadline не меняются |
| `Test-RecruitmentCapacityLog.ps1` | Полный controlled donor native ledger, source freeze, stopped exit и все sibling fatal/resources отдельно |
| `Test-LooseWeaponObserverContracts.ps1` | Fixture-only наблюдение за drop/storage/world identities; static контракт не подтверждает безопасность native Physics getters |
| `Test-ReboardApproachProgressContracts.ps1` | Реальное приближение к exact seat; круг/отход/движение машины не продлевают stall observation |
| `Test-TransitMoveFailureContracts.ps1` | Exact owned native vehicle failure receipt, однократный callback и прежние bounded recovery attempts |
| `Test-TransitFailureRecoveryLogContracts.ps1` | Контролируемый fault требует двух реальных восстановлений движения, exact callback и отказа при исчерпанном budget |
| `Test-NativeCrewEntryContracts.ps1` | Exact own reserved seat и actual RUNNING GetIn, неизменяемый срок, оба role records до WAIT |
| `Test-VehicleBoardingSiteContracts.ps1` | Foot-navmesh coverage площадки до выбора и свежий повтор перед spawn |
| `Test-VehicleStagingHandoffContracts.ps1` | Единственный владелец движения при acquisition approach |
| `Test-VehiclePresetParallelEntryContracts.ps1` | Максимум два входа через доказанно независимые двери, unknown/shared/Turret serial |
| `Test-VehicleNativeRouteProgressContracts.ps1` | Полный текущий car path, projection/physics/history, proof-gated progress и rejection telemetry |
| `Test-PointHoldRepairAccountingContracts.ps1` | Terminal receipt до замены completed POINT hold, без ложного CONFIRMED |
| `Test-CompletedTransportIntentContracts.ps1` | Завершённый intent не переармляется от handoff revision; новые приказы и explicit admission сохранены |
| `Test-TransportNativeGateContracts.ps1` | Cargo/OpenGate, единственная native chain, физический выход/возврат/движение и неизменяемое60s окно |
| `Test-TransportNativeAbandonContracts.ps1` | Штатный current group GetOut exact машины приводит к typed fallback до driver recovery |
| `Test-NativeGateCompletionContracts.ps1` | Exact completion callback, COMPLETED→EVALUATED, listener cleanup при invalid stop |
| `Test-DismountOperationBudgetContracts.ps1` | Раздельные3+3 bounded batches, свежий logical egress до физического перемещения |
| `Test-VehicleHealthTraceContracts.ps1` | Повторный read-only leaf health/physics fence |
| `Test-TransportDamageTraceContracts.ps1` | Fixture-only causal callbacks, точный scope, однократный native super |

`aicfGroupTracePathPointLimit` оставляет default128, допускает диапазон128..512.
CombatResume runner явно выбирает512 для одной USSR/0; Multi сохраняет128.
`truncated=1` запрещает объявлять trace полным remaining path.

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

GC contracts: `Test-VehicleGarbageContracts.ps1` —54 source mutants для lease ownership, weak registry, cap-held cleanup states, exact restoration, stock callback chain и test-only server→client proxy preparation/teardown null guards/RPC guard receipts. `Test-VehicleGarbageProbeContracts.ps1` —33 fixture mutants, включая deferred native commit, teardown и TTL deletion. `Test-VehicleGarbageLog.ps1` требует полный stopped native GC log с manifest/exit. Controlled TTL5/TTL1 не заменяет свежий ordinary1800s Soak.

`Test-VehiclePresetClientLog.ps1 -BehaviorSettings` требует единственный actual
`VEHICLE_UI_GC_AUTHORITY` receipt и `GC_NO_AUTHORITY_REJECTED` на первом client
и JIP: native realm0/master0/proxy1, полный private Fleet/lease setup и оба
отказа GC permission. Matching contract содержит49 controls, включая native
master/отсутствующий proxy, server safe setup, совпадение RplId origin и exact
cleanup request, штатный EntityID format с optional literal ` {}` и strict teardown
NULL-pointer fatal. Zero identity и посторонний token отклоняются. Подготовка через fixture-only
owned RPC не меняет production protocol; local spawn setup FAIL не удаляется.
Для отдельного ALIVE cleanup native gate передаётся `-aicfCleanupAliveProbe 1`
в isolated behavior fixture; runner/helper обязан явно сохранить этот CLI,
полный stopped log и все6 `CLEANUP_ALIVE_*` receipts.

Source54/fixture33 mutants и49 client-log controls не заменяют свежие native
client/JIP logs. Server-local prefab оказался MASTER и не является proxy negative;
actual server-spawned RplId обязан соответствовать полученному client proxy.

### Manual staging progress proof (issue78,30.10)

`AICF_ManualStagingProbe.c` использует isolated scaffold
`AICF_VehicleBehaviorProbe.c` + `AICF_VehicleBehaviorSettingsProbe.c`; в stage
копируется как `ZZ_AICF_ManualStagingProbe.c`, после modded scaffold fragments.
Production checkout fixture не получает. CLI `-aicfManualStagingProbe 1`
с `-aicfManualStagingUnblockProbe 1` проверяет bounded native safeEscape,
actual landing→тот же native action→отдельный пеший ход→original5 rally→реальный
spawn/debit. Условный proof не заменяет ordinary/Multi gates.
`-aicfManualStagingExhaustionProbe 1` — отдельный explicit placement-veto case,
с обычной native-safe point целью≥200m и неизменным strict order-restore oracle.
Режимы не смешиваются. Аудитор `Test-ManualStagingLog.ps1 -Unblock|-Exhaustion`
выводит функциональный результат отдельно от строгого resourceFAIL.

`AICF_ManualStagingGeometryProbe.c`, CLI `-aicfManualStagingGeometryProbe 1`,
делает только present-world native geometry queries на явно test-only captured
poses. Не перемещает бойцов/машины, не меняет health/caps, не реконструирует
исторических dynamic occupants.91bounded forward candidates включают old9prefix;
physical3D≤8m/vertical≤1m/projection/pad/progress guards прежние. Контракт
`Test-ManualStagingGeometryProbeContracts.ps1` входит в auto-discovery checks.
Evidence `spawn/manual-historical-geometry-v2/` и `spawn/staging-final-frozen-hashes.json`.

`Test-VehicleSoakHeartbeatContracts.ps1` исполняет извлечённый nullgroup Update
branch и реальный heartbeat helper с API shims, включая actualNONE/alive0/WAIT
и сохранённую GROUP_UNAVAILABLE classification. Неизменный Proof4 auditor всё
так же отклоняет реальную паузу>60s. Добавочные heartbeatfields диагностические;
они не являются readiness, arrival или syntheticprogress proof.

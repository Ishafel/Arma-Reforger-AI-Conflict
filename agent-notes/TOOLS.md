# Утилиты и тестовые скрипты

Этот каталог описывает поддерживаемые файлы, их назначение и границы проверки.
Выбор тестов по игровому домену — в [TESTING.md](TESTING.md), запуск игры и
Workbench — в [DEVELOPMENT.md](DEVELOPMENT.md). Наличие файла в Git или статус
«поддерживаемый» не означает PASS. Новый verdict требует собственного evidence.

## Структура и статус

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

| Файл | Вход, результат и побочные эффекты |
|---|---|
| [Start-AICFRuntime.ps1](../tools/Start-AICFRuntime.ps1) | `-Role`, `-Variant`; foreground server/client, свежий profile, manifest и readiness. Полный контракт в DEVELOPMENT. `-DryRun` только строит план |
| [Invoke-AICFChecks.ps1](../tools/Invoke-AICFChecks.ps1) | Offline набор и evidence, контракт выше |
| [fetch_reforger_api_reference.sh](../tools/fetch_reforger_api_reference.sh) | Git Bash, сеть при отсутствии кэша; закреплённые version/commit/SHA-256, результат только в `.cache/reforger-api/` |
| [Build-AICFLocalization.ps1](../tools/Build-AICFLocalization.ps1) | `-RepositoryRoot`, `-Check`; без `-Check` обновляет производные localization файлы. С `-Check` проверяет соответствие |
| [AICFLocalization.Common.ps1](../tools/lib/AICFLocalization.Common.ps1) | Dot-source библиотека чтения и представления localization table |
| [Stage3StaticAudit.Common.ps1](../tests/lib/Stage3StaticAudit.Common.ps1) | Dot-source parser Enforce и общие vehicle rules/negative self-checks; используется несколькими аудиторами |
| [Measure-ConstructionSearch.ps1](../tools/Measure-ConstructionSearch.ps1) | `-LogPath`, `-OutputDirectory`; таблицы поиска/этапов, JSON/CSV/Markdown. Измерение не заменяет log gate |
| [Measure-ConstructionAllBases.ps1](../tools/Measure-ConstructionAllBases.ps1) | Те же параметры; snapshot полного лога, вызывает Search, строит статистику баз. Live snapshot допускается для наблюдения, не для итогового runtime verdict |
| [Start-AICFRHSWardrobeShowcase.ps1](../tools/Start-AICFRHSWardrobeShowcase.ps1) | `-Role Server|Client`, `-SessionRoot`; создаёт отдельный source stage EveronNorthRHS и копирует showcase fixture. Нужны RHS и generated databases. Оба peers — отдельными терминальными сессиями с одним SessionRoot; запуск через canonical launcher |
| [Watch-AICFRuntimeDiagnostics.ps1](../tools/Watch-AICFRuntimeDiagnostics.ps1) | `-Profile`, `-DeadlineSeconds` (650 по умолчанию); ищет сервер по profile и **принудительно останавливает** его при диагностической ошибке/дедлайне. Только для своего тестового server profile. Exit 0 — наблюдал выход; 2 — ошибка; 3 — дедлайн. Это не graceful shutdown и не runtime PASS |

## Локальные файлы вне набора

На момент реорганизации 2026-10-07 обнаружены четыре ignored/untracked файла.
Они сохранены по прежним путям, с отдельными точными ignore rules:

- `tools/Test-FIAPatrolContracts.ps1`
- `tools/Test-FIAPatrolLog.ps1`
- `tools/fixtures/AICF_FIAPatrolProbe.c`
- `tools/fixtures/AICF_ReplicationHierarchyProbe.c`

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

## Каталог offline проверок

Все файлы в таблице поддерживаются как отдельные gates. `Static` и `Contracts`
обозначают размещение, а не verdict. Имена ведут прямо к исходнику и его `param`.

| Проверка | Назначение |
|---|---|
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
| [Test-DefendWaypointInputContracts.ps1](../tests/contracts/Test-DefendWaypointInputContracts.ps1) | Входы native defend waypoint |
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
| [Test-NorthFailureContracts.ps1](../tests/contracts/Test-NorthFailureContracts.ps1) | Recovery regression для failed movement inputs |
| [Test-PersonalLoadoutContracts.ps1](../tests/contracts/Test-PersonalLoadoutContracts.ps1) | Личные recipes, identity, валидация и RPC |
| [Test-RankRestrictionsStatic.ps1](../tests/static/Test-RankRestrictionsStatic.ps1) | Rank gates; опционально -PolicyPath |
| [Test-RecoveryEpisodeContracts.ps1](../tests/contracts/Test-RecoveryEpisodeContracts.ps1) | Episode fencing и synthetic logs; дополнительные runtime switches в param |
| [Test-RecruitmentSupplyContracts.ps1](../tests/contracts/Test-RecruitmentSupplyContracts.ps1) | Прогноз казарм и отрицательные мутации supply model |
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
| [Test-AICommanderModeLog.ps1](../tests/log-audits/Test-AICommanderModeLog.ps1) | `-ServerLogPath` + `-ExpectedMode` либо `-ExpectedInvalidValue` | CLI BOTH/US/USSR либо ожидаемое невалидное значение; optional client и initial coverage |
| [Test-BaseBuildersLog.ps1](../tests/log-audits/Test-BaseBuildersLog.ps1) | `-LogPath` | Worker completion, lifecycle и tool use; optional client |
| [Test-ConstructionLog.ps1](../tests/log-audits/Test-ConstructionLog.ps1) | `-LogPath` | Транзакции, identity, завершение; optional all types/factions |
| [Test-ConstructionSearchLog.ps1](../tests/log-audits/Test-ConstructionSearchLog.ps1) | `-LogPath` | Сценарии functional/dynamic/owner/provider/cancel/context/no-site |
| [Test-ForcedSmallBarracksLog.ps1](../tests/log-audits/Test-ForcedSmallBarracksLog.ps1) | `-LogPath` | Forced fallback и recruitment; optional probe и стороны |
| [Test-InfantryRecruitmentLog.ps1](../tests/log-audits/Test-InfantryRecruitmentLog.ps1) | `-LogPath` | Roster, muster, supply planning и supply bounds |
| [Test-LogisticsLog.ps1](../tests/log-audits/Test-LogisticsLog.ps1) | `-LogPath` | Physical delivery, ledger, recovery, fallback и exact client identity |
| [Test-PresetRespawnLog.ps1](../tests/log-audits/Test-PresetRespawnLog.ps1) | `-ServerLogPath`, `-ClientLogPath` | Сохранение preset и respawn через server/client pipeline |
| [Test-RecruitmentAcceptanceLog.ps1](../tests/log-audits/Test-RecruitmentAcceptanceLog.ps1) | `-LogPath`, `-Mode` | Матрица -Mode Lifecycle или Stability |
| [Test-RHSWardrobeInventoryLog.ps1](../tests/log-audits/Test-RHSWardrobeInventoryLog.ps1) | `-LogPath`, `-OutputDirectory` | Inventory showcase; сохраняет отдельный отчёт |
| [Test-Stage2Log.ps1](../tests/log-audits/Test-Stage2Log.ps1) | `-LogPath` | Приказы, repeated recovery, route replans и failed barracks visits |
| [Test-Stage4Log.ps1](../tests/log-audits/Test-Stage4Log.ps1) | `-LogPath` | Экономика, tickets/supplies и незавершённые операции |

# Разработка и терминальный запуск

Работай из корня репозитория в Windows PowerShell/PowerShell 7. Нужны
Arma Reforger, Server и Tools одной версии, для API helper — Git for Windows.
Перед изменением запиши `git status`, branch/commit и фактические версии.
Создай отдельную ветку задачи по [AGENTS.md](../AGENTS.md).

Для полного offline набора из глубоко вложенного checkout используй временный
`subst` в свободной букве диска: legacy WinPS 5 `Copy-Item` в source fixtures
может превысить MAX_PATH. Например, после проверки свободного `X:` создай
`subst X: <checkout>` и запускай `X:/tools/Invoke-AICFChecks.ps1` с новым
EvidenceRoot. Сохрани mapping вместе с context/commands, после завершения
дочерних проверок удали только свой alias: `subst X: /D`. Production и auditors
при таком повторе не меняются. Для native server/client сохраняй canonical
launcher и проверку точного CLI/manifest, не подменяй их этим offline приёмом.

Назначение и статус утилит `tools/`, структура `tests/` и общий offline runner —
в [TOOLS.md](TOOLS.md). Runtime probes выбираются по [TEST_FIXTURES.md](TEST_FIXTURES.md).

## API reference

Закреплённый Script Diff — `1.8.0.13`, commit
`3d77cc212d5cda9922daf5f45635c7300d2d4cce`. Сначала ищи сигнатуру в
`.cache/reforger-api/Arma-Reforger-Script-Diff-1.8.0.13/`. При отсутствии кэша:

```powershell
& 'C:\Program Files\Git\bin\bash.exe' -c 'export PATH=/usr/bin:/mingw64/bin:$PATH; exec tools/fetch_reforger_api_reference.sh'
```

Кэш vendor-owned и не коммитится. Смена версии API — отдельная задача с новым
baseline и проверкой всех затронутых контрактов, а не повод угадывать сигнатуры.

## Workbench Validate/Compile

Только терминальный `ArmaReforgerWorkbenchSteamDiag.exe`; GUI automation,
Launcher, screenshots/video и управление окнами запрещены. Пример для stock Arland:

```powershell
$repoRoot = (Resolve-Path '.').Path
$toolsRoot = 'C:\Program Files (x86)\Steam\steamapps\common\Arma Reforger Tools'
$gameRoot = 'C:\Program Files (x86)\Steam\steamapps\common\Arma Reforger'
$logsDir = Join-Path $repoRoot ('.codex-runtime/workbench-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))

& "$toolsRoot\Workbench\ArmaReforgerWorkbenchSteamDiag.exe" `
  -noThrow -wbsilent `
  -gproj "$repoRoot\AIConflictArland\addon.gproj" `
  -addonsDir "$gameRoot\addons,$repoRoot" `
  -addons '9178E5822AFE48EA,B52C5F6AEDBF423E' `
  -logsDir "$logsDir" -wbModule=ScriptEditor -run -validate | Out-Null

$compileExit = $LASTEXITCODE
if ($compileExit -ne 0) { throw "Workbench exit=$compileExit; logs=$logsDir" }
```

`Out-Null` позволяет дождаться GUI-subsystem executable из PowerShell. Exit 0
проверяется вместе с полными логами: успешная validation/Game module и отсутствие
`SCRIPT (E/F)`, `ENGINE (F)`, VM/null exceptions. `SteamAPI_Init failed` или crash
не дают PASS. Resource/backend/shutdown errors классифицируются отдельно.

Для другого сценария получи точный source graph без запуска игры:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Start-AICFRuntime.ps1 `
  -Role Client -Variant EveronWCSRHS -DryRun
```

В JSON `AICF_RUNTIME_MANIFEST_JSON` массив `arguments` содержит пары `-gproj`,
`-addonsDir`, `-addons`. Перенеси **эти три значения** в Workbench-команду,
остальные Workbench flags сохрани; runtime flags ему не передаются. Client
manifest использует game resources, подходящие Workbench. При необходимости
задай реальные `-GameRoot`, `-ServerRoot`, `-RhsAddonsRoot`. DryRun допускает
отсутствие generated resource databases и не подтверждает compile/runtime.

## Сценарии и зависимости

| `-Variant` | Source root |
|---|---|
| `Stock` | `AIConflictArland` |
| `Everon`, `EveronNorth` | `AIConflictEveron` |
| `RHS` | `AIConflictArlandRHS` |
| `EveronRHS`, `EveronNorthRHS` | `AIConflictEveronRHS` |
| `ArlandWCSRHS` | `AIConflictArlandWCSRHS` |
| `EveronWCSRHS`, `EveronNorthWCSRHS` | `AIConflictEveronWCSRHS` |

Точный граф и ресурс `.conf` определяет каталог `tools/config/AICF_Scenarios.json`.
`-MatchMode Standard|CoopUS|CoopUSSR` выбирает режим для любого из девяти Variant;
default `Standard`. Стороны US/USSR логические: RHS использует USMC/МСВ,
WCS — НАТО/РФ. Старый `-Variant StockCoopUSSR` сохраняется как alias
`-Variant Stock -MatchMode CoopUSSR`; несовместимый явно переданный режим отклоняется.
Не заменяй сценарий raw `.ent`
с отдельным `-MissionHeader`: whitelist должен применяться до инициализации баз.
RHS/WCS версии и каталоги сверяй с установленными пакетами и addon dependencies;
сведения WCS: [Arland](../AIConflictArlandWCSRHS/README.md),
[Everon](../AIConflictEveronWCSRHS/README.md).

Для любого Variant и MatchMode используйте `-Difficulty Easy|Medium|Hard`
(default `Easy`), например `-Variant EveronNorth -Difficulty Hard`.
Передавайте одинаковые `Variant`, `MatchMode` и `Difficulty` серверу и клиенту: readiness gate проверяет
точный server header. Medium добавляет один БТР со случайным десантом до 2/4;
Hard — случайные пары бронетехники и одну усиленную точку с 3/4 машинами.
Недоступные танки заменяются БТР. Правила и подготовка графа описаны в
[FIA_GARRISONS.md](FIA_GARRISONS.md). Выбор сценария
в меню даёт тот же результат; параметры header определяют состав гарнизонов.

## Server и client

Запускай только через `tools/Start-AICFRuntime.ps1` в отдельных терминальных
сессиях. Helper работает в foreground; не создавай ad-hoc `Start-Process`.
Каждый запуск требует свежего profile и непустых generated `resourceDatabase.rdb`
для всех локальных addon выбранного graph.

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Start-AICFRuntime.ps1 `
  -Role Server -Variant Stock -AICommanderMode BOTH
```

Сохрани напечатанные `AICF_RUNTIME_PROFILE` и `AICF_RUNTIME_MANIFEST_JSON`.
Для локальной клиентской проверки укажи этот server profile и тот же Variant/MatchMode/Difficulty
в другой сессии:

```powershell
$serverProfileRoot = '<абсолютный AICF_RUNTIME_PROFILE работающего сервера>'
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Start-AICFRuntime.ps1 `
  -Role Client -Variant Stock -ServerProfileRoot $serverProfileRoot
```

Placeholder заменяется фактическим путём. Перед подключением launcher проверяет
свежий server log, точный `CLI Params`, живой server process и `[ROSTER_READY]`.
`-ClientAddress` задаёт адрес. Не обходи readiness gate ручным native запуском.

**`-ServerPort` не настраивает серверный listener.** Этот параметр используется
для клиентского подключения и readiness-проверки; роль `Server` не передаёт
его как настройку порта серверному executable. Например, `-ServerPort 2022`
сам по себе оставит stock listener на 2001, а клиент будет ждать 2022.
Указывай фактический порт работающего сервера (по умолчанию 2001). Другой порт
сначала должен быть отдельно настроен и подтверждён на сервере; разные значения
`-ServerPort` не изолируют параллельные серверы. Без отдельной настройки listener
запускай их последовательно.

`-AICommanderMode BOTH|US|USSR` задаётся при старте и не меняется в матче.
В Co-op launcher по умолчанию выбирает противоположную людям сторону;
явный несовместимый commander отклоняется. Пример за US на Севере Эверона WCS:

```powershell
& tools/Start-AICFRuntime.ps1 -Role Server -Variant EveronNorthWCSRHS -MatchMode CoopUS -Difficulty Medium
# В другой сессии, с фактическим свежим server profile:
& tools/Start-AICFRuntime.ps1 -Role Client -Variant EveronNorthWCSRHS -MatchMode CoopUS -Difficulty Medium -ServerProfileRoot $serverProfileRoot
```

Добавление нового сочетания карты/контента описано в [COOP_SCENARIO.md](COOP_SCENARIO.md).

Единый пункт «AI Conflict» открывается через Play/Host в меню: карта, установленный
контент, режим, сторона и сложность разрешаются в leaf header и точный addon graph.
Canonical launcher frontend не открывает: для server/client остаются
`-Variant Stock -MatchMode ... -Difficulty ...`. Явный CLI запуск setup через
`-world`/`-server` сохраняет vanilla Arland Co-op USSR Medium. Первый Workshop
Play открывает picker после штатной загрузки модулей; этот путь не требует
предварительно принудительно загруженного source graph. Core и Arland нужно
обновлять согласованной версией: меняются и hooks, и формат setup header.
Полный контракт и NOT RUN gates —
[SCENARIO_SETUP.md](SCENARIO_SETUP.md).

Для ручной проверки frontend владелец открывает клиент с базовым source graph,
но без `-client`/`-server`, и сам управляет меню. Граф берётся из Client `-DryRun`;
фактические argv и свежий внешний profile сохраняются в отдельном manifest.
Это открытие меню по запросу владельца, не direct connect и не GUI automation
агента. Диагностический `-listScenarios` печатает каталог из native Game API.
У универсального stock source graph — 31 штатная запись и один AI Conflict
`A1CF261019500001`; наличие файла в `.rdb` не заменяет этой проверки.
Для выбора всех установленных профилей используй `AIConflictArland/addon.gproj`
и базовый CSV `9178E5822AFE48EA,B52C5F6AEDBF423E`; добавь существующий каталог
RHS/WCS в `-addonsDir`, не в принудительный `-addons`. Принудительный WCS CSV
не выгружается native transition и закономерно блокирует выбор Vanilla/RHS.
Все local `.rdb` выбранных интеграций заранее генерируются терминальным Workbench.
Подключение к dedicated по-прежнему требует canonical launcher и readiness gate.

`-LoadoutLibraryPath <старый-profile>/profile/AICF_Loadouts` импортирует библиотеку
шаблонов в свежий server profile; manifest фиксирует файлы и SHA-256.

## Локальные Play/Host

Для главного меню и локального Play/Host используй `-Role Frontend`. Он запускает
`ArmaReforgerSteam.exe`, передаёт source graph без `-client`/`-server`/`-world`,
сохраняет `AICF_RUNTIME_MANIFEST_JSON`, наблюдает процесс и требует настоящий
native exit code. Установленный dedicated server для этого режима не нужен.
Пример с доступом picker к уже установленным Workshop пакетам:

```powershell
$workshopAddons = 'C:\Users\retar\OneDrive\Документы\My Games\ArmaReforger\addons'
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Start-AICFRuntime.ps1 `
  -Role Frontend -Variant EveronWCSRHS -RhsAddonsRoot $workshopAddons
```

`Variant` задаёт начальный graph. Пример загружает выбранный WCS/RHS профиль
сразу; это **не исправление native crash**: один из трёх повторов такого Host
тоже упал при завершении. Принудительный graph
ограничивает совместимые варианты picker существующим forced-CLI gate.
Явный `RhsAddonsRoot` добавляет каталог поиска и для Stock, не включает
и не скачивает моды. Для Vanilla без внешних пакетов его можно опустить.
Профиль остаётся свежим и вне source root; production resource databases
проверяются, как для Client. Параметры подключения и `-world` в Frontend
отклоняются: direct-connect остаётся ролью Client с её readiness gate.

Native shutdown crash 1.8.0.13 воспроизведён без production scripts AICF,
а затем и в обычном клиенте после первого Host со сменой graph. Переключение
Diag/retail само по себе его не устраняет. Frontend нужен для проверки игрового
binary и его JSON поведения; внешний FAIL сохранён в [TESTING.md](TESTING.md).

## Удалённый source-сервер и Diag/retail

`-Role Client` по умолчанию запускает `ArmaReforgerSteamDiag.exe`. Для сервера
с обычным (retail) бинарником используй `-UseRetailClient`: он выбирает
`ArmaReforgerSteam.exe`, сохраняя addon graph. Смешивание Diag/retail отклоняется
движком с `isDevBinary value does not match`; совпадения номера версии недостаточно.
Локальная роль `Server` этого launcher выбирает `ArmaReforgerServerDiag.exe`;
`-UseRetailClient` не переключает серверный executable.

Для удалённого сервера используй `-RemoteReadinessProbe` вместо локального
`-ServerProfileRoot`. Сам `-ClientAddress` не переключает readiness на удалённый
хост. Подготовь доверенный локальный `.ps1` probe и согласованный source commit:

```powershell
$remoteAddress = '<IP удалённого сервера без порта>'
$remotePort = 2001 # Фактический, отдельно подтверждённый server listener
$expectedCommit = '<полный source SHA из 40 строчных hex-символов>'
$probePath = '<абсолютный путь к доверенному локальному readiness probe.ps1>'
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Start-AICFRuntime.ps1 `
  -Role Client -Variant Stock -UseRetailClient `
  -ClientAddress $remoteAddress -ServerPort $remotePort `
  -RemoteReadinessProbe $probePath -ExpectedSourceCommit $expectedCommit
```

Замени placeholders, выбери тот же `-Variant`, что на сервере; для Diag-сервера
опусти `-UseRetailClient`. На клиенте нужен подготовленный локальный source graph
с согласованными исходниками и внешними пакетами. Проверки локальных
`-ServerRoot`, `-GameRoot` и source resource databases сохраняются даже при
удалённом подключении; при нестандартной установке передай нужные пути.

Probe выполняется с правами пользователя и получает именованные параметры
`Address`, `Port`, `ExpectedSourceCommit`. Через аутентифицированный терминальный
канал он обязан проверить на удалённом хосте:

- текущий живой server process и фактический listener по указанному адресу/порту;
- точный `CLI Params`, свежие profile/log этого процесса и `[ROSTER_READY]`;
- развёрнутый source SHA и хеши исходников, сценарий и полный addon graph;
- совпадение версий игры и внешних RHS/WCS пакетов с клиентом и совместимый
  тип бинарников Diag/retail.

Проверяется действующий процесс с его текущими исходниками и логом. Статический
сохранённый JSON или копия старого лога не являются реализацией probe.
Адреса, SSH-настройки и конкретный probe ведутся в эксплуатационном репозитории;
пароли и приватные ключи не передаются в аргументах launcher.

Probe возвращает **один PowerShell-объект**, а не строку JSON. Не добавляй строки
диагностики в success pipeline рядом с этим объектом.

| Поле | Контракт |
|---|---|
| `ok` | Boolean `$true` только после всех удалённых проверок |
| `commit` | Проверенный source SHA, совпадающий с `ExpectedSourceCommit` |
| `address` | Проверенный адрес, совпадающий с `ClientAddress` |
| `port` | Проверенный порт listener, совпадающий с `ServerPort` |
| `process_id` | Положительный PID проверенного живого server process |
| `observed_at` | Числовое время наблюдения в Unix UTC seconds |
| `log` | Путь к полному текущему server log на удалённом хосте |

Launcher проверяет `ok`, SHA, адрес, порт, положительный PID и возраст ответа:
не более 30 секунд, допустимое опережение часов — до 5 секунд; `NaN`/Infinity
отклоняются. Исключение, отрицательный или неполный ответ прерывают подключение.
Сам launcher не читает удалённый `log` и не проверяет процесс по сети: эти проверки
принадлежат probe. Принятые PID/SHA/log печатаются в `REMOTE_SERVER_READY`;
сохрани их вместе с manifest и полными server/client logs.

`-DryRun` только печатает план: он не вызывает probe и не подтверждает readiness,
совместимость бинарников или успешное подключение. Контракты режимов проверяет
`tests/static/Test-RuntimeLauncherStatic.ps1`: retail executable, положительный ответ
probe и отказ для ложного/неполного, устаревшего/будущего ответа, неверных
SHA/адреса/порта и неположительного PID. Это статика без реального remote runtime.

## Завершение работы

Полные остановленные Workbench/server/client logs, manifest, команды, exit codes
и версии храни в игнорируемом `.codex-runtime/<задача>/` либо вне checkout;
ссылки и verdict — в отчёте задачи. Отфильтрованные `[AICF]` строки служат индексом.
Fixture добавляются только в отдельный stage и удаляются после проверки;
необходимое evidence сохраняется, ненужные stage и свои процессы убираются.

Обнови затронутые `agent-notes/`, выполни [проверки](TESTING.md) и закоммить
нужные файлы. При подготовке нового релиза создай `releases/<версия>.md`.
Workshop metadata/preview и package outputs в репозитории не хранятся;
публикация является отдельной задачей владельца, её не доказывает Git-тег.

Workshop `CreateAssetUpload` принимает Version только в формате трёх чисел
без ведущих нулей (кроме самого `0`): `0.2.2`. Суффиксы вроде `0.2.1a`
отклоняются ValidationError в `data.asset.version` и при cancel в
`data.assetVersionNumber`. Это отдельное поле publisher; после исправления
значения нужно начать новую загрузку, прежние retry сохраняют старые данные.
При публикации исправления первого входа из [0.2.2](../releases/0.2.2.md)
согласованно обнови **AIConflictCore** и **AIConflictArland**: setup flag в header
и bootstrap в Core должны принадлежать одному выпуску. Для скрытия старых
вариантов обнови также все используемые сценарные addons: hidden flag хранится
в их headers. Укажи `0.2.2` в Version publisher; после обновления полностью
закрой игру и проверь новый первый Play/Host и список «Сценарии».
Наличие Git-тега также не создаёт GitHub Release автоматически: публикация
должна завершаться проверкой страницы Release, notes, target и статуса Latest.

### Подготовка окончательного списка Workshop

В опубликованной 0.2.2 `asset.scenarios` всё ещё содержит старые пункты,
несмотря на hidden headers. После native bundling и до загрузки на backend
подготовь отдельную копию и проверь её:

```powershell
powershell.exe -NoProfile -File tools/Prepare-AICFWorkshopBundle.ps1 `
  -BundlePath '<native working dir с manifest.json>' -OutputPath '<новый каталог>'
powershell.exe -NoProfile -File tools/Prepare-AICFWorkshopBundle.ps1 `
  -BundlePath '<новый каталог>' -Check
```

Повтори для всех семи addons. Tool сохраняет version; новую номерную версию
согласуют отдельно. Подготовленные в evidence копии 0.2.2 служат воспроизведению
дефекта и не являются новой опубликованной версией. Нельзя повторно нажимать
обычный Publish, предполагая, что исправленный список сохранится: Workbench
пересобирает manifest. Native CLI имеет `publishAddonDir` для подготовленных
данных, однако его фактическое использование этой metadata и приём backend
в этой задаче **NOT RUN**. Любая публикация требует отдельного запроса владельца
и проверки результата на backend; не вызывай uploader как «сухую проверку».

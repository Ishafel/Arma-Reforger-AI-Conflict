# Пользовательский гайд: запуск из игры и dedicated server

## Что создаёт этот гайд

Инструкция описывает два пути: ручной запуск через встроенное меню `Сценарии`
и локальный development server Arma Reforger Conflict на Arland с исходными
аддонами `AIConflictCore` и `AIConflictArland` либо их опциональным RHS
root-addon `AIConflictArlandRHS`. Сервер запускается из PowerShell напрямую
через `ArmaReforgerServerDiag.exe`, без Launcher, Host UI, Workbench GUI и
скриншотов.

Это пока не готовая схема публичного Workshop-сервера. Репозиторий содержит
Workshop metadata и release checklist, но packaged build ещё должен быть
впервые вручную опубликован по [`docs/PUBLISHING.md`](PUBLISHING.md). До этого
для подключения клиент должен иметь тот же checkout исходников и запускаться
с теми же addon GUID. Публичный сервер для произвольных игроков потребует
публикации обоих аддонов.

## Что потребуется

- Windows 10/11 x64 и PowerShell.
- Checkout этого репозитория на машине сервера.
- `Arma Reforger Server`.
- `Arma Reforger` для локального клиента.
- `Arma Reforger Tools` для проверки скриптов перед запуском.
- Одинаковые версии Game, Server и Tools.

Проект и локальный API reference сейчас ориентированы на версию `1.8.0.10`.
Переход на другую версию требует отдельного Workbench Validate и проверки
совместимости Enfusion API.

Команды ниже предполагают стандартную Steam Library:

```text
C:\Program Files (x86)\Steam\steamapps\common\Arma Reforger
C:\Program Files (x86)\Steam\steamapps\common\Arma Reforger Server
C:\Program Files (x86)\Steam\steamapps\common\Arma Reforger Tools
```

Если игры установлены в другой Steam Library, измени только переменные
`$gameRoot`, `$serverRoot` и `$toolsRoot`.

## Запуск из меню сценариев

Оба сценария AICF отключают player-rank gates для строительства и отдельных
построек, заказа техники, арсенала, loadouts, групп, защитников, radial commands
и Commander volunteer. Всем игрокам назначается XP floor стартового звания
`GENERAL`. В Reforger 1.8 stock/default `RankContainer` заканчивается на
`MAJOR`; для контейнера без отдельной записи `GENERAL` effective floor равен
его максимальному non-renegade XP-порогу. Authoritative server повторно
применяет floor после каждого изменения XP, reconnect/JIP и восстановления
persistence state: штрафы сначала уменьшают накопленный избыток, но не могут
понизить итоговый XP ниже floor, а replicated character state остаётся
`GENERAL`. Ограничения по supply, cooldown, принадлежности фракции, capacity,
authority и другим правилам Conflict сохраняются.

После установки и включения packaged `AI Conflict Arland` открой в игре
`Сценарии` и выбери `AI Conflict - Arland`. Для RHS установи и включи
`AI Conflict Arland RHS` со всеми dependencies, затем выбери
`AI Conflict RHS - Arland`.

Пока Workshop build не опубликован, плитку можно проверить из checkout через
Diag-клиент. Открой PowerShell в репозитории и выполни stock-команду:

```powershell
$repoRoot = (Resolve-Path '.').Path
$gameRoot = 'C:\Program Files (x86)\Steam\steamapps\common\Arma Reforger'
$scenarioProfile = Join-Path $env:LOCALAPPDATA ('AICF\ScenarioMenu-Stock-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))

& "$gameRoot\ArmaReforgerSteamDiag.exe" `
  -gproj "$repoRoot\AIConflictArland\addon.gproj" `
  -addonsDir "$repoRoot,$gameRoot\addons" `
  -addons '9178E5822AFE48EA,B52C5F6AEDBF423E' `
  -profile "$scenarioProfile" `
  -backendFreshSession
```

Для RHS добавь установленный RHS-каталог и используй его root project:

```powershell
$rhsRoot = 'C:\Users\retar\OneDrive\Документы\My Games\ArmaReforger\addons'
$rhsScenarioProfile = Join-Path $env:LOCALAPPDATA ('AICF\ScenarioMenu-RHS-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))

& "$gameRoot\ArmaReforgerSteamDiag.exe" `
  -gproj "$repoRoot\AIConflictArlandRHS\addon.gproj" `
  -addonsDir "$repoRoot,$gameRoot\addons,$rhsRoot" `
  -addons '9178E5822AFE48EA,B52C5F6AEDBF423E,1337C0DE5DABBEEF,BADC0DEDABBEDA5E,595F2BF2F44836FB,9F88011DA22B471C' `
  -profile "$rhsScenarioProfile" `
  -backendFreshSession
```

После появления главного меню пользователь вручную открывает `Сценарии` и
выбирает нужную плитку. В RHS graph видна также stock-плитка из dependency;
для RHS запуска выбирай именно `AI Conflict RHS - Arland`. Для stock не
подключай `AIConflictArlandRHS`.

Запуск из меню использует default `aicfAICommanderMode=BOTH`. Выбор только
одного AI commander (`US` или `USSR`) доступен в dedicated server через CLI.
Codex не проверяет визуальную плитку или controls через GUI: такой критерий
остаётся `NOT RUN` до ручного verdict пользователя.

## 1. Открой PowerShell в репозитории

```powershell
Set-Location 'C:\path\to\Arma-Reforger-AI-Conflict'
$repoRoot = (Resolve-Path '.').Path
```

Замени путь на фактический абсолютный путь checkout. Все дальнейшие команды
выполняются из этого каталога.

## 2. Проверь установку и версии

```powershell
$serverRoot = 'C:\Program Files (x86)\Steam\steamapps\common\Arma Reforger Server'
$gameRoot = 'C:\Program Files (x86)\Steam\steamapps\common\Arma Reforger'
$toolsRoot = 'C:\Program Files (x86)\Steam\steamapps\common\Arma Reforger Tools'

$serverExe = Join-Path $serverRoot 'ArmaReforgerServerDiag.exe'
$clientExe = Join-Path $gameRoot 'ArmaReforgerSteamDiag.exe'
$workbenchExe = Join-Path $toolsRoot 'Workbench\ArmaReforgerWorkbenchSteamDiag.exe'

$requiredPaths = @(
  $serverExe,
  $clientExe,
  $workbenchExe,
  (Join-Path $repoRoot 'AIConflictCore\addon.gproj'),
  (Join-Path $repoRoot 'AIConflictArland\addon.gproj')
)

$missingPaths = $requiredPaths | Where-Object {
  -not (Test-Path -LiteralPath $_)
}

if ($missingPaths)
{
  throw "Не найдены обязательные пути:`n$($missingPaths -join "`n")"
}

$serverVersion = (Get-Item -LiteralPath $serverExe).VersionInfo.FileVersion
$clientVersion = (Get-Item -LiteralPath $clientExe).VersionInfo.FileVersion
$toolsVersion = (Get-Item -LiteralPath $workbenchExe).VersionInfo.FileVersion

[pscustomobject]@{
  Server = $serverVersion
  Client = $clientVersion
  Tools = $toolsVersion
} | Format-List

if ($serverVersion -ne $clientVersion -or $serverVersion -ne $toolsVersion)
{
  throw 'Версии Server, Client и Tools различаются.'
}
```

На текущем рабочем окружении все три файла имеют версию `1.8.0.10`.

## 3. Проверь скрипты через терминальный Workbench

Перед первым запуском, а также после любого изменения production `.c` или
Enfusion API, выполни terminal-only Validate/Compile:

```powershell
$validationStamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$workbenchLogs = Join-Path $env:LOCALAPPDATA "AICF\Workbench-$validationStamp"

& $workbenchExe `
  -noThrow `
  -wbsilent `
  -gproj "$repoRoot\AIConflictArland\addon.gproj" `
  -addonsDir "$gameRoot\addons,$repoRoot" `
  -addons '9178E5822AFE48EA,B52C5F6AEDBF423E' `
  -logsDir "$workbenchLogs" `
  -wbModule=ScriptEditor `
  -run `
  -validate

$workbenchExitCode = $LASTEXITCODE
"Workbench exit code: $workbenchExitCode"
"Workbench logs: $workbenchLogs"

if ($workbenchExitCode -ne 0)
{
  throw "Workbench Validate завершился с exit code $workbenchExitCode."
}
```

Проверь полный log. Успешный process exit сам по себе недостаточен: не должно
быть `SCRIPT (E/F)`, `ENGINE (F)`, `Virtual Machine Exception` или null errors.
Подробный compile contract находится в
[`TESTING.md`](TESTING.md#workbench-gate).

## 4. Проверь порт сервера

Canonical local server использует стандартный game port `2001/UDP`.

```powershell
$occupied = Get-NetUDPEndpoint -LocalPort 2001 -ErrorAction SilentlyContinue

if ($occupied)
{
  $occupied | Select-Object LocalAddress, LocalPort, OwningProcess
  throw 'UDP port 2001 уже занят.'
}
```

Для LAN/direct-IP подключения `2001/UDP` должен быть разрешён локальным
firewall. В этой raw-world схеме A2S и RCON не настроены. После отдельной
настройки для них открываются только выбранные порты; стандартные значения в
JSON config — `17777/UDP` для A2S и `19999/UDP` для RCON, а A2S можно настроить
и CLI-параметрами. Не открывай неиспользуемые порты.

Port forwarding, server-list visibility, name/password/admin и public address
выходят за рамки этого source-server guide: они требуют отдельной JSON config
и схемы упаковки/распространения аддонов.

## 5. Запусти сервер

Оставь этот PowerShell открытым: сервер работает в foreground, а полный вывод
остаётся доступен в терминале и profile logs.

```powershell
$runStamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$profileRoot = Join-Path $env:LOCALAPPDATA "AICF\Server-$runStamp"
"Server profile: $profileRoot"

& $serverExe `
  -gproj "$repoRoot\AIConflictArland\addon.gproj" `
  -server 'worlds/MP/CTI_Campaign_Arland.ent' `
  -MissionHeader 'Missions/AICF_Conflict_Arland.conf' `
  -worldSystemsConfig 'Configs/Systems/ConflictSystems.conf' `
  -addonsDir "$repoRoot,$serverRoot\addons" `
  -addons '9178E5822AFE48EA,B52C5F6AEDBF423E' `
  -profile "$profileRoot" `
  -backendFreshSession `
  -aicfAICommanderMode BOTH `
  -maxFPS 60 `
  -logStats 10000

$serverExitCode = $LASTEXITCODE
"Server exit code: $serverExitCode"
```

Обязательные части команды:

| Параметр | Назначение |
|---|---|
| `-gproj` | загружает проект Arland; через dependency подключается Core |
| `-server` | запускает raw Arland Conflict world |
| `-MissionHeader` | подключает AICF header, наследующий штатную Campaign mission |
| `-worldSystemsConfig` | подключает штатные Conflict systems |
| `-addonsDir` | добавляет repo и vanilla server addons в пути поиска |
| `-addons` | загружает Core и Arland по неизменяемым GUID |
| `-profile` | отделяет логи и runtime state конкретного запуска |
| `-backendFreshSession` | начинает новую backend session |
| `-aicfAICommanderMode BOTH` | фиксирует command authority обеих фракций; без параметра default также `BOTH` |
| `-maxFPS 60` | ограничивает нагрузку dedicated server |
| `-logStats 10000` | пишет performance statistics раз в 10 секунд |

Не объединяй эту raw-world схему с `-config`: официальный `-server` mode
игнорирует JSON server config. Для постоянной JSON-конфигурации потребуется
отдельно адаптировать scenario и схему распространения аддонов.

## 6. Проверь успешный старт

Открой второй PowerShell в репозитории. Вставь точный `Server profile` path,
который первый терминал напечатал перед запуском, и найди log:

```powershell
$profileRoot = Read-Host 'Вставь Server profile path из первого терминала'
if (-not (Test-Path -LiteralPath $profileRoot))
{
  throw "Server profile не найден: $profileRoot"
}

$serverLogsRoot = Join-Path $profileRoot 'logs'
if (-not (Test-Path -LiteralPath $serverLogsRoot))
{
  throw "Каталог server logs ещё не создан: $serverLogsRoot"
}

$log = Get-ChildItem -LiteralPath $serverLogsRoot -File -Recurse -Filter 'console.log' |
  Sort-Object LastWriteTime -Descending |
  Select-Object -First 1

if (-not $log)
{
  throw "В server profile ещё не создан console.log: $profileRoot"
}

$log.FullName
Select-String -LiteralPath $log.FullName -Pattern `
  '\[AICF\]|Game successfully created|SCRIPT\s+\((E|F)\)|ENGINE\s+\(F\)|BACKEND\s+\((E|F)\)|Virtual Machine Exception|NULL pointer'
```

Признаки нормального bootstrap:

- процесс продолжает работать;
- `Get-NetUDPEndpoint -LocalPort 2001` показывает UDP endpoint процесса;
- log содержит `Game successfully created`;
- log содержит `[AICF][STAGE1] ... BOOTSTRAP_SERVER`;
- `CONFIG` содержит `ai_commander_mode`, `ai_commander_us` и
  `ai_commander_ussr`, затем для обеих фракций появляется
  `COMMAND_AUTHORITY_SET`;
- затем появляется `[AICF][STAGE1] ... MATCH_START`;
- после создания roster появляются `SPAWN_BOUND`, а примерно через минуту —
  `HEARTBEAT` events;
- отсутствуют fatal script/engine/VM/null errors.

Эти признаки подтверждают startup, но не заменяют длительный runtime verdict.
Всегда сохраняй полный `console.log`, а не только отфильтрованные `[AICF]`
строки.

Для live-наблюдения без GUI:

```powershell
Get-Content -LiteralPath $log.FullName -Wait |
  Where-Object {
    $_ -match '\[AICF\]|SCRIPT\s+\((E|F)\)|ENGINE\s+\(F\)|BACKEND\s+\((E|F)\)|Virtual Machine Exception|NULL pointer'
  }
```

## 7. Подключи локальный клиент

Клиент запускается из отдельного PowerShell и должен видеть тот же checkout
исходных аддонов:

```powershell
$repoRoot = (Resolve-Path '.').Path
$gameRoot = 'C:\Program Files (x86)\Steam\steamapps\common\Arma Reforger'
$clientExe = Join-Path $gameRoot 'ArmaReforgerSteamDiag.exe'
$clientStamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$clientProfile = Join-Path $env:LOCALAPPDATA "AICF\Client-$clientStamp"
"Client profile: $clientProfile"

& $clientExe `
  -gproj "$repoRoot\AIConflictArland\addon.gproj" `
  -client 127.0.0.1 `
  -addonsDir "$repoRoot,$gameRoot\addons" `
  -addons '9178E5822AFE48EA,B52C5F6AEDBF423E' `
  -profile "$clientProfile" `
  -backendFreshSession

$clientExitCode = $LASTEXITCODE
"Client exit code: $clientExitCode"
```

Codex может запустить client этой командой и анализировать его logs, но не
управляет окном, мышью или клавиатурой и не создаёт screenshots/video.
Визуальная проверка UI выполняется пользователем вручную либо остаётся
`NOT RUN`.

Для клиента на другой машине в той же LAN замени `127.0.0.1` на LAN IPv4
сервера. На клиентской машине должны находиться exact same commit обоих
аддонов; скорректируй `$repoRoot` и `$gameRoot` и предварительно выполни там
терминальный Workbench Validate, чтобы создать согласованную Resource Database.
Подключение произвольных Internet-клиентов этим source workflow не покрывается.

### RHS-вариант: USMC против MSV

Нужны RHS - Status Quo `0.16.5150` (`595F2BF2F44836FB`), Content Pack 01
(`1337C0DE5DABBEEF`) и Content Pack 02 (`BADC0DEDABBEDA5E`). Постоянный GUID
`AIConflictArlandRHS` — `9F88011DA22B471C`; stock Core/Arland от RHS не
зависят. Сначала выполни RHS Workbench-команду из `docs/DEVELOPMENT.md`.

Запуск dedicated server на штатной RHS mission:

```powershell
$repoRoot = (Resolve-Path '.').Path
$serverRoot = 'C:\Program Files (x86)\Steam\steamapps\common\Arma Reforger Server'
$serverExe = Join-Path $serverRoot 'ArmaReforgerServerDiag.exe'
$rhsRoot = 'C:\Users\retar\OneDrive\Документы\My Games\ArmaReforger\addons'
$rhsServerProfile = Join-Path $env:LOCALAPPDATA ('AICF\Server-RHS-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))

& $serverExe `
  -gproj "$repoRoot\AIConflictArlandRHS\addon.gproj" `
  -server 'Worlds/MP/Conflict/CTI_Campaign_Arland_RHS.ent' `
  -MissionHeader 'Missions/AICF_RHS_Conflict_Arland.conf' `
  -worldSystemsConfig 'Configs/Systems/ConflictSystems.conf' `
  -addonsDir "$repoRoot,$serverRoot\addons,$rhsRoot" `
  -addons '9178E5822AFE48EA,B52C5F6AEDBF423E,1337C0DE5DABBEEF,BADC0DEDABBEDA5E,595F2BF2F44836FB,9F88011DA22B471C' `
  -profile "$rhsServerProfile" `
  -backendFreshSession `
  -aicfAICommanderMode BOTH `
  -maxFPS 60 `
  -logStats 10000
```

Здесь обязательны именно RHS raw world и AICF RHS `MissionHeader`, который
наследует штатный RHS header. Stock world с RHS header оставляет faction keys
`US`/`USSR` и не является RHS runtime.
Корректный startup сообщает:

```text
[AICF][CONTENT][INFO][PROFILE_SELECTED] profile=RHS_USMC_MSV_0_16_5150 runtime_blufor=RHS_USAF stable_blufor=US runtime_opfor=RHS_AFRF stable_opfor=USSR
```

RHS-клиент запускается с тем же checkout и packages:

```powershell
$gameRoot = 'C:\Program Files (x86)\Steam\steamapps\common\Arma Reforger'
$clientExe = Join-Path $gameRoot 'ArmaReforgerSteamDiag.exe'
$rhsClientProfile = Join-Path $env:LOCALAPPDATA ('AICF\Client-RHS-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))

& $clientExe `
  -gproj "$repoRoot\AIConflictArlandRHS\addon.gproj" `
  -client 127.0.0.1 `
  -addonsDir "$repoRoot,$gameRoot\addons,$rhsRoot" `
  -addons '9178E5822AFE48EA,B52C5F6AEDBF423E,1337C0DE5DABBEDA5E,595F2BF2F44836FB,9F88011DA22B471C' `
  -profile "$rhsClientProfile" `
  -backendFreshSession
```

Перед verdict останови оба процесса и проверь полные server/client logs.
`GROUP_ROSTER_CONFIGURED` должен иметь RHS `profile`, `fallback_slots=0` и
USMC/MSV `prefabs`; vehicle `CATALOG` и `LIVE` capacity events должны указывать
только candidates из RHS profile. Внешний вид формы, оружия и UI проверяется
пользователем вручную, не через GUI automation Codex.

## 8. Параметры AICF

Vehicle и economy subsystems включены всегда. Параметры
`aicfVehiclesEnabled` и `aicfEconomyEnabled` больше не читаются, поэтому
server command не требует feature-enable flags.

Command authority выбирается одним exact-case параметром:

| CLI | Результат |
|---|---|
| параметр отсутствует | `BOTH`: AI commander для `US` и `USSR` |
| `-aicfAICommanderMode BOTH` | AI commander для `US` и `USSR` |
| `-aicfAICommanderMode US` | AI commander только для `US`; `USSR` управляется player orders |
| `-aicfAICommanderMode USSR` | AI commander только для `USSR`; `US` управляется player orders |

Mode неизменяем в течение матча. Чтобы поменять его, останови server и запусти
новый process; желательно использовать новый profile. `NONE`, пустое,
lowercase и любое неизвестное значение не поддерживаются: server пишет
`CONFIG_INVALID` в Arland bootstrap и отклоняет AICF startup до запуска
`AICF_ArlandRadioBridgeNormalizer`, его base-owner subscription/radio mutation,
MatchController roster и loops вместо fallback к `BOTH`. Bootstrap передаёт в
MatchController тот же предварительно проверенный объект config; CLI внутри
controller/policy не перечитывается.

У player-commanded стороны сохраняются все десять slots, tickets, economy,
vehicles, reliability, victory и UI. До valid player order её группы показывают
`AWAITING_PLAYER_COMMAND` и получают `SYSTEM_HOLD` на своей HQ. Player order
снимает ожидание; если target после capture становится недопустим, server
возвращает slot в hold, не выбирая другую базу автономно.

Два replicated authority availability flags позволяют подключившемуся позже
клиенту получить действующий mode. Пока все двадцать асинхронных initial slots
не перешли в `READY`, они намеренно равны `false/false`, и UI показывает
`COMMAND SYNC`. Затем `TryLogRosterReady()` публикует выбранную пару
(`BOTH=1/1`, `US=1/0`, `USSR=0/1`). При
`AICF_MatchController.Stop()` pair снова сбрасывается в `false/false` до снятия
domain subscriptions и cleanup; immutable server policy от этого не становится
runtime-настройкой.

`aicfExpectedPlayerFaction` влияет только на проверку результата и не должен
использоваться вместо `aicfAICommanderMode`.

Чтобы матч в `BOTH` мог зафиксировать результат без подключённого игрока,
добавь:

```text
-aicfRequirePlayerForResult 0
```

Не меняй все tuning-параметры одновременно при поиске ошибки. Сначала проверь
штатную always-on конфигурацию, затем выполняй отдельные targeted runs.

## 9. Останови сервер и сохрани evidence

Сначала нажми `Ctrl+C` в терминале client и дождись его выхода и печати
`$clientExitCode`. Если запущен отдельный live-tail, останови его `Ctrl+C`.
Затем вернись в терминал server, нажми `Ctrl+C` и дождись выхода и печати
`$serverExitCode`.

После остановки проверь полные logs ещё раз. Репозиторий не определяет graceful
shutdown contract: `Ctrl+C` завершает локальный process, но не гарантирует
запись всех final summaries. Не удаляй profiles до окончания анализа.

Обычный путь:

```text
%LOCALAPPDATA%\AICF\Server-YYYYMMDD-HHMMSS\logs\logs_YYYY-MM-DD_HH-MM-SS\console.log
```

Для каждого запуска сохрани:

- версию Game/Server/Tools;
- commit и `git status`;
- полный server command со всеми `aicf*` flags;
- Workbench/server/client exit codes;
- server/client profile paths;
- полный `console.log` и соседние logs;
- время запуска и остановки;
- отдельные verdict `compile`, `server`, `client`, `JIP`, `soak`;
- всё, что осталось `NOT RUN`.

## Типовые проблемы

| Симптом | Что проверить |
|---|---|
| Executable не найден | Steam Library path в `$serverRoot`, `$gameRoot`, `$toolsRoot` |
| Server сразу завершился | полный `console.log`, версии, `SCRIPT/ENGINE/VM` errors |
| Запустился обычный vanilla Conflict | оба GUID в `-addons`, repo в `-addonsDir`, Arland `-gproj` |
| Нет `MATCH_START` | raw world, `-MissionHeader`, `-worldSystemsConfig`, bootstrap errors |
| `CONFIG_INVALID` для command mode | exact uppercase `BOTH`, `US` или `USSR`; отсутствие лишних пробелов и `NONE` |
| Invalid mode всё же дал readiness/radio/roster events | regression: после `CONFIG_INVALID` не должно быть `CONFLICT_READY`, любого `RADIO_BRIDGE_*`, controller `CONFIG`, `MATCH_START` или spawn/roster events |
| UI показывает `COMMAND SYNC` во время bootstrap | ожидай перехода всех двадцати initial slots в `READY`; до публикации availability pair равна `false/false` |
| Player-commanded группы стоят на HQ | это ожидаемый `AWAITING_PLAYER_COMMAND`/`SYSTEM_HOLD`; отправь valid player order своей фракции |
| Клиент не подключается | `2001/UDP`, LAN IPv4, локальный firewall, exact commit и версии |
| UDP 2001 занят | `Get-NetUDPEndpoint -LocalPort 2001` и точный `OwningProcess` |
| Нет `[AICF]` событий | загружены ли оба аддона и не произошёл ли fallback к vanilla |
| Нет плитки `AI Conflict - Arland` | включён ли packaged Arland addon либо запущен ли source-client с его `-gproj`/GUID |
| В RHS запустился stock world | выбрана ли точная плитка `AI Conflict RHS - Arland`, а не stock-плитка dependency |
| Видно окно клиента, но UI не проверен | это `NOT RUN`; визуальный verdict даёт пользователь вручную |

Дополнительные project-specific требования к запуску и evidence находятся в
[`DEVELOPMENT.md`](DEVELOPMENT.md) и [`TESTING.md`](TESTING.md).

## Официальные справочники

- [Bohemia Interactive: Server Hosting](https://community.bistudio.com/wiki/Arma_Reforger:Server_Hosting)
- [Bohemia Interactive: Server Config](https://community.bistudio.com/wiki/Arma_Reforger:Server_Config)
- [Bohemia Interactive: Startup Parameters](https://community.bistudio.com/wiki/Arma_Reforger:Startup_Parameters)

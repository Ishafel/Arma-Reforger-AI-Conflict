# Разработка и локальный запуск

## Модель проекта

Репозиторий не имеет обычного compiler/package manager. Он проверяется через
Diag Workbench и запускается как unpacked source addon. Canonical Workshop
metadata и preview assets хранятся в `workshop/`, но первая упаковка и upload
выполняются владельцем вручную через Workbench по
[`docs/PUBLISHING.md`](PUBLISHING.md).

Stock рабочий `gproj` — `AIConflictArland/addon.gproj`; он зависит от Core и
vanilla. RHS рабочий `gproj` — `AIConflictArlandRHS/addon.gproj`, постоянный
GUID `9F88011DA22B471C`; обычные проекты не получают его RHS dependencies.

## Требования

- Windows 10/11 x64.
- Arma Reforger, Arma Reforger Server и Arma Reforger Tools одной версии.
- PowerShell 7 или Windows PowerShell для scripts в `tools/`.
- Git for Windows, если нужно обновить локальный Script Diff через Bash.

Зафиксированная API-база проекта — Arma Reforger Script Diff `1.8.0.10`, commit
`b46bdd8f4932f3a256c765f93a44417996a6da73`. Это не отменяет запись фактически
установленных версий перед проверкой.

## Workbench Validate из терминала

Codex не открывает Launcher, Workbench или Script Editor через GUI и не
управляет ими через Computer Use. Единственный разрешённый агенту способ
Workbench validation — терминальный Diag-запуск. Команда ниже пишет артефакты
в выбранный `$logsDir`:

```powershell
$repoRoot = (Resolve-Path '.').Path
$toolsRoot = 'C:\Program Files (x86)\Steam\steamapps\common\Arma Reforger Tools'
$gameRoot = 'C:\Program Files (x86)\Steam\steamapps\common\Arma Reforger'
$logsDir = Join-Path $env:LOCALAPPDATA ('AICF\Workbench-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))

& "$toolsRoot\Workbench\ArmaReforgerWorkbenchSteamDiag.exe" `
  -noThrow `
  -wbsilent `
  -gproj "$repoRoot\AIConflictArland\addon.gproj" `
  -addonsDir "$gameRoot\addons,$repoRoot" `
  -addons '9178E5822AFE48EA,B52C5F6AEDBF423E' `
  -logsDir "$logsDir" `
  -wbModule=ScriptEditor `
  -run `
  -validate

if ($LASTEXITCODE -ne 0)
{
  throw "Workbench Validate failed with exit code $LASTEXITCODE. Logs: $logsDir"
}
```

Для RHS-варианта укажи установленный каталог RHS и валидируй отдельный root
project со всем dependency graph:

```powershell
$rhsRoot = 'C:\Users\retar\OneDrive\Документы\My Games\ArmaReforger\addons'
$rhsLogsDir = Join-Path $env:LOCALAPPDATA ('AICF\Workbench-RHS-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))

& "$toolsRoot\Workbench\ArmaReforgerWorkbenchSteamDiag.exe" `
  -noThrow `
  -wbsilent `
  -gproj "$repoRoot\AIConflictArlandRHS\addon.gproj" `
  -addonsDir "$gameRoot\addons,$repoRoot,$rhsRoot" `
  -addons '9178E5822AFE48EA,B52C5F6AEDBF423E,1337C0DE5DABBEEF,BADC0DEDABBEDA5E,595F2BF2F44836FB,9F88011DA22B471C' `
  -logsDir "$rhsLogsDir" `
  -wbModule=ScriptEditor `
  -run `
  -validate
```

RHS target зафиксирован на Status Quo `0.16.5150` и Reforger `1.8.0.10`.
После обновления любого RHS package нужны новый catalog probe, Workbench и
runtime; числа capacity и prefab mappings нельзя переносить автоматически.

`resourceDatabase.rdb` может быть создан Workbench локально и не коммитится.
Compile evidence требует успешного создания Game module и отсутствия
`SCRIPT (E/F)`, `ENGINE (F)` и VM/null errors. Оно не является runtime PASS.

## Локальный API reference

Кэш находится под игнорируемым каталогом:

```text
.cache/reforger-api/Arma-Reforger-Script-Diff-1.8.0.10/
```

Проверить или загрузить его из Git Bash:

```bash
./tools/fetch_reforger_api_reference.sh
```

Из PowerShell, если `bash` не находится в `PATH`:

```powershell
& 'C:\Program Files\Git\bin\bash.exe' `
  -c 'export PATH=/usr/bin:/mingw64/bin:$PATH; exec tools/fetch_reforger_api_reference.sh'
```

Кэш vendor-owned: не редактируй его. При смене целевой версии обновление
version/commit/checksum в helper и миграция всех protected/internal API должны
выполняться как отдельная задача.

## Рекомендуемый цикл изменения

1. Записать branch/commit и `git status`.
2. Прочитать `AGENTS.md`, релевантный раздел `docs/ARCHITECTURE.md` и соседние
   классы затронутого домена.
3. Выполнить подходящие static audits и сохранить pre-change baseline.
4. Сделать минимальную правку без обхода ownership boundaries.
5. Повторить static audits и сравнить набор rule IDs.
6. Выполнить терминальный Workbench Validate/Compile для production `.c` или
   Enfusion API.
7. Если меняется поведение, запустить server/client из терминала на свежих
   profiles и проверить полные логи.
8. Передать diff, команды, exit codes, verdict и не выполненные gates.

## Direct Diag server: Arland Conflict

Пошаговая пользовательская инструкция: [`SERVER_SETUP.md`](SERVER_SETUP.md).

Для stock Conflict parity нужны raw world, mission header и systems config
одновременно. Codex запускает server только этой терминальной схемой, без
Launcher или Host UI. Каждый проверочный запуск получает новый profile.

```powershell
$serverRoot = 'C:\Program Files (x86)\Steam\steamapps\common\Arma Reforger Server'
$repoRoot = (Resolve-Path '.').Path
$runStamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$profileRoot = Join-Path $env:LOCALAPPDATA "AICF\Server-$runStamp"

& "$serverRoot\ArmaReforgerServerDiag.exe" `
  -gproj "$repoRoot\AIConflictArland\addon.gproj" `
  -server 'worlds/MP/CTI_Campaign_Arland.ent' `
  -MissionHeader 'Missions/23_Campaign_Arland.conf' `
  -worldSystemsConfig 'Configs/Systems/ConflictSystems.conf' `
  -addonsDir "$repoRoot,$serverRoot\addons" `
  -addons '9178E5822AFE48EA,B52C5F6AEDBF423E' `
  -profile "$profileRoot" `
  -backendFreshSession `
  -aicfAICommanderMode BOTH `
  -maxFPS 60 `
  -logStats 10000
```

`-aicfAICommanderMode` фиксирует command authority на весь process:

| Значение | Автономный target selection |
|---|---|
| `BOTH` | отдельный AI commander для `US` и для `USSR` |
| `US` | только `US`; `USSR` ожидает player orders |
| `USSR` | только `USSR`; `US` ожидает player orders |

Если параметр опущен, применяется `BOTH`. Проверка exact-case: `NONE`, пустое,
lowercase и неизвестное значение вызывают `CONFIG_INVALID` и останавливают
startup до запуска изменяющего состояние Arland radio normalizer, его
subscription/изменения replicated radio state, MatchController composition,
roster и periodic loops. Arland bootstrap создаёт config один раз и передаёт тот
же предварительно проверенный объект в `AICF_MatchController`; controller строит
immutable policy без повторного чтения CLI. Runtime-переключения нет; для
другого mode останови server и начни новый run. Для воспроизводимого evidence
указывай mode явно даже при проверке default-поведения, а отдельный default-run
выполняй без параметра.

Replicated `ai_commander_us`/`ai_commander_ussr` — availability snapshot, а не
изменяемая замена policy. Пока все двадцать асинхронных initial slots не
перешли в `READY`, они остаются `false/false`, поэтому UI показывает
`COMMAND SYNC`. В `TryLogRosterReady()` controller публикует flags допустимого
mode одним authoritative state change. При любом
`AICF_MatchController.Stop()` flags снова сбрасываются в `false/false` до снятия
domain subscriptions и cleanup. Готовность group/agent roster по-прежнему
проверяется отдельными readiness/generation gates.

Player-commanded сторона продолжает иметь roster, tickets, economy, vehicles,
reliability, victory и replicated UI state. Пока valid player order отсутствует,
её combat-ready slots имеют `AWAITING_PLAYER_COMMAND` и `SYSTEM_HOLD` на HQ.
`aicfExpectedPlayerFaction` не задаёт authority и не заменяет этот параметр.

Добавляй только остальные параметры конкретного сценария проверки, например:

```powershell
-aicfRequirePlayerForResult 0
```

Vehicle и economy subsystems включены всегда. Параметры
`aicfVehiclesEnabled` и `aicfEconomyEnabled` больше не читаются.

Не переиспользуй старый profile и не заменяй эту схему Host UI или raw world
без `-MissionHeader`/`-worldSystemsConfig`.

## Direct Diag server: RHS Arland Conflict

RHS source-run использует штатные RHS world и mission. Не подставляй stock raw
world: он создаёт stock faction manager даже при RHS `MissionHeader`.

```powershell
$serverRoot = 'C:\Program Files (x86)\Steam\steamapps\common\Arma Reforger Server'
$repoRoot = (Resolve-Path '.').Path
$rhsRoot = 'C:\Users\retar\OneDrive\Документы\My Games\ArmaReforger\addons'
$runStamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$profileRoot = Join-Path $env:LOCALAPPDATA "AICF\Server-RHS-$runStamp"

& "$serverRoot\ArmaReforgerServerDiag.exe" `
  -gproj "$repoRoot\AIConflictArlandRHS\addon.gproj" `
  -server 'Worlds/MP/Conflict/CTI_Campaign_Arland_RHS.ent' `
  -MissionHeader 'Missions/RHS_Conflict_Arland.conf' `
  -worldSystemsConfig 'Configs/Systems/ConflictSystems.conf' `
  -addonsDir "$repoRoot,$serverRoot\addons,$rhsRoot" `
  -addons '9178E5822AFE48EA,B52C5F6AEDBF423E,1337C0DE5DABBEEF,BADC0DEDABBEDA5E,595F2BF2F44836FB,9F88011DA22B471C' `
  -profile "$profileRoot" `
  -backendFreshSession `
  -aicfAICommanderMode BOTH `
  -maxFPS 60 `
  -logStats 10000
```

Startup должен дать `[AICF][CONTENT][INFO][PROFILE_SELECTED]` с profile
`RHS_USMC_MSV_0_16_5150`, runtime sides `RHS_USAF`/`RHS_AFRF` и stable sides
`US`/`USSR`. Все `[AICF][STAGE...] faction=...` сохраняют stable values.

## Direct Diag client

Если клиент нужен для log-side проверки, Codex запускает его из терминала, не
управляет его окном и не делает screenshots/video. Клиент должен загрузить те
же local addons и иметь отдельный свежий profile:

```powershell
$gameRoot = 'C:\Program Files (x86)\Steam\steamapps\common\Arma Reforger'
$repoRoot = (Resolve-Path '.').Path
$runStamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$clientProfile = Join-Path $env:LOCALAPPDATA "AICF\Client-$runStamp"

& "$gameRoot\ArmaReforgerSteamDiag.exe" `
  -gproj "$repoRoot\AIConflictArland\addon.gproj" `
  -client 127.0.0.1 `
  -addonsDir "$repoRoot,$gameRoot\addons" `
  -addons '9178E5822AFE48EA,B52C5F6AEDBF423E' `
  -profile "$clientProfile" `
  -backendFreshSession
```

Стандартный локальный game port — `2001`. Для другого порта укажи его в
client target согласно текущему Reforger CLI.

RHS-клиент использует тот же RHS dependency set:

```powershell
$rhsRoot = 'C:\Users\retar\OneDrive\Документы\My Games\ArmaReforger\addons'
$rhsClientProfile = Join-Path $env:LOCALAPPDATA ('AICF\Client-RHS-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))

& "$gameRoot\ArmaReforgerSteamDiag.exe" `
  -gproj "$repoRoot\AIConflictArlandRHS\addon.gproj" `
  -client 127.0.0.1 `
  -addonsDir "$repoRoot,$gameRoot\addons,$rhsRoot" `
  -addons '9178E5822AFE48EA,B52C5F6AEDBF423E,1337C0DE5DABBEEF,BADC0DEDABBEDA5E,595F2BF2F44836FB,9F88011DA22B471C' `
  -profile "$rhsClientProfile" `
  -backendFreshSession
```

## Логи

Server/client profile обычно содержит:

```text
<profile>/logs/logs_YYYY-MM-DD_HH-MM-SS/console.log
```

Найти последний log:

```powershell
$log = Get-ChildItem -LiteralPath "$profileRoot\logs" -Filter console.log -File -Recurse |
  Sort-Object LastWriteTime -Descending |
  Select-Object -First 1

$log.FullName
Select-String -LiteralPath $log.FullName -SimpleMatch '[AICF]' |
  ForEach-Object Line
```

Сохраняй полный `console.log` и соседние server/client logs. Фильтр `[AICF]`
удобен для навигации, но может скрыть `SCRIPT`, `ENGINE`, `RESOURCES`, `RPL` и
VM errors.

## Параллельный vehicle smoke-helper

`.codex-runtime/start-parallel-vehicle-servers.ps1` запускает два специальных
dedicated server: transport-only и armed-only, на отдельных портах. Это узкий
smoke harness, а не канонический runtime gate:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File .\.codex-runtime\start-parallel-vehicle-servers.ps1
```

Остановить запущенную пару:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File .\.codex-runtime\stop-parallel-vehicle-servers.ps1
```

Ограничения helper: hardcoded Steam path, нет клиента, завершение через
`Stop-Process` не гарантирует graceful finalization. Созданный
`.codex-runtime/active-parallel-batch.txt` не коммитится.

## Generated и локальные данные

Не редактировать и не коммитить:

- `.cache/` — API snapshots и локальный evidence;
- `.idea/`, `.gigaide/`;
- `*.log`, `logs/`, `profile/`;
- `**/resourceDatabase.rdb`;
- `%LOCALAPPDATA%\AICF\...` profiles;
- файлы в установленных каталогах игры, Server и Tools.

Первая публикация не автоматизирована: Workbench CLI поддерживает
`-publishAddon` только для обновления уже опубликованного проекта. Целевой
канал, canonical metadata, versioning, порядок Core -> Arland -> RHS и проверка
packaged build зафиксированы в [`docs/PUBLISHING.md`](PUBLISHING.md).

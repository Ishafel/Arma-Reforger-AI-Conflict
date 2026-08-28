# Разработка и локальный запуск

## Модель проекта

Репозиторий не имеет обычного compiler/package manager. Он проверяется через
Diag Workbench и запускается как unpacked source addon. Packaging, `.pak`,
Workshop metadata, upload и CI сейчас не предоставлены.

Рабочий `gproj` — `AIConflictArland/addon.gproj`; он зависит от Core и vanilla.

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
  -maxFPS 60 `
  -logStats 10000
```

Добавляй только параметры конкретного сценария проверки, например:

```powershell
-aicfRequirePlayerForResult 0
```

Vehicle и economy subsystems включены всегда. Параметры
`aicfVehiclesEnabled` и `aicfEconomyEnabled` больше не читаются.

Не переиспользуй старый profile и не заменяй эту схему Host UI или raw world
без `-MissionHeader`/`-worldSystemsConfig`.

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

Публикация/упаковка пока не автоматизирована. Если она понадобится, сначала
нужно определить целевой канал, metadata, versioning и воспроизводимый
Resource Publisher workflow.

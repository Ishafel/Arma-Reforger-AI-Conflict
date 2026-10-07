# Разработка и терминальный запуск

Работай из корня репозитория в Windows PowerShell/PowerShell 7. Нужны
Arma Reforger, Server и Tools одной версии, для API helper — Git for Windows.
Перед изменением запиши `git status`, branch/commit и фактические версии.
Создай отдельную ветку задачи по [AGENTS.md](../AGENTS.md).

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

Точный граф и ресурс `.conf` определяет launcher. Не заменяй сценарий raw `.ent`
с отдельным `-MissionHeader`: whitelist должен применяться до инициализации баз.
RHS/WCS версии и каталоги сверяй с установленными пакетами и addon dependencies;
сведения WCS: [Arland](../AIConflictArlandWCSRHS/README.md),
[Everon](../AIConflictEveronWCSRHS/README.md).

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
Для клиентской проверки укажи этот server profile и тот же Variant в другой сессии:

```powershell
$serverProfileRoot = '<абсолютный AICF_RUNTIME_PROFILE работающего сервера>'
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Start-AICFRuntime.ps1 `
  -Role Client -Variant Stock -ServerProfileRoot $serverProfileRoot
```

Placeholder заменяется фактическим путём. Перед подключением launcher проверяет
свежий server log, точный `CLI Params`, живой server process и `[ROSTER_READY]`.
Порт по умолчанию — 2001; другой `-ServerPort` должен совпадать у обеих ролей.
`-ClientAddress` задаёт адрес. Не обходи readiness gate ручным native запуском.

`-AICommanderMode BOTH|US|USSR` задаётся при старте и не меняется в матче.
`-LoadoutLibraryPath <старый-profile>/profile/AICF_Loadouts` импортирует библиотеку
шаблонов в свежий server profile; manifest фиксирует файлы и SHA-256.

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

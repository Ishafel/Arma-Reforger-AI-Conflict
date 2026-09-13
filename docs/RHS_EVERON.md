# AI Conflict RHS — Everon

`AIConflictEveronRHS` добавляет сценарий **AI Conflict RHS - Everon**:
автономный Conflict RHS USMC против RHS MSV на полном острове Everon.
Это отдельный source root-addon, который объединяет существующие Everon
integration и RHS content profile. Он содержит один локальный callsign
compatibility adapter. Новых world, bases, layouts, controllers или server
loops в нём нет.

## Ресурсы и зависимости

| Ресурс | Идентичность |
|---|---|
| Root project | `AIConflictEveronRHS/addon.gproj`, `FA9FDCCA428A43BA` |
| Плитка | `{57FA3D0337BE47E5}Missions/AICF_RHS_Conflict_Everon.conf` |
| Родительский RHS header | `{AAD43C10045857C1}Missions/RHS_Conflict.conf` |
| RHS world | `{EEF09465C62534F7}Worlds/MP/Conflict/CTI_Campaign_Eden_RHS.ent` |
| Systems config для direct server | `Configs/Systems/ConflictSystems.conf` |

Parent и world GUID прочитаны из установленного `RHS-StatusQuo` resource
database версии `0.16.5150`; Arland GUID `7577640CD42A00BD` использован как
контроль декодирования. Установленные файлы не изменяются и не копируются.

Dependency graph включает vanilla `58D0FB3206B6F859`, Core
`9178E5822AFE48EA`, Arland `B52C5F6AEDBF423E`, Everon `A4B2E62595F645A4`,
RHS Content Pack 01 `1337C0DE5DABBEEF`, Content Pack 02 `BADC0DEDABBEDA5E`,
Status Quo `595F2BF2F44836FB` и ArlandRHS `9F88011DA22B471C`.
ArlandRHS остаётся владельцем общего RHS profile и compatibility adapters.
Everon предоставляет `AICF_EveronRadioBridgeNormalizer`. Их factory overrides
меняют разные методы общего bootstrap; новая копия этих скриптов не нужна.

При первом runtime штатный `InitializeBases` исчерпал короткий RHS callsign
pool и вызвал `Index out of bounds` до завершения инициализации баз. Локальный
`AICF_RHSEveronCallsignPool.c` через pinned `GetSharedCallsignPool` расширяет
непустой временный массив до фактического числа initialized bases, только на
authoritative RHS Everon. Числовые индексы уникальны, а stock
`SCR_Faction.GetBaseCallsignByIndex` уже поддерживает cyclic lookup имён и
радиосигналов на всех peers. Поэтому при коротком каталоге отображаемые имена
могут повторяться; числовая identity базы не меняется. Каталоги и world assets
не редактируются. При достаточном пуле adapter ничего не добавляет.

Stable sides остаются `US` / `USSR`, runtime factions — `RHS_USAF` /
`RHS_AFRF`. Политики roster, vehicles, economy, recruitment и player orders
общие для проекта. Header отключает session persistence (`m_eSaveTypes 0`),
задаёт `GENERAL`, старт в 08:00 и ускорение дня/ночи 6/24, как stock Everon.

## Dedicated server и client

Из корня репозитория, в отдельной терминальной сессии:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File .\tools\Start-AICFRuntime.ps1 `
  -Role Server -Variant EveronRHS -AICommanderMode BOTH
```

Если RHS не найден автоматически, добавь `-RhsAddonsRoot` с абсолютным путём
к каталогу установленных RHS addons. Для другого расположения игры доступны
`-ServerRoot` / `-GameRoot`. Launcher печатает `AICF_RUNTIME_PROFILE` и
`AICF_RUNTIME_MANIFEST_JSON`; сохрани оба значения.

Клиент запускается во второй сессии после server readiness:

```powershell
$rhsEveronServerProfile = Read-Host 'Вставь AICF_RUNTIME_PROFILE сервера EveronRHS'
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File .\tools\Start-AICFRuntime.ps1 `
  -Role Client -Variant EveronRHS -ServerProfileRoot $rhsEveronServerProfile
```

Перед подключением launcher проверяет точный server CLI, живой process и
`ROSTER_READY`. Для просмотра аргументов без запуска используй `-DryRun`.
Stock raw world для этого варианта не подходит: RHS faction manager принадлежит
RHS world, а один MissionHeader не заменяет его.

## Ручное меню сценариев

Эта команда предназначена для пользователя; Codex проверяет сценарий через
терминальный dedicated server и логи. Запусти source-client с полным graph:

```powershell
$repoRoot = (Resolve-Path '.').Path
$gameRoot = 'C:\Program Files (x86)\Steam\steamapps\common\Arma Reforger'
$rhsRoot = 'C:\Users\retar\OneDrive\Документы\My Games\ArmaReforger\addons'
$rhsEveronProfile = Join-Path $env:LOCALAPPDATA ('AICF\ScenarioMenu-EveronRHS-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
$rhsEveronGraph = '9178E5822AFE48EA,B52C5F6AEDBF423E,A4B2E62595F645A4,1337C0DE5DABBEEF,BADC0DEDABBEDA5E,595F2BF2F44836FB,9F88011DA22B471C,FA9FDCCA428A43BA'

& "$gameRoot\ArmaReforgerSteamDiag.exe" `
  -gproj "$repoRoot\AIConflictEveronRHS\addon.gproj" `
  -addonsDir "$repoRoot,$gameRoot\addons,$rhsRoot" `
  -addons $rhsEveronGraph -profile $rhsEveronProfile -backendFreshSession
```

Выбери `Сценарии -> AI Conflict RHS - Everon`. В списке также присутствуют
плитки зависимостей; stock сценарии запускаются с отдельным stock graph.
Каждый запуск начинает новую кампанию, default commander mode — `BOTH`.
Новый addon ещё не опубликован в Workshop; source-проверка не заменяет
упаковку, публикацию и packaged multiplayer gate.

## Workbench Validate

С теми же `$repoRoot`, `$gameRoot`, `$rhsRoot`, `$rhsEveronGraph`:

```powershell
$toolsRoot = 'C:\Program Files (x86)\Steam\steamapps\common\Arma Reforger Tools'
$rhsEveronLogs = Join-Path $env:LOCALAPPDATA ('AICF\Workbench-EveronRHS-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))

& "$toolsRoot\Workbench\ArmaReforgerWorkbenchSteamDiag.exe" `
  -noThrow -wbsilent -gproj "$repoRoot\AIConflictEveronRHS\addon.gproj" `
  -addonsDir "$gameRoot\addons,$repoRoot,$rhsRoot" -addons $rhsEveronGraph `
  -logsDir $rhsEveronLogs -wbModule=ScriptEditor -run -validate
```

Требуются exit 0, `Game successfully created`, `Script validation successful`
и отсутствие SCRIPT E/F, ENGINE F, VM/null errors в полном логе.

## Проверки реализации — 2026-09-13

Evidence хранится в `.codex-runtime/rhs-everon-20260913/`; исходный commit,
dirty status, версии и прочитанные resource identities сохранены отдельно.
Все Game/Server/Tools имеют версию `1.8.0.13`.

Команда каждого аудита: `powershell.exe -NoProfile -ExecutionPolicy Bypass
-File tools/Test-<Name>Static.ps1`.

| Проверка | До | После |
|---|---|---|
| `ScenarioHeaders` | PASS / 0 | PASS / 0, включая RHS Everon parent, GUID, graph и platform metadata |
| `RuntimeLauncher` | PASS / 0 | PASS / 0, включая EveronRHS server/client с пробелами и кириллицей в paths |
| `RHSIntegration` | PASS / 0 | PASS / 0 |
| `RankRestrictions` | PASS / 0 | PASS / 0, включая новый header |
| `AICommanderMode` | FAIL / 1 | FAIL / 1, тот же `AI_COMMANDER_UI_STATE` |
| `Stage35` | PASS / 0 перед добавлением adapter | PASS / 0 |

Сохранённый failure относится к прежней английской подписи ожидания player
command, не к RHS Everon. Существующие production `.c` не менялись; добавлен
только локальный RHS Everon adapter. Три отрицательные проверки отдельной
временной копии (удалённый authority guard, неверная граница ёмкости и stock
parent вместо RHS) дали ожидаемый FAIL соответствующего rule; исходники
рабочего дерева при этом не менялись.

### Compile и runtime

Терминальный Workbench Validate после добавления adapter — **PASS / exit 0**:
`Game successfully created`, `Script validation successful`, SCRIPT E/F,
ENGINE F и VM/null — 0. Полный лог:
[workbench-final/console.log](../.codex-runtime/rhs-everon-20260913/workbench-final/console.log).
Аргументы сохранены в `workbench-final-args.json`, результат — в
`workbench-final-result.json`. Первый sandbox-запуск не смог завершить
validation из-за `SteamAPI_Init`; успешный финальный запуск выполнен через
терминальный Diag с доступом к пользовательскому Steam. Отдельный запуск
с неполным массивом аргументов остановлен по exact PID/profile и повторён;
его evidence не используется как compile gate.

Финальные команды runtime — `Start-AICFRuntime.ps1 -Role Server -Variant
EveronRHS -AICommanderMode BOTH -RhsAddonsRoot <установленные RHS addons>
-ProfileRoot <fresh server profile>` и `-Role Client -Variant EveronRHS
-RhsAddonsRoot <тот же каталог> -ProfileRoot <fresh client profile>
-ServerProfileRoot <server profile>`. Полные manifests, CLI и exit сохранены
в `server-final-launch.txt` и `client-final-launch.txt`.

| Gate | Verdict и evidence |
|---|---|
| Server bootstrap/roster | PASS: один `BOOTSTRAP_SERVER map=Everon`, `RADIO_BRIDGE_READY policy=ONE_WAY_OR_ISOLATED_COMPONENT`, один `PROFILE_SELECTED` RHS USMC/MSV, `ROSTER_READY us_groups=10 ussr_groups=10` |
| Callsign compatibility | PASS: `BASE_CALLSIGN_POOL_EXTENDED original=40 required=41`; VM/null errors 0 против одного `Index out of bounds` до adapter |
| Client connection/replicated snapshot | PASS: launcher подтвердил exact CLI, process и roster до подключения; клиент получил 18 `STATE_REPLICATED`; SCRIPT E/F, ENGINE F, VM/null errors 0 |
| Полный runtime audit | **FAIL / exit 1**: `Test-AICommanderModeLog.ps1 -ServerLogPath <server console.log> -ClientLogPath <client console.log> -ExpectedMode BOTH -RequireInitialCoverage`; единственный failure — `Server script/engine/VM failures: 6` |
| Ручной menu/visual verdict, deployment, gameplay JIP/reconnect, multiplayer-матрица, длительный soak, packaged Workshop build | **NOT RUN** |

Шесть server `SCRIPT (E)` — по два `SCR_Faction ... not a valid SCR_Faction`
для `US`, `USSR`, `RHS_ION`. Это тот же набор, который подтверждён на исходном
RHS baseline в [BASE_BUILDERS_VALIDATION.md](BASE_BUILDERS_VALIDATION.md) и
[AI_COMBAT_VALIDATION.md](AI_COMBAT_VALIDATION.md). Он не объявляется исправленным.
В обоих полных логах также остаются RHS/stock resource/world compatibility
messages (`m_fAILimitThreshold`, task manager, localization, Hierarchy и др.);
на server есть pathfinding tile и Arsenal RPC errors, на client — GUI resource
errors. Их полный индекс и группировка сохранены в `*-final-error-index.txt`
и `*-final-error-groups.txt`. Ограниченный PASS подключения не означает
отсутствия этих ошибок или полной игровой приёмки.

Полные остановленные логи:

- [Server console.log](C:/Users/retar/AppData/Local/AICF/Server-EveronRHS-Final-20260913-151837/logs/logs_2026-09-13_15-18-37/console.log) — 1217 строк; runtime 15:18:37–15:21:02 MSK.
- [Client console.log](C:/Users/retar/AppData/Local/AICF/Client-EveronRHS-Final-20260913-151922/logs/logs_2026-09-13_15-19-23/console.log) — 2173 строки; runtime 15:19:23–15:21:02 MSK.

Все соседние logs остаются в соответствующих fresh profiles. `runtime-final-summary.json`
содержит пути, SHA-256, error counts и event counts. Оба созданных процесса
остановлены через `Stop-Process -Force` после повторной проверки exact
profile/PID; native exit `-1`, shell exit `1` отражают принудительную остановку,
а не успешный graceful shutdown. Identity и время сохранены в
`runtime-final-stop.json`. Graceful shutdown не проверен.

### Изменённые файлы

- `AIConflictEveronRHS/addon.gproj`, `license.txt`, inherited header и его
  `.meta`, `Scripts/Game/AIConflictEveronRHS/Integration/AICF_RHSEveronCallsignPool.c`.
- `tools/Start-AICFRuntime.ps1`, `Test-ScenarioHeadersStatic.ps1`,
  `Test-RuntimeLauncherStatic.ps1`, `Test-RankRestrictionsStatic.ps1`.
- `README.md`, `docs/ARCHITECTURE.md`, `DEVELOPMENT.md`, `SERVER_SETUP.md`,
  `TESTING.md` и этот документ.

`git diff --check` — PASS. Исходные пользовательские untracked файлы и
предыдущие runtime evidence не изменялись.

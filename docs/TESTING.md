# Проверки и evidence

## Семантика gates

Каждый уровень отвечает на отдельный вопрос:

| Gate | Что доказывает | Чего не доказывает |
|---|---|---|
| Static audit | Структурные и текстовые project contracts | Компиляцию Enforce и игровое поведение |
| Workbench Validate/Compile | Совместимость исходников с текущим Enfusion API | Server/client lifecycle и долгий runtime |
| Server runtime | Authoritative gameplay и server diagnostics | Клиентский UI, replication/JIP и визуальное поведение |
| Client runtime/logs | Запуск клиента, log-side ошибки и наблюдаемую в логах репликацию | Визуальный UI и controls без ручной проверки пользователя |
| Soak | Устойчивость на заданном времени и профиле | Поведение в неиспытанных конфигурациях |

`NOT RUN` не равен `PASS`. Запущенный процесс или отсутствие `[AICF][ERROR]` в
коротком фрагменте не образуют runtime PASS. Агент не присваивает результату
статус `ACCEPTED` без решения владельца.

## Terminal-only evidence

Codex не использует Computer Use, screen capture, GUI automation,
screenshots/video или управление мышью и клавиатурой для разработки и
тестирования этого проекта. Workbench, server и client запускаются только из
терминала командами из `docs/DEVELOPMENT.md`.

Evidence агента состоит из команд, exit codes и полных логов. Если критерий
можно проверить только визуально или через интерактивный UI, Codex фиксирует
его как `NOT RUN`. Такой критерий становится проверенным только после отдельной
ручной проверки пользователя; наличие запущенного окна этого не доказывает.

## Статические аудиторы

Запускай из корня репозитория:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\Test-Stage3Static.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\Test-Stage35Static.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\Test-Stage35RecoveryPolicy.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\Test-Stage4Static.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\Test-AICommanderModeStatic.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\Test-RHSIntegrationStatic.ps1
```

Назначение:

| Script | Основной охват |
|---|---|
| `Test-Stage3Static.ps1` | vehicle architecture, acquisition, trip, cleanup и diagnostics contracts |
| `Test-Stage35Static.ps1` | force structure, vehicle/infantry integration и Enforce language audit |
| `Test-Stage35RecoveryPolicy.ps1` | точные recovery, timing, ownership и fail-closed policies |
| `Test-Stage4Static.ps1` | economy transaction, supply balance, replication, strategic UI и RPC authority |
| `Test-AICommanderModeStatic.ps1` | Arland CLI preflight, immutable authority policy, faction commander boundary, intent, availability replication и UI contract |
| `Test-RHSIntegrationStatic.ps1` | optional dependency graph, Core isolation, stock/RHS profiles, fail-closed roles/vehicles, stable-side Arland radio normalization, single lifecycle и cleanup symmetry |

Аудиторы проверяют часть архитектуры регулярными выражениями. Красный rule ID
может означать реальный regression или drift тестового контракта. Сначала
сопоставь правило с поведением и историей; не меняй production-код либо regex
только ради зелёного вывода.

## Зафиксированный baseline

Baseline получен `2026-08-28` на `main`, commit
`6ff6c550dc7c5a40d5876e3f96ddf57bfc64f107`, до создания этой документации.
После изменения HEAD его нужно запускать заново.

| Проверка | Exit | Результат |
|---|---:|---|
| `Test-Stage3Static.ps1` | 1 | `FAIL`, 5 issues |
| `Test-Stage35Static.ps1` | 1 | `FAIL`, 4 issues |
| `Test-Stage35RecoveryPolicy.ps1` | 0 | `PASS` |
| `Test-Stage4Static.ps1` | 0 | `PASS` |

`Test-AICommanderModeStatic.ps1` добавлен после этого исторического baseline и
должен передаваться отдельным verdict, без выдуманного сравнения с `main`.
То же относится к `Test-RHSIntegrationStatic.ps1`: его первый успешный прогон
является отдельным focused verdict, а не частью исторического baseline commit.

Stage 3 baseline failures:

- `STAGE3_PROGRESS_EVIDENCE`: auditor ожидает five-minute objective timeout,
  код задаёт `120000` ms;
- два `STAGE3_RUNNING_CARGO_STALL` contracts;
- `STAGE3_BOUNDED_PROTECTED_CLEARANCE` reason signature;
- `STAGE3_MARKER_STATE`: auditor ожидает marker fragment `VEH `.

Stage 3.5 baseline failures:

- `STAGE35_FORCE_STRUCTURE`: auditor всё ещё ожидает floor `80`, текущий код
  использует более высокий `MIN_MANAGED_AGENTS = 128`;
- `STAGE35_MEANINGFUL_TASK_PROOF` queue evidence;
- `STAGE35_EXACT_CARGO_STALL`;
- `STAGE35_BOUNDED_PROTECTED_CLEARANCE` reason signature.

При работе в этих областях отчёт должен показывать pre-change и post-change
наборы failures. Новых failures быть не должно; исчезнувший failure объясняется
изменением реализации или осознанным обновлением контракта.

## Выбор проверок по области

| Изменение | Минимум |
|---|---|
| Только Markdown | проверить ссылки/команды, `git diff --check` |
| Command config/bootstrap, authority, intent или availability state | `Test-AICommanderModeStatic.ps1` + затронутые Stage audits + Workbench Validate; server/client runtime и JIP при изменении snapshot |
| Прочие faction/group state, forces, objectives, orders | релевантные static audits + Workbench Validate; runtime при изменении поведения |
| `Vehicles/` или `State/Vehicles/` | Stage 3, Stage 3.5 и RecoveryPolicy + Workbench + targeted runtime |
| `Economy/` | Stage 4 static + Workbench + server runtime/log audit |
| `UI/`, RPC или campaign replicated state | Stage 4 static + Workbench + server/client runtime; JIP при изменении snapshot |
| Arland `modded` integration | Workbench + canonical Arland server runtime; клиент, если меняется UI/replication |
| Content profile или `AIConflictArlandRHS` | все Stage/authority audits + `Test-RHSIntegrationStatic.ps1`; отдельный stock и RHS Workbench; fresh RHS server; client/JIP при изменении faction/UI mapping |
| PowerShell analyzer | позитивный и негативный representative input; не ослаблять rule молча |
| Enfusion version/API | pinned reference diff + полный Workbench Validate по целевой версии |

Static audit после Enforce-изменения обязателен, но не заменяет Workbench.
Runtime, выполненный до последнего product-code изменения, не доказывает новый
commit.

## Workbench gate

Используй только терминальную Diag-команду из `docs/DEVELOPMENT.md`. Сохраняй:

- exact game/tools version;
- commit и dirty status;
- команду или последовательность действий;
- полный `console.log`, `script.log`, `error.log`;
- exit code;
- факт `Game successfully created`;
- количество `SCRIPT (E/F)`, `ENGINE (F)`, VM/null exceptions.

Platform/backend или shutdown resource messages нельзя автоматически скрывать.
Их нужно классифицировать отдельно от AICF compile error.

## Анализаторы runtime-логов

### AI commander mode

Для valid mode проверяй server authority contract и, при необходимости, JIP
client log:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File .\tools\Test-AICommanderModeLog.ps1 `
  -ServerLogPath 'C:\absolute\path\server-console.log' `
  -ExpectedMode US `
  -RequireInitialCoverage `
  -ClientLogPath 'C:\absolute\path\client-console.log'
```

Для rejected value используй отдельный parameter set:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File .\tools\Test-AICommanderModeLog.ps1 `
  -ServerLogPath 'C:\absolute\path\server-console.log' `
  -ExpectedInvalidValue NONE
```

Анализатор сверяет `CONFIG`, два `COMMAND_AUTHORITY_SET`, допустимый
`decision_authority`, initial `S0..S9` coverage и отсутствие скрытых AI
assignments на player-commanded стороне. Для invalid run он требует ровно один
`CONFIG_INVALID`/Stage 1 `RESULT FAIL` и отсутствие `MATCH_START`, roster,
strategic assignment и heartbeat activity. `-ClientLogPath` дополнительно
проверяет `COMMAND_AUTHORITY_REPLICATED`, но не заменяет ручную визуальную
проверку UI.

Server `CONFIG ai_commander_us/ussr` описывает immutable policy, но сам по себе
не доказывает момент публикации RplProp. Availability остаётся `false/false` до
перехода всех двадцати асинхронных slot в `READY`; source ordering внутри
`TryLogRosterReady()` проверяет rule `AI_COMMANDER_REPLICATION` статического
аудитора. Для runtime JIP запускай новый
client только после server `MATCH_START` и initial coverage PASS, а затем требуй
в client log последнюю согласованную пару `COMMAND_AUTHORITY_REPLICATED`.

Обычный valid analyzer ожидает mode flags последним client snapshot и поэтому
не является проверкой shutdown reset. Для отдельного lifecycle-run оставь
client подключённым при `AICF_MatchController.Stop()` и потребуй последующее
`COMMAND_AUTHORITY_REPLICATED ai_commander_us=0 ai_commander_ussr=0`; этот raw
client-log criterion и визуальный `COMMAND SYNC` фиксируются отдельно от
standard valid-mode verdict.

### Stage 2

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File .\tools\Test-Stage2Log.ps1 `
  -LogPath 'C:\absolute\path\console.log'
```

Опционально: `-MaxRepeatedOrderRecoveries 3`.

Известное ограничение: binding regex принимает только slots `[0-3]` и требует
минимум восемь bindings, тогда как текущая модель имеет slots `0-9` на каждую
фракцию. Поэтому `PASS` этого анализатора может быть false green для полного
20-slot roster и не заменяет ручную проверку всех `SPAWN_BOUND` identities.

### Stage 4

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File .\tools\Test-Stage4Log.ps1 `
  -LogPath 'C:\absolute\path\console.log'
```

`Test-Stage4Log.ps1` требует постоянный always-on invariant
`[AICF][STAGE4][INFO][CONFIG] ... enabled=1` и `SUPPLY_PROBE`.
`-AllowActiveAtEnd` разрешает только незавершённые reservations/shipments на
момент остановки; errors он не игнорирует.

Сейчас отсутствуют:

- общий Stage 0/1 log verdict за пределами command-authority contract;
- специальный Stage 3/3.5 runtime log analyzer;
- общий client-log validator за пределами command-authority contract и
  автоматизированный visual validator;
- единая команда `test all` и CI.

Это пробелы покрытия, а не неявный PASS.

## Runtime contract

Запускай canonical Arland server/client из терминала по `docs/DEVELOPMENT.md`:

- новый server profile на каждый run;
- новый client profile, если нужен клиент;
- `-backendFreshSession`;
- raw Arland world вместе с `-MissionHeader` и `-worldSystemsConfig`;
- оба addon GUID;
- записанные `aicf*` flags, включая факт отсутствия или exact value
  `aicfAICommanderMode`;
- одинаковые версии Game, Server и Tools.

После завершения изучи полный остановленный log. Минимальный поиск:

```powershell
Select-String -LiteralPath $log.FullName -Pattern `
  '\[AICF\]|SCRIPT\s+\((E|F)\)|ENGINE\s+\(F\)|Virtual Machine Exception|NULL pointer'
```

Префиксы приложения:

```text
[AICF][STAGE0]
[AICF][STAGE1]
[AICF][STAGE2]
[AICF][STAGE3]
[AICF][STAGE3.5]
[AICF][STAGE4]
[AICF][CONTENT]
```

Stage 2–4 используют Stage 1 `run` и `t_ms`, поэтому события одной сессии можно
сопоставлять без приблизительных wall-clock timestamps.

Для RHS runtime дополнительно обязательно подтвердить по полному остановленному
логу:

- `PROFILE_SELECTED` содержит `RHS_USMC_MSV_0_16_5150`, runtime keys
  `RHS_USAF`/`RHS_AFRF` и stable `US`/`USSR`;
- ровно двадцать initial slots дошли до `GROUP_ROSTER_READY`, затем один
  `ROSTER_READY`;
- каждый `GROUP_ROSTER_CONFIGURED` содержит `fallback_slots=0`, RHS USMC/MSV
  `prefabs` и ни одного `Character_US_`/`Character_USSR_`;
- после захвата односторонней frontier-base событие `RADIO_BRIDGE_NORMALIZED`
  предшествует graph rebuild, а новый graph даёт владельцу путь от базы к relay;
- vehicle metadata `CATALOG` повторена событием `LIVE` после spawn; выбранные
  prefab принадлежат заявленному RHS faction catalog;
- нет `SCRIPT (E/F)`, `ENGINE (F)`, VM exception или null-pointer;
- RHS deployment map не пишет `Can't find image ''` для
  `UI/Imagesets/MilitarySymbol/ID_D.imageset`; spawn-point factions отображаются
  как полные `BLUFOR`/`OPFOR` symbols, а не пустые цветные квадраты;
- отдельный stock Arland run по обычному `AIConflictArland` не выбрал RHS
  profile и сохранил stock roster/vehicle behavior.

Комплектные loadouts принадлежат RHS character prefab. Лог доказывает выбранный
prefab, но визуальное соответствие оружия/формы и UI остаётся `NOT RUN`, пока
пользователь не проверит клиент вручную.

### Runtime matrix: command authority

Каждая строка является отдельным run на новом profile. Default и explicit
`BOTH` не объединяются: первый доказывает default при отсутствии параметра,
второй — разбор exact CLI value.

| Сценарий | CLI / действие | Обязательное evidence |
|---|---|---|
| Default | параметр отсутствует | `CONFIG ai_commander_mode=BOTH ai_commander_us=1 ai_commander_ussr=1`; `COMMAND_AUTHORITY_SET` = `AI` для обеих сторон; initial `AI_COMMANDER` coverage |
| Explicit both | `-aicfAICommanderMode BOTH` | то же authority state, exact CLI записан в evidence |
| AI only US | `-aicfAICommanderMode US` | `US=AI`, `USSR=PLAYER`; у initial USSR slots `AWAITING_PLAYER_COMMAND`, `SYSTEM_HOLD` на HQ и ни одного `AI_COMMANDER` assignment |
| AI only USSR | `-aicfAICommanderMode USSR` | зеркально: `US=PLAYER`, `USSR=AI` |
| Invalid values | отдельные runs для `NONE`, unknown, lowercase и empty | один `CONFIG_INVALID` с raw value и Stage 1 `RESULT FAIL`; нет `CONFLICT_READY`, любого `RADIO_BRIDGE_*`, deferred controller `CONFIG`, `MATCH_START`, roster/spawn, `COMMAND_AUTHORITY_SET`, assignments и loops |
| Initial spawn | valid single-AI mode | static rule доказывает передачу exact prevalidated config и publish ordering; до единственного `ROSTER_READY` runtime даёт `reason=INITIAL_DEPLOYMENT group_generation=1 assignment_revision=1` своей authority для всех `S0..S9`; `COMMAND_WAITING` совпадает с `SYSTEM_HOLD` по generation/revision и идёт после assignment; hold использует HQ Defend waypoint независимо от role |
| Player override | отправить valid player order на player-commanded и AI-controlled стороне | `PLAYER_COMMAND` имеет приоритет, снимает `COMMAND_WAITING`; server повторно проверяет faction/slot/target |
| Replacement | уничтожить группу с active player и AI intent | новый `group_generation` того же `faction + stable_slot`; intent target/authority переживает runtime cleanup и восстанавливается только после revalidation |
| Base capture | сделать player target недопустимым | player-commanded slot переходит в `SYSTEM_HOLD` без нового autonomous target; AI side получает решение только через свой commander |
| Reliability/stuck/lone survivor | вызвать потерю/rebuild waypoint и завершение retreat | valid intent восстанавливает тот же target; safety recovery не меняет `decision_authority`; player side не получает скрытый AI retarget; owned MOB rebuild принимает только exact runtime transition `assignment revision N → N+1` при неизменных target/authority/intent и завершается отдельным physical confirmation |
| Vehicle replan/fallback | invalidation или fallback во время active trip | vehicle domain продолжает/восстанавливает выбранный intent, не создаёт `AI_COMMANDER` assignment для player-commanded faction; hold не поступает в vehicle admission; новый intent revision в начатом `HANDOFF` получает свежий bounded restore budget без сброса cleanup, waypoint-only revision budget не переармит |
| Client JIP | после `ROSTER_READY` и initial coverage PASS подключить новый client | server connection marker расположен после `ROSTER_READY`, client connection marker — до `COMMAND_AUTHORITY_REPLICATED`, последняя реплика содержит согласованные flags; UI показывает `AI COMMANDER`/`PLAYER COMMAND`, waiting state доступен; визуальный verdict пользователя или `NOT RUN` |
| Controller stop | отдельный lifecycle-run: оставить client подключённым при fatal/completed Stop | после ранее опубликованного valid pair client получает `ai_commander_us=0 ai_commander_ussr=0`; UI возвращается в `COMMAND SYNC`; source ordering также закрывает `AI_COMMANDER_REPLICATION` |

Для всех valid runs сохрани `CONFIG`, `COMMAND_AUTHORITY_SET`,
`COMMAND_WAITING`, `STRATEGIC_ASSIGNMENT decision_authority=...`, полные logs и
отдельные server/client/JIP verdict. Одних этих filtered events недостаточно для
общего runtime PASS.

## Evidence checklist

Для каждого проверочного прогона запиши:

- цель и конфигурацию сценария;
- branch, full commit SHA и dirty status;
- Game/Server/Tools versions;
- server/client роли, world, mission header и systems config;
- полный CLI с `aicf*` flags;
- время начала/завершения и способ остановки;
- команды, exit codes и rule IDs;
- абсолютные пути к полным Workbench/server/client logs;
- отдельные verdict: static, compile, server, client, JIP, soak;
- ручной verdict пользователя для визуальных критериев либо `NOT RUN`; Codex
  сам screenshots/video не создаёт;
- все `FAIL`, `BLOCKED` и `NOT RUN` без повышения статуса.

Финальная передача должна отделять новый regression от сохранённого baseline и
явно перечислять непроверенные свойства.

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
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\Test-MapPointOrdersStatic.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\Test-RHSIntegrationStatic.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\Test-ScenarioHeadersStatic.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\Test-RankRestrictionsStatic.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\Test-RuntimeLauncherStatic.ps1
```

Назначение:

| Script | Основной охват |
|---|---|
| `Test-Stage3Static.ps1` | vehicle architecture, acquisition, trip, cleanup и diagnostics contracts |
| `Test-Stage35Static.ps1` | force structure, vehicle/infantry integration и Enforce language audit |
| `Test-Stage35RecoveryPolicy.ps1` | точные recovery, timing, ownership и fail-closed policies |
| `Test-Stage4Static.ps1` | economy transaction, supply balance, replication, strategic UI и RPC authority |
| `Test-AICommanderModeStatic.ps1` | Arland CLI preflight, immutable authority policy, faction commander boundary, intent, availability replication и UI contract |
| `Test-MapPointOrdersStatic.ps1` | `BASE|POSITION` model, RPC trust boundary, bounded streamable-navmesh retry и identity guards, terrain/navmesh validation, planner ownership, отдельные current-destination/base-candidate validity boundaries, durable recovery, vehicle stale guards, deferred stock map cursor, dynamic UI palette, static allied/JIP marker и diagnostics |
| `Test-RHSIntegrationStatic.ps1` | optional dependency graph, Core isolation, stock/RHS profiles, fail-closed roles/vehicles, personnel building-browser adapter, loadout UI guard, stable-side Arland radio normalization, RHS_AFRF identity voice, single lifecycle и cleanup symmetry |
| `Test-ScenarioHeadersStatic.ps1` | stock/RHS inherited MissionHeader, menu visibility, отключённый persistence, stable resource GUID, platform metadata и отсутствие собственных world/layer resources |
| `Test-RankRestrictionsStatic.ps1` | `GENERAL` join/XP floor, maximum non-renegade fallback для container без `GENERAL`, central mutation/restore hook, authoritative faction/spawn recheck, replicated character state, authority/replication, запрет раннего polling и сохранение остальных admission checks |
| `Test-RuntimeLauncherStatic.ps1` | прямой native invocation без повторной сериализации, целостность `addonsDir` с пробелами/кириллицей, stock/RHS graph, fresh profile и fail-closed client readiness gate |

Аудиторы проверяют часть архитектуры регулярными выражениями. Красный rule ID
может означать реальный regression или drift тестового контракта. Сначала
сопоставь правило с поведением и историей; не меняй production-код либо regex
только ради зелёного вывода.

## Зафиксированный baseline

Baseline повторно проверен `2026-08-30` на чистом `main`, commit
`2575ff07a4c9d2afe1e1cc7aa8f6c0d657e72a5b`, до изменений map-point orders.
После изменения HEAD его нужно запускать заново.

| Проверка | Exit | Результат |
|---|---:|---|
| `Test-Stage3Static.ps1` | 1 | `FAIL`, 5 issues |
| `Test-Stage35Static.ps1` | 1 | `FAIL`, 3 issues |
| `Test-Stage35RecoveryPolicy.ps1` | 0 | `PASS` |
| `Test-Stage4Static.ps1` | 0 | `PASS` |

`Test-AICommanderModeStatic.ps1` добавлен после этого исторического baseline и
должен передаваться отдельным verdict, без выдуманного сравнения с `main`.
То же относится к `Test-RHSIntegrationStatic.ps1`: его первый успешный прогон
является отдельным focused verdict, а не частью исторического baseline commit.
`Test-ScenarioHeadersStatic.ps1` добавлен вместе со scenario headers и также
передаётся отдельным focused verdict без сравнения с историческим commit.
`Test-MapPointOrdersStatic.ps1` добавлен вместе с map-point orders и передаётся
как отдельный focused verdict относительно этого baseline.

Stage 3 baseline failures:

- `STAGE3_PROGRESS_EVIDENCE`: auditor ожидает five-minute objective timeout,
  код задаёт `120000` ms;
- два `STAGE3_RUNNING_CARGO_STALL` contracts;
- `STAGE3_BOUNDED_PROTECTED_CLEARANCE` reason signature;
- `STAGE3_MARKER_STATE`: auditor ожидает marker fragment `VEH `.

Stage 3.5 baseline failures:

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
| Map-point orders (`POSITION`) | `Test-MapPointOrdersStatic.ps1` + AI commander/Stage 3/3.5/RecoveryPolicy/Stage 4 + Workbench; server/client/JIP runtime и отдельный ручной visual/input verdict |
| Arland `modded` integration | Workbench + canonical Arland server runtime; клиент, если меняется UI/replication |
| Content profile или `AIConflictArlandRHS` | все Stage/authority audits + `Test-RHSIntegrationStatic.ps1`; отдельный stock и RHS Workbench; fresh RHS server; client/JIP при изменении faction/UI mapping |
| `Missions/*.conf` или `.conf.meta` | `Test-ScenarioHeadersStatic.ps1` на позитивном и негативном input; отдельный stock и RHS Workbench; source/direct MissionHeader load; ручная проверка плитки и packaged build |
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

### Rank floor

Rank runtime выполняется с отдельным fresh server/client profile. После
автоматического или ручного входа за игровую фракцию зафиксируй XP и rank,
затем последовательно проверь:

```text
GENERAL -> teamkill/collision penalty -> GENERAL -> respawn -> GENERAL
        -> disconnect/reconnect или новый JIP client -> GENERAL
```

Server log должен содержать
`[AICF][RANK][INFO][XP_FLOOR_VERIFIED] ... current_xp=... floor_xp=...
catalog_floor_rank=... rank=GENERAL` для initial join, XP mutation, respawn и
reconnect/JIP. `catalog_floor_rank=MAJOR` ожидаем для stock/default Reforger 1.8
container без отдельной записи `GENERAL`; это источник effective XP threshold,
а итоговый replicated character rank всё равно обязан быть `GENERAL`.
`XP_FLOOR_APPLIED` дополнительно требуется для каждой коррекции, которая иначе
опустила бы XP ниже порога. Сами эти строки не доказывают всю цепочку: отдельно
сохрани server evidence исходного штрафа, server respawn и
disconnect/reconnect, а в client log и HUD проверь итоговый owner-only XP/rank
после каждой фазы. Визуальный HUD остаётся ручным критерием пользователя.
Default AICF scenario headers отключают persistence, поэтому загрузка save не
входит в их release gate. Защитный deserialize path rank policy проверяется
отдельно только при появлении явно поддержанного persistence-enabled integration
header; такой тест должен закончиться `XP_FLOOR_APPLIED`, а затем
`XP_FLOOR_VERIFIED` до наблюдаемого rank state.

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

Для stock runtime используй
`Missions/AICF_Conflict_Arland.conf`, для RHS —
`Missions/AICF_RHS_Conflict_Arland.conf`. Оба header наследуют официальный
scenario contract; raw world и `worldSystemsConfig` в terminal-команде должны
соответствовать выбранному родителю.

### Scenario menu

Scenario gate разделяется на четыре независимых уровня:

1. `Test-ScenarioHeadersStatic.ps1` подтверждает exact official parent,
   `m_bShowInScenarioMenu`, `m_eSaveTypes 0`, metadata GUID и отсутствие
   скопированных `.ent`/`.layer`.
2. Отдельный stock/RHS Workbench Validate подтверждает регистрацию ресурсов и
   отсутствие `SCRIPT (E/F)`, `ENGINE (F)`, VM/null ошибок проекта.
3. Direct source run с новым `-MissionHeader` должен прочитать header и
   загрузить world/systems его официального родителя; это ещё не доказывает
   появление плитки в UI или полный gameplay runtime.
4. Пользователь вручную проверяет плитку, её название, выбор и начало новой
   сессии в меню `Сценарии`, затем повторный hosting с тем же game profile:
   прежняя progression не должна загружаться, а save/continue UI для AICF не
   должен появляться. Для релиза проверка повторяется на packaged Workshop
   build, который не видит source checkout.

Визуальные пункты остаются `NOT RUN`, пока пользователь не передаст ручной
verdict. Запуск из меню использует default `aicfAICommanderMode=BOTH`; другие
режимы command authority проверяются через отдельный dedicated server CLI.

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
  `ROSTER_READY`; каждый ready snapshot подтверждает `expected=10`, `actual=10`;
- `AI_WORLD_CAPACITY` имеет `effective_limit >= required_limit`, а каждый
  `GROUP_ROSTER_CONFIGURED` содержит `size=10`, `fallback_slots=0`, RHS USMC/MSV
  `prefabs` и ни одного `Character_US_`/`Character_USSR_`;
- при первом открытии building mode событие `PERSONNEL_BROWSER_BOUND` содержит
  `bound_count=2`, ненулевой `filtered_count` и только USMC/MSV `SentryTeam`;
- после нажатия карточки `PERSONNEL_SERVER_VALIDATED` содержит
  `essential_projected=1 allowed=1`, а server log подтверждает spawn выбранной
  RHS group prefab;
  ручной клиентский запрос одного бойца из каждой постройки проходит через
  штатные supplies, capacity и authority checks без VM/null ошибки;
- после захвата односторонней frontier-base событие `RADIO_BRIDGE_NORMALIZED`
  предшествует graph rebuild, а новый graph даёт владельцу путь от базы к relay;
- vehicle metadata `CATALOG` повторена событием `LIVE` после spawn; выбранные
  prefab принадлежат заявленному RHS faction catalog;
- нет `SCRIPT (E/F)`, `ENGINE (F)`, VM exception или null-pointer;
- RHS deployment map не пишет `Can't find image ''` для
  `UI/Imagesets/MilitarySymbol/ID_D.imageset`; spawn-point factions отображаются
  как полные `BLUFOR`/`OPFOR` symbols, а не пустые цветные квадраты;
- за `RHS_AFRF` сообщения HQ-комментатора воспроизводятся русским голосом;
  это клиентский аудиокритерий и без ручного прослушивания остаётся `NOT RUN`;
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

### Runtime matrix: vehicle boarding и dismount

| Сценарий | Действие | Обязательное evidence |
|---|---|---|
| Staging и deterministic crew | дождаться transport request со свежим spawn, включая неровный рельеф/растянутую формацию и одного настоящего outlier | `SPAWN_SITE_PLANNED` сохраняет guidance `43 м / 25 м`; `VEHICLE_SPAWN_STAGING_PROGRESS` считает всех живых в фактическом safe pad envelope `10..90 м`, поэтому собранная формация не остаётся на `9/10`; outlier получает bounded `VEHICLE_STAGING_MEMBER_ACTION_ISSUED`/`REISSUED` с live progress и exact identity; после исчерпания двух actions при доказанном `N-1/N` появляются `VEHICLE_STAGING_MEMBER_RELOCATION_ATTEMPTED` и успешный `..._RESULT postcondition=1`, после чего staging завершается; plan cancellation завершает owned action; waypoint-only `assignment revision` и чужой глобальный `base revision` дают `VEHICLE_REQUEST_RUNTIME_REVISION_ADOPTED action=KEEP_SPAWN_PLAN` без нового request generation, exact site при этом revalidated live; boarding не начинает повторный долгий `APPROACH` для уже staged roster; `CREW_AGENT_SELECTED` содержит nearest exact-door distance и stable member identity |
| Partial exact-seat plan | временно заблокировать одну Cargo reservation/seat при наличии других готовых мест | `PASSENGERS_ASSIGNED policy=DETERMINISTIC_PARTIAL_EXACT_CARGO_AFTER_MANDATORY_CREW`; действия готовых `member -> seat` пар продолжаются; `BOARDING_BLOCKER` для заблокированной пары содержит `phase`, `blocker_member`, `distance_m`, `action_state`, `progress_age_ms`, `retry`, `seat`, `linked`, `getting_in`, `recovery_fence`, `deadline_remaining_ms`; корректные reservations не снимаются |
| Boarding recovery fence | получить stalled Pilot/Turret/Cargo сначала без игроков и combat рядом, затем повторить с активным fence | normal action предшествует новому tracked `SCR_AIGetInVehicle` к тому же exact seat; retry не вызывает параллельный `CompartmentAccessComponent.GetInVehicle(... false ...)`, сохраняет action/reservation ownership, а их потеря обнаруживается следующим poll; forced exact-seat mutation происходит не раньше следующего scheduler tick и не более чем для одного member; identity/seat остаются exact; при player radius/LOS/combat Cargo пишет `*_DEFERRED` и ждёт, а mandatory Pilot/Turret сначала пишет `CREW_AGENT_ROTATION_SCHEDULED`, не раньше следующего scheduler tick — `CREW_AGENT_ROTATED`, и выдаёт видимый exact-seat action следующему детерминированному кандидату; новая reservation остаётся owned после следующего poll; исчерпание кандидатов ждёт до общего phase deadline; при terminal fallback не должно быть позднего `linked/getting_in` после восстановления infantry order |
| Full boarding at deadline | завершить физическую посадку всех живых на первом tick после истечения grace | `BOARDING_FULL_OCCUPANCY_DEADLINE_SUPPRESSED` содержит `alive=linked=settled`, `settled_polls=1`, `action=WAIT_NEXT_TICK_NO_FALLBACK`; следующий tick даёт `BOARDING_COMPLETE` и `TRANSIT`; для того же operation отсутствуют `BOARDING_TIMEOUT`, `INFANTRY_FALLBACK` и `FALLBACK_FORCE_DISEMBARK` |
| Explicit vehicle reacquisition | после terminal fallback, когда до текущей цели осталось менее `minimum_route_m`, переключить unit type `INFANTRY -> MOTORIZED_*` | `GROUP_CONFIG_ACCEPTED explicit_vehicle_admission=1`, затем `VEHICLE_EXPLICIT_ADMISSION_REQUESTED route_threshold_bypass=1`; после прохождения остальных gates один `VEHICLE_EXPLICIT_ADMISSION_CONSUMED trip_created=1` и новый `VEHICLE_REQUESTED`; обычный motorized admission без explicit intent по-прежнему получает `ROUTE_BELOW_VEHICLE_THRESHOLD` |
| Early dismount handoff | выполнить поездку и оставить вышедших members рядом с vehicle не на target-side | через `5 с` допустим exact animated retry, через `10 с` — fenced forced exact exit; `DISEMBARK_COMPLETE reason=TRIP_EXIT_PHYSICALLY_PROVEN` появляется после двух consecutive polls без occupants/transitions/inside-bounds и infantry order восстанавливается; cleanup отдельно сохраняет target-side/clearance и непрерывный `5 с` stable-clear до release/delete |

### Runtime matrix: map-point orders

| Сценарий | Действие | Обязательное evidence |
|---|---|---|
| Valid point | выбрать READY slot, нажать `MOVE TO MAP POINT`, затем отдельным кликом выбрать доступную navmesh; дождаться минимум одного commander tick и одной несвязанной смены владельца базы | первый клик только скрывает panel и после input-frame активирует cursor; для unloaded tile есть `PLAYER_POINT_ORDER_PENDING reason=NAVMESH_TILE_LOADING`, затем server `PLAYER_ORDER_ACCEPTED target_kind=POSITION` и `STRATEGIC_ASSIGNMENT target_kind=POSITION`; summary содержит `POSITION`, authoritative X/Z и новый intent revision; waypoint `MOVE_AND_HOLD` current/queued; последующие `COMMANDER_REPLAN`/`BASE_OWNER_CHANGED` не создают для того же stable slot `target_kind=BASE decision_authority=AI_COMMANDER` и не очищают player intent |
| Invalid point | отправить non-finite, за terrain bounds и точку без nearby navmesh отдельными RPC/runtime fixtures | `PLAYER_ORDER_REJECTED target_kind=POSITION` с exact `COORDINATE_NOT_FINITE`, `OUTSIDE_WORLD_BOUNDS`, `NO_NAVMESH_ENDPOINT_NEARBY`/`NAVMESH_ENDPOINT_OUT_OF_RANGE`, `NAVMESH_TILE_UNAVAILABLE` или bounded `NAVMESH_TILE_TIMEOUT`; reliable owner response приходит до UI timeout, UI показывает exact reason и actionable подсказку; runtime target/intent/waypoint не изменились |
| Cancel, input и map lifecycle | начать выбор, убедиться, что клик кнопки не выбрал точку; отменить; повторить и закрыть карту | panel/scrim скрыты только во время выбора; prompt имеет тёмно-синий фон, читаемый светлый текст и красную cancel-action; map-point action синяя, base targets янтарные; stock pan/zoom остаются доступны; после cancel/close нет deferred activation, selection callback и повторного cursor update; визуальный verdict пользователя или `NOT RUN` |
| Role/replacement/recovery | сменить role, уничтожить группу, вызвать waypoint loss и stuck rebuild | сохраняются `faction + numeric slot`, `POSITION`, X/Z и intent revision; новая group generation получает тот же endpoint; bounded recovery не выбирает BASE и не запускает capture/S&D |
| Base target list after point | выдать `POSITION` первому slot роли, затем открыть base targets другого или того же slot этой роли | ATTACK/DEFEND/RESERVE списки по-прежнему содержат только role/ownership-valid `BASE`; текущий `POSITION` representative-slot не делает все objective graph nodes видимыми; выбранная base повторно проходит ту же server validation |
| Active vehicle retarget | выдать point order в acquisition, boarding, transit и handoff | snapshot содержит kind/position; старый request/trip не коммитит stale destination; acquisition site выбирается независимо от destination; после fallback/dismount восстановлен тот же point order |
| Allied marker/JIP | выдать point order, подключить союзного и вражеского JIP client, затем сменить point и BASE | static server marker виден только союзникам, присутствует у allied JIP, меняет позицию/label и удаляется при BASE/clear/Stop; визуальный verdict пользователя или `NOT RUN` |

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
